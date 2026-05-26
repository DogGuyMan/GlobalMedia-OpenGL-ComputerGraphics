add_library(myapp_entity STATIC
    BaseEntity.cpp
)
add_library(MyApp::Entity ALIAS myapp_entity)

# PUBLIC: src/ 디렉토리  consumer 가 "Entity/BaseEntity.h" / "Entity/Components/LifeComponents.h"
# PRIVATE: 자기 디렉토리 — BaseEntity.h 내부의 "Components/Components.Interfaces.h" 등.
target_include_directories(myapp_entity
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# BaseEntity.h / LifeComponents.h: SJH::Scene::Component 상속 + scene/actor.h include
# -> SJH::scene PUBLIC 의존.
# LifeComponents.h / WeaponComponents.h: Stat 멤버 보유 (Algebraic/Stat.h)
# -> MyApp::Algebraic PUBLIC 의존.
target_link_libraries(myapp_entity
    PUBLIC  SJH::scene
            MyApp::Algebraic
)

target_compile_features(myapp_entity PUBLIC cxx_std_17)
