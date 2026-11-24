@echo off
cd /d "%~dp0"
if not exist "flash_upload.exe" goto :nogui
start "" "%~dp0flash_upload.exe" %*
goto :eof
:nogui
echo flash_upload.exe not found
pause
exit /b 1
