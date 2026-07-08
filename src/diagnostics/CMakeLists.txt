add_library(sjhopengl_diagnostics STATIC
    gl_log.cpp
    uniform_diagnostics.cpp
    gl_state_fields.cpp
    gl_state_log.cpp
    gl_validate.cpp
    effekseer_diagnostics.cpp
    frame_capture.cpp   # 골든 readback (glReadPixels + PNG) - 캡처 facility 엔진 이관
    pass_capture.cpp    # PassIterator 변형별 골든 runner (cycle-exempt: render/buffer 상향 링크)
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
    # ★ cycle-exempt (CLAUDE.md 예외 모듈): pass_capture 의 orchestration 이 render/buffer 를 상향 의존.
    #   render->diagnostics(gl_log 등) 와 diagnostics->render 가 STATIC 순환 - CMake 가 link-line 반복으로 해소.
    #   진단/캡처는 어느 계층이든 관측하는 게 본질이라 diagnostics 만 무순환 규율 예외. 타 모듈은 유지.
    PRIVATE SJH::render
            SJH::buffer
            stb_extra
)

target_compile_features(sjhopengl_diagnostics PRIVATE cxx_std_17)
