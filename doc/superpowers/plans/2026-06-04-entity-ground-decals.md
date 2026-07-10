# Entity Ground Decals Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Player/Enemy 발밑에 그림자 + 피격범위 원 두 장의 XZ 바닥 평면(MeshRenderer)을 렌더링한다.

**Architecture:** 엔티티 root 아래 depth+1 — Enemy는 `renderActor`(스프라이트+워블) / `groundActor`(데칼) 분리로 워블과 데칼 Transform을 독립시킨다. Player는 root 워블이 없어 `groundActor`만 추가. 데칼 부착 코드는 별도 헬퍼 파일 없이 각 빌더 파일의 익명 네임스페이스 file-local 함수로 작성한다.

**Tech Stack:** SJH OpenGL 엔진 (`SJH::Mesh::CreatePlane` / `SJH::Scene::MeshRenderer` / `SJH::Material` Transparent Pass / `ResourceRegistry`), Box2D fixture 반경 조회, `simple_texture.vs/.fs` 멀티플라이 셰이더.

**규약 (프로젝트 메모리):**
- 단위 테스트 자동 작성 금지(`no_auto_tests`) — 검증 = 빌드 GREEN + 셰이더 런타임 컴파일 + 육안.
- 커밋은 **partial path** (`git commit <경로>`) — 사용자 병렬 staging 보호. 인덱스 전체 커밋 금지.
- `Co-Authored-By` 트레일러 미사용.
- 빌드는 사용자가 직접 수행할 수 있음 — 빌드 명령은 명시하되 실패 시 사용자에 확인.

**참조 사실 (spec `doc/superpowers/specs/2026-06-04-entity-ground-decals-design.md`):**
- `MeshRenderer(SJH::Mesh*, SJH::Material*, int queueOffset=0)` — `src/render/mesh_renderer.h:43`.
- `CreatePlane()` = 1×1 XY quad(원점 중심, z=0, normal +Z, UV 0..1) → `EulerRot[0]=-90` 으로 XZ 바닥에 눕힘.
- `ResourceRegistry::Create*` 는 "이미 있으면 nullptr" → **Find 먼저, 없으면 Create**.
- `Properties.Textures["uTex"] = { tex, 0 }` / `Properties.Vec4s["baseColor"] = vmath::vec4(...)`.
- `ForEachSpriteRenderer(root)` 는 root + **직속 자식** 을 훑음 → renderActor가 root 직속자식이면 hit/dissolve 도달, `AttachEntityPresentation` 무수정.

---

## File Structure

| 파일 | 변경 | 책임 |
|---|---|---|
| `apps/_MyApp_/resources/shaders/simple_texture.vs` | 수정 | `out` 3줄 추가 (컴파일 버그 수정) |
| `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` | 수정 | file-local `AttachGroundDecals` + `BuildPlayer` 에서 호출 |
| `apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp` | 수정 | `renderActor` 분리 + file-local `AttachGroundDecals` + 호출 |

> `AttachGroundDecals` 헬퍼는 **두 파일에 동일하게 인라인 복제** 한다(사용자 확정 — 공유 `.h/.cpp` 미생성). 두 파일 모두 동일 `ResourceRegistry` 키를 쓰므로 자원 자체는 1회만 생성된다.

---

## Task 1: simple_texture.vs 컴파일 버그 수정

**Files:**
- Modify: `apps/_MyApp_/resources/shaders/simple_texture.vs:6-13`

- [ ] **Step 1: VS에 빠진 `out` 선언 3줄 추가**

`apps/_MyApp_/resources/shaders/simple_texture.vs` 의 `aTexCoord` 입력 선언과 `uModel` uniform 선언 사이(현재 8행과 10행 사이)에 아래를 추가:

```glsl
layout(location = 2) in vec2 aTexCoord;

out vec3 vsPosition;
out vec3 vsNormal;
out vec2 vsTexCoord;

uniform mat4 uModel;
```

> FS(`simple_texture.fs`)는 무수정 — 이미 `in vec3 vsNormal; in vec3 vsPosition; in vec2 vsTexCoord;` 를 받고 있다.

- [ ] **Step 2: 빌드 + 셰이더 런타임 컴파일 확인**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 빌드 GREEN (error 0). (셰이더 컴파일은 런타임 — 다음 Task에서 program이 실제 로드될 때 확인.)

- [ ] **Step 3: 커밋**

```bash
git commit apps/_MyApp_/resources/shaders/simple_texture.vs \
  -m "[fix] : simple_texture.vs 누락된 out 선언 3줄 추가 (vsPosition/vsNormal/vsTexCoord — 컴파일 버그)"
```

---

## Task 2: PlayerBuilder — groundActor 데칼 인라인 부착

**Files:**
- Modify: `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` (include 추가 + 익명 네임스페이스 helper + `BuildPlayer` 호출 1줄)

- [ ] **Step 1: include 추가**

`PlayerBuilder.cpp` 상단 include 블록(현재 `#include "scene/scene.h"` 부근)에 추가:

```cpp
#include "object/mesh.h"               // SJH::Mesh::CreatePlane
#include "render/mesh_renderer.h"      // SJH::Scene::MeshRenderer
#include "material/material.h"         // SJH::Material
#include "material/pass.h"             // SJH::Pass::Kind::Transparent
#include "src/texture/image.h"  // SJH::Image::Load
#include "apps/_MyApp_/src/Physics/PhysicsComponent.h"  // FindPhysics / Physics::GetBody
#include <<box2d>/box2d.h>               // b2Shape / b2PolygonShape / b2Fixture
#include <algorithm>                   // std::max
```

- [ ] **Step 2: 익명 네임스페이스에 `AttachGroundDecals` 추가**

`PlayerBuilder.cpp` 의 기존 `namespace { ... }` (현재 `BuildPlayerDirectionalGroups` / `RegisterPlayerCombatPlayables` 가 있는 블록) **안 끝부분**에 아래 함수를 추가:

```cpp
		/// @brief [데칼] 엔티티 발밑 그림자 + 피격범위 원 — groundActor 자식 1개 아래 MeshRenderer 2장.
		/// @details depth+1: root.groundActor.{decal_shadow, decal_hitrange}. 워블/스케일과 독립 Transform.
		///          공유 자원은 find-or-create (스폰마다 호출돼도 1회 생성). 크기는 첫 fixture 반경 자동.
		void AttachGroundDecals(SJH::Scene::Actor &root)
		{
			auto &reg = SJH::ResourceRegistry::Get();

			// ── 공유 자원 (find-or-create) ──
			SJH::Program *prog = reg.FindProgram("simple_texture");
			if (!prog)
				prog = reg.CreateProgram("simple_texture",
				    "./resources/shaders/simple_texture.vs",
				    "./resources/shaders/simple_texture.fs");

			SJH::Mesh *plane = reg.FindMesh("_ground_plane");
			if (!plane)
				plane = reg.RegisterMesh("_ground_plane", SJH::Mesh::CreatePlane());

			SJH::Texture *shadowTex = reg.FindTexture("entity_shadow");
			if (!shadowTex)
				shadowTex = reg.CreateTexture("entity_shadow",
				    SJH::Image::Load("entity_shadow", "resources/texture/EntityShadow.png").get());

			SJH::Texture *circleTex = reg.FindTexture("hit_range_circle");
			if (!circleTex)
				circleTex = reg.CreateTexture("hit_range_circle",
				    SJH::Image::Load("hit_range_circle", "resources/texture/Circle_albedo.png").get());

			SJH::Material *shadowMat = reg.FindSharedMaterial("shadow_decal_mat");
			if (!shadowMat)
			{
				shadowMat = reg.CreateSharedMaterial("shadow_decal_mat");
				shadowMat->SetProgram(prog);
				shadowMat->SetPass(SJH::Pass::Kind::Transparent);
				shadowMat->Properties.Textures["uTex"]   = {shadowTex, 0};
				shadowMat->Properties.Vec4s["baseColor"] = vmath::vec4(1.0f, 1.0f, 1.0f, 0.5f);
			}
			SJH::Material *hitMat = reg.FindSharedMaterial("hitrange_decal_mat");
			if (!hitMat)
			{
				hitMat = reg.CreateSharedMaterial("hitrange_decal_mat");
				hitMat->SetProgram(prog);
				hitMat->SetPass(SJH::Pass::Kind::Transparent);
				hitMat->Properties.Textures["uTex"]   = {circleTex, 0};
				hitMat->Properties.Vec4s["baseColor"] = vmath::vec4(1.0f, 0.0f, 0.0f, 0.45f);
			}

			// ── 충돌 반경 (첫 fixture) ──
			float hitRadius = 0.5f;
			if (auto *phys = TopdownShooter::Physics::Components::FindPhysics(&root))
			{
				if (b2Body *body = phys->GetBody())
				{
					if (b2Fixture *fx = body->GetFixtureList())
					{
						const b2Shape *sh = fx->GetShape();
						if (sh->GetType() == b2Shape::e_circle)
							hitRadius = sh->m_radius;
						else if (sh->GetType() == b2Shape::e_polygon)
						{
							const auto *poly   = static_cast<const b2PolygonShape *>(sh);
							float        maxExt = 0.0f;
							for (int32 i = 0; i < poly->m_count; ++i)
								maxExt = std::max(maxExt, poly->m_vertices[i].Length());
							hitRadius = maxExt;
						}
					}
				}
			}
			const float hitD    = hitRadius * 2.0f;
			const float shadowD = hitRadius * 2.0f * 1.2f;

			// ── groundActor + 데칼 2장 ──
			auto *ground = root.AddChild(std::make_unique<SJH::Scene::Actor>("groundActor"));

			auto *shadow = ground->AddChild(std::make_unique<SJH::Scene::Actor>("decal_shadow"));
			shadow->AddComponent<SJH::Scene::MeshRenderer>(plane, shadowMat, /*queueOffset*/ 0);
			shadow->GetTransform().EulerRot[0] = -90.0f;                       // XY → XZ 눕힘
			shadow->GetTransform().Scale       = vmath::vec3(shadowD, 1.0f, shadowD);
			shadow->GetTransform().Translate   = vmath::vec3(0.0f, 0.02f, 0.0f); // z-fight 회피

			auto *circle = ground->AddChild(std::make_unique<SJH::Scene::Actor>("decal_hitrange"));
			circle->AddComponent<SJH::Scene::MeshRenderer>(plane, hitMat, /*queueOffset*/ 1);
			circle->GetTransform().EulerRot[0] = -90.0f;
			circle->GetTransform().Scale       = vmath::vec3(hitD, 1.0f, hitD);
			circle->GetTransform().Translate   = vmath::vec3(0.0f, 0.03f, 0.0f);
		}
```

- [ ] **Step 3: `BuildPlayer` 에서 호출**

`PlayerBuilder.cpp` 의 `BuildPlayer` 에서, VFX seam 주입 블록 끝(현재 196행 `weapon->SetOnFireFx(...)` 직후, 197행 주석 라인 부근)과 `result.SpriteActor = dir.Root().AddChild(std::move(spriteActor));` (현재 199행) **사이**에 추가:

```cpp
		// 발밑 그림자 + 피격범위 원 (groundActor 자식). std::move 전 = pre-entry.
		AttachGroundDecals(*spriteActor);
```

> `spriteActor` 는 아직 `std::move` 되기 전(199행) 이라 유효. Player physics는 `BoxBody`(size 1×1) → e_polygon → hitRadius ≈ 0.707(대각).

- [ ] **Step 4: 빌드**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 빌드 GREEN (error 0).

- [ ] **Step 5: 육안 확인 (실행)**

Run: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`
Expected: 플레이어 발밑에 반투명 그림자 + 반투명 적색 원. 로그에 셰이더 컴파일 에러 없음 (`simple_texture` program 정상 로드 — Task 1 검증 겸).

- [ ] **Step 6: 커밋**

```bash
git commit apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp \
  -m "[feat] : Player 발밑 그림자+피격범위 원 데칼 (groundActor 자식, simple_texture Transparent, fixture 반경 자동)"
```

---

## Task 3: EnemyBuilder — renderActor 분리 (sprite + 워블)

**Files:**
- Modify: `apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp:37-83`

- [ ] **Step 1: sprite 부착을 renderActor 자식으로 이전**

`EnemyBuilder.cpp` 의 현재 37–41행:

```cpp
		// 2) ENEMY_FRONT[variant] 스프라이트 + 2프레임 애니 — owner-direct(child 없음, 단일 레이어).
		//    공유 헬퍼로 atlas+SpriteRenderer(+애니) 부착. QueueOffset=DrawOrder(=0, 기본값과 동일·무해).
		const auto& tex = Playable::ENEMY_FRONT[deps.variant % 3];
		auto& reg = SJH::ResourceRegistry::Get();
		Playable::AttachSpriteLayer(*enemy, reg, tex, deps.spriteFps);
```

을 아래로 교체 (sprite를 `renderActor` 자식에 부착 — root 워블 분리):

```cpp
		// 2) renderActor(root 직속 자식) — sprite + 워블 전용. groundActor 데칼과 Transform 독립.
		//    (ForEachSpriteRenderer 가 root 직속자식을 훑으므로 hit-flash/dissolve 는 그대로 도달.)
		auto* renderActor = enemy->AddChild(std::make_unique<SJH::Scene::Actor>("renderActor"));
		const auto& tex = Playable::ENEMY_FRONT[deps.variant % 3];
		auto& reg = SJH::ResourceRegistry::Get();
		Playable::AttachSpriteLayer(*renderActor, reg, tex, deps.spriteFps);
```

- [ ] **Step 2: 워블 트윈 타깃을 renderActor로 변경**

현재 50–83행 워블 블록의 `self` / `baseScale` / `par` 부착 대상을 `enemy` → `renderActor` 로 변경. 51–52행:

```cpp
			SJH::Scene::Actor* self      = enemy.get();             // 이동 후에도 동일 heap Actor — 댕글링 없음
			const vmath::vec3  baseScale = enemy->GetTransform().Scale; // 베이스 스케일 보존 (factory 설정 존중)
```

을:

```cpp
			SJH::Scene::Actor* self      = renderActor;            // root 자식 — 주소 안정(enemy children 보유)
			const vmath::vec3  baseScale = renderActor->GetTransform().Scale; // 신규 Actor 기본 (1,1,1)
```

그리고 79행:

```cpp
			auto* par = enemy->AddComponent<SJH::Playable::ParallelPlayable>();
```

을:

```cpp
			auto* par = renderActor->AddComponent<SJH::Playable::ParallelPlayable>();
```

> 람다 `[self, baseScale](float s){ self->GetTransform().Scale = ... }` / `[self](float deg){ self->GetTransform().EulerRot[2] = deg; }` 는 `self` 만 renderActor로 바뀌면 그대로 동작. `AttachEntityPresentation(*enemy, ...)`(89–95행) / hit FX seam(98–99행)은 **무수정** (director·Life는 root 유지).

- [ ] **Step 3: 빌드**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 빌드 GREEN (error 0).

- [ ] **Step 4: 육안 확인 (회귀)**

Run: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`
Expected: 적 스프라이트가 이전과 동일하게 스케일 펄스 + z회전 워블. 피격 시 hit-flash, 사망 시 dissolve 정상(= ForEachSpriteRenderer가 renderActor 도달 확인).

- [ ] **Step 5: 커밋**

```bash
git commit apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp \
  -m "[refactor] : 적 sprite+워블을 renderActor 자식으로 분리 (depth+1 — root 워블 제거, 데칼 독립 준비)"
```

---

## Task 4: EnemyBuilder — groundActor 데칼 인라인 부착

**Files:**
- Modify: `apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp` (include 추가 + 익명 네임스페이스 helper + 호출 1줄)

- [ ] **Step 1: include 추가**

`EnemyBuilder.cpp` 상단 include 블록에 추가:

```cpp
#include "object/mesh.h"               // SJH::Mesh::CreatePlane
#include "render/mesh_renderer.h"      // SJH::Scene::MeshRenderer
#include "material/material.h"         // SJH::Material
#include "material/pass.h"             // SJH::Pass::Kind::Transparent
#include "src/texture/image.h"  // SJH::Image::Load
#include "apps/_MyApp_/src/Physics/PhysicsComponent.h"  // FindPhysics / Physics::GetBody
#include <<box2d>/box2d.h>               // b2Shape / b2PolygonShape / b2Fixture
#include <algorithm>                   // std::max
```

- [ ] **Step 2: 익명 네임스페이스 + `AttachGroundDecals` 추가 (Task 2와 동일 본문)**

`EnemyBuilder.cpp` 는 현재 익명 네임스페이스가 없으므로, `namespace TopdownShooter::Bootstrap { ... }` **내부 상단**(`SJH::Scene::Actor* BuildEnemy(...)` 정의 직전)에 익명 네임스페이스를 신설하고 아래 함수를 넣는다 (Task 2 Step 2 와 **완전히 동일한 본문** — 두 파일에 의도적 복제):

```cpp
	namespace
	{
		/// @brief [데칼] 엔티티 발밑 그림자 + 피격범위 원 — groundActor 자식 1개 아래 MeshRenderer 2장.
		/// @details depth+1: root.groundActor.{decal_shadow, decal_hitrange}. 워블/스케일과 독립 Transform.
		///          공유 자원은 find-or-create (스폰마다 호출돼도 1회 생성). 크기는 첫 fixture 반경 자동.
		void AttachGroundDecals(SJH::Scene::Actor &root)
		{
			auto &reg = SJH::ResourceRegistry::Get();

			SJH::Program *prog = reg.FindProgram("simple_texture");
			if (!prog)
				prog = reg.CreateProgram("simple_texture",
				    "./resources/shaders/simple_texture.vs",
				    "./resources/shaders/simple_texture.fs");

			SJH::Mesh *plane = reg.FindMesh("_ground_plane");
			if (!plane)
				plane = reg.RegisterMesh("_ground_plane", SJH::Mesh::CreatePlane());

			SJH::Texture *shadowTex = reg.FindTexture("entity_shadow");
			if (!shadowTex)
				shadowTex = reg.CreateTexture("entity_shadow",
				    SJH::Image::Load("entity_shadow", "resources/texture/EntityShadow.png").get());

			SJH::Texture *circleTex = reg.FindTexture("hit_range_circle");
			if (!circleTex)
				circleTex = reg.CreateTexture("hit_range_circle",
				    SJH::Image::Load("hit_range_circle", "resources/texture/Circle_albedo.png").get());

			SJH::Material *shadowMat = reg.FindSharedMaterial("shadow_decal_mat");
			if (!shadowMat)
			{
				shadowMat = reg.CreateSharedMaterial("shadow_decal_mat");
				shadowMat->SetProgram(prog);
				shadowMat->SetPass(SJH::Pass::Kind::Transparent);
				shadowMat->Properties.Textures["uTex"]   = {shadowTex, 0};
				shadowMat->Properties.Vec4s["baseColor"] = vmath::vec4(1.0f, 1.0f, 1.0f, 0.5f);
			}
			SJH::Material *hitMat = reg.FindSharedMaterial("hitrange_decal_mat");
			if (!hitMat)
			{
				hitMat = reg.CreateSharedMaterial("hitrange_decal_mat");
				hitMat->SetProgram(prog);
				hitMat->SetPass(SJH::Pass::Kind::Transparent);
				hitMat->Properties.Textures["uTex"]   = {circleTex, 0};
				hitMat->Properties.Vec4s["baseColor"] = vmath::vec4(1.0f, 0.0f, 0.0f, 0.45f);
			}

			float hitRadius = 0.5f;
			if (auto *phys = TopdownShooter::Physics::Components::FindPhysics(&root))
			{
				if (b2Body *body = phys->GetBody())
				{
					if (b2Fixture *fx = body->GetFixtureList())
					{
						const b2Shape *sh = fx->GetShape();
						if (sh->GetType() == b2Shape::e_circle)
							hitRadius = sh->m_radius;
						else if (sh->GetType() == b2Shape::e_polygon)
						{
							const auto *poly   = static_cast<const b2PolygonShape *>(sh);
							float        maxExt = 0.0f;
							for (int32 i = 0; i < poly->m_count; ++i)
								maxExt = std::max(maxExt, poly->m_vertices[i].Length());
							hitRadius = maxExt;
						}
					}
				}
			}
			const float hitD    = hitRadius * 2.0f;
			const float shadowD = hitRadius * 2.0f * 1.2f;

			auto *ground = root.AddChild(std::make_unique<SJH::Scene::Actor>("groundActor"));

			auto *shadow = ground->AddChild(std::make_unique<SJH::Scene::Actor>("decal_shadow"));
			shadow->AddComponent<SJH::Scene::MeshRenderer>(plane, shadowMat, /*queueOffset*/ 0);
			shadow->GetTransform().EulerRot[0] = -90.0f;
			shadow->GetTransform().Scale       = vmath::vec3(shadowD, 1.0f, shadowD);
			shadow->GetTransform().Translate   = vmath::vec3(0.0f, 0.02f, 0.0f);

			auto *circle = ground->AddChild(std::make_unique<SJH::Scene::Actor>("decal_hitrange"));
			circle->AddComponent<SJH::Scene::MeshRenderer>(plane, hitMat, /*queueOffset*/ 1);
			circle->GetTransform().EulerRot[0] = -90.0f;
			circle->GetTransform().Scale       = vmath::vec3(hitD, 1.0f, hitD);
			circle->GetTransform().Translate   = vmath::vec3(0.0f, 0.03f, 0.0f);
		}
	} // namespace
```

- [ ] **Step 3: `BuildEnemy` 에서 호출**

`EnemyBuilder.cpp` 의 hit FX seam 블록(현재 97–99행 `life->SetOnHitFx(...)`) 직후, 씬 트리 부착(현재 101–103행 `if (!deps.spawnParent) return nullptr;`) **직전**에 추가:

```cpp
		// 발밑 그림자 + 피격범위 원 (groundActor 자식, renderActor 와 형제). AddChild 전 = pre-entry.
		AttachGroundDecals(*enemy);
```

> Enemy physics는 `CircleBody(ENEMY_RADIUS)` → e_circle → hitRadius = ENEMY_RADIUS.

- [ ] **Step 4: 빌드**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 빌드 GREEN (error 0).

- [ ] **Step 5: 육안 확인 (핵심 — 독립성)**

Run: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`
Expected:
- 적 발밑에 반투명 그림자 + 충돌반경(ENEMY_RADIUS)에 맞는 반투명 적색 원.
- 적이 워블(스케일 펄스 + z회전)해도 **그림자·원은 회전·스케일하지 않음** (독립 Transform 확인 = 본 설계의 핵심 목표).
- 적 이동 시 데칼이 따라옴(translate 상속).

- [ ] **Step 6: 커밋**

```bash
git commit apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp \
  -m "[feat] : 적 발밑 그림자+피격범위 원 데칼 (groundActor 자식, 워블 독립 — fixture 반경 자동)"
```

---

## Self-Review (작성자 체크 완료)

**1. Spec coverage:**
- §2 셰이더 버그 → Task 1 ✅
- §3 계층 depth+1 (Enemy renderActor / groundActor) → Task 3 + 4 ✅
- §3 Player groundActor → Task 2 ✅
- §4 인라인 작성 (헬퍼 파일 없음) → Task 2/4 익명 네임스페이스 복제 ✅
- §4.1 find-or-create 공유 자원 → AttachGroundDecals 본문 ✅
- §4.4 fixture 반경 자동 → AttachGroundDecals circle/polygon 분기 ✅
- §6 Transparent + QueueOffset(그림자0<원1) + y오프셋 → AttachGroundDecals ✅
- §5 AttachEntityPresentation 무수정 → Task 3 Step 2 명시 ✅

**2. Placeholder scan:** 모든 step에 실제 코드/명령 포함. TBD/TODO 없음. (박스 반경 대각 기준은 Step 5 육안에서 조정 가능 — 기본값 명시됨.)

**3. Type consistency:** `AttachGroundDecals(SJH::Scene::Actor&)` 시그니처 Task 2/4 동일. `MeshRenderer(Mesh*, Material*, int)` 시그니처 일치. `Properties.Textures`/`Properties.Vec4s`/`SetPass`/`SetProgram`/`FindSharedMaterial`/`CreateSharedMaterial` 명칭 spec과 일치. `b2Shape::e_circle`/`e_polygon`, `b2PolygonShape::m_count`/`m_vertices`/`b2Vec2::Length` Box2D v2.4.1 일치.

**위험 노트:** include 순서 — `EnemyBuilder.cpp`/`PlayerBuilder.cpp` 모두 최상단이 `#include <GL/gl3w.h>` 여야 함(기존 유지). 신규 include는 그 아래 블록에 추가.

---

# Round 2 — Player 데칼 spin 해결 + renderActor/aimPivot 분리 + Y offset Constants

**Goal:** Player 데칼이 커서 따라 회전(spin)하는 문제를 root 비회전 + aimPivot 회전이전으로 해결하고, 스프라이트를 renderActor로 묶고, 데칼 Y offset을 Constants로 빼서 직접 튜닝 가능하게 한다.

**근거:** spec `doc/superpowers/specs/2026-06-04-entity-ground-decals-design.md` **Round 2** 섹션 (R2.1~R2.7).

**Round 2 핵심 사실:**
- 방향 스프라이트·손은 `billboard_atlas` 빌보드라 모델 X/Y회전 무시(Z롤만). **데칼만 `simple_texture`라 root Y회전을 그대로 받아 spin.**
- root `EulerRot[1]=mAimAngleY`의 유일 소비자 = 손 궤도(WorldMatrix 상속). 무기/raycast/movement는 root 회전 미참조.
- `ForEachSpriteRenderer`(SpriteFxPlayable.cpp)는 root+직속자식 1단계만 → 스프라이트가 renderActor 손자가 되면 hit-flash/dissolve 미도달 → **재귀화 필요**.
- Transform: `world = parent_world * local`, `local = T*Rz*Ry*Rx*S` (EulerRot degrees). 자식이 부모 Y회전 상속.

**Task 순서 안전성:** Task 5~8은 개별적으로 **동작 보존(현행과 동일)** — Task 9가 renderActor/aimPivot를 배선해야 비로소 활성. 각 Task GREEN 독립 검증.

---

## Task 5: ForEachSpriteRenderer 서브트리 재귀화

**Files:**
- Modify: `apps/_MyApp_/src/Playable/SpriteFxPlayable.cpp` (익명 namespace의 `ForEachSpriteRenderer` 템플릿)

- [ ] **Step 1: 1단계 훑기를 서브트리 DFS 재귀로 교체**

현재 (직속 자식 1단계):
```cpp
		template <typename Fn>
		void ForEachSpriteRenderer(SJH::Scene::Actor *root, Fn &&fn)
		{
			if (!root) return;
			if (auto *r = root->GetComponent<SJH::Sprite::SpriteRenderer>()) fn(r);
			for (const auto &child : root->GetChildren())
				if (auto *r = child->GetComponent<SJH::Sprite::SpriteRenderer>()) fn(r);
		}
```
를 서브트리 전체 재귀로 교체:
```cpp
		// 대상 액터 + 서브트리 전체(손자 이하 포함)의 SpriteRenderer 에 fn 적용.
		// (renderActor 하위로 내려간 32 스프라이트 + aimPivot 하위 손까지 도달 — Round 2.
		//  영향 집합은 현행과 동일: groundActor 데칼은 SpriteRenderer 없어 자동 skip.)
		template <typename Fn>
		void ForEachSpriteRenderer(SJH::Scene::Actor *root, Fn &&fn)
		{
			if (!root) return;
			if (auto *r = root->GetComponent<SJH::Sprite::SpriteRenderer>()) fn(r);
			for (const auto &child : root->GetChildren())
				ForEachSpriteRenderer(child.get(), fn); // 손자 이하까지 재귀
		}
```

> `child` 는 `const std::unique_ptr<Actor>&` → `child.get()` 로 `Actor*` 전달. `fn` 은 참조로 재사용(forward 안 함).

- [ ] **Step 2: 빌드**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: build GREEN (error 0). (현행 트리에선 동작 보존 — 현재도 root 직속 32스프라이트+손 전부 적용 중, 재귀해도 동일 집합.)

- [ ] **Step 3: 커밋**
```bash
git commit apps/_MyApp_/src/Playable/SpriteFxPlayable.cpp \
  -m "[refactor] : ForEachSpriteRenderer 서브트리 재귀화 (renderActor 하위 스프라이트 hit-flash/dissolve 도달 — Round 2 준비)"
```

---

## Task 6: 데칼 Y offset Constants + AttachGroundDecals baseY 파라미터

**Files:**
- Modify: `apps/_MyApp_/src/Bootstrap/Constants.h` (상수 3개 추가)
- Modify: `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` (helper 시그니처 + 호출 + include)
- Modify: `apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp` (helper 시그니처 + 호출)

- [ ] **Step 1: Constants.h 에 데칼 Y 상수 추가**

`apps/_MyApp_/src/Bootstrap/Constants.h` 의 `namespace TopdownShooter::Bootstrap { ... }` 안(기존 `ENEMY_ROT_WOBBLE_MS` 아래)에 추가:
```cpp
	// 발밑 데칼 Y offset (바닥 z-fight 회피 + 엔티티별 높이 튜닝). 직접 조작용 하드코딩.
	constexpr float PLAYER_DECAL_Y       = 0.02f;
	constexpr float ENEMY_DECAL_Y        = 0.02f;
	constexpr float DECAL_CIRCLE_Y_DELTA = 0.01f; // 피격원이 그림자보다 약간 위 (queueOffset 과 함께 분리)
```

- [ ] **Step 2: 두 빌더의 AttachGroundDecals 시그니처 + Translate 변경 (동일 유지)**

PlayerBuilder.cpp / EnemyBuilder.cpp 두 helper 모두 시그니처를:
```cpp
		void AttachGroundDecals(SJH::Scene::Actor &root)
```
→
```cpp
		void AttachGroundDecals(SJH::Scene::Actor &root, float baseY)
```
그리고 두 데칼의 Translate 하드코딩을 baseY 기반으로 (양 파일 동일):
```cpp
			shadow->GetTransform().Translate   = vmath::vec3(0.0f, baseY, 0.0f);                        // (구 0.02f)
```
```cpp
			circle->GetTransform().Translate   = vmath::vec3(0.0f, baseY + DECAL_CIRCLE_Y_DELTA, 0.0f); // (구 0.03f)
```
> `DECAL_CIRCLE_Y_DELTA` 는 `TopdownShooter::Bootstrap` 네임스페이스 — helper가 그 안의 익명 네임스페이스라 unqualified 해석됨.
> 변경 후 두 helper가 byte-identical 유지되도록 동일하게 적용.

- [ ] **Step 3: 호출 사이트 + PlayerBuilder include**

PlayerBuilder.cpp 상단 include 블록에 추가 (EnemyBuilder.cpp 는 이미 포함):
```cpp
#include "apps/_MyApp_/src/Bootstrap/Constants.h"  // PLAYER_DECAL_Y / DECAL_CIRCLE_Y_DELTA
```
PlayerBuilder.cpp 의 호출 (현재 `AttachGroundDecals(*spriteActor);`):
```cpp
		AttachGroundDecals(*spriteActor, PLAYER_DECAL_Y);
```
EnemyBuilder.cpp 의 호출 (현재 `AttachGroundDecals(*enemy);`):
```cpp
		AttachGroundDecals(*enemy, ENEMY_DECAL_Y);
```

- [ ] **Step 4: helper 동일성 + 빌드**

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics && diff <(awk '/void AttachGroundDecals/,/^\t\t}$/' apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp) <(awk '/void AttachGroundDecals/,/^\t\t}$/' apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp)
```
Expected: EMPTY (두 helper 동일). 이후:
```bash
cmake --build --preset ninja --target _MyApp_
```
Expected: GREEN. (값 0.02/0.03 동일 → 동작 보존.)

- [ ] **Step 5: 커밋 (세 파일 한 커밋, partial path)**
```bash
git commit apps/_MyApp_/src/Bootstrap/Constants.h apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp \
  -m "[refactor] : 데칼 Y offset Constants 이전 (PLAYER/ENEMY_DECAL_Y + CIRCLE_DELTA, AttachGroundDecals baseY 파라미터)"
```

---

## Task 7: PlayerController SetFacingPivot 주입

**Files:**
- Modify: `apps/_MyApp_/src/InputHandler/PlayerController.h`
- Modify: `apps/_MyApp_/src/InputHandler/PlayerController.cpp`

- [ ] **Step 1: 헤더 — setter 선언 + 멤버**

`PlayerController.h` public 영역(예: `SetDamageCallback` 선언 아래)에 추가:
```cpp
		/// @brief facing 회전(EulerRot[1]=aimAngleY)을 적용할 pivot 주입 (선택적, 멱등).
		///        미주입이면 owner(root) 회전(하위호환). 데칼 spin 분리용 — root는 비회전,
		///        aimPivot만 회전시켜 손 궤도는 aimPivot 상속으로 유지.
		PlayerController &SetFacingPivot(SJH::Scene::Actor *pivot);
```
private 영역(예: `mCamera` 멤버 아래)에 추가:
```cpp
		SJH::Scene::Actor *mFacingPivot = nullptr; // facing 회전 대상 (미주입 시 owner) — 데칼 spin 분리
```
> `scene/actor.h` 가 이미 include 되어 `SJH::Scene::Actor` 완전형 가용.

- [ ] **Step 2: cpp — setter 구현 + Update 회전 대상 변경**

`PlayerController.cpp` 의 다른 setter 군 옆(예: `SetWorldCamera` 아래)에 구현:
```cpp
	PlayerController &PlayerController::SetFacingPivot(SJH::Scene::Actor *pivot)
	{
		// 멱등 — 첫 비-null 주입 후 무시.
		if (mFacingPivot == nullptr)
			mFacingPivot = pivot;
		return *this;
	}
```
`Update` 의 회전 적용부 (현재):
```cpp
		SJH::Scene::Actor *owner = GetOwner();
		if (owner != nullptr)
			owner->GetTransform().EulerRot[1] = mAimAngleY; // 논리 facing — 자식 손이 WorldMatrix 로 상속
```
를 pivot 우선으로 교체:
```cpp
		SJH::Scene::Actor *owner = GetOwner();
		if (owner != nullptr)
		{
			// facing 회전은 pivot(주입 시)에만 적용 — root는 비회전(데칼 spin 분리).
			// 미주입이면 owner(하위호환). aimPivot 하위 손이 WorldMatrix 로 회전 상속.
			SJH::Scene::Actor *pivot = (mFacingPivot != nullptr) ? mFacingPivot : owner;
			pivot->GetTransform().EulerRot[1] = mAimAngleY;
		}
```

- [ ] **Step 3: 빌드**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: GREEN. (pivot 미주입 → owner 회전 = 현행 동작, dormant.)

- [ ] **Step 4: 커밋**
```bash
git commit apps/_MyApp_/src/InputHandler/PlayerController.h apps/_MyApp_/src/InputHandler/PlayerController.cpp \
  -m "[feat] : PlayerController SetFacingPivot 주입 — facing 회전을 root 대신 pivot에 (미주입 시 owner fallback)"
```

---

## Task 8: PlayerHands orbit parent 주입

**Files:**
- Modify: `apps/_MyApp_/src/Entity/Player/PlayerHand.h`
- Modify: `apps/_MyApp_/src/Entity/Player/PlayerHand.cpp`

- [ ] **Step 1: 헤더 — ctor 주입 + 멤버**

`PlayerHand.h` 의 `class PlayerHands` public 영역 맨 위(예: `OnEnter` 선언 위)에 ctor 추가:
```cpp
		/// @brief 손 child 액터를 부착할 orbit parent 주입 (선택적). 미주입이면 owner(root).
		///        aimPivot 주입 시 손이 root 대신 aimPivot 회전을 상속(데칼 spin 분리 후에도 궤도 유지).
		explicit PlayerHands(SJH::Scene::Actor *orbitParent = nullptr) : mOrbitParent(orbitParent) {}
```
private 영역(예: `mLeft` 위)에 멤버 추가:
```cpp
		SJH::Scene::Actor *mOrbitParent = nullptr; // 손 child 부착 대상 (미주입 시 owner)
```
> `scene/actor.h` 가 이미 include 되어 `SJH::Scene::Actor` 가용.

- [ ] **Step 2: cpp — OnEnter 손 부착 대상 변경**

`PlayerHand.cpp` 의 `PlayerHands::OnEnter` 에서, `auto makeHand = ...` 람다 정의 **직전**에 부착 대상 결정 라인 추가:
```cpp
		// 손 child 는 orbit parent(주입 시 aimPivot)에 부착 — 미주입이면 owner(root).
		SJH::Scene::Actor *handParent = (mOrbitParent != nullptr) ? mOrbitParent : owner;
```
그리고 람다 안의 손 액터 생성 `owner->AddChild(...)` 를 `handParent->AddChild(...)` 로 변경:
```cpp
			SJH::Scene::Actor *hand =
			    handParent->AddChild(std::make_unique<SJH::Scene::Actor>(name));
```
> `PlayerHands` 컴포넌트 자체는 root에 그대로 — `Update` 의 `owner->GetComponent<PlayerController>()` 와 `TriggerFire` 의 controller→hands 조회 모두 무수정. atlas/reg 도 무수정.

- [ ] **Step 3: 빌드**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: GREEN. (orbitParent 미주입 → owner 부착 = 현행 동작, dormant.)

- [ ] **Step 4: 커밋**
```bash
git commit apps/_MyApp_/src/Entity/Player/PlayerHand.h apps/_MyApp_/src/Entity/Player/PlayerHand.cpp \
  -m "[feat] : PlayerHands orbit parent 주입 — 손 child를 aimPivot 아래 부착 (미주입 시 owner fallback)"
```

---

## Task 9: PlayerBuilder 배선 — renderActor/aimPivot 생성 + 주입 (활성화)

**Files:**
- Modify: `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` (`BuildPlayer`)

- [ ] **Step 1: renderActor + aimPivot 생성**

`CreatePlayerActor` 직후, `spriteActor->GetTransform().Scale = vmath::vec3(1.0f, 1.0f, 1.0f);` 줄 **아래**에 삽입:
```cpp
		// depth+1: renderActor(방향 스프라이트) + aimPivot(조준 회전·손 궤도) — root는 비회전(데칼 spin 분리).
		// 둘 다 std::move 후에도 주소 안정(spriteActor children 보유).
		auto *renderActor = spriteActor->AddChild(std::make_unique<SJH::Scene::Actor>("renderActor"));
		auto *aimPivot    = spriteActor->AddChild(std::make_unique<SJH::Scene::Actor>("aimPivot"));
```

- [ ] **Step 2: 방향 그룹 대상 → renderActor**

```cpp
		BuildPlayerDirectionalGroups(*spriteActor, *director);
```
→
```cpp
		BuildPlayerDirectionalGroups(*renderActor, *director);
```

- [ ] **Step 3: controller 에 aimPivot 주입**

controller 블록(현재 `SetFireCallback`/`SetDamageCallback`)에 한 줄 추가:
```cpp
		if (auto *controller = spriteActor->GetComponent<Controller::PlayerController>())
		{
			controller->SetFireCallback([director] { director->Play("fire"); });
			controller->SetDamageCallback([director] { director->ReactDamaged(0); });
			controller->SetFacingPivot(aimPivot); // root 대신 aimPivot 회전 (데칼 spin 분리)
		}
```

- [ ] **Step 4: PlayerHands 에 aimPivot 주입**

```cpp
		result.SpriteActor->AddComponent<TopdownShooter::Entity::PlayerHands>();
```
→
```cpp
		result.SpriteActor->AddComponent<TopdownShooter::Entity::PlayerHands>(aimPivot);
```
> `aimPivot` 은 `std::move(spriteActor)` 후에도 유효(heap, 이미 entered). PlayerHands::OnEnter 가 손 child를 aimPivot 아래 생성.

- [ ] **Step 5: 빌드**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: GREEN.

- [ ] **Step 6: 육안 검증 (사람)**

Run: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`
체크: ①플레이어 그림자·피격원이 커서 회전 시 **spin 안 함** ②손은 여전히 조준 방향으로 궤도 ③방향 스프라이트 8방향 facing 정상 ④피격 hit-flash/사망 dissolve 정상(스프라이트 전부) ⑤적 데칼 Y가 `ENEMY_DECAL_Y` 로 조절됨.

- [ ] **Step 7: 커밋**
```bash
git commit apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp \
  -m "[feat] : Player renderActor/aimPivot 분리 — root 비회전(데칼 spin 해결), 손은 aimPivot 궤도, 스프라이트는 renderActor"
```

---

## Round 2 Self-Review

**Spec(R2) coverage:**
- R2.3② 재귀 ForEachSpriteRenderer → Task 5 ✅
- R2.4 Y Constants + baseY param → Task 6 ✅
- R2.4 SetFacingPivot → Task 7 ✅
- R2.3① 손 child 재부모(컴포넌트 root 유지) → Task 8 ✅
- R2.2 renderActor/aimPivot 계층 + 배선 → Task 9 ✅

**Type consistency:** `SetFacingPivot(SJH::Scene::Actor*)` / `PlayerHands(SJH::Scene::Actor*)` / `AttachGroundDecals(Actor&, float)` 시그니처 Task 7/8/6·9 일치. 상수명 `PLAYER_DECAL_Y`/`ENEMY_DECAL_Y`/`DECAL_CIRCLE_Y_DELTA` Task 6·9 일치.

**위험 노트:**
- Task 9 후 root 가 비회전 — `owner->GetTransform().EulerRot[1]` 다른 소비자 0 확인했으나 구현 시 `grep -rn "EulerRot\[1\]" apps/_MyApp_` 재확인 권장.
- renderActor/aimPivot 는 기본 Transform(identity) — 스프라이트 world 위치 불변. aimPivot 만 매 프레임 EulerRot[1] 갱신.
- 두 `AttachGroundDecals` helper byte-identical 불변식 유지(Task 6 diff 검증).
