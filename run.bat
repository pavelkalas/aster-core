@echo off
setlocal

set "PROJECT=%~dp0"
set "IMG=%PROJECT%build\aster.img"
set "DATA=%PROJECT%asterfs.img"

if not exist "%IMG%" (
    echo ERROR: %IMG% not found. Run build.bat first!
    exit /b 1
)

echo ============================================
echo   Starting AsterOS in QEMU...
echo   Kernel : %IMG%
echo   Data   : %DATA%
echo ============================================
echo.

qemu-system-x86_64 ^
    -m 128M ^
    -drive format=raw,file="%IMG%",if=ide,index=0 ^
    -drive format=raw,file="%DATA%",if=ide,index=1

exit /b 0
