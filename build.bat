@echo off
setlocal
cd /D "%~dp0"

if /I "%~1"=="test" goto test
if /I "%~1"=="clean" goto clean

clang -std=c99 -O0 -g -o mirage.exe main.c tokenize.c parse.c codegen.c type.c string.c
exit /b 0

:test
clang -std=c99 -O2 -Wall -o mirage.exe main.c tokenize.c parse.c codegen.c type.c string.c
clang -std=c99 -O2 test.c -o test.exe && test.exe
exit /b 0

:clean
if exist *.exe del /f /q *.exe
if exist *.pdb del /f /q *.pdb
if exist *.s del /f /q *.s
if exist *.o del /f /q *.o
if exist tmp* del /f /q tmp*
exit /b 0
