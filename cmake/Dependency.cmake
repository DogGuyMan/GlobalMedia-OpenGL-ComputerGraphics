# ______ 프로젝트 의존성 관리 ______
# lib/{macos,windows}/ 는 shell/BuildExternLibs.{sh,bat} 로 사전 빌드 필요
# project_deps PUBLIC 타겟으로 집약하여 앱 타겟에 PRIVATE 링크

find_package(OpenGL REQUIRED)

# ====== 플랫폼별 라이브러리 경로 ======

if(WIN32)
    set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib/windows)
else()
    set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib/macos)
endif()

# set_target_properties 는 빌드되는 파일의 경로를 지정하는것이다.
# glfw3 (Release + Debug _d 접미사)
add_library(glfw3 STATIC IMPORTED)
if(WIN32)
    set_target_properties(glfw3 PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/glfw3.lib
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/glfw3_d.lib)
else()
    set_target_properties(glfw3 PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/libglfw3.a
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/libglfw3_d.a)
endif()

# sb7 (Release + Debug _d 접미사)
add_library(sb7 STATIC IMPORTED)
if(WIN32)
    set_target_properties(sb7 PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/sb7.lib
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/sb7_d.lib)
else()
    set_target_properties(sb7 PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/libsb7.a
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/libsb7_d.a)
endif()

# ====== project_deps INTERFACE 타겟 ======
add_library(project_deps INTERFACE)
target_link_libraries(project_deps INTERFACE sb7 glfw3 ${OPENGL_LIBRARIES})
target_include_directories(project_deps INTERFACE "${CMAKE_SOURCE_DIR}/include") 

# ====== 플랫폼별 의존성 ======
if(APPLE)
    target_link_libraries(project_deps INTERFACE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework CoreFoundation")
    target_compile_definitions(project_deps INTERFACE
        GL_SILENCE_DEPRECATION
        __glext_h_)
elseif(WIN32)
    target_link_libraries(project_deps INTERFACE opengl32 gdi32 winmm)
endif()

# ====== 게임/엔진 라이브러리 (extern 서브모듈 → lib/include 사전 빌드) ======
# Box2D v2.4.1 — C++ 정적 라이브러리
add_library(box2d STATIC IMPORTED)
if(WIN32)
    set_target_properties(box2d PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/box2d.lib
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/box2d_d.lib)
else()
    set_target_properties(box2d PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/libbox2d.a
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/libbox2d_d.a)
endif()

# Effekseer — 파티클 엔진 + OpenGL 렌더러
add_library(Effekseer STATIC IMPORTED)
add_library(EffekseerRendererGL STATIC IMPORTED)
if(WIN32)
    set_target_properties(Effekseer PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/Effekseer.lib
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/Effekseer_d.lib)
    set_target_properties(EffekseerRendererGL PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/EffekseerRendererGL.lib
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/EffekseerRendererGL_d.lib)
else()
    set_target_properties(Effekseer PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/libEffekseer.a
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/libEffekseer_d.a)
    set_target_properties(EffekseerRendererGL PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/libEffekseerRendererGL.a
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/libEffekseerRendererGL_d.a)
endif()
# EffekseerRendererGL 는 Effekseer 코어에 의존 — 링크 순서 보장
target_link_libraries(EffekseerRendererGL INTERFACE Effekseer)
# 우산 헤더(Effekseer.h / EffekseerRendererGL.h)가 flat <...> 인클루드를 쓰므로
# include/Effekseer 디렉토리 자체를 인클루드 경로에 추가.
set_target_properties(Effekseer PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/include/Effekseer)
set_target_properties(EffekseerRendererGL PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/include/Effekseer)

# assimp v5.4.3 — 3D 모델 임포트 라이브러리. 번들 zlib(zlibstatic) 정적 링크.
add_library(assimp STATIC IMPORTED)
add_library(zlibstatic STATIC IMPORTED)
if(WIN32)
    set_target_properties(assimp PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/assimp.lib
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/assimp_d.lib)
    set_target_properties(zlibstatic PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/zlibstatic.lib
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/zlibstatic_d.lib)
else()
    set_target_properties(assimp PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/libassimp.a
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/libassimp_d.a)
    set_target_properties(zlibstatic PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/libzlibstatic.a
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/libzlibstatic_d.a)
endif()
# assimp 는 번들 zlib 의 inflate/deflate 심볼에 의존 — 링크 순서 보장
target_link_libraries(assimp INTERFACE zlibstatic)

# spdlog v1.17.0 — 컴파일 정적 라이브러리 (헤더 온리 모드 아님)
add_library(spdlog STATIC IMPORTED)
if(WIN32)
    set_target_properties(spdlog PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/spdlog.lib
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/spdlog_d.lib)
else()
    set_target_properties(spdlog PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/libspdlog.a
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/libspdlog_d.a)
endif()
# 컴파일된 정적 라이브러리를 쓰므로 소비자는 SPDLOG_COMPILED_LIB 매크로가 필요하다.
# INTERFACE_INCLUDE_DIRECTORIES — spdlog 헤더는 include/ 에 있어 소비자에게 전파 필요.
set_target_properties(spdlog PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS SPDLOG_COMPILED_LIB
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/include")
# spdlog::spdlog ALIAS — 외부 코드(테스트 등)가 표준 네임스페이스 형식으로 링크 가능.
add_library(spdlog::spdlog ALIAS spdlog)

# 헤더 온리 — 헤더는 이미 include/ 에 체크인. INTERFACE 타겟은 game_deps 멤버 표식.
add_library(tweeny INTERFACE)
add_library(stb_extra INTERFACE)

# game_deps — 게임/엔진 챕터가 project_deps 와 함께 링크하는 집계 타겟.
# Box2D / Effekseer 는 각각 Task 4 / Task 5 에서 이 줄에 추가된다.
add_library(game_deps INTERFACE)
target_link_libraries(game_deps INTERFACE
    box2d
    EffekseerRendererGL
    assimp
    spdlog
    tweeny stb_extra)
# SYSTEM 인클루드 — 서드파티 헤더(Effekseer/Tweeny/Box2D 등)는 Debug 의
# -Wall -Werror 대상에서 제외한다. (예: <Effekseer/Effekseer.h> 의 -Wmacro-redefined,
#  -Woverloaded-virtual 가 -Werror 로 빌드를 깨지 않도록.)
target_include_directories(game_deps SYSTEM INTERFACE "${CMAKE_SOURCE_DIR}/include")

# ====== FMOD 공용 — macOS rpath 설정 ======
# FMOD Core / Studio dylib 는 install_name 이 @rpath/lib*.dylib 라 실행 파일 rpath 에
# @loader_path 가 들어가야 startup 시 POST_BUILD 로 옆에 복사된 dylib 를 찾는다.
# 적용 시점: add_subdirectory(apps/...) 보다 먼저여야 모든 챕터 타겟에 전파.
# 비-FMOD 챕터에는 무해 (참조되는 @rpath dylib 가 없으면 rpath 항목은 미사용).
if(APPLE)
    list(APPEND CMAKE_BUILD_RPATH   "@loader_path")
    list(APPEND CMAKE_INSTALL_RPATH "@loader_path")
endif()

# ====== FMOD Core API (독점 SDK, 수동 설치 — doc/FMOD_Setup.md 참조) ======
# 동적 라이브러리만 배포 → SHARED IMPORTED. fmodL 은 로깅 빌드 → Debug 매핑.
# include/fmod/fmod.h 존재 여부로 가드 — SDK 미설치자도 configure/빌드 통과(오디오 비활성).
#
# game_deps INTERFACE 자동 합류 — 모든 game 챕터가 fmod 를 링크하게 된다.
# SHARED 이므로 startup 시 .dll/.dylib 가 실행 파일 옆에 있어야 한다 →
# game_deps 를 쓰는 챕터 CMakeLists.txt 는 POST_BUILD 에서
# $<TARGET_FILE:fmod> 를 $<TARGET_FILE_DIR:${CHAPTER_NAME}> 로 copy_if_different.
if(EXISTS "${CMAKE_SOURCE_DIR}/include/fmod/fmod.h")
    add_library(fmod SHARED IMPORTED)
    if(WIN32)
        set_target_properties(fmod PROPERTIES
            IMPORTED_LOCATION         ${LIB_DIR}/fmod.dll
            IMPORTED_IMPLIB           ${LIB_DIR}/fmod_vc.lib
            IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/fmodL.dll
            IMPORTED_IMPLIB_DEBUG     ${LIB_DIR}/fmodL_vc.lib)
    else()
        set_target_properties(fmod PROPERTIES
            IMPORTED_LOCATION         ${LIB_DIR}/libfmod.dylib
            IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/libfmodL.dylib)
    endif()
    target_link_libraries(game_deps INTERFACE fmod)
else()
    message(STATUS "FMOD 미설치 — 오디오 비활성. doc/FMOD_Setup.md 참조")
endif()

# FMOD Studio API — .bank 파일 기반 이벤트/믹서 시스템 (Core 위의 고수준 layer).
# 독립적으로 등록. fmod_studio.h 존재 시에만 활성화. fmodstudio 는 fmod 코어 심볼을
# DT_NEEDED 로 참조 → INTERFACE 링크 의존성으로 순서 보장.
if(EXISTS "${CMAKE_SOURCE_DIR}/include/fmod/fmod_studio.h")
    add_library(fmodstudio SHARED IMPORTED)
    if(WIN32)
        set_target_properties(fmodstudio PROPERTIES
            IMPORTED_LOCATION         ${LIB_DIR}/fmodstudio.dll
            IMPORTED_IMPLIB           ${LIB_DIR}/fmodstudio_vc.lib
            IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/fmodstudioL.dll
            IMPORTED_IMPLIB_DEBUG     ${LIB_DIR}/fmodstudioL_vc.lib)
    else()
        set_target_properties(fmodstudio PROPERTIES
            IMPORTED_LOCATION         ${LIB_DIR}/libfmodstudio.dylib
            IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/libfmodstudioL.dylib)
    endif()
    # fmodstudio 는 fmod 의 심볼에 의존 — 링크 순서 보장 + 단독 링크해도 fmod 자동 동반
    if(TARGET fmod)
        target_link_libraries(fmodstudio INTERFACE fmod)
    endif()
    target_link_libraries(game_deps INTERFACE fmodstudio)
endif()
