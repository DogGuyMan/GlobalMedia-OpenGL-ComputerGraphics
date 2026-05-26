add_library(sjhopengl_scene STATIC
    actor.cpp
    scene.cpp
    scene_context.cpp      # SP-SceneContext+ProgramRegistry — Cocos2D-x `Scene::_cameras/_lights` 정통 Aggregate
    model_spawner.cpp
    camera.cpp
    compound_actor.cpp
)
add_library(SJH::scene ALIAS sjhopengl_scene)

target_include_directories(sjhopengl_scene
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sjhopengl_scene
    PUBLIC  SJH::common SJH::object project_deps
    PRIVATE spdlog        # scene_context.cpp 의 warn/limit 송신
)

target_compile_features(sjhopengl_scene PUBLIC cxx_std_17)
