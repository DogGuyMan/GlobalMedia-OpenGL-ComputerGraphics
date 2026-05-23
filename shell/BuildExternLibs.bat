@echo off
REM extern 라이브러리(glfw3, sb7, Box2D, Effekseer, assimp, spdlog) 빌드 스크립트 (Windows)
REM 프로젝트 루트에서 실행: shell\BuildExternLibs.bat
REM 출력: build_extern\output\windows\ (glfw3.lib, glfw3_d.lib, sb7_d.lib, sb7_d.pdb,
REM                                      box2d.lib, box2d_d.lib,
REM                                      Effekseer.lib, Effekseer_d.lib,
REM                                      EffekseerRendererGL.lib, EffekseerRendererGL_d.lib,
REM                                      assimp.lib, assimp_d.lib,
REM                                      zlibstatic.lib, zlibstatic_d.lib,
REM                                      spdlog.lib, spdlog_d.lib)
REM       build_extern\output\include\
REM 필요한 파일을 직접 lib\windows\, include\ 로 복사하여 사용

setlocal enabledelayedexpansion

REM ====== MSVC CRT 옵션 (메인 프로젝트와 일치: /MT (Release), /MTd (Debug)) ======
REM CMakePresets.json 의 CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug> 와 동일
REM CMP0091 정책: CMake 3.15+ 가 CMAKE_MSVC_RUNTIME_LIBRARY 변수를 인식하게 함
REM 따옴표 필수: BAT 가 $<...> 의 < > 를 redirection 으로 해석하지 못하게 막음
set MSVC_CRT_OPT=-DCMAKE_POLICY_DEFAULT_CMP0091=NEW "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>"

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
    %MSVC_CRT_OPT% ^
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
    %MSVC_CRT_OPT% ^
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

REM ====== Box2D 빌드 (Release + Debug) ======
echo [+] Box2D v2.4.1 빌드...
set BOX2D_DIR=%ROOT_DIR%\extern\box2d
set BOX2D_BUILD=%BUILD_DIR%\box2d

cmake -S "%BOX2D_DIR%" -B "%BOX2D_BUILD%" ^
    -G "Visual Studio 17 2022" ^
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
    %MSVC_CRT_OPT% ^
    -DBOX2D_BUILD_TESTBED=OFF ^
    -DBOX2D_BUILD_UNIT_TESTS=OFF ^
    -DBOX2D_BUILD_DOCS=OFF
if errorlevel 1 goto :error

cmake --build "%BOX2D_BUILD%" --config Release
if errorlevel 1 goto :error
for /R "%BOX2D_BUILD%" %%f in (box2d.lib) do (
    echo %%f | findstr /I "Release" >nul && copy /Y "%%f" "%LIB_DIR%\box2d.lib" >nul
)

cmake --build "%BOX2D_BUILD%" --config Debug
if errorlevel 1 goto :error
for /R "%BOX2D_BUILD%" %%f in (box2d.lib) do (
    echo %%f | findstr /I "Debug" >nul && copy /Y "%%f" "%LIB_DIR%\box2d_d.lib" >nul
)

REM Box2D 공개 헤더 복사
if not exist "%INCLUDE_DIR%\box2d" mkdir "%INCLUDE_DIR%\box2d"
xcopy /E /Y /I "%BOX2D_DIR%\include\box2d\*" "%INCLUDE_DIR%\box2d\" >nul

echo   -^> Box2D 빌드 완료 (Release + Debug)

REM ====== Effekseer 빌드 (Release + Debug) ======
echo [+] Effekseer 빌드...
set EFK_DIR=%ROOT_DIR%\extern\Effekseer
set EFK_BUILD=%BUILD_DIR%\effekseer

cmake -S "%EFK_DIR%" -B "%EFK_BUILD%" ^
    -G "Visual Studio 17 2022" ^
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
    %MSVC_CRT_OPT% ^
    -DBUILD_GL=ON ^
    -DBUILD_VULKAN=OFF ^
    -DBUILD_METAL=OFF ^
    -DBUILD_DX9=OFF ^
    -DBUILD_DX11=OFF ^
    -DBUILD_DX12=OFF ^
    -DBUILD_VIEWER=OFF ^
    -DBUILD_EDITOR=OFF ^
    -DBUILD_EXAMPLES=OFF ^
    -DBUILD_TEST=OFF ^
    -DBUILD_UNITYPLUGIN=OFF ^
    -DUSE_LIBPNG_LOADER=OFF ^
    -DNETWORK_ENABLED=OFF
if errorlevel 1 goto :error

cmake --build "%EFK_BUILD%" --config Release --target Effekseer EffekseerRendererGL
if errorlevel 1 goto :error
for /R "%EFK_BUILD%" %%f in (Effekseer.lib) do (
    echo %%f | findstr /I "Release" >nul && copy /Y "%%f" "%LIB_DIR%\Effekseer.lib" >nul
)
for /R "%EFK_BUILD%" %%f in (EffekseerRendererGL.lib) do (
    echo %%f | findstr /I "Release" >nul && copy /Y "%%f" "%LIB_DIR%\EffekseerRendererGL.lib" >nul
)

cmake --build "%EFK_BUILD%" --config Debug --target Effekseer EffekseerRendererGL
if errorlevel 1 goto :error
for /R "%EFK_BUILD%" %%f in (Effekseer.lib) do (
    echo %%f | findstr /I "Debug" >nul && copy /Y "%%f" "%LIB_DIR%\Effekseer_d.lib" >nul
)
for /R "%EFK_BUILD%" %%f in (EffekseerRendererGL.lib) do (
    echo %%f | findstr /I "Debug" >nul && copy /Y "%%f" "%LIB_DIR%\EffekseerRendererGL_d.lib" >nul
)

REM Effekseer 공개 헤더만 복사 (.h — .cpp/.fx/.py/CMakeLists.txt 제외)
if not exist "%INCLUDE_DIR%\Effekseer" mkdir "%INCLUDE_DIR%\Effekseer"
for /R "%EFK_DIR%\Dev\Cpp\Effekseer" %%f in (*.h) do (
    xcopy /Y "%%f" "%INCLUDE_DIR%\Effekseer\" >nul
)
for /R "%EFK_DIR%\Dev\Cpp\EffekseerRendererGL" %%f in (*.h) do (
    xcopy /Y "%%f" "%INCLUDE_DIR%\Effekseer\" >nul
)

echo   -^> Effekseer 빌드 완료 (Release + Debug)

REM ====== assimp 빌드 (Release + Debug) ======
echo [+] assimp v5.4.3 빌드...
set ASSIMP_DIR=%ROOT_DIR%\extern\assimp
set ASSIMP_BUILD=%BUILD_DIR%\assimp

cmake -S "%ASSIMP_DIR%" -B "%ASSIMP_BUILD%" ^
    -G "Visual Studio 17 2022" ^
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
    %MSVC_CRT_OPT% ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DASSIMP_BUILD_TESTS=OFF ^
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF ^
    -DASSIMP_BUILD_SAMPLES=OFF ^
    -DASSIMP_INSTALL=OFF ^
    -DASSIMP_WARNINGS_AS_ERRORS=OFF ^
    -DASSIMP_BUILD_ZLIB=ON
if errorlevel 1 goto :error

cmake --build "%ASSIMP_BUILD%" --config Release
if errorlevel 1 goto :error
REM assimp Release: 경로에 "Release" 가 포함된 assimp*.lib 를 복사
for /R "%ASSIMP_BUILD%" %%f in (assimp*.lib) do (
    echo %%f | findstr /I "Release" >nul && copy /Y "%%f" "%LIB_DIR%\assimp.lib" >nul
)
for /R "%ASSIMP_BUILD%" %%f in (zlibstatic.lib) do (
    echo %%f | findstr /I "Release" >nul && copy /Y "%%f" "%LIB_DIR%\zlibstatic.lib" >nul
)

cmake --build "%ASSIMP_BUILD%" --config Debug
if errorlevel 1 goto :error
REM assimp Debug: 경로에 "Debug" 가 포함된 assimp*.lib 를 복사
for /R "%ASSIMP_BUILD%" %%f in (assimp*.lib) do (
    echo %%f | findstr /I "Debug" >nul && copy /Y "%%f" "%LIB_DIR%\assimp_d.lib" >nul
)
REM zlibstatic Debug: assimp 가 ASSIMP_BUILD_ZLIB=ON 시 zlibstaticd.lib 생성
for /R "%ASSIMP_BUILD%" %%f in (zlibstaticd.lib) do (
    echo %%f | findstr /I "Debug" >nul && copy /Y "%%f" "%LIB_DIR%\zlibstatic_d.lib" >nul
)

REM assimp 소스 헤더 복사
if not exist "%INCLUDE_DIR%\assimp" mkdir "%INCLUDE_DIR%\assimp"
for /R "%ASSIMP_DIR%\include\assimp" %%f in (*.h *.hpp *.inl) do (
    xcopy /Y "%%f" "%INCLUDE_DIR%\assimp\" >nul
)
REM assimp 빌드 생성 헤더 복사 (config.h, revision.h)
for /R "%ASSIMP_BUILD%\include\assimp" %%f in (*.h) do (
    xcopy /Y "%%f" "%INCLUDE_DIR%\assimp\" >nul
)

echo   -^> assimp 빌드 완료 (Release + Debug)

REM ====== spdlog 빌드 (Release + Debug) ======
echo [+] spdlog v1.17.0 빌드...
set SPDLOG_DIR=%ROOT_DIR%\extern\spdlog
set SPDLOG_BUILD=%BUILD_DIR%\spdlog

cmake -S "%SPDLOG_DIR%" -B "%SPDLOG_BUILD%" ^
    -G "Visual Studio 17 2022" ^
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
    %MSVC_CRT_OPT% ^
    -DSPDLOG_BUILD_EXAMPLE=OFF ^
    -DSPDLOG_BUILD_TESTS=OFF ^
    -DSPDLOG_BUILD_BENCH=OFF
if errorlevel 1 goto :error

cmake --build "%SPDLOG_BUILD%" --config Release
if errorlevel 1 goto :error
for /R "%SPDLOG_BUILD%" %%f in (spdlog.lib) do (
    echo %%f | findstr /I "Release" >nul && copy /Y "%%f" "%LIB_DIR%\spdlog.lib" >nul
)

cmake --build "%SPDLOG_BUILD%" --config Debug
if errorlevel 1 goto :error
for /R "%SPDLOG_BUILD%" %%f in (spdlogd.lib) do (
    echo %%f | findstr /I "Debug" >nul && copy /Y "%%f" "%LIB_DIR%\spdlog_d.lib" >nul
)

REM spdlog 헤더 복사 (.h/.hpp/.inl, fmt/bundled/fmt.license.rst 제외)
if not exist "%INCLUDE_DIR%\spdlog" mkdir "%INCLUDE_DIR%\spdlog"
for /R "%SPDLOG_DIR%\include\spdlog" %%f in (*.h *.hpp *.inl) do (
    xcopy /Y "%%f" "%INCLUDE_DIR%\spdlog\" >nul
)

echo   -^> spdlog 빌드 완료 (Release + Debug)

REM ====== 자동 복사: build_extern\output\* -> lib\windows\, include\ ======
echo.
echo [+] lib\windows\ 및 include\ 로 자동 복사 중...
if not exist "%ROOT_DIR%\lib\windows" mkdir "%ROOT_DIR%\lib\windows"
xcopy /Y "%LIB_DIR%\*" "%ROOT_DIR%\lib\windows\" >nul
xcopy /E /Y "%INCLUDE_DIR%\*" "%ROOT_DIR%\include\" >nul
echo   -^> 복사 완료

REM ====== 완료 ======
echo.
echo =========================================
echo  빌드 완료!
echo  라이브러리 (원본):  %LIB_DIR%
echo  헤더 (원본):       %INCLUDE_DIR%
echo  -^> %ROOT_DIR%\lib\windows\ 와 %ROOT_DIR%\include\ 로 자동 복사됨
echo.
echo  다음 단계:
echo    git add lib\windows\ include\
echo    git commit -m "build: Windows MSVC 사전 빌드 lib 갱신"
echo    git push
echo =========================================
dir "%ROOT_DIR%\lib\windows"
goto :eof

:error
echo 빌드 실패!
exit /b 1
