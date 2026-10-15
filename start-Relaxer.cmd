@echo off
setlocal
cd /d "%~dp0"
if not exist "RelaxerStart.exe" (
  echo Please extract the entire Relaxer ZIP first.
  pause
  exit /b 2
)
start "" "%CD%\RelaxerStart.exe"
