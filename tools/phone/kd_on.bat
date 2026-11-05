@echo off
fltmc >nul 2>&1 || (powershell -Command "Start-Process -Verb RunAs -FilePath %~f0" & exit /b)
bcdedit /debug on
echo KD ON - reboot with WinDbg attached
pause
