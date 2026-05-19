add_library(sjhopengl_resource_registry STATIC
    resource_registry.cpp
    texture.cpp
    image.cpp
)
add_library(SJH::resource_registry ALIAS sjhopengl_resource_registry)

target_include_directories(sjhopengl_resource_registry
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            ${CMAKE_SOURCE_DIR}/include   # vmath.h
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${Stb_INCLUDE_DIR}
)

target_link_libraries(sjhopengl_resource_registry
    PUBLIC  SJH::common
            SJH::object
            SJH::material      # resource_registry.h: CreateMaterial/FindMaterial 가 Material* 노출
            project_deps
            spdlog
    PRIVATE SJH::diagnostics
)

target_compile_features(sjhopengl_resource_registry PRIVATE cxx_std_17)
