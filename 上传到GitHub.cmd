@echo off
setlocal
cd /d "%~dp0"
call ".\publish-to-github.cmd"
exit /b %errorlevel%
