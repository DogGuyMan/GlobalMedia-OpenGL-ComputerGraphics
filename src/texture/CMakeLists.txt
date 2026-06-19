# SJH::texture — leaf GPU 자원 (Texture RAII + Image 픽셀 로드). 2026-06-11 E3/E5/E6 사이클 해소로
# resource_registry 에서 하위 추출 (18번째 모듈). 상위 캐시 파사드(resource_registry)와 분리해
# leaf 만 필요한 buffer/object/sprite 가 rr 허브 전체로 역의존하던 사이클을 끊는다.
add_library(sjhopengl_texture STATIC
    texture.cpp
    image.cpp                # STB_IMAGE_IMPLEMENTATION 단일 owner (메모리 규칙 유지)
)
add_library(SJH::texture ALIAS sjhopengl_texture)

target_include_directories(sjhopengl_texture
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            ${CMAKE_SOURCE_DIR}/include   # image.h: vmath.h + image.cpp: stb_image.h (체크인본)
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sjhopengl_texture
    PUBLIC  SJH::common      # texture.h/image.h: CLASS_PTR 매크로
            project_deps     # GLuint / gl3w
    PRIVATE spdlog           # texture.cpp / image.cpp 의 spdlog::error
)

target_compile_features(sjhopengl_texture PUBLIC cxx_std_17)
