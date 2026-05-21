add_library(sjhopengl_buffer STATIC
    buffer.cpp
    framebuffer.cpp
)
add_library(SJH::buffer ALIAS sjhopengl_buffer)

target_include_directories(sjhopengl_buffer
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# SP4 T1 — framebuffer.h 가 RenderTarget 인터페이스 + Texture 헤더 노출 → PUBLIC.
target_link_libraries(sjhopengl_buffer
    PUBLIC  SJH::common
            SJH::program
            SJH::render               # framebuffer.h : RenderTarget 인터페이스 (SP4 T1)
            SJH::resource_registry    # framebuffer.h : TexturePtr / Texture
            project_deps
    PRIVATE SJH::diagnostics
)

target_compile_features(sjhopengl_buffer PUBLIC cxx_std_17)
