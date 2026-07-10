# Handoff — PlayerBehavior 분해 *구현 진행 상태* (subagent-driven 실행 중)

> **🔴 대체됨 (2026-06-02)**: 본 문서의 §3 Task 상태표·git 스냅샷은 **stale**(이후 Task 3 커밋 + Life Timer 마이그레이션 + 적 스폰 live + 커밋갭 닫힘). **재개 단일 진입점 = [`doc/handoffs/2026-06-02/2026-06-02-playerbehavior-decomposition-resume-handoff.md`](2026-06-02-playerbehavior-decomposition-resume-handoff.md)** 를 보라. 본 문서는 §4(설계갭 정정)·§10(병렬 조율) 의 *경위 기록* 으로만 참고.

> **목적**: PlayerBehavior god-component 분해의 **구현 실행**(plan Task 0~7)을 subagent-driven-development 로 진행하던 중, **Task 4 진입 전 다른 작업이 끼어들 수 있어** 무손실 보존. 재개 시 이 문서 + 아래 정본으로 즉시 복귀.
> **작성**: 2026-06-01. 브랜치 `game/module/ingame/temp`.
> **현재 위치**: **Task 0·1·2 커밋 완료 / Task 3 구현 완료(미커밋, GUI 실행 검증 대기) / Task 4~7 미착수.**

---

## 0. 정본 문서 (읽기 순서)

| 문서 | 경로 | git | 역할 |
|---|---|---|---|
| **설계 핸드오프** | `doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition.md` | 추적됨 | §13(잠긴 결정)+§4(분해테이블)+§14(빌드순서). **§13.1 은 2026-06-01 정정됨**(아래 §4 참조) |
| **spec** | `doc/superpowers/specs/2026-06-01-playerbehavior-decomposition-design.md` | **gitignore(로컬)** | 잠긴 결정 직렬화 + 실제 시그니처. §3.1 정정됨 |
| **plan** | `doc/superpowers/plans/2026-06-01-playerbehavior-decomposition.md` | **gitignore(로컬)** | **Task 0~7 완전 코드 + 검증/커밋 step.** 재개 시 각 Task 전문을 여기서 추출 |
| (이 문서) 실행 상태 | `doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition-execution.md` | 추적됨 | 진행 SHA + 잔여 Task + 재개 절차 |

> ⚠ spec/plan 은 `doc/superpowers/` = **gitignore(로컬 전용)**. 다른 머신/세션에서 없으면 본 핸드오프 + 설계 핸드오프 §13/§14 로 재구성. (git status 에 안 뜨지만 파일은 로컬에 존재.)

---

## 1. 실행 모델 (subagent-driven-development + 프로젝트 override)

스킬 `superpowers:subagent-driven-development` 기본형에 프로젝트 제약 2건 override:

- **`no_auto_tests`**: 서브에이전트 TDD/단위테스트 **금지**. Task 검증 = ① 빌드 `cmake --build --preset ninja --target _MyApp_` exit 0 + ② (해당 시) GUI 실행 검증.
- **commit = 사용자 blocking**: 서브에이전트·오케스트레이터 **자율 커밋 금지**. 카덴스 = **Task별 게이트**(사용자 확정): `구현(no commit) → 오케스트레이터 diff 직접 검증(보고 불신) → 사용자 승인 → 사용자 커밋`.
- **GUI run-verify Task = 3/5/6/7** (오케스트레이터가 창을 못 봄 → 사용자 실행). 빌드-only Task = 0/1/2/4.
- 구현 서브에이전트 모델 = **sonnet**(mechanical~moderate, plan 이 완전 명세). **무거운 Task 5/6 은 별도 spec+quality 리뷰 서브에이전트** 권장.
- 구현 서브에이전트 프롬프트 패턴: full task text(plan 파일 읽히지 말고 직접 제공) + "커밋 금지/테스트 금지/지정 파일만/한국어 주석" + 빌드 검증 + self-review + DONE/DONE_WITH_CONCERNS/BLOCKED 보고.
- **동시 작업자 경합**: `apps/_MyApp_/resources/vfx/`(VFX 작업 — `d4d37ef` 로 커밋됨) + `main.cpp`(Fog/PostFX agent). **커밋 시 자기 Task 파일만 `git add`**(사용자가 자주 fold → 미커밋 유실 위험, 메모리 경고). 본 분해 Task 들은 main.cpp 안 건드림(Task 6 도 PlayerController/PlayerBuilder/PlayerActor 만).

---

## 2. 커밋 로그 (진행분)

```
d4d37ef [add] : effekseer vfx 추가            ← 동시 작업자(VFX), 내 작업 아님
1f6c23f [feat] : Life + DoDie + sink forwar   ← Task 2 (+ actor.h if-constexpr 보완 fold)
82735ed [feat] : IActorPresentation + IImpulsable + EFacing/EPose 인터페이스 (PB분해 Step1)
78ca9ee [refactor] : 엔진 GetComponent 게이트 is_polymorphic 완화 (PB분해 Step0)
afa9871 [dev] : 애니메이션 프레임 설정         ← 분해 착수 전 base
```

---

## 3. Task별 상태

| Task | 내용 | 상태 | 커밋/검증 |
|---|---|---|---|
| **0** | 엔진 `GetComponent<T>` 게이트 `is_base_of<Component>`→`is_polymorphic_v` (actor.h) + PhysicsComponent.h:73 주석 | ✅ **커밋** | `78ca9ee`. 빌드 66/66 |
| **1** | `IActorPresentation`+`IImpulsable`+`EFacing`/`EPose`/`Quantize4` (Components.Interfaces.h) | ✅ **커밋** | `82735ed`. 빌드 15/15 |
| **2** | `Life` 확장(i-frame plain float+`DoDie` one-shot+`SetOnDeathFx`+OnEnter sink 캐시+`DoDamaged` i-frame fold+Life(hp,cur,iframe) ctor) **+ actor.h if-constexpr 보완**(아래 §4) | ✅ **커밋** | `1f6c23f`. 빌드 66/66 |
| **3** | `EnemyContactHandler` → `Life::DoDamaged` repoint (load-bearing 접촉 데미지 작동 + 인터페이스 조회 라이브 증명). PlayerBehavior.h include 제거 | 🟡 **구현 완료·미커밋** | 빌드 5/5 exit 0, spec 검증 ✅. **GUI 실행 검증 대기** |
| **4** | `Impulse`(`Physics/physics_impulse.h`, IImpulsable, DashForce/CoolDownSpeed Stat) + CreatePlayerActor physics 분기 AddComponent. **미배선 ship**(dash 입력 없음) | ⬜ 미착수 | 빌드-only |
| **5** | `Carrier` base(얇은 배달)+`Projectile`/`ContactCarrier` — Bullet→Projectile / 적→ContactCarrier 흡수. BulletContactHandler/EnemyContactHandler 삭제. **가장 무거움(5a/5b 2 sub-commit)** | ⬜ 미착수 | 빌드+GUI |
| **6** | `PlayerSpriteDirector`(8그룹 Visible 토글, IActorPresentation sink)+PlayerActor 8그룹 빌드+PlayerBuilder 8테이블+i-frame 0.5s 주입+PlayerController RD5 계산→push+**flipX 핵 삭제** | ⬜ 미착수 | 빌드+GUI(facing 부호 확인) |
| **7** | `PlayerBehavior.{h,cpp}`+`BulletSpawnPlayable.{h,cpp}`+`PlayerMovementComponents.h` 철거 + Entity CMake 정리 | ⬜ 미착수 | 빌드+GUI(회귀 0) |

---

## 4. ⚠ 중요 설계 갭 발견 + 수정 (Task 2 중, 잠긴 §13.1 정정)

**발견**: 잠긴 설계 §13.1/spec §3.1 의 "게이트만 완화하면 됨, static_cast 오용 없음(인터페이스 typeid 맵 키 미스로 fast-path 미스)" 은 **사실 오류**.
- `GetComponent<T>` 본문 fast-path `static_cast<T*>(it->second.get())` 의 `it->second.get()` 정적 타입은 `Component*`. T 가 **비-Component 인터페이스**(예: `IActorPresentation`)면 `static_cast<인터페이스*>(Component*)` = 무관한 polymorphic 타입 변환 → **컴파일 에러**. fast-path 가 런타임에 안 타도 `GetComponent<Interface>()` **인스턴스화 시점에 컴파일**돼야 함.
- Task 0 빌드는 인터페이스 호출이 없어 통과 → **Task 2 `Life::OnEnter` 의 `GetComponent<IActorPresentation>()` 에서 처음 노출**.

**수정 (정착)**: `src/scene/actor.h` GetComponent 본문 fast-path 를 **`if constexpr (std::is_base_of_v<Component, T>)`** 로 가드.
```cpp
template<typename T, typename>
T* Actor::GetComponent() const
{
    if constexpr (std::is_base_of_v<Component, T>)   // concrete 만 — static_cast 핫패스(RTTI 없음)
    {
        auto it = mComponents.find(typeid(T));
        if (it != mComponents.end())
            return static_cast<T*>(it->second.get());
    }
    // 인터페이스/base = slow-path dynamic_cast (if constexpr 밖 — 모든 T 에 항상 실행)
    for (auto& [ti, comp] : mComponents)
        if (auto* p = dynamic_cast<T*>(comp.get()))
            return p;
    return nullptr;
}
```
- **인터페이스 조회는 막히지 않음**: slow-path `dynamic_cast` 루프가 `if constexpr` **밖**이라 모든 T(인터페이스 포함)에 실행. concrete=fast static_cast / interface=slow dynamic_cast. (구현자 초안은 fast-path 를 `dynamic_cast` 로 바꿨으나, 오케스트레이터가 핫패스 RTTI 회피 위해 if-constexpr 로 정제.)
- **이 actor.h 보완은 Task 2 커밋 `1f6c23f` 에 fold됨**(Task 0 의 완성).
- **문서 정정 완료**: 핸드오프 §13.1(미커밋 — 본 작업과 별개 doc 변경) + spec §3.1(로컬).
- **검증**: 빌드 66/66 + Task 3 의 `GetComponent<IDamageable>` 도 컴파일·동작.

---

## 5. 현재 미커밋 워킹트리 (재개 시 처리)

```
 M apps/_MyApp_/src/Entity/Enemy/EnemyContactHandler.cpp   ← Task 3 (GUI 검증 후 커밋)
 M apps/_MyApp_/src/Entity/Enemy/EnemyContactHandler.h      ← Task 3
 M doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition.md   ← §13.1 정정 (별도 doc 커밋)
```
(`doc/superpowers/specs|plans/...` 는 gitignore 라 status 무표시 — 정상.)

---

## 6. 재개 절차 (Task 4 진입 전 다른 작업 후 복귀)

1. **git 상태 재확인**: `git log --oneline -6` (위 §2 와 일치?) + `git status --short` (Task 3 미커밋 잔존? 다른 작업이 reset 했으면 §7 의 Task 3 코드로 재적용).
2. **actor.h if-constexpr 유지 확인**: `grep -n "if constexpr" src/scene/actor.h` (line ~188). 유실됐으면 §4 코드로 복구.
3. **Task 3 마무리**:
   - GUI 실행: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`. 플레이어를 적에 붙임 → **빠른 데미지 → 사라짐(SetActive(false))** (i-frame 미주입이라 즉사 — 정상, Task 6 에서 0.5s 주입). 이전엔 no-op. = load-bearing 수정 + 인터페이스 조회 런타임 증명.
   - 커밋(EnemyContactHandler.{cpp,h} **2파일만**):
     ```
     [fix] : 적 접촉 데미지 repoint → Life::DoDamaged (PB분해 Step3, load-bearing)
     - dead PlayerBehavior::Hit(nullptr no-op) → GetComponent<IDamageable>()->DoDamaged
     - 접촉 데미지 작동 + PlayerBehavior.h include 제거
     ```
   - 핸드오프 §13.1 정정 doc 은 별도 커밋(`[docs] PB분해 §13.1 static_cast 오용 정정`).
4. **Task 4~7**: plan `doc/superpowers/plans/2026-06-01-playerbehavior-decomposition.md` 의 해당 Task 전문을 추출 → §1 의 구현 서브에이전트 패턴(sonnet, no commit/test, 빌드 검증)으로 디스패치 → diff 직접 검증 → 사용자 승인 커밋. Task 5/6 은 별도 리뷰 서브에이전트 권장.

---

## 7. Task 3 코드 (유실 시 재적용용 — 무손실 보존)

**`apps/_MyApp_/src/Entity/Enemy/EnemyContactHandler.cpp`** (전체):
```cpp
#include "Entity/Enemy/EnemyContactHandler.h"
#include "Entity/Components/Components.Interfaces.h"   // IDamageable
#include "scene/actor.h"

namespace TopdownShooter::Entity::Enemy
{
    EnemyContactHandler::EnemyContactHandler(int damage) : mDamage(damage) {}
    EnemyContactHandler::~EnemyContactHandler() = default;

    void EnemyContactHandler::OnCollisionEnter(SJH::Scene::Actor* other)
    {
        if (!other) return;
        // 게이트 완화(Task0)로 인터페이스 직접 조회. Life 가 IDamageable 다중상속 → slow-path dynamic_cast.
        // i-frame 게이트가 Life::DoDamaged 안에 있어 총알+접촉 모두 보호.
        if (auto* dmg = other->GetComponent<IDamageable>())
            dmg->DoDamaged(mDamage);
    }
}
```
**`EnemyContactHandler.h`**: 클래스 위 주석 1줄만 `/// @brief 플레이어에 접촉 시 IDamageable::DoDamaged(damage) 호출 (i-frame 게이트는 Life 거주).` (include/시그니처 무변경.)

---

## 8. 검증된 사실 / 재개 시 유의

- **인터페이스 GetComponent 작동**: Task 0 if-constexpr + slow-path dynamic_cast 로 `GetComponent<IDamageable/IActorPresentation>()` 컴파일·동작 확인. **단 sink(`IActorPresentation`) 구현체 `PlayerSpriteDirector` 는 Task 6 에서야 부착** → 그 전까지 `Life.mSink == nullptr`(silent no-op, 정상).
- **회귀 0 유지 중**: PB 는 여전히 dead(미삭제, Task 7). Task 0~3 동작 변화 = Task 3 접촉 데미지 작동(양성)뿐.
- **Task 4 미배선 의도**: Impulse 는 dash 입력 없이 ship(correct-by-construction). 속도싸움 suppress 는 dash 바인딩 시 controller 에(현재 dormant).
- **Task 6 열린 디테일**: `Quantize4` facing 부호 규약(z>0=Front)은 GUI 실행에서 1줄 조정 여지. `SpriteCfg` 단일 `direction` → 8그룹 `DirGroupCfg` 테이블로 config 형태 변경(PlayerBuilder `kPlayerGroups` 8 + PlayerActor 루프). PlayerSpriteDirector::OnEnter 가 `Apply()` 로 초기 가시성(Front/Idle).
- **Task 5 무거움**: 5a(Bullet→Projectile, BulletContactHandler 흡수) + 5b(적→ContactCarrier, 적 body 재사용=동작 보존; "자식 센서 hurtbox" 별도 fixture 는 후속). Spawns↔Entity 순환 link 주의(Carrier.h header-only→link 불필요, include 경로만).
- **확정 4 디테일**: i-frame=plain float / Components::Movement **유지**(삭제 안 함) / 공격윈도 0.15s / Carrier 얇은 베이스.
- **잠긴 결정 재논의 금지**(§13/§14). §13.1 만 사실오류로 정정됨(설계 의도 불변, 컴파일 메커니즘만).

---

## 10. 병렬 작업 조율 — Entity 피격/사망 FX (다른 에이전트)

분해와 **병렬로** "피격 깜빡임 + 사망 디졸브" 연출을 다른 Claude Agent 가 진행(사용자 결정 2026-06-01). 정본 spec: `doc/superpowers/specs/2026-06-01-entity-hit-death-fx-design.md`.
- **다른 에이전트 = Layer A+B (지금, 충돌 0)**: ① 엔진 `SpriteRenderer` FX uniform 필드+송신(`src/sprite/sprite_component.{h,cpp}` — billboard_atlas 의 uEnableHit/uTime/uEnableDissolve/uDissolve* 구동, 기본값 동작 변화 0) ② `Life` 사망지연 캐퍼빌리티(`mDeathDelaySeconds` 기본 0, DoDie 지연 비활성 — additive backward-compat).
- **나 = Layer C (분해 Task 6 에 fold)**: PlayerSpriteDirector 의 `ReactDamaged`(깜빡임 윈도우)/`ReactDied`(디졸브 램프)/Update FX + `OnEnter` dissolve.png 로드 + PlayerBuilder `SetDeathDelaySeconds(0.5f)` 주입. **Task 6 은 Layer A+B 산출에 의존** → A+B 먼저.
- **충돌 매트릭스**: A=`src/sprite/`(내 분해 미접근) / B=`LifeComponents.h`(내 Task 2 커밋 후 additive, Task 4/5/7 미접근) → **분리 OK**. 단 Task 6 의 PlayerBuilder i-frame 주입 ↔ B 의 사망지연 주입은 같은 줄에 합쳐짐(내 Task 6 에서). 다른 에이전트는 **PlayerSpriteDirector 를 만들지 않는다**(그건 내 Task 6 — 병렬 생성 시 충돌).
- 순서: 다른 에이전트(A+B 커밋) → 내 분해 재개 Task 4→7(Task 6 이 A+B 활용).

### 10.2 병렬 작업 — EnemyBuilder (적 스프라이트, 또 다른 에이전트)
적 시각 확인용 스프라이트+2프레임 애니. 정본 spec `doc/superpowers/specs/2026-06-01-enemy-builder-sprite-design.md` + 프롬프트 `doc/handoffs/2026-06-01/2026-06-01-enemy-builder-agent-prompt.md`.
- 다른 에이전트 = `Bootstrap/EnemyBuilder.{h,cpp}` 신설(PlayerBuilder 미러) + `WaveController` 라우팅 + Stage/Bootstrap CMake.
- **분해 Task 5 와 충돌 0**: EnemyBuilder 는 **`EnemyFactory.h` 를 안 건드린다**(CreateEnemyActor 위에 스프라이트만 얹음). Task 5 가 EnemyContactHandler→ContactCarrier 로 바꿔도 BuildEnemy 는 CreateEnemyActor 시그니처만 의존 → 영향 없음. 순서 무관.
- 신규 모듈 의존 Stage→Bootstrap(acyclic). main.cpp 미접근(WaveController 활성 시). ENEMY_FRONT[3] 텍스처/PNG 존재.

## 9. 변경 기록
| 일자 | 변경 |
|---|---|
| 2026-06-01 | 실행 핸드오프 작성 — Task 0·1·2 커밋(78ca9ee/82735ed/1f6c23f) + Task 3 구현(미커밋, GUI 검증 대기). **설계 갭 발견·수정**(§13.1 static_cast 오용 → if-constexpr 가드, Task 2 에 fold). 카덴스=Task별 게이트. 동시 VFX/Fog 경합 주의. |
| 2026-06-01 | §10 추가 — 병렬 피격/사망 FX 작업 조율(다른 에이전트 = 엔진 SpriteRenderer FX + Life 사망지연 / 나 = Task 6 sink 구동). spec `2026-06-01-entity-hit-death-fx-design.md`. |
