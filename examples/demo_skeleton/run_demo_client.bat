@echo off
setlocal EnableExtensions
rem Demo launcher (Windows). Put idtech3.exe next to this file or on PATH.
rem Layout: same folder as this file must contain base\ (or baseq3\) and idtech3_demo\idtech3_demo.pk3

if not defined IDTECH3_DEMO_ROOT set "IDTECH3_DEMO_ROOT=%~dp0"
if "%IDTECH3_DEMO_ROOT:~-1%"=="\" set "IDTECH3_DEMO_ROOT=%IDTECH3_DEMO_ROOT:~0,-1%"

if not exist "%IDTECH3_DEMO_ROOT%\idtech3_demo\idtech3_demo.pk3" (
  echo Missing: %IDTECH3_DEMO_ROOT%\idtech3_demo\idtech3_demo.pk3
  echo Build the pack from the engine repo: examples\demo_game\build_demo_pack.sh
  exit /b 2
)

if not defined DEMO_BASE_DIR set "DEMO_BASE_DIR=base"
if not exist "%IDTECH3_DEMO_ROOT%\%DEMO_BASE_DIR%" (
  if "%DEMO_BASE_DIR%"=="base" if exist "%IDTECH3_DEMO_ROOT%\baseq3" (
    echo Found baseq3 but not base. Set DEMO_BASE_DIR=baseq3 in the environment or in this file.
    exit /b 2
  )
  echo Missing game data folder: %IDTECH3_DEMO_ROOT%\%DEMO_BASE_DIR%
  exit /b 2
)

if not defined DEMO_RENDERER set "DEMO_RENDERER=vulkan"

if exist "%~dp0idtech3.exe" (set "ENGINE=%~dp0idtech3.exe") else (set "ENGINE=idtech3.exe")

set "BGARG="
if not "%DEMO_BASE_DIR%"=="base" set "BGARG=+set fs_basegame %DEMO_BASE_DIR%"

set "MAPARG="
if defined DEMO_MAP set "MAPARG=+map %DEMO_MAP%"

"%ENGINE%" +set fs_basepath "%IDTECH3_DEMO_ROOT%" +set fs_game idtech3_demo %BGARG% +set cl_renderer %DEMO_RENDERER% %MAPARG% %*
endlocal
