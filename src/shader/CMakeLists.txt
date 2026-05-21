add_library(sjhopengl_shader STATIC shader.cpp)
add_library(SJH::shader ALIAS sjhopengl_shader)

target_include_directories(sjhopengl_shader
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)
# 주의: PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} 추가 금지 — sb7code 의 include/shader.h 와
#       파일명 충돌. PRIVATE 가 있으면 #include <shader.h> 가 sb7 대신 SJH 자체 헤더로 해상.
#       cpp 는 "shader/shader.h" 완전 경로 사용으로 충분.

target_link_libraries(sjhopengl_shader
    PUBLIC  SJH::common
            project_deps
    PRIVATE SJH::diagnostics
)

target_compile_features(sjhopengl_shader PUBLIC cxx_std_17)
