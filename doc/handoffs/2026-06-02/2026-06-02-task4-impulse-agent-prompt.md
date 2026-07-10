# 다른 Claude Code Agent 용 프롬프트 — PlayerBehavior 분해 Task 4 (Impulse 컴포넌트)

> 아래 ` ``` ` 코드블록을 새 Claude Code 세션에 그대로 붙여 사용. 자기완결.
> 정본(참고): plan `doc/superpowers/plans/2026-06-01-playerbehavior-decomposition.md` Task 4 / spec `doc/superpowers/specs/2026-06-01-playerbehavior-decomposition-design.md` §5.4.
> 이건 분해 8-task 중 **Task 4 단발** — foundation·연출·다른 task 와 무관(미배선 ship)이라 병렬 가능.

---

```
[ROLE]
너는 C++17/CMake OpenGL 탑다운 슈터(/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics, 브랜치 game/module/ingame/temp)의 구현 에이전트다.
"PlayerBehavior god-component 분해" 8-task 중 **Task 4 (Impulse 컴포넌트)** 를 구현한다. Dash/Knockback 용 일회성 물리 속도버스트 컴포넌트를 신설하고 플레이어에 부착한다. **단, 입력 바인딩은 하지 않는다(미배선 ship)** — dash 키가 아직 없으므로 컴포넌트만 존재하고 트리거는 후속(correct-by-construction).

[절대 규칙]
- 커밋/git add 금지(사용자 승인 후). 빌드 검증만. 단위테스트 자동추가 금지(no_auto_tests).
- 주석 한국어. 헤더가드 __XXX_H__ (#pragma once 금지). long 금지(고정폭). 경로 슬래시.
- 이 브랜치는 병렬 에이전트 多 — path-scoped 커밋(git add <구체경로>만), 무관 dirty(resources/vfx 등) 미접근.
- **너의 작업 파일 = 딱 2개**: ① 신규 `apps/_MyApp_/src/Physics/physics_impulse.h` ② `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp` (AddComponent 1줄). 그 외 아무것도 건드리지 마라.
- **입력 바인딩/PlayerController/Action::Dash 금지** — Impulse 는 미배선 상태로 ship. 속도싸움 suppress(controller 가 IsActive() 중 DoForward skip)도 *이번엔 하지 마라*(dash 바인딩 시 후속).
- `Entity::IImpulsable` 인터페이스는 **이미 존재**(Task 1 커밋됨, Components.Interfaces.h) — 재정의 금지, 구현만.

[중요 — 파일명 확인 (rename 진행 중)]
이 저장소는 일부 헤더가 snake_case → PascalCase 로 rename 되는 중이다(예: enemy_factory.h → EnemyFactory.h). include 전에 **실제 파일명을 확인**하라:
- `ls apps/_MyApp_/src/Physics/` 로 PhysicsComponent.h / physics_movement.h(또는 PascalCase) 실제명 확인.
- `apps/_MyApp_/src/Physics/physics_movement.h` 를 열어 그 include 들(PhysicsComponent.h, Components.Interfaces.h, Algebraic/Stat.h, scene/actor.h)의 *실제 경로* 를 그대로 복사해 써라. physics_impulse.h 는 physics_movement.h 와 같은 디렉토리·같은 의존 패턴이다.

[검증된 사실 — physics_movement.h 패턴 (Impulse 가 그대로 따름)]
- `physics_movement.h`: namespace `TopdownShooter::Physics`, `class PhysicsMovement : public SJH::Scene::Component, public Entity::IMovable`. OnEnter 에서 `mPhysicsBody = Components::FindPhysics(GetOwner());`. DoForward 에서 `mPhysicsBody->GetBody()->SetLinearVelocity(b2Vec2(n[0]*speed, -n[1]*speed));` (XZ→Box2D XY = Z→-Y). 멤버 `Algebraic::Numeric::Stat mMoveSpeed;` + `Components::Physics* mPhysicsBody=nullptr;`. **header-only**(Physics/CMakeLists 의 소스목록에 physics_movement.cpp 없음).
- `Components::FindPhysics(Actor*)` → 첫 Physics-derived(BoxBody/CircleBody) 반환(비소유). `Components::Physics::GetBody()` → b2Body*. (Components = TopdownShooter::Physics::Components.)
- Stat: `Algebraic::Numeric::Stat(float base, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::DashForce)`. `.GetValue()` → float. enum 에 `DashForce`, `CoolDownSpeed` 존재(Algebraic/Algebraic.Common.h).
- `Entity::IImpulsable`(Components.Interfaces.h): protected ctor + copy/move delete + `virtual void DoImpulse(vmath::vec2 dir) = 0;` (Task1 커밋).
- box2d(b2Vec2/b2Body)는 PhysicsComponent.h 가 include(box2d/box2d.h). Entity 모듈은 game_deps(box2d) PRIVATE link 됨. PlayerActor.cpp 는 이미 box2d + physics_movement.h 사용.
- PlayerActor.cpp `CreatePlayerActor`: physics 분기(`if (cfg.physics.world != nullptr)`) 안에 BoxBody 생성 → `auto *pb = actor->AddComponent<Physics::Components::BoxBody>();` → `auto *pm = actor->AddComponent<Physics::PhysicsMovement>(cfg.movement.speed);` 가 있다. 그 `pm` 라인 *직후* 에 Impulse 부착.

========================================================================
[STEP 1] apps/_MyApp_/src/Physics/physics_impulse.h (신규, header-only)
========================================================================
physics_movement.h 와 같은 디렉토리·의존 패턴. include 경로는 physics_movement.h 에서 그대로 복사(rename 반영).
```cpp
#ifndef __MYAPP_PHYSICS_IMPULSE_H__
#define __MYAPP_PHYSICS_IMPULSE_H__

#include "Algebraic/Stat.h"
#include "Entity/Components/Components.Interfaces.h"   // Entity::IImpulsable (실제 경로 확인)
#include "Physics/PhysicsComponent.h"                  // Components::Physics / FindPhysics (실제 파일명 확인)
#include "scene/actor.h"
#include <cmath>

namespace TopdownShooter::Physics
{
    /// @brief 범용 물리 속도 버스트 — Player Dash + Monster Knockback. PhysicsMovement 옆 거주(b2Body 동일 취급).
    /// @details
    ///   - body 는 OnEnter 에서 Components::FindPhysics(GetOwner()) 로 해소(비소유).
    ///   - DoImpulse(dir): cd/active 게이트 → SetLinearVelocity(normalize(dir)*force) XZ→XY(Z→-Y) → 타이머 arm.
    ///   - i-frame/FX 슬롯 부착 금지(미니 god 방지). i-frame=Life, FX=연출.
    ///   - 속도싸움(후속): dash 입력 바인딩 시 controller 가 IsActive() 중 DoForward suppress. 현재 미배선.
    class Impulse : public SJH::Scene::Component, public Entity::IImpulsable
    {
    public:
        Impulse()
            : mImpulseForce(7.5f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::DashForce),
              mCooldown(0.8f, Algebraic::ENumericStatUseType::Natural, Algebraic::ENumericStatType::CoolDownSpeed)
        {
        }

        void OnEnter() override { mBody = Components::FindPhysics(GetOwner()); }
        void OnExit()  override { mBody = nullptr; }

        void Update(float dt) override
        {
            if (mActiveTimer   > 0.0f) mActiveTimer   -= dt;
            if (mCooldownTimer > 0.0f) mCooldownTimer -= dt;
        }

        void DoImpulse(vmath::vec2 dir) override
        {
            if (mCooldownTimer > 0.0f || IsActive()) return;
            if (!mBody || !mBody->GetBody()) return;
            const float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
            if (len <= 0.001f) return;
            vmath::vec2 n(dir[0] / len, dir[1] / len);
            const float force = mImpulseForce.GetValue();
            // XZ → Box2D XY (Z → -Y, spec §4.4) — PhysicsMovement/PB::Dash 미러
            mBody->GetBody()->SetLinearVelocity(b2Vec2(n[0] * force, -n[1] * force));
            mActiveTimer   = mDurationSec;
            mCooldownTimer = mCooldown.GetValue();
        }

        void ApplyKnockback(vmath::vec2 fromXZ) { DoImpulse(fromXZ); }  // convenience (Monster Knockback)
        bool IsActive() const { return mActiveTimer > 0.0f; }

    private:
        Algebraic::Numeric::Stat mImpulseForce;   // Stat(DashForce, base 7.5)
        Algebraic::Numeric::Stat mCooldown;       // Stat(CoolDownSpeed, base 0.8 — sec 저장)
        float mDurationSec   = 0.3f;              // plain (맞는 enum 없음)
        float mActiveTimer   = 0.0f;
        float mCooldownTimer = 0.0f;
        Components::Physics* mBody = nullptr;      // FindPhysics — 비소유
    };
}

#endif // __MYAPP_PHYSICS_IMPULSE_H__
```
- header-only → Physics/CMakeLists.txt 변경 불필요(physics_movement.h 와 동일).
- 만약 `Components::FindPhysics`/`Components::Physics` 가 안 잡히면 physics_movement.h 가 쓰는 정확한 표기(네임스페이스/include)를 그대로 복사.

========================================================================
[STEP 2] apps/_MyApp_/src/Entity/Player/PlayerActor.cpp — physics 분기에 부착
========================================================================
상단 include 에 추가(다른 Physics include 옆):
```cpp
#include "Physics/physics_impulse.h"
```
physics 분기의 `auto *pm = actor->AddComponent<Physics::PhysicsMovement>(cfg.movement.speed);` **직후**에 추가:
```cpp
            // Dash/Knockback 속도버스트 — dash 입력 미배선이라 현재 dormant (Action::Dash 바인딩 시 활성).
            actor->AddComponent<Physics::Impulse>();
```
- 이게 전부. controller wiring·Action::Dash·DoForward suppress 는 하지 마라(미배선 ship).

========================================================================
[검증]
========================================================================
1. (선택) configure: `cmake --preset ninja`
2. 빌드: `cmake --build --preset ninja --target _MyApp_` → exit 0, `Linking CXX executable apps/_MyApp_/_MyApp_`.
3. 실행 시각변화 없음이 정상 — Impulse 는 미배선(아무도 DoImpulse 호출 안 함)이라 게임플레이 동작 변화 0. (빌드가 검증의 전부.)

[Self-review]
- physics_impulse.h 가 physics_movement.h 와 같은 include 패턴(실제 파일명 반영)·header-only 인가?
- Impulse 가 Component + Entity::IImpulsable 구현(DoImpulse cd/active 게이트 → SetLinearVelocity XZ→XY → 타이머 arm)? OnEnter=FindPhysics?
- PlayerActor.cpp 는 include 1줄 + AddComponent<Physics::Impulse>() 1줄만 추가? controller/Action 미접근?
- 빌드 exit 0? IImpulsable 재정의 안 했나(이미 존재)?
- 변경 파일 = physics_impulse.h(신규) + PlayerActor.cpp 2개뿐? (`git status --short` — 무관 dirty 미관여로 구분)

[보고]
DONE/DONE_WITH_CONCERNS/BLOCKED + 변경 파일별 요약(physics_impulse.h 시그니처 + PlayerActor.cpp 추가 위치) + 빌드 마지막 줄 + git status(네 2파일). 커밋 금지.
```

---

## 사용 메모 (오케스트레이터/사용자용)
- Task 4 는 **미배선 ship**(dash 입력 없음) — 빌드 검증만, 게임플레이 변화 0. 속도싸움 suppress 는 dash 바인딩 시 후속.
- **유일 경합점 = PlayerActor.cpp** — PlayableDirector foundation 도 PlayerActor 에 director 부착(AddComponent)을 추가한다. 둘 다 *additive*(다른 줄)라 충돌 적으나, 같은 파일이니 **순서가 겹치면 한쪽 rebase**. (foundation 과 Task4 가 동시에 PlayerActor.cpp 를 만지면 merge 주의.)
- 결과(DONE+diff) 가져오시면 내가 spec(§5.4) 대비 검증 후 커밋.
- 커밋 메시지(권장): `[feat] : Impulse 컴포넌트 (Dash/Knockback 속도버스트, 미배선 ship) — PB분해 Step4`.
