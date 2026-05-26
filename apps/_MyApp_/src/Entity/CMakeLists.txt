add_library(myapp_entity STATIC
    BaseEntity.cpp
    Player/PlayerBehavior.cpp
    Player/BulletSpawnPlayable.cpp
    Bullet/BulletContactHandler.cpp
    Enemy/SimplePursueAI.cpp
    Enemy/EnemyContactHandler.cpp
)
add_library(MyApp::Entity ALIAS myapp_entity)

# PUBLIC: src/ 디렉토리 consumer 가 "Entity/BaseEntity.h" / "Entity/Components/LifeComponents.h"
# PRIVATE: 자기 디렉토리 — BaseEntity.h 내부의 "Components/Components.Interfaces.h" 등.
target_include_directories(myapp_entity
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# SJH::scene    — Component/Actor 베이스 (헤더 노출)
# MyApp::Algebraic — LifeComponents.h → Stat
# SJH::playable  — PlayerBehavior.h / BulletSpawnPlayable.h 헤더 노출
# SJH::sprite  (PRIVATE) — PlayerBehavior.cpp 가 SpriteSequencePlayable 메서드 호출
# game_deps    (PRIVATE) — SimplePursueAI.cpp / PlayerBehavior.cpp 가 box2d 헤더 사용
target_link_libraries(myapp_entity
    PUBLIC
        SJH::scene
        MyApp::Algebraic
        SJH::playable
    PRIVATE
        SJH::sprite
        game_deps
)

target_compile_features(myapp_entity PUBLIC cxx_std_17)
