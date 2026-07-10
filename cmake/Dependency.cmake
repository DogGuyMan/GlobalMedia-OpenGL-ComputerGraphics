# ______ 프로젝트 의존성 관리 ______
# lib/{macos,windows}/ 는 `python3 scripts/dev.py extern` 로 사전 빌드 필요
# project_deps PUBLIC 타겟으로 집약하여 앱 타겟에 PRIVATE 링크

# vcpkg toolchain 은 CMAKE_FIND_FRAMEWORK 를 LAST 로 설정한다(프레임워크보다 vcpkg 라이브러리 우선).
# 그 상태로 find_package(OpenGL) 하면 macOS 에서 Apple OpenGL.framework 대신 MacPorts/Homebrew
# (/opt/local, /usr/local) 의 Mesa libGL.dylib 를 잡아 GLFW(NSGL) 컨텍스트 생성이 깨진다
# ("Failed to open window"). macOS 는 시스템 OpenGL.framework 가 정답이므로 OpenGL 탐색 동안만
# 프레임워크 우선을 복원하고, 이후 다시 vcpkg 기본값으로 되돌린다.
if(APPLE)
    set(_sjh_saved_find_framework "${CMAKE_FIND_FRAMEWORK}")
    set(CMAKE_FIND_FRAMEWORK FIRST)
endif()
find_package(OpenGL REQUIRED)
if(APPLE)
    set(CMAKE_FIND_FRAMEWORK "${_sjh_saved_find_framework}")
endif()

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

# ====== 게임/엔진 라이브러리 ======
# 전이분(box2d/assimp/spdlog/tweeny)은 vcpkg manifest(루트 vcpkg.json) 가 정확 버전 핀으로 설치.
# 잔류분(Effekseer/stb/FMOD)은 lib/include 사전 빌드 유지.
#
# Box2D — vcpkg (manifest 2.4.1 핀, 코드 무변경). 타겟: box2d::box2d
find_package(box2d CONFIG REQUIRED)

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

# assimp — vcpkg (manifest 5.4.3 핀). 번들 zlib 도 vcpkg 가 의존성으로 자동 동반
# (별도 zlibstatic 타겟/링크 순서 처리 불필요). 타겟: assimp::assimp
find_package(assimp CONFIG REQUIRED)

# spdlog — vcpkg (manifest 1.17.0 핀, 컴파일 정적 라이브러리). vcpkg 타겟이
# SPDLOG_COMPILED_LIB 매크로를 INTERFACE 로 전파하므로 수동 정의 불필요. 타겟: spdlog::spdlog
find_package(spdlog CONFIG REQUIRED)

# tweeny — vcpkg (manifest 3.2.0; 사전 3.0.0 에서 업글, 헤더온리). vcpkg 가 INTERFACE
# IMPORTED 타겟 'tweeny' 를 직접 노출. 타겟: tweeny
find_package(tweeny CONFIG REQUIRED)

find_package(glm CONFIG REQUIRED)
# glm 은 vmath 대체 *전역* 수학 라이브러리 — common.h 등 코어 전반이 사용. project_deps(전역 base)에 합류
# (vmath 가 include/ 로 전역 노출되던 자리와 동형). 모든 모듈/앱이 project_deps 경유로 glm 획득.
target_link_libraries(project_deps INTERFACE glm::glm)

# stb — vcpkg (헤더온리, manifest dependency). vcpkg 는 IMPORTED 타겟 대신 Stb_INCLUDE_DIR
# 변수를 노출하므로, 동명 INTERFACE 타겟 stb_extra 에 그 include 경로를 실어 소비자에 전파한다.
# (stb_image.h / stb_rect_pack.h 등) 소비자: src/texture(image.cpp 의 STB_IMAGE_IMPLEMENTATION) + game_deps.
find_package(Stb REQUIRED)
add_library(stb_extra INTERFACE)
target_include_directories(stb_extra INTERFACE ${Stb_INCLUDE_DIR})

# nlohmann-json — vcpkg (헤더온리, manifest dependency). JSON 파서/직렬화.
# 타겟: nlohmann_json::nlohmann_json (find_package 명은 nlohmann_json, port 명은 nlohmann-json).
find_package(nlohmann_json CONFIG REQUIRED)

# imgui — vcpkg (override 1.53). 코어(imgui::imgui)만 제공 — GLFW 3.0.4 호환 마지막 태그라 핀.
# GLFW 백엔드(imgui_impl_glfw_gl3)는 vcpkg 1.53 port 에 없어 apps/_MyApp_/third_party/imgui 로 vendoring.
# game_deps 합류 (실 사용은 _MyApp_ UI). imguizmo 는 imgui 1.53 비호환이라 미사용(드롭).
find_package(imgui CONFIG REQUIRED)

# ====== 하위 호환 래퍼 (bare 타겟명 -> vcpkg 네임스페이스 타겟) ======
# 일부 모듈 CMakeLists (src/object, src/common, src/diagnostics, src/resource_registry,
# apps/_MyApp_/src/*) 가 bare 'assimp' / 'spdlog' 로 직접 링크한다. find_package 전이 후
# 그 이름은 타겟이 아니라 -l 링크 플래그로 오인되므로, vcpkg 네임스페이스 타겟을 forward 하는
# 동명 INTERFACE 래퍼를 제공해 모듈 CMakeLists 를 무수정으로 둔다.
# (ALIAS-to-IMPORTED 는 CMake 버전별 GLOBAL 제약이 있어 INTERFACE 래퍼가 더 견고.)
if(NOT TARGET assimp)
    add_library(assimp INTERFACE)
    target_link_libraries(assimp INTERFACE assimp::assimp)
endif()
if(NOT TARGET spdlog)
    add_library(spdlog INTERFACE)
    target_link_libraries(spdlog INTERFACE spdlog::spdlog)
endif()

# game_deps — 게임/엔진 챕터가 project_deps 와 함께 링크하는 집계 타겟.
# Box2D / Effekseer 는 각각 Task 4 / Task 5 에서 이 줄에 추가된다.
add_library(game_deps INTERFACE)
target_link_libraries(game_deps INTERFACE
    box2d::box2d
    EffekseerRendererGL
    assimp::assimp
    spdlog::spdlog
    imgui::imgui
    tweeny stb_extra
    nlohmann_json::nlohmann_json)
    # glm::glm 은 project_deps(전역 base)로 이주 — 코어 모듈도 사용하므로 game_deps 만으론 부족.
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
# 동적 라이브러리만 배포 -> SHARED IMPORTED. fmodL 은 로깅 빌드 -> Debug 매핑.
# include/fmod/fmod.h 존재 여부로 가드 — SDK 미설치자도 configure/빌드 통과(오디오 비활성).
#
# game_deps INTERFACE 자동 합류 — 모든 game 챕터가 fmod 를 링크하게 된다.
# SHARED 이므로 startup 시 .dll/.dylib 가 실행 파일 옆에 있어야 한다 ->
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
    # FMOD 존재 시에만 SJH_HAS_FMOD 정의 — game_deps 경유로 모든 consumer
    # (core resource_registry + client _MyApp_ Audio) 에 전파.
    # 미설치(CI 등)면 fmod 타겟 자체가 없어 매크로 미정의 → FMOD 의존 .cpp 는
    # #ifdef SJH_HAS_FMOD 로 no-op 스텁 컴파일 (헤더는 전방선언만 써 FMOD-free).
    target_compile_definitions(fmod INTERFACE SJH_HAS_FMOD=1)
else()
    message(STATUS "FMOD 미설치 — 오디오 비활성. doc/FMOD_Setup.md 참조")
endif()

# FMOD Studio API — .bank 파일 기반 이벤트/믹서 시스템 (Core 위의 고수준 layer).
# 독립적으로 등록. fmod_studio.h 존재 시에만 활성화. fmodstudio 는 fmod 코어 심볼을
# DT_NEEDED 로 참조 -> INTERFACE 링크 의존성으로 순서 보장.
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
