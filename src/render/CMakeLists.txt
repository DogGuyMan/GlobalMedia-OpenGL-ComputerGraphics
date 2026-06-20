add_library(sjhopengl_render STATIC
    device_context.cpp           # D-RS-1 — GL state 단일 권위 흡수 (구 pipeline_state_setter 통합)
    mesh_pass_processor.cpp
    property_block_setter.cpp   # SP-PropertyBlockSetter — 옛 MaterialApplier rename + 시그니처 정밀화
    light_ubo_uploader.cpp        # Phase 2.5 D-LUD — LightUniformDispatcher O4 승격(UBO owner). loose 경로 공존
    # render_stage(IRenderStage 추상 + vtable 홈) / camera_stage / scene_renderer / screen_quad_stage 는
    # render_stage/ 서브폴더로 이동. add_subdirectory(render_stage) 가 target_sources 로 본 타겟에 기여 (아래).
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
            SJH::material # 2026-06-21 D-RS-1 — device_context.h(공개) 가 material/pass.h(Pass::PipelineState) 노출 -> PRIVATE->PUBLIC 승격 (사이클 없음, pass.h 는 GL 헤더만 의존)
    PRIVATE SJH::diagnostics SJH::object
            SJH::texture  # 2026-06-11 E3 — property_block_setter.cpp 의 Texture (rr 에서 texture 하위추출)
)

target_compile_features(sjhopengl_render PUBLIC cxx_std_17)

# render_stage 서브폴더 -- IRenderStage 구현체(SceneRenderer + ScreenQuadStage) 통합 TU 를
# 본 타겟에 기여 (별도 라이브러리 아님 -- 서브 CMakeLists 의 사이클 회피 주석 참조).
# add_library 정의 이후에 호출해야 target_sources 대상 타겟이 존재한다.
add_subdirectory(render_stage)
