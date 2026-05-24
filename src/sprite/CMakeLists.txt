# SJH::sprite — 등간격 N×M atlas + SpriteComponent (게임 무관 sprite 데이터)
# spec docs/superpowers/specs/2026-05-24-topdown-shooter-design.md §1.4
add_library(sjh_sprite STATIC
    uniform_atlas.cpp
)
add_library(SJH::sprite ALIAS sjh_sprite)

target_include_directories(sjh_sprite PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# 의존 — PUBLIC: 헤더 노출
target_link_libraries(sjh_sprite PUBLIC
    SJH::scene                # SJH::Scene::Component 베이스 (sprite_component.h)
    SJH::resource_registry    # SJH::Image / SJH::Texture (uniform_atlas.h/.cpp)
    project_deps              # gl3w, vmath, OpenGL, 플랫폼 프레임워크
)

# PRIVATE: cpp 내부 전용
target_link_libraries(sjh_sprite PRIVATE
    spdlog                    # uniform_atlas.cpp 의 spdlog::error/info
)
