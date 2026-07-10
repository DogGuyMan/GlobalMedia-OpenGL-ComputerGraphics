# 핸드오프 프롬프트 — SJH::Timer 마이그레이션 + Entity FX (다른 Claude Code 용)

> 아래 코드블록 전체를 새 Claude Code 세션에 그대로 붙여 사용. 자기완결 — 별도 문서 안 읽어도 이어받을 수 있다.
> 직전 세션이 (A) Entity 피격/사망 FX 엔진 레이어[커밋됨]와 (B) `BulletLifetime`·`Life`의 `SJH::Timer` 마이그레이션[미커밋]을 완료한 직후의 컨텍스트다.

---

```
[ROLE]
너는 C++17/CMake OpenGL 탑다운 슈터 프로젝트(/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics, 브랜치 game/module/ingame/temp)의 구현 에이전트다.
직전 세션이 끝낸 작업을 이어받는다. 아래 §1(완료)·§2(설계 결정)·§3(절대 규칙)을 먼저 읽고, §5(다음 작업)을 진행하라.

[절대 규칙]
- 커밋/git add 금지 (사용자 리뷰 후 직접 커밋). 구현+빌드 검증 후 보고만.
- 단위 테스트/TDD 금지 (프로젝트 no_auto_tests 정책). 검증 = 빌드 + 육안.
- 주석 한국어. #pragma once 금지(헤더가드 __..._H__ 또는 _..._ 유지). long 금지(고정폭 int32_t 등). 경로 슬래시 /.
- main.cpp 절대 수정 금지 (다른 워커가 Fog/PostFX 리팩토링 중 — 경합).
- 이 브랜치는 혼자가 아니다(§6). 항상 path-scoped 커밋(git add <구체경로>만, -A/. 금지).

========================================================================
[§1] 이미 완료된 것
========================================================================

### 1.1 커밋됨 — 1a9196c "[dev] : entity hit, dissolve"
Entity 피격/사망 FX 의 *엔진 레이어(Layer A+B)*:
- src/sprite/sprite_component.{h,cpp} — SpriteRenderer 에 FX uniform 송신 추가.
  신규 멤버: enableHit / enableDissolve / dissolveThreshold / dissolveOutlineThickness /
  dissolveOutlineColor / dissolveTex(const SJH::Texture*) + private mEffectClock.
  Update(dt) 에서 uniform 7개 송신: uEnableHit, uTime, uEnableDissolve, uDissolveThreshold,
  uDissolveOutlineThickness, uDissolveOutlineColor, (dissolveTex 있을 때만) uDissolveTex(unit=1, uAtlas=unit0).
  기본값 false → 셰이더 분기 skip → 기존 비주얼 회귀 0.
- 셰이더 billboard_atlas.fs 는 이미 완성(uniform 선언/구현 존재). *수정 금지*.
- ⚠ FX의 "구동"(피격→깜빡임 on/off, 사망→디졸브 진행)은 아직 없다.
  그건 PlayerBehavior god-component 분해의 **Task 6 (PlayerSpriteDirector)** 소유다 — 너는 만들지 마라.
  현재 상태 = "uniform 배선만 됨, 셰이더가 uniform 에 반응함"까지.

### 1.2 미커밋 (현재 working tree) — SJH::Timer 마이그레이션
`apps/_MyApp_` 의 손수 굴리던 시간 누산기(`-= dt`/`+= dt`)를 코어 모듈 SJH::Timer 로 옮겼다.
대상 정본 핸드오프: doc/handoffs/2026-06-01/2026-06-01-timer-migration-agent-prompt.md (인벤토리 §2 참고).

- **Phase 1 — apps/_MyApp_/src/Entity/Bullet/BulletLifetime.h** (미커밋)
  float mLifetime/mElapsed → `SJH::Timer::Timer mTimer{lifetime}` 자가 보유(패턴 A).
  Update: mTimer.Tick(dt); if(mTimer.IsTimesUp() && GetOwner()) SetActive(false).
  카운트업→카운트업이라 §5.1 역전 함정 없음. lifetime(cfg 기본 3.0f)은 항상 양수.

- **Phase 2 — apps/_MyApp_/src/Entity/Components/LifeComponents.h** (미커밋, 1a9196c의 사망지연 코드를 대체)
  i-frame/사망지연을 `std::optional<SJH::Timer::Timer>` 2개로:
    std::optional<SJH::Timer::Timer> mInvincibleTimer;  // i-frame    (nullopt = 무적 없음)
    std::optional<SJH::Timer::Timer> mDieTimer;         // 사망 연출 지연 (nullopt = 즉시)
  helper: static ArmInactive(slot, s) — s>0 이면 emplace(s)+Tick(s)로 *finished(비활성)* 장전, s<=0 이면 reset().
  IsInvincible() = mInvincibleTimer && !mInvincibleTimer->IsTimesUp().
  DoDamaged: if(mInvincibleTimer) mInvincibleTimer->Reset();  // passed=0 → 무적 발동 (없으면 no-op)
  DoDie:     mDieTimer ? mDieTimer->Reset() : SetActive(false);  // 지연 or 즉시
  Update:    mInvincibleTimer Tick + (mDeathFxFired && mDieTimer)면 mDieTimer Tick→만료 시 SetActive.
  ★ 제거된 것: float mIFrameSeconds / float mInvincibleTimer / float mDeathDelaySeconds /
    bool mDying / float mDeathTimer / armed bool / accel 역수 트릭 — 전부 사라짐.
    시간 상태는 optional<Timer> 2개로 완전 응집.

- **src/timer/timer.h — 원복(변경 없음)**. 직전에 누군가 임의로 넣었던 mIsInitialized/SetUp()
  (2단계 초기화)를 git restore 로 제거했다. *Timer 코어는 건드리지 마라*(§2.1).

빌드 검증 완료: `cmake --build --preset ninja --target _MyApp_` → [11/11] Linking executable (exit 0).

========================================================================
[§2] 핵심 설계 결정 (변경 금지 — 직전 세션에서 사용자와 확정)
========================================================================

2.1 **Timer 코어(src/timer/timer.h)는 순수 Value Object — 불가침.**
    baseTime 은 ctor 고정·assert(>0)·const(재대입 불가)다. 이건 *의도된 제약*이지 버그가 아니다.
    "비활성/미설정/0길이"를 Timer 내부 플래그(mIsInitialized 같은)나 Pause(blocked) 오버로드로
    표현하지 마라 — 2단계 초기화 안티패턴 + 의미 혼선. clean-ddd 검토로 거부됨.

2.2 **"타이머 부재(없음)" = optional<Timer> 의 nullopt** (DDD Value Object 정통).
    "i-frame 없는 적"은 Timer 에 비활성 플래그를 다는 게 아니라 Timer 자체가 nullopt.

2.3 **i-frame/사망지연은 현재 둘 다 미사용(설정처 0)**.
    적(EnemyFactory)·플레이어(PlayerActor) 모두 Life(hp) 만 호출 → 두 optional 모두 nullopt →
    무적 없음/즉시 사망 = 기존 동작. SetIFrameSeconds/SetDeathDelaySeconds 호출처 0.
    → 미래 Task 6 이 플레이어 Life 에 SetDeathDelaySeconds(0.5) 주입 예정(사망 디졸브 0.5초). 본 설계는 그와 호환.

2.4 **패턴 A(자가 보유) 채택, MultipleTimer 미사용**. Player 다중 타이머(MultipleTimer)는
    PlayerBehavior 분해(철거) 이후 후속 컴포넌트에서 검토.

2.5 **길이 변경은 ctor/setter(emplace) 로만**. 게임 도중 동적 길이 변경은 없다(확정).
    만약 필요해지면 그때 acceleration 또는 재 emplace 로 대응(코어 수정 아님).

========================================================================
[§3] 검증
========================================================================
cmake --preset ninja                                   # (최초만)
cmake --build --preset ninja --target _MyApp_          # 기대: [N/N] Linking executable, exit 0
cd build_ninja/apps/_MyApp_ && ./_MyApp_               # 실행 (리소스 상대경로 → cd 필수)
헤더 단독: c++ -std=c++17 -I src -I include -fsyntax-only <헤더>
실행 관찰: 총알이 수명(3초)대로 사라지는가 / (i-frame·사망지연은 현재 미사용이라 시각변화 없음 — 정상)

========================================================================
[§4] 회귀 안전성 (왜 동작이 안 바뀌는지)
========================================================================
- BulletLifetime: Timer(3.0f) 카운트업 → 기존 mElapsed 카운트업과 동일. timer.h 원복으로 Tick 정상.
- Life 미설정(nullopt): IsInvincible=false, DoDamaged Reset no-op, DoDie 즉시 SetActive = 기존 동일.
- Life 설정(미래 0.5): emplace+finished → DoDie서 Reset → Update가 0.5초 후 SetActive.

========================================================================
[§5] 다음 작업 (우선순위)
========================================================================
1. (사용자 결정) 위 §1.2 미커밋 2파일 path-scoped 커밋.
   제안: `refactor(timer): BulletLifetime + Life(i-frame/사망지연) SJH::Timer 마이그레이션`
   (Co-Authored-By 미사용 — 프로젝트 컨벤션. git add 는 그 2파일 경로만.)
2. (경합 해소 후) #4 WaveController — Stage/WaveController.cpp:77 의 mSpawnTimer += dt 스폰 케이던스를
   Timer + SetInterval(period) + PollInterval() 로. ⚠ 별도 워커(doc/handoffs/2026-06-01/2026-06-01-wave-controller-wiring-agent-prompt.md)와
   경합 — git status 로 WaveController.* 가 누군가 잡고 있는지 확인 후, 끝났으면 진행.
   PollInterval 계약: 한 Tick 에 경계 N개 넘어도 N프레임에 1회씩 이연(합산 아님).
3. (다른 워커 영역, 건드리지 말 것) #5 PlayerBehavior 의 타이머 3종 → 철거 예정(god-component 분해).
   분해 Task 6(PlayerSpriteDirector)이 §1.1 FX 를 구동 + 플레이어 Life 에 SetDeathDelaySeconds 주입.
4. (대상 아님) Stage/Components/MaterialTimeComponent.h 의 mElapsed → 무한 자유 클럭(baseTime 없음), Timer 부적합.

========================================================================
[§6] 병렬 워커 경합 — 이 브랜치는 혼자가 아니다
========================================================================
- 동시에 다른 Claude 에이전트들이 작업 중(PlayerBehavior 분해, EnemyBuilder, Effekseer 진단, WaveController).
  working tree dirty 목록과 HEAD 가 수시로 바뀐다. 작업 전 git status / git log 최신 확인.
- 미커밋 무관 변경(main.cpp staged, Bootstrap/Stage CMakeLists.txt, doc/handoff/*.md 등)이 섞여 있다 — 건드리지 마라.
- PlayerBehavior.{h,cpp} 절대 수정 금지(철거 예정). main.cpp 절대 수정 금지(경합).

========================================================================
[§7] 함정 (코드 만지기 전 필독)
========================================================================
- SJH::Timer::Timer: baseTime ctor 고정·assert(>0)·const(재대입 불가). 0/비활성은 optional nullopt 로.
- Timer 는 카운트업(0→base). IsTimesUp()=passed>=base. 재시작=Reset()(baseTime 유지). 매 프레임 Tick(dt) 필요.
- 코드베이스에 SetUp() 호출이 보여도 그건 PlayerController / ActorFolower 의 것 — Timer 의 SetUp 아님(Timer엔 SetUp 없음).
- optional<Timer> 는 emplace/reset/has_value/operator-> 만 사용(Timer 는 const 멤버라 copy-assign 불가, 끼리 대입 금지).
- 들여쓰기: src/sprite/* 와 BulletLifetime.h 는 스페이스, LifeComponents.h 는 탭 — 파일별로 일치시켜라.

[보고]
DONE / DONE_WITH_CONCERNS / BLOCKED + 변경 요약(file별) + 빌드 마지막 줄 + git status(네 파일 명시). 커밋하지 마라.
```

---

## 사용 메모 (오케스트레이터/사용자용)

- 이 핸드오프는 **직전 세션 = Timer 마이그레이션 Phase 1+2 완료(미커밋)** 시점의 컨텍스트다.
- 가장 먼저 할 일은 보통 **§1.2 미커밋 2파일 커밋 결정**. 그 후 §5의 #4(WaveController, 경합 해소 후) 또는 PlayerBehavior 분해 재개.
- 핵심 불변식: **Timer 코어 불가침(§2.1) + 부재=optional(§2.2)**. 이걸 어기면 직전 세션이 거부한 mIsInitialized 안티패턴으로 회귀한다.
