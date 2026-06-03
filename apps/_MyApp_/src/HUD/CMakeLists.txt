# 머리 위 분절형 체력바 — HealthBarDriver(Life->uFill) + AttachHealthBar 팩토리.
# 신규 STATIC lib. SJH::engine(Material/Mesh/Geometry/MeshRenderer/ResourceRegistry/Program/Actor) +
# MyApp::Entity(ILivable) 의존.
add_library(myapp_hud STATIC
    HealthBarDriver.cpp
    HealthBarFactory.cpp
)
add_library(MyApp::HUD ALIAS myapp_hud)

target_include_directories(myapp_hud
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(myapp_hud
    PUBLIC
        SJH::engine    # Actor/Component/Material/Mesh/Geometry/MeshRenderer/ResourceRegistry/Program/vmath
        MyApp::Entity  # TopdownShooter::Entity::ILivable
)
target_compile_features(myapp_hud PUBLIC cxx_std_17)
