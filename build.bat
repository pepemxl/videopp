@echo off
rem Build helper for Windows.
rem   set BUILD_DIR=build & set BUILD_TYPE=Release & build.bat [extra cmake args...]
setlocal enabledelayedexpansion

if "%BUILD_DIR%"=="" set BUILD_DIR=build
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

cmake -S . -B "%BUILD_DIR%" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% %*
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --config %BUILD_TYPE%
if errorlevel 1 exit /b %errorlevel%

echo.
echo Built in %BUILD_DIR%\bin
endlocal
