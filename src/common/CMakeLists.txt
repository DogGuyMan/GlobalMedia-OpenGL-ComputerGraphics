# common 모듈 소스를 SJH 라이브러리에 추가
target_sources(SJH PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common.h
)
