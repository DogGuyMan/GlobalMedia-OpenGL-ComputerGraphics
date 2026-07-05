add_library(sjhopengl_buffer STATIC
    buffer.cpp
    render_texture.cpp
    render_target.cpp           # 2026-06-11 E4 — RenderTarget/DefaultRenderTarget (render 에서 이주). vtable home TU.
)
add_library(SJH::buffer ALIAS sjhopengl_buffer)

target_include_directories(sjhopengl_buffer
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# 2026-06-11 E4 — RenderTarget 이 buffer 로 이주해 SJH::render 역의존 제거 (buffer -> render 절단).
# render_texture.h 는 이제 같은 모듈의 render_target.h 만 참조.
target_link_libraries(sjhopengl_buffer
    PUBLIC  SJH::common
            SJH::program
            SJH::texture              # 2026-06-11 E3 — render_texture.h : TexturePtr / Texture (rr 에서 texture 하위추출)
            project_deps
    PRIVATE SJH::diagnostics
)

target_compile_features(sjhopengl_buffer PUBLIC cxx_std_17)
