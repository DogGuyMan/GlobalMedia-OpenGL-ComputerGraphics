# render_bootstrap -- 파이프라인 조립 함수 (SetupDefaultPipeline / BuildPostFXChain).
# render core 밖 최상위 모듈 -- render 가 rr 을 include 하던 사이클을 끊는다 (spec D-2).
# render_pipeline 은 rr/render/object/material/scene 를 *구상 의존* 하지만, 자신은
# 아무도 역의존하지 않는 최상위라 신규 사이클이 생기지 않는다.
add_library(sjhopengl_render_bootstrap STATIC
    render_pipeline.cpp
)
add_library(SJH::render_bootstrap ALIAS sjhopengl_render_bootstrap)

target_include_directories(sjhopengl_render_bootstrap
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# render_pipeline.h(공개) 는 buffer/framebuffer.h 만 노출 -> SJH::buffer PUBLIC.
# 나머지(render/rr/object/material/scene) 는 .cpp 전용 -> PRIVATE.
# spdlog 는 SJH::resource_registry 의 PUBLIC game_deps 로 전파되어 .cpp 컴파일 시 해소.
target_link_libraries(sjhopengl_render_bootstrap
    PUBLIC  SJH::buffer
    PRIVATE SJH::render SJH::resource_registry SJH::object SJH::material SJH::scene
)

target_compile_features(sjhopengl_render_bootstrap PUBLIC cxx_std_17)
