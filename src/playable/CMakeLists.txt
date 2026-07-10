# SJH::playable — IPlayable + PlayableBase + Composite (Sequence/Parallel)
# spec doc/superpowers/specs/2026-05-26-playable-component-interface-design.md
add_library(sjh_playable STATIC
    playable_base.cpp
    composite_playable.cpp
    interval_playable.cpp
)
add_library(SJH::playable ALIAS sjh_playable)

target_include_directories(sjh_playable PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# PUBLIC — 헤더 노출 (IPlayable 은 헤더 only, PlayableBase 가 Component 헤더 노출)
target_link_libraries(sjh_playable PUBLIC
    SJH::scene                # SJH::Scene::Component 베이스 (playable_base.h)
)
