# SJH::timer — 게임플레이 타이머 (header-only, spec docs/.../2026-06-01-timer-module-design.md)
add_library(sjh_timer INTERFACE)
add_library(SJH::timer ALIAS sjh_timer)

target_include_directories(sjh_timer INTERFACE
    ${CMAKE_SOURCE_DIR}/src
)

# multiple_timer.h 가 SJH::Scene::Component (scene/actor.h) 를 헤더에 노출 → scene INTERFACE 의존
target_link_libraries(sjh_timer INTERFACE
    SJH::scene
)
