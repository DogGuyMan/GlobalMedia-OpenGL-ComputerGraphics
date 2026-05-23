#  ========================================================================
#  Core Engine Modules — src/<모듈>/
#  ========================================================================
#  각 모듈은 자체 CMakeLists.txt 에서 include 경로를 관리.
#  target_link_libraries(SJH::<모듈>) 시 include 경로가 INTERFACE 로 전파됨.
#  모듈 간 명시적 의존성: 각 모듈 CMakeLists.txt 에 target_link_libraries 추가.
#
#  *Core 라이브러리 vs Client 양산 코드 분리* (Unreal Runtime / Cocos libcocos2d 정통):
#  - `src/` = 엔진 코어 (저변동) — 13 모듈. 단일 INTERFACE 타깃 SJH::engine 으로 묶음.
#  - `apps/` = 클라이언트 (양산) — 챕터/데모. Mouse/Keyboard/CameraController 같은
#    Component 상속 구현체는 챕터 측 또는 챕터-공용 헬퍼로 발전.

add_subdirectory(buffer)
add_subdirectory(object)
add_subdirectory(common)
add_subdirectory(diagnostics)
add_subdirectory(layout)
add_subdirectory(program)
add_subdirectory(material)
add_subdirectory(scene)
add_subdirectory(shader)
add_subdirectory(render)
add_subdirectory(resource_registry)
add_subdirectory(input)
# add_subdirectory(imgui) — src/imgui/ 모듈 폐기됨 (사용자 working tree 변경).
#   ImGui 사용 챕터 (imguitest / audio_demo) 는 별도 link 패턴.

#  ========================================================================
#  SJH::engine — 12 코어 모듈 INTERFACE 묶음
#  ========================================================================
#  정통:
#  - Cocos2D-x: libcocos2d.a 단일 정적 binary 가 base/2d/3d/renderer/ui/... 통합.
#  - Unreal Engine: Engine/Source/Runtime/ 의 모든 Module 이 UnrealEngine target 으로 묶임.
#  - Unity (C++ 내부): Runtime/{Core, Graphics, ...} 가 module manifest 로 통합.
#  - Godot: tools.dll 단일 binary 가 core/scene/servers/modules 통합.
#
#  사용:
#    target_link_libraries(<chapter> PRIVATE project_deps SJH::engine)
#    -> 모든 코어 모듈 (13개) 자동 link + include 경로 자동 전파.
#
#  명시 모듈 link 도 호환 — SJH::scene 만 link 같은 부분 사용 OK.
#  주의 — SJH::imgui 도 포함. ImGui *컨텍스트 초기화* 는 클라이언트 책임 (link 만 보장).
add_library(sjhopengl_engine INTERFACE)
add_library(SJH::engine ALIAS sjhopengl_engine)

target_link_libraries(sjhopengl_engine INTERFACE
    SJH::common
    SJH::buffer
    SJH::diagnostics
    SJH::input
    SJH::layout
    SJH::material
    SJH::object
    SJH::program
    SJH::render
    SJH::resource_registry
    SJH::scene
    SJH::shader
)
