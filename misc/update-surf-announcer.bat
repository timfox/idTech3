@echo off
REM Downloads the latest Surf announcer pk3 and removes stale checksum-named
REM variants left by auto-download.
REM
REM Place announcer sounds in zzz-surf-announcer.pk3 (sound/ paths as usual).
REM Drop the pk3 into the surf game directory (fs_game), e.g. release\surf\.
REM
REM Override with SURF_ANNOUNCER_URL, or set SURF_ANNOUNCER_REPO to a GitHub
REM "owner/repo" whose latest release attaches zzz-surf-announcer.pk3.
REM Placeholder default (replace when a public release exists):
REM   https://github.com/OWNER/surf/releases/latest/download/zzz-surf-announcer.pk3

setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "REPO_ROOT=%SCRIPT_DIR%.."
set "ANNOUNCER_PK3=zzz-surf-announcer.pk3"

if defined SURF_GAME_DIR (
    set "SURF_DIR=%SURF_GAME_DIR%"
) else if exist "%REPO_ROOT%\release\surf\" (
    set "SURF_DIR=%REPO_ROOT%\release\surf"
) else (
    set "SURF_DIR=%SCRIPT_DIR%surf"
)

if not defined SURF_ANNOUNCER_REPO set "SURF_ANNOUNCER_REPO=OWNER/surf"
if not defined SURF_ANNOUNCER_URL (
    set "SURF_ANNOUNCER_URL=https://github.com/%SURF_ANNOUNCER_REPO%/releases/latest/download/%ANNOUNCER_PK3%"
)

if not exist "%SURF_DIR%" mkdir "%SURF_DIR%"

REM Remove checksummed variants and the current copy, then download fresh.
del /q "%SURF_DIR%\zzz-surf-announcer.*.pk3" 2>nul
del /q "%SURF_DIR%\%ANNOUNCER_PK3%" 2>nul

echo Downloading Surf announcer from:
echo   %SURF_ANNOUNCER_URL%
echo   -^> %SURF_DIR%\%ANNOUNCER_PK3%

curl -fL -o "%SURF_DIR%\%ANNOUNCER_PK3%" "%SURF_ANNOUNCER_URL%"
if errorlevel 1 (
    echo   FAILED ^(set SURF_ANNOUNCER_URL to a real download URL^)
    del /q "%SURF_DIR%\%ANNOUNCER_PK3%" 2>nul
    exit /b 1
)

echo   OK
echo Done.
endlocal
