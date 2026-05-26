# MyApp::Audio — FMOD Core + Studio leaf Playable. game_deps 의존, Client 거주.
# spec docs/superpowers/specs/2026-05-26-playable-component-interface-design.md §6.2
# handoff doc/design/2026-05-26-M5-handoff.md T1 + T3
#
# ※ 본 모듈은 M5 *스켈레톤* — 새 Agent 가 brainstorming 후 헤더/.cpp 채울 것.
#   현재는 vtable anchor 만 정의된 stub 라 컴파일 즉시 가능 + 빈 PlayableBase 파생체 동작.

add_library(myapp_audio STATIC
    AudioSystem.cpp           # M5 CL1 — FMOD Core+Studio owner
    FmodPlayable.cpp
    FmodStudioPlayable.cpp
)
add_library(MyApp::Audio ALIAS myapp_audio)

target_include_directories(myapp_audio
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(myapp_audio
    PUBLIC
        SJH::playable   # PlayableBase 베이스 (헤더 노출)
        project_deps    # vmath / GL — 헤더 안에서 사용 가능성
        game_deps       # fmod / fmodstudio 헤더 + lib 자동 link (FMOD 설치 가정)
    PRIVATE
        spdlog          # warn-once 패턴 (.cpp 안에서만)
)

target_compile_features(myapp_audio PUBLIC cxx_std_17)
