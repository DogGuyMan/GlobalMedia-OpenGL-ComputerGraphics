@echo off
setlocal
set ROOT_DIR=%~dp0..
set BUILD_TYPE=%~1
set TARGET=%~2

if "%TARGET%"=="" (
    echo 실행할 타겟 이름을 두 번째 인자로 입력해야 합니다. 예: %~nx0 release exercise6
    exit /b 1
)

if /i "%BUILD_TYPE%"=="debug" (
    set CONFIG_DIR=Debug
) else if /i "%BUILD_TYPE%"=="release" (
    set CONFIG_DIR=Release
) else (
    echo 사용법: %~nx0 ^<debug^|release^> ^<타겟명^>
    exit /b 1
)

set EXEC_DIR=%ROOT_DIR%\build_msvc\apps\%TARGET%\%CONFIG_DIR%
set EXE_PATH=%EXEC_DIR%\%TARGET%.exe

if not exist "%EXE_PATH%" (
    echo 실행 파일을 찾을 수 없습니다: %EXE_PATH%
    exit /b 1
)

REM 리소스 상대경로 해결을 위해 실행 파일 디렉토리에서 실행
pushd "%EXEC_DIR%"
"%TARGET%.exe"
set EXIT_CODE=%errorlevel%
popd

endlocal & exit /b %EXIT_CODE%
