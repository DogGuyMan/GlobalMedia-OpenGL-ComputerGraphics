add_library(myapp_algebraic STATIC
    Stat.cpp
)
add_library(MyApp::Algebraic ALIAS myapp_algebraic)

# PUBLIC: src/ 디렉토리를 루트로 노출 → consumer 가 "Algebraic/Stat.h" 형식으로 include.
# PRIVATE: 자기 디렉토리 — Stat.h 내부의 "StatModifier.h" / "Algebraic.Common.h" 같은
#          동일 디렉토리 헤더 따옴표 include 안전성 (소스 디렉토리는 기본 검색되나 명시).
target_include_directories(myapp_algebraic
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# Stat.h: #include "scene/actor.h" — SJH::scene 헤더 의존 (PUBLIC 전파)
target_link_libraries(myapp_algebraic
    PUBLIC  SJH::scene
)

target_compile_features(myapp_algebraic PUBLIC cxx_std_17)
