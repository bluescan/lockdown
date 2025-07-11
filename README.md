# lockdown
A system tray app to force your Windows workstation to become locked after a period of inactivity.

Sometimes Windows 11 fails to lock even if you set the screensaver timeout and check the lock box. This app is an unobtrusive alternative to the screensaver timeout. Additionally this tray application can also monitor gamepads for input, making sure the computer does not lock if a gamepad button or joystick is moved. If you encrypt your drive partitions for physical security, you probably want to make sure your machine locks when left unattended.

This screenshot shows the lockdown system tray icon (on the left) and the lockdown menu. The default timeout is 20 minutes and is settable as a command-line parameter if you want a different value. Hovering over the lock icon shows the current remaining time (in minutes and seconds) before the machine locks.

![Lockdown](https://raw.githubusercontent.com/bluescan/lockdown/master/Screenshots/LockdownScreenshot.png)


# usage

Consider running as a scheduled task on logon.

![Lockdown](https://raw.githubusercontent.com/bluescan/lockdown/master/Screenshots/LockdownTaskTriggers.png)


There are various command line parameters to control what inputs are monitored and to set timeout durations. To view the available options type lockdown.exe -h. The default timeout is 20 minutes. The countdown resets on key-presses, mouse button clicks, mouse movement beyond a reasonable threshold, gamepad button presses, and gamepad joystick/trigger input.

![Lockdown](https://raw.githubusercontent.com/bluescan/lockdown/master/Screenshots/LockdownTaskActions.png

Do not terminate the task.

![Lockdown](https://raw.githubusercontent.com/bluescan/lockdown/master/Screenshots/LockdownTaskSettings.png)
