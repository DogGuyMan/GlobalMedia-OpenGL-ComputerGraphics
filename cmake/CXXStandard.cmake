# C++17 표준은 모든 빌드 타입에 적용
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MSVC) # GCC/Clang 전용
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
else() # MSVC 전용 옵션
    add_compile_options(
        # 전 config 공통: 인코딩/표준 준수
        /utf-8           # 소스 파일을 UTF-8로 해석 — 한글 주석/문자열 깨짐 방지
        /Zc:__cplusplus  # __cplusplus 매크로가 실제 표준 버전을 반환하도록
        # Debug 전용: narrowing 경고 침묵 (학습용 코드 빠른 빌드)
        # Release에서는 다시 활성화되어 진짜 narrowing 버그를 노출시킴
        $<$<CONFIG:Debug>:/wd4244>  # 'conversion': double/float 좁힘 변환
        $<$<CONFIG:Debug>:/wd4305>  # 'truncation': double 리터럴 -> float
        $<$<CONFIG:Debug>:/wd4267>  # 'conversion': size_t -> 작은 정수
    )
endif()

# 전 플랫폼: M_PI 등 수학 매크로 활성화 (MSVC에서만 실효, GCC/Clang은 무해)
add_compile_definitions(_USE_MATH_DEFINES)