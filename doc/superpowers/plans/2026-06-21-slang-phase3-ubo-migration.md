# Slang Phase 3 (Phase B) 구현 Plan — 잔여 셰이더 UBO화 + 런타임키 (b)

> 정본 spec: [`doc/superpowers/specs/2026-06-21-dynamic-properties-deprecation-design.md`](../specs/2026-06-21-dynamic-properties-deprecation-design.md) (D-DPP-1 = **(b) 확정**).
> 상위 arc: [`doc/superpowers/plans/2026-06-21-render-state-consolidation.md`](./2026-06-21-render-state-consolidation.md) Phase B (= 본 plan). 완료 = Gate Bᴳ → Phase C(모듈 제거) 착수 가능.
> ⚠ gitignore 로컬. 커밋·빌드 사용자 직접. **각 슬라이스 = 사용자 빌드 + 런타임 GL 로그 + 육안 게이트** (R1 사용자만 확정).

## ★ 착수 전 발견된 갭 (grounded, 2026-06-21) — 슬라이스 0.5 선행 필요
1. **useUbo 경로가 텍스처/샘플러 미바인딩.** [`mesh_pass_processor.cpp:182-200`](../../../src/render/mesh_pass_processor.cpp#L182) useUbo 분기는 FrameBlock/DrawBlock/MaterialBlock(baseColor)만 송신 — phong/simple 이 textureless 라 충분했음. 잔여 셰이더 대부분은 샘플러 보유(`uScene`/`uTex`/skybox/billboard). **GL 4.1 은 sampler 를 UBO 에 못 넣음** → UBO 셰이더라도 sampler 는 `glUniform1i` loose. → **Slice 0.5: useUbo 분기에 텍스처 바인딩 추가** (가장 단순 = useUbo 여도 `PropertyBlockSetter::Set` 를 *호출* — UBO 멤버는 location=-1 라 자연 skip, sampler 만 바인딩됨. bypass 를 textured 셰이더에 한해 해제).
2. **Phase C 게이트 영향 (정직):** spec/상위 plan 의 "loose glUniform* 소비자 0" 은 *sampler* 때문에 엄밀히 0이 안 됨. PropertyBlockSetter(또는 sampler 전용 경량 대체)가 텍스처 바인딩용으로 생존할 수 있음. → **Phase C C1/C2 는 "loose *uniform 값*(float/vec/mat) 경로 제거" 로 범위 재정의** 하고 sampler 바인딩은 별도 처리(잔존 또는 sampler-only setter). Phase C 착수 시 D-DPP-4 에서 확정.

## 목표 (Gate Bᴳ)
- 비-UBO(loose glUniform*) 셰이더 0 → `PropertyBlockSetter`/`uniform_cache` 소비자 0 (Phase C C1/C2 gate).
- fog viewPos → FrameBlock 이전 → `LightUboUploader` loose 경로 마지막 consumer 소멸 (Phase C C3 gate).

## 토큰/툴체인 가드레일 (반드시)
- 새 `.slang` 은 `apps/_MyApp_/shaders_slang/` 에 작성 + [`shaders_slang/CMakeLists.txt`](../../../apps/_MyApp_/shaders_slang/CMakeLists.txt) 에 `sjh_compile_slang`/`sjh_reflect_slang` + `add_custom_target` DEPENDS **라인 추가만** (구조 변경 금지 — 사용자 병렬 toolchain, [[slang-toolchain-conventions]]).
- `scripts/slang_compile.py` / `Slang.cmake` **수정 금지**.
- ⚠ **varying 함정** ([[slang-varying-name-mismatch-macos]]): varying 보유 셰이더는 빌드 GREEN/glslangValidator ≠ 링크 성공. **앱 기동 GL 로그 필수** ("투명/사라짐"=program=null 1순위). post-process 가 `_slangVaryN` 정규화하나 슬라이스마다 재확인.
- 셰이더당 1슬라이스 + 육안 게이트. R1 사용자만.

---

## Slice 0 — (b) UBO 멤버 offset 조회 인프라 (런타임키 enabler)
> postfx 런타임키 슬라이스(아래 S3) *직전* 에만 필요. 컴파일타임 슬라이스(S1)는 불요 → S1 먼저 가도 됨.
- **설계 sub-결정 (구현 시 확정):** offset 출처 = ① refl.json 파싱(spec 표기) vs ② **GL introspection `glGetActiveUniformsiv(GL_UNIFORM_OFFSET)`** (링크된 program 에서 직접, 런타임 native, refl.json 미배포). → ②가 더 단순·정합 추정(Program 이 이미 `BuildUniformBlocks` introspection 보유). 구현 시 확정.
- 신규: `Program` 에 `name→{block, offset, size}` 조회 API (예: `UpdateUniformBlockMember(blockName, memberName, data)` 또는 offset 테이블). R1/R2/R3 가 `Properties.Floats[name]=v` 대신 이걸 호출.
- **acceptance:** 빌드 GREEN. 단위 검증 = phong/simple 의 알려진 멤버 offset 이 std140 값과 일치(grep refl.json 대조).

## Slice 1 — 컴파일타임 키 셰이더 UBO화 (D-DPP-1 무관, 선행)
우선순위(단순→복잡), **각각 별 슬라이스 + 육안 게이트**:
1. **passthrough** (blit, `uScene`) — varying=texcoord. ⚠ ScreenQuadStage bypass 핵심경로.
2. **simple_texture** (`baseColor`/`uTex`/uModel·view·proj) — ground 데칼(shadow/circle) 사용. varying=texcoord.
3. **transparent** (벽: uvScale/uScrollSpeed/tintColor/emissive) — varying.
4. **healthbar** (`uColor`/`uFill`/`uSegmentCount`/`uSegmentSpacing`/`uHeadOffset`) — UI, varying=uv.
5. **billboard_atlas** (atlas uniform) — sprite.
6. **matrix_skybox** (`u_time`/chars/noise_tex) — skybox Pass(depth/cull 특수).
- 각: phong 패턴 (`.slang` + 필요시 `import phong_lighting`(lit 만) + UBO 블록 + `shaders_slang/CMakeLists.txt` 배선). mesh_pass `useUbo` 분기는 자동 (program->HasUniformBlocks()).
- **acceptance(슬라이스별):** 빌드 GREEN + 앱 기동 GL 로그 link 에러 0 + 해당 오브젝트 육안 정상 (사용자).

## Slice 2 — PostFX 셰이더 UBO화 (Slice 0 필요)
- `postprocess/postprocess.vs`(공유 VS) + 8 FS(bloom/blurring/fog/gamma/grayscale_vignetting/invert/sharpening/sobel).
- 컴파일타임 키(예: grayscale_vignetting `uVignetteColor`)는 UBO 멤버 직접.
- **런타임 KEY 3사이트** ((b) Slice 0 사용):
  - R1 [`HpGrayscalePostFX.cpp:47`](../../../apps/_MyApp_/src/Playable/HpGrayscalePostFX.cpp#L47), R2 [`PostFXTweenPlayable.cpp:43`](../../../apps/_MyApp_/src/Playable/PostFXTweenPlayable.cpp#L43), R3 [`render_pipeline.cpp:115`](../../../src/render_bootstrap/render_pipeline.cpp#L115) → `Properties.Floats[name]=v` 를 Slice 0 의 member-offset 송신으로 교체.
- **acceptance:** PostFX 체인(HP grayscale / hit 비네팅 tween) 육안 정상.

## Slice 3 — fog viewPos → FrameBlock 이전
- fog.fs 가 loose viewPos(`UNI_VIEW_POS` sentinel) 대신 FrameBlock UBO 멤버로 viewPos 수신.
- → `LightUboUploader` loose 경로(`LooseDispatch`/`BindTo` sentinel)의 **마지막 consumer 소멸** (Phase C C3 gate 충족).
- **acceptance:** fog 육안 정상 + loose lit 0 grep.

### ⛔ Gate Bᴳ (Phase C 착수 전제)
- [ ] 전 셰이더 UBO화 완료 — `program->HasUniformBlocks()==false` 인 활성 셰이더 0 (grep + 런타임).
- [ ] fog viewPos → FrameBlock 완료.
- [ ] 각 슬라이스 육안 게이트 통과 (사용자 확정).

---

## 설계 REVISIT (사용자 직감 2026-06-21 - 코드에 `[REVISIT]` 마킹)
- `mesh_pass_processor.cpp Process` - 분기 과다(kind dispatch + transition 추적 + UBO/loose ABI + 멤버 업로드 혼재).
- `program.h` - uniform 추적 3중 공존(mUniformCache loose / mUniformBlocks UBO / mUniformMembers).
- 전이분(loose)은 Phase C(D-DPP-4 loose/PropertyBlockSetter 제거)에서 소멸 → **Phase C 후 구조적 재설계 판단** (지금은 마킹만).

## 권고 진행 순서
S1-1(passthrough) → S1-2(simple_texture) → … → S1-6(skybox) → Slice 0(offset infra) → Slice 2(postfx+런타임키) → Slice 3(fog). 각 슬라이스 후 사용자 빌드+육안 게이트, 통과 시 다음.
