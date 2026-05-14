add_library(sjhopengl_engine STATIC
    material.cpp
    geometry.cpp
    model_base.cpp
    shader_program.cpp
    scene_graph.cpp
    resource_management.cpp
)
add_library(SJH::engine ALIAS sjhopengl_engine)

target_include_directories(sjhopengl_engine
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# 엔진 헤더(예: model_base.h)에서 GL 타입과 vmath 사용 → PUBLIC 전파.
# diagnostics 는 진단 매크로/타입을 헤더에서 직접 노출하므로 PUBLIC 전파.
#
# 정책: ShaderProgram base + Default/Texture 구체 클래스 (셰이더 의존 specific 머티리얼과 동일 패턴)
#       는 엔진에 포함. Material 이 Program* 을 들고 Material→Program 단방향 의존.
#       ApplicationBase / Engine::Context 는 chapter 별 멤버 구성이 달라 여전히 chapter
#       메인에 인라인 보관.
target_link_libraries(sjhopengl_engine
    PUBLIC project_deps SJH::diagnostics
)

target_compile_features(sjhopengl_engine PUBLIC cxx_std_17)
