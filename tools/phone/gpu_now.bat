@echo off
net session >nul 2>&1 || (powershell -NoProfile -Command "Start-Process -Verb RunAs -FilePath %~f0" & exit /b)
powershell -ExecutionPolicy Bypass -File C:\gpu_now.ps1
pause
