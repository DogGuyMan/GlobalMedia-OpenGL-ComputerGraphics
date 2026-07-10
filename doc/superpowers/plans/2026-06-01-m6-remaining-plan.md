# M6 잔여 구현 Plan (Tasks 6/7/8/9) — 2026-06-01

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development 또는 executing-plans. 단계는 checkbox(`- [ ]`).
> **짝 spec**: [`2026-06-01-m6-as-built-design.md`](../specs/2026-06-01-m6-as-built-design.md) (as-built + 결정). 원본 plan [`2026-05-31-m6-sequencing-sound.md`](2026-05-31-m6-sequencing-sound.md) Task 6~10 의 *현행화*.

**Goal:** M6 Tasks 1~5(단발 FX 인프라+오디오+BGM, 커밋 완료) 위에 **적 피격/사망 FX + 플레이어 슬롯/배선 + 입력 트리거**를 얹어 M6 완료.

**Baseline (HEAD `2ff1c6b` 시점):** `MyApp::Spawns` 완성(`SpawnAudioInstance`/`SpawnVfxInstance`/`CombatSequences`/`AmbientSequences`/`SequenceContext`/`AutoDespawnOnFinish`/`OneShotSweeper`), `mFxRoot`+sweep 배선, Audio worldPos/listener, BGM 이관. 빌드 통과.

**검증/커밋 규약:** [[no_auto_tests]] — 단위 테스트 없음, `cmake --build --preset ninja --target _MyApp_` 빌드 성공 + 실행 시각 검증. 커밋은 **Task별 사용자 승인**. main.cpp 변경은 **즉시 커밋**(경합 reset 유실 방지).

---

## ⚠ 경합 가드레일 (필독 — spec §5)
- main.cpp = **Fog/Bloom PostFX Agent** 공유 / mouse_input·PlayerController = **입력 Agent** 공유.
- 착수 전 `git diff --stat` 로 HEAD 이동 확인. 별도 파일(Spawns/Entity) 위주, main.cpp surgical.
- **Task R4(입력)는 입력 Agent 완료 후.**

---

## Task R1 — 적 피격/사망 FX (원본 Task 9) 🟢 우선 (비경합)

**Files:**
- Create: `apps/_MyApp_/src/Entity/Enemy/EnemyDeathHandler.{h,cpp}`
- Modify: `apps/_MyApp_/src/Entity/Bullet/BulletContactHandler.{h,cpp}` (hit FX delegate)
- Modify: `<apps>/_MyApp_/src/Entity/Enemy/enemy_factory.h` (EnemyConfig.onDeathFx + attach)
- Modify: `apps/_MyApp_/src/Entity/CMakeLists.txt` (EnemyDeathHandler.cpp)
- Modify: `apps/_MyApp_/src/Stage/WaveController.{h,cpp}` (death delegate 멤버/적용)
- Modify: `apps/_MyApp_/src/Stage/StageConfig.h` + `StageBuilder.{h,cpp}` (ctx/delegate 전달) — 구조 확인 후
- Modify: `apps/_MyApp_/main.cpp` (fxRoot 를 CreateStageActor *전*으로 이동 + ctx 전달)

- [ ] **Step 1: EnemyDeathHandler.h**
```cpp
#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_DEATH_HANDLER_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_ENEMY_DEATH_HANDLER_H__
#include "scene/actor.h"
#include <functional>
#include <vmath.h>
namespace TopdownShooter::Entity::Enemy
{
    /// @brief 적 사망 self-poll — Life::IsAlive()==false 감지 시 death FX delegate + despawn (Q4).
    class EnemyDeathHandler : public SJH::Scene::Component
    {
      public:
        using DeathFx = std::function<void(const vmath::vec3&)>;
        explicit EnemyDeathHandler(DeathFx fx) : mOnDeathFx(std::move(fx)) {}
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override;
      private:
        DeathFx mOnDeathFx;
        bool    mDead = false;
    };
}
#endif
```

- [ ] **Step 2: EnemyDeathHandler.cpp**
```cpp
#include "<Entity>/Enemy/EnemyDeathHandler.h"
#include "apps/_MyApp_/src/Entity/Components/LifeComponents.h"
namespace TopdownShooter::Entity::Enemy
{
    void EnemyDeathHandler::Update(float /*dt*/)
    {
        if (mDead || !GetOwner()) return;
        auto* life = GetOwner()->GetComponent<Components::Life>();
        if (life && !life->IsAlive())
        {
            mDead = true;
            if (mOnDeathFx) mOnDeathFx(GetOwner()->GetTransform().Translate);
            GetOwner()->SetActive(false); // despawn (Bullet SetActive(false) 정통)
        }
    }
}
```

- [ ] **Step 3: BulletContactHandler hit FX delegate**
`.h` — `#include <functional>` + `<vmath.h>`, public `using HitFx = std::function<void(const vmath::vec3&)>; void SetOnHitFx(HitFx fx){ mOnHitFx=std::move(fx);}` + private `HitFx mOnHitFx;`.
`.cpp` `HandleHit` 의 `if (life) life->DoDamaged(mDamage);` 다음:
```cpp
        if (life && mOnHitFx) mOnHitFx(other->GetTransform().Translate);
```

- [ ] **Step 4: enemy_factory — EnemyConfig.onDeathFx + 부착**
`enemy_factory.h`: `#include "<Entity>/Enemy/EnemyDeathHandler.h"`. `EnemyConfig` 에 `EnemyDeathHandler::DeathFx onDeathFx;`. `CreateEnemyActor` 의 EnemyContactHandler 부착 다음:
```cpp
        if (cfg.onDeathFx) actor->AddComponent<EnemyDeathHandler>(cfg.onDeathFx);
```

- [ ] **Step 5: WaveController death delegate**
`.h`: `#include <functional>`, public `void SetEnemyDeathFx(std::function<void(const vmath::vec3&)> fn){ mEnemyDeathFx=std::move(fn);}` + private 멤버. `.cpp` `SpawnEnemy` 의 `cfg.damage = 10;` 다음: `cfg.onDeathFx = mEnemyDeathFx;`.

- [ ] **Step 6: ctx 전달 + fxRoot 재배치 (main + Stage)**
- **WaveController 생성 지점 확인**: `StageBuilder`/`CreateStageActor` 가 WaveController 를 어디서 AddComponent 하는지 read 후, `StageConfig` 에 `SequenceContext`(또는 death delegate) 필드 추가 → WaveController.SetEnemyDeathFx 호출.
- **main.cpp**: `mFxRoot` 생성을 `CreateStageActor` **전**으로 이동(현재 후). StageConfig 에 `ctx`(audio/vfx/reg/fxRoot/sceneRoot/world) 채워 전달. (예: death delegate = `[ctx](const vmath::vec3& p){ Spawns::SpawnEnemyDeathFX(ctx, p); }`.)
- ⚠ **대안(더 단순)**: WaveController 가 ctx 를 직접 보유 못 하면, main 이 CreateStageActor 반환 actor 에서 `GetComponent<WaveController>()` 로 찾아 SetEnemyDeathFx. 구현자가 StageBuilder 구조 보고 택일.

- [ ] **Step 7: bullet hit FX 바인딩** — Task R2(PlayerSequences)의 BulletFactory closure 에서 `bullet->GetComponent<BulletContactHandler>()->SetOnHitFx([ctx](const vec3& p){ Spawns::SpawnHitSpark(ctx,p); })`. (R2 와 연동 — R2 착수 시 함께.)

- [ ] **Step 8: CMake + 빌드** — `Entity/CMakeLists.txt` 에 `<Enemy>/EnemyDeathHandler.cpp`. `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_` → exit 0.

- [ ] **Step 9: 실행 검증** — 총알 명중 시 spark+피격음, 적 HP 0 시 폭발+사망음+사라짐, FxRoot 누수 없음.

- [ ] **Step 10: 커밋(사용자 승인)** — `feat(_MyApp_/M6): 적 피격(BulletContactHandler hit FX) + 사망(EnemyDeathHandler self-poll + despawn)`

---

## Task R2 — 플레이어 슬롯 빌더 PlayerSequences (원본 Task 6) 🟢 비경합

**Files:** Create `apps/_MyApp_/src/Spawns/PlayerSequences.{h,cpp}` + Modify `Spawns/CMakeLists.txt`.

원본 plan [`2026-05-31-m6-sequencing-sound.md`](2026-05-31-m6-sequencing-sound.md) **Task 6** 의 코드 그대로 유효(변경 없음). 요약:
- `BuildPlayerAttack(host, ctx, pb)` = `Parallel(BulletSpawnPlayable ∥ EffekseerPlayable "muzzle" ∥ FmodPlayable "shot")` — host 자식 "Player_AtkSeq" Composite, raw 반환. BulletFactory closure 에 **Task R1 Step 7 hit FX 바인딩 포함**.
- `BuildPlayerHit` = `Parallel(TweenPlayable shake ∥ FmodStudioPlayable "Damaged")` (인라인 G키 이관).
- `BuildPlayerDash` / `BuildPlayerDie` / `BuildPlayerMoveEffect(loop)`.
- ⚠ **검토**: 단발 SFX 가 AudioInstance(Studio) 로 통일됐으므로, 슬롯 안 `FmodPlayable "shot"`(Core)를 유지할지 `FmodStudioPlayable` 이벤트로 바꿀지 재개 시 확인. (슬롯은 재사용 Composite 라 AudioInstance 단발과 무관 — Core 유지 가능.)

- [ ] 빌드 + 커밋 `feat(_MyApp_/M6): 슬롯 빌더 PlayerSequences`

---

## Task R3 — PlayerFactory composition root + multi-clip (원본 Task 7) 🔴 스프라이트 블록

**🔴 선행 결정 (사용자 = "새 스프라이트 통합")**: `apps/_MyApp_/resources/texture/player/` 의 개별 directional PNG(BACK/FRONT × IDLE/MOVE × frame)를 애니로 로드하는 **방식 확정 필요**. 현 `UniformAtlas` = 단일 그리드 PNG 전용. 후보:
- (a) 개별 텍스처 N장 + 프레임마다 texture 전환 (sprite 시스템 확장/신규)
- (b) grid atlas 패킹 후 기존 UniformAtlas
- (c) 방향별 atlas 분리
**→ 이 결정 없이 R3 착수 불가. 재개 시 사용자 질의.**

결정 후:
- `PlayerActorConfig`(Entity) 에 SpriteCfg(atlas/clips 또는 새 sprite 표현) + BehaviorCfg 추가.
- `Spawns/PlayerFactory.{h,cpp}` — `CreatePlayerActorFull(config, ctx)`: base `CreatePlayerActor`(Life/Physics/Controller, 변경 0) + SpriteRenderer + 멀티클립(또는 새 sprite) + PlayerBehavior(Init) + 5슬롯(R2 Build*) SET. (의존 사이클 회피 spec §2.1 — Spawns 거주.)
- `main.cpp` `WramupPlayer` → `CreatePlayerActorFull(pac, ctx)` 호출 (현 인라인 대체). ⚠ clip 보관 lifetime(원본 plan Task 7 Step 3 경고) — PlayerBehavior 멤버 벡터 권장.
- 빌드 + 실행(이동 시 Move/정지 Idle 클립) + 커밋.

---

## Task R4 — 입력 트리거 + 인라인 제거 (원본 Task 8) 🟡 경합 (입력 Agent 후)

**Files:** `apps/_MyApp_/main.cpp` (`onMouseButton`/`onKey`).
원본 plan **Task 8** 코드 유효:
- 좌클릭 → `pb->Attack(ndcDir)` (bullet = 슬롯 BulletSpawnPlayable) — 인라인 ShotComposite(main ~320) 제거.
- Shift → `pb->Dash(pb->GetAttackDirection())`.
- 인라인 G키 DamageComposite(main ~285) 제거 (피격은 EnemyContactHandler→Hit 슬롯).
- ⚠ `mouse_input`/`PlayerController` 경합 — 입력 Agent 완료 확인 후. NDC 방향 산출은 그쪽 변경과 정합 맞출 것.
- 빌드 + 실행(좌클릭 발사/Shift 대시/피격 연출) + 커밋.

---

## Task R5 — 최종 통합 시각 검증 (원본 Task 10)

`cd build_ninja/apps/_MyApp_ && ./_MyApp_` — 체크: BGM / WASD 클립전환 / 좌클릭 Attack+발사 / Shift Dash / 적 접촉 Hit / 총알명중 spark+음 / 적 사망 폭발+despawn / 단발 FX 누수 없음 / main 인라인 잔존 없음(grep). 누수 점검 `sh <shell>/CMakeExecute.sh debug _MyApp_ leaks`. → `doc/topdown-shooter-progress.md` M6 갱신.

---

## 다음 세션 진입점
1. `git diff --stat` 로 HEAD/경합 확인 (Fog·입력 Agent 진행도).
2. **권장 순서**: R1(적 FX, 비경합) → R2(슬롯, 비경합) → **R3 착수 전 스프라이트 로딩 방식 사용자 질의** → R4(입력 Agent 후) → R5.
3. 막히면 spec [`2026-06-01-m6-as-built-design.md`](../specs/2026-06-01-m6-as-built-design.md) §4(잔여 상세)·§5(경합) 참조.

## 변경 기록
| 일자 | 변경 |
|---|---|
| 2026-06-01 | 잔여 plan 작성 — Tasks 1~5 완료 baseline 위 R1(적FX)/R2(슬롯)/R3(PlayerFactory+스프라이트블록)/R4(입력,경합)/R5(검증). 경합 가드레일 + 스프라이트 로딩 미결정 명시. 작성 Claude |
