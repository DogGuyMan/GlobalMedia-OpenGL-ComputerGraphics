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
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/sb7_d.lib)
else()
    set_target_properties(sb7 PROPERTIES
        IMPORTED_LOCATION         ${LIB_DIR}/libsb7.a
        IMPORTED_LOCATION_DEBUG   ${LIB_DIR}/libsb7_d.a)
endif()
set_target_properties(sb7 PROPERTIES
    PUBLIC_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/include")

# ====== project_deps PUBLIC 타겟 ======
add_library(project_deps PUBLIC)
target_link_libraries(project_deps PUBLIC sb7 glfw3 ${OPENGL_LIBRARIES})

# ====== 플랫폼별 의존성 ======
if(APPLE)
    target_link_libraries(project_deps PUBLIC
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework CoreFoundation")
    target_compile_definitions(project_deps PUBLIC
        GL_SILENCE_DEPRECATION
        __glext_h_)
elseif(WIN32)
    target_link_libraries(project_deps PUBLIC opengl32 gdi32 winmm)
endif()
