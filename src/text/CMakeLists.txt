# SJH::text — BMFont 비트맵 폰트 + 월드 텍스트 렌더 (게임 무관)
# spec doc/superpowers/specs/2026-06-02-world-text-renderer-design.md
add_library(sjh_text STATIC
    bitmap_font.cpp
    text_renderer.cpp      # <- 추가 (T2)
)
add_library(SJH::text ALIAS sjh_text)

target_include_directories(sjh_text PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# PUBLIC: 헤더에 노출되는 의존
target_link_libraries(sjh_text
    PUBLIC
        SJH::scene             # Component/Actor — TextRenderer 베이스 (T2)
        SJH::sprite            # SpriteRenderer / UniformAtlas — 글리프 렌더 (T2)
        SJH::resource_registry # UniformAtlas 캐시 — BitmapFont
        project_deps           #, gl3w, OpenGL
    PRIVATE
        spdlog                 # bitmap_font.cpp 로드 실패 warn
)
