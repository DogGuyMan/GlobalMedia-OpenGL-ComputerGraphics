@echo off
REM extern 라이브러리(glfw3, sb7) 빌드 스크립트 (Windows)
REM 프로젝트 루트에서 실행: shell\BuildExternLibs.bat
REM 출력: build_extern\output\windows\ (glfw3.lib, glfw3_d.lib, sb7_d.lib, sb7_d.pdb)
REM       build_extern\output\include\
REM 필요한 파일을 직접 lib\windows\, include\ 로 복사하여 사용

setlocal enabledelayedexpansion

set ROOT_DIR=%~dp0..
set SB7CODE_DIR=%ROOT_DIR%\extern\sb7code
set GLFW_DIR=%SB7CODE_DIR%\extern\glfw-3.0.4
set BUILD_DIR=%ROOT_DIR%\build_extern
set OUTPUT_DIR=%BUILD_DIR%\output
set LIB_DIR=%OUTPUT_DIR%\windows
set INCLUDE_DIR=%OUTPUT_DIR%\include

echo =========================================
echo  extern 라이브러리 빌드 (Windows)
echo  출력: build_extern\output\windows\
echo        build_extern\output\include\
echo =========================================

REM ====== 출력 디렉토리 준비 ======
if not exist "%LIB_DIR%" mkdir "%LIB_DIR%"
if not exist "%INCLUDE_DIR%" mkdir "%INCLUDE_DIR%"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM ====== 헤더 복사 ======
echo [1/3] 헤더 복사...
xcopy /E /Y /I "%SB7CODE_DIR%\include\*" "%INCLUDE_DIR%\" >nul
if not exist "%INCLUDE_DIR%\GLFW" mkdir "%INCLUDE_DIR%\GLFW"
xcopy /Y "%GLFW_DIR%\include\GLFW\*" "%INCLUDE_DIR%\GLFW\" >nul
echo   -^> %INCLUDE_DIR% 에 복사 완료

REM ====== glfw3 빌드 (Release + Debug) ======
echo [2/3] glfw3 빌드...
set GLFW_BUILD=%BUILD_DIR%\glfw

cmake -S "%GLFW_DIR%" -B "%GLFW_BUILD%" ^
    -G "Visual Studio 17 2022" ^
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
    -DGLFW_BUILD_EXAMPLES=OFF ^
    -DGLFW_BUILD_TESTS=OFF ^
    -DGLFW_BUILD_DOCS=OFF ^
    -DGLFW_INSTALL=OFF
if errorlevel 1 goto :error

cmake --build "%GLFW_BUILD%" --config Release
if errorlevel 1 goto :error
for /R "%GLFW_BUILD%" %%f in (glfw3.lib) do (
    echo %%f | findstr /I "Release" >nul && copy /Y "%%f" "%LIB_DIR%\glfw3.lib" >nul
)

cmake --build "%GLFW_BUILD%" --config Debug
if errorlevel 1 goto :error
for /R "%GLFW_BUILD%" %%f in (glfw3.lib) do (
    echo %%f | findstr /I "Debug" >nul && copy /Y "%%f" "%LIB_DIR%\glfw3_d.lib" >nul
)

echo   -^> glfw3 빌드 완료 (Release + Debug)

REM ====== sb7 빌드 (Debug only) ======
echo [3/3] sb7 빌드...
set SB7_BUILD=%BUILD_DIR%\sb7
if not exist "%SB7_BUILD%" mkdir "%SB7_BUILD%"

REM sb7 빌드용 임시 CMakeLists.txt 생성
(
echo cmake_minimum_required(VERSION 3.14^)
echo project(sb7_build LANGUAGES C CXX^)
echo set(SB7CODE_DIR "" CACHE PATH "sb7code"^)
echo set(GLFW_DIR "" CACHE PATH "GLFW"^)
echo file(GLOB SB7_SOURCES ${SB7CODE_DIR}/src/sb7/*.cpp ${SB7CODE_DIR}/src/sb7/*.c^)
echo add_library(sb7 STATIC ${SB7_SOURCES}^)
echo target_include_directories(sb7 PRIVATE ${SB7CODE_DIR}/include ${GLFW_DIR}/include^)
echo target_compile_options(sb7 PRIVATE /w^)
) > "%SB7_BUILD%\CMakeLists.txt"

cmake -S "%SB7_BUILD%" -B "%SB7_BUILD%\build" ^
    -G "Visual Studio 17 2022" ^
    -DSB7CODE_DIR="%SB7CODE_DIR%" ^
    -DGLFW_DIR="%GLFW_DIR%"
if errorlevel 1 goto :error

cmake --build "%SB7_BUILD%\build" --config Debug
if errorlevel 1 goto :error
for /R "%SB7_BUILD%\build" %%f in (sb7.lib) do (
    echo %%f | findstr /I "Debug" >nul && copy /Y "%%f" "%LIB_DIR%\sb7_d.lib" >nul
)
for /R "%SB7_BUILD%\build" %%f in (sb7.pdb) do (
    echo %%f | findstr /I "Debug" >nul && copy /Y "%%f" "%LIB_DIR%\sb7_d.pdb" >nul
)

echo   -^> sb7 빌드 완료 (Debug)

REM ====== 완료 ======
echo.
echo =========================================
echo  빌드 완료!
echo  라이브러리: %LIB_DIR%
echo  헤더:      %INCLUDE_DIR%
echo.
echo  lib\windows\, include\ 로 필요한 파일을 직접 복사하세요:
echo    xcopy /Y "%LIB_DIR%\*" "%ROOT_DIR%\lib\windows\"
echo    xcopy /E /Y "%INCLUDE_DIR%\*" "%ROOT_DIR%\include\"
echo =========================================
dir "%LIB_DIR%"
goto :eof

:error
echo 빌드 실패!
exit /b 1
