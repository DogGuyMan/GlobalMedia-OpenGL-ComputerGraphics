add_library(sjhopengl_program STATIC
    program.cpp
    program_uniforms.cpp
)
add_library(SJH::program ALIAS sjhopengl_program)

target_include_directories(sjhopengl_program
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            ${CMAKE_SOURCE_DIR}/include   # vmath.h
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sjhopengl_program
    PUBLIC  SJH::common
            SJH::shader
            project_deps
    PRIVATE SJH::diagnostics
)

target_compile_features(sjhopengl_program PRIVATE cxx_std_17)
