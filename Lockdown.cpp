// Lockdown.cpp
//
// Locks the computer after a period of inactivity. As of 2020_03_03 on Windows 10
// it seems sometimes setting a screensaver timeout does not reliably lock the machine.
// This simple system-tray app will hopefully be more reliable.
//
// Copyright (c) 2020 Tristan Grimmer.
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
#include <tchar.h>
#include <commctrl.h>
#include "resource.h"
#include <stdio.h>
#pragma warning(disable: 4996)


#define LockdownVersion "V 1.0.1"
#define	WM_USER_TRAYICON (WM_USER+1)


namespace Lockdown
{
	HINSTANCE hInst;
	NOTIFYICONDATA NotifyIconData;
	HHOOK hKeyboardHook						= NULL;
	HHOOK hMouseHook						= NULL;
	BOOL NotifyIconAdded					= 0;

	int SecondsToLock						= 20 * 60;
	int CountdownSeconds					= SecondsToLock;

	LRESULT CALLBACK MainWinProc(HWND hwnd, UINT message, WPARAM, LPARAM);
	LRESULT CALLBACK KeyboardHook(int code, WPARAM, LPARAM);
	LRESULT CALLBACK MouseHook(int code, WPARAM, LPARAM);
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

			CountdownSeconds--;
			int mins = CountdownSeconds / 60;
			int secs = CountdownSeconds % 60;

			if (NotifyIconAdded)
			{
				sprintf(NotifyIconData.szTip, "Lock in %02d:%02d", mins, secs);
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
					::MessageBox
					(
						hwnd,
						"Lockdown " LockdownVersion " by Tristan Grimmer.\n"
						"Under ISC licence (similar to MIT).\n\n"
						"Homepage at https://github.com/bluescan/lockdown\n\n"
						"Usage: Consider running as a scheduled task on logon.\n"
						"Do not terminate the task. An optional single integer\n"
						"command line parameter may given to specify the number\n"
						"of minutes. If no parameter 20 minutes is used. Timer is\n"
						"reset on keypress and mouse buttons. Mouse movement\n"
						"alone is NOT considered and will not reset the timer.\n",
						"About Lockdown", MB_OK | MB_ICONINFORMATION
					);
					break;

				case ID_MENU_QUIT:
				{
					int result = ::MessageBox(hwnd, "If you quit the Lockdown app your computer will not automatically lock.\n\nAre you sure you want to quit?", "Quit Lockdown", MB_YESNO | MB_ICONEXCLAMATION);
					if (result == IDYES)
						DestroyWindow(hwnd);
					break;
				}

				case ID_MENU_LOCK10:
					CountdownSeconds = 10;
					break;

				case ID_MENU_LOCKNOW:
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


LRESULT CALLBACK Lockdown::KeyboardHook(int code, WPARAM wparam, LPARAM lparam)
{
	if (wparam == WM_KEYDOWN)
		CountdownSeconds = SecondsToLock;

	return CallNextHookEx(hKeyboardHook, code, wparam, lparam);
}


LRESULT CALLBACK Lockdown::MouseHook(int code, WPARAM wparam, LPARAM lparam)
{
	if
	(
		(wparam == WM_LBUTTONDOWN) || (wparam == WM_LBUTTONUP) ||
		(wparam == WM_RBUTTONDOWN) || (wparam == WM_RBUTTONUP) ||
		(wparam == WM_MOUSEWHEEL)
	)
	{
		CountdownSeconds = SecondsToLock;
	}

	return CallNextHookEx(hKeyboardHook, code, wparam, lparam);
}


int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprevInstance, LPSTR cmdLine, int cmdShow)
{
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

	Lockdown::hKeyboardHook		= SetWindowsHookEx(WH_KEYBOARD_LL,	Lockdown::KeyboardHook,	NULL, 0);
	Lockdown::hMouseHook		= SetWindowsHookEx(WH_MOUSE_LL,		Lockdown::MouseHook,	NULL, 0);

	Lockdown::hInst = hinstance;

	INITCOMMONCONTROLSEX comControls;
	memset(&comControls, 0, sizeof(comControls));
	comControls.dwSize		= sizeof(comControls);
	comControls.dwICC		= ICC_UPDOWN_CLASS | ICC_LISTVIEW_CLASSES;
	if (!InitCommonControlsEx(&comControls))
		return 1;

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
	winClass.lpszClassName		= "Lockdown";
	winClass.hIconSm			= LoadIcon(hinstance, (LPCTSTR)MAKEINTRESOURCE(IDI_LOCKDOWN_ICON));
	if (!RegisterClassEx(&winClass))
		return 2;

	HWND hwnd = CreateWindowEx
	(
		WS_EX_CLIENTEDGE,	"Lockdown",			"Lockdown",		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,		CW_USEDEFAULT,		CW_USEDEFAULT,	CW_USEDEFAULT,
		NULL,				NULL,				hinstance,		NULL
	);

	if (!hwnd)
		return 3;

	// System tray icon.
	memset(&Lockdown::NotifyIconData, 0, sizeof(Lockdown::NotifyIconData));
	Lockdown::NotifyIconData.cbSize			= sizeof(Lockdown::NotifyIconData);
	Lockdown::NotifyIconData.hWnd			= hwnd;
	Lockdown::NotifyIconData.uID			= IDI_LOCKDOWN_ICON;
	Lockdown::NotifyIconData.uFlags			= NIF_ICON | NIF_MESSAGE | NIF_TIP;

	int mins = Lockdown::CountdownSeconds / 60;
	int secs = Lockdown::CountdownSeconds % 60;
	sprintf(Lockdown::NotifyIconData.szTip, "Lock in %02d:%02d", mins, secs);

	Lockdown::NotifyIconData.hIcon = LoadIcon(hinstance, (LPCTSTR)MAKEINTRESOURCE(IDI_LOCKDOWN_ICON));
	Lockdown::NotifyIconData.uCallbackMessage = WM_USER_TRAYICON;

	Lockdown::NotifyIconAdded = Shell_NotifyIcon(NIM_ADD, &Lockdown::NotifyIconData);

	// Send a timer message every second.
	SetTimer(hwnd, 42, 1000, NULL);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}
