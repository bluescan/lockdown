# lockdown
A system tray app to force your Windows workstation to become locked after a period of inactivity.

Sometimes Windows 10 fails to lock even if you set the screensaver timeout and check the lock box.
I suspect noisy mouse movements resetting the timer. This app is an unobtrusive alternative to the screensaver timeout. For example, if you encrypt your drive partitions for physical security, you probably want to make sure your machine locks when left unattended.

This screenshot shows the lockdown system tray icon (on the left) and the lockdown menu. The default timeout is 20 minutes and is settable as a command-line parameter if you want a different value. Hovering over the lock icon shows the current remaining time (in minutes and seconds) before the machine locks.

![Lockdown](https://raw.githubusercontent.com/bluescan/lockdown/master/Screenshots/LockdownScreenshot.png)


# usage

Consider running as a scheduled task on logon.

![Lockdown](https://raw.githubusercontent.com/bluescan/lockdown/master/Screenshots/LockdownTaskTriggers.png)


An optional single integer command line parameter may be given to specify the number of minutes. No parameter means 20 minutes is used. The countdown resets on key-presses and mouse button clicks. Mouse movement alone is NOT considered and will not reset the timer.
![Lockdown](https://raw.githubusercontent.com/bluescan/lockdown/master/Screenshots/LockdownTaskActions.png)


Do not terminate the task.

![Lockdown](https://raw.githubusercontent.com/bluescan/lockdown/master/Screenshots/LockdownTaskSettings.png)
