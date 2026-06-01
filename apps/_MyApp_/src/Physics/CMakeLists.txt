add_library(myapp_physics STATIC
    PhysicsSystem.cpp
    ContactListener.cpp
)
add_library(MyApp::Physics ALIAS myapp_physics)

target_include_directories(myapp_physics
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(myapp_physics
    PUBLIC
        SJH::scene
        project_deps
        game_deps
    PRIVATE
        MyApp::Entity
)

target_compile_features(myapp_physics PUBLIC cxx_std_17)
