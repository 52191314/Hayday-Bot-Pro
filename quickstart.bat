@echo off
TITLE "Hayday-Bot-Pro Quickstart Launcher"

echo ========================================================
echo   Hayday-Bot-Pro Quickstart Launcher
echo ========================================================
echo.

:: Check for ADB
where adb >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [!] WARNING: adb command not found in system PATH.
    echo     Please make sure Android SDK Platform-Tools or your emulator's ADB is installed.
) else (
    echo [OK] ADB found in PATH.
)

:: Check for compiled executable in HD\x64\Release\premium.exe
if exist "HD\x64\Release\premium.exe" (
    echo [OK] Compiled binary found: HD\x64\Release\premium.exe
    echo.
    echo Launching Hayday-Bot-Pro GUI...
    start "" "HD\x64\Release\premium.exe"
    exit /b 0
)

:: Check for compiled executable in x64\Release\premium.exe
if exist "x64\Release\premium.exe" (
    echo [OK] Compiled binary found: x64\Release\premium.exe
    echo.
    echo Launching Hayday-Bot-Pro GUI...
    start "" "x64\Release\premium.exe"
    exit /b 0
)

echo [!] Pre-compiled binary not found at HD\x64\Release\premium.exe
echo     Attempting to locate Visual Studio Build Tools / MSBuild...

:: Search for vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
        set "MSBUILD=%%i"
    )
)

if defined MSBUILD (
    echo [OK] MSBuild found at: "%MSBUILD%"
    echo Building HD/Premium bot.sln (Release^|x64)...
    "%MSBUILD%" "HD\Premium bot.sln" /p:Configuration=Release /p:Platform=x64
    if %ERRORLEVEL% EQU 0 (
        echo [OK] Build succeeded! Launching bot...
        start "" "HD\x64\Release\premium.exe"
        exit /b 0
    ) else (
        echo [ERROR] Build failed. Please open HD\Premium bot.sln in Visual Studio 2022 to inspect errors.
        pause
        exit /b 1
    )
) else (
    echo [!] MSBuild not detected automatically.
    echo     Please open 'HD\Premium bot.sln' in Visual Studio 2022 and build 'Release^|x64'.
    pause
    exit /b 1
)
