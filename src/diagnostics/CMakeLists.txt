add_library(sjhopengl_diagnostics STATIC
    gl_log.cpp
    uniform_diagnostics.cpp
    gl_state_fields.cpp
    gl_state_log.cpp
    gl_validate.cpp
)
add_library(SJH::diagnostics ALIAS sjhopengl_diagnostics)

target_include_directories(sjhopengl_diagnostics
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# 공개 헤더에서 GLuint/GLenum 등 GL 타입 사용 -> project_deps PUBLIC 전파 (glad 인클루드 + OpenGL 링크 포함).
target_link_libraries(sjhopengl_diagnostics
    PUBLIC  project_deps
            spdlog
)

target_compile_features(sjhopengl_diagnostics PRIVATE cxx_std_17)
