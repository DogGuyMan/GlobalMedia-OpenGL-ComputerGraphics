add_library(myapp_spawns STATIC
    OneShotSweeper.cpp
    AudioInstance.cpp
    VfxInstance.cpp
    CombatSequences.cpp
    AmbientSequences.cpp
)
add_library(MyApp::Spawns ALIAS myapp_spawns)

target_include_directories(myapp_spawns
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# SJH::scene/playable — Actor/Component/IPlayable 헤더 노출.
# MyApp::Entity/Audio/VFX/Tween — 후속 Task 의 빌더가 사용 (지금은 OneShotSweeper 만이라 scene 만 필수지만 미리 선언).
target_link_libraries(myapp_spawns
    PUBLIC
        SJH::scene
        SJH::playable
        MyApp::Entity
        MyApp::Audio
        MyApp::VFX
        MyApp::Tween
    PRIVATE
        game_deps
)
target_compile_features(myapp_spawns PUBLIC cxx_std_17)
