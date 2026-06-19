add_library(sjhopengl_render STATIC
    device_context.cpp
    render_stage.cpp            # vtable home TU — IRenderStage virtual class (SP5)
    mesh_pass_processor.cpp
    pipeline_state_setter.cpp   # SP-PipelineSetter — GL state machine 분리 (Applier 패턴)
    property_block_setter.cpp   # SP-PropertyBlockSetter — 옛 MaterialApplier rename + 시그니처 정밀화
    scene_renderer.cpp
    light_uniform_dispatcher.cpp  # Phase 2 — SendLightUniforms SceneRenderer 에서 분리
    screen_quad_stage.cpp         # SP-UniversalRenderTarget Phase A — N FBO ->backbuffer 합성
    camera_stage.cpp              # SP-RenderStage 완성 — 단일 Camera 를 IRenderStage 로 wrap
    actor_factory.cpp             # 2026-06-11 E2 — Skybox/ScreenCamera 팩토리 (scene 에서 이주)
    model_spawner.cpp             # 2026-06-11 E2 — Model RenderUnit -> Actor (scene 에서 이주)
)
add_library(SJH::render ALIAS sjhopengl_render)

target_include_directories(sjhopengl_render
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# property_block_setter / pipeline_state_setter 는 GL 호출이 본업이므로 render 모듈 소속.
# 공개 헤더(mesh_pass_processor.h / property_block_setter.h) 는 Material/Texture forward decl 만 ->
# SJH::material 은 PRIVATE. SJH::resource_registry 는 D-1 push 로 의존 제거 (2026-06-19).
target_link_libraries(sjhopengl_render
    PUBLIC  SJH::common SJH::program project_deps
            SJH::buffer   # 2026-06-11 E4 — device_context.h(공개) 가 buffer/render_target.h(RenderTarget) 노출 -> PUBLIC 승격
            SJH::scene    # 2026-06-11 E2 — actor_factory.h/model_spawner.h(공개) 가 SJH::Scene::Actor 반환 -> PUBLIC 승격
    PRIVATE SJH::diagnostics SJH::material SJH::object
            SJH::texture  # 2026-06-11 E3 — property_block_setter.cpp 의 Texture (rr 에서 texture 하위추출)
)

target_compile_features(sjhopengl_render PUBLIC cxx_std_17)
