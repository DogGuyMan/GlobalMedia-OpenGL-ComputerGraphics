# cmake/Slang.cmake
# Slang 을 tool-only 로 통합 (find_program + add_custom_command). 라이브러리 링크 안 함.
# 정본 spec: doc/superpowers/specs/2026-06-20-slang-shader-migration-design.md
#
# 책임 분담 (2026-06-21 재배치):
#   - 본 파일      : slangc/python3 발견 + 함수 시그니처 + ninja 의존성 그래프 통합만.
#   - slang_compile.py : 명령줄 조립, slangc 호출, GLSL 410 post-process, reflection, depfile 생성.
# 클라이언트 인터페이스 (sjh_compile_slang/sjh_reflect_slang) 무변경.

# DEPFILE 옵션을 add_custom_command 에서 사용 - CMake 3.20+ 정책 NEW 명시.
# (NEW = ninja generator 가 DEPFILE 을 cmake 정의 절대경로 그대로 통과 - 정통 동작)
if(POLICY CMP0116)
    cmake_policy(SET CMP0116 NEW)
endif()

find_program(SLANGC_EXECUTABLE
    NAMES slangc
    PATHS
        $ENV{SLANG_ROOT}/bin
        $ENV{HOME}/slang/bin
    DOC "Slang shader compiler (slangc)")

if(SLANGC_EXECUTABLE)
    message(STATUS "Found slangc: ${SLANGC_EXECUTABLE}")
    if(APPLE)
        # 다운로드 바이너리 quarantine 시 첫 실행이 Gatekeeper 에 막힐 수 있음 (spec 함정).
        execute_process(
            COMMAND xattr -dr com.apple.quarantine "${SLANGC_EXECUTABLE}"
            ERROR_QUIET RESULT_VARIABLE _xattr_rv)
    endif()
    # 본 파이프라인은 Python 으로 단일화 - 명령줄/post-process/reflection/depfile 통합.
    #   슬랭이 있는 환경(macOS/Linux dev)엔 python3 가 표준 - 미발견 시 명확 에러로 막는다.
    find_program(SLANG_PYTHON NAMES python3 python DOC "Slang 파이프라인 인터프리터")
    if(NOT SLANG_PYTHON)
        message(FATAL_ERROR
            "slangc 는 발견했으나 python3 미발견 - Slang 파이프라인(slang_compile.py) 불가. "
            "python3 를 PATH 에 추가하거나 slangc 를 제거(커밋된 GLSL 사용)하라.")
    endif()
    # 스크립트 경로 = scripts/CMakeLists.txt 가 부모 scope 에 등록한 변수 사용.
    #   (직접 경로 하드코딩 회피 - scripts/ 가 *Python 도구 위치 진실의 원천*)
    if(NOT SJH_SLANG_COMPILE_SCRIPT)
        message(FATAL_ERROR
            "SJH_SLANG_COMPILE_SCRIPT 미정의 - root CMakeLists.txt 에서 "
            "add_subdirectory(scripts) 가 include(cmake/Slang.cmake) *이전* 에 호출됐는지 확인.")
    endif()
    set(SLANG_COMPILE_PY "${SJH_SLANG_COMPILE_SCRIPT}")
else()
    # slangc 미설치 환경(예: Windows CI)은 Slang 컴파일을 건너뛰고 기존 커밋된 GLSL 을 사용.
    # FMOD/Doxygen 과 동일한 optional-tool graceful 패턴 (FATAL 금지).
    message(WARNING
        "slangc not found — Slang 셰이더 컴파일을 건너뜁니다 (기존 GLSL 사용). "
        "활성화하려면 Slang 설치 후 SLANG_ROOT 설정 또는 slangc 를 PATH 에 추가. "
        "참고 spec: doc/superpowers/specs/2026-06-20-slang-shader-migration-design.md")
endif()

# sjh_compile_slang(<out_var> <input.slang> <entry> <stage> <slang_target> <profile> <out_file>)
#   - out_var      : 최종 산출 파일 경로가 담길 변수 (PARENT_SCOPE)
#   - slang_target : glsl | wgsl  (glsl 은 410 post-process 자동 적용)
#   - profile      : glsl_410 등 (wgsl 은 "" 전달)
#   - out_file     : 최종 산출 파일 절대경로 (예: .../simple.fs)
# NOTE: 파라미터명 TARGET 회피 - CMake 예약 키워드라 if(TARGET STREQUAL ...) 오인.
# NOTE: 본문은 add_custom_command 의 *얇은 wrapper* - 실 작업은 slang_compile.py.
#       sibling .slang 의존은 Python 이 .d 파일로 생성, ninja 가 DEPFILE 로 동적 추적 (file(GLOB) stale 함정 해소).
function(sjh_compile_slang OUT_VAR INPUT ENTRY STAGE SLANG_TARGET PROFILE OUT_FILE)
    # PROFILE 빈 문자열(wgsl 호출 등) 처리: VERBATIM 이 "" 토큰을 생략하므로
    # --profile 인자 *자체* 를 조건부로 분기 (Python argparse 의 "expected one argument" 회피).
    set(_profile_args "")
    if(NOT PROFILE STREQUAL "")
        set(_profile_args --profile ${PROFILE})
    endif()
    add_custom_command(
        OUTPUT  ${OUT_FILE}
        COMMAND ${SLANG_PYTHON} ${SLANG_COMPILE_PY}
                --slangc ${SLANGC_EXECUTABLE}
                --in ${INPUT}
                --entry ${ENTRY}
                --stage ${STAGE}
                --target ${SLANG_TARGET}
                ${_profile_args}
                --out ${OUT_FILE}
                --depfile ${OUT_FILE}.d
        DEPENDS ${INPUT} ${SLANG_COMPILE_PY}
        DEPFILE ${OUT_FILE}.d
        COMMENT "[slang:${SLANG_TARGET}] ${INPUT} (${STAGE}/${ENTRY}) -> ${OUT_FILE}"
        VERBATIM)
    set(${OUT_VAR} ${OUT_FILE} PARENT_SCOPE)
endfunction()

# sjh_reflect_slang(<out_var> <input.slang> <entry> <stage> <out_json>)
#   리플렉션 JSON 생성 (Phase 2 C++ 바인딩 메타). 본문은 동일 Python 스크립트의 --reflection-out 모드.
function(sjh_reflect_slang OUT_VAR INPUT ENTRY STAGE OUT_JSON)
    add_custom_command(
        OUTPUT  ${OUT_JSON}
        COMMAND ${SLANG_PYTHON} ${SLANG_COMPILE_PY}
                --slangc ${SLANGC_EXECUTABLE}
                --in ${INPUT}
                --entry ${ENTRY}
                --stage ${STAGE}
                --reflection-out ${OUT_JSON}
                --depfile ${OUT_JSON}.d
        DEPENDS ${INPUT} ${SLANG_COMPILE_PY}
        DEPFILE ${OUT_JSON}.d
        COMMENT "[slang:refl] ${INPUT} (${STAGE}/${ENTRY}) -> ${OUT_JSON}"
        VERBATIM)
    set(${OUT_VAR} ${OUT_JSON} PARENT_SCOPE)
endfunction()
