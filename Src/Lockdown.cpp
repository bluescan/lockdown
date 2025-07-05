// Lockdown.cpp
//
// Locks the computer after a period of inactivity. On both Windows 10 and Windows 11
// sometimes setting a screensaver timeout does not reliably lock the machine.
// This simple system-tray app is reliable and reads gamepad inputs for game dev.
//
// Copyright (c) 2020, 2024, 2025 Tristan Grimmer.
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <windows.h>
#include <windowsx.h>
#include <System/tPrint.h>
#include <tchar.h>
#include <commctrl.h>
#include "resource.h"
#include <libgamepad.hpp>
#include "Version.cmake.h"
#pragma warning(disable: 4996)


//#define LockdownVersion "V 1.0.4"
#define	WM_USER_TRAYICON (WM_USER+1)


namespace Lockdown
{
	HINSTANCE hInst;
	NOTIFYICONDATA NotifyIconData;
	HHOOK hKeyboardHook						= NULL;
	HHOOK hMouseHook						= NULL;
	BOOL NotifyIconAdded					= 0;

	BOOL Enabled							= 1;
	int SecondsToLock						= 20 * 60;				// 20 minutes.
	int MaxDisabledSeconds					= 3 * 60 * 60;			// 3 hour max disable time.
	int CountdownSeconds					= SecondsToLock;
	int CountdownDisabled					= MaxDisabledSeconds;

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
				CountdownDisabled--;
				if (CountdownDisabled <= 0)
					Enabled = 1;
			}

			int mins = CountdownSeconds / 60;
			int secs = CountdownSeconds % 60;

			if (NotifyIconAdded)
			{
				if (Enabled)
					sprintf(NotifyIconData.szTip, "Lock in %02d:%02d", mins, secs);
				else
					sprintf(NotifyIconData.szTip, "Lockdown Disabled");
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
						"Homepage at https://github.com/bluescan/lockdown\n\n"
						"Usage: Consider running as a scheduled task on logon.\n"
						"Do not terminate the task. An optional single integer\n"
						"command line parameter may given to specify the number\n"
						"of minutes before locking. If no parameter 20 minutes is\n"
						"used. Timer is reset on keypress, mouse buttons. Mouse movement\n"
						"alone is NOT considered and will not reset the timer.\n",
						LockdownVersion::Major, LockdownVersion::Minor, LockdownVersion::Revision
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
						"If you quit the Lockdown app your computer will not automatically lock.\n\n"
						"Are you sure you want to quit?",
						"Quit Lockdown", MB_YESNO | MB_ICONEXCLAMATION
					);
					if (result == IDYES)
						DestroyWindow(hwnd);
					break;
				}

				case ID_MENU_LOCK10:
					Enabled = 1;
					CountdownSeconds = 10;
					break;

				case ID_MENU_ENABLED:
					// If about to toggle off print a warning.
					if (Enabled)
					{
						int result = ::MessageBox
						(
							hwnd,
							"Please confirm you want to disable lockdown.\n\n"
							"Pressing OK will disable auto locking for 3 hours.\n"
							"Pressing Cancel will leave lockdown enabled.\n\n",
							"Disable Lockdown?", MB_OKCANCEL | MB_ICONQUESTION
						);
						if (result == IDOK)
						{
							CountdownDisabled = MaxDisabledSeconds;
							Enabled = 0;
						}
					}
					else
					{
						Enabled = 1;
					}
					break;

				case ID_MENU_LOCKNOW:
					Enabled = 1;
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
		(wparam == WM_LBUTTONDOWN) || (wparam == WM_LBUTTONUP) ||
		(wparam == WM_RBUTTONDOWN) || (wparam == WM_RBUTTONUP) ||
		(wparam == WM_MOUSEWHEEL)
	)
	{
		CountdownSeconds = SecondsToLock;
	}

	if
	(
		(wparam == WM_MOUSEMOVE) || (wparam == WM_NCMOUSEMOVE)
	)
	{
		int xpos = mouseStruct->pt.x;
		int ypos = mouseStruct->pt.y;
//		int xpos = GET_X_LPARAM(lparam);
//		int ypos = GET_Y_LPARAM(lparam);
		tPrintf
		(
			"Received MouseMove: X:%d Y:%d\n",
			xpos,
			ypos
		);

		// CountdownSeconds = SecondsToLock;
	}

	return CallNextHookEx(hKeyboardHook, code, wparam, lparam);
}


void Lockdown::Hook_GamepadButton(std::shared_ptr<gamepad::device> dev)
{
	tPrintf
	(
		"Received button event: Native id: %i, Virtual id: 0x%X (%i) val: %f\n",
		dev->last_button_event()->native_id, dev->last_button_event()->vc,
		dev->last_button_event()->vc, dev->last_button_event()->virtual_value
	);

	// @todo Ideally this write would be mutex protected.
	CountdownSeconds = SecondsToLock;
};


void Lockdown::Hook_GamepadAxis(std::shared_ptr<gamepad::device> dev)
{
	tPrintf
	(
		"Received axis event: Native id: %i, Virtual id: 0x%X (%i) val: %f\n",
		dev->last_axis_event()->native_id, dev->last_axis_event()->vc,
		dev->last_axis_event()->vc, dev->last_axis_event()->virtual_value
	);
};


void Lockdown::Hook_GamepadConnect(std::shared_ptr<gamepad::device> dev)
{
	tPrintf("%s connected\n", dev->get_name().c_str());
	// @todo Ideally this write would be mutex protected.
	CountdownSeconds = SecondsToLock;
};


void Lockdown::Hook_GamepadDisconnect(std::shared_ptr<gamepad::device> dev)
{
	tPrintf("%s disconnected\n", dev->get_name().c_str());
	// @todo Ideally this write would be mutex protected.
	CountdownSeconds = SecondsToLock;
};


int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprevInstance, LPSTR cmdLine, int cmdShow)
{
	// If one is already running, do not start another.
	LPCTSTR OtherWindowName = "LockdownTrayWindowName";
	HWND otherFoundWindowHandle = FindWindow(NULL, OtherWindowName);
	if (otherFoundWindowHandle)
		return Lockdown::ExitCode_AlreadyRunning;

	tSystem::tSetSupplementaryDebuggerOutput();
	Lockdown::CountdownSeconds = Lockdown::SecondsToLock;
	if (cmdLine && strlen(cmdLine) > 0)
	{
		int numMins = atoi(cmdLine);
		if (numMins > 0)
		{
			Lockdown::SecondsToLock = numMins * 60;
			Lockdown::CountdownSeconds = Lockdown::SecondsToLock;
		}
	}

	Lockdown::hKeyboardHook		= SetWindowsHookEx(WH_KEYBOARD_LL,	Lockdown::Hook_Keyboard,	NULL, 0);
	Lockdown::hMouseHook		= SetWindowsHookEx(WH_MOUSE_LL,		Lockdown::Hook_Mouse,		NULL, 0);

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

	// System tray icon.
	memset(&Lockdown::NotifyIconData, 0, sizeof(Lockdown::NotifyIconData));
	Lockdown::NotifyIconData.cbSize			= sizeof(Lockdown::NotifyIconData);
	Lockdown::NotifyIconData.hWnd			= hwnd;
	Lockdown::NotifyIconData.uID			= IDI_LOCKDOWN_ICON;
	Lockdown::NotifyIconData.uFlags			= NIF_ICON | NIF_MESSAGE | NIF_TIP;

	int mins = Lockdown::CountdownSeconds / 60;
	int secs = Lockdown::CountdownSeconds % 60;
	Lockdown::Enabled = 1;
	sprintf(Lockdown::NotifyIconData.szTip, "Lock in %02d:%02d", mins, secs);

	Lockdown::NotifyIconData.hIcon = LoadIcon(hinstance, (LPCTSTR)MAKEINTRESOURCE(IDI_LOCKDOWN_ICON));
	Lockdown::NotifyIconData.uCallbackMessage = WM_USER_TRAYICON;
	Lockdown::NotifyIconAdded = Shell_NotifyIcon(NIM_ADD, &Lockdown::NotifyIconData);

	// Send a timer message every second.
	SetTimer(hwnd, 42, 1000, NULL);

	// Hook into gamepad/controller events.
	auto hook = gamepad::hook::make();
	hook->set_plug_and_play(true, gamepad::ms(1000));
	hook->set_sleep_time(gamepad::ms(100)); // 10fps poll.
	hook->set_button_event_handler(Lockdown::Hook_GamepadButton);
	hook->set_axis_event_handler(Lockdown::Hook_GamepadAxis);
	hook->set_connect_event_handler(Lockdown::Hook_GamepadConnect);
	hook->set_disconnect_event_handler(Lockdown::Hook_GamepadDisconnect);

	if (!hook->start())
	{
		tPrintf("Couldn't start gamepad hook.\n");
		return Lockdown::ExitCode_XInputGamepadHookFailure;
	}

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return Lockdown::ExitCode_Success;
}
