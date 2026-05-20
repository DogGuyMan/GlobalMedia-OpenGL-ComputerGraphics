add_library(sjhopengl_material STATIC
    material.cpp
)
add_library(SJH::material ALIAS sjhopengl_material)

target_include_directories(sjhopengl_material
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# material.h 는 Program/Texture 를 전방선언만 함 → PUBLIC 은 common/glad 뿐.
# material.cpp 는 빈 TU (SP3 T3: Apply() 폐기) — link 의존 0.
# MaterialApplier 는 GL 호출이 본업이므로 SP3 T5 follow-up 에서 SJH::render 로 이전 →
# 이로써 material → render 단방향 의존이 사라지고 material 이 순수 leaf 모듈로 회귀.
target_link_libraries(sjhopengl_material
    PUBLIC  SJH::common
            project_deps
)

target_compile_features(sjhopengl_material PUBLIC cxx_std_17)
