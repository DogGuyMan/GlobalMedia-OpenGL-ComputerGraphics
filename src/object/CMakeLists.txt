add_library(sjhopengl_object STATIC
    mesh.cpp
    model.cpp
    geometry.cpp
)
add_library(SJH::object ALIAS sjhopengl_object)

target_include_directories(sjhopengl_object
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            ${CMAKE_SOURCE_DIR}/include   #.h
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# 2026-06-11 E1 사이클 해소: 광원 컴포넌트(DirLight/PointLight/SpotLight)를 scene 으로 이주.
# object/light.h 는 순수 데이터(Light POD + GetAttenuationCoeff)만 남아 SJH::scene 의존 제거 -> object 는 scene 에 단방향 피의존만.
target_link_libraries(sjhopengl_object
    PUBLIC  SJH::common      # mesh.h: CLASS_PTR(Mesh) 매크로
            SJH::buffer      # mesh.h: Buffer / BufferPtr 필드
            SJH::layout      # mesh.h: VertexLayout / VertexLayoutUPtr 필드
            SJH::material    # model.h: RenderUnit.material / mMaterials / Material
            SJH::texture     # 2026-06-11 E5 — model.h/model.cpp: TextureUPtr (rr 에서 texture 하위추출, transitive 명시화)
            project_deps     # mesh.h: <glad/glad.h>, GLuint
            assimp
)

target_compile_features(sjhopengl_object PUBLIC cxx_std_17)
