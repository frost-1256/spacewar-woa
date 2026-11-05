@echo off
setlocal
pushd "%~dp0"
if not defined WindowsSdkDir goto :env_error
if not defined WindowsSDKVersion goto :env_error
set "DSC_KIT_VERSION=%WindowsSDKVersion%"
if "%DSC_KIT_VERSION:~-1%"=="\" set "DSC_KIT_VERSION=%DSC_KIT_VERSION:~0,-1%"
set "DSC_KIT_ROOT=%WindowsSdkDir%"
if not "%DSC_KIT_ROOT:~-1%"=="\" set "DSC_KIT_ROOT=%DSC_KIT_ROOT%\"
if not exist "%DSC_KIT_ROOT%Include\%DSC_KIT_VERSION%\km\ntddk.h" goto :env_error
if not exist "%DSC_KIT_ROOT%Lib\%DSC_KIT_VERSION%\km\arm64\aux_klib.lib" goto :env_error
where cl.exe >nul 2>nul
if errorlevel 1 goto :env_error
where link.exe >nul 2>nul
if errorlevel 1 goto :env_error
cl.exe /nologo /c /TC /kernel /W4 /WX /O1 /Oi /GS- /Zl /D_KERNEL_MODE /D_WIN64 /DWINVER=0x0A00 /D_WIN32_WINNT=0x0A00 /DNTDDI_VERSION=0x0A00000A /I"%DSC_KIT_ROOT%Include\%DSC_KIT_VERSION%\km" /I"%DSC_KIT_ROOT%Include\%DSC_KIT_VERSION%\shared" /I"%DSC_KIT_ROOT%Include\%DSC_KIT_VERSION%\ucrt" /FoDscProbe.obj DscProbe.c
if errorlevel 1 goto :failed
link.exe /nologo /DRIVER /MACHINE:ARM64 /SUBSYSTEM:NATIVE /ENTRY:DriverEntry /NODEFAULTLIB /INCREMENTAL:NO /RELEASE /OPT:REF /OPT:ICF /OUT:DscProbe.sys /LIBPATH:"%DSC_KIT_ROOT%Lib\%DSC_KIT_VERSION%\km\arm64" DscProbe.obj ntoskrnl.lib aux_klib.lib
if errorlevel 1 goto :failed
echo Built DscProbe.sys
popd
exit /b 0
:env_error
echo Run from an EWDK LaunchBuildEnv shell targeting ARM64 with WindowsSdkDir and WindowsSDKVersion set. 1>&2
:failed
popd
exit /b 1
