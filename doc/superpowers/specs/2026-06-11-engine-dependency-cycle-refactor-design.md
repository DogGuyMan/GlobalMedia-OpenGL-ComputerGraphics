# 엔진 의존 사이클 해소 (E1~E6) — 설계 spec

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> 2026-06-11 · branch `game/main` · 근거: `doc/handoffs/2026-06-11/2026-06-11-dependency-cycle-refactor-handoff.md` (사이클 10건 라이브 코드 전건 재검증 완료)
> 범위: **엔진 묶음해소 ①+②+③ (E1~E6 전부)**. 클라이언트 묶음해소 ④(C1~C4)는 사용자 병렬 편집 중이라 **명시적으로 제외**.

---

## 0. 목표 상태

- 엔진 mutual 사이클 6 → **0**. 7-노드 SCC `{buffer, object, playable, render, resource_registry, scene, sprite}` 해체.
- 모듈 의존 전부 단방향 (높은 응집 · 낮은 결합):

```
texture(신규 18번째) ← buffer ← object ← scene ← render ← resource_registry(캐시 파사드)
                         ↑                  ↑
                       sprite ──────────────┘ (sprite → texture, render → sprite 등 기존 단방향 유지)
```

- 3-cycle 7개는 별도 수정 없이 2-cycle 절단으로 **동반 소멸** (핸드오프 §1C 판정).

## 1. 확정 결정 (사용자 합의 2026-06-11)

| # | 결정 | 선택 |
|---|------|------|
| D1 | Texture/Image 추출 위치 | **신규 `src/texture/` 모듈 (`SJH::texture`)** — gpu_resource 통합안·buffer 흡수안 기각 |
| D2 | RenderTarget 거처 (E4) | **buffer 로 이동** — 주 구현 Framebuffer + DefaultRenderTarget 과 동거 |
| D3 | Light 컴포넌트 거처 (E1) | **`scene/light.{h,cpp}` 신설** — Camera 컴포넌트와 동일 거주 패턴. render 안·신규 lighting 모듈 기각 |
| D4 | render-결합 팩토리 거처 (E2) | **render 모듈** (`render/actor_factory.{h,cpp}` + `render/model_spawner.{h,cpp}`) — 신규 scene_builder 모듈 기각 (YAGNI) |
| D5 | 슬라이스 순서 | **② → ③ → ①** (작은 것부터, 각 슬라이스 빌드 GREEN 후 보고) |
| D6 | Light uniform setter 거처 (파생) | `Uniforms::SetDirLight/SetPointLight/SetSpotLight` 3종을 `<render>/light_uniform_dispatcher.cpp` **파일-로컬 헬퍼로 이주** — 유일 호출처(검증: dispatcher 단독). 이거 없이는 `program → scene → object → material → program` **신규 4-사이클** 발생 |
| D7 | 네임스페이스 정책 | 이동 클래스의 네임스페이스 **불변** (`SJH::`, `SJH::Scene::`) — MeshRenderer 가 "render 파일 + Scene 네임스페이스" 기존 선례. 호출처는 include 경로만 변경 |
| D8 | E6 잔존 레그 처리 (파생) | **DI 완전 절단** — `SpriteRenderer` 의 lazy registry 자가해결을 폐기하고 `(UniformAtlas*, Mesh*, Material*)` 주입으로 전환. ensure 로직은 rr 신설 `sprite_resources.{h,cpp}` 로 이주 (rr → sprite/object/material/program 전부 기존 단방향). 헤더-사이클만 끊는 B+ 안 기각 — mutual 완전 0 목표 |

## 2. 슬라이스 1 — 묶음 ② (E1 + E4)

### 2.1 E1: DirLight/PointLight/SpotLight → scene

근거: 세 클래스는 `Scene::Component` 파생 + `OnEnter/OnExit` 가 scene 의 Director(`SceneContext`)를 호출 — 몸이 이미 scene 쪽. `Light` POD + `GetAttenuationCoeff`(static free) 는 순수 데이터/함수라 object 잔존.

**파일 이동**
- `src/object/light.h` 의 `DirLight`/`PointLight`/`SpotLight` 클래스 → 신설 `src/scene/light.h` (`__SJH_SCENE_LIGHT_H__` 가드, 파일 헤더 Doxygen 주석의 거주지 서술 갱신)
- `src/scene/light.cpp` 의 대응 구현 (`GetWorldDirection`/`GetWorldPosition`/`OnEnter`/`OnExit`) → 신설 `src/scene/light.cpp`
- `src/object/light.h` 잔존: `Light` POD + `GetAttenuationCoeff`. `scene/actor.h` include 제거 (절단점) — `vmath.h` 만 잔존
- `src/scene/light.cpp` 는 잔존 구현이 없으면 삭제 + object CMake 소스 목록에서 제거

**D6 동반 이동 (program → object 엣지 소멸)**
- `src/program/program_uniforms.h` 의 `SetDirLight/SetPointLight/SetSpotLight` 선언 3종 + forward decl 3종 제거
- `src/program/program_uniforms.cpp` 의 구현 3종 + `#include "object/light.h"` 제거
- `<src>/render/light_uniform_dispatcher.cpp` 의 익명 네임스페이스에 동일 구현 이주 (`Uniforms::SetVec3/SetFloat` 는 program 공개 함수라 그대로 호출). `GetAttenuationCoeff` 는 `object/light.h` static free — render 가 include (render → object 기존 PRIVATE 방향과 일치)

**include 갱신 (`"object/light.h"` → `"scene/light.h"`)**
- `src/scene/scene_context.h` / `scene_context.cpp` (모듈 내부화)
- `src/scene/compound_actor.cpp`
- `<src>/render/scene_renderer.cpp`, `<src>/render/light_uniform_dispatcher.cpp` (+ `.h` 의 forward decl 은 그대로 유효)
- `apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp` (1줄)
- `test/test_light.cpp` (+ `test/CMakeLists.txt` 의 test_light link 가 `SJH::object` 단독이면 `SJH::scene` 추가)

**CMake**
- `src/object/CMakeLists.txt`: `SJH::scene` PUBLIC link **제거** (절단점), 소스 목록에서 `light.cpp` 제거
- `src/scene/CMakeLists.txt`: 소스에 `light.cpp` 추가
- `src/program/CMakeLists.txt`: 변동 없음 (include 제거뿐)

### 2.2 E4: RenderTarget → buffer

**파일 이동**
- `src/buffer/render_target.h` → `src/buffer/render_target.h` (`RenderTarget` 인터페이스 + `DefaultRenderTarget`, 가드 유지)
- `src/buffer/render_target.cpp` → `src/buffer/render_target.cpp` (vtable home TU 주석 포함)

**include 갱신 (`"src/buffer/render_target.h"` → `"buffer/render_target.h"`)**
- `<src>/buffer/framebuffer.h`
- `src/render/device_context.h`, `<src>/render/screen_quad_stage.cpp`, `<src>/render/scene_renderer.cpp`
- `apps/_MyApp_/main.cpp`, `apps/_MyApp_/src/VFX/ParticleStage.cpp` (각 1줄)

**CMake**
- `src/buffer/CMakeLists.txt`: `SJH::render` PUBLIC link **제거** (절단점), 소스에 `render_target.cpp` 추가. (`SJH::resource_registry` link 는 슬라이스 3 에서 `SJH::texture` 로 교체 — 본 슬라이스에서는 유지)
- `src/render/CMakeLists.txt`: 소스 목록에서 `render_target.cpp` 제거. `SJH::buffer` link 를 PRIVATE → **PUBLIC 승격** (`device_context.h` 가 공개 헤더에서 `buffer/render_target.h` include — RenderTarget 노출)

**검증:** `cmake --preset ninja` + `cmake --build --preset ninja --target _MyApp_` 에러 0 → 보고 (커밋 없음).

## 3. 슬라이스 2 — 묶음 ③ (E2)

scene → render 역의존을 만드는 것은 `CreateSkyboxActor`(MeshRenderer 조립) / `CreateScreenCameraActor`(Framebuffer\* 인자) / `ModelSpawner`(MeshRenderer 조립) 셋뿐. `CreateCameraActor` + Light 팩토리 3종은 render 무관이라 scene 잔존.

**파일 이동/신설**
- 신설 `src/render/actor_factory.{h,cpp}` — `CreateSkyboxActor` + `CreateScreenCameraActor` 이주 (네임스페이스 `SJH::Scene` 유지, D7)
- `src/scene/model_spawner.{h,cpp}` → `src/render/model_spawner.{h,cpp}` (네임스페이스 `SJH::Scene::ModelSpawner` 유지)
- `src/scene/compound_actor.{h,cpp}` 잔존분에서 `render/mesh_renderer.h` · `material/material.h` · `<buffer>/framebuffer.h` · `object/mesh.h` include 제거

**include/호출처 갱신**
- `apps/_MyApp_/main.cpp`, `apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp`, `<apps>/_MyApp_/src/Stage/StageBuilder.cpp` — `"apps/_MyApp_/src/Bootstrap/model_spawner.h"` → `"apps/_MyApp_/src/Bootstrap/model_spawner.h"`, Skybox/ScreenCamera 팩토리 호출 파일에 `"apps/_MyApp_/src/Bootstrap/actor_factory.h"` 추가

**CMake**
- `src/scene/CMakeLists.txt`: `SJH::render` PRIVATE link **제거** (절단점), 소스 목록에서 `model_spawner.cpp` 제거
- `src/render/CMakeLists.txt`: 소스에 `actor_factory.cpp` + `model_spawner.cpp` 추가. 필요 의존(`SJH::material`/`SJH::object`/`SJH::buffer`/`SJH::scene`) 은 전부 기존 link. 헤더는 현 `compound_actor.h` 패턴대로 forward decl 유지 (포인터 인자) — 기존 PRIVATE link 로 충분, 승격 불요. 단 scene 은 시그니처가 `unique_ptr<Actor>` 반환이라 PUBLIC (기존 render 에 scene PRIVATE → **PUBLIC 승격**)

**검증:** 슬라이스 1 과 동일.

## 4. 슬라이스 3 — 묶음 ① (E3 + E5 + E6, 최대 슬라이스)

근본 원인: leaf 타입 `Texture`/`Image` 가 상위 캐시 파사드 모듈 `resource_registry` 에 동거 → leaf 만 필요한 buffer(Framebuffer)/object(Model)/sprite(UniformAtlas)/material 이 허브 전체로 역의존.

**신규 모듈 `src/texture/` (18번째)**
- `src/resource_registry/{texture.h,texture.cpp,image.h,image.cpp}` → `src/texture/` 이동. 네임스페이스 `SJH` / 헤더가드 `__SJH_TEXTURE_H__`·`__SJH_IMAGE_H__` 불변
- **stb_image 단일 owner 불변식 보존**: `STB_IMAGE_IMPLEMENTATION` 은 이동한 `src/texture/image.cpp` 단 한 곳 (메모리 규칙 유지)
- 신설 `src/texture/CMakeLists.txt`: `sjhopengl_texture` STATIC + `SJH::texture` ALIAS. `PUBLIC SJH::common project_deps` + include dir (`src/..`, `${CMAKE_SOURCE_DIR}/include` — image.h 의 vmath.h + image.cpp 의 stb_image.h 체크인본). ※ 구 rr CMake 의 `${Stb_INCLUDE_DIR}` 는 프로젝트 어디에도 미정의인 no-op 변수 — 이관하지 않고 의도적 폐기 (검증 정정 2026-06-11)
- `src/CMakeLists.txt`: `add_subdirectory(texture)` + `SJH::engine` 우산에 `SJH::texture` 합류 (17→18 모듈)

**resource_registry 정리 (캐시 파사드로 깊어짐)**
- CMake 소스 목록에서 `texture.cpp`/`image.cpp` 제거, `SJH::texture` **PUBLIC** link 추가 (`resource_registry.h` 가 Texture 캐시 API 노출). **`game_deps` PUBLIC link 불변** (M5 Sound/Effect — FMOD 가드 보존)

**E6 완전 절단 — SpriteRenderer DI 전환 (D8)**

검증 결과 E6 은 두 레그: (a) `uniform_atlas.h → src/texture/texture.h` (texture 추출로 절단), (b) `sprite_component.cpp → resource_registry.h` **캐시 파사드 사용** (lazy ensure 3종: `_sprite_plane` Mesh / `_sprite_billboard_program`+template Material / `_sprite_inst_N` instance). (b) 처리:

- `SpriteRenderer` 생성자: `explicit SpriteRenderer(UniformAtlas* atlas = nullptr, Mesh* mesh = nullptr, Material* materialInstance = nullptr)` — 자가해결 폐기, base `MeshRenderer(mesh, material)` 로 전달만. nullptr 허용 의미(빈 렌더러, 테스트/지연 셋업) 유지
- lazy ensure 헬퍼 3종 → 신설 `src/resource_registry/sprite_resources.{h,cpp}` 자유 함수로 이주 (네임스페이스 `SJH::SpriteResources`, 키 컨벤션 `_sprite_*` 불변). rr 은 sprite/object/material/program 에 이미(또는 정당하게) 단방향 의존
- sprite 모듈: `#include "resource_registry/resource_registry.h"` 제거 → **rr 의존 0**
- **생성 지점 3곳 갱신** (실코드 변경): 클라 2곳 — `apps/_MyApp_/src/Playable/SpriteLayerFactory.cpp` (3-빌더 공유 헬퍼, 이미 rr 사용 중) + `apps/_MyApp_/src/Entity/Player/PlayerHand.cpp:108` (병렬 편집 주의) — 그리고 **엔진 1곳 — `src/text/text_renderer.cpp:79`** (월드 텍스트 글리프; 2026-06-11 적대 검증에서 발견된 누락분 — 미갱신 시 빌드 GREEN 인 채 월드 텍스트 무성 소멸). 각각 `SpriteResources` 로 Mesh/Material 해결 후 `AddComponent<SpriteRenderer>(atlas, mesh, mat)` 주입. 읽기 전용 사용처(PlayableDirector.cpp / SpriteFxPlayable.cpp / EnemyBuilder.cpp 주석)는 무변경

**consumer link 교체 (절단점)**
- `src/buffer/CMakeLists.txt`: `SJH::resource_registry` → `SJH::texture` (framebuffer.h 의 TexturePtr)
- `src/object/CMakeLists.txt`: `SJH::texture` PUBLIC **추가** (model.h/model.cpp 의 TextureUPtr — 현재 rr link 없이 transitive 로 동작 중이던 것을 명시화)
- `src/sprite/CMakeLists.txt`: `SJH::resource_registry` 제거 → `SJH::texture` PUBLIC (uniform_atlas.h) — D8 로 rr 의존 0
- `src/resource_registry/CMakeLists.txt`: `SJH::sprite` PUBLIC **추가** (resource_registry.h 의 uniform_atlas include + sprite_resources — 현재 link 누락 상태의 명시화. sprite → rr 이 사라지므로 단방향) + `SJH::buffer` PUBLIC **추가** (resource_registry.h:32 의 framebuffer.h include — 역시 현재 link 누락의 명시화. buffer → rr 이 사라지므로 단방향)
- `src/material/`: **무변경** (rr include 0 — 검증 완료)
- `src/render/CMakeLists.txt`: rr PRIVATE 유지 (render_pipeline.cpp·scene_renderer.cpp 의 캐시 사용 — render → rr 단방향 무해) + `SJH::texture` PRIVATE 추가 (property_block_setter.cpp)
- `src/text/`: bitmap_font.cpp 의 rr 사용은 단방향 — 무변경

**include 전수 갱신**
- `"src/texture/texture.h"` → `"texture/texture.h"`, `"src/texture/image.h"` → `"texture/image.h"` — src (framebuffer.h / model.h / model.cpp / uniform_atlas.h / uniform_atlas.cpp / property_block_setter.cpp / rr 내부) + apps 약 12 파일 + `<test>/test_texture.cpp` 등 test (grep 전수 후 일괄)

**검증:** `_MyApp_` 빌드 에러 0 + `cmake --preset ninja -DENABLE_TESTING=ON` configure 로 test include 정합 확인 + 스프라이트 표시는 사용자 GUI 육안 검증 (DI 전환이 유일한 동작-영향 변경).

## 5. 검증 · 가드레일 (전 슬라이스 공통)

- **빌드:** `cmake --preset ninja` → `cmake --build --preset ninja --target _MyApp_` 에러 0. 수신자는 컴파일 검증만 — 실행/GUI 검증은 사용자
- **커밋 정책:** 임의 커밋 금지 — 슬라이스별 보고만. 커밋은 사용자가 path-scoped 로 게이트. `Co-Authored-By` 미사용
- **병렬 편집:** 각 슬라이스 직전 `git status` 재확인. 클라이언트 접촉 = include 1줄 변경 4파일 (main.cpp / WorldSceneBuilder.cpp / StageBuilder.cpp / ParticleStage.cpp) + **본문 변경 3파일** (SpriteLayerFactory.cpp / PlayerHand.cpp / 엔진 text_renderer.cpp — D8, 슬라이스 3 한정. Entity/ 활성 영역이라 직전 조율 필수). **선행 전제:** working tree 의 미커밋 `SFX_*→SHADER_PROPERTIE_*` 리네임(constants.h + program_uniforms.cpp 의 M)은 기대된 상태 — 중단 사유 아님 (사라져 있으면 중단 후 보고)
- **컨벤션:** 한국어 Doxygen 주석 (이동 파일 헤더 주석의 거주지/책임 서술 갱신, 특수문자 0), `__SJH_*_H__` 가드 (`#pragma once` 금지), Tab indent, `long` 금지, sb7code 불가침
- **테스트:** 신규 단위 테스트 작성 없음 (no_auto_tests) — 기존 테스트의 include/link 정합만 유지

## 6. 문서 후속 (코드 GREEN 후 별도 슬라이스, 사용자 확인 후)

- `doxygen/pages/00-mainpage.md` ModuleDeps 그래프 — mutual 표기 정정(6→0) + texture 모듈 추가. **uncommitted 사용자 파일이라 편집 전 조율**
- `.claude/CLAUDE.md` · `.claude/architecture.md` — 모듈 수 17→18, texture 모듈 행 추가, resource_registry 서술 갱신
- `doc/handoffs/2026-06-11/2026-06-11-dependency-cycle-refactor-handoff.md` Change log — E1~E6 해소 기록 (C1~C4 는 미착수 잔존 명시)

## 7. 리스크

| 리스크 | 완화 |
|--------|------|
| D6 누락 시 program→scene 신규 사이클 | 슬라이스 1 에 D6 포함 (별도 아님) |
| D8 DI 전환의 동작 회귀 (스프라이트 미표시) | ensure 로직은 이주만 (키 컨벤션·생성 순서 불변). 빌드 GREEN 후 사용자 GUI 육안 검증 항목으로 명시 |
| 클라이언트 본문 변경 2파일 (SpriteLayerFactory.cpp / PlayerHand.cpp — Entity/ 활성 편집 영역) | 슬라이스 3 직전 git status + 해당 2파일 편집 중이면 사용자와 조율 후 진행. 나머지 클라이언트 접촉은 include 1줄 변경만 |
| MSVC(무-FMOD CI) 회귀 | 가드(`SJH_HAS_FMOD`) 미접촉 — rr 의 game_deps link 불변. CI 는 커밋 후 사용자 확인 |
| test/ link 누락 | 슬라이스 3 후 ENABLE_TESTING configure 검증 |

## Change log

- 2026-06-11 — 최초 작성. 결정 D1~D8 합의 (사용자 5 질의 + 파생 3). 클라이언트 묶음 ④ 제외 확정. E6 잔존 레그(핸드오프 과소분석) 발견 → D8 DI 완전 절단으로 확정.
