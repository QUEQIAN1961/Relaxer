@echo off
setlocal
for %%I in ("%~dp0..") do set "RELAXER_SOURCE_DIR=%%~fI"
if not defined RELAXER_BUILD_DIR set "RELAXER_BUILD_DIR=%RELAXER_SOURCE_DIR%\build-ga"
if not exist "%RELAXER_BUILD_DIR%\Relaxer.exe" (
  echo Build Relaxer first. See docs\releases\v0.2.6-GA\BUILD-AND-SOURCE.md.
  echo Set RELAXER_BUILD_DIR to the matching CMake build directory.
  exit /b 2
)
if defined RELAXER_QT_PREFIX (
  set "PATH=%RELAXER_QT_PREFIX%\bin;%PATH%"
  set "QML_IMPORT_PATH=%RELAXER_QT_PREFIX%\share\qt6\qml"
)
cd /d "%RELAXER_SOURCE_DIR%"
start "" "%RELAXER_BUILD_DIR%\Relaxer.exe" --ui qml %*
