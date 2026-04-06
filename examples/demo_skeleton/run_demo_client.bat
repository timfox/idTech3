@echo off
setlocal EnableExtensions
rem Place beside base\ and idtech3_demo\, or set IDTECH3_DEMO_ROOT to that folder.
rem idtech3.exe on PATH or in this directory.

if not defined IDTECH3_DEMO_ROOT set "IDTECH3_DEMO_ROOT=%~dp0"
rem trim trailing backslash
if "%IDTECH3_DEMO_ROOT:~-1%"=="\" set "IDTECH3_DEMO_ROOT=%IDTECH3_DEMO_ROOT:~0,-1%"

if not exist "%IDTECH3_DEMO_ROOT%\idtech3_demo\idtech3_demo.pk3" (
  echo Missing idtech3_demo\idtech3_demo.pk3 under %IDTECH3_DEMO_ROOT%
  exit /b 2
)

if not defined DEMO_RENDERER set "DEMO_RENDERER=vulkan"

if exist "%~dp0idtech3.exe" (set "ENGINE=%~dp0idtech3.exe") else (set "ENGINE=idtech3.exe")

set "MAPARG="
if defined DEMO_MAP set "MAPARG=+map %DEMO_MAP%"

"%ENGINE%" +set fs_basepath "%IDTECH3_DEMO_ROOT%" +set fs_game idtech3_demo +set cl_renderer %DEMO_RENDERER% %MAPARG% %*
endlocal
