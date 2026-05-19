add_library(sjhopengl_object STATIC
    mesh.cpp
    model.cpp
    geometry.cpp
    # SJH::Geometry 가 위임하는 Engine::Model 빌더 정의.
    # engine 모듈 전체를 빌드하지 않고 이 파일만 in-place 컴파일.
    # 주의: 훗날 src/CMakeLists.txt 에 add_subdirectory(engine) 가 켜지면
    #       중복 컴파일(중복 심볼) 발생 — 그 시점에 이 줄을 제거할 것.
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/geometry.cpp
)
add_library(SJH::object ALIAS sjhopengl_object)

target_include_directories(sjhopengl_object
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            ${CMAKE_SOURCE_DIR}/include   # vmath.h
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sjhopengl_object
    PUBLIC  SJH::common      # mesh.h: CLASS_PTR(Mesh) 매크로
            SJH::buffer      # mesh.h: Buffer / BufferPtr 필드
            SJH::layout      # mesh.h: VertexLayout / VertexLayoutUPtr 필드
            SJH::material    # model.h: RenderUnit.material / mMaterials / Material::Apply()
            project_deps     # mesh.h: <glad/glad.h>, GLuint
            assimp
)

target_compile_features(sjhopengl_object PUBLIC cxx_std_17)
