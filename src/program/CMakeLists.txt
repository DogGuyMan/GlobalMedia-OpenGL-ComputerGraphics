add_library(sjhopengl_program STATIC
    program.cpp
    program_uniforms.cpp
    uniform_cache.cpp         # SP6 T1 — Program 의 UniformCache 분리.
    uniform_buffer.cpp        # Slang Phase 2 T2 — std140 UBO RAII (사이클 회피: buffer 모듈 아닌 program 내부).
)
add_library(SJH::program ALIAS sjhopengl_program)

target_include_directories(sjhopengl_program
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            ${CMAKE_SOURCE_DIR}/include   #.h
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# SP6 T2 — program.cpp 가 Material::OnProgramReleased cascade (Observer 패턴).
# SJH::material 은 INTERFACE 라이브러리 (SP4 T5) — link 비용 없는 헤더 의존.
target_link_libraries(sjhopengl_program
    PUBLIC  SJH::common
            SJH::shader
            project_deps
    PRIVATE SJH::diagnostics
            SJH::material     # SP6 T2 — Material::OnProgramReleased 호출용 (cpp 한정)
)

target_compile_features(sjhopengl_program PUBLIC cxx_std_17)
