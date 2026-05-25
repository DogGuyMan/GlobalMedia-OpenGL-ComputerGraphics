add_subdirectory(buffer)
add_subdirectory(object)
add_subdirectory(common)
add_subdirectory(diagnostics)
add_subdirectory(layout)
add_subdirectory(program)
add_subdirectory(material)
add_subdirectory(scene)
add_subdirectory(sprite)    # ← 추가 (M1)
add_subdirectory(fsm)       # ← 추가 (M2)
add_subdirectory(shader)
add_subdirectory(render)
add_subdirectory(resource_registry)
add_subdirectory(input)
add_subdirectory(playable)
add_subdirectory(sprite_sequence)

add_library(sjhopengl_engine INTERFACE)
add_library(SJH::engine ALIAS sjhopengl_engine)

target_link_libraries(sjhopengl_engine INTERFACE
    SJH::common
    SJH::buffer
    SJH::diagnostics
    SJH::input
    SJH::layout
    SJH::material
    SJH::object
    SJH::program
    SJH::render
    SJH::resource_registry
    SJH::scene
    SJH::shader
    SJH::sprite          # M1 — 13 모듈 (sprite 합류)
    SJH::fsm             # M2 — 14 모듈 (fsm 합류)
)
