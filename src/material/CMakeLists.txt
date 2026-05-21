# SP3 T3 (Apply() 폐기) + T5 follow-up (MaterialApplier → SJH::render 이전) 결과로
# material.cpp 가 빈 TU 가 되어 ranlib 가 'has no symbols' 경고를 발생시켰음.
# INTERFACE 라이브러리로 전환 — 헤더 only, 컴파일 산출물 없음, ranlib 경고 해소.
#
# material.h 의 Program/Texture 는 전방선언만 → PUBLIC link 는 common + project_deps 뿐.
# Material 은 순수 데이터 leaf 모듈 — 다른 의존 없음.
add_library(sjhopengl_material INTERFACE)
add_library(SJH::material ALIAS sjhopengl_material)

target_include_directories(sjhopengl_material INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(sjhopengl_material INTERFACE
    SJH::common
    project_deps
)

target_compile_features(sjhopengl_material INTERFACE cxx_std_17)
