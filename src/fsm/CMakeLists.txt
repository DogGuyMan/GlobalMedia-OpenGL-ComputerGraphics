# SJH::fsm — 비트 enum 기반 FSM template (spec §6.0)
add_library(sjh_fsm INTERFACE)
add_library(SJH::fsm ALIAS sjh_fsm)

target_include_directories(sjh_fsm INTERFACE
    ${CMAKE_SOURCE_DIR}/src
)

# 헤더에 SJH::Scene::Component 노출  INTERFACE dependency
target_link_libraries(sjh_fsm INTERFACE
    SJH::scene
)
