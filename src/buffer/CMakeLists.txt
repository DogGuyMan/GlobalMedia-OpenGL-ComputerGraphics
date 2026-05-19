add_library(sjhopengl_buffer STATIC
    buffer.cpp
    framebuffer.cpp
)
add_library(SJH::buffer ALIAS sjhopengl_buffer)

target_include_directories(sjhopengl_buffer
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sjhopengl_buffer
    PUBLIC  SJH::common
            SJH::program
            project_deps
    PRIVATE SJH::diagnostics
)

target_compile_features(sjhopengl_buffer PRIVATE cxx_std_17)
