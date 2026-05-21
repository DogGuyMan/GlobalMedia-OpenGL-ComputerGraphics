add_library(sjhopengl_render STATIC
    render_target.cpp
    render_context.cpp
    render_queue.cpp
    material_applier.cpp
    render_system.cpp
)
add_library(SJH::render ALIAS sjhopengl_render)

target_include_directories(sjhopengl_render
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# material_applier 는 GL 호출이 본업이므로 render 모듈 소속 (SP3 T5 follow-up).
# 공개 헤더(render_queue.h / material_applier.h) 는 Material/Texture forward decl 만 →
# SJH::material + SJH::resource_registry 는 PRIVATE.
target_link_libraries(sjhopengl_render
    PUBLIC  SJH::common SJH::program project_deps
    PRIVATE SJH::diagnostics SJH::material SJH::object SJH::resource_registry SJH::scene
)

target_compile_features(sjhopengl_render PUBLIC cxx_std_17)
