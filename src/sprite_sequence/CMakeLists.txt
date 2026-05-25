# SJH::sprite_sequence — SpriteFrameClip + SpriteSequencePlayable
# spec docs/superpowers/specs/2026-05-26-playable-component-interface-design.md §3.4
add_library(sjh_sprite_sequence STATIC
    sprite_sequence_playable.cpp
)
add_library(SJH::sprite_sequence ALIAS sjh_sprite_sequence)

target_include_directories(sjh_sprite_sequence PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# PUBLIC — 헤더 노출
target_link_libraries(sjh_sprite_sequence PUBLIC
    SJH::playable            # PlayableBase 베이스 (sprite_sequence_playable.h)
    SJH::sprite              # SpriteRenderer (sprite_sequence_playable.h)
)
