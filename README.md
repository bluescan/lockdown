# lockdown
A system tray app to force your Windows workstation to become locked after a period of inactivity.

Sometimes Windows 10 fails to lock even if you set the screensaver timeout and check the lock box.
I suspect noisy mouse movements resetting the timer. This app is an alternative to the screensaver timeout.

# usage

Consider running as a scheduled task on logon. Do not terminate the task. An optional single integer command line parameter may given to specify the number of minutes.

If no parameter 20 minutes is used. Timer is reset on keypress and mouse buttons.

Mouse movement alone is NOT considered and will not reset the timer.


