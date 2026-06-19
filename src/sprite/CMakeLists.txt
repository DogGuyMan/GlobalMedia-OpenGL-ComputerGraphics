# SJH::sprite — 등간격 N×M atlas + SpriteRenderer (게임 무관 sprite 데이터)
# spec docs/superpowers/specs/2026-05-24-topdown-shooter-design.md §1.4
add_library(sjh_sprite STATIC
    uniform_atlas.cpp
    sprite_component.cpp
    sprite_sequence_playable.cpp
)
add_library(SJH::sprite ALIAS sjh_sprite)

target_include_directories(sjh_sprite PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# 의존 — PUBLIC: 헤더 노출
target_link_libraries(sjh_sprite PUBLIC
    SJH::scene # SJH::Scene::Component 베이스 (SpriteRenderer / SpriteSequencePlayable)
    SJH::render # SJH::Scene::MeshRenderer 베이스 (SpriteRenderer 상속)
    SJH::material # SJH::Material / Pass / Uniforms (sprite_component.cpp)
    SJH::object # SJH::Mesh (uniform_atlas / sprite_sequence)
    SJH::texture # 2026-06-11 E6 — uniform_atlas.h/.cpp: SJH::Texture / SJH::Image (rr->texture 하위추출 + D8 로 sprite->rr 절단)
    SJH::playable # PlayableBase 베이스 — sprite_sequence_playable.h 가 PUBLIC include
    project_deps # gl3w, vmath, OpenGL, 플랫폼 프레임워크
)

# PRIVATE: cpp 내부 전용
target_link_libraries(sjh_sprite PRIVATE
    spdlog # uniform_atlas.cpp / sprite_component.cpp 의 spdlog
)
