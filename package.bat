@echo off
REM ===========================================================================
REM  package.bat - gather NextGenTweaks.exe and EVERY file it needs into dist\
REM
REM  This does NOT bundle anything. It just collects the exe, its Qt DLLs, the
REM  platform plugin, and the runtime DLLs into one clean folder so you can point
REM  Enigma Virtual Box at it (see DEPLOYMENT.md, Part 5).
REM
REM  Run it from the project root after building:  package.bat
REM
REM  If your Qt / MinGW is installed somewhere else, edit the three paths below.
REM ===========================================================================
setlocal

set "QT_BIN=C:\Qt\6.11.1\mingw_64\bin"
set "MINGW_BIN=C:\Qt\Tools\mingw1310_64\bin"
set "OPENSSL_BIN=C:\Qt\Tools\mingw1310_64\opt\bin"

set "ROOT=%~dp0"
set "BUILT_EXE=%ROOT%build\NextGenTweaks.exe"
set "DIST=%ROOT%dist"

echo.
echo === NextGen Tweaks packaging: gathering files into dist\ ===
echo.

if not exist "%BUILT_EXE%" (
    echo ERROR: %BUILT_EXE% not found. Build the app first.
    exit /b 1
)
if not exist "%QT_BIN%\windeployqt.exe" (
    echo ERROR: windeployqt not found at %QT_BIN%. Edit QT_BIN at the top of this file.
    exit /b 1
)

REM Fresh dist folder.
if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%"

echo Copying the executable...
copy /y "%BUILT_EXE%" "%DIST%\NextGenTweaks.exe" >nul

echo Running windeployqt (Qt DLLs + platform plugin)...
"%QT_BIN%\windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw --compiler-runtime "%DIST%\NextGenTweaks.exe"
if errorlevel 1 (
    echo ERROR: windeployqt failed.
    exit /b 1
)

echo Copying the MinGW runtime DLLs...
copy /y "%MINGW_BIN%\libgcc_s_seh-1.dll" "%DIST%\" >nul 2>&1
copy /y "%MINGW_BIN%\libstdc++-6.dll"    "%DIST%\" >nul 2>&1
copy /y "%MINGW_BIN%\libwinpthread-1.dll" "%DIST%\" >nul 2>&1

echo Copying the OpenSSL DLL...
copy /y "%OPENSSL_BIN%\libcrypto-1_1-x64.dll" "%DIST%\" >nul 2>&1
if not exist "%DIST%\libcrypto-1_1-x64.dll" copy /y "%OPENSSL_BIN%\libcrypto-3-x64.dll" "%DIST%\" >nul 2>&1

echo.
echo === Done. Everything is in: %DIST%
echo.
echo Next steps:
echo   1. Double-click dist\NextGenTweaks.exe to confirm it opens with nothing else.
echo   2. Open Enigma Virtual Box, input = dist\NextGenTweaks.exe, add the dist
echo      folder (Add Folder Recursive), enable Compress + file virtualization,
echo      then Process. See DEPLOYMENT.md Part 5.
echo.
endlocal
