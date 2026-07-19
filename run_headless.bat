@echo off
REM run_headless.bat
REM Builds and runs the Hay Day headless client using the user-local .NET SDK

SET DOTNET="%LOCALAPPDATA%\dotnet\dotnet.exe"
SET PROJECT=d:\HD\HaydayMod\third_party\SupercellProxy\src\SupercellProxy.Playground

echo [*] Building...
%DOTNET% build "%PROJECT%" -c Release --nologo

if %ERRORLEVEL% NEQ 0 (
    echo [!] Build failed.
    exit /b 1
)

echo.
echo [*] Running headless login...
%DOTNET% run --project "%PROJECT%" -c Release --no-build
