add_library(sjhopengl_scene STATIC
    actor.cpp
    scene.cpp
    scene_context.cpp      # SP-SceneContext+ProgramRegistry — Cocos2D-x `Scene::_cameras/_lights` 정통 Aggregate
    camera.cpp
    compound_actor.cpp
    light.cpp              # 2026-06-11 E1 — DirLight/PointLight/SpotLight 컴포넌트 (object 에서 이주)
)
# 2026-06-24 — render-결합 팩토리(actor_factory/model_spawner)는 apps client(Bootstrap)로 이주.
#   엔진에 두면 scene->render 역의존이 생기므로 클라이언트 거주로 단방향 유지.
add_library(SJH::scene ALIAS sjhopengl_scene)

target_include_directories(sjhopengl_scene
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# 2026-06-11 E2 — render-결합 팩토리(Skybox/ScreenCamera) + ModelSpawner 를 render 로 이주.
# compound_actor.cpp 의 MeshRenderer 사용이 사라져 SJH::render 역의존 제거 (scene -> render 절단).
target_link_libraries(sjhopengl_scene
    PUBLIC  SJH::common SJH::object project_deps
    PRIVATE spdlog        # scene_context.cpp 의 warn/limit 송신
)

target_compile_features(sjhopengl_scene PUBLIC cxx_std_17)
