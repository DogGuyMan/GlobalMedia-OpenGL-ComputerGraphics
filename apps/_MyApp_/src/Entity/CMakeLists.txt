add_library(myapp_entity STATIC
    BaseEntity.cpp
    Components/WeaponComponents.cpp
    Player/PlayerActor.cpp
    Player/PlayerEntity.cpp
    Player/PlayerHand.cpp
    Enemy/SimplePursueAI.cpp
)
add_library(MyApp::Entity ALIAS myapp_entity)

# PUBLIC: src/ 디렉토리 consumer 가 "Entity/BaseEntity.h" / "Entity/Components/LifeComponents.h"
# PRIVATE: 자기 디렉토리 — 같은 폴더 헤더를 prefix 없이 include (예: BaseEntity.cpp 의 "BaseEntity.h").
target_include_directories(myapp_entity
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# SJH::scene    — Component/Actor 베이스 (헤더 노출)
# MyApp::Algebraic — LifeComponents.h ->Stat
# SJH::playable  — (PB분해 Task7 로 PlayerBehavior 철거 후 현재 Entity 직접 사용 0 — umbrella 전파 consumer 보호용 보수적 유지)
# SJH::sprite  (PRIVATE) — PlayerHand/PlayerActor 가 SpriteRenderer/SpriteSequencePlayable 사용
# game_deps    (PRIVATE) — SimplePursueAI / WeaponComponents / 팩토리들이 box2d 헤더 사용
target_link_libraries(myapp_entity
    PUBLIC
        SJH::scene
        MyApp::Algebraic
        SJH::playable
    PRIVATE
        SJH::sprite
        game_deps
        MyApp::Playable  # BaseEntity.cpp 전용(.cpp only) — PlayableDirector 완전형(Play/GetComponent). 헤더는 fwd-decl 라 PRIVATE (Stage/HUD 로 불필요 전파 방지)
)

target_compile_features(myapp_entity PUBLIC cxx_std_17)
