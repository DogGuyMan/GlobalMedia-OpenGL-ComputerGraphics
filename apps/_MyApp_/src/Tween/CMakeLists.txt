# MyApp::Tween — Tweeny header-only leaf Playable. game_deps 의존, Client 거주.
# spec docs/superpowers/specs/2026-05-26-playable-component-interface-design.md §6.2
# handoff doc/design/2026-05-26-M5-handoff.md T4 (선택)
#
# ※ 본 모듈은 M5 *스켈레톤* — 새 Agent 가 brainstorming Phase 5 결정 후 본격 또는 폐기.
# ※ Tweeny 가 header-only 라 INTERFACE 타겟. .cpp 가 필요해지면 STATIC 으로 전환.
# ※ tween_playable.h 는 template — vtable anchor 가 *각 인스턴스화 단위* 마다 자동 생성.

add_library(myapp_tween INTERFACE)
add_library(MyApp::Tween ALIAS myapp_tween)

target_include_directories(myapp_tween
    INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(myapp_tween
    INTERFACE
        SJH::playable   # PlayableBase 베이스
        project_deps    # vmath
        game_deps       # tweeny header-only
)

target_compile_features(myapp_tween INTERFACE cxx_std_17)
