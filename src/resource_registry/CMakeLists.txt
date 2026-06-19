add_library(sjhopengl_resource_registry STATIC
    resource_registry.cpp
    sprite_resources.cpp     # 2026-06-11 E6 D8 — sprite 공유 자원(plane/material) 해결 (sprite_component.cpp 에서 이주)
    sound.cpp                # M5 — FMOD::Sound RAII wrap
    effect.cpp               # M5 — Effekseer::EffectRef wrap
)
add_library(SJH::resource_registry ALIAS sjhopengl_resource_registry)

target_include_directories(sjhopengl_resource_registry
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            ${CMAKE_SOURCE_DIR}/include   # vmath.h
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# 2026-06-11 E3/E5/E6 — Texture/Image 를 SJH::texture 로 하위추출 후 캐시 파사드로 깊어짐.
# resource_registry.h 가 노출하는 자원 헤더(texture/sprite/buffer)를 PUBLIC 명시화 (기존 transitive link 정식화).
# sprite -> rr 이 D8 로 사라지므로 rr -> sprite 단방향.
target_link_libraries(sjhopengl_resource_registry
    PUBLIC  SJH::common
            SJH::texture       # resource_registry.h: Texture/Image 캐시 API
            SJH::object        # resource_registry.h: Mesh/Model 노출 (RegisterMesh/CreateModel)
            SJH::material      # resource_registry.h: CreateMaterial/FindMaterial 가 Material* 노출
            SJH::program       # resource_registry.h: CreateProgram/FindProgram 가 Program* 노출
            SJH::sprite        # resource_registry.h: uniform_atlas.h (UniformAtlas 캐시) + sprite_resources.cpp
            SJH::buffer        # resource_registry.h: framebuffer.h (Framebuffer 캐시)
            project_deps
            spdlog
            game_deps          # M5 — Sound/Effect 헤더가 FMOD/Effekseer 노출 (spec §6.1)
    PRIVATE SJH::diagnostics
)

target_compile_features(sjhopengl_resource_registry PUBLIC cxx_std_17)
