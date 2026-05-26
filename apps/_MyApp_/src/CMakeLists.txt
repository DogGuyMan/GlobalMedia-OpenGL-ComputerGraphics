add_subdirectory(Algebraic)
add_subdirectory(Entity)
add_subdirectory(InputHandler)
add_subdirectory(Physics)
add_subdirectory(Stage)
add_subdirectory(Audio)
add_subdirectory(VFX)
add_subdirectory(Tween)

# M5 CL3 — Director 싱글톤 (Audio + VFX + Physics 집계)
add_library(myapp_director STATIC
    Director.cpp
)
add_library(MyApp::Director ALIAS myapp_director)
target_include_directories(myapp_director
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(myapp_director
    PUBLIC
        MyApp::Audio
        MyApp::VFX
        MyApp::Physics
    PRIVATE
        spdlog
)
target_compile_features(myapp_director PUBLIC cxx_std_17)

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
    MyApp::Director  # M5 CL3 — Audio+VFX+Physics 집계 싱글톤
)
