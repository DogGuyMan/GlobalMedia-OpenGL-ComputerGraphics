# 다른 Claude Code Agent 용 프롬프트 — PlayableDirector 연출 foundation

> ✅ **완료(2026-06-02) — 이 프롬프트는 실행됨. 결과/현재 상태의 단일 진입점은 [`2026-06-02-playable-director-foundation-DONE-resume-handoff.md`](2026-06-02-playable-director-foundation-DONE-resume-handoff.md).** 다음 슬라이스(Task6)는 [`2026-06-02-task6-on-director-foundation-agent-prompt.md`](2026-06-02-task6-on-director-foundation-agent-prompt.md). 본 문서는 입력 기록(아카이브)으로만 남긴다.

> 아래 코드블록을 새 세션에 그대로 붙여 사용. 자기완결. 정본 spec(참고): `doc/superpowers/specs/2026-06-02-playable-director-foundation-design.md`.
> 이건 **공유 foundation** — 끝나면 두 트랙(스프라이트=PlayerBehavior 분해 Task6 / PostFX·사운드=hit-FX)이 이 위에 Playable 을 등록한다.

---

```
[ROLE]
너는 C++17/CMake OpenGL 탑다운 슈터(/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics, 브랜치 game/module/ingame/temp)의 구현 에이전트다.
철학: "게임 로직(HP/damage/physics/AI/입력판정) 제외, 모든 연출/비주얼/사운드는 PlayableDirector 를 통한 Playable 로만." 너는 그 *공유 foundation* 을 만든다.

[절대 규칙]
- 커밋/git add 금지(사용자 승인 후). 빌드+육안 검증만. 단위테스트 자동추가 금지(no_auto_tests).
- 주석 한국어. 헤더가드 __XXX_H__ (#pragma once 금지). long 금지(고정폭). 경로 슬래시.
- 이 브랜치는 병렬 에이전트 多 — path-scoped 커밋, 무관 dirty(resources/vfx 등) 미접근.
- **코어 수정 금지**: src/playable/(IPlayable/PlayableBase/Composite/Interval), src/timer/, 셰이더(billboard_atlas.fs/grayscale_vignetting.fs) 손대지 마라.
- **Spawns/ 폐기 금지**: OneShotSweeper/AutoDespawnOnFinish/VfxInstance/AudioInstance/CombatSequences 는 월드점 단발의 검증된 기구 — 너는 *트리거*만.
- **이번에 만들지 마라(병렬 트랙 소유)**: 스프라이트 Playable(HitBlink/Dissolve/directional) = PlayerBehavior 분해 Task6 / PostFX·사운드 Playable(Vignette/Grayscale/Damaged) = hit-FX 에이전트. 너는 *그것들이 등록될 director API + key 규약 + 자리*만 만든다.
- **EnemyFactory.h / EnemyDeathHandler / Carrier / Life 수정 금지**: onDeathFx/SetOnHitFx delegate 제거는 분해 Task5/7 소유 — 순서 조율 필요(아래 §C). Life::DoDamaged 는 이미 mSink->ReactDamaged 호출함(수정 불필요).
- **main.cpp**: PostFXRegistry 등록 1줄만(Fog/PostFX 에이전트 경합 — PostFX 체인/카메라/fog 라인 미접근, init 끝에 덧붙이기).

[검증된 사실 — 현 인프라]
- PlayableBase(`src/playable/playable_base.h`) : public IPlayable, public SJH::Scene::Component. Update(float) final → OnUpdate. Play()=재개(paused/finished=false+OnPlay), Stop()=리셋(elapsed_=0+OnStop), IsFinished(). Composite: SequencePlayable.Append / ParallelPlayable.Join (composite_playable.h).
- Playable 은 씬 tick: main.cpp:348 `Director::Get().Update(dt)` 가 root 재귀 → Component.Update. 종료정리 main.cpp:349 `SweepFinishedChildren(*mFxRoot)`.
- 현 onFire/onDamage = PlayerBuilder.cpp:47-94 람다가 Director.Root() 밑 임시 Actor("ShotComposite"/"DamageComposite") + Composite 직접 조립 + Play. leaf: EffekseerPlayable(manager, effect, pos, TrackPolicy)/FmodPlayable(sys, sound)/FmodStudioPlayable(desc, pos?)/TweenPlayable<T>(tween, onStep).
- 월드점 단발 자유함수: Spawns::SpawnVfxInstance(fxParent, vfx, effect, pos) / SpawnAudioInstance(fxParent, desc, pos?) / CombatSequences(SpawnHitSpark/SpawnEnemyDeathFX/SpawnPickupChime)(ctx, pos). ctx=SequenceContext{audio,vfx,reg,world,sceneRoot,fxRoot}.
- PostFX: passComponent->mMaterial->Properties.{Floats|Vec3s|Ints}[key] 직접. main.cpp:477 FindPassMaterial(name). mPassComponents 는 game_application(main.cpp) 소유.
- IActorPresentation(Components.Interfaces.h): ReactDamaged(int)/ReactDied(vec3)/ReactAttack(vec2)/FaceAim(vec2)/SetFacing(EFacing)/SetPose(EPose) — defaulted no-op. Life.mSink 가 OnEnter 에 GetComponent<IActorPresentation>() 캐시, DoDamaged→ReactDamaged / DoDie→ReactDied 호출(이미 있음, 구현체 0).
- MultipleTimer(src/timer/multiple_timer.h): Component + map<string,Timer> + 중앙 Tick — **PlayableDirector 가 따를 패턴.**

========================================================================
[A] PlayableDirector 신설 — apps/_MyApp_/src/Playable/PlayableDirector.{h,cpp}
========================================================================
Component + Entity::IActorPresentation. named Playable 보유 + 중앙 tick(MultipleTimer 식). verb→Play(key).
```cpp
namespace TopdownShooter::Playable
{
    class PlayableDirector : public SJH::Scene::Component, public Entity::IActorPresentation
    {
      public:
        PlayableDirector& Register(const std::string& key, std::unique_ptr<SJH::Playable::PlayableBase> p);
        void Play(const std::string& key);   // 있으면 Stop()+Play(), 없으면 silent no-op
        void Stop(const std::string& key);
        bool Has(const std::string& key) const;

        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override;       // 보유 Playable 중 !IsFinished() 인 것 ->Update(dt) (MultipleTimer 식 중앙 tick)

        void ReactDamaged(int) override { Play("hit"); }
        void ReactDied(vmath::vec3 pos) override;    // Play("death") + 월드점 death FX(§B 트리거) — ctx 주입 필요
        void ReactAttack(vmath::vec2 aim) override;  // Play("attack")(있으면) + 월드점 muzzle(§B)
        void SetFacing(Entity::EFacing f) override;  // 분해 Task6 가 directional 등록 — 지금은 빈 훅(미등록 시 no-op)
        void SetPose(Entity::EPose p) override;

      private:
        std::map<std::string, std::unique_ptr<SJH::Playable::PlayableBase>> mPlayables;
    };
}
```

- 보유 Playable 은 AddComponent 안 하고 director 가 직접 ->Update(dt) (MultipleTimer 가 Timer Tick 하듯). Play(key)=찾아서 Stop()+Play().
- ReactDied/ReactAttack 의 월드점 spawn 을 위해 director 가 Spawns SequenceContext(또는 fxRoot+vfx+audio) 를 주입받는 setter 필요(예: SetSpawnContext(...)). PlayerBuilder 가 주입.
- CMake: apps/_MyApp_/src/Playable/ 에 CMakeLists 있으면 PlayableDirector.cpp 추가, 없으면(헤더온리 dir) 적절 모듈(MyApp::Entity 또는 신규 타겟)에 등록 — 빌드로 확인. SJH::playable(PlayableBase)+SJH::scene+MyApp::Entity(IActorPresentation)+MyApp::Spawns(트리거) link 필요.

========================================================================
[B] PostFXRegistry 싱글턴 — apps/_MyApp_/src/.../PostFXRegistry.{h,cpp}
========================================================================
```cpp
class PostFXRegistry {
  public:
    static PostFXRegistry& Get();                 // Meyer's
    void Register(const std::string& passName, SJH::Material* mat);
    SJH::Material* Material(const std::string& passName) const;  // 없으면 nullptr
  private:
    std::map<std::string, SJH::Material*> mMats;
};
```
- main.cpp init(FindPassMaterial 사용 가능 지점)에 1줄: PostFXRegistry::Get().Register("grayscale_vignetting", FindPassMaterial("grayscale_vignetting")); (필요시 "fog" 등도). **PostFX 체인/카메라/fog 셰이더 라인 미접근, 등록 줄만 덧붙이기.**
- 이후 hit-FX 트랙의 PostFXTweenPlayable 이 PostFXRegistry::Get().Material(name)->Properties.Floats[...] 로 도달(너가 만드는 건 registry 까지).

========================================================================
[C] 기존 seam 마이그레이션 (너의 범위 = onFire/onDamage + 플레이어 sink 부착)
========================================================================
1. **PlayerActor/PlayerBuilder**: 플레이어 액터에 PlayableDirector 부착(= IActorPresentation sink). Life.OnEnter 의 GetComponent<IActorPresentation>() 가 이걸 잡도록(액터당 1개).
2. **onFire 마이그레이션**: PlayerBuilder.cpp:47-73 의 ad-hoc 조립을 → PlayableDirector.Register("fire", <기존과 동일한 Effekseer muzzle → Parallel(shot∥slash) Sequence>) 로 옮기고, PlayerController 의 발사 콜백(onFire)이 dir.Play("fire") 하도록. (또는 ReactAttack 경유.) 동작 동일(회귀0) 확인.
3. **onDamage(G키)**: PlayerBuilder.cpp:75-94 를 Register("damaged_test", <기존 Parallel(shake∥Damaged)>)+Play 로. (테스트용 — 유지 또는 ReactDamaged 로 흡수.)
4. **ReactDied/ReactAttack 의 월드점**: director 가 주입받은 ctx 로 Spawns::SpawnEnemyDeathFX/SpawnHitSpark/머즐 트리거.
- **⚠ onDeathFx/SetOnHitFx delegate 제거(EnemyFactory/EnemyDeathHandler/Carrier)는 하지 마라** — 분해 Task5/7 이 그 파일을 소유. 너는 *플레이어 onFire/onDamage 마이그레이션 + sink→Spawns 트리거 경로*까지. enemy/bullet 쪽은 "후속(Task5 조율)" 으로 남겨 보고.

========================================================================
[검증]
========================================================================
- 빌드 exit0. 실행: 좌클릭 발사 시 머즐 이펙트+사운드가 **이전과 동일하게** 보임(director 경유로 옮겼어도 회귀0). G키 동작 유지.
- (이후 트랙이 hit/death/directional Playable 을 등록해야 피격/사망/방향 연출이 보임 — 본 작업 범위 아님.)

[Self-review]
- PlayableDirector(Component+IActorPresentation, named Play/Stop/Register, 중앙 tick) + PostFXRegistry(싱글톤) 신설?
- 플레이어에 PlayableDirector 1개 부착 = sink? onFire/onDamage 가 director 경유로 마이그레이션 + 회귀0?
- 코어(playable/timer)/셰이더/Spawns//EnemyFactory/Carrier/Life 무수정? main.cpp 는 PostFXRegistry 1줄만?
- 스프라이트/PostFX Playable 은 안 만들었는가(병렬 트랙 소유)? key 규약("hit"/"death"/"fire"/directional)만 마련?

[보고]
DONE/DONE_WITH_CONCERNS/BLOCKED + 변경 파일별 요약 + 빌드 마지막 줄 + 실행(발사 FX 회귀 확인) + enemy/bullet delegate 미처리(Task5 조율) 명시 + git status. 커밋 금지.
```

---

## 사용 메모 (오케스트레이터/사용자용)
- 이건 **공유 foundation** — 끝나면 (1) 스프라이트 트랙=PlayerBehavior 분해 Task6(directional/HitBlink/Dissolve 등록) (2) PostFX·사운드 트랙=hit-FX(Vignette/Grayscale/Damaged 를 "hit"/"death" Composite 합성) 가 병렬.
- **공유 편집점 = PlayerBuilder.cpp**(3자) → 순서: foundation → (Task6 ∥ hit-FX). **enemy/bullet delegate 제거는 분해 Task5 와 조율**(이 foundation 은 손대지 않음).
- 커밋 메시지(권장): `[feat] : PlayableDirector 연출 foundation + PostFXRegistry — onFire/onDamage 마이그레이션`.
