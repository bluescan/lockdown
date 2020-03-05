# lockdown
A system tray app to force your Windows workstation to become locked after a period of inactivity.

Sometimes Windows 10 fails to lock even if you set the screensaver timeout and check the lock box.
I suspect noisy mouse movements resetting the timer. This app is an unobtrusive alternative to the screensaver timeout. If you encrypt you drive partitions for example, it's likely important to you that your machine locks when left unattended.

This screenshot shows the tray icon (the lock) and the menu. The default timeout is 20 minutes, but it is settable as a commandline parameter.

![Lockdown](https://raw.githubusercontent.com/bluescan/lockdown/master/Screenshots/LockdownScreenshot.png)



# usage

Consider running as a scheduled task on logon.

![Lockdown](https://raw.githubusercontent.com/bluescan/lockdown/master/Screenshots/LockdownTaskTriggers.png)


An optional single integer command line parameter may given to specify the number of minutes. If no parameter 20 minutes is used. Timer is reset on keypress and mouse buttons. Mouse movement alone is NOT considered and will not reset the timer.
![Lockdown](https://raw.githubusercontent.com/bluescan/lockdown/master/Screenshots/LockdownTaskActionss.png)


Do not terminate the task.

![Lockdown](https://raw.githubusercontent.com/bluescan/lockdown/master/Screenshots/LockdownTaskSettings.png)
