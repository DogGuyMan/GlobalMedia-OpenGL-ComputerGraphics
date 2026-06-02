add_subdirectory(Algebraic)
add_subdirectory(Entity)
add_subdirectory(InputHandler)
add_subdirectory(Physics)
add_subdirectory(Stage)
add_subdirectory(Audio)
add_subdirectory(VFX)
add_subdirectory(Tween)
add_subdirectory(Text)        # <- 추가 (MyApp::Text — Manager 가 의존, Manager 앞)
add_subdirectory(Spawns)
add_subdirectory(Playable)   # 연출 foundation — PlayableDirector + PostFXRegistry (Spawns/Entity 뒤)
add_subdirectory(HUD)        # <- 추가 (MyApp::HUD — 체력바, Bootstrap 앞)

# M5 CL3 — Director 싱글톤 (Audio + VFX + Physics 집계)
add_library(myapp_manager STATIC
    Manager.cpp
)
add_library(MyApp::Manager ALIAS myapp_manager)
target_include_directories(myapp_manager
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(myapp_manager
    PUBLIC
        MyApp::Audio
        MyApp::VFX
        MyApp::Physics
        MyApp::Text       # <- 추가 (WorldTextSystem)
    PRIVATE
        spdlog
)
target_compile_features(myapp_manager PUBLIC cxx_std_17)

# Warmup 보일러플레이트 추출 빌더 — MyApp::Manager 정의 뒤 (Bootstrap 이 Manager 의존).
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
    MyApp::Manager  # M5 CL3 — Audio+VFX+Physics 집계 싱글톤
    MyApp::Spawns
    MyApp::Playable # 연출 foundation — PlayableDirector + PostFXRegistry
    MyApp::HUD      # <- 추가 (체력바)
    MyApp::Bootstrap # Warmup 보일러플레이트 추출 빌더 (WorldScene/Player/Audio)
)
