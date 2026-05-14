add_library(sjhopengl_diagnostics STATIC
    gl_log.cpp
    uniform_diagnostics.cpp
    gl_state_fields.cpp
    gl_state_log.cpp
    engine_diagnostics.cpp
)
add_library(SJH::diagnostics ALIAS sjhopengl_diagnostics)

target_include_directories(sjhopengl_diagnostics
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# 공개 헤더(gl_log.h / gl_state_fields.h / uniform_diagnostics.h)에서 GLuint/GLenum 등
# GL 타입을 쓰므로 GL/gl3w.h 인클루드 경로 + OpenGL 링크가 필요 -> project_deps 를 PUBLIC 전파.
# (이 프로젝트는 vcpkg 미사용 — 과거 glad/spdlog/fmt 의존은 sb7 동봉 gl3w + 표준 라이브러리로
#  대체됨. spdlog::*/fmt::format/fmt::join 흉내는 내부 전용 헤더 log_util.h 참고.)
target_link_libraries(sjhopengl_diagnostics
    PUBLIC project_deps
)

target_compile_features(sjhopengl_diagnostics PUBLIC cxx_std_17)
