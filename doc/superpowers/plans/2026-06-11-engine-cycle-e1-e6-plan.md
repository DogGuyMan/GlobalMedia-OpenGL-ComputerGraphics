# 엔진 의존 사이클 E1~E6 해소 — 구현 Plan (후보 ①②③)

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **상태: 계획만 (구현 미착수).** 클라 plan(`2026-06-11-client-cycle-c1-c4-dip-plan.md`)과 대칭되는 file-by-file Task 분해.
> 상위 인덱스: `doc/superpowers/plans/2026-06-11-dependency-cycle-master-roadmap.md` (후보 ①②③④ 전체).
> **설계 정본 = `doc/superpowers/specs/2026-06-11-engine-dependency-cycle-refactor-design.md` (D1~D8, 사용자 합의 2026-06-11).** 본 plan 은 그 spec 의 슬라이스를 Task 체크리스트로 옮긴 것 — *결정 근거/대안은 spec 참조*(중복 미서술). 모든 파일 경로는 2026-06-11 grounded.

---

## 0. TL;DR

엔진 6 mutual(E1~E6) → 0. 7-노드 SCC `{buffer,object,playable,render,resource_registry,scene,sprite}` 해체. **슬라이스 순서 ②→③→①** (spec D5). 각 슬라이스 끝 `_MyApp_` 빌드 GREEN.

| 후보 | 슬라이스 | 끊는 사이클 | spec § |
|---|---|---|---|
| ② RenderTarget+Light | 슬라이스 1 | E1(scene↔object) · E4(buffer↔render) | spec §2 |
| ③ scene 팩토리 상위이동 | 슬라이스 2 | E2(scene↔render) | spec §3 |
| ① Texture/Image 하위추출 | 슬라이스 3 | E3·E5·E6(rr↔buffer/object/sprite) | spec §4 |

---

## 슬라이스 1 — 묶음 ② (E1 + E4) · spec §2  ✅ 완료 (빌드 GREEN·미커밋 2026-06-19)

> 결과: `_MyApp_` GREEN. **E1 절단** = `grep '#include "scene/"' src/object`=0 + `grep '#include "object/light.h"' src/program`=0(D6). **E4 절단** = `grep '#include "render/"' src/buffer`=0. 신규 `src/scene/light.{h,cpp}`, `src/scene/light.cpp` 삭제, `render/render_target.{h,cpp}`→`buffer/`. D6 광원 setter 3종 = `light_uniform_dispatcher.cpp` 익명 ns 이주. test_light 에 `SJH::scene` link 추가(DirLight 등 구성). ⚠ ENABLE_TESTING configure 는 **사전 breakage**(root `CMakeLists.txt:44` `add_subdirectory(test_smoke)` 디렉토리 부재, 내 변경 무관)로 막힘 - test 타겟 컴파일 검증은 그 해소 후.

### Task 1.1 — E1: Light 컴포넌트 → scene + D6 동반
- **이동:** `object/light.h`·`light.cpp` 의 `DirLight`/`PointLight`/`SpotLight` → 신설 `scene/light.{h,cpp}` (ns `SJH` 불변, 가드 `__SJH_SCENE_LIGHT_H__`). `object/light.h` 잔존 = `Light` POD + `GetAttenuationCoeff` (`scene/actor.h` include 제거 = 절단점).
- **D6 (필수 동반, 누락 시 program→scene 신규 사이클):** `program/program_uniforms.{h,cpp}` 의 `SetDirLight/SetPointLight/SetSpotLight` 3종 제거 → `<render>/light_uniform_dispatcher.cpp` 익명 ns 로컬 헬퍼로 이주.
  - **F1 (Uniforms family 분리 — 명문화):** 이 이주는 `SJH::Uniforms` family(architecture.md §11.1)를 *쪼갠다* — 광원 setter 3종만 render 로, 나머지(`SetVec*/SetFloat`)는 program 잔존. 검증(grounded): object-타입 인자 setter는 정확히 이 3개뿐(`program_uniforms.h:121/128/137`; `DirLight` 등 :68-70 전방선언), 나머지는 vmath 인자 → `program→object` 엣지 원천이 정확히 이 3개. 사이클상 불가피 + light setter↔light dispatcher 동거로 응집 향상. **문서후속(§ Out of scope): architecture.md §11.1 에 "family 가 dependency layer 로 분리됨" 노트 추가.**
- **include 갱신:** `"object/light.h"`→`"scene/light.h"` — scene_context.{h,cpp}, compound_actor.cpp, <render>/scene_renderer.cpp, <render>/light_uniform_dispatcher.cpp, `apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp`, `test/test_light.cpp`.
- **CMake:** object → `SJH::scene` PUBLIC **제거** + 소스에서 `light.cpp` 제거 / scene 소스에 `light.cpp` 추가.
- **수용:** 빌드 GREEN. E1 절단 (object→scene 엣지 + program→object 엣지 동시 소멸).

### Task 1.2 — E4: RenderTarget → buffer
- **이동:** `render/render_target.{h,cpp}` → `buffer/render_target.{h,cpp}` (`RenderTarget`+`DefaultRenderTarget`, 가드/ns 불변).
- **include 갱신:** `"src/buffer/render_target.h"`→`"buffer/render_target.h"` — <buffer>/framebuffer.h, render/device_context.h, <render>/screen_quad_stage.cpp, <render>/scene_renderer.cpp, `apps/_MyApp_/main.cpp`, `apps/_MyApp_/src/VFX/ParticleStage.cpp`.
- **CMake:** buffer → `SJH::render` PUBLIC **제거** + 소스에 `render_target.cpp` 추가 / render → 소스에서 `render_target.cpp` 제거 + `SJH::buffer` PRIVATE→**PUBLIC 승격**.
- **수용:** 빌드 GREEN. E4 절단 (buffer→render 엣지 소멸).

---

## 슬라이스 2 — 묶음 ③ (E2) · spec §3  ✅ 완료 (빌드 GREEN·미커밋 2026-06-19)

> 결과: `_MyApp_` GREEN. **E2 절단** = `grep '#include "render/"' src/scene`=0 + scene CMake `SJH::render` link 제거. 신규 `render/actor_factory.{h,cpp}`(Skybox/ScreenCamera 이주), `scene/model_spawner.{h,cpp}`→`render/`. `compound_actor.{h,cpp}` 에서 render/buffer/object include 5종 + 2 팩토리 제거(Camera/Light 만 잔존). render CMake `SJH::scene` PRIVATE→PUBLIC. 클라 갱신: main.cpp(actor_factory.h, 미사용 compound_actor.h 제거)·WorldSceneBuilder.cpp(actor_factory.h 추가)·StageBuilder.cpp(model_spawner 경로). object/model.h 의 ModelSpawner 언급은 doc 주석뿐(코드 의존 0, 무변경).

### Task 2.1 — scene 의 render-결합 팩토리 → render
- **신설:** `render/actor_factory.{h,cpp}` ← `CreateSkyboxActor` + `CreateScreenCameraActor` 이주 (ns `SJH::Scene` 불변, D7).
- **이동:** `scene/model_spawner.{h,cpp}` → `render/model_spawner.{h,cpp}` (ns `SJH::Scene::ModelSpawner` 불변).
- **정리:** `scene/compound_actor.{h,cpp}` 에서 `render/mesh_renderer.h`·`material/material.h`·`<buffer>/framebuffer.h`·`object/mesh.h` include 제거. (잔존: `CreateCameraActor` + Light 팩토리 3종 = render 무관.)
- **include/호출처 갱신:** `apps/_MyApp_/main.cpp`, `apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp`, `<Stage>/StageBuilder.cpp` — `"apps/_MyApp_/src/Bootstrap/model_spawner.h"`→`"apps/_MyApp_/src/Bootstrap/model_spawner.h"` + Skybox/ScreenCamera 호출 파일에 `"apps/_MyApp_/src/Bootstrap/actor_factory.h"` 추가.
- **CMake:** scene → `SJH::render` PRIVATE **제거** + 소스에서 `model_spawner.cpp` 제거 / render → 소스에 `actor_factory.cpp`+`model_spawner.cpp` 추가 + `SJH::scene` PRIVATE→**PUBLIC 승격**(`unique_ptr<Actor>` 반환).
- **수용:** 빌드 GREEN. E2 절단 (scene→render 엣지 소멸).

---

## 슬라이스 3 — 묶음 ① (E3 + E5 + E6, 최대) · spec §4  ✅ 완료 (빌드 GREEN·미커밋 2026-06-19)

> 결과: `_MyApp_` GREEN(115/115). **E3/E5/E6 절단** = buffer/object/sprite -> resource_registry include 전부 0 + texture->rr 0. 신규 18번째 모듈 `src/texture/`(texture/image 이주, stb 단일 owner=image.cpp 보존). rr 캐시 파사드화(texture/image 소스 제거, SJH::texture/sprite/buffer PUBLIC 명시화). **D8 SpriteRenderer DI**: ctor `(UniformAtlas*, Mesh*, Material*)` 주입형, lazy ensure 3종 -> 신규 `resource_registry/sprite_resources.{h,cpp}`(ns SpriteResources). sprite->rr 0. 생성 3 사이트(text_renderer.cpp / PlayerHand.cpp / SpriteLayerFactory.cpp) SpriteResources 해결 후 주입. include 전수 swap(texture/texture.h·texture/image.h, ~16파일). texture.cpp 의 불필요 `resource_registry.h` include -> 자기헤더(texture→rr 사이클 방지).
>
> ⚠ **잔존 발견 — `render -> rr -> sprite -> render` 3-사이클**(사전 존재, 본 작업 회귀 아님). 세 엣지(render→rr=scene_renderer/render_pipeline 캐시 사용 · rr→sprite=resource_registry.h 의 uniform_atlas + sprite_resources · sprite→render=SpriteRenderer:MeshRenderer) 모두 slice 3 이전부터 존재. spec 의 "3-cycle 은 2-cycle 절단으로 동반 소멸" 가정이 이 사이클엔 미적용(절단된 mutual 엣지 sprite→rr 이 아니라 sprite→render 경유). **6 mutual→0 은 달성**했으나 *완전 DAG* 는 미달 — 별도 결정 필요(render↛rr 역전 / rr↛sprite / sprite_resources 거처 재고 중 택1). 메모리 `dependency_cycles_survey.md` 의 "ModuleDeps mutual 3개 과소집계" 와 일치.
> ⚠ ENABLE_TESTING configure 는 사전 breakage(test_smoke 부재)로 막혀 test include 정합 검증 못 함(slice 1 과 동일).

### Task 3.1 — 신규 `src/texture/` 모듈 (18번째)
- **이동:** `resource_registry/{texture,image}.{h,cpp}` → `src/texture/` (ns `SJH` / 가드 불변). **stb 단일 owner 불변식 보존** — `STB_IMAGE_IMPLEMENTATION` 은 이동한 `texture/image.cpp` 한 곳.
- **신설 CMake:** `texture/CMakeLists.txt` (`sjhopengl_texture` STATIC + `SJH::texture` ALIAS, `PUBLIC SJH::common project_deps`). 구 `${Stb_INCLUDE_DIR}` no-op 변수 폐기. `src/CMakeLists.txt` 에 `add_subdirectory(texture)` + 우산 합류(17→18).
- **F5 (§7 체크리스트 준수):** architecture.md §2 canonical template + §7 체크리스트 — 헤더가드 `__SJH_TEXTURE_H__`/`__SJH_IMAGE_H__`, `CLASS_PTR(Texture)`/`CLASS_PTR(Image)`, 팩토리(`Texture::CreateTexture`/`Image::Load`) 전부 **이동만**(신규 작성 아님 → 기존 준수 그대로). PUBLIC/PRIVATE = 헤더 노출 여부 일치. build-verify.
- **수용:** texture 모듈 단독 빌드 OK.

### Task 3.2 — resource_registry 캐시 파사드화
- CMake 소스에서 `texture.cpp`/`image.cpp` 제거 + `SJH::texture` **PUBLIC** link 추가. **`game_deps` PUBLIC link 불변** (FMOD 가드 보존).

### Task 3.3 — E6 완전 절단: SpriteRenderer DI (D8)
- `SpriteRenderer` 생성자 → `explicit SpriteRenderer(UniformAtlas*=nullptr, Mesh*=nullptr, Material*=nullptr)` (자가해결 폐기, base `MeshRenderer(mesh,material)` 전달).
- lazy ensure 3종 → 신설 `resource_registry/sprite_resources.{h,cpp}` 자유 함수(ns `SJH::SpriteResources`, 키 `_sprite_*` 불변).
- sprite 모듈 → `resource_registry/resource_registry.h` include 제거 (**rr 의존 0**).
- **생성 지점 3곳 본문 변경:** `apps/_MyApp_/src/Playable/SpriteLayerFactory.cpp` · `apps/_MyApp_/src/Entity/Player/PlayerHand.cpp:108` (병렬 편집 주의) · **`src/text/text_renderer.cpp:79`** (적대 검증 발견 누락분 — 미갱신 시 빌드 GREEN 인데 월드 텍스트 소멸). 각각 `SpriteResources` 로 Mesh/Material 해결 후 `AddComponent<SpriteRenderer>(atlas,mesh,mat)`.
- **F2 (§11.2 dangling 불변식 — 명문화):** 주입되는 `Mesh*`/`Material*` 는 **반드시 `SpriteResources`(rr 세션수명 캐시) 산출분**이어야 함 — rr 이 세션 내내 살아 비소유 관찰자가 dangling 안 됨(architecture.md §11.2). transient/스택 Mesh·Material 주입 금지. `SpriteRenderer` 는 base `MeshRenderer` 의 *비소유* `Mesh*`/`Material*` 계약을 그대로 승계 (소유권 이전 아님). 미래 호출자도 이 규약 준수.
- **수용:** 빌드 GREEN + 스프라이트/월드텍스트 GUI 육안(동작-영향 유일 변경).

### Task 3.4 — consumer link 교체 + include 전수 갱신 (절단점)
- CMake: buffer `rr→SJH::texture` / object `SJH::texture PUBLIC 추가` / sprite `rr 제거→SJH::texture` / rr `SJH::sprite+SJH::buffer PUBLIC 추가` / render `SJH::texture PRIVATE 추가` / material·text 무변경.
- include 전수: `"src/texture/texture.h"`→`"texture/texture.h"`, `image.h` 동일 — src + apps(~12) + test (grep 전수 후 일괄).
- **수용:** `_MyApp_` 빌드 GREEN + `-DENABLE_TESTING=ON` configure 로 test include 정합 확인. E3·E5·E6 절단.

---

## 검증 · 가드레일 (spec §5 와 동일)

- 빌드: `cmake --preset ninja` → `cmake --build --preset ninja --target _MyApp_` 에러 0. 수신자는 컴파일 검증만, 실행/GUI 는 사용자.
- 사이클 절단 증명: 각 슬라이스 후 `grep -rn '#include "<역방향모듈>/"'` 0.
- 커밋: 사용자 path-scoped 게이트, `git add -A` 금지, **`Co-Authored-By` 미사용**.
- 병렬 편집: 슬라이스 직전 `git status`. **선행 전제** — 미커밋 `SFX_*→SHADER_PROPERTIE_*` 리네임(constants.h+program_uniforms.cpp `M`)은 기대 상태(사라져 있으면 중단·보고).
- 컨벤션: 한국어 Doxygen(이동 파일 거주지 서술 갱신, 특수문자 0), `__SJH_*_H__` 가드, Tab indent, sb7code 불가침. no_auto_tests.

## Decision Log (요약 — 상세/대안은 spec §1)

| ID | 결정 | 채택 |
|----|------|------|
| D1 | Texture/Image 위치 | 신규 `src/texture/` (gpu_resource·buffer흡수 기각) |
| D2 | RenderTarget 거처 | buffer 이동 |
| D3 | Light 컴포넌트 거처 | `scene/light.{h,cpp}` (render안·신규모듈 기각) |
| D4 | render-팩토리 거처 | render(`actor_factory`+`model_spawner`) (scene_builder 기각) |
| D5 | 슬라이스 순서 | ②→③→① |
| D6 | Light uniform setter | `light_uniform_dispatcher.cpp` 로컬 이주 (program→scene 사이클 방지) |
| D7 | 네임스페이스 | 불변(`SJH::`/`SJH::Scene::`) |
| D8 | E6 잔존 레그 | SpriteRenderer DI 완전 절단 (헤더-사이클만 끊는 안 기각) |

## Out of scope
- 클라이언트 C1~C4 = 별도 `2026-06-11-client-cycle-c1-c4-plan`.
- 문서 후속(mainpage 그래프 정정 6→0, CLAUDE.md 18모듈) = 코드 GREEN 후 별도 슬라이스(spec §6).
