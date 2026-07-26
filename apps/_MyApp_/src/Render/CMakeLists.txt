# MyApp::Render — 클라이언트 사이드 렌더 파이프라인 제어 (PostFX 체인 조립/토글).
#
# 2026-07-26 신설. 이전에는 부모(apps/_MyApp_/CMakeLists.txt) 의 add_executable 이
#   src/Render/PostProcessController.cpp 를 직접 나열했다 — 서브디렉토리 소스를 부모가
#   손대는 경계 침범. 소스 소유권을 자기 디렉토리로 회수한다.
#
# 주의: PostProcessController.cpp 는 현재 *빈 스캐폴드* (0 바이트).
# Why: 내용이 없으므로 target_link_libraries 를 비워 둔다. 아직 쓰지 않는 의존을 미리
#      선언하면 빌드 그래프가 실제와 어긋나 불필요한 재빌드/순환을 만든다
#      (modular-build-discipline §1 — 실제 사용하는 것만 직접 선언).
#      내용이 들어오는 시점에 .h 노출분은 PUBLIC / .cpp 전용분은 PRIVATE 로 명시할 것.
#      (예상 후보: SJH::render / SJH::material / SJH::resource_registry / MyApp::Playable)
add_library(myapp_render STATIC
    PostProcessController.cpp
)
add_library(MyApp::Render ALIAS myapp_render)

# PUBLIC: src/ 를 루트로 노출 — consumer 가 "Render/PostProcessController.h" 형식으로 include.
# PRIVATE: 자기 디렉토리 — 같은 폴더 헤더를 prefix 없이 include (예: .cpp 의 "PostProcessController.h").
target_include_directories(myapp_render
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(myapp_render PUBLIC cxx_std_17)
