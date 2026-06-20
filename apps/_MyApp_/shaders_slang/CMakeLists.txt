# apps/_MyApp_/shaders_slang/CMakeLists.txt
# 셰이더 디렉토리의 *자기완결* Slang 컴파일 책임 (책임 분리 2026-06-21).
# 부모(apps/_MyApp_/CMakeLists.txt) 가 SLANGC_EXECUTABLE 가드 + ${CHAPTER_NAME} 정의 +
# ${CHAPTER_NAME} executable 생성 후 add_subdirectory(shaders_slang) 호출.
#
# 책임:
#   1) simple.slang / phong.slang -> GLSL 410 + WGSL + reflection JSON 산출.
#   2) 산출 셰이더 -> 실행파일 옆 resources/shaders overlay (매 빌드 보장).
# 비-책임:
#   - [X] slangc/python3 발견 - cmake/Slang.cmake 가 담당.
#   - [X] sjh_compile_slang/sjh_reflect_slang 정의 - cmake/Slang.cmake 가 담당.
#   - [X] resources/ 디렉토리 복사 - 부모 CMakeLists 의 POST_BUILD 가 담당.

# === 경로 명시 ===
# 산출 위치: build_ninja/apps/_MyApp_/shaders_slang/generated  (본 sub-dir 의 binary 안)
# overlay 대상: build_ninja/apps/_MyApp_/resources/shaders  (exe 거주지 = 부모 binary dir)
#   - $<TARGET_FILE_DIR:${CHAPTER_NAME}> 대신 _app_binary_dir 사용 -
#     generator expression 은 _shaders -> ${CHAPTER_NAME} 의존을 만들어
#     ${CHAPTER_NAME} -> _shaders 와 *사이클* 이 된다.
set(_slang_gen_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
get_filename_component(_app_binary_dir ${CMAKE_CURRENT_BINARY_DIR}/.. ABSOLUTE)

# === simple.slang (Phase 2 PoC - 불릿 마젠타 디버그 + UBO 정통성 검증) ===
set(SIMPLE_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/simple.slang)
sjh_compile_slang(_SIMPLE_VS   ${SIMPLE_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/simple.vs)
sjh_compile_slang(_SIMPLE_FS   ${SIMPLE_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/simple.fs)
sjh_compile_slang(_SIMPLE_WVS  ${SIMPLE_SLANG} vsMain vertex   wgsl ""       ${_slang_gen_dir}/simple.vert.wgsl)
sjh_compile_slang(_SIMPLE_WFS  ${SIMPLE_SLANG} fsMain fragment wgsl ""       ${_slang_gen_dir}/simple.frag.wgsl)
sjh_reflect_slang(_SIMPLE_REFL ${SIMPLE_SLANG} fsMain fragment ${_slang_gen_dir}/simple.refl.json)

# === phong.slang (Phase 2.5 - 라이팅 + LightBlock UBO, import phong_lighting 모듈) ===
# PCB 스테이지가 사용. phong_lighting.slang 은 import 대상(sibling) - depfile 로 동적 추적.
set(PHONG_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/phong.slang)
sjh_compile_slang(_PHONG_VS   ${PHONG_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/phong.vs)
sjh_compile_slang(_PHONG_FS   ${PHONG_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/phong.fs)
sjh_reflect_slang(_PHONG_REFL ${PHONG_SLANG} fsMain fragment ${_slang_gen_dir}/phong.refl.json)

# === overlay 타겟 - 매 빌드 실행으로 셰이더-only 변경 시에도 복사 보장 ===
# custom target 은 매 빌드 실행되므로 셰이더만 바뀌어(=${CHAPTER_NAME} relink 없이) 도 복사 보장.
#   (POST_BUILD overlay 는 target relink 시에만 실행돼 셰이더-only 변경 시 stale 산출을 남겼다 - 2026-06-21 fix).
#   부모의 resources 복사(POST_BUILD)는 copy_directory 가 *머지* 라 본 타겟의 생성 phong.* 등을 지우지 않는다.
add_custom_target(${CHAPTER_NAME}_shaders ALL
    DEPENDS ${_SIMPLE_VS} ${_SIMPLE_FS} ${_SIMPLE_WVS} ${_SIMPLE_WFS} ${_SIMPLE_REFL}
            ${_PHONG_VS} ${_PHONG_FS} ${_PHONG_REFL}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${_slang_gen_dir} ${_app_binary_dir}/resources/shaders
    COMMENT "${CHAPTER_NAME} Slang 셰이더 생성 + resources/shaders overlay")

# 부모 scope 의 ${CHAPTER_NAME} executable 이 본 셰이더 타겟에 의존하도록 결속.
#   (executable 빌드 전에 셰이더가 산출/overlay 되어 있어야 런타임 셰이더 로드가 성공)
add_dependencies(${CHAPTER_NAME} ${CHAPTER_NAME}_shaders)
