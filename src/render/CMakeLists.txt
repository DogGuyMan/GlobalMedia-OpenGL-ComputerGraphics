add_library(sjhopengl_render STATIC
    device_context.cpp
    render_target.cpp           # vtable home TU — virtual class 의 ODR 보장 (header-only 만으로 부족)
    render_stage.cpp            # vtable home TU — IRenderStage virtual class (SP5)
    mesh_pass_processor.cpp
    pipeline_state_setter.cpp   # SP-PipelineSetter — GL state machine 분리 (Applier 패턴)
    property_block_setter.cpp   # SP-PropertyBlockSetter — 옛 MaterialApplier rename + 시그니처 정밀화
    scene_renderer.cpp
    light_uniform_dispatcher.cpp  # Phase 2 — SendLightUniforms SceneRenderer 에서 분리
    screen_quad_stage.cpp         # SP-UniversalRenderTarget Phase A — N FBO ->backbuffer 합성
    camera_stage.cpp              # SP-RenderStage 완성 — 단일 Camera 를 IRenderStage 로 wrap
)
add_library(SJH::render ALIAS sjhopengl_render)

target_include_directories(sjhopengl_render
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# property_block_setter / pipeline_state_setter 는 GL 호출이 본업이므로 render 모듈 소속.
# 공개 헤더(mesh_pass_processor.h / property_block_setter.h) 는 Material/Texture forward decl 만 ->
# SJH::material + SJH::resource_registry 는 PRIVATE.
target_link_libraries(sjhopengl_render
    PUBLIC  SJH::common SJH::program project_deps
    PRIVATE SJH::diagnostics SJH::material SJH::object SJH::resource_registry SJH::scene
            SJH::buffer   # scene_renderer.cpp 가 framebuffer.h (RenderTarget 상속 인식) 사용 (SP4 T3)
)

target_compile_features(sjhopengl_render PUBLIC cxx_std_17)
