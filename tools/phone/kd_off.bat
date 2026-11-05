@echo off
fltmc >nul 2>&1 || (powershell -Command "Start-Process -Verb RunAs -FilePath %~f0" & exit /b)
bcdedit /debug off
echo KD OFF - standalone boot
pause
