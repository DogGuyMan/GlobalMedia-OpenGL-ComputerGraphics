# C++17 표준은 모든 빌드 타입에 적용
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug 경고 플래그 (GCC/Clang 전용, MSVC는 multi-config이므로 generator expression 사용)
if(NOT MSVC)
    add_compile_options(
        $<$<CONFIG:Debug>:-Wall>
        $<$<CONFIG:Debug>:-Werror>
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Debug>:-Warray-bounds>
        $<$<CONFIG:Debug>:-Wunused-but-set-variable>
        # unused-but-set-variable을 에러로 처리하지 않음
        $<$<CONFIG:Debug>:-Wno-error=unused-but-set-variable>
        $<$<CONFIG:Debug>:-Wno-unused-variable>
        $<$<CONFIG:Debug>:-Wno-unused-function>
        $<$<CONFIG:Debug>:-Wno-unused-parameter>
        # sb7 외부 헤더의 #warning (gl.h + gl3.h 동시 포함)을 에러로 처리하지 않음
        "$<$<CONFIG:Debug>:-Wno-error=#warnings>"
    )
endif()
