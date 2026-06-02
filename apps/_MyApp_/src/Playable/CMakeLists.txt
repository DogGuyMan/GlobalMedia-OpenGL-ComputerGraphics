# 연출 foundation — PlayableDirector(named Playable 보유 + 중앙 tick + IActorPresentation sink)
# + PostFXRegistry(PostFX pass Material 레지스트리). 신규 STATIC lib (Entity↔Spawns 사이클 회피용 —
# Entity 에 두면 Spawns→Entity 와 Entity→Spawns 순환이 생긴다).
add_library(myapp_playable STATIC
    PlayableDirector.cpp
    PostFXRegistry.cpp
    PostFXTweenPlayable.cpp
    SpriteFxPlayable.cpp
    HpGrayscalePostFX.cpp
)
add_library(MyApp::Playable ALIAS myapp_playable)

target_include_directories(myapp_playable
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(myapp_playable
    PUBLIC
        SJH::scene       # SJH::Scene::Component 베이스
        SJH::playable    # IPlayable / PlayableBase / Composite (map 보유 + Register 시그니처)
        SJH::material    # PostFXRegistry — SJH::Material*
        # MyApp::Entity 링크 제거 — IActorPresentation 은 순수 인터페이스(헤더-only, 컴파일 심볼 0).
        # target_include_directories PUBLIC(=apps/_MyApp_/src) 로 include 가 이미 해소되므로 링크 불필요.
        # 제거 이유: myapp_entity → MyApp::Playable 링크(B3-2) 와의 순환을 끊기 위함.
        game_deps        # tweeny (PostFXTweenPlayable.h 가 tweeny::tween 노출)
    PRIVATE
        SJH::sprite             # SpriteFxPlayable.cpp — SpriteRenderer.enableHit/dissolve (.cpp 전용)
        SJH::resource_registry  # SpriteFxPlayable.cpp — dissolve 노이즈 텍스처 로드 (Image::Load + CreateTexture)
)
target_compile_features(myapp_playable PUBLIC cxx_std_17)
