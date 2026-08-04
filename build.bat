@echo off
setlocal enabledelayedexpansion
cd /D "%~dp0"

set "cx=%CD%\mirage.exe"
set "cc=%CD%\cc.exe"
set "clang_common=-std=c23 -o %cx% main.c tokenize.c parse.c codegen.c type.c string.c preprocess.c arena.c"

if not exist build   mkdir build
if /I "%~1"=="test"  goto  test
if /I "%~1"=="clean" goto  clean
if /I "%~1"=="perf"  goto  perf

clang -O0 -g -Wall %clang_common%
exit /b 0

:test
clang -O2 %clang_common%
for %%f in (test\*.c) do (
    echo ---------------------- %%~nf start ----------------------
    set "tmp_asm=build\%%~nf.s"
    set "tmp_exe=build\%%~nf.exe"

    %cx% -S -o !tmp_asm! %%f -Itest
    if !errorlevel! neq 0 exit /b !errorlevel!

    clang -o !tmp_exe! !tmp_asm! -xc test/common
    if !errorlevel! neq 0 exit /b !errorlevel!

    !tmp_exe!
    if !errorlevel! neq 0 exit /b !errorlevel!
)
exit /b 0

:perf
clang -O2 %clang_common% -D PERF

%cx% -o %cc% main.c tokenize.c preprocess.c parse.c codegen.c type.c string.c arena.c -Iperf
if !errorlevel! neq 0 exit /b !errorlevel!

for %%f in (test\*.c) do (
    echo ---------------------- %%~nf start ----------------------
    set "tmp_asm=build\%%~nf.asm"
    set "tmp_exe=build\%%~nf.exe"

    %cc% -S -o !tmp_asm! %%f -Itest
    if !errorlevel! neq 0 exit /b !errorlevel!

    clang -o !tmp_exe! !tmp_asm! -xc test/common
    if !errorlevel! neq 0 exit /b !errorlevel!

    !tmp_exe!
    if !errorlevel! neq 0 exit /b !errorlevel!
)

exit /b 0

:clean
del /s /f /q *.exe *.pdb *.s *.o *.obj >nul 2>nul
del /s /f /q tmp* >nul 2>nul
exit /b 0
