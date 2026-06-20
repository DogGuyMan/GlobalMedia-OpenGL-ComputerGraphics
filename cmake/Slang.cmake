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

    if(SLANG_TARGET STREQUAL "glsl")
        # 1단계: slangc -> raw .glsl, 2단계: post-process -> OUT_FILE
        set(_raw "${OUT_FILE}.raw")
        add_custom_command(
            OUTPUT  ${OUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/slang_generated/shaders"
            COMMAND ${SLANGC_EXECUTABLE} ${INPUT}
                    -target glsl ${_profile_arg}
                    -entry ${ENTRY} -stage ${STAGE}
                    -o ${_raw}
            COMMAND ${CMAKE_COMMAND}
                    -DSLANG_IN=${_raw} -DSLANG_OUT=${OUT_FILE}
                    -P ${CMAKE_SOURCE_DIR}/cmake/SlangPostProcess410.cmake
            DEPENDS ${INPUT}
            COMMENT "[slang:glsl410] ${INPUT} (${STAGE}/${ENTRY}) -> ${OUT_FILE}"
            VERBATIM)
    else()
        add_custom_command(
            OUTPUT  ${OUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/slang_generated/shaders"
            COMMAND ${SLANGC_EXECUTABLE} ${INPUT}
                    -target ${SLANG_TARGET} ${_profile_arg}
                    -entry ${ENTRY} -stage ${STAGE}
                    -o ${OUT_FILE}
            DEPENDS ${INPUT}
            COMMENT "[slang:${SLANG_TARGET}] ${INPUT} (${STAGE}/${ENTRY}) -> ${OUT_FILE}"
            VERBATIM)
    endif()

    set(${OUT_VAR} ${OUT_FILE} PARENT_SCOPE)
endfunction()

# sjh_reflect_slang(<out_var> <input.slang> <entry> <stage> <out_json>)
#   리플렉션 JSON 생성 (Phase 2 의 C++ 바인딩 메타). 코드 생성은 -o /dev/null, -reflection-json 만 수확.
function(sjh_reflect_slang OUT_VAR INPUT ENTRY STAGE OUT_JSON)
    add_custom_command(
        OUTPUT  ${OUT_JSON}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/slang_generated/shaders"
        COMMAND ${SLANGC_EXECUTABLE} ${INPUT}
                -target glsl -profile glsl_410
                -entry ${ENTRY} -stage ${STAGE}
                -reflection-json ${OUT_JSON} -o ${OUT_JSON}.ignore.glsl
        DEPENDS ${INPUT}
        COMMENT "[slang:refl] ${INPUT} (${STAGE}/${ENTRY}) -> ${OUT_JSON}"
        VERBATIM)
    set(${OUT_VAR} ${OUT_JSON} PARENT_SCOPE)
endfunction()
