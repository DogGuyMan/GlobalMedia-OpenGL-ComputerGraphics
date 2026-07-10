# Refactoring Handoff — 의존 사이클 전수조사 + 해소 플레이북 (2026-06-11)

> **수신자(다른 Claude Code)에게:** 이 문서 하나로 시작할 수 있게 자족적으로 작성됨. 이 세션에서 만든
> de-noised 의존 그래프(아래 *Pointers*)를 근거로 **엔진 + 클라이언트의 모든 양방향 사이클을 전수조사**한 결과와,
> 각 사이클의 *근본 원인 + 권장 해소 전략 + 위험도 + 대상 파일*을 담았다.
> **이 핸드오프는 "설문 + 리팩토링 플레이북"이지 "지금 다 고쳐라"가 아니다.** 각 사이클 수정은 코어 모듈 또는
> 병렬 개발 중인 클라이언트를 건드리는 **고위험 변경**이므로, 착수 전 반드시 사용자와 *대상 사이클·접근법·범위*를
> 합의하라(아래 *작업 방식*). 모든 `file:line` 은 2026-06-11 라이브 코드 기준 — 착수 시 drift 재확인 필수.

---

## TL;DR + 다음 행동

- **무엇이 문제인가:** 엔진 17모듈 중 7개가, 클라이언트 14서브시스템 중 5개가 각각 **하나의 강결합 사이클 덩어리(SCC)**로 묶여 있다. 컴파일 방화벽이 무너지고(한 헤더 수정 → 7모듈 재컴파일), 단위 테스트·교체·이해가 어렵다.
- **핵심 통찰 2개로 사이클 대부분이 풀린다:**
  1. **엔진** — `Texture`/`Image` 가 *상위* 모듈 `resource_registry` 에 살아서, 그 타입을 보유하는 `buffer`·`object`·`sprite` 가 모두 역방향 의존을 만든다. → **`Texture`+`Image` 를 하위 모듈로 추출**하면 엔진 6개 중 **3개**(buffer↔rr, object↔rr, sprite↔rr)가 한 번에 끊어진다.
  2. **클라이언트** — `Entity::I*` 인터페이스(IMovable/IImpulsable/IDamageable/ILivable/IDieable/IActorPresentation)가 *구상* `Entity::BaseEntity` 와 **같은 모듈**에 살아서, 그 인터페이스를 구현/소비하는 `Physics`·`InputHandler`·`Spawns`·`Playable` 이 전부 `Entity` 로 역의존한다. → **인터페이스를 의존-제로 하위 디렉토리로 추출(DIP)** 하면 클라이언트 **4개 사이클이 한 번에** 끊어진다.
- **다음 행동(권장 첫 슬라이스):** 위 두 추출 중 **위험이 낮고 효과가 큰 쪽부터** 사용자와 합의. 보통 **클라이언트 인터페이스 추출(C-1)** 이 코어 GL 모듈을 안 건드려 더 안전한 출발점이다. 단, 클라이언트는 사용자가 **병렬 편집 중**이라 충돌 위험이 있으니(아래 *충돌 매트릭스*) 착수 직전 `git status`/최근 커밋 재확인.

---

## 작업 방식 (이 핸드오프를 어떻게 쓰나)

1. 이 문서로 사이클 전체 지형을 파악한다.
2. 사용자에게 **어느 사이클(또는 어느 "묶음 해소")부터** 할지, **접근법**(추출 위치/이름)을 확인받는다. 모듈 경계 이동은 되돌리기 어렵다 — 임의 진행 금지.
3. 합의된 슬라이스에 대해 `superpowers:brainstorming` → (필요 시) plan → 구현. 구현 후 **빌드 검증**(아래 명령) 후 **커밋하지 말고** 결과 보고. 커밋은 사용자가 한다.
4. 사이클 1개를 끊을 때마다 그래프 문서(아래 Pointers)의 해당 엣지/사이클 표기를 갱신하면 좋다.

---

## State of the world (2026-06-11 재측정)

- **branch:** `game/main`
- **최근 커밋:** `bf94351 [doc] client 모듈` · `1e5a6ca [doc] 주석 재검토` · `45d137b [doc] 주석 작성` · `8054706 [fix] doxygen workflow update` · `8174b2e [chore] 쓸모 없는 파일 제거`
- **uncommitted(이번 doc 세션 산출물):** `doxygen/pages/00-mainpage.md`(M — 엔진 ModuleDeps 그래프) · `doxygen/pages/30-client-architecture.md`(?? — 클라이언트 ClientDeps 그래프) · `extern/Catch2`(서브모듈 m, 무관)
- FMOD 옵셔널 가드 / Doxygen 툴체인 / `docs.yml` 은 **이미 커밋됨**(위 commits). 코드 리팩토링은 아직 0 — 본 문서는 *설문*까지만.

> ⚠️ **그래프 주의:** 이전 ModuleDeps 그래프(`00-mainpage.md`)는 엔진 mutual 을 **3개로 과소 집계**했다(`buffer↔render`, `object↔resource_registry`, `sprite↔resource_registry` 누락). **본 핸드오프의 6개가 정본.** 그래프 문서를 고칠 때 같이 정정할 것.

---

# PART 1 — 사이클 전수조사 (전부 인스턴스/상속 결합. static·const·constexpr·free-func 노이즈는 제외)

> 판정 규칙: `A → B` = A 가 B 의 *인스턴스를 소유/생성/보유*하거나 *비-static 멤버 호출/상속*. 양방향 모두 성립하면 **사이클**.

## 1A. 엔진 모듈 사이클 (6개 mutual)

**엔진 SCC = `{ buffer, object, playable, render, resource_registry, scene, sprite }` (7노드, 전원 상호 도달 가능).**
나머지(`material`→`program`→`shader`, `layout`, `text`, `fsm`, `timer`)는 SCC 밖(단방향 tail/source). `common`·`diagnostics`·`input` 은 노이즈(macro/static)로 제외.

| # | 사이클 | 방향 A→B 근거 (`file:line`) | 방향 B→A 근거 (`file:line`) | 근본 원인 |
|---|--------|------------------------------|------------------------------|-----------|
| E1 | **scene ↔ object** | `scene/actor.h:27` → `object/transform.h` : `Actor` 가 `Transform` 값 멤버 보유 | `object/light.h:25` → `scene/actor.h` : `DirLight`/`PointLight`/`SpotLight : public Scene::Component` (`light.h:60,96,128`) | `Component` base 가 scene 에 있고, Light 가 Component 화됨. Actor 는 Transform(object) 필요 |
| E2 | **scene ↔ render** | `scene/compound_actor.cpp:23`, `scene/model_spawner.cpp:15` → render : 팩토리가 `MeshRenderer`/Camera 단 Actor 조립 | `render/mesh_renderer.h:34`, `render/pass_component.h:27`, `render/scene_renderer.cpp:33-35` → scene : `MeshRenderer`/`PassComponent : Scene::Component`, SceneRenderer 가 컴포넌트 수집 | render 컴포넌트가 Scene::Component, 그런데 scene 팩토리가 render 컴포넌트 조립 (*.cpp* 에서만) |
| E3 | **buffer ↔ resource_registry** | `buffer/framebuffer.h:27` → `resource_registry/texture.h` : `Framebuffer` 가 `TexturePtr` color attachment 보유 | `resource_registry/resource_registry.h:32` → `buffer/framebuffer.h` : 레지스트리가 Framebuffer 캐시(`CreateFramebuffer`) | **`Texture` 가 상위 rr 모듈에 거주** |
| E4 | **buffer ↔ render** | `buffer/framebuffer.h:26` → `render/render_target.h` : `class Framebuffer : public RenderTarget` (`framebuffer.h:46`) | `render/mesh_pass_processor.h:70-71`, `render/pass_component.h:55` : render 가 `Framebuffer*` 인스턴스 사용 | **`RenderTarget` 인터페이스가 render 에 거주**, Framebuffer 가 구현 |
| E5 | **object ↔ resource_registry** | `object/model.h:24` → `resource_registry/texture.h` : `Model` 이 `TextureUPtr` 보유(`model.h:75`) | `resource_registry/resource_registry.h:37-38` → `object/mesh.h`,`object/model.h` : 레지스트리가 Mesh/Model 캐시 | **`Texture` 가 상위 rr 모듈에 거주** (E3 와 동일 뿌리) |
| E6 | **sprite ↔ resource_registry** | `sprite/uniform_atlas.h:26`, `sprite/sprite_component.cpp:25` → resource_registry : `UniformAtlas` 가 Texture/Image 사용 | `resource_registry/resource_registry.h:41` → `sprite/uniform_atlas.h` : 레지스트리가 `UniformAtlas` 캐시 | **`Texture` 가 상위 rr 모듈에 거주** (E3/E5 와 동일 뿌리) |

## 1B. 클라이언트 서브시스템 사이클 (4개 mutual — 전부 `Entity` 허브)

**클라이언트 SCC = `{ Entity, Physics, InputHandler, Spawns, Playable }` (5노드, 전부 Entity 경유 상호 도달).**
`Bootstrap`/`Stage`/`Manager`/`HUD`/`Tween`/`Algebraic` 등은 단방향(소스/싱크)이라 SCC 밖.

| # | 사이클 | Entity→X 근거 (`file:line`) | X→Entity 근거 (`file:line`) | 근본 원인 |
|---|--------|------------------------------|------------------------------|-----------|
| C1 | **Entity ↔ Physics** | `Entity/BaseEntity.h:51` : `Physics::Impulse* mImpulse` 멤버 + `PlayerActor.cpp`/`EnemyFactory.h:77` `AddComponent<Physics::Components::...>` | `Physics/PhysicsImpulse.h:63,73` `GetComponent<Entity::BaseEntity>()` + `PhysicsMovement : Entity::IMovable` | Physics 가 Entity 인터페이스 구현 + 구상 `BaseEntity` 조회 |
| C2 | **Entity ↔ InputHandler** | `Entity/Player/PlayerActor.cpp` `AddComponent<Controller::PlayerController>` + `PlayerHand.cpp:125` `GetComponent<Controller::PlayerController>()` | `InputHandler/PlayerController.h:127` `Entity::IMovable* mMovementPtr` (+ `BaseEntity*`,`IActorPresentation*` 멤버; `:82` `SetMovableTarget(Entity::IMovable*)`) | PlayerController 가 Entity 인터페이스 포인터 보유 |
| C3 | **Entity ↔ Spawns** | `Entity/Bullet/bullet_factory.h:89` `AddComponent<Spawn::Carrier::Projectile>` + `EnemyFactory.h:77` `ContactCarrier` | `Spawns/Carrier.h:80,82` `GetComponent<Entity::IDamageable/IImpulsable>` + `Carrier.h:193` `GetComponent<Entity::Components::Life>()->IsAlive()` + `Carrier.h:98` `Projectile : public Entity::IDieable` | Carrier 가 Entity 인터페이스 **+ 구상 `Components::Life`** 소비 |
| C4 | **Entity ↔ Playable** | `Entity/BaseEntity.h:50` : `Playable::PlayableDirector* mDirector` 멤버 + `mDirector->Play()` | `Playable/HpGrayscalePostFX.h:58` `Entity::ILivable* mLife` (`HpGrayscalePostFX.cpp:31`) + `Playable/PlayableDirector.h:59` `PlayableDirector : Entity::IActorPresentation` | Playable 이 Entity 인터페이스 구현/소비 |

---

## 1C. 3-depth 의존 구조 (3-cycle + depth≤3 도달) — 전수

> 2-cycle(1A/1B) 너머의 깊이. SCC 내부에서 3-hop 으로 닫히는 사이클과 허브의 depth≤3 도달 범위(정밀 계산: 2-cycle/3-cycle 열거 + Tarjan SCC + BFS 폐포).

**엔진 3-cycle (7개, 전수):**
- `buffer → object → render → buffer`
- `buffer → object → resource_registry → buffer`
- `buffer → render → resource_registry → buffer`
- `buffer → render → scene → buffer`
- `object → render → scene → object`
- `object → resource_registry → sprite → object`
- `render → resource_registry → sprite → render`

**클라이언트 3-cycle (1개):** `Entity → Physics → Spawns → Entity`.

**cross-layer:** `engine → client` 엣지 **0** → 레이어 간 사이클(길이 2든 3이든) **불가**. 모든 깊이의 사이클은 레이어 내부에 갇힘.

**depth≤3 도달 폐포 (허브 — 변경 파급 범위):**
| 허브 | d1 | d2 | d3 |
|---|---|---|---|
| `Entity` | Algebraic,InputHandler,Physics,Playable,Spawns,Tween,material,object,resource_registry,scene,sprite,timer | Audio,VFX,buffer,input,layout,playable,program,render,text | shader |
| `scene` | buffer,material,object,render | layout,program,resource_registry | shader,sprite |
| `render` | buffer,material,object,program,resource_registry,scene | layout,shader,sprite | playable |
| `resource_registry` | buffer,material,object,program,sprite | layout,playable,render,scene,shader | (없음) |

> 해석: 7개 엔진 3-cycle 은 전부 §PART 2 묶음해소 ①(Texture 추출)·②(Component/RenderTarget)·③(scene→render 팩토리)로 닫는 2-cycle 들의 *조합*이다 — 별도 수정 불요(2-cycle 을 끊으면 3-cycle 도 동반 소멸). `Entity` 가 depth 3 안에 17모듈 중 22노드(엔진+클라이언트)에 도달 = 클라이언트 변경 파급의 진앙.

---

# PART 2 — 해소 전략 (근본 원인별)

## 묶음해소 ① — 엔진 `Texture`/`Image` 하위 추출 → **E3·E5·E6 동시 해소**

- **원인:** `resource_registry` 모듈이 두 책임을 섞어 보유 — (a) 저수준 GL 타입 `Texture`/`Image`(leaf), (b) 고수준 캐시 파사드 `ResourceRegistry`(buffer/object/sprite/material/program 을 include 하는 hub). `buffer(Framebuffer)`·`object(Model)`·`sprite(UniformAtlas)` 는 (a)만 필요한데 같은 모듈이라 (b)로의 역의존이 생긴다.
- **전략:** `Texture`(`texture.{h,cpp}`) + `Image`(`image.{h,cpp}`)를 **하위 모듈로 추출** — 후보: 신규 `src/gpu_resource/`(또는 `src/texture/`), 또는 `buffer` 로 흡수(Framebuffer 와 자연스러움). 의존 방향: `buffer/object/sprite → texture(하위)`, `resource_registry → buffer/object/sprite`(캐시) — 전부 단방향.
- **대상 파일:** `src/resource_registry/{texture.h,texture.cpp,image.h,image.cpp}` 이동 + `src/resource_registry/CMakeLists.txt`, 신규 모듈 `CMakeLists.txt`, `src/CMakeLists.txt`(`SJH::engine` 우산), 그리고 `#include "resource_registry/texture.h"` → 새 경로로 바꾸는 consumer(`buffer/framebuffer.h`, `object/model.h`, `sprite/uniform_atlas.h`, 다수 client). **stb_image 단일 owner** 규칙 유지(`image.cpp` 가 유일 `STB_IMAGE_IMPLEMENTATION` — 이동해도 그 불변식 보존).
- **위험:** 中~高. include 경로가 엔진·클라이언트 광범위. `SJH::engine` 우산이 흡수하므로 우산 link consumer 는 대체로 무수정이나, 모듈 직접 link 한 곳·테스트(`test/`)는 갱신 필요. **resource_registry 의 game_deps PUBLIC link(FMOD/Effekseer)** 와 stb 정의 책임을 깨지 않게 주의.

## 묶음해소 ② — `Component` base / `RenderTarget` 하위화 → **E1·E4 해소**

- **E1(scene↔object):** `DirLight/PointLight/SpotLight : Scene::Component` 가 object→scene 을 만든다. **선택지:** (A) Light 의 *Component 파생* 클래스를 scene(또는 신규 `lighting` 상위 모듈)으로 이동하고 object 에는 순수 Light *데이터* struct 만 남긴다 → object 가 scene 비의존 leaf 화. (B) `Scene::Component`/`Actor` base 를 더 하위 `core`/`node` 모듈로 추출해 object·scene 둘 다 그 하위에 의존(`Light : core::Component`, `Actor`(scene)→`Transform`(object) 단방향 유지). **권장: (A) 가 더 surgical** (Light-Component 만 이동).
- **E4(buffer↔render):** `RenderTarget` 은 순수 인터페이스 — **하위 모듈(buffer 또는 `core`)로 이동**하면 `Framebuffer : RenderTarget` 가 render 비의존, `render→buffer`(Framebuffer 사용)만 단방향 잔존.
- **위험:** 中. 인터페이스/소수 클래스 이동이라 ①보다 좁다.

## 묶음해소 ③ — scene 의 render-팩토리 상위 이동 → **E2 해소**

- **원인:** `scene/compound_actor.cpp`·`model_spawner.cpp`(*.cpp only*)가 render 컴포넌트를 조립해 scene→render 를 만든다. scene 코어(Actor/Component)는 render 가 필요 없다.
- **전략:** 해당 *팩토리 헬퍼*(예: `CreateCameraActor`, `model_spawner`)를 render 모듈 또는 상위 "scene-builder" 레이어로 이동. `render→scene`(컴포넌트가 Scene::Component)만 단방향 잔존.
- **위험:** 中. 호출처(주로 client builder)가 새 위치를 include 하도록 갱신.

## 묶음해소 ④ — 클라이언트 `Entity::I*` 인터페이스 추출(DIP) → **C1·C2·C3·C4 동시 해소**

- **원인:** 인터페이스(`IMovable`/`IImpulsable`/`IDamageable`/`ILivable`/`IDieable`/`IActorPresentation`)가 구상 `Entity::BaseEntity`·`Components::*` 와 **같은 `Entity` 모듈**. Physics/InputHandler/Spawns/Playable 이 이 인터페이스를 구현·소비하려고 `Entity` 전체에 의존 → Entity 가 그들을 조립하면서 사이클.
- **전략:** 인터페이스를 **의존-제로 하위 디렉토리**(예: `apps/_MyApp_/src/Entity/Interfaces/` 또는 신규 `Contracts/`)로 추출. Physics/InputHandler/Spawns/Playable → *Interfaces*(Entity 아님). Entity → 그 서브시스템(단방향). **DIP 표준.**
- **⚠ 스냅(중요):** `Spawns/Carrier.h:193,199` (2곳)이 *구상* `Entity::Components::Life::IsAlive()` 를 호출 → 인터페이스만 빼면 이 두 줄 때문에 C3 가 *약하게* 잔존. 같이 인터페이스화(예: `ILivable::IsAlive()` 경유)해야 완전 해소. 착수 시 `Components::Life` 의 구상 소비처를 전수 grep 해 인터페이스로 라우팅.
- **대상 파일:** `Entity` 의 인터페이스 헤더들 이동(구현 없음 — 순수 추상이라 비교적 안전) + 각 consumer(`Physics/*`, `InputHandler/PlayerController.h`, `Spawns/Carrier.h`, `Playable/*`)의 include 경로 변경 + `apps/_MyApp_/CMakeLists.txt`(소스 목록).
- **위험:** 中. 단, 클라이언트는 **사용자 병렬 편집 중**(아래 충돌 매트릭스) — `Entity/`·`Physics/`·`Spawns/` 등 동시 작업 충돌 주의.

### 권장 우선순위
1. **④ 클라이언트 인터페이스 추출** (코어 GL 미접촉, 4사이클 동시, 인터페이스만 이동 → 위험 대비 효과 최고) — *단, 병렬편집 충돌 먼저 확인*.
2. **① Texture/Image 하위 추출** (3사이클 동시, 효과 최대지만 광범위 include) — 별도 큰 슬라이스로.
3. **②, ③** (E1/E2/E4 — 소규모 이동) 마무리.

---

# PART 3 — 가드레일 & 컨벤션 (반드시 준수)

- **빌드/검증 (사용자가 직접 빌드 선호 — 수신자는 변경 후 *컴파일 검증*만):**
  - Configure: `cmake --preset ninja`
  - 빌드: `cmake --build --preset ninja --target _MyApp_` → **에러 0** 기대
  - 무-FMOD 경로까지 보려면 개별 TU 를 `compile_commands.json` 명령에서 `-DSJH_HAS_FMOD=1` 제거 후 컴파일(이번 세션에서 쓴 기법). MSVC CI(`build-msvc.yml`)는 FMOD 없이 빌드되어야 함.
  - 문서 그래프 갱신 시: `cmake --build build_ninja --target doxygen` (Graphviz 경고 0 유지).
- **커밋 정책:** **사용자가 게이트.** 임의 커밋 금지 — 구현 후 보고만. 커밋 시 **반드시 path-scoped** (`git commit <경로>`), `git add -A`/인덱스 전체 커밋 금지(사용자 병렬 staging 을 휩쓺). **`Co-Authored-By` 트레일러 미사용**(이 프로젝트 컨벤션).
- **테스트:** **요청 시에만** 단위 테스트 작성. TDD red-green 강제 안 함. 구현/리팩토링에 집중.
- **주석/코드 컨벤션:** 주석 **한국어**, Doxygen + **ASCII/한글만**(특수문자 0 — `src/render` 스타일). 멤버 `mPascalCase` / 지역 `camelCase` / 타입·함수 `PascalCase` / bool `mIs*` / 포인터 `*Ptr`. 헤더가드 `__...__`(`#pragma once` 미사용). `.clang-format`(Microsoft, **Tab** indent, ColumnLimit=0).
- **불가침:** `extern/sb7code` 수정 금지(의존 충돌은 다른 쪽에서 해결). **FMOD 옵셔널 가드(`#ifdef SJH_HAS_FMOD`) 보존** — Audio/resource_registry 의 FMOD 호출은 가드 안에만, 헤더는 전방선언만. `entt` 추가 금지(OOP Actor+Component 선호).
- **크로스플랫폼:** `long` 금지(`int32_t`/`uint64_t`), `windows.h` 는 `#ifdef _WIN32` + `WIN32_LEAN_AND_MEAN`+`NOMINMAX`, 경로 슬래시 통일. 모듈 이동 시 macOS(clang `-Werror`) + MSVC 양쪽 깨지지 않게.
- **모듈 추가 절차:** 새 `src/<module>/` 추가 시 자체 `CMakeLists.txt` + `SJH::<module>` ALIAS + `src/CMakeLists.txt` 의 `SJH::engine` 우산에 합류(상세 `.claude/architecture.md`).

---

# PART 4 — 충돌 매트릭스 (병렬 작업 — 충돌 회피)

> 사용자는 **같은 working tree 에서 병렬로 편집/staging** 한다(특히 클라이언트 `apps/_MyApp_/src/`). 착수 직전 `git status` + 최근 커밋 재확인, 편집 중인 파일은 피하거나 사용자와 조율.

| 영역 | 위험도 | 메모 |
|------|--------|------|
| `apps/_MyApp_/src/Audio/*`, `resource_registry/{resource_registry,sound,effect}.cpp` | **높음** | FMOD 가드가 최근(이 세션) 들어감. 사용자가 Audio 후속 작업 중일 수 있음(메모리: "다음=FMOD 별도 에이전트"). 묶음해소 ①(Texture 이동)이 `resource_registry/` 를 건드리니 특히 조율. |
| `apps/_MyApp_/src/Entity/`, `Physics/`, `Spawns/`, `InputHandler/`, `Playable/` | **높음** | 묶음해소 ④ 대상이자 게임플레이 핵심 — 사용자 활발 편집 영역. 인터페이스 *이동*은 헤더 추가/경로변경 위주라 비교적 격리 가능하나, 구상 클래스 본문은 건드리지 말 것. |
| `doxygen/pages/00-mainpage.md`, `doxygen/pages/30-client-architecture.md` | 낮음 | 이번 세션 uncommitted. 사이클 끊으면 그래프 갱신 대상. |
| `src/<engine>/` (코어) | 중 | 사용자는 주로 client 편집. 코어 모듈 이동(①②③)은 충돌 적으나 광범위 include 변경. |

---

## Pointers (근거 문서)

- **엔진 모듈 그래프:** `doxygen/pages/00-mainpage.md` §"모듈 의존 그래프" (`ModuleDeps` \dot). ⚠ mutual 3개 과소 — 본 핸드오프가 정정.
- **클라이언트 그래프:** `doxygen/pages/30-client-architecture.md` (`ClientDeps` \dot + Client×Engine 행렬). 죽은 include(InputHandler 6종) 등 각주 포함 — 리팩토링 시 정리 후보.
- **아키텍처 규칙:** `.claude/architecture.md`(모듈 패턴, PUBLIC/PRIVATE, include 형식). `.claude/CLAUDE.md`(빌드/컨벤션 정본).
- de-noising 규칙(이 핸드오프의 사이클 판정 기준)은 두 그래프 페이지 상단에 명시.

## 슈퍼시드 / 하이진

- 본 문서가 **의존 사이클 관련 정본**. 이전 ModuleDeps 그래프의 "상호의존 3쌍" 표기는 본 문서로 **대체(6쌍)**. 그래프 페이지를 갱신할 때 그 표기를 본 문서 1A 와 일치시킬 것.
- 이 핸드오프는 `doc/handoff/`(git-tracked)에 위치 — 다른 머신/세션 도달 가능.

## Change log

- 2026-06-11 — 최초 작성. 엔진 6 mutual + 7-SCC, 클라이언트 4 mutual + 5-SCC 전수조사 grounding(`file:line`). 묶음해소 ①~④ + 우선순위 + 가드레일 + 충돌 매트릭스.
