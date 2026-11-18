@echo off
setlocal
cd /d "%~dp0"
g++ -std=c++17 -Os -municode -mwindows -static -s -ffunction-sections -fdata-sections ^
    -Wl,--gc-sections -Wl,--stack,65536 -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -Wall ^
    src\main.cpp -o TrimFlow.exe -lgdi32 -lshell32 -luser32
if errorlevel 1 exit /b 1
echo Built TrimFlow.exe
