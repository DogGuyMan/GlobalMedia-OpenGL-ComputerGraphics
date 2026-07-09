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

# === simple_texture.slang (Phase 3 Slice 1-1 - texture(uTex,uv)*baseColor, ground 데칼 사용) ===
# 3블록 UBO + uTex 샘플러(loose). 기존 hand-written simple_texture.{vs,fs} 대체 (소스 GLSL 삭제됨).
set(SIMPLE_TEXTURE_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/simple_texture.slang)
sjh_compile_slang(_SIMPLE_TEXTURE_VS   ${SIMPLE_TEXTURE_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/simple_texture.vs)
sjh_compile_slang(_SIMPLE_TEXTURE_FS   ${SIMPLE_TEXTURE_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/simple_texture.fs)
sjh_reflect_slang(_SIMPLE_TEXTURE_REFL ${SIMPLE_TEXTURE_SLANG} fsMain fragment ${_slang_gen_dir}/simple_texture.refl.json)

# === passthrough.slang (Phase C C-2 - ScreenQuad bypass blit, uScene 샘플러만, UBO 없음) ===
# 기존 hand-written passthrough.{vs,fs} 대체 (소스 GLSL 삭제됨). 산출은 resources/shaders 루트(postprocess/ 아님).
set(PASSTHROUGH_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/passthrough.slang)
sjh_compile_slang(_PASSTHROUGH_VS ${PASSTHROUGH_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/passthrough.vs)
sjh_compile_slang(_PASSTHROUGH_FS ${PASSTHROUGH_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/passthrough.fs)

# === transparent.slang (Phase 3 Slice - 반투명 unlit emissive 벽, MaterialBlock 다중멤버) ===
# 기존 hand-written transparent.{vs,fs} 대체 (소스 GLSL 삭제됨).
set(TRANSPARENT_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/transparent.slang)
sjh_compile_slang(_TRANSPARENT_VS   ${TRANSPARENT_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/transparent.vs)
sjh_compile_slang(_TRANSPARENT_FS   ${TRANSPARENT_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/transparent.fs)
sjh_reflect_slang(_TRANSPARENT_REFL ${TRANSPARENT_SLANG} fsMain fragment ${_slang_gen_dir}/transparent.refl.json)

# === warning_wall.slang (transparent 기반 - 시간 U-스크롤 경고 벽, MaterialBlock: tintColor/uvScale/uTime/uScrollSpeed) ===
# WallMat 이 사용 (WorldSceneBuilder EnsureWarningWallProgram). uTime 은 main.cpp 렌더루프가 매 프레임 주입.
set(WARNING_WALL_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/warning_wall.slang)
sjh_compile_slang(_WARNING_WALL_VS   ${WARNING_WALL_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/warning_wall.vs)
sjh_compile_slang(_WARNING_WALL_FS   ${WARNING_WALL_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/warning_wall.fs)
sjh_reflect_slang(_WARNING_WALL_REFL ${WARNING_WALL_SLANG} fsMain fragment ${_slang_gen_dir}/warning_wall.refl.json)

# === healthbar.slang (Phase 3 Slice - 머리 위 빌보드 체력바, MaterialBlock 6멤버, 텍스처 없음) ===
# 기존 hand-written healthbar.{vs,fs} 대체 (소스 GLSL 삭제됨). 빌보드 카메라축 = transpose+mul.
set(HEALTHBAR_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/healthbar.slang)
sjh_compile_slang(_HEALTHBAR_VS   ${HEALTHBAR_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/healthbar.vs)
sjh_compile_slang(_HEALTHBAR_FS   ${HEALTHBAR_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/healthbar.fs)
sjh_reflect_slang(_HEALTHBAR_REFL ${HEALTHBAR_SLANG} fsMain fragment ${_slang_gen_dir}/healthbar.refl.json)

# === matrix_skybox.slang (Phase 3 Slice - Matrix 스카이박스, .xyww depth + 회전전용 view + 2 sampler) ===
# 기존 hand-written matrix_skybox.{vs,fs} 대체 (소스 GLSL 삭제됨). uModel/DrawBlock 없음.
set(MATRIX_SKYBOX_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/matrix_skybox.slang)
sjh_compile_slang(_MATRIX_SKYBOX_VS   ${MATRIX_SKYBOX_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/matrix_skybox.vs)
sjh_compile_slang(_MATRIX_SKYBOX_FS   ${MATRIX_SKYBOX_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/matrix_skybox.fs)
sjh_reflect_slang(_MATRIX_SKYBOX_REFL ${MATRIX_SKYBOX_SLANG} fsMain fragment ${_slang_gen_dir}/matrix_skybox.refl.json)

# === billboard_atlas.slang (Phase 3 Slice - sprite 빌보드, MaterialBlock 10멤버 + bool 2 + 2 sampler) ===
# 기존 hand-written billboard_atlas.{vs,fs} 대체 (소스 GLSL 삭제됨). SpriteRenderer(=MeshRenderer) 사용 - 최다 호출.
set(BILLBOARD_ATLAS_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/billboard_atlas.slang)
sjh_compile_slang(_BILLBOARD_ATLAS_VS   ${BILLBOARD_ATLAS_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/billboard_atlas.vs)
sjh_compile_slang(_BILLBOARD_ATLAS_FS   ${BILLBOARD_ATLAS_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/billboard_atlas.fs)
sjh_reflect_slang(_BILLBOARD_ATLAS_REFL ${BILLBOARD_ATLAS_SLANG} fsMain fragment ${_slang_gen_dir}/billboard_atlas.refl.json)

# === postfx (Phase 3 Slice 2, 옵션 B 자립 effect) - 생성물은 postprocess/ 서브디렉토리 (로더 경로 일치) ===
# 각 effect 가 자체 NDC passthrough VS + FS 보유 (공유 postprocess.vs 점진 폐기). 미전환분은 hand-written 유지.
set(GAMMA_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/gamma.slang)
sjh_compile_slang(_GAMMA_VS   ${GAMMA_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/postprocess/gamma.vs)
sjh_compile_slang(_GAMMA_FS   ${GAMMA_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/postprocess/gamma.fs)
sjh_reflect_slang(_GAMMA_REFL ${GAMMA_SLANG} fsMain fragment ${_slang_gen_dir}/postprocess/gamma.refl.json)

set(GRAYSCALE_VIGNETTING_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/grayscale_vignetting.slang)
sjh_compile_slang(_GV_VS   ${GRAYSCALE_VIGNETTING_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/postprocess/grayscale_vignetting.vs)
sjh_compile_slang(_GV_FS   ${GRAYSCALE_VIGNETTING_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/postprocess/grayscale_vignetting.fs)
sjh_reflect_slang(_GV_REFL ${GRAYSCALE_VIGNETTING_SLANG} fsMain fragment ${_slang_gen_dir}/postprocess/grayscale_vignetting.refl.json)

# postfx uScene-only(invert/sharpening/sobel/blurring - 값 uniform 없음) + bloom(3멤버). 자립 VS+FS.
set(INVERT_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/invert.slang)
sjh_compile_slang(_INVERT_VS   ${INVERT_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/postprocess/invert.vs)
sjh_compile_slang(_INVERT_FS   ${INVERT_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/postprocess/invert.fs)
sjh_reflect_slang(_INVERT_REFL ${INVERT_SLANG} fsMain fragment ${_slang_gen_dir}/postprocess/invert.refl.json)

set(BLURRING_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/blurring.slang)
sjh_compile_slang(_BLURRING_VS   ${BLURRING_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/postprocess/blurring.vs)
sjh_compile_slang(_BLURRING_FS   ${BLURRING_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/postprocess/blurring.fs)
sjh_reflect_slang(_BLURRING_REFL ${BLURRING_SLANG} fsMain fragment ${_slang_gen_dir}/postprocess/blurring.refl.json)

set(SHARPENING_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/sharpening.slang)
sjh_compile_slang(_SHARPENING_VS   ${SHARPENING_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/postprocess/sharpening.vs)
sjh_compile_slang(_SHARPENING_FS   ${SHARPENING_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/postprocess/sharpening.fs)
sjh_reflect_slang(_SHARPENING_REFL ${SHARPENING_SLANG} fsMain fragment ${_slang_gen_dir}/postprocess/sharpening.refl.json)

set(SOBEL_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/sobel.slang)
sjh_compile_slang(_SOBEL_VS   ${SOBEL_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/postprocess/sobel.vs)
sjh_compile_slang(_SOBEL_FS   ${SOBEL_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/postprocess/sobel.fs)
sjh_reflect_slang(_SOBEL_REFL ${SOBEL_SLANG} fsMain fragment ${_slang_gen_dir}/postprocess/sobel.refl.json)

set(BLOOM_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/bloom.slang)
sjh_compile_slang(_BLOOM_VS   ${BLOOM_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/postprocess/bloom.vs)
sjh_compile_slang(_BLOOM_FS   ${BLOOM_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/postprocess/bloom.fs)
sjh_reflect_slang(_BLOOM_REFL ${BLOOM_SLANG} fsMain fragment ${_slang_gen_dir}/postprocess/bloom.refl.json)

set(FOG_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/fog.slang)   # 마지막 postfx - mat4+depth sampler+int
sjh_compile_slang(_FOG_VS   ${FOG_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/postprocess/fog.vs)
sjh_compile_slang(_FOG_FS   ${FOG_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/postprocess/fog.fs)
sjh_reflect_slang(_FOG_REFL ${FOG_SLANG} fsMain fragment ${_slang_gen_dir}/postprocess/fog.refl.json)

set(DEPTH_DEBUG_SLANG ${CMAKE_CURRENT_SOURCE_DIR}/depth_debug.slang)   # Phase1 학습 - 깊이 흑백 시각화(depth sampler + float3 UBO)
sjh_compile_slang(_DEPTH_DEBUG_VS   ${DEPTH_DEBUG_SLANG} vsMain vertex   glsl glsl_410 ${_slang_gen_dir}/postprocess/depth_debug.vs)
sjh_compile_slang(_DEPTH_DEBUG_FS   ${DEPTH_DEBUG_SLANG} fsMain fragment glsl glsl_410 ${_slang_gen_dir}/postprocess/depth_debug.fs)
sjh_reflect_slang(_DEPTH_DEBUG_REFL ${DEPTH_DEBUG_SLANG} fsMain fragment ${_slang_gen_dir}/postprocess/depth_debug.refl.json)

# === overlay 타겟 - 매 빌드 실행으로 셰이더-only 변경 시에도 복사 보장 ===
# custom target 은 매 빌드 실행되므로 셰이더만 바뀌어(=${CHAPTER_NAME} relink 없이) 도 복사 보장.
#   (POST_BUILD overlay 는 target relink 시에만 실행돼 셰이더-only 변경 시 stale 산출을 남겼다 - 2026-06-21 fix).
#   부모의 resources 복사(POST_BUILD)는 copy_directory 가 *머지* 라 본 타겟의 생성 phong.* 등을 지우지 않는다.
add_custom_target(${CHAPTER_NAME}_shaders ALL
    DEPENDS ${_SIMPLE_VS} ${_SIMPLE_FS} ${_SIMPLE_WVS} ${_SIMPLE_WFS} ${_SIMPLE_REFL}
            ${_PHONG_VS} ${_PHONG_FS} ${_PHONG_REFL}
            ${_SIMPLE_TEXTURE_VS} ${_SIMPLE_TEXTURE_FS} ${_SIMPLE_TEXTURE_REFL}
            ${_PASSTHROUGH_VS} ${_PASSTHROUGH_FS}
            ${_TRANSPARENT_VS} ${_TRANSPARENT_FS} ${_TRANSPARENT_REFL}
            ${_WARNING_WALL_VS} ${_WARNING_WALL_FS} ${_WARNING_WALL_REFL}
            ${_HEALTHBAR_VS} ${_HEALTHBAR_FS} ${_HEALTHBAR_REFL}
            ${_MATRIX_SKYBOX_VS} ${_MATRIX_SKYBOX_FS} ${_MATRIX_SKYBOX_REFL}
            ${_BILLBOARD_ATLAS_VS} ${_BILLBOARD_ATLAS_FS} ${_BILLBOARD_ATLAS_REFL}
            ${_GAMMA_VS} ${_GAMMA_FS} ${_GAMMA_REFL}
            ${_GV_VS} ${_GV_FS} ${_GV_REFL}
            ${_INVERT_VS} ${_INVERT_FS} ${_INVERT_REFL}
            ${_BLURRING_VS} ${_BLURRING_FS} ${_BLURRING_REFL}
            ${_SHARPENING_VS} ${_SHARPENING_FS} ${_SHARPENING_REFL}
            ${_SOBEL_VS} ${_SOBEL_FS} ${_SOBEL_REFL}
            ${_BLOOM_VS} ${_BLOOM_FS} ${_BLOOM_REFL}
            ${_FOG_VS} ${_FOG_FS} ${_FOG_REFL}
            ${_DEPTH_DEBUG_VS} ${_DEPTH_DEBUG_FS} ${_DEPTH_DEBUG_REFL}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${_slang_gen_dir} ${_app_binary_dir}/resources/shaders
    COMMENT "${CHAPTER_NAME} Slang 셰이더 생성 + resources/shaders overlay")

# 부모 scope 의 ${CHAPTER_NAME} executable 이 본 셰이더 타겟에 의존하도록 결속.
#   (executable 빌드 전에 셰이더가 산출/overlay 되어 있어야 런타임 셰이더 로드가 성공)
add_dependencies(${CHAPTER_NAME} ${CHAPTER_NAME}_shaders)
