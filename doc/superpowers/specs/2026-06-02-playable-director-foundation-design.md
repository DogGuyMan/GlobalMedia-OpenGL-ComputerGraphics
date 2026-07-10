# Spec — PlayableDirector 연출 foundation (모든 연출은 PlayableDirector 경유)

> **철학 (사용자 확정)**: *게임 로직(HP/damage/physics/AI/입력판정)을 제외한 모든 연출/비주얼/사운드는 반드시 `PlayableDirector` 를 통한 `Playable` 로만 구현한다.* 산재한 직접 uniform/sound 호출·ad-hoc Composite 조립 금지.
> **상태**: 설계. 코드 0줄. **foundation 에이전트가 먼저 구현 → 이후 스프라이트(Task6) ∥ PostFX·사운드(hit-FX) 병렬.**
> **작성**: 2026-06-02. 브랜치 `game/module/ingame/temp`. HEAD `f1e1a23`.
> **잠긴 분해와 정합**: 이 foundation 은 PlayerBehavior 분해(Task 0~7)의 `IActorPresentation` sink(RD1~6)를 *위반하지 않고 구현 방식만 PlayableDirector 로 구체화*. 분해 Task6 의 `PlayerSpriteDirector` = 이 PlayableDirector 로 **통합**.

---

## 1. 목표 / 비목표

### 목표
- `PlayableDirector` 신설 — **엔티티별 연출 재생 관리자 + `IActorPresentation` sink 구현체**. named Playable 보유 + `Play(key)` + 중앙 tick.
- `PostFXRegistry` 싱글턴 — PostFX material 핸들을 이름으로 노출(Playable 이 비네팅/흑백 uniform 도달).
- **기존 연출 seam 전면 마이그레이션** (사용자 확정 "전부 통합"): `onFire`/`onDamage`/`onDeathFx`/`SetOnHitFx` 가 ad-hoc 람다/delegate 대신 PlayableDirector(엔티티 반응) 또는 Spawns/(월드점 단발, director 가 트리거) 경유.
- 이후 두 병렬 트랙이 이 위에 Playable 을 등록: 스프라이트(분해 Task6) ∥ PostFX·사운드(hit-FX).

### 비목표
- **Playable 코어(`src/playable/`, `src/timer/`) 수정 금지** — IPlayable/PlayableBase/Composite 그대로 사용.
- **Spawns/ (OneShotSweeper/AutoDespawnOnFinish/VfxInstance/AudioInstance/CombatSequences) 폐기 안 함** — 월드점 단발의 검증된 기구. PlayableDirector 가 *트리거*만.
- **잠긴 분해 결정(RD1~6) 변경 금지** — sink 추상 seam 유지, 구현만 director.
- **셰이더 수정 금지** (billboard_atlas.fs / grayscale_vignetting.fs 완성).
- **단위 테스트 자동 추가 금지** (no_auto_tests).

## 1.5 정통 ownership/tick 규칙 (역할별 분리 — 2026-06-02 사용자 확정)

연출 Playable 의 **소유·tick·정리**는 *역할(role)* 에 따라 정확히 3가지로만 구분한다. "Playable 이니까" 가 아니라 "어떤 역할의 연출인가" 로 소유 모델이 결정된다.

| 역할 | 소유 / tick / 정리 모델 | 예 |
|---|---|---|
| **트리거 이벤트 연출** (게임 verb 로 1회 발동: fire/hit/death/attack) | **[C] `PlayableDirector` 소유** — `Register(key)` + 중앙 tick, verb→`Play(key)`=Stop()+Play() | player fire / hit / death |
| **엔티티 상시 루프** (엔티티 존재와 함께 지속·반복) | **[A] 영속 actor 의 scene-Component** — `AddComponent` + `SetIsLoop(true).Play()`, scene 트리가 tick (actor 와 함께 소멸) | 스프라이트 애니, enemy idle tween, BGM |
| **월드점 fire-and-forget** (특정 좌표 1회 + 자동 소멸) | **[B] `Spawns/` 임시 actor + `AutoDespawnOnFinish` + `SweepFinishedChildren(fxRoot)`** | 피격 spark, 적 사망 폭발 |

**핵심 규칙 — [B] 트리거 주체는 *도메인 seam* 이지 director 가 아니다.** `PlayableDirector` 는 **순수 [C]** 다(named Playable 오케스트레이션 전용). 월드점 단발 [B] 는 `Life::mOnDeathFx` / `BulletContactHandler::onHitFx` 같은 **도메인 seam** 이 `Spawns/` 자유함수(`SpawnEnemyDeathFX`/`SpawnHitSpark`)를 직접 호출해 트리거한다. 따라서 `ReactDied` 가 `Play("death")`[C] **와** `SpawnEnemyDeathFX`[B] 를 동시에 하던 이중 트리거는 **폐기**(P2). 이로써 director 는 `Spawns/`·`SequenceContext` 의존이 사라지고(`SetSpawnContext`/`mCtx` 제거), un-swept fxRoot 누수 문제도 소멸한다.

> 이 규칙은 본 spec §1·§3 의 "director 가 월드점 단발을 트리거" 표현을 **갱신**한다(역할별 분리 정합, Q2 확정). [A] 는 위반이 아니다 — 상시 루프 연출(스프라이트/BGM/idle tween)은 scene-Component 가 정통. 적도 동일 [C] 포트(`IActorPresentation`)로 통일 + delegate 은퇴는 **P4(Task5/M6-Task9 조율)**, `BulletSpawnPlayable`(게임로직-in-Playable 위반) 삭제는 **P1**, `PlayerBehavior` 철거는 **P5(Task7)**.

## 2. 검증된 현 인프라 (proto-PlayableDirector)

- `PlayableBase : public IPlayable, public SJH::Scene::Component` ([playable_base.h:12](../../../src/playable/playable_base.h#L12)). `Update(float) final`(line 44-49) → `OnUpdate(dt)`. `Play()`(paused/finished=false+OnPlay) / `Stop()`(+elapsed_=0, =Reset) / `IsFinished()`.
- Playable 은 **씬 tick**: `Director::Update(dt)`([main.cpp:348](../../../apps/_MyApp_/main.cpp#L348)) → root 재귀 → Component.Update. 즉 Playable 을 Component 로 보유하거나 직접 보유 후 Update 호출.
- 종료 정리: `AutoDespawnOnFinish` + `SweepFinishedChildren(fxRoot)`([main.cpp:349](../../../apps/_MyApp_/main.cpp#L349) / [OneShotSweeper.cpp:7](../../../apps/_MyApp_/src/Spawns/OneShotSweeper.cpp#L7)).
- 월드점 단발 자유함수: `SpawnVfxInstance(fxParent, vfx, effect, pos)` / `SpawnAudioInstance(fxParent, desc, pos?)` / `CombatSequences`(`SpawnHitSpark`/`SpawnEnemyDeathFX`/`SpawnPickupChime`) — fxRoot 밑 임시 Actor + leaf Playable + AutoDespawn. ([Spawns/](../../../apps/_MyApp_/src/Spawns/))
- 현 seam: `onFire`/`onDamage` = PlayerBuilder 람다가 **Director.Root() 밑 임시 Actor + Composite 직접 조립**([PlayerBuilder.cpp:47-94](../../../apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp#L47)). `onDeathFx`/`SetOnHitFx` = delegate(현 미바인딩).
- PostFX: `passComponent->mMaterial->Properties.{Floats|Vec3s|Ints}[key]` 직접. `FindPassMaterial(name)`([main.cpp:477](../../../apps/_MyApp_/main.cpp#L477)). `mPassComponents` 는 **game_application 소유**(Manager 아님).
- `IActorPresentation`(6 verb defaulted no-op) ([Components.Interfaces.h](../../../apps/_MyApp_/src/Entity/Components/Components.Interfaces.h)). `Life.mSink`→`ReactDamaged`/`ReactDied`(LifeComponents.h). 구현체 0.
- **MultipleTimer 정통**([src/timer/multiple_timer.h]) — `Component` + `map<string,Timer>` 보유 + 중앙 Tick. **PlayableDirector 가 따를 패턴.**

## 3. `PlayableDirector` 설계

`apps/_MyApp_/src/Playable/PlayableDirector.{h,cpp}` (Client — IActorPresentation·게임 연출 어휘에 의존).
```cpp
namespace TopdownShooter::Playable
{
    /// @brief 엔티티별 연출 재생 관리자 + IActorPresentation sink. MultipleTimer 정통(컨테이너+중앙 tick).
    ///        named Playable 보유, gameplay verb → Play(key). 월드점 단발은 Spawns/ 로 위임(트리거만).
    class PlayableDirector : public SJH::Scene::Component, public Entity::IActorPresentation
    {
      public:
        /// @brief 반복 가능한 연출 등록(이동소유). 같은 key 재등록 = 교체.
        PlayableDirector& Register(const std::string& key, std::unique_ptr<SJH::Playable::PlayableBase>);
        void Play(const std::string& key);   // 있으면 Stop()+Play() (재발동). 없으면 silent no-op.
        void Stop(const std::string& key);
        bool Has(const std::string& key) const;

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override;       // 보유 Playable 중 미종료 것 Tick (MultipleTimer 식)

        // === IActorPresentation (sink — gameplay verb → 연출 key) ===
        void ReactDamaged(int) override { Play("hit"); }
        void ReactDied(vmath::vec3 pos) override;     // Play("death") + (월드점 death FX 는 Spawns/ — §4)
        void ReactAttack(vmath::vec2 aim) override;   // Play("attack") + (월드점 muzzle 은 Spawns/)
        void SetFacing(Entity::EFacing) override;     // directional Playable 선택 (분해 Task6 등록분)
        void SetPose(Entity::EPose) override;

      private:
        std::map<std::string, std::unique_ptr<SJH::Playable::PlayableBase>> mPlayables;
        // (facing/pose 선택 상태 + directional 그룹 핸들 — Task6 가 채움)
    };
}
```
- **보유 + 중앙 tick**: `PlayableDirector` 가 Component 라 씬이 그를 tick → `Update` 가 보유 Playable 의 `Update(dt)` 를 직접 호출(MultipleTimer 가 Timer 를 Tick 하듯). 보유 Playable 은 *AddComponent 안 함*(타입충돌 회피 + 중앙관리). FollowOwner 류(GetOwner 필요)는 *월드점*이라 여기 안 들어옴(§4).
- **RD2**: 액터당 PlayableDirector 1개 = 그 액터의 유일 IActorPresentation sink. **분해 Task6 PlayerSpriteDirector 와 동일물**(별도 director 금지).
- **verb→key 매핑은 foundation 이 skeleton**, key 의 *내용*(어떤 Playable)은 PlayerBuilder 가 조립(§8).

## 4. 두 연출 수명 (확정 — Q1: Director 가 Spawns/ 트리거)

| 모드 | 예 | 거주 | 기구 |
|---|---|---|---|
| **엔티티 지속·반복** | 플레이어 피격깜빡/디졸브/facing/비네팅펄스/흑백/Damaged 사운드 | `PlayableDirector`(엔티티 Component) named Playable | `Play(key)` 재발동 |
| **월드점 단발** | 머즐(총구), 사망버스트(적 위치), 히트스파크(탄착) | 기존 `Spawns/`(fxRoot+AutoDespawn) | director 가 verb 시 `SpawnVfxInstance/SpawnAudioInstance/CombatSequences` **호출(트리거)** |
- `ReactDied(pos)`: `Play("death")`(엔티티 디졸브 등) + `Spawns::SpawnEnemyDeathFX(ctx, pos)`(월드점 버스트). director 가 ctx(fxRoot 등) 보유 또는 주입받음.
- `ReactAttack(aim)`: `Play("attack")`(있으면) + 머즐 월드점 spawn.
- **delegate(onDeathFx/SetOnHitFx) 제거**: 이제 sink/director 가 직접 Spawns/ 트리거. (Carrier Task5 의 hit→히트스파크도 director/Spawns 경유.)

## 5. `PostFXRegistry` 싱글턴 (확정 — Q2)

`apps/_MyApp_/src/...PostFXRegistry.{h,cpp}` (또는 Manager 인접, 단 독립).
```cpp
class PostFXRegistry {  // Meyer's 싱글톤
  public:
    static PostFXRegistry& Get();
    void      Register(const std::string& passName, SJH::Material* mat);  // main.cpp init 1회
    SJH::Material* Material(const std::string& passName) const;            // 없으면 nullptr
};
```
- **main.cpp 1줄 등록**(init, FindPassMaterial 결과): `PostFXRegistry::Get().Register("grayscale_vignetting", FindPassMaterial("grayscale_vignetting"));` (Fog 경합 — surgical, PostFX 체인/카메라 라인 미접근).
- **Playable 이 사용**: `PostFXTweenPlayable`(hit-FX) 가 `PostFXRegistry::Get().Material("grayscale_vignetting")->Properties.Floats["uVignetteAmount"] = v` 로 도달. Manager↔render 커플링 회피(독립 싱글톤).
- material 미등록(nullptr)이면 Playable silent no-op(테스트/헤드리스 안전).

## 6. 마이그레이션 맵 (전면 — 사용자 확정 "전부 통합")

| seam | 현재 | → 통합 후 (foundation 작업) |
|---|---|---|
| `onFire`(PlayerBuilder 람다) | Root 밑 ad-hoc Sequence(Effekseer muzzle → Parallel(shot∥slash)) + Play | 플레이어 PlayableDirector `Register("fire", <동일 Composite>)`; PlayerController 발사 시 `dir.Play("fire")`. 머즐은 월드점이면 Spawns/ 트리거 |
| `onDamage`(G키 람다) | Root 밑 ad-hoc Parallel(shake∥Damaged) | `Register("damaged_test", ...)` 또는 ReactDamaged 경유로 흡수 |
| `onDeathFx`(delegate, 미바인딩) | — | `ReactDied`→`Play("death")`+`Spawns::SpawnEnemyDeathFX`. delegate 멤버 제거(enemy_factory/EnemyDeathHandler 은 분해 Task5/7 소유 — *조율*) |
| `SetOnHitFx`(bullet delegate) | — | Carrier(Task5) hit→`Spawns::SpawnHitSpark`. delegate 제거 (Task5 와 조율) |
> ⚠ `onDeathFx`/`SetOnHitFx` 제거는 **분해 Task5(Carrier)/Task7 과 겹치는 파일**(enemy_factory/EnemyDeathHandler/Carrier). foundation 은 *PlayerBuilder onFire/onDamage 마이그레이션 + sink→Spawns 트리거 경로*까지, enemy/bullet delegate 제거는 **Task5 와 순서 조율**(§8 경합).

## 7. 잠긴 분해와의 정합 (RD1~6 보존)

- **RD1/RD3**: `Life::DoDamaged → mSink->ReactDamaged`(추상 seam, 게임로직은 director 모름) 그대로. PlayableDirector = sink 구현체.
- **RD2**: 액터당 sink 1 = PlayableDirector 1. **분해 Task6 의 PlayerSpriteDirector 는 이 PlayableDirector 로 일반화·통합**(별도 만들지 않음).
- **A2**: FX = seam(이제 director/Spawns). 게임플레이 베이스에 연출 슬롯 부착 금지(여전히).
- **확정 4 디테일**(i-frame optional<Timer>·Movement유지·공격윈도0.15s·Carrier얇은베이스) 무관·보존.

## 8. 병렬 분할 (확정: foundation 먼저 → 이후 병렬)

**A. foundation 에이전트 (먼저, 단독)**:
1. `PlayableDirector`(§3) + `PostFXRegistry`(§5) 신설 + CMake 등록.
2. 플레이어 액터에 PlayableDirector 부착(= IActorPresentation sink) — PlayerActor/PlayerBuilder.
3. `onFire`/`onDamage` → PlayableDirector `Register/Play` 마이그레이션(§6).
4. `ReactDied`/`ReactAttack` 의 월드점 Spawns/ 트리거 경로.
5. main.cpp PostFXRegistry 등록 1줄(surgical).
- 산출 = 두 병렬 트랙이 Playable 을 *등록*할 director API + key 규약("hit"/"death"/"fire"/directional).

**B. 이후 병렬 (foundation 위)**:
- **스프라이트 트랙 (= 분해 Task6, 나)**: directional SpriteSequencePlayable 8개 + `HitBlinkPlayable`(enableHit 0.25s) + `DissolvePlayable`(dissolveThreshold ramp) 를 director 에 등록. `SetFacing/SetPose` 선택 로직. PlayerController RD5.
- **PostFX·사운드 트랙 (= hit-FX 에이전트)**: `PostFXTweenPlayable`(uVignetteAmount 0.5→0 0.25s) + 흑백(HP 지속 — `GrayscaleBindPlayable` loop 또는 ReactDamaged 시 갱신) + `FmodStudioPlayable`("Damaged") 를 "hit"/"death" key Composite 에 합성.
- **합성점 = PlayerBuilder**: `dir.Register("hit", ParallelPlayable{ HitBlink(스프라이트) ∥ Vignette(PostFX) ∥ Damaged(사운드) })`. 두 트랙이 각자 Playable *타입* 제공, PlayerBuilder 가 조립(additive). → PlayerBuilder 가 공유 편집점(조율).

**경합 매트릭스**:
| 파일 | foundation | Task6(나) | hit-FX |
|---|---|---|---|
| PlayableDirector/PostFXRegistry (신규) | ✅ 생성 | 등록만 | 등록만 |
| PlayerBuilder.cpp | onFire/onDamage 마이그레이션 | 8그룹+facing 등록+SetIFrame 주입 | "hit" Composite 에 vignette/sound 합성 | → **3자 공유 — 순서: foundation→(Task6∥hitFX)** |
| main.cpp | PostFXRegistry 1줄 | 미접근 | (PostFX 등록은 foundation) | Fog 경합 |
| enemy_factory/EnemyDeathHandler/Carrier | onDeathFx 트리거 경로 | — | — | **Task5 와 조율**(delegate 제거 순서) |
| Life | 미수정(sink 호출 이미 있음) | 미수정 | 미수정 | |

## 9. 검증 / 가드레일 / 컨벤션
- 빌드 `cmake --build --preset ninja --target _MyApp_` exit0. 실행: 좌클릭 발사 FX(머즐+사운드) 가 director 경유로 동일하게 보임(회귀0) + (이후 트랙) 피격 시 비네팅/흑백/깜빡 + 사망 디졸브.
- **Playable 코어/Spawns//셰이더/Timer코어 수정 금지.** Life 수정 금지(sink 호출 이미 존재). **enemy/bullet delegate 제거는 Task5 와 순서 조율**. main.cpp = PostFXRegistry 1줄만(Fog surgical).
- path-scoped 커밋, 무단 커밋 금지, no_auto_tests, 주석 한국어, 헤더가드 `__XXX_H__`, long 금지, 경로 슬래시, Co-Authored-By 미사용.

## 10. 비목표/열린점
- PlayableDirector 의 core 승격(generic named-registry)은 후속(지금 Client). 
- enemy 도 PlayableDirector 부착(피격/사망 연출)은 적 스프라이트 sink 도착 후(현 적은 단일 sprite, sink 없음) — 후속.
- `GrayscaleBindPlayable`(HP 지속 바인딩) vs ReactDamaged 즉시 갱신 — hit-FX 트랙이 §확정(둘 다 director 경유면 철학 충족).

## 11. self-review
- ✅ 철학 충족: 모든 연출 = PlayableDirector(엔티티) 또는 Spawns/(월드점, director 트리거). 산재 직접호출 제거.
- ✅ RD1~6 보존(sink 추상 seam·액터당 1·delegate→seam). PlayerSpriteDirector=PlayableDirector 통합.
- ✅ 기존 자산 재사용(Spawns/, Composite, leaf Playable, MultipleTimer 패턴). 코어 무수정.
- ⚠ PlayerBuilder 3자 공유 → foundation 먼저 후 Task6∥hitFX. enemy/bullet delegate 제거는 Task5 조율.
