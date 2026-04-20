@echo off
setlocal
set BUILD_TYPE=%~1
set TARGET=%~2

if "%BUILD_TYPE%"=="" goto :usage
if "%TARGET%"=="" goto :usage

set SCRIPT_DIR=%~dp0

call "%SCRIPT_DIR%CMakePrepare.bat"
if errorlevel 1 exit /b %errorlevel%

call "%SCRIPT_DIR%CMakeConfigureAndGenerate.bat" %BUILD_TYPE%
if errorlevel 1 exit /b %errorlevel%

call "%SCRIPT_DIR%CMakeBuild.bat" %BUILD_TYPE% %TARGET%
if errorlevel 1 exit /b %errorlevel%

call "%SCRIPT_DIR%CMakeExecute.bat" %BUILD_TYPE% %TARGET%
if errorlevel 1 exit /b %errorlevel%

endlocal
exit /b 0

:usage
echo 사용법: %~nx0 [debug^|release] ^<타겟명^>
echo 예시:  %~nx0 release exercise6
exit /b 1
