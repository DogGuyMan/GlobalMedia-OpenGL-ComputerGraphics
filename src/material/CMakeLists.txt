add_library(sjhopengl_material STATIC material.cpp)
add_library(SJH::material ALIAS sjhopengl_material)

target_include_directories(sjhopengl_material
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# material.h 는 Program/Texture 를 전방선언만 함 → PUBLIC 은 common/glad 뿐.
# material.cpp 만 program_uniforms.h(Uniforms::Set* — out-of-line) 를 쓰므로 PRIVATE.
# texture.h 는 GetTextureID() inline 만 호출 → 헤더 경로(src/)로 충분, link 불필요.
target_link_libraries(sjhopengl_material
    PUBLIC  SJH::common
            project_deps
    PRIVATE SJH::program
)

target_compile_features(sjhopengl_material PRIVATE cxx_std_17)
