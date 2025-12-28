@echo off
set "ROOT_DIR=%~dp0.."
set "ENGINE=%ROOT_DIR%release\idtech3.x86_64.exe"
set "MODS=%ROOT_DIR%mods"
if not exist "%ENGINE%" (
  echo Engine binary not found: %ENGINE%
  goto :eof
)
echo Running engine with mods %MODS%
"%ENGINE%" -mods "%MODS%"
goto :eof

