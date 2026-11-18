@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
g++ -std=c++17 -O2 -Wall -Wextra -Werror tests\heuristics_test.cpp -o build\heuristics_test.exe
if errorlevel 1 exit /b 1
build\heuristics_test.exe
if errorlevel 1 exit /b 1
echo All unit tests passed.
