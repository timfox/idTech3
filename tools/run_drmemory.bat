@echo off
REM Dr. Memory memory checker script for id Tech 3 (Windows)
REM Usage: run_drmemory.bat <executable> [args...]

if "%~1"=="" (
    echo Usage: %0 ^<executable^> [args...]
    echo Example: %0 ioquake3.x86_64.exe +set dedicated 1 +map q3dm1
    goto :eof
)

set EXECUTABLE=%~1
shift

REM Check if drmemory is in PATH
where drmemory >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: drmemory not found in PATH.
    echo Install Dr. Memory from: https://drmemory.org/
    goto :eof
)

REM Build the argument string
set ARGS=
:argloop
if "%~1"=="" goto :endargs
set ARGS=%ARGS% %1
shift
goto :argloop
:endargs

echo Starting %EXECUTABLE% with Dr. Memory...
echo Results will be displayed in the Dr. Memory window
echo.

REM Run with Dr. Memory
drmemory -batch -results_to_stderr -callstack_max_frames 20 %EXECUTABLE%%ARGS%

echo.
echo Dr. Memory analysis complete.