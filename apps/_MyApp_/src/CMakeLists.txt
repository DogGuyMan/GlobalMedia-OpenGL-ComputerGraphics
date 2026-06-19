add_subdirectory(Algebraic)
add_subdirectory(Entity)
add_subdirectory(InputHandler)
add_subdirectory(Physics)
add_subdirectory(Stage)
add_subdirectory(Audio)
add_subdirectory(VFX)
add_subdirectory(Tween)
add_subdirectory(Text)        # <- 추가 (MyApp::Text — GameSystems 가 의존, GameSystems 앞)
add_subdirectory(Spawns)
add_subdirectory(Playable)   # 연출 foundation — PlayableDirector (Spawns/Entity 뒤)
add_subdirectory(HUD)        # <- 추가 (MyApp::HUD — 체력바, Bootstrap 앞)

# M5 CL3 — GameSystems 싱글톤 (Audio + VFX + Physics 집계)
add_library(myapp_gamesystems STATIC
    GameSystems.cpp
)
add_library(MyApp::GameSystems ALIAS myapp_gamesystems)
target_include_directories(myapp_gamesystems
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(myapp_gamesystems
    PUBLIC
        MyApp::Audio
        MyApp::VFX
        MyApp::Physics
        MyApp::Text       # <- 추가 (WorldTextSystem)
    PRIVATE
        spdlog
)
target_compile_features(myapp_gamesystems PUBLIC cxx_std_17)

# Warmup 보일러플레이트 추출 빌더 — MyApp::GameSystems 정의 뒤 (Bootstrap 이 GameSystems 의존).
add_subdirectory(Bootstrap)

# 우산 INTERFACE — 클라이언트 스크립트 전체 묶음
add_library(myapp_client INTERFACE)
add_library(MyApp::Client ALIAS myapp_client)

target_link_libraries(myapp_client INTERFACE
    MyApp::Algebraic
    MyApp::Entity
    MyApp::InputHandler
    MyApp::Physics
    MyApp::Stage
    MyApp::Audio
    MyApp::VFX
    MyApp::Tween
    MyApp::Text       # <- 추가 (WorldTextSystem)
    MyApp::GameSystems  # M5 CL3 — Audio+VFX+Physics 집계 싱글톤
    MyApp::Spawns
    MyApp::Playable # 연출 foundation — PlayableDirector
    MyApp::HUD      # <- 추가 (체력바)
    MyApp::Bootstrap # Warmup 보일러플레이트 추출 빌더 (WorldScene/Player/Audio)
)
