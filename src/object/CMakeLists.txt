add_library(sjhopengl_object STATIC
    mesh.cpp
    model.cpp
    geometry.cpp
    light.cpp
)
add_library(SJH::object ALIAS sjhopengl_object)

target_include_directories(sjhopengl_object
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            ${CMAKE_SOURCE_DIR}/include   # vmath.h
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# SP5 — light.h 가 Scene::Component 상속, light.cpp 가 Actor::GetWorldMatrix 호출
# -> SJH::scene PUBLIC 의존 (헤더 + cpp 둘 다). 단 SJH::scene 도 SJH::object PUBLIC 의존
# (model_spawner) -> CMake 가 static 라이브러리 순환을 link command 에 자동 반복 처리.
target_link_libraries(sjhopengl_object
    PUBLIC  SJH::common      # mesh.h: CLASS_PTR(Mesh) 매크로
            SJH::buffer      # mesh.h: Buffer / BufferPtr 필드
            SJH::layout      # mesh.h: VertexLayout / VertexLayoutUPtr 필드
            SJH::material    # model.h: RenderUnit.material / mMaterials / Material
            SJH::scene       # light.h: Scene::Component base 상속 (SP5)
            project_deps     # mesh.h: <glad/glad.h>, GLuint
            assimp
)

target_compile_features(sjhopengl_object PUBLIC cxx_std_17)
