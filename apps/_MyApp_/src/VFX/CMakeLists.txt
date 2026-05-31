# MyApp::VFX — Effekseer leaf Playable. game_deps 의존, Client 거주.
# spec docs/superpowers/specs/2026-05-26-playable-component-interface-design.md §6.2
# handoff doc/design/2026-05-26-M5-handoff.md T2
#
# ※ 본 모듈은 M5 *스켈레톤* — 새 Agent 가 brainstorming 후 채울 것.

add_library(myapp_vfx STATIC
    VFXSystem.cpp             # M5 CL2 — Effekseer Manager+Renderer owner
    EffekseerPlayable.cpp
    ParticleStage.cpp         # SP-ParticleStage — Effekseer→sceneFB 합성 stage
)
add_library(MyApp::VFX ALIAS myapp_vfx)

target_include_directories(myapp_vfx
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(myapp_vfx
    PUBLIC
        SJH::playable   # PlayableBase 베이스 (헤더 노출)
        SJH::render     # IRenderStage 헤더 노출 (ParticleStage.h 의 #include)
        project_deps    # vmath / GL
        game_deps       # Effekseer + EffekseerRendererGL 헤더 + lib 자동 link
    PRIVATE
        SJH::scene      # Scene::Camera — ParticleStage.cpp 만 include, 헤더는 forward decl
        spdlog
)

target_compile_features(myapp_vfx PUBLIC cxx_std_17)
