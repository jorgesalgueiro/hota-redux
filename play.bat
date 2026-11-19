@echo off
rem ====================================================================
rem  Heart of The Alien Redux 2.0.1 - Windows launcher
rem
rem  Starts the game from the Sega CD ISO with the normal settings:
rem  HD renderer, light diagonal smoothing, keys from keys.txt.
rem
rem  In the release zip this file sits next to alien.exe. In a source
rem  checkout it finds build\alien.exe automatically.
rem
rem  Game data:
rem      Heart Of The Alien (U)\Heart Of The Alien (U).iso
rem
rem  Extra arguments are passed through to the game, for example:
rem      play.bat --fullscreen
rem      play.bat --sharp
rem      play.bat --hd-smooth 15
rem      play.bat --iso "My Rip"
rem ====================================================================
setlocal
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
