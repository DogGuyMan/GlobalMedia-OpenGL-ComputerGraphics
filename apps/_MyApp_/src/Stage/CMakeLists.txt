add_library(myapp_stage STATIC
    StageBuilder.cpp
    Components/PickupTriggerLogger.cpp
    WaveController.cpp
)
add_library(MyApp::Stage ALIAS myapp_stage)

# src/ 기준 상대 인클루드 ("Stage/StageBuilder.h", "Stage/WaveController.h" 등)
target_include_directories(myapp_stage
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(myapp_stage
    PUBLIC
        SJH::scene
        SJH::object
        SJH::material
        SJH::render
        SJH::resource_registry
        MyApp::Physics    # Stage/Factories/*.h 가 Physics/filter.h + Components.Interfaces.h include
        MyApp::Entity     # WaveController → enemy_factory (헤더 온리 팩토리 포함)
        project_deps
        game_deps
)

target_compile_features(myapp_stage PUBLIC cxx_std_17)
