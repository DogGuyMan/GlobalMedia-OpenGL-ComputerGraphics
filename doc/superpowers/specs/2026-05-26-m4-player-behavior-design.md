# M4 — Player Behavior + Playable Integration + Enemy + Wave

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

**날짜:** 2026-05-26  
**범위:** SpriteSequencePlayable 다중 클립 + IntervalPlayable 신설 → PlayerBehavior flat 함수 + Bullet + Enemy + WaveController 완성.

---

## 개요

M5 완료로 leaf Playable(EffekseerPlayable/FmodStudioPlayable/TweenPlayable)이 클라이언트에 정착했다.  
M4 는 이것을 "조립"해 게임 루프를 완성한다.

- 결정 수: 11
- 엔진 변경: `SJH::playable` + `SJH::sprite`
- 클라이언트 신규: Entity/Player + Entity/Bullet + Entity/Enemy + Stage/WaveController

---

## 확정 결정

### D1 — Player FSM 없음
`StateMachine<PlayerState, PlayerActor>` 미사용.  
Idle/Move/Attack/Hit/Dash/Die 는 `PlayerBehavior : PlayableBase` Component 의 **flat 메서드**.  
`SJH::fsm` 은 Stage/Enemy 용도 보존.

### D2 — SpriteSequencePlayable 다중 클립 확장
기존 단일 `clip_` (하위 호환 유지) 에 추가:
```
RegisterClip(int clipIdx, const SpriteFrameClip*)   → 클립 등록
RegisterOnClipEnter(int clipIdx, IPlayable*)         → 클립 진입 시 Play() 호출 슬롯
PlayClip(int clipIdx)                                → 전환 + elapsed_ 리셋 + 슬롯 Play
int CurrentClip() const
```
`SJH::sprite` 는 이미 `SJH::playable` PUBLIC 링크 → `IPlayable*` 사용 CMake 변경 없음.

### D3 — IntervalPlayable 신설 + AppendInterval ✅ 완료 (2026-05-26)
`SJH::playable` 에 `IntervalPlayable : PlayableBase` 추가 (duration 초 대기 → finished_=true).  
`SequencePlayable::AppendInterval(float seconds)` = `Append(make_unique<IntervalPlayable>(s))`.  
DOTween `seq.AppendInterval(N)` 정통.  
**구현 완료**: `src/playable/interval_playable.{h,cpp}` + `composite_playable.{h,cpp}` + CMakeLists 수정.

### D4 — Attack Playable 체인
PlayerBehavior 초기화 시 조립, Attack 클립 진입 시 Play():
```
ParallelPlayable {
  EffekseerPlayable(centerEffect)          ← 즉시
  FmodStudioPlayable(attackAudio)          ← 즉시
  SequencePlayable {
    AppendInterval(0.3f)                   ← 풍업 딜레이 (D3)
    BulletSpawnPlayable(owner, sceneRoot, factory)
  }
}
```
나머지 클립 체인:
- Hit:  `ParallelPlayable { EffekseerPlayable(hitFx) ∥ FmodStudioPlayable(hitSfx) }`
- Dash: `ParallelPlayable { EffekseerPlayable(dashFx) ∥ FmodStudioPlayable(dashSfx) }`
- Die:  `ParallelPlayable { EffekseerPlayable(dieFx)  ∥ FmodStudioPlayable(dieSfx) }`
- Move: EffekseerPlayable(footFx) — 클립 진입 시 Play, 다른 클립 진입 시 Stop (D10)

### D5 — Bullet Actor (compound, 소멸 없는 비활성화)
`CreateBulletActor(world, pos, dir, speed, damage, lifetime)` 헤더 온리 팩토리.
```
Components:
  CircleBody (kinematic, BulletPlayer category)
  BulletContactHandler : SJH::Scene::Component, Physics::IContactable
  BulletLifetime : SJH::Scene::Component
```
**소멸 방식**: Box2D step 내부 + Actor::Update 순회 중 RemoveChild 금지 →  
대신 `GetOwner()->SetActive(false)` 로 비활성화만 (ghost b2Body 허용 — M4 한정).  
`BulletContactHandler::mAlive` 가드로 중복 데미지 방지.

### D6 — Enemy Actor
`CreateEnemyActor(cfg, playerTarget*)` 헤더 온리 팩토리.
```
Components:
  CircleBody (dynamic, Enemy category)
  Components::Life(hp)
  SimplePursueAI : SJH::Scene::Component
  EnemyContactHandler : SJH::Scene::Component, Physics::IContactable
```
`SimplePursueAI::Update()`: Life.IsAlive() 가드 + b2Body velocity → playerTarget 방향.  
EnemyContactHandler: `OnCollisionEnter(other)` → `other->GetComponent<PlayerBehavior>()->Hit(damage)`.

### D7 — WaveController ⛔ M4 제외 → M6/M7 위임
Stage FSM(M7) 도입 후 자연스럽게 합류.  
M4 에서는 `startup()` 에서 수동으로 Enemy 1~2마리 spawn 하는 것으로 대체.  
(WaveController 설계 상세는 본 §의 API 섹션에 참고용으로 보존)

### D8 — PlayerBehavior Component
`Entity/Player/PlayerBehavior.{h,cpp}` — `PlayableBase` 상속 (Composite 자식으로 쓰는 경우 없음 — 실질은 Component + 상태 보유).
```cpp
void Init(SpriteSequencePlayable*, Actor* self, b2World*, int bulletDmg);
void SetSceneRoot(Actor*);           // bullet 스폰 대상 (root 혹은 Stage)
void Idle();
void Move(vmath::vec2 vel);
void Attack(vmath::vec2 dir);        // dir 저장 + PlayClip(Attack)
void Hit(int damage);                // invincibility 체크 + Life.DoDamaged + PlayClip(Hit)
void Dash(vmath::vec2 dir);          // cooldown 체크 + speed 배율 + PlayClip(Dash)
void Die();                          // SetActive(false) + PlayClip(Die)
void SetAttackPlayable(IPlayable*);  // D4 chain 등록
void SetHitPlayable(IPlayable*);
void SetDashPlayable(IPlayable*);
void SetDiePlayable(IPlayable*);
void SetMoveEffect(IPlayable*);      // 발 파티클
vmath::vec2 GetAttackDirection() const;
bool IsAlive() const;
bool IsDashing() const;
// internal: dash timer(0.3s), cooldown(0.8s), invincibility timer(0.5s)
```

### D9 — EPlayerClip enum
```cpp
enum class EPlayerClip : int {
    Idle   = 0,   // frames 0–2   (3프레임, loop)
    Move   = 1,   // frames 3–5   (3프레임, loop)
    Attack = 2,   // frames 6–8   (3프레임, no-loop → 종료 시 Idle 복귀)
    Hit    = 3,   // frames 9–10  (2프레임, no-loop → 종료 시 Idle 복귀)
    Die    = 4,   // frames 11–13 (3프레임, no-loop, 종료 유지)
    Dash   = 5,   // frames 14–15 (2프레임, loop, 지속 입력 시 유지)
};
```
TestPattern 4×4 (16프레임) 플레이스홀더 배분.  
실제 아트 시트 교체 시 `SpriteFrameClip` startFrame/frameCount 값만 변경 — enum 불변.

### D10 — Move 발 파티클 특수 처리
Move 클립 진입 시 `mMoveEffect->Play()`. 다른 클립 진입 시 `mMoveEffect->Stop()`.  
`PlayerBehavior::PlayClip(EPlayerClip clip)` 내부에서 처리:
```cpp
if (clip != EPlayerClip::Move && mMoveEffect) mMoveEffect->Stop();
if (clip == EPlayerClip::Move && mMoveEffect) mMoveEffect->Play();
```

### D11 — 순환 의존 회피 (Entity ↔ Physics)
`MyApp::Physics` PRIVATE → `MyApp::Entity` 이미 존재. 역방향 링크 금지.  
해결: `bullet_factory.h` / `enemy_factory.h` 헤더 온리 팩토리 (main.cpp 에서 include) →  
Entity STATIC .cpp 파일은 b2Body 접근에 `game_deps` (box2d 헤더) PRIVATE 추가.  
`BulletSpawnPlayable` 은 physics 미의존 팩토리 **delegate** `std::function<Actor::UPtr(vec2,vec2)>` 수신.

---

## 모듈 의존 변경 (추가분)

```
Engine:
  SJH::playable  += interval_playable.{h,cpp}
                  += composite_playable.h (AppendInterval 선언)
                  += composite_playable.cpp (AppendInterval 구현)
  SJH::sprite    += sprite_sequence_playable.{h,cpp} (multi-clip + OnClipEnter)

Client (MyApp::Entity):
  Entity/Player/ += PlayerBehavior.{h,cpp}
                 += BulletSpawnPlayable.{h,cpp}
  Entity/Bullet/ += BulletContactHandler.{h,cpp}
                 += BulletLifetime.h                   (헤더 온리 Component)
                 += bullet_factory.h                   (헤더 온리 팩토리)
  Entity/Enemy/  += SimplePursueAI.{h,cpp}
                 += EnemyContactHandler.{h,cpp}
                 += enemy_factory.h                    (헤더 온리 팩토리)
  Entity/CMakeLists.txt  += 5개 .cpp + SJH::playable PUBLIC + SJH::sprite/game_deps PRIVATE

Client (MyApp::Stage):
  Stage/         += WaveController.{h,cpp}
  Stage/CMakeLists.txt += WaveController.cpp + MyApp::Entity PUBLIC
```

---

## 모듈 책임 표

| 파일 | 책임 |
|---|---|
| `src/playable/interval_playable.{h,cpp}` | 고정 시간 대기 leaf Playable |
| `src/playable/composite_playable.{h,cpp}` | `AppendInterval` 추가 |
| `src/sprite/sprite_sequence_playable.{h,cpp}` | multi-clip + `RegisterOnClipEnter` |
| `Entity/Player/PlayerBehavior.{h,cpp}` | flat 행동 + clip 전환 + Playable wiring |
| `Entity/Player/BulletSpawnPlayable.{h,cpp}` | delegate 호출로 Bullet Actor 스폰 leaf |
| `apps/_MyApp_/src/Entity/Bullet/bullet_factory.h` | `CreateBulletActor` 인라인 팩토리 |
| `Entity/Bullet/BulletContactHandler.{h,cpp}` | `IContactable` — 데미지 + SetActive(false) |
| `apps/_MyApp_/src/Entity/Bullet/BulletLifetime.h` | 수명 타이머 → SetActive(false) |
| `<Entity>/Enemy/enemy_factory.h` | `CreateEnemyActor` 인라인 팩토리 |
| `Entity/Enemy/SimplePursueAI.{h,cpp}` | b2Body velocity 추적 Component |
| `Entity/Enemy/EnemyContactHandler.{h,cpp}` | `IContactable` — PlayerBehavior::Hit 전달 |
| `Stage/WaveController.{h,cpp}` | 웨이브 타이머 + Enemy spawn + 전멸 감지 |
| `Entity/CMakeLists.txt` | 신규 .cpp + deps 추가 |
| `Stage/CMakeLists.txt` | WaveController.cpp + MyApp::Entity 추가 |
| `apps/_MyApp_/main.cpp` | PlayerBehavior Init + 클립 등록 + Playable 조립 + WaveController |

---

## 핵심 API 시그니처

### IntervalPlayable
```cpp
namespace SJH::Playable {
    class IntervalPlayable : public PlayableBase {
    public:
        explicit IntervalPlayable(float duration);
        ~IntervalPlayable() override;
    protected:
        void OnUpdate(float dt) override; // elapsed_ >= duration_ → finished_ = true
    private:
        float duration_;
    };
}
```

### SequencePlayable::AppendInterval
```cpp
// composite_playable.h 추가:
SequencePlayable& AppendInterval(float seconds);
// 구현: return Append(std::make_unique<IntervalPlayable>(seconds));
```

### SpriteSequencePlayable 확장 멤버
```cpp
// 공개 추가:
SpriteSequencePlayable& RegisterClip(int clipIdx, const SpriteFrameClip* clip);
SpriteSequencePlayable& RegisterOnClipEnter(int clipIdx, IPlayable* sideEffect);
void PlayClip(int clipIdx);
int  CurrentClip() const { return currentClipIdx_; }

// 비공개 추가:
std::unordered_map<int, const SpriteFrameClip*>        clips_;
std::unordered_map<int, std::vector<IPlayable*>>       onClipEnter_;
int                                                    currentClipIdx_ = 0;
```

### PlayerBehavior
```cpp
namespace TopdownShooter::Entity::Player {
using UActorPtr = std::unique_ptr<SJH::Scene::Actor>;
namespace SprSeq = SJH::SpriteSequence;

class PlayerBehavior : public SJH::Scene::Component {
public:
    PlayerBehavior()  = default;
    ~PlayerBehavior() override;

    void Init(SprSeq::SpriteSequencePlayable* seq,
              SJH::Scene::Actor* self,
              float dashSpeed, float normalSpeed,
              float dashDuration = 0.3f,
              float dashCooldown = 0.8f,
              float hitInvincibility = 0.5f);
    void SetSceneRoot(SJH::Scene::Actor* root);
    void SetMoveBody(b2Body* body);  // for Dash speed boost

    // Playable 슬롯 등록 (main.cpp 가 조립 후 주입)
    void SetAttackPlayable(SJH::Playable::IPlayable* p);
    void SetHitPlayable(SJH::Playable::IPlayable* p);
    void SetDashPlayable(SJH::Playable::IPlayable* p);
    void SetDiePlayable(SJH::Playable::IPlayable* p);
    void SetMoveEffect(SJH::Playable::IPlayable* p);

    // Flat 행동 API
    void Idle();
    void Move(vmath::vec2 vel);
    void Attack(vmath::vec2 dir);
    void Hit(int damage);
    void Dash(vmath::vec2 dir);
    void Die();

    vmath::vec2 GetAttackDirection() const { return mAttackDir; }
    bool IsAlive() const;
    bool IsDashing() const { return mDashTimer > 0.0f; }

    // SJH::Scene::Component hooks
    void OnEnter() override {}
    void OnExit()  override {}
    void Update(float dt) override;

private:
    void PlayClip(int clipIdx);

    SprSeq::SpriteSequencePlayable* mSpriteSeq = nullptr;
    SJH::Scene::Actor* mSelf = nullptr;
    SJH::Scene::Actor* mSceneRoot = nullptr;
    b2Body* mMoveBody = nullptr;

    SJH::Playable::IPlayable* mAttackPlayable = nullptr;
    SJH::Playable::IPlayable* mHitPlayable    = nullptr;
    SJH::Playable::IPlayable* mDashPlayable   = nullptr;
    SJH::Playable::IPlayable* mDiePlayable    = nullptr;
    SJH::Playable::IPlayable* mMoveEffect     = nullptr;

    vmath::vec2 mAttackDir    = vmath::vec2(0.0f, -1.0f);
    float mNormalSpeed        = 3.0f;
    float mDashSpeed          = 7.5f;
    float mDashDuration       = 0.3f;
    float mDashCooldown       = 0.8f;
    float mHitInvincibility   = 0.5f;
    float mDashTimer          = 0.0f;
    float mDashCooldownTimer  = 0.0f;
    float mInvincibilityTimer = 0.0f;
    bool  mDead               = false;
};
}
```

### BulletSpawnPlayable
```cpp
namespace TopdownShooter::Entity::Player {
class BulletSpawnPlayable : public SJH::Playable::PlayableBase {
public:
    using BulletFactory = std::function<std::unique_ptr<SJH::Scene::Actor>(vmath::vec2 pos, vmath::vec2 dir)>;

    BulletSpawnPlayable(PlayerBehavior* behavior, SJH::Scene::Actor* sceneRoot, BulletFactory factory);
    ~BulletSpawnPlayable() override;

    void OnEnter() override {}
    void OnExit()  override {}

protected:
    void OnPlay()   override;  // spawn bullet + finished_ = true
    void OnUpdate(float) override {}

private:
    PlayerBehavior*    mBehavior;
    SJH::Scene::Actor* mSceneRoot;
    BulletFactory      mFactory;
};
}
```

### CreateBulletActor
```cpp
// apps/_MyApp_/src/Entity/Bullet/bullet_factory.h — 헤더 온리
struct BulletConfig {
    b2World*    world;
    vmath::vec2 pos;
    vmath::vec2 dir;       // normalized
    float       speed     = 15.0f;
    int         damage    = 10;
    float       lifetime  = 3.0f;
};
inline std::unique_ptr<SJH::Scene::Actor> CreateBulletActor(const BulletConfig& cfg);
```

### CreateEnemyActor
```cpp
// <Entity>/Enemy/enemy_factory.h — 헤더 온리
struct EnemyConfig {
    b2World*           world;
    vmath::vec2        pos;
    SJH::Scene::Actor* playerTarget;
    int   hp     = 30;
    float speed  = 2.0f;
    int   damage = 10;
};
inline std::unique_ptr<SJH::Scene::Actor> CreateEnemyActor(const EnemyConfig& cfg);
```

### WaveController
```cpp
namespace TopdownShooter::Stage {
class WaveController : public SJH::Scene::Component {
public:
    WaveController(b2World* world, SJH::Scene::Actor* spawnParent,
                   SJH::Scene::Actor* playerActor, float arenaHalfExtent);
    ~WaveController() override;

    void OnEnter() override {}
    void OnExit()  override {}
    void Update(float dt) override;

private:
    void SpawnEnemy();
    vmath::vec2 RandomEdgePos() const;
    int LiveCount() const;

    b2World*           mWorld;
    SJH::Scene::Actor* mSpawnParent;
    SJH::Scene::Actor* mPlayerActor;
    float              mArenaHalfExtent;

    std::vector<SJH::Scene::Actor*> mEnemies;
    int   mWave          = 0;
    float mSpawnTimer    = 0.0f;

    static constexpr float kSpawnInterval = 3.0f;
    static constexpr int   kMaxEnemies    = 5;
};
}
```

---

## 작업 순서 (13 스텝)

| # | 대상 | 내용 |
|---|---|---|
| A | `SJH::playable` | IntervalPlayable + AppendInterval |
| B | `SJH::sprite` | SpriteSequencePlayable multi-clip |
| C | `Entity/Player/` | PlayerBehavior + EPlayerClip |
| D | `Entity/Player/` | BulletSpawnPlayable |
| E | `Entity/Bullet/` | BulletContactHandler + BulletLifetime + bullet_factory |
| F | `Entity/Enemy/` | SimplePursueAI + EnemyContactHandler + enemy_factory |
| G | `Stage/` | WaveController |
| H | `Entity/CMakeLists.txt` | .cpp 추가 + deps |
| I | `Stage/CMakeLists.txt` | WaveController.cpp + MyApp::Entity |
| J | `main.cpp` | PlayerBehavior Init + clip 등록 + Playable 조립 |
| K | `main.cpp` | WaveController AddComponent + onKey Dash + onMouseButton Attack |
| L | Build | cmake + ninja |
| M | 시각 회귀 | 체크리스트 |

---

## 시각 회귀 체크리스트

- [ ] 기존 Idle 애니메이션 Loop 유지 (TestPattern 0-15 sweep 대신 Idle 클립 0-3 loop)
- [ ] WASD 이동 시 Move 클립(4-7) 전환 확인
- [ ] 마우스 클릭 시 Attack 클립(8-11) 전환 + 0.3s 후 총알 생성 확인
- [ ] 총알이 Enemy 에 닿으면 Enemy SetActive(false) 확인
- [ ] Enemy 가 Player 에 닿으면 Hit 클립(12-15) 전환 확인
- [ ] 3초마다 Enemy 스폰 (최대 5마리) 확인
- [ ] Shift 키 Dash: Move 클립 + 가속 확인
- [ ] 빌드 경고 0 (Debug 기준, MSVC narrowing 무시)
