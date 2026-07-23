@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File ".\scripts\publish-github.ps1"
if errorlevel 1 (
  echo.
  echo Upload failed. Review the error message above.
  pause
  exit /b 1
)
echo.
echo Upload completed successfully.
pause

