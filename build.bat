@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"

if /I "%~1"=="test" goto test
if /I "%~1"=="clean" goto clean

clang -std=c23 -O0 -g -Wall -o build\mirage.exe main.c tokenize.c parse.c codegen.c type.c string.c preprocess.c
exit /b 0

:test
clang -std=c23 -O2 -o build\mirage.exe main.c tokenize.c parse.c codegen.c type.c string.c preprocess.c
for %%f in (test\*.c) do (
    echo ---------------------- %%~nf start ----------------------
    set "tmp_obj=build\%%~nf.obj"
    set "tmp_exe=build\%%~nf.exe"

    build\mirage.exe -c -o !tmp_obj! %%f
    if !errorlevel! neq 0 exit /b !errorlevel!

    clang -o !tmp_exe! !tmp_obj! -xc test/common
    if !errorlevel! neq 0 exit /b !errorlevel!

    !tmp_exe!
    if !errorlevel! neq 0 exit /b !errorlevel!
)
exit /b 0

:clean
del /s /f /q *.exe *.pdb *.s *.o *.obj >nul 2>nul
del /s /f /q tmp* >nul 2>nul
exit /b 0
