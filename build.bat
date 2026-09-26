@echo off
setlocal
cd /d "%~dp0"

g++ -std=c++17 -Wall -Wextra -g ^
    src\main.cpp src\ParkingSystem.cpp src\WebServer.cpp ^
    -I include -o parking_system.exe -lws2_32

if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo Build succeeded.
endlocal
