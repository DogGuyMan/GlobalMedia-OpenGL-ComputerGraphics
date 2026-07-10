# Entity Accessor-facade Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **검증 정책(프로젝트 `no_auto_tests`)**: 단위테스트 자동 작성 금지. 각 Task 검증 = `cmake --build --preset ninja --target _MyApp_` **exit 0** + (해당 시) GUI 육안. TDD red-green 미적용.
> **커밋**: 사용자 승인 후 **path-scoped** (`git add <경로>` — `-A`/`.` 금지, 병렬 트랙 HealthBar/cull/Example 휩쓸지 말 것). `Co-Authored-By` 미사용.
> **정본 spec**: `doc/superpowers/specs/2026-06-03-entity-accessor-facade-design.md` (gitignored 로컬).

**Goal:** Player/Enemy 액터에 Accessor-facade 컴포넌트(`PlayerEntity`/`EnemyEntity` : `BaseEntity`)를 도입해 외부의 `GetComponent` 산재를 단일 facade로 통합하고, Physics body 생성을 컴포넌트화하며, 적 넉백(+플레이어 대시 게이트)을 `Impulse`+`SJH::Timer`로 활성화한다.

**Architecture:** `BaseEntity`(공유, Component)가 `Life`/`Physics`/`PlayableDirector`/`Impulse`를 OnEnter에서 비소유 캐시 + 도메인 인터페이스(ILivable/IDieable/IDamageable/IImpulsable) 포워딩 + `GetPhysics()/GetDirector()/IsImpulseActive()` accessor. `PlayerEntity`(얇음, +IMovable)는 `GetMovement()/GetWeapon()`+`Dash()/Attack()`. body 생성은 `BoxBody`/`CircleBody` ctor로 eager 이관. 이동자(`SimplePursueAI`/`PlayerController`)는 `IsImpulseActive()` 게이트로 자유이동 skip.

**Tech Stack:** C++17, CMake/Ninja, Box2D v2.4.1, `SJH::engine` 코어(특히 `SJH::scene` Component/Actor, `SJH::timer`), spdlog.

**브랜치 상태(작성 시 HEAD `5e0343f`)**: PlayerBehavior 분해 완료. 병렬 트랙(HealthBar=`HUD/`·`EnemyBuilder.cpp`, cull=`src/material/pass.h`, World Text). **미접근**: `main.cpp`, `HUD/*`, `EnemyBuilder.cpp`, `src/material/pass.h`, Timer 코어, 셰이더. **구현 전 `git log --oneline -5` + `git status --short` 재측정**.

---

## File Structure (decomposition)

| 파일 | 책임 | 본 플랜 |
|---|---|---|
| `apps/_MyApp_/src/Physics/PhysicsComponent.h` | b2Body 보유 abstract base + `BodyConfig`/`MakeBody`/owner 등록 | 수정 (Part A) |
| `apps/_MyApp_/src/Physics/PhysicsComponent.Imp.h` | `BoxBody`/`CircleBody` shape ctor (eager body 생성) | 수정 (Part A) |
| `apps/_MyApp_/src/Physics/PhysicsImpulse.h` | `Impulse` — `SJH::Timer` 기반 active/cooldown | 수정 (Part D') |
| `Entity/BaseEntity.{h,cpp}` | 공유 facade — 4캐시 + 인터페이스 포워딩 + accessor | 수정 .h / 신규 구현 .cpp (Part B) |
| `Entity/Player/PlayerEntity.{h,cpp}` | 플레이어 facade (얇음) | 재작성 .h / 신규 .cpp (Part C) |
| `apps/_MyApp_/src/Entity/Enemy/EnemyEntity.h` | 적 facade (베이스만) | 신규 (Part D) |
| `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp` | body 생성 컴포넌트화 + PlayerEntity 부착 | 수정 (Part A/C) |
| `apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h` | body 컴포넌트화 + EnemyEntity/Impulse 부착 | 수정 (Part A/D/D') |
| `apps/_MyApp_/src/Entity/Enemy/SimplePursueAI.cpp` | facade IsAlive + 넉백 suppress | 수정 (Part D') |
| `<Entity>/Enemy/EnemyDeathHandler.cpp` | facade IsAlive 정리 | 수정 (Part D') |
| `InputHandler/PlayerController.{h,cpp}` | 대시 게이트(dormant) | 수정 (Part D') |
| `Entity/CMakeLists.txt`, `Playable/CMakeLists.txt` | 소스 추가 + 순환 회피 | 수정 (Part B) |

---

## Part A — Physics body 생성 Component화

> 최위험·선행. 인라인 외부 body 생성을 컴포넌트 ctor(eager)로. 회귀 가드: b2BodyDef/fixture 파라미터 **완전 보존**.

### Task A1: `Physics` base에 BodyConfig + MakeBody + OnEnter owner 등록

**Files:**
- Modify: `apps/_MyApp_/src/Physics/PhysicsComponent.h`

- [ ] **Step 1: `BodyConfig` 구조체 추가** — 네임스페이스 `TopdownShooter::Physics::Components` 안, `class Physics` 선언 **바로 위**에 삽입:

```cpp
	/// @brief body+fixture 생성 파라미터 (b2BodyDef + fixture 공통부). shape 는 subtype 책임.
	struct BodyConfig
	{
		b2World*    world         = nullptr;
		vmath::vec2 startPosition = vmath::vec2(0.0f, 0.0f);
		float       linearDamping = 0.0f;
		float       density       = 1.0f;
		float       friction      = 0.3f;
		bool        isSensor      = false;
		uint16_t    categoryBits  = 0;
		uint16_t    maskBits      = 0;
		float       heightOffset  = 0.0f;
	};
```

이를 위해 파일 상단 include에 `#include <vmath.h>` 가 필요(없으면 추가). `<box2d>/box2d.h`, `cstdint`는 이미 있음.

- [ ] **Step 2: `MakeBody`/`InitBody` protected 헬퍼 + `OnEnter` concrete 화** — `class Physics`의 `private:` mBody 블록과 메서드들을 아래로 교체.

기존:
```cpp
		virtual void OnEnter() = 0;
		virtual void OnExit() = 0;
		virtual void Update(float dt) = 0;
	};
```
교체:
```cpp
		// ── body 생성 (subtype ctor 가 호출, eager). owner 무관 — 등록은 OnEnter. ──
	  protected:
		/// @brief b2BodyDef → CreateBody (fixture 없음). subtype ctor 가 호출 후 shape fixture 추가.
		b2Body* MakeBody(const BodyConfig& cfg)
		{
			if (cfg.world == nullptr) return nullptr;
			b2BodyDef bd;
			bd.type          = b2_dynamicBody;
			bd.position.Set(cfg.startPosition[0], cfg.startPosition[1]);
			bd.linearDamping = cfg.linearDamping;
			return cfg.world->CreateBody(&bd);
		}
		/// @brief 생성된 body 를 멤버에 등록(owner 제외 — OnEnter 가 담당). subtype ctor 말미 호출.
		void InitBody(b2Body* body, const BodyConfig& cfg)
		{
			mBody         = body;
			mIsSensor     = cfg.isSensor;
			mHeightOffset = cfg.heightOffset;
		}

	  public:
		/// @brief owner userdata 등록 (ctor 엔 GetOwner=null → 여기서). contact 콜백은 런타임에만 읽음.
		void OnEnter() override
		{
			if (mBody != nullptr)
				mBody->GetUserData().pointer = reinterpret_cast<uintptr_t>(GetOwner());
		}
		virtual void OnExit() = 0;
		virtual void Update(float dt) = 0;
	};
```

> 주의: `OnEnter` 가 더 이상 pure 아님(concrete). `OnExit`/`Update` 는 pure 유지 → `Physics` 는 abstract 유지. 기존 `SetBody`(외부 주입 경로)는 그대로 둔다(리팩토링 후 호출처 0 이지만 호환 유지).

- [ ] **Step 3: 빌드** — `cmake --build --preset ninja --target _MyApp_`
  예상: **FAIL** — BoxBody/CircleBody 가 아직 `OnEnter() override {}` 를 갖고 있어 base concrete 와 충돌하거나(중복 정의 아님 — override 는 허용되나 무의미), 호출부(PlayerActor/EnemyFactory)가 아직 구 API. **Task A2~A4 후 GREEN**. (A1 단독은 컴파일만 확인 — 헤더만 바뀌어 즉각 에러 없을 수 있음.)

### Task A2: `BoxBody`/`CircleBody` ctor 생성 (eager) + 빈 OnEnter 제거

**Files:**
- Modify: `apps/_MyApp_/src/Physics/PhysicsComponent.Imp.h`

- [ ] **Step 1: 두 클래스를 ctor 생성 방식으로 교체** — 파일 전체 본문(`CircleBody`/`BoxBody`)을 아래로:

```cpp
	/// @brief Circle shape 물리 Component — ctor 에서 b2CircleShape body 생성(eager).
	class CircleBody : public Physics
	{
	  public:
		CircleBody(const BodyConfig& cfg, float radius)
		{
			b2Body* body = MakeBody(cfg);
			if (body != nullptr)
			{
				b2CircleShape circle;
				circle.m_radius = radius;
				b2FixtureDef fd;
				fd.shape               = &circle;
				fd.density             = cfg.density;
				fd.friction            = cfg.friction;
				fd.isSensor            = cfg.isSensor;
				fd.filter.categoryBits = cfg.categoryBits;
				fd.filter.maskBits     = cfg.maskBits;
				body->CreateFixture(&fd);
			}
			InitBody(body, cfg);
		}
		void OnExit()  override {}
		void Update(float /*dt*/) override {}
	};

	/// @brief Box shape 물리 Component — ctor 에서 b2PolygonShape::SetAsBox body 생성(eager).
	class BoxBody : public Physics
	{
	  public:
		BoxBody(const BodyConfig& cfg, vmath::vec2 size)
		{
			b2Body* body = MakeBody(cfg);
			if (body != nullptr)
			{
				b2PolygonShape box;
				box.SetAsBox(size[0] * 0.5f, size[1] * 0.5f);
				b2FixtureDef fd;
				fd.shape               = &box;
				fd.density             = cfg.density;
				fd.friction            = cfg.friction;
				fd.isSensor            = cfg.isSensor;
				fd.filter.categoryBits = cfg.categoryBits;
				fd.filter.maskBits     = cfg.maskBits;
				body->CreateFixture(&fd);
			}
			InitBody(body, cfg);
		}
		void OnExit()  override {}
		void Update(float /*dt*/) override {}
	};
```

> `OnEnter` override 제거(base concrete 상속). `OnExit`/`Update` 만 빈 override.

- [ ] **Step 2: 빌드** — `cmake --build --preset ninja --target _MyApp_`
  예상: **FAIL** — `PlayerActor.cpp`/`EnemyFactory.h` 가 아직 `AddComponent<BoxBody>()`(인자 없음) 호출 → ctor 시그니처 불일치. A3/A4 후 GREEN.

### Task A3: `PlayerActor.cpp` body 생성 호출부 전환

**Files:**
- Modify: `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp:26-47` (physics 분기 인라인 body)

- [ ] **Step 1: 인라인 body 생성(26-47행)을 BodyConfig + ctor 부착으로 교체**

기존 26-47행:
```cpp
			// Physics body 생성 — b2World 가 lifetime 소유.
			b2BodyDef bd;
			bd.type = b2_dynamicBody;
			bd.position.Set(cfg.physics.startPosition[0], cfg.physics.startPosition[1]);
			bd.linearDamping = cfg.physics.linearDamping;
			b2Body *body = cfg.physics.world->CreateBody(&bd);

			b2PolygonShape box;
			box.SetAsBox(cfg.physics.size[0] * 0.5f, cfg.physics.size[1] * 0.5f);

			b2FixtureDef fd;
			fd.shape = &box;
			fd.density = cfg.physics.density;
			fd.friction = cfg.physics.friction;
			fd.isSensor = cfg.physics.isSensor;
			fd.filter.categoryBits = cfg.physics.categoryBits;
			fd.filter.maskBits = cfg.physics.maskBits;
			body->CreateFixture(&fd);

			auto *pb = actor->AddComponent<Physics::Components::BoxBody>();
			pb->SetBody(body);
			pb->SetSensor(cfg.physics.isSensor);
```
교체:
```cpp
			// Physics body 생성은 BoxBody ctor 가 담당(eager) — b2World 가 lifetime 소유.
			Physics::Components::BodyConfig bc;
			bc.world         = cfg.physics.world;
			bc.startPosition = cfg.physics.startPosition;
			bc.linearDamping = cfg.physics.linearDamping;
			bc.density       = cfg.physics.density;
			bc.friction      = cfg.physics.friction;
			bc.isSensor      = cfg.physics.isSensor;
			bc.categoryBits  = cfg.physics.categoryBits;
			bc.maskBits      = cfg.physics.maskBits;
			auto *pb = actor->AddComponent<Physics::Components::BoxBody>(bc, cfg.physics.size);
			(void)pb; // 아래 pm/Impulse 가 OnEnter 에서 FindPhysics 로 해소
```

> `categoryBits`/`maskBits` 는 `PhysicsCfg` 가 `uint16_t`, `BodyConfig` 도 `uint16_t` — 무손실. `SetSensor` 호출 제거(ctor 가 fixture isSensor 설정 + base mIsSensor=cfg.isSensor).

- [ ] **Step 2: 빌드** — `cmake --build --preset ninja --target _MyApp_`
  예상: 여전히 **FAIL**(EnemyFactory 미전환). A4 후 GREEN.

### Task A4: `EnemyFactory.h` body 생성 호출부 전환

**Files:**
- Modify: `apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h:32-50`

- [ ] **Step 1: 인라인 body(32-50행)를 BodyConfig + ctor 로 교체**

기존 32-50행:
```cpp
        b2BodyDef bd;
        bd.type          = b2_dynamicBody;
        bd.position.Set(cfg.pos[0], cfg.pos[1]);
        bd.linearDamping = 0.5f;
        b2Body* body     = cfg.world->CreateBody(&bd);

        b2CircleShape circle;
        circle.m_radius = 0.4f;

        b2FixtureDef fd;
        fd.shape               = &circle;
        fd.density             = 1.0f;
        fd.friction            = 0.3f;
        fd.filter.categoryBits = Physics::ToBits(Physics::PhysicsLayer::Enemy);
        fd.filter.maskBits     = Physics::ToBits(Physics::EnemyMask);
        body->CreateFixture(&fd);

        auto* pb = actor->AddComponent<Physics::Components::CircleBody>();
        pb->SetBody(body);
```
교체(하드코딩 값 동일 보존):
```cpp
        Physics::Components::BodyConfig bc;
        bc.world         = cfg.world;
        bc.startPosition = cfg.pos;
        bc.linearDamping = 0.5f;
        bc.density       = 1.0f;
        bc.friction      = 0.3f;
        bc.categoryBits  = Physics::ToBits(Physics::PhysicsLayer::Enemy);
        bc.maskBits      = Physics::ToBits(Physics::EnemyMask);
        auto* pb = actor->AddComponent<Physics::Components::CircleBody>(bc, 0.4f);
```

> 이후 `actor->AddComponent<SimplePursueAI>(cfg.playerTarget, body, cfg.speed);` 의 `body` 참조가 깨짐 → 다음 step.

- [ ] **Step 2: `SimplePursueAI` 부착의 `body` 인자를 `pb->GetBody()` 로** — 같은 파일 53행:

기존:
```cpp
        actor->AddComponent<SimplePursueAI>(cfg.playerTarget, body, cfg.speed);
```
교체:
```cpp
        actor->AddComponent<SimplePursueAI>(cfg.playerTarget, pb->GetBody(), cfg.speed);
```

- [ ] **Step 3: 빌드** — `cmake --build --preset ninja --target _MyApp_`
  예상: **exit 0**, `[*/*] Linking CXX executable apps/_MyApp_/_MyApp_`. (sb7 `#warning` 1건만 허용.)

- [ ] **Step 4: GUI 회귀 검증** — `cd build_ninja/apps/_MyApp_ && ./_MyApp_`
  확인: ① WASD 이동 정상 ② 좌클릭 발사→적 명중 HP감소·총알소멸 / 벽 명중 소멸 ③ 적 접촉→플레이어 데미지 ④ 적 사살→dissolve ⑤ 크래시 0. **= body Component화 회귀 0**.

- [ ] **Step 5: 커밋 (사용자 승인 후, path-scoped)**

> ⚠ **실행 중 발견(플랜 갭)**: BoxBody/CircleBody call-site 가 Player/Enemy 외에 **wall_factory.h(static) / pickup_factory.h(static+sensor) / bullet_factory.h(kinematic+velocity)** 3곳 더 존재. `BodyConfig` 를 `bodyType`+`linearVelocity` 추가 + `friction` 기본값 0.3→0.2(box2d 기본, 미설정 site 보존)로 확장하고 5곳 전부 변환. 커밋 범위 7파일:

```bash
git add apps/_MyApp_/src/Physics/PhysicsComponent.h \
        apps/_MyApp_/src/Physics/PhysicsComponent.Imp.h \
        apps/_MyApp_/src/Entity/Player/PlayerActor.cpp \
        apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h \
        <apps>/_MyApp_/src/Stage/Factories/wall_factory.h \
        apps/_MyApp_/src/Bootstrap/pickup_factory.h \
        apps/_MyApp_/src/Entity/Bullet/bullet_factory.h
git commit -m "[refactor] : Physics body 생성 컴포넌트화 — BoxBody/CircleBody ctor(eager) + BodyConfig(static/kinematic/velocity) + OnEnter owner 등록"
```

---

## Part B — `BaseEntity` 공유 facade 구현

### Task B1: `BaseEntity.h` 확장 (4캐시 + IImpulsable + accessor)

**Files:**
- Modify: `apps/_MyApp_/src/Entity/BaseEntity.h` (전체 재작성)

- [ ] **Step 1: 파일 전체 교체**

```cpp
#ifndef _TOPDOWNSHOOTER_ENTITY_BASE__
#define _TOPDOWNSHOOTER_ENTITY_BASE__

#include "apps/_MyApp_/src/Physics/Components.Interfaces.h"   // ILivable/IDieable/IDamageable/IImpulsable
#include "scene/actor.h"
#include <string>

// fwd — 포인터 멤버만 보유(완전형은 .cpp 에서). Entity 헤더가 Playable/Physics 헤더를 안 끌어오게.
namespace TopdownShooter::Entity::Components { class Life; }
namespace TopdownShooter::Physics::Components { class Physics; }
namespace TopdownShooter::Physics { class Impulse; }
namespace TopdownShooter::Playable { class PlayableDirector; }

namespace TopdownShooter::Entity
{
	/// @brief 엔티티 공통 Accessor-facade (Player/Enemy 공유). 로직 0 — 형제 컴포넌트 비소유 캐시 + 위임/노출.
	///        = C# Sophia `Entity` 베이스(ILifeAccessible + entityRigidbody/연출 캐시).
	class BaseEntity : public SJH::Scene::Component,
	                   public ILivable, public IDieable, public IDamageable, public IImpulsable
	{
	  protected:
		Components::Life*                    mLife     = nullptr;
		Physics::Components::Physics*        mPhysics  = nullptr;   // = entityRigidbody/Collider
		Playable::PlayableDirector*          mDirector = nullptr;   // 연출+Audio (named playable)
		Physics::Impulse*                    mImpulse  = nullptr;   // Player+Enemy 부착. null-guard=비엔티티 바디 대비

	  public:
		void OnEnter() override;   // 4캐시 (.cpp — GetComponent/FindPhysics 완전형 필요)
		void OnExit()  override;
		void Update(float /*dt*/) override {}   // 로직 없음 — 형제가 자기 Update 보유

		// ── Life 위임 (ILivable/IDieable/IDamageable) ──
		bool IsAlive()  const override;
		int  GetHp()    const override;
		int  GetMaxHp() const override;
		void DoDamaged(int damage) override;   // i-frame 은 Life 내부
		void DoDie()               override;
		// ── Impulse 위임 (IImpulsable) — 넉백/대시 ──
		void DoImpulse(vmath::vec2 dir) override;
		bool IsImpulseActive() const;          // 버스트 활성 창 = 이동 suppress 게이트
		// ── accessor ──
		Physics::Components::Physics* GetPhysics()  const { return mPhysics; }
		Playable::PlayableDirector*   GetDirector() const { return mDirector; } // 포인터 반환 = fwd OK
		void Play(const std::string& key);     // .cpp (director->Play 완전형 필요)
	};
} // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_BASE__
```

> 기존 `BaseEntity(int hp, const char*)` ctor / `~BaseEntity()` 선언 제거(미구현 데드코드였음). 기본 ctor(Component) 사용.

### Task B2: `BaseEntity.cpp` 구현 (현 빈 파일 첫 구현)

**Files:**
- Modify: `apps/_MyApp_/src/Entity/BaseEntity.cpp` (0바이트 → 구현)

- [ ] **Step 1: 전체 작성**

```cpp
#include "apps/_MyApp_/src/Entity/BaseEntity.h"

#include "apps/_MyApp_/src/Entity/Components/LifeComponents.h"
#include "apps/_MyApp_/src/Physics/PhysicsComponent.h"        // Components::Physics + FindPhysics
#include "apps/_MyApp_/src/Physics/PhysicsImpulse.h"          // Impulse (IsActive)
#include "apps/_MyApp_/src/Playable/PlayableDirector.h"       // 완전형 — GetComponent/Play

namespace TopdownShooter::Entity
{
	void BaseEntity::OnEnter()
	{
		auto* owner = GetOwner();
		if (owner == nullptr) return;
		mLife     = owner->GetComponent<Components::Life>();
		mPhysics  = Physics::Components::FindPhysics(owner);
		mDirector = owner->GetComponent<Playable::PlayableDirector>();
		mImpulse  = owner->GetComponent<Physics::Impulse>();
	}

	void BaseEntity::OnExit()
	{
		mLife = nullptr; mPhysics = nullptr; mDirector = nullptr; mImpulse = nullptr;
	}

	bool BaseEntity::IsAlive()  const { return mLife && mLife->IsAlive(); }
	int  BaseEntity::GetHp()    const { return mLife ? mLife->GetHp()    : 0; }
	int  BaseEntity::GetMaxHp() const { return mLife ? mLife->GetMaxHp() : 0; }
	void BaseEntity::DoDamaged(int damage) { if (mLife) mLife->DoDamaged(damage); }
	void BaseEntity::DoDie()               { if (mLife) mLife->DoDie(); }

	void BaseEntity::DoImpulse(vmath::vec2 dir) { if (mImpulse) mImpulse->DoImpulse(dir); }
	bool BaseEntity::IsImpulseActive() const   { return mImpulse && mImpulse->IsActive(); }

	void BaseEntity::Play(const std::string& key) { if (mDirector) mDirector->Play(key); }
} // namespace TopdownShooter::Entity
```

### Task B3: CMake 순환 회피 + BaseEntity.cpp 링크

**Files:**
- Modify: `apps/_MyApp_/src/Playable/CMakeLists.txt`
- Modify: `apps/_MyApp_/src/Entity/CMakeLists.txt`

- [ ] **Step 1: `Playable/CMakeLists.txt` 에서 `MyApp::Entity` 링크 제거** — `IActorPresentation` 은 헤더-only 라 Playable 의 `..` include 경로로 해소됨(컴파일 심볼 불요). `target_link_libraries(myapp_playable ...)` 의 `MyApp::Entity` 줄 삭제:

기존:
```cmake
target_link_libraries(myapp_playable
    PUBLIC
        SJH::scene       # SJH::Scene::Component 베이스
        SJH::playable    # IPlayable / PlayableBase / Composite (map 보유 + Register 시그니처)
        SJH::material    # PostFXRegistry — SJH::Material*
        MyApp::Entity    # IActorPresentation (apps/_MyApp_/src/Physics/Components.Interfaces.h)
        game_deps        # tweeny (PostFXTweenPlayable.h 가 tweeny::tween 노출)
)
```
교체:
```cmake
target_link_libraries(myapp_playable
    PUBLIC
        SJH::scene       # SJH::Scene::Component 베이스
        SJH::playable    # IPlayable / PlayableBase / Composite (map 보유 + Register 시그니처)
        SJH::material    # PostFXRegistry — SJH::Material*
        game_deps        # tweeny (PostFXTweenPlayable.h 가 tweeny::tween 노출)
)
# IActorPresentation(Entity) 은 헤더-only — 컴파일 심볼 불요. myapp_entity 와의 링크 순환 회피 위해
# 링크 대신 include 경로만 사용(이미 위 target_include_directories 의 `..` 로 해소). [accessor-facade 2026-06-03]
```

- [ ] **Step 2: `Entity/CMakeLists.txt` 에 신규 소스 + `MyApp::Playable` 링크** — `add_library(myapp_entity STATIC ...)` 목록에 `apps/_MyApp_/src/Entity/Player/PlayerEntity.cpp` 추가(Part C에서 생성), `target_link_libraries` 의 PUBLIC에 `MyApp::Playable` 추가:

기존 `add_library`:
```cmake
add_library(myapp_entity STATIC
    BaseEntity.cpp
    apps/_MyApp_/src/Entity/Components/WeaponComponents.cpp
    apps/_MyApp_/src/Entity/Player/PlayerActor.cpp
    apps/_MyApp_/src/Entity/Player/PlayerHand.cpp
    <Bullet>/BulletContactHandler.cpp
    apps/_MyApp_/src/Entity/Enemy/SimplePursueAI.cpp
    <Enemy>/EnemyContactHandler.cpp
    <Enemy>/EnemyDeathHandler.cpp
)
```
> ⚠ 작성 시 `BulletContactHandler.cpp`/`EnemyContactHandler.cpp` 는 이미 삭제됨(`f45211c`) — **현재 파일 내용을 먼저 확인**(`git status`/Read)하고, 존재하는 소스 목록 기준으로 `apps/_MyApp_/src/Entity/Player/PlayerEntity.cpp` 를 추가한다. (병렬 트랙이 이 파일을 바꿨을 수 있음.)

`target_link_libraries(myapp_entity ...)` PUBLIC 에 추가:
```cmake
    PUBLIC
        SJH::scene
        MyApp::Algebraic
        SJH::playable
        MyApp::Playable   # <- BaseEntity.cpp 가 PlayableDirector 사용 (순환은 B3 Step1 로 회피)
```

- [ ] **Step 3: 빌드** — `cmake --build --preset ninja --target _MyApp_`
  예상: **exit 0**. (만약 Playable 이 Entity 컴파일 심볼을 실제로 참조해 링크 에러가 나면 — `undefined reference to TopdownShooter::Entity::...` — 그 심볼만 헤더-inline 인지 확인; 진짜 .cpp 심볼이면 B3 Step1 롤백하고 `--start-group` 또는 IDirectable DIP 대안(spec §10) 적용. 1차로는 제거가 안전.)

- [ ] **Step 4: 커밋 (승인 후)**

```bash
git add apps/_MyApp_/src/Entity/BaseEntity.h apps/_MyApp_/src/Entity/BaseEntity.cpp \
        apps/_MyApp_/src/Entity/CMakeLists.txt apps/_MyApp_/src/Playable/CMakeLists.txt
git commit -m "[refactor] : BaseEntity 공유 facade 구현 (Life/Physics/Director/Impulse 4캐시 + 인터페이스 포워딩) + Entity↔Playable 순환 회피"
```

---

## Part C — `PlayerEntity` (얇음)

### Task C1: `PlayerEntity.h` 재작성

**Files:**
- Modify: `apps/_MyApp_/src/Entity/Player/PlayerEntity.h` (전체 재작성)

- [ ] **Step 1: 파일 전체 교체**

```cpp
#ifndef _TOPDOWNSHOOTER_ENTITY_PLAYER__
#define _TOPDOWNSHOOTER_ENTITY_PLAYER__

#include "BaseEntity.h"
#include "apps/_MyApp_/src/Physics/Components.Interfaces.h"   // IMovable
#include "apps/_MyApp_/src/Entity/Components/WeaponComponents.h"        // Components::Weapon (UseWeapon 호출 — 완전형)
#include <vmath.h>

namespace TopdownShooter::Entity
{
	/// @brief 플레이어 Accessor-facade — BaseEntity(공유) + Movement/Weapon 접근 + Dash/Attack verb.
	class PlayerEntity : public BaseEntity, public IMovable
	{
	  protected:
		IMovable*           mMovement = nullptr;  // PhysicsMovement (인터페이스 캐시 — 물리/비물리 양분기)
		Components::Weapon* mWeapon   = nullptr;

	  public:
		void OnEnter() override;   // BaseEntity::OnEnter() + mMovement/mWeapon 캐시

		// accessor
		IMovable*           GetMovement() const { return mMovement; }
		Components::Weapon* GetWeapon()   const { return mWeapon; }

		// verb
		void DoForward(vmath::vec2 dir, float dt) override { if (mMovement) mMovement->DoForward(dir, dt); } // IMovable
		void Dash(vmath::vec2 dir)   { DoImpulse(dir); }                       // BaseEntity 기반(대시)
		void Attack(vmath::vec2 aim) { if (mWeapon) mWeapon->UseWeapon(aim); } // ranged bullet
	};
} // namespace TopdownShooter::Entity

#endif //_TOPDOWNSHOOTER_ENTITY_PLAYER__
```

> 기존 `: BaseEntity, IMovable, IAttackable` 의 **IAttackable 제거**(melee 부적합 + `Weapon::Damage` private 접근 불가). `mMovementComponentPtr`(구체 Movement) → `IMovable* mMovement`. `mKeyboard`/`mMouse`/`mPlayerControllerPtr` 멤버 제거. `GetMovemenet()`/`GetWeapon()` 등 기존 인라인 getter 는 새 시그니처로 대체.

### Task C2: `PlayerEntity.cpp` 신규 (OnEnter 캐시)

**Files:**
- Create: `apps/_MyApp_/src/Entity/Player/PlayerEntity.cpp`

- [ ] **Step 1: 작성**

```cpp
#include "apps/_MyApp_/src/Entity/Player/PlayerEntity.h"

#include "apps/_MyApp_/src/Physics/PhysicsMovement.h"   // PhysicsMovement (IMovable 구현체 — 캐시 대상)
#include "scene/actor.h"

namespace TopdownShooter::Entity
{
	void PlayerEntity::OnEnter()
	{
		BaseEntity::OnEnter();   // Life/Physics/Director/Impulse 캐시
		auto* owner = GetOwner();
		if (owner == nullptr) return;
		// ⚠ GetComponent<IMovable>(인터페이스) 금지 — PlayerEntity 자신도 IMovable 라 자기매칭 위험.
		//    구체 PhysicsMovement(typeid 매칭, fast-path)로 캐시 → 자기 자신 배제.
		//    (PlayerEntity 는 물리 분기에서만 부착되므로 PhysicsMovement 로 충분.)
		mMovement = owner->GetComponent<Physics::PhysicsMovement>();  // PhysicsMovement→IMovable 업캐스트
		mWeapon   = owner->GetComponent<Components::Weapon>();
	}
} // namespace TopdownShooter::Entity
```

### Task C3: `PlayerActor.cpp` 에 PlayerEntity 부착 + CMake

**Files:**
- Modify: `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp` (return 직전)
- (CMake `apps/_MyApp_/src/Entity/Player/PlayerEntity.cpp` 추가는 Part B Task B3 Step2 에서 이미 반영 — 미반영 시 여기서)

- [ ] **Step 1: include 추가** — `PlayerActor.cpp` 상단 include 블록에:
```cpp
#include "apps/_MyApp_/src/Entity/Player/PlayerEntity.h"
```

- [ ] **Step 2: 부착** — `return actor;` (125행) **직전**에:
```cpp
		// 모든 형제(Life/Physics/Movement/Impulse/Weapon/Controller) 부착 후 facade 부착 — OnEnter 캐시.
		actor->AddComponent<PlayerEntity>();
```

- [ ] **Step 3: 빌드** — `cmake --build --preset ninja --target _MyApp_` → exit 0.

- [ ] **Step 4: GUI 검증** — `cd build_ninja/apps/_MyApp_ && ./_MyApp_` : WASD/발사/피격 정상(PlayerEntity 부착이 기존 동작 무영향 — facade 는 캐시만).

- [ ] **Step 5: 커밋 (승인 후)**

```bash
git add apps/_MyApp_/src/Entity/Player/PlayerEntity.h apps/_MyApp_/src/Entity/Player/PlayerEntity.cpp \
        apps/_MyApp_/src/Entity/Player/PlayerActor.cpp
git commit -m "[refactor] : PlayerEntity 얇은 facade (GetMovement/GetWeapon + Dash/Attack) + PlayerActor 부착"
```

---

## Part D — `EnemyEntity`

### Task D1: `EnemyEntity.h` 신규 + EnemyFactory 부착

**Files:**
- Create: `apps/_MyApp_/src/Entity/Enemy/EnemyEntity.h`
- Modify: `apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h`

- [ ] **Step 1: `EnemyEntity.h` 작성** (header-only, 베이스만):

```cpp
#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_ENTITY_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_ENTITY_H__

#include "apps/_MyApp_/src/Entity/BaseEntity.h"

namespace TopdownShooter::Entity::Enemy
{
	/// @brief 적 Accessor-facade — BaseEntity(Life/Physics/Director/Impulse) 만으로 충분.
	///        (적 전용 능력 생기면 여기 확장.) header-only — OnEnter 는 BaseEntity 상속.
	class EnemyEntity : public TopdownShooter::Entity::BaseEntity
	{
	};
} // namespace TopdownShooter::Entity::Enemy

#endif // __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_ENTITY_H__
```

- [ ] **Step 2: `EnemyFactory.h` include + 부착** — include 블록에 `#include "apps/_MyApp_/src/Entity/Enemy/EnemyEntity.h"` 추가. `return actor;` (59행) **직전**에:
```cpp
        actor->AddComponent<EnemyEntity>();
```

- [ ] **Step 3: 빌드** — `cmake --build --preset ninja --target _MyApp_` → exit 0. (EnemyEntity 는 header-only, CMake 소스 추가 불요.)

- [ ] **Step 4: GUI 검증** — 적 스폰/추적/사망 정상(부착이 동작 무영향).

- [ ] **Step 5: 커밋 (승인 후)**

```bash
git add apps/_MyApp_/src/Entity/Enemy/EnemyEntity.h apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h
git commit -m "[refactor] : EnemyEntity facade 신설 + EnemyFactory 부착"
```

---

## Part D' — Impulse 활성화 (ii): Timer 모듈화 + 적 넉백 + 게이트

### Task E1: `PhysicsImpulse.h` — active/cooldown 을 `SJH::Timer` 로

**Files:**
- Modify: `apps/_MyApp_/src/Physics/PhysicsImpulse.h` (전체 재작성)

- [ ] **Step 1: 전체 교체** (arm-inactive 패턴 — Life 정통, 동작 0.3s/0.8s 보존):

```cpp
#ifndef __MYAPP_PHYSICS_IMPULSE_H__
#define __MYAPP_PHYSICS_IMPULSE_H__

#include "apps/_MyApp_/src/Algebraic/Stat.h"
#include "apps/_MyApp_/src/Physics/Components.Interfaces.h"
#include "apps/_MyApp_/src/Physics/PhysicsComponent.h"
#include "scene/actor.h"
#include "timer/timer.h"   // SJH::Timer::Timer (header-only)
#include <cmath>

namespace TopdownShooter::Physics
{
	/// @brief 범용 물리 속도 버스트 — Player Dash + Monster Knockback.
	/// @details 시간 로직은 SJH::Timer (active/cooldown). 이동자(Controller/AI)는 IsActive() 게이트로 자유이동 skip.
	class Impulse : public SJH::Scene::Component,
	                public Entity::IImpulsable
	{
	public:
		Impulse()
		    : mImpulseForce(7.5f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::DashForce),
		      mCooldown(0.8f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::CoolDownSpeed),
		      mActiveTimer(kDurationSec),
		      mCooldownTimer(0.8f)
		{
			// arm-inactive — 생성 직후 finished(비활성). 평소 IsActive=false / 쿨다운 해제. (Reset 으로 발동)
			mActiveTimer.Tick(kDurationSec);
			mCooldownTimer.Tick(mCooldownTimer.GetBaseTime());
		}

		void OnEnter() override { mBody = Components::FindPhysics(GetOwner()); }
		void OnExit() override { mBody = nullptr; }

		void Update(float dt) override
		{
			mActiveTimer.Tick(dt);
			mCooldownTimer.Tick(dt);
		}

		void DoImpulse(vmath::vec2 dir) override
		{
			if (!mCooldownTimer.IsTimesUp() || IsActive()) return;   // 쿨다운 중 or 이미 active → 게이트
			if (!mBody || !mBody->GetBody()) return;
			const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
			if (len <= 0.001f) return;
			vmath::vec2 n(dir[0] / len, dir[1] / len);
			const float force = mImpulseForce.GetValue();
			// XZ → Box2D XY (Z → -Y, spec §4.4)
			mBody->GetBody()->SetLinearVelocity(b2Vec2(n[0] * force, -n[1] * force));
			mActiveTimer.Reset();     // 버스트 창 발동 (0.3s)
			mCooldownTimer.Reset();   // 쿨다운 발동 (0.8s)
		}

		void ApplyKnockback(vmath::vec2 fromXZ) { DoImpulse(fromXZ); } // convenience (Monster Knockback)
		bool IsActive() const { return !mActiveTimer.IsTimesUp(); }    // 버스트 창 진행 중

	private:
		static constexpr float kDurationSec = 0.3f;   // active 창 (plain — 맞는 enum 없음)
		Algebraic::Numeric::Stat mImpulseForce;       // Stat(DashForce, base 7.5)
		Algebraic::Numeric::Stat mCooldown;           // Stat(CoolDownSpeed, base 0.8) — 문서/베이스값
		SJH::Timer::Timer        mActiveTimer;        // 0.3s 버스트 창
		SJH::Timer::Timer        mCooldownTimer;      // 0.8s 재발동 게이트
		Components::Physics*     mBody = nullptr;     // FindPhysics — 비소유
	};
} // namespace TopdownShooter::Physics

#endif // __MYAPP_PHYSICS_IMPULSE_H__
```

> `mActiveTimer.Tick(kDurationSec)` 로 passed=base → `IsTimesUp()`=true → `IsActive()`=false(평소 비활성). `DoImpulse` 의 `Reset()` 이 passed=0 → active. `cooldownTimer_` 동일(평소 `IsTimesUp()`=true → 발동 허용). 동작 0.3s/0.8s 보존. `mCooldown` Stat 은 baseTime 동기화용으로 유지하되 Timer 는 리터럴 0.8f 로 생성(Timer baseTime const).

- [ ] **Step 2: 빌드** — `cmake --build --preset ninja --target _MyApp_` → exit 0. (플레이어 Impulse 는 dash 입력 미배선이라 동작 변화 0.)

### Task E2: 적에 `Impulse` 부착

**Files:**
- Modify: `apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h`

- [ ] **Step 1: include + 부착** — include 블록에 `#include "apps/_MyApp_/src/Physics/PhysicsImpulse.h"` 추가. `actor->AddComponent<EnemyEntity>();` (D1에서 추가한 줄) **앞**에:
```cpp
        actor->AddComponent<Physics::Impulse>();   // 넉백 타겟 (Carrier::Projectile 이 DoImpulse 배달)
```
> 부착 순서: CircleBody → Life → SimplePursueAI → ContactCarrier → (EnemyDeathHandler) → **Impulse** → EnemyEntity. EnemyEntity.OnEnter 가 Impulse 캐시하므로 Impulse 가 EnemyEntity 보다 먼저 부착돼야 하나, OnEnter 는 씬 진입 시 일괄 발화라 부착 순서 무관(맵에 존재하면 됨). 안전하게 EnemyEntity 직전.

- [ ] **Step 2: 빌드** — exit 0.

### Task E3: `SimplePursueAI` — facade IsAlive + 넉백 suppress

**Files:**
- Modify: `apps/_MyApp_/src/Entity/Enemy/SimplePursueAI.h`
- Modify: `apps/_MyApp_/src/Entity/Enemy/SimplePursueAI.cpp`

- [ ] **Step 1: `.h` 에 facade 캐시 멤버 추가** — `private:` 블록에:
```cpp
        TopdownShooter::Entity::BaseEntity* mEntity = nullptr; // facade (IsAlive/IsImpulseActive)
```
그리고 `OnEnter() override {}` 를 `OnEnter() override;` 로 변경(선언만 — 정의는 .cpp). 상단에 fwd: `namespace TopdownShooter::Entity { class BaseEntity; }`.

- [ ] **Step 2: `.cpp` — include + OnEnter + Update 게이트**

include 추가(상단):
```cpp
#include "apps/_MyApp_/src/Entity/BaseEntity.h"
```

`SimplePursueAI::Update` 직전에 OnEnter 정의 추가:
```cpp
    void SimplePursueAI::OnEnter()
    {
        mEntity = GetOwner() ? GetOwner()->GetComponent<TopdownShooter::Entity::BaseEntity>() : nullptr;
    }
```

`Update` 본문 교체(기존 13-33행):
```cpp
    void SimplePursueAI::Update(float /*dt*/)
    {
        if (!mBody || !mTarget) return;

        // 사망 — 추적 정지(속도 0). facade IsAlive (기존 raw GetComponent<Life> 매프레임 호출 제거).
        if (mEntity && !mEntity->IsAlive())
        {
            mBody->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
            return;
        }

        // 넉백 중 — 추적 속도 설정 skip(버스트 보존, 0 설정 아님). 0.3s 후 자동 재개.
        if (mEntity && mEntity->IsImpulseActive())
            return;

        const auto& tp = mTarget->GetTransform().Translate;
        const b2Vec2 ep = mBody->GetPosition();
        float dx = tp[0] - ep.x;
        float dy = -tp[2] - ep.y;   // XZ→Box2D XY (Z→-Y, spec §4.4)
        const float len = std::sqrt(dx*dx + dy*dy);
        if (len < 0.01f) return;
        mBody->SetLinearVelocity(b2Vec2(dx/len * mSpeed, dy/len * mSpeed));
    }
```
> 기존 `#include "apps/_MyApp_/src/Entity/Components/LifeComponents.h"` 의 `GetComponent<Components::Life>` 사용이 사라지면 해당 include 는 제거 가능(IWYU). 단 다른 사용 없으면.

- [ ] **Step 3: 빌드** — exit 0.

### Task E4: `PlayerController` — 대시 게이트(dormant)

**Files:**
- Modify: `apps/_MyApp_/src/InputHandler/PlayerController.h`
- Modify: `apps/_MyApp_/src/InputHandler/PlayerController.cpp:177-182`

- [ ] **Step 1: `.h` 에 facade 캐시 멤버** — `private:` 블록(`mCachedSprite` 근처)에:
```cpp
		TopdownShooter::Entity::BaseEntity* mEntity = nullptr; // 대시 중 이동 suppress 게이트(IsImpulseActive)
```
상단 fwd 추가: `namespace TopdownShooter::Entity { class BaseEntity; }` (이미 `Components.Interfaces.h` include 중이지만 BaseEntity 는 별도 — fwd 로 충분).

- [ ] **Step 2: `.cpp` — include + Update 게이트** — 상단 include 에 `#include "apps/_MyApp_/src/Entity/BaseEntity.h"` 추가.

`Update`(177행~) 의 DoForward 호출(182행)을 게이트로 감싼다. 기존:
```cpp
		if (!mIsInitialized)
			return;
		// dt 는 Movement::DoForward 가 units/sec  프레임 변위로 변환 (fps-independent).
		mMovementPtr->DoForward({mInputValue[0], mInputValue[2]}, dt);
```
교체:
```cpp
		if (!mIsInitialized)
			return;

		// facade lazy 캐시 (controller 가 facade 보다 먼저 OnEnter 될 수 있어 첫 Update 에서 조회).
		if (mEntity == nullptr && GetOwner() != nullptr)
			mEntity = GetOwner()->GetComponent<TopdownShooter::Entity::BaseEntity>();

		// 대시(Impulse) 중에는 입력 자유이동을 Block — DoForward 호출 자체를 skip.
		// (입력 0 이어도 DoForward(0) 이 속도를 0 으로 만들어 버스트를 죽이므로 호출 자체를 막아야 함.)
		// 현재 dash 입력 미배선이라 IsImpulseActive()=false → 게이트 dormant(행동 변화 0).
		const bool impulseActive = (mEntity != nullptr && mEntity->IsImpulseActive());
		if (!impulseActive)
			mMovementPtr->DoForward({mInputValue[0], mInputValue[2]}, dt);
```
> ⚠ 나머지 Update(조준/회전/facing) 는 그대로 진행. `mInputValue` 리셋(215행)도 유지.

- [ ] **Step 3: 빌드** — exit 0.

### Task E5: `EnemyDeathHandler` — facade IsAlive 정리 (선택적, 일관성)

**Files:**
- Modify: `<apps>/_MyApp_/src/Entity/Enemy/EnemyDeathHandler.cpp`

- [ ] **Step 1: 현재 구현 확인** — `EnemyDeathHandler.cpp` 를 Read 해 `GetComponent<Components::Life>` self-poll 패턴을 확인. 있으면 `GetOwner()->GetComponent<TopdownShooter::Entity::BaseEntity>()->IsAlive()` (OnEnter 캐시) 로 교체. **동작 보존**(IsAlive 동일). 패턴이 다르면 이 Task skip(가드: 동작 변경 금지).

- [ ] **Step 2: 빌드** — exit 0.

### Task E6: 빌드 + GUI 검증 + 커밋

- [ ] **Step 1: 전체 빌드** — `cmake --build --preset ninja --target _MyApp_` → exit 0.

- [ ] **Step 2: GUI 검증** — `cd build_ninja/apps/_MyApp_ && ./_MyApp_`
  - ⑥ **적 총알 명중 시 잠깐 뒤로 밀린 뒤 추적 재개**(넉백 — Impulse+Timer+AI suppress 작동).
  - ⑦ **WASD 이동 평소와 동일**(플레이어 게이트 dormant — 무변화여야 정상).
  - ① 적 접촉 데미지(i-frame) / 적 사살 dissolve / 크래시 0 회귀 0.

- [ ] **Step 3: 커밋 (승인 후, path-scoped)**

```bash
git add apps/_MyApp_/src/Physics/PhysicsImpulse.h \
        apps/_MyApp_/src/Entity/Enemy/EnemyFactory.h \
        apps/_MyApp_/src/Entity/Enemy/SimplePursueAI.h apps/_MyApp_/src/Entity/Enemy/SimplePursueAI.cpp \
        apps/_MyApp_/src/InputHandler/PlayerController.h apps/_MyApp_/src/InputHandler/PlayerController.cpp \
        <apps>/_MyApp_/src/Entity/Enemy/EnemyDeathHandler.cpp
git commit -m "[feat] : 적 넉백 활성화 — Impulse SJH::Timer 모듈화 + 적 Impulse 부착 + AI/Controller IsImpulseActive 게이트(플레이어 dormant)"
```

---

## Self-Review (작성자 체크)

**Spec coverage**: §5 Part A→A1-A4 ✅ / §6 Part B→B1-B3 ✅ / §7 Part C→C1-C3 ✅ / §8 Part D→D1 ✅ / §8.5 (ii)→E1-E6 ✅ / §10 CMake 순환→B3 ✅ / §11 검증→각 Part GUI step ✅.

**Type 일관성**: `BodyConfig`(A1) → A2/A3/A4 사용 동일. `BaseEntity::IsImpulseActive`(B1/B2) → E3/E4 사용. `Impulse::IsActive`(E1) → BaseEntity.IsImpulseActive(B2) 사용. `PhysicsMovement`(C2) 구체 캐시. `GetPhysics`/`GetDirector`/`Play` 일관.

**알려진 리스크(플랜 내 가드 명시)**: ① C2 의 `GetComponent<IMovable>` 자기매칭 → Step1b 에서 구체 `PhysicsMovement` 로 교체. ② B3 CMake 순환 제거 후 링크 에러 가능 → fallback(--start-group / IDirectable) 명시. ③ EnemyFactory/Entity CMake 소스 목록이 병렬 트랙으로 바뀌었을 수 있음 → "현재 내용 먼저 확인" 가드.

---

## 구현 카덴스 (subagent-driven 권장)

Task별: 서브에이전트 구현(no-commit) → 오케스트레이터 diff 직접검증 → 빌드 exit0 + (해당 시) GUI → **사용자 승인 커밋**(path-scoped). Part A 는 최위험(물리 회귀) — 독립 GUI 검증 후 진행. 병렬 트랙(HealthBar/cull/World Text) 미접근.
