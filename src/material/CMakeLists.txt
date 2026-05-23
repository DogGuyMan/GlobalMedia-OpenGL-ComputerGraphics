# SP6 — Material 이 properties bag + Program/UniformCache reference (Unity 정통) 으로 재작성.
#   material_uniforms.cpp 의 self-link symbol 발생 -> STATIC 으로 복귀 (SP4 T5 의 INTERFACE 폐기).
#
# material.h 가 program.h 완전 포함 (Program::RegisterMaterial 호출) -> SJH::program PUBLIC 의존.
# material -> program 정방향 의존 + program.cpp 가 material.h 의 OnProgramReleased 호출 (Observer cascade).
# program 측 CMakeLists 에 SJH::material PRIVATE 추가 (SP6 T2 commit 9841b92).
add_library(sjhopengl_material STATIC
    material_uniforms.cpp
)
add_library(SJH::material ALIAS sjhopengl_material)

target_include_directories(sjhopengl_material
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

# material.h 가 program.h 완전 포함 -> SJH::program PUBLIC.
target_link_libraries(sjhopengl_material
    PUBLIC  SJH::common
            SJH::program
            project_deps
)

target_compile_features(sjhopengl_material PUBLIC cxx_std_17)
