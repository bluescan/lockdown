// Lockdown.cpp
//
// Locks the computer after a period of inactivity. On both Windows 10 and Windows 11 sometimes setting a screensaver
// timeout does not reliably lock the machine. This simple system-tray app is reliable and reads gamepad inputs for
// game dev.
//
// Copyright (c) 2020, 2024, 2025 Tristan Grimmer.
//
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby
// granted, provided that the above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
// AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <windows.h>
#include <System/tPrint.h>
#include <System/tCmdLine.h>
#include <Math/tVector2.h>
#include <tchar.h>
#include <commctrl.h>
#include "resource.h"
#include <libgamepad.hpp>
#include "Version.cmake.h"
#pragma warning(disable: 4996)
using namespace tMath;
#define	WM_USER_TRAYICON (WM_USER+1)


// Command-line options.
tCmdLine::tOption OptionHelp				("Display help and usage screen.",	"help",		'h'			);
tCmdLine::tOption OptionSyntax				("Display CLI syntax guide.",		"syntax",	'y'			);
tCmdLine::tOption OptionTimeoutMinutes		("Timeout in minutes.",				"minutes",	'm',	1	);
tCmdLine::tOption OptionTimeoutSeconds		("Timeout in seconds.",				"seconds",	's',	1	);
tCmdLine::tOption OptionMaxSuspendMinutes	("Max suspend time in minutes.",	"suspend",	'x',	1	);
tCmdLine::tOption OptionKeyboard			("Detect any keyboard input",		"keyboard",	'k'			);
tCmdLine::tOption OptionMouseMovement		("Detect any mouse movement.",		"movement",	'v'			);
tCmdLine::tOption OptionMouseButton			("Detect any mouse button presses.","button",	'b'			);
tCmdLine::tOption OptionPadButtons			("Detect any gamepad button input.","pad",		'p'			);
tCmdLine::tOption OptionAxis				("Detect any gamepad axis changes.","axis",		'a'			);


namespace Lockdown
{
	HINSTANCE hInst;
	NOTIFYICONDATA NotifyIconData;
	HHOOK hKeyboardHook						= NULL;
	HHOOK hMouseHook						= NULL;
	BOOL NotifyIconAdded					= 0;

	bool Enabled							= true;
	int SecondsToLock						= 20 * 60;				// 20 minutes unless overridden by command line.
	int MaxSuspendSeconds					= 3 * 60 * 60;			// 3 hour max suspend time unless overridden by command line.
	int CountdownSeconds					= SecondsToLock;
	int CountdownSuspendSeconds				= MaxSuspendSeconds;
	int MouseX								= 0;					// This may be negative for multiple monitors.
	int MouseY								= 0;					// This may be negative for multiple monitors.
	const int MouseDistanceThreshold		= 20;					// How far mouse must move to count as activity.

	LRESULT CALLBACK MainWinProc(HWND hwnd, UINT message, WPARAM, LPARAM);
	LRESULT CALLBACK Hook_Keyboard(int code, WPARAM, LPARAM);
	LRESULT CALLBACK Hook_Mouse(int code, WPARAM, LPARAM);

	void Hook_GamepadButton(std::shared_ptr<gamepad::device>);
	void Hook_GamepadAxis(std::shared_ptr<gamepad::device>);
	void Hook_GamepadConnect(std::shared_ptr<gamepad::device>);
	void Hook_GamepadDisconnect(std::shared_ptr<gamepad::device>);

	enum ExitCode
	{
		ExitCode_Success,
		ExitCode_AlreadyRunning,
		ExitCode_CommonControlsInitFailure,
		ExitCode_RegisterClassFailure,
		ExitCode_CreateWindowFailure,
		ExitCode_XInputGamepadHookFailure
	};
}


LRESULT CALLBACK Lockdown::MainWinProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	static UINT taskbarRestart = 0;

	switch (message)
	{
		case WM_CREATE:
			taskbarRestart = RegisterWindowMessage(TEXT("TaskbarCreated"));
			break;

		case WM_DESTROY:
			if (NotifyIconAdded)
				Shell_NotifyIcon(NIM_DELETE, &NotifyIconData);
			PostQuitMessage(0);
			break;

		case WM_TIMER:
		{
			if (!NotifyIconAdded)
				NotifyIconAdded = Shell_NotifyIcon(NIM_ADD, &NotifyIconData);

			if (Enabled)
			{
				CountdownSeconds--;
			}
			else
			{
				CountdownSuspendSeconds--;
				if (CountdownSuspendSeconds <= 0)
					Enabled = true;
			}

			int secondsLeft = Lockdown::CountdownSeconds + 1;
			int mins = secondsLeft / 60;
			int secs = secondsLeft % 60;
			if (NotifyIconAdded)
			{
				if (Enabled)
					tsPrintf(NotifyIconData.szTip, sizeof(NotifyIconData.szTip), "Lock in %02d:%02d", mins, secs);
				else
					tsPrintf(NotifyIconData.szTip, sizeof(NotifyIconData.szTip), "Lockdown Disabled");
				Shell_NotifyIcon(NIM_MODIFY, &NotifyIconData);
			}

			if (CountdownSeconds <= 0)
			{
				CountdownSeconds = SecondsToLock;
				LockWorkStation();
			}
		}

		case WM_USER_TRAYICON:
			switch (LOWORD(lparam))
			{
				case WM_RBUTTONDOWN:
				case WM_LBUTTONDOWN:
				{
					POINT cursorPos;
					GetCursorPos(&cursorPos);

					HMENU hmenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_TRAY_MENU));
					if (!hmenu)
						return -1;

					HMENU hsubMenu = GetSubMenu(hmenu, 0);
					if (!hsubMenu)
					{
						DestroyMenu(hmenu);
						return -1;
					}

					if (Enabled)
						CheckMenuItem(hmenu, ID_MENU_ENABLED, MF_BYCOMMAND | MF_CHECKED);
					else
						CheckMenuItem(hmenu, ID_MENU_ENABLED, MF_BYCOMMAND | MF_UNCHECKED);

					SetForegroundWindow(hwnd);
					TrackPopupMenu(hsubMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN, cursorPos.x, cursorPos.y, 0, hwnd, NULL);
					SendMessage(hwnd, WM_NULL, 0, 0);
					DestroyMenu(hmenu);
					break;
				}
			}
			break;

		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;

		case WM_COMMAND:
			switch (LOWORD(wparam))
			{
				case ID_MENU_ABOUT:
				{
					tString messageString;
					tsPrintf
					(
						messageString,
						// @todo Support command line options for gamepad buttons, gamepad axis, mouse buttons, mouse movement, keyboard, timeout minutes.
						"Lockdown V%d.%d.%d by Tristan Grimmer.\n"
						"Under ISC licence (similar to MIT).\n\n"
						"Homepage at https://github.com/bluescan/lockdown\n"
						"\n"
						"This system tray program locks the computer after a\n"
						"specified duration without user input.\n"
						"\n"
						"Consider running as a scheduled task on logon. Do not\n"
						"terminate the task.\n"
						"\n"
						"The timeout duration as well as what inputs should be\n"
						"detected may be set via command line parameters. Run\n"
						"'lockdown.exe -h' to view all supported options.\n"
						"\n"
						"The current timeout is %d minutes and %d seconds.\n"
						"By default the timer is reset on keyboard activity, mouse\n"
						"button presses, mouse movement, gamepad button presses,\n"
						"and gamepad axis displacement.\n",
						LockdownVersion::Major, LockdownVersion::Minor, LockdownVersion::Revision, Lockdown::SecondsToLock/60, Lockdown::SecondsToLock%60
					);
					::MessageBox
					(
						hwnd, messageString.Chr(), "About Lockdown", MB_OK | MB_ICONINFORMATION
					);
					break;
				}

				case ID_MENU_QUIT:
				{
					int result = ::MessageBox
					(
						hwnd,
						"If you quit the Lockdown app your computer may not automatically lock.\n\n"
						"Are you sure you want to quit?",
						"Quit Lockdown", MB_YESNO | MB_ICONEXCLAMATION
					);
					if (result == IDYES)
						DestroyWindow(hwnd);
					break;
				}

				case ID_MENU_LOCK10:
					Enabled = true;
					CountdownSeconds = 10;
					break;

				case ID_MENU_ENABLED:
					// If about to toggle off print a warning.
					if (Enabled)
					{
						tString message;
						tsPrintf
						(
							message,
							"Please confirm you want to suspend lockdown.\n\n"
							"OK will suspend auto-locking for %d hours %d minutes.\n"
							"Cancel will leave lockdown enabled.\n\n",
							MaxSuspendSeconds / 3600, (MaxSuspendSeconds % 3600) / 60
						);
						int result = ::MessageBox(hwnd, message.Chr(), "Suspend Lockdown?", MB_OKCANCEL | MB_ICONQUESTION);
						if (result == IDOK)
						{
							CountdownSuspendSeconds = MaxSuspendSeconds;
							Enabled = false;
						}
					}
					else
					{
						Enabled = true;
					}
					break;

				case ID_MENU_LOCKNOW:
					Enabled = true;
					LockWorkStation();
					break;
			}
			break;

		default:
			if (message == taskbarRestart)
			{
				NotifyIconAdded = Shell_NotifyIcon(NIM_ADD, &NotifyIconData);
			}
			break;
	}

	return DefWindowProc(hwnd, message, wparam, lparam);
}


LRESULT CALLBACK Lockdown::Hook_Keyboard(int code, WPARAM wparam, LPARAM lparam)
{
	if (wparam == WM_KEYDOWN)
		CountdownSeconds = SecondsToLock;

	return CallNextHookEx(hKeyboardHook, code, wparam, lparam);
}


LRESULT CALLBACK Lockdown::Hook_Mouse(int code, WPARAM wparam, LPARAM lparam)
{
	MSLLHOOKSTRUCT* mouseStruct = (MSLLHOOKSTRUCT*)lparam;
	if
	(
		OptionMouseButton.IsPresent() &&
		(
			(wparam == WM_LBUTTONDOWN) || (wparam == WM_LBUTTONUP) ||
			(wparam == WM_RBUTTONDOWN) || (wparam == WM_RBUTTONUP) ||
			(wparam == WM_MOUSEWHEEL)
		)
	)
	{
		CountdownSeconds = SecondsToLock;
	}

	if
	(
		OptionMouseMovement.IsPresent() &&
		(
			(wparam == WM_MOUSEMOVE) || (wparam == WM_NCMOUSEMOVE)
		)
	)
	{
		tVector2 prevPos{float(MouseX), float(MouseY)};
		int xpos = mouseStruct->pt.x;
		int ypos = mouseStruct->pt.y;
		tVector2 currPos{float(xpos), float(ypos)};
		tVector2 delta = currPos - prevPos;
		if (delta.Length() > MouseDistanceThreshold)
		{
			MouseX = xpos;
			MouseY = ypos;
			CountdownSeconds = SecondsToLock;
		}
	}

	return CallNextHookEx(hMouseHook, code, wparam, lparam);
}


void Lockdown::Hook_GamepadButton(std::shared_ptr<gamepad::device> dev)
{
	tdPrintf
	(
		"Received button event: Native id: %i, Virtual id: 0x%X (%i) val: %f\n",
		dev->last_button_event()->native_id, dev->last_button_event()->vc,
		dev->last_button_event()->vc, dev->last_button_event()->virtual_value
	);

	// Any button press on any gamepad resets the countdown.
	// @todo Test that LB RB bumper buttons reset.
	// @todo Ideally this write would be mutex protected.
	CountdownSeconds = SecondsToLock;
};


void Lockdown::Hook_GamepadAxis(std::shared_ptr<gamepad::device> dev)
{
	tdPrintf
	(
		"Received axis event: Native id: %i, Virtual id: 0x%X (%i) val: %f\n",
		dev->last_axis_event()->native_id, dev->last_axis_event()->vc,
		dev->last_axis_event()->vc, dev->last_axis_event()->virtual_value
	);

	// The gamepad device already deals with axis dead-zones. This means we can safely ignore
	// the fact that we're getting events from different gamepads and 'wobbling' between
	// them. We can simply reset the countdown on any axis event -- regardless of which
	// gamepad or the particular axis.

	// @todo Test that LT RT triggers reset.
	// @todo Ideally this write would be mutex protected.
	CountdownSeconds = SecondsToLock;
};


void Lockdown::Hook_GamepadConnect(std::shared_ptr<gamepad::device> dev)
{
	tdPrintf("%s connected\n", dev->get_name().c_str());

	// @todo Ideally this write would be mutex protected.
	CountdownSeconds = SecondsToLock;
};


void Lockdown::Hook_GamepadDisconnect(std::shared_ptr<gamepad::device> dev)
{
	tdPrintf("%s disconnected\n", dev->get_name().c_str());

	// On a gamepad disconnect it makes sense _not_ to coult it as an input. One might,
	// for example, be disconnecting the gamepad when leaving for the day.
};


int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprevInstance, LPSTR cmdLine, int cmdShow)
{
	// If one is already running, do not start another.
	LPCTSTR OtherWindowName = "LockdownTrayWindowName";
	HWND otherFoundWindowHandle = FindWindow(NULL, OtherWindowName);
	if (otherFoundWindowHandle)
		return Lockdown::ExitCode_AlreadyRunning;

	tSystem::tSetSupplementaryDebuggerOutput();

	// Parse command line.
	tCmdLine::tParse((char8_t*)cmdLine, false, false);

	// Was a timeout override specified?
	int timeoutOverride = 0;
	if (OptionTimeoutMinutes.IsPresent())
		timeoutOverride += 60 * OptionTimeoutMinutes.Arg1().AsInt();
	if (OptionTimeoutSeconds.IsPresent())
		timeoutOverride += OptionTimeoutSeconds.Arg1().AsInt();
	if (timeoutOverride > 0)
		Lockdown::SecondsToLock = timeoutOverride;
	Lockdown::CountdownSeconds = Lockdown::SecondsToLock;

	int suspendOverride = 0;
	if (OptionMaxSuspendMinutes.IsPresent())
		suspendOverride = 60 * OptionMaxSuspendMinutes.Arg1().AsInt();
	if (suspendOverride > 0)
		Lockdown::MaxSuspendSeconds = suspendOverride;
	Lockdown::CountdownSuspendSeconds = Lockdown::MaxSuspendSeconds;

	if
	(
		!OptionKeyboard.IsPresent()		&& !OptionMouseMovement.IsPresent()		&& !OptionMouseButton.IsPresent() &&
		!OptionPadButtons.IsPresent()	&& !OptionAxis.IsPresent()
	)
	{
		OptionKeyboard.Present = true;
		OptionMouseMovement.Present = true;
		OptionMouseButton.Present = true;
		OptionPadButtons.Present = true;
		OptionAxis.Present = true;
	}

	if (OptionKeyboard.IsPresent())
		Lockdown::hKeyboardHook	= SetWindowsHookEx(WH_KEYBOARD_LL,	Lockdown::Hook_Keyboard,	NULL, 0);

	if (OptionMouseMovement.IsPresent() || OptionMouseButton.IsPresent())
		Lockdown::hMouseHook	= SetWindowsHookEx(WH_MOUSE_LL,		Lockdown::Hook_Mouse,		NULL, 0);

	Lockdown::hInst = hinstance;

	INITCOMMONCONTROLSEX comControls;
	memset(&comControls, 0, sizeof(comControls));
	comControls.dwSize		= sizeof(comControls);
	comControls.dwICC		= ICC_UPDOWN_CLASS | ICC_LISTVIEW_CLASSES;
	if (!InitCommonControlsEx(&comControls))
		return Lockdown::ExitCode_CommonControlsInitFailure;

	WNDCLASSEX winClass;
	memset(&winClass, 0, sizeof(winClass));
	winClass.cbSize				= sizeof(winClass);
	winClass.style				= CS_HREDRAW | CS_VREDRAW;
	winClass.lpfnWndProc		= Lockdown::MainWinProc;
	winClass.cbClsExtra			= 0;
	winClass.cbWndExtra			= 0;
	winClass.hInstance			= hinstance;
	winClass.hIcon				= LoadIcon(hinstance, (LPCTSTR)MAKEINTRESOURCE(IDI_LOCKDOWN_ICON));
	winClass.hCursor			= LoadCursor(NULL, IDC_ARROW);
	winClass.hbrBackground		= (HBRUSH)GetStockObject(WHITE_BRUSH);
	winClass.lpszMenuName		= NULL;
	winClass.lpszClassName		= "LockdownTrayClass";
	winClass.hIconSm			= LoadIcon(hinstance, (LPCTSTR)MAKEINTRESOURCE(IDI_LOCKDOWN_ICON));
	if (!RegisterClassEx(&winClass))
		return Lockdown::ExitCode_RegisterClassFailure;

	HWND hwnd = CreateWindowEx
	(
		WS_EX_CLIENTEDGE,	"LockdownTrayClass",	"LockdownTrayWindowName",		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,		CW_USEDEFAULT,			CW_USEDEFAULT,					CW_USEDEFAULT,
		NULL,				NULL,					hinstance,						NULL
	);

	if (!hwnd)
		return Lockdown::ExitCode_CreateWindowFailure;

	// We can't display message boxes until we have an hwnd.
	if (OptionHelp.IsPresent())
	{
		tString usage;
		tCmdLine::tStringUsageNI
		(
			usage, u8"Tristan Grimmer",
			u8""
			"Lockdown is a system-tray program that locks the computer after a period of "
			"inactivity. It can monitor user input from keyboard, mouse, and gamepads. If no "
			"inputs are specified on the command line (-kvbpa), all inputs are monitored.",
			LockdownVersion::Major, LockdownVersion::Minor, LockdownVersion::Revision
		);
		::MessageBox(hwnd, usage.Chr(), "Lockdown CLI Usage", MB_OK | MB_ICONINFORMATION);
		if (!OptionSyntax.IsPresent())
		{
			DestroyWindow(hwnd);
			return Lockdown::ExitCode_Success;
		}
	}

	if (OptionSyntax.IsPresent())
	{
		tString syntaxGuide;
		tCmdLine::tStringSyntax(syntaxGuide, 140);
		::MessageBox(hwnd, syntaxGuide.Chr(), "Lockdown CLI Syntax Guide", MB_OK | MB_ICONINFORMATION);
		DestroyWindow(hwnd);
		return Lockdown::ExitCode_Success;
	}

	// System tray icon.
	memset(&Lockdown::NotifyIconData, 0, sizeof(Lockdown::NotifyIconData));
	Lockdown::NotifyIconData.cbSize			= sizeof(Lockdown::NotifyIconData);
	Lockdown::NotifyIconData.hWnd			= hwnd;
	Lockdown::NotifyIconData.uID			= IDI_LOCKDOWN_ICON;
	Lockdown::NotifyIconData.uFlags			= NIF_ICON | NIF_MESSAGE | NIF_TIP;

	int secondsLeft = Lockdown::CountdownSeconds + 1;
	int mins = secondsLeft / 60;
	int secs = secondsLeft % 60;
   	Lockdown::Enabled = true;
	tsPrintf(Lockdown::NotifyIconData.szTip, sizeof(Lockdown::NotifyIconData.szTip), "Lock in %02d:%02d", mins, secs);

	Lockdown::NotifyIconData.hIcon = LoadIcon(hinstance, (LPCTSTR)MAKEINTRESOURCE(IDI_LOCKDOWN_ICON));
	Lockdown::NotifyIconData.uCallbackMessage = WM_USER_TRAYICON;
	Lockdown::NotifyIconAdded = Shell_NotifyIcon(NIM_ADD, &Lockdown::NotifyIconData);

	// Send a timer message every second.
	SetTimer(hwnd, 42, 1000, NULL);

	// Hook into gamepad/controller events.
	if (OptionPadButtons.IsPresent() || OptionAxis.IsPresent())
	{
		auto hook = gamepad::hook::make();
		hook->set_plug_and_play(true, gamepad::ms(1000));
		hook->set_sleep_time(gamepad::ms(100)); // 10fps poll.
		if (OptionPadButtons.IsPresent())
			hook->set_button_event_handler(Lockdown::Hook_GamepadButton);
		if (OptionAxis.IsPresent())
			hook->set_axis_event_handler(Lockdown::Hook_GamepadAxis);
		hook->set_connect_event_handler(Lockdown::Hook_GamepadConnect);
		hook->set_disconnect_event_handler(Lockdown::Hook_GamepadDisconnect);

		if (!hook->start())
		{
			tdPrintf("Couldn't start gamepad hook.\n");
			DestroyWindow(hwnd);
			return Lockdown::ExitCode_XInputGamepadHookFailure;
		}
	}

  	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// If we get here WM_CLOSE has already handled DestroyWindow.
	return Lockdown::ExitCode_Success;
}
