# cmake/Slang.cmake
# Slang 을 tool-only 로 통합 (find_program + add_custom_command). 라이브러리 링크 안 함.
# 정본 spec: docs/superpowers/specs/2026-06-20-slang-shader-migration-design.md

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
    # GLSL post-process(410 호환 + varying 정규화) 는 Python 스크립트가 담당 (CMake 텍스트처리 이관 2026-06-21).
    #   슬랭이 있는 환경(macOS/Linux dev)엔 python3 가 표준 - 미발견 시 명확 에러로 막는다.
    find_program(SLANG_PYTHON NAMES python3 python DOC "Slang GLSL post-process 인터프리터")
    if(NOT SLANG_PYTHON)
        message(FATAL_ERROR
            "slangc 는 발견했으나 python3 미발견 - Slang GLSL post-process(slang_postprocess_410.py) 불가. "
            "python3 를 PATH 에 추가하거나 slangc 를 제거(커밋된 GLSL 사용)하라.")
    endif()
    set(SLANG_POSTPROCESS_PY "${CMAKE_SOURCE_DIR}/cmake/slang_postprocess_410.py")
else()
    # slangc 미설치 환경(예: Windows CI)은 Slang 컴파일을 건너뛰고 기존 커밋된 GLSL 을 사용.
    # FMOD/Doxygen 과 동일한 optional-tool graceful 패턴 (FATAL 금지).
    message(WARNING
        "slangc not found — Slang 셰이더 컴파일을 건너뜁니다 (기존 GLSL 사용). "
        "활성화하려면 Slang 설치 후 SLANG_ROOT 설정 또는 slangc 를 PATH 에 추가. "
        "참고 spec: docs/superpowers/specs/2026-06-20-slang-shader-migration-design.md")
endif()

# sjh_compile_slang(<out_var> <input.slang> <entry> <stage> <slang_target> <profile> <out_file>)
#   - out_var      : 최종 산출 파일 경로가 담길 변수 (PARENT_SCOPE)
#   - slang_target : glsl | wgsl  (glsl 은 410 post-process 자동 적용)
#   - profile      : glsl_410 등 (wgsl 은 "" 전달)
#   - out_file     : 최종 산출 파일 절대경로 (예: .../simple.fs)
# NOTE: 파라미터명을 TARGET -> SLANG_TARGET 으로 변경 — TARGET 은 CMake 예약 키워드라
#       if(TARGET STREQUAL ...) 가 "target-exists" 체크로 오인됨.
function(sjh_compile_slang OUT_VAR INPUT ENTRY STAGE SLANG_TARGET PROFILE OUT_FILE)
    set(_profile_arg "")
    if(NOT PROFILE STREQUAL "")
        set(_profile_arg -profile ${PROFILE})
    endif()

    # 모듈 import 지원 (import phong_lighting 등): INPUT 디렉토리를 검색 경로(-I)로,
    # 그리고 같은 디렉토리의 모든 .slang 을 DEPENDS 에 추가해 *모듈 파일 편집 시 재컴파일* 되게 한다
    # (그러지 않으면 import 대상(.slang) 수정이 stale 산출을 남긴다 - configure 시점 GLOB).
    get_filename_component(_slang_dir "${INPUT}" DIRECTORY)
    file(GLOB _slang_deps "${_slang_dir}/*.slang")

    if(SLANG_TARGET STREQUAL "glsl")
        # 1단계: slangc -> raw .glsl, 2단계: post-process(410 호환 + varying 정규화) -> OUT_FILE
        set(_raw "${OUT_FILE}.raw")
        add_custom_command(
            OUTPUT  ${OUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/slang_generated/shaders"
            COMMAND ${SLANGC_EXECUTABLE} ${INPUT}
                    -I ${_slang_dir}
                    -target glsl ${_profile_arg}
                    -entry ${ENTRY} -stage ${STAGE}
                    -o ${_raw}
            COMMAND ${SLANG_PYTHON} ${SLANG_POSTPROCESS_PY}
                    --in ${_raw} --out ${OUT_FILE} --stage ${STAGE}
            DEPENDS ${_slang_deps} ${SLANG_POSTPROCESS_PY}
            COMMENT "[slang:glsl410] ${INPUT} (${STAGE}/${ENTRY}) -> ${OUT_FILE}"
            VERBATIM)
    else()
        add_custom_command(
            OUTPUT  ${OUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/slang_generated/shaders"
            COMMAND ${SLANGC_EXECUTABLE} ${INPUT}
                    -I ${_slang_dir}
                    -target ${SLANG_TARGET} ${_profile_arg}
                    -entry ${ENTRY} -stage ${STAGE}
                    -o ${OUT_FILE}
            DEPENDS ${_slang_deps}
            COMMENT "[slang:${SLANG_TARGET}] ${INPUT} (${STAGE}/${ENTRY}) -> ${OUT_FILE}"
            VERBATIM)
    endif()

    set(${OUT_VAR} ${OUT_FILE} PARENT_SCOPE)
endfunction()

# sjh_reflect_slang(<out_var> <input.slang> <entry> <stage> <out_json>)
#   리플렉션 JSON 생성 (Phase 2 의 C++ 바인딩 메타). 코드 생성은 -o /dev/null, -reflection-json 만 수확.
function(sjh_reflect_slang OUT_VAR INPUT ENTRY STAGE OUT_JSON)
    # 모듈 import 해결(-I) + sibling .slang DEPENDS (sjh_compile_slang 과 동일 사유).
    get_filename_component(_slang_dir "${INPUT}" DIRECTORY)
    file(GLOB _slang_deps "${_slang_dir}/*.slang")
    add_custom_command(
        OUTPUT  ${OUT_JSON}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/slang_generated/shaders"
        COMMAND ${SLANGC_EXECUTABLE} ${INPUT}
                -I ${_slang_dir}
                -target glsl -profile glsl_410
                -entry ${ENTRY} -stage ${STAGE}
                -reflection-json ${OUT_JSON} -o ${OUT_JSON}.ignore.glsl
        DEPENDS ${_slang_deps}
        COMMENT "[slang:refl] ${INPUT} (${STAGE}/${ENTRY}) -> ${OUT_JSON}"
        VERBATIM)
    set(${OUT_VAR} ${OUT_JSON} PARENT_SCOPE)
endfunction()
