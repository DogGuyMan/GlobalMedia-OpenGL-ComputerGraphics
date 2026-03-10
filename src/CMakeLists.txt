# ______ SJH 오브젝트 라이브러리 ______
# 정적 라이브러리(.a) 없이 오브젝트 파일을 직접 링크
add_library(SJH OBJECT)

# 각 모듈 서브디렉토리에서 target_sources(SJH ...) 로 소스 추가
add_subdirectory(common)

# src/ 하위 헤더를 #include "common/common.h" 형태로 사용
target_include_directories(SJH PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(SJH PRIVATE cxx_std_17)

# SJH가 필요한 챕터에서 직접 링크: target_link_libraries(... SJH)
