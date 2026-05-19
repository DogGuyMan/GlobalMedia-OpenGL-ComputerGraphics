add_library(sjhopengl_shader STATIC shader.cpp)
add_library(SJH::shader ALIAS sjhopengl_shader)

target_include_directories(sjhopengl_shader
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sjhopengl_shader
    PUBLIC  SJH::common
            project_deps
    PRIVATE SJH::diagnostics
)

target_compile_features(sjhopengl_shader PUBLIC cxx_std_17)
