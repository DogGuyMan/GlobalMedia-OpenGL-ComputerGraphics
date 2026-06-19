add_subdirectory(texture)   # <- 추가 (2026-06-11 E3/E5/E6 — leaf GPU 자원, rr 에서 하위추출)
add_subdirectory(buffer)
add_subdirectory(object)
add_subdirectory(common)
add_subdirectory(diagnostics)
add_subdirectory(layout)
add_subdirectory(program)
add_subdirectory(material)
add_subdirectory(scene)
add_subdirectory(sprite)    # <- 추가 (M1)
add_subdirectory(fsm)       # <- 추가 (M2)
add_subdirectory(shader)
add_subdirectory(render)
add_subdirectory(render_bootstrap)  # <- 추가 (D-2 — 파이프라인 조립, render->rr 절단)
add_subdirectory(resource_registry)
add_subdirectory(input)
add_subdirectory(playable)
add_subdirectory(timer)     # <- 추가 (SJH::timer, spec 2026-06-01)
add_subdirectory(text)      # <- 추가 (SJH::text, spec 2026-06-02 World Text)

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
    SJH::sprite             # M1 — 13 모듈 (sprite 합류)
    SJH::fsm                # M2 — 14 모듈 (fsm 합류)
    SJH::playable           # M3.5 — 15 모듈 (playable 합류)
    SJH::timer              # 16 모듈 — 게임플레이 타이머 (spec 2026-06-01)
    SJH::text               # 17 모듈 — World Text (spec 2026-06-02)
    SJH::texture            # 18 모듈 — leaf GPU 자원 (2026-06-11 E3/E5/E6 사이클 해소)
    SJH::render_bootstrap   # 19 모듈 — 파이프라인 조립 (2026-06-19 render->rr 절단)
)
