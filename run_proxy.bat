@echo off
REM run_proxy.bat
REM Builds and runs the SupercellProxy MitM decryption proxy using the user-local .NET SDK

SET DOTNET="%LOCALAPPDATA%\dotnet\dotnet.exe"
SET PROJECT="%~dp0third_party\SupercellProxy\src\SupercellProxy.Playground"

echo [*] Building...
%DOTNET% build %PROJECT% -c Release --nologo

if %ERRORLEVEL% NEQ 0 (
    echo [!] Build failed.
    exit /b 1
)

echo.
echo [*] Running SupercellProxy Decryption Proxy...
%DOTNET% run --project %PROJECT% -c Release --no-build -- -p
