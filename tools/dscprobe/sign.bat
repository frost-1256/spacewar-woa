@echo off
setlocal
pushd "%~dp0"
if not exist DscProbe.sys goto :failed
where signtool.exe >nul 2>nul
if errorlevel 1 goto :failed
rem Default certificate location is CurrentUser; deliberately omit /sm.
rem /n matches the existing certificate's subject name; /a selects a valid match.
signtool.exe sign /s My /n "spacewar test" /a /fd SHA256 DscProbe.sys
if errorlevel 1 goto :failed
popd
exit /b 0
:failed
echo Signing failed. Check DscProbe.sys, signtool on PATH, and CN=spacewar test in CurrentUser\My with its private key. 1>&2
popd
exit /b 1
