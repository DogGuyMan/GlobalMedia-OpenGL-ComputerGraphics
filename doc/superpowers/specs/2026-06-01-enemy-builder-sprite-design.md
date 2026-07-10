# Spec — EnemyBuilder (적 스프라이트 + 2프레임 애니)

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **상태**: 설계. 코드 0줄. **별도 Claude Code Agent 가 구현 예정**(PlayerBuilder 미러 + ENEMY_FRONT 텍스처).
> **작성**: 2026-06-01. 브랜치 `game/module/ingame/temp`.
> **목표 동기**: 적이 현재 *비주얼 없음*(물리+Life+AI 만) → 시각 확인 불가. 플레이어처럼 Sprite + Playable 애니를 적에게 붙여 형태를 눈으로 확인.
> **PlayerBehavior 분해와 일관**: EnemyBuilder 는 `enemy_factory.h` 를 **건드리지 않는다** → 분해 Task 5(EnemyContactHandler→ContactCarrier)와 **충돌 0**.

---

## 1. 목표 / 비목표

### 목표
- `apps/_MyApp_/src/Bootstrap/EnemyBuilder.{h,cpp}` 신설 — **PlayerBuilder 미러**(composition root).
- 적 Actor 에 **단일 빌보드 SpriteRenderer + 2프레임 SpriteSequencePlayable** 부착 (텍스처 = `Playable::ENEMY_FRONT[variant]`).
- 라이브 스폰 경로(WaveController)를 EnemyBuilder 경유로 라우팅 → 스폰되는 적이 스프라이트를 가짐(시각 확인).

### 비목표
- **`enemy_factory.h` / `CreateEnemyActor` 수정 금지** (분해 Task 5 소유). EnemyBuilder 는 그 위에 스프라이트만 얹음.
- **적 8방향/4레이어 금지** — ENEMY_FRONT 는 단일 방향·단일 레이어(SimplePursueAI 도 facing 없음). 플레이어의 8그룹 패턴 적용 안 함.
- **적 피격/사망 FX 금지** — 적은 IActorPresentation sink 없음(그건 별도 feature). 본 작업은 *형태 확인용 walk 애니*만.
- **main.cpp 수정 최소화** — Fog/PostFX 에이전트 경합. 불가피하면 surgical.
- 단위 테스트 (`no_auto_tests`).

## 2. 검증된 사실 (드리프트 0)

- **ENEMY_FRONT** ([Constants.h:78-82](../../../apps/_MyApp_/src/Playable/Constants.h#L78)): `const EntityTextureConfig ENEMY_FRONT[3]` — C배열. 각 `{DrawOrder=0, "resources/texture/enemy/ENEMYn_FRONT.png", RowCount=1, ColCount=2, Flip=false}`. **PNG 3종 존재**(2528/1771/1376 bytes). ColCount=2 → 가로 2프레임 스트립.
- **CreateEnemyActor** ([enemy_factory.h:28-59](../../../apps/_MyApp_/src/Entity/Enemy/enemy_factory.h#L28)): `EnemyConfig{world,pos,playerTarget,hp,speed,damage,onDeathFx}` → CircleBody(r=0.4) + Life + SimplePursueAI(target,body,speed) + EnemyContactHandler + (onDeathFx 시)EnemyDeathHandler. **스프라이트 없음.** unique_ptr<Actor> 반환(트리 미부착).
- **PhysicsSystem::SyncToTransform** ([physics_system.cpp:39-55](../../../apps/_MyApp_/src/Physics/physics_system.cpp#L39)): 매 프레임 `FindPhysics(actor)->GetBody()` 위치로 `actor.Transform.Translate = (p.x, heightOffset, -p.y)`. → 적 actor 에 SpriteRenderer 직접 부착 시 빌보드가 b2Body 따라옴. heightOffset 기본 0(ground).
- **플레이어 스프라이트 패턴** ([PlayerActor.cpp:86-118](../../../apps/_MyApp_/src/Entity/Player/PlayerActor.cpp#L86)): `reg.FindUniformAtlas(path)` → 없으면 `CreateUniformAtlas(path, path, ColCount, RowCount)` → `AddComponent<SpriteRenderer>(atlas)` + `spr->flipX` + (ColCount>1) `AddComponent<SpriteSequencePlayable>(spr, SpriteFrameClip{0, ColCount, fps})` → `SetIsLoop(true); Play();`.
- **SpriteSequencePlayable ctor**(값 버전): `(SJH::Sprite::SpriteRenderer*, SJH::SpriteSequence::SpriteFrameClip)`. `SpriteFrameClip{startFrame, frameCount, fps}`.
- **WaveController** ([WaveController.cpp:43-51](../../../apps/_MyApp_/src/Stage/WaveController.cpp#L43)): inline EnemyConfig 채움(world/pos=RandomEdgePos/playerTarget/hp=20+wave*5/speed=1.5+wave*0.3/damage=10, onDeathFx 미바인딩) → `CreateEnemyActor(cfg)` → `mSpawnParent->AddChild`. ctor=(b2World*, spawnParent, playerActor, arenaHalfExtent).
- **Bootstrap CMake**: 명시 소스목록 STATIC `myapp_bootstrap`. PUBLIC = SJH::engine + MyApp::{InputHandler,Entity,Audio,Physics,Manager}. PRIVATE = MyApp::{VFX,Tween,Spawns} + game_deps + spdlog. (MyApp::Entity PUBLIC → CreateEnemyActor 가시. SJH::engine → SpriteRenderer/SpriteSequencePlayable 가시.)
- **SimplePursueAI**: facing 없음(단일 전진) → ENEMY_FRONT 단일방향으로 충분.
- **⚠ 미확인**: WaveController 가 현재 *실제 tick 되는지*(main 에 시스템으로 배선됐는지) Explore 미확정. 구현 시 라이브 스폰 경로 확인 필요(§5).

## 3. 아키텍처 (PlayerBuilder 미러 + 분해 비충돌)

```
WaveController(스폰 정책: 언제/어디/몇)  ──BuildEnemy(deps)──▶  EnemyBuilder(composition root)
EnemyBuilder ──CreateEnemyActor(cfg)──▶ enemy_factory (물리+Life+AI+contact, 무변경)
EnemyBuilder ──AddComponent<SpriteRenderer+SpriteSequencePlayable>──▶ 적 actor (ENEMY_FRONT[variant])
EnemyBuilder ──spawnParent->AddChild──▶ 씬 트리
```
- **enemy_factory 무변경** = 분해 Task 5(ContactCarrier 교체)와 같은 파일을 안 건드려 충돌 0. 스프라이트는 EnemyBuilder 가 *CreateEnemyActor 결과 위에* 얹음(컴포넌트는 entry 전 부착 OK — 플레이어 동형).
- **단일 레이어 owner-direct**: 플레이어는 4레이어라 child actor + QueueOffset 분리; 적은 1레이어라 **적 actor 에 직접** SpriteRenderer 부착(billboard 가 SyncToTransform 위치 추종). 더 단순.

## 4. 상세 설계

### 4.1 `EnemyBuilder.h` (신규 — Bootstrap)
```cpp
#ifndef __TOPDOWNSHOOTER_BOOTSTRAP_ENEMY_BUILDER_H__
#define __TOPDOWNSHOOTER_BOOTSTRAP_ENEMY_BUILDER_H__

#include <functional>
#include <vmath.h>

class b2World;
namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Bootstrap
{
    /// @brief BuildEnemy 입력 의존 (PlayerDeps 미러 — 비싱글턴만).
    struct EnemyDeps
    {
        b2World*           world        = nullptr;
        SJH::Scene::Actor* spawnParent  = nullptr;   ///< 적 child 부착 부모 (WaveController.mSpawnParent)
        SJH::Scene::Actor* playerTarget = nullptr;   ///< SimplePursueAI 추적 대상
        vmath::vec2        pos          = vmath::vec2(0.0f);
        int                hp           = 30;
        float              speed        = 2.0f;
        int                damage       = 10;
        int                variant      = 0;          ///< 0~2 → ENEMY_FRONT[variant % 3]
        float              spriteFps    = 6.0f;        ///< 2프레임 walk 애니 속도
        std::function<void(const vmath::vec3&)> onDeathFx; ///< 선택 (미바인딩 가능)
    };

    /// @brief 적 1체 조립 — CreateEnemyActor + ENEMY_FRONT 스프라이트/애니 + spawnParent 부착.
    /// @return 트리 부착된 적 Actor* (실패 시 nullptr).
    SJH::Scene::Actor* BuildEnemy(const EnemyDeps& deps);
}

#endif // __TOPDOWNSHOOTER_BOOTSTRAP_ENEMY_BUILDER_H__
```
> PlayerResult 같은 다중-핸들 struct 불필요(적은 transient·다수 — main 이 핸들 캐시 안 함). Actor* 반환만.

### 4.2 `EnemyBuilder.cpp` (신규)
```cpp
#include <GL/gl3w.h> // 최상단 — resource_registry.h→...→gl3w.h 보다 먼저.

#include "apps/_MyApp_/src/Bootstrap/EnemyBuilder.h"

#include "<Entity>/Enemy/enemy_factory.h"   // CreateEnemyActor / EnemyConfig (+box2d)
#include "apps/_MyApp_/src/Playable/Constants.h"           // ENEMY_FRONT
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "sprite/sprite_component.h"
#include "sprite/sprite_sequence_playable.h"

#include <<spdlog>/spdlog.h>

namespace TopdownShooter::Bootstrap
{
    SJH::Scene::Actor* BuildEnemy(const EnemyDeps& deps)
    {
        // 1) 물리+Life+AI+contact = 기존 factory (무변경)
        Entity::Enemy::EnemyConfig cfg;
        cfg.world        = deps.world;
        cfg.pos          = deps.pos;
        cfg.playerTarget = deps.playerTarget;
        cfg.hp           = deps.hp;
        cfg.speed        = deps.speed;
        cfg.damage       = deps.damage;
        cfg.onDeathFx    = deps.onDeathFx;
        auto enemy = Entity::Enemy::CreateEnemyActor(cfg);   // unique_ptr<Actor> (미부착)

        // 2) ENEMY_FRONT[variant] 스프라이트 + 2프레임 애니 (owner-direct, 단일 레이어)
        const auto& tex = Playable::ENEMY_FRONT[deps.variant % 3];
        auto& reg = SJH::ResourceRegistry::Get();
        auto* atlas = reg.FindUniformAtlas(tex.TexturePath);
        if (!atlas)
            atlas = reg.CreateUniformAtlas(tex.TexturePath, tex.TexturePath, tex.ColCount, tex.RowCount);
        if (atlas)
        {
            auto* spr = enemy->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
            spr->flipX = tex.Flip;
            if (tex.ColCount > 1)   // 애니 (2프레임 walk)
            {
                auto* seq = enemy->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
                    spr, SJH::SpriteSequence::SpriteFrameClip{0, tex.ColCount, deps.spriteFps});
                seq->SetIsLoop(true);
                seq->Play();
            }
        }
        else
        {
            spdlog::error("[enemy] atlas load 실패: {}", tex.TexturePath);
        }

        // 3) 씬 트리 부착 (entry → 컴포넌트 OnEnter 캐스케이드)
        if (!deps.spawnParent) return nullptr;
        return deps.spawnParent->AddChild(std::move(enemy));
    }
}
```
- **SpriteRenderer 부착이 entry 전이어도 OK**: AddChild(spawnParent entered) 시 적 actor + 자기 컴포넌트 OnEnter 캐스케이드(플레이어 동형). SpriteSequencePlayable.Play() 는 즉시 호출(엔터 무관 — elapsed_ 진행은 Update).
- **billboard 위치**: SyncToTransform 가 매 프레임 b2Body→transform → 빌보드 추종. heightOffset 0(ground) — 안 보이면 `FindPhysics(enemy.get())->SetHeightOffset(...)` 또는 QueueOffset 튜닝(§6).

### 4.3 WaveController 라우팅 (Stage)
[WaveController.cpp:43-51](../../../apps/_MyApp_/src/Stage/WaveController.cpp#L43) 의 inline `EnemyConfig + CreateEnemyActor + AddChild` 를 `Bootstrap::BuildEnemy` 호출로 교체:
```cpp
#include "apps/_MyApp_/src/Bootstrap/EnemyBuilder.h"
...
    Bootstrap::EnemyDeps d;
    d.world        = mWorld;
    d.spawnParent  = mSpawnParent;
    d.playerTarget = mPlayerActor;
    d.pos          = RandomEdgePos();
    d.hp           = 20 + mWave * 5;
    d.speed        = 1.5f + (mWave * 0.3f);
    d.damage       = 10;
    d.variant      = mSpawnCount % 3;   // 3종 순환 (mSpawnCount = 누적 스폰 카운터, 없으면 추가)
    Bootstrap::BuildEnemy(d);
```
- `mSpawnCount` 누적 카운터가 없으면 WaveController 에 멤버 추가(스폰마다 ++). variant 순환용.
- **CMake**: `Stage/CMakeLists.txt` 의 link 에 `MyApp::Bootstrap` 추가(Stage→Bootstrap, acyclic — Bootstrap 은 Stage 미의존). `Bootstrap/CMakeLists.txt` 소스목록에 `EnemyBuilder.cpp` 추가.

## 5. 라이브 스폰 경로 확인 (구현 시 필수)
- **WaveController 가 실제 tick 되는가?** main.cpp / StageBuilder 에서 WaveController 가 컴포넌트로 부착·Update 되는지 확인. **활성이면** §4.3 라우팅으로 적이 스프라이트와 함께 스폰됨(완료).
- **비활성(또는 main 수동 스폰)이면**: 적이 안 나오거나 main.cpp 에서 수동 spawn 중일 수 있음. 그 경로를 BuildEnemy 로 라우팅(단 main.cpp 는 Fog 경합 — surgical). 최악의 경우 **임시 디버그 스폰**(spawnParent=Director.Root, playerTarget=플레이어)으로 1~2체 띄워 시각 확인 후 정리.
- 목표는 "적 형태가 화면에 보임" — 스폰 경로가 막혀 있으면 그걸 먼저 풀어야 함.

## 6. 검증
- 빌드: `cmake --build --preset ninja --target _MyApp_` exit 0. (Bootstrap + Stage CMake 변경 → 자동 reconfigure.)
- 실행: `cd build_ninja/apps/_MyApp_ && ./_MyApp_` → **적이 ENEMY_FRONT 스프라이트로 표시 + 2프레임 walk 애니 루프**. 3종 순환(variant) 확인. 플레이어 추적 이동(SimplePursueAI) 중에도 빌보드가 b2Body 따라옴.
- 안 보이면 점검 순서: atlas 로드 로그 / QueueOffset(다른 빌보드/벽에 가려짐) / heightOffset / SyncToTransform 동작.

## 7. 소유 분할 / 조율 (PlayerBehavior 분해와)
| 항목 | 소유 | 충돌 |
|---|---|---|
| EnemyBuilder.{h,cpp} (신규) | EnemyBuilder 에이전트 | 신규 파일 — 충돌 0 |
| WaveController.cpp 라우팅 + Stage CMake | EnemyBuilder 에이전트 | 분해 미접근(Stage) — 충돌 0 |
| Bootstrap CMake (+EnemyBuilder.cpp) | EnemyBuilder 에이전트 | 분해 미접근 — 충돌 0 |
| **enemy_factory.h** | **분해 Task 5 (나)** | EnemyBuilder 는 **안 건드림** → 충돌 0 |
- **순서 무관**: EnemyBuilder 는 enemy_factory.h 를 안 건드리므로 분해 Task 4/5/6/7 과 독립. 어느 쪽 먼저든 OK. (Task 5 가 EnemyContactHandler→ContactCarrier 로 바꿔도 BuildEnemy 는 CreateEnemyActor 시그니처만 의존 — 영향 없음.)

## 8. self-review
- ✅ enemy_factory.h 무변경 → 분해 Task 5 충돌 0 (핵심 일관성).
- ✅ PlayerBuilder 미러(composition root) + 플레이어 스프라이트 패턴 재사용(FindUniformAtlas/SpriteSequencePlayable).
- ✅ 단일 레이어/방향(ENEMY_FRONT) — SimplePursueAI facing 없음과 일치. 8그룹 미적용(YAGNI).
- ⚠ WaveController 활성 여부 미확정 → 구현 시 라이브 스폰 경로 확인(§5). main.cpp 경합 주의.
- ⚠ variant 순환에 WaveController 스폰 카운터 필요(없으면 추가).
