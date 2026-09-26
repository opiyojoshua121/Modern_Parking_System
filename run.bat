@echo off
setlocal
cd /d "%~dp0"
call build.bat
if errorlevel 1 exit /b 1
echo.
echo Starting web server at http://localhost:8080
echo Keep the server window open while using the website.
start "MMU Parking System" cmd /k parking_system.exe 8080
timeout /t 2 /nobreak >nul
start "MMU Parking Browser" http://localhost:8080
endlocal
