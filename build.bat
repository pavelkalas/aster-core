@echo off
setlocal

set "PROJECT=%~dp0"
if "%PROJECT:~-1%"=="\" set "PROJECT=%PROJECT:~0,-1%"
for /f "usebackq delims=" %%P in (`wsl -d Ubuntu -- wslpath "%PROJECT%"`) do set "WSL_PROJECT=%%P"

if "%WSL_PROJECT%"=="" (
    echo ERROR: Ubuntu WSL was not found.
    exit /b 1
)

echo [1/1] Building AsterOS in Ubuntu WSL...
wsl -d Ubuntu -- bash -lc "cd \"%WSL_PROJECT%\" && make"
if %ERRORLEVEL% neq 0 (
    echo ERROR: WSL build failed
    exit /b 1
)

echo.
echo ============================================
echo   BUILD SUCCESSFUL!
echo   Output: %PROJECT%\build\aster.img
echo ============================================
exit /b 0
