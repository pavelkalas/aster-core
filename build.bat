@echo off
setlocal

set "MSYS2=C:\msys64"
set "PROJECT=%~dp0"

rem Convert Windows path to MSYS2 format (e.g. /c/Users/cmpn/Desktop/aster-core)
set "MSYS_DIR=%PROJECT:\=/%"
set "MSYS_DIR=/%MSYS_DIR::=%"
rem Strip trailing /
if "%MSYS_DIR:~-1%"=="/" set "MSYS_DIR=%MSYS_DIR:~0,-1%"

echo [1/3] Compiling memcpy.o ...
"%MSYS2%\usr\bin\bash.exe" -c "export PATH=\"/usr/bin:/mingw64/bin:\$PATH\" && cd \"%MSYS_DIR%\" && clang --target=x86_64-unknown-elf -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -fno-stack-protector -fno-builtin -ffreestanding -nostdlib -nostdlibinc -c tools/memcpy.c -o tools/memcpy.o"
if %ERRORLEVEL% neq 0 (
    echo ERROR: Failed to compile memcpy.o
    exit /b 1
)

echo [2/3] Cleaning previous build ...
"%MSYS2%\usr\bin\bash.exe" -c "export PATH=\"/usr/bin:/mingw64/bin:%MSYS_DIR%/tools:\$PATH\" && cd \"%MSYS_DIR%\" && CROSS=x86_64-elf- make clean"
if %ERRORLEVEL% neq 0 (
    echo ERROR: Clean failed
    exit /b 1
)

echo [3/3] Building kernel ...
"%MSYS2%\usr\bin\bash.exe" -c "export PATH=\"/usr/bin:/mingw64/bin:%MSYS_DIR%/tools:\$PATH\" && cd \"%MSYS_DIR%\" && CROSS=x86_64-elf- make"
if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo ============================================
echo   BUILD SUCCESSFUL!
echo   Output: %PROJECT%build\aster.img
echo ============================================
exit /b 0
