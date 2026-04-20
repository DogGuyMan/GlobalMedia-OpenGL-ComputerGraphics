@echo off
setlocal
set ROOT_DIR=%~dp0..
set BUILD_TYPE=%~1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=debug

REM MSVC 프리셋은 Debug/Release 가 동일한 configurePreset 을 공유 (build 시 --config 로 분기)
cmake --preset msvc-2022 -S "%ROOT_DIR%"
if errorlevel 1 exit /b %errorlevel%

endlocal
exit /b 0
