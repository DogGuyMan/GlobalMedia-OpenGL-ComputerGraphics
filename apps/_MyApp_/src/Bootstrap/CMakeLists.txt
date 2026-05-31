# main.cpp 의 Warmup 보일러플레이트 추출 — 관심사별 free-function 빌더 (B/C 그룹).
# WorldScene(camera+light+skybox) / Player / Audio 3종. UI 빌더는 ImGui 의존이라
# executable 직접 컴파일(UI_SRC) — 본 STATIC 에 포함 안 함.
add_library(myapp_bootstrap STATIC
    WorldSceneBuilder.cpp
    PlayerBuilder.cpp
    AudioWarmup.cpp
)
add_library(MyApp::Bootstrap ALIAS myapp_bootstrap)

target_include_directories(myapp_bootstrap
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(myapp_bootstrap
    PUBLIC
        SJH::engine          # scene/material/sprite/render/resource_registry/object/playable/input 헤더 노출
        MyApp::InputHandler  # ActorFolower / PlayerController::Action
        MyApp::Entity        # CreatePlayerActor (PlayerActor.h)
        MyApp::Audio         # AudioSystem& (헤더 노출)
        MyApp::Physics       # b2World (PlayerBuilder)
        MyApp::Manager       # Manager::Get()
    PRIVATE
        MyApp::VFX           # EffekseerPlayable (PlayerBuilder .cpp)
        MyApp::Tween         # TweenPlayable (PlayerBuilder .cpp)
        MyApp::Spawns        # BuildBGM (AudioWarmup .cpp)
        game_deps            # box2d / tweeny / fmod / effekseer (.cpp 전용)
        spdlog
)
target_compile_features(myapp_bootstrap PUBLIC cxx_std_17)
