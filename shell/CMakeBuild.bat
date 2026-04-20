@echo off
setlocal
set BUILD_TYPE=%~1
set TARGET=%~2
if "%BUILD_TYPE%"=="" set BUILD_TYPE=debug

if /i "%BUILD_TYPE%"=="debug" (
    set BUILD_PRESET=msvc-2022
) else if /i "%BUILD_TYPE%"=="release" (
    set BUILD_PRESET=msvc-2022-release
) else (
    echo 사용법: %~nx0 [debug^|release] [타겟명]
    exit /b 1
)

if "%TARGET%"=="" (
    cmake --build --preset %BUILD_PRESET%
) else (
    cmake --build --preset %BUILD_PRESET% --target %TARGET%
)
if errorlevel 1 exit /b %errorlevel%

endlocal
exit /b 0
