@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"

if /I "%~1"=="test" goto test
if /I "%~1"=="clean" goto clean

clang -std=c99 -O0 -g -Wall -o mirage.exe main.c tokenize.c parse.c codegen.c type.c string.c
exit /b 0

:test
clang -std=c99 -O2 -o mirage.exe main.c tokenize.c parse.c codegen.c type.c string.c
for %%f in (test\*.c) do (
    set "_s=test\%%~nf.s"
    set "_e=test\%%~nf.exe"

    clang -o- -E -P -C %%f | mirage.exe -o !_s! -
    if errorlevel 1 exit /b !errorlevel!

    clang -o !_e! !_s! -xc test/common
    if errorlevel 1 exit /b !errorlevel!

    !_e!
    if errorlevel 1 exit /b !errorlevel!
    echo ----------------------------------------
)
exit /b 0

:clean
del /s /f /q *.exe *.pdb *.s *.o >nul 2>nul
del /s /f /q tmp* >nul 2>nul
exit /b 0
