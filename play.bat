@echo off
rem ====================================================================
rem  Heart of The Alien Redux 2.0.1 - Windows launcher
rem
rem  Double-click this file to open the setup screen. It shows the
rem  current options and their defaults; change anything you like and
rem  then choose START GAME.
rem
rem      play.bat                 setup screen
rem      play.bat --setup         setup screen (explicit)
rem      play.bat --fullscreen    start immediately with extra options
rem
rem  In the release zip this file sits next to alien.exe. In a source
rem  checkout it finds build\alien.exe automatically.
rem
rem  Game data:
rem      Heart Of The Alien (U)\Heart Of The Alien (U).iso
rem ====================================================================
setlocal
title Heart of The Alien Redux - Setup
cd /d "%~dp0"

set "GAME=%~dp0alien.exe"
if not exist "%GAME%" if exist "%~dp0build\alien.exe" set "GAME=%~dp0build\alien.exe"

if not exist "%GAME%" (
    echo.
    echo ERROR: alien.exe was not found.
    echo Keep play.bat in the same folder as alien.exe, or build the game
    echo first so that build\alien.exe exists.
    echo.
    pause
    exit /b 1
)

rem Prefer the setup screen. It also runs the game, so a missing ISO is
rem reported there with a chance to change the path.
if not exist "%~dp0play.ps1" goto fallback
set "PS=powershell"
where powershell >nul 2>nul
if not errorlevel 1 goto runmenu
set "PS=pwsh"
where pwsh >nul 2>nul
if errorlevel 1 goto fallback

:runmenu
"%PS%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0play.ps1" %*
set "rc=%ERRORLEVEL%"
if not "%rc%"=="0" (
    echo.
    echo The launcher exited with code %rc%.
    pause
)
exit /b %rc%

:fallback
rem Systems without PowerShell keep the old direct launch.
echo.
echo NOTE: the setup screen needs play.ps1 and PowerShell.
echo Starting the game with the default settings.
echo.
if not exist "Heart Of The Alien (U)\Heart Of The Alien (U).iso" (
    echo.
    echo NOTE: "Heart Of The Alien (U)\Heart Of The Alien (U).iso" was not found.
    echo A copy of the original Sega CD game is required. Put its ISO in that
    echo folder, or point the game at another file with:
    echo     play.bat --iso "My Rip"
    echo.
)

"%GAME%" --iso --hd --hd-smooth 30 %*
set "rc=%ERRORLEVEL%"
if not "%rc%"=="0" (
    echo.
    echo The game exited with code %rc%.
    pause
)

exit /b %rc%
