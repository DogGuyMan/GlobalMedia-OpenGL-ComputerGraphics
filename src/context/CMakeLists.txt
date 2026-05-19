add_library(sjhopengl_context INTERFACE)
add_library(SJH::context ALIAS sjhopengl_context)

target_include_directories(sjhopengl_context
    INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(sjhopengl_context
    INTERFACE SJH::common
              SJH::shader
              SJH::buffer
              SJH::layout
              SJH::object
              SJH::material
              SJH::resource_registry
              SJH::program
              SJH::input
              project_deps
)
