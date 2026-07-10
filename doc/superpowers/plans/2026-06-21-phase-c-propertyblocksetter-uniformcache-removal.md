# Phase C — PropertyBlockSetter + uniform_cache 모듈 삭제 (D-DPP-5 = B) Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> 상위 핸드오프: [`2026-06-21-slang-phase3-propertyblocksetter-removal-handoff.md`](../../handoffs/2026-06-21-slang-phase3-propertyblocksetter-removal-handoff.md) §0.5.
> 결정: **D-DPP-5 = (B) 모듈 삭제 강행** (사용자 pick, 2026-06-21 저녁). 추천이던 (A) sampler-only 슬림 대신 (B) 채택.
> ⚠ gitignore 로컬. ⚠ **원자적** — 부분 적용 시 program.h API 미정의로 빌드 깨짐 (한 번에 편집 → 한 번 빌드 게이트).

## Summary

Gate Bᴳ(전 셰이더 Slang/UBO) 도달 후, GL410 에서 *값* uniform 은 전부 UBO. 남는 loose 책임은 **sampler 바인딩** 하나. (B) 는 이를 `Program` 의 **sampler-only schema** 로 옮기고 PropertyBlockSetter / uniform_cache 모듈을 삭제한다.

### Decision Log
| ID | 결정 | 선택 | 근거 |
|----|------|------|------|
| D-DPP-5 | 끝점 형태 | (B) 모듈 삭제 | 사용자 pick. passthrough Slang화 + Program sampler schema 로 sampler 바인딩 흡수 → 두 모듈 제거. |
| C-a | sampler 위치 보유 | `Program::mSamplers` (name->loc, link 시 1회 introspect) | 값 schema(uniform_cache) 불요. sampler 만 per-draw 바인딩에 필요. |
| C-b | `program_uniforms` (Uniforms::Set*(Program&)) | 잔존, `GetLocation` -> `glGetUniformLocation` 직접 | 삭제 목록 아님. 유일 live 호출(render_stage uScene)만 남음 — uniform_cache 의존만 절단. |
| C-c | mesh_pass else(loose 행렬) | 제거 | useUbo 항상 true (WorldMesh 전부 UBO). DEAD. |
| C-d | LightUboUploader LooseDispatch | 제거 | loose `UNI_VIEW_POS` 소비자 0. DEAD. |
| C-e | uniform_diagnostics GetType 의존 | 타입검사 완화(live 조회 or 제거) | uniform_cache 삭제 동반. |

## Task 1 — passthrough -> Slang (전 셰이더 Slang화)
- `apps/_MyApp_/shaders_slang/passthrough.slang` 신규 (sampler-only blit: VS = NDC passthrough + uv, FS = `uScene.Sample(uv)`). UBO 블록 불요(멤버 0) — `HasUniformBlocks()==false` 여도 무방(sampler 만).
- `shaders_slang/CMakeLists.txt` 배선 + `Constants.h` SCREEN_PASSTHROUGH VertFile/FragFile -> 생성물 경로.
- 구 `resources/shaders/passthrough.{vs,fs}` 삭제.
- 검증: passthrough blit 정상(화면 그대로 출력).

## Task 2 — Program: sampler schema 도입 + uniform_cache 제거
- `src/program/program.h`: `#include "<program>/uniform_cache.h"` 제거. `mUniformCache` + `GetUniformCache`/`GetLocation`/`GetType` 제거. 신설 `struct Sampler { std::string Name; GLint Location; }; std::vector<Sampler> mSamplers; const std::vector<Sampler>& GetSamplers() const; GLint GetSamplerLocation(const char*) const;`. REVISIT(3중 메커니즘) 주석 -> (b)+(c)+sampler 로 갱신.
- `src/program/program.cpp`: `BuildUniformCache` 호출 제거. `BuildUniformBlocks` 내(또는 신규 `BuildSamplers`)에서 `glGetActiveUniform` 순회하며 `GL_SAMPLER_2D/CUBE` 만 `mSamplers` 적재(location = `glGetUniformLocation`).

## Task 3 — program_uniforms: GetLocation 절단
- `src/program/program_uniforms.cpp`: 각 setter `prog.GetLocation(name)` -> `glGetUniformLocation(prog.<핸들>, name)`. `prog.GetType(name)` 기반 `NotifyTypeMismatch` 는 제거(또는 live 조회). `#include uniform_cache.h` 제거.

## Task 4 — mesh_pass: 죽은 분기 제거 + sampler 바인딩 흡수
- else(loose view/proj/model) 분기 제거. `useUbo` 게이트 단순화(WorldMesh 항상 UBO; 단 passthrough 는 ScreenQuad 경로라 무관).
- `PropertyBlockSetter::Set` 2곳(:166 ScreenQuad, :223 WorldMesh) -> anon-ns `BindSamplers(rc, block, prog)` (prog.GetSamplers() 순회 -> block.Textures find -> `glUniform1i(loc, unit)` + `rc.BindTexture`).
- `#include property_block_setter.h` 제거. REVISIT 주석 갱신(분기 (1) 소멸 반영).

## Task 5 — render_stage uScene 재배선
- `render_stage.impls.cpp:249` `Uniforms::SetInt(mProgram,"uScene",0)` -> sampler schema 기반(`mProgram.GetSamplerLocation("uScene")` + `glUniform1i`) 또는 program_uniforms 잔존분 사용(C-b 후 live 조회라 무방). 최소변경: 그대로 두되 program_uniforms 가 live 조회면 동작.

## Task 6 — LightUboUploader loose 제거
- `LooseDispatch`(:142-188) + `BindTo` sentinel 분기(:290-292) 제거. UBO 경로(UpdateUniformBlock LightBlock)만 유지. 헤더 주석(:9,:16) 갱신.

## Task 7 — 모듈 파일 삭제 + CMake
- `src/render/property_block_setter.{h,cpp}` 삭제, `src/render/CMakeLists.txt` 에서 제거.
- `src/program/uniform_cache.{h,cpp}` 삭제, `src/program/CMakeLists.txt` 에서 제거.

## Task 8 — diagnostics + 잔여 주석
- `src/diagnostics/uniform_diagnostics.h`: mUniformCache 참조 정리(C-e).
- `material.h`/`material_uniforms.*`/`material_property_block.h`/`mesh_pass.h`/`constants.h` 등의 PropertyBlockSetter/UniformCache 주석 갱신(기능 무변).

## 검증 (한 번에)
- 빌드 GREEN (사용자). 런타임 GL 로그 0 error.
- 육안: (1) 일반 씬(phong/simple/transparent/healthbar/billboard/skybox) 텍스처·색 정상, (2) postfx 체인(gamma~fog) 정상, (3) passthrough(비활성 패스 bypass) 정상, (4) sprite FX(uEnableHit/Dissolve), 조명(phong) 정상.

## Out of scope
- `program_uniforms` 모듈 삭제(잔존 — C-b). `material_uniforms`(저장측) 불변. codegen(별 핸드오프).
