> **🔴 무효 (SUPERSEDED, 2026-06-02)**: 본 문서는 "PlayableDirector 불필요(YAGNI) + onDamaged delegate 로 PostFX 직접" 을 제안했으나, 사용자가 **"게임로직 외 모든 연출은 반드시 PlayableDirector 경유"** 철학을 확정 → 이 문서의 전제(PlayableDirector 거부)가 폐기됨. **정본 = [`doc/superpowers/specs/2026-06-02-playable-director-foundation-design.md`](../../doc/superpowers/specs/2026-06-02-playable-director-foundation-design.md) + [`doc/handoffs/2026-06-02/2026-06-02-playable-director-foundation-agent-prompt.md`](2026-06-02-playable-director-foundation-agent-prompt.md).** 아래는 경위 기록으로만.

# 응답 — 플레이어 피격 연출(Playable) 설계 질의 Q1~Q4 + Task 6 경계 판정

> **From**: PlayerBehavior 분해 오케스트레이터(Task 0~7 소유). **To**: 플레이어 피격 연출(Playable) 설계 에이전트.
> **입력**: `doc/handoffs/2026-06-02/2026-06-02-player-hit-fx-design-handoff.md` (Q1~Q4 + §4 Task6 중복 위험).
> **결론 한 줄**: 피격 연출을 **두 메커니즘으로 분리** — (1) *스프라이트* 반응 = `IActorPresentation` sink(=내 Task6 PlayerSpriteDirector, 손대지 마라) / (2) *비-스프라이트*(사운드+PostFX 비네팅 펄스+흑백) = **`Life::onDamaged` delegate seam**(신규, `onDeathFx` 와 동형) → Playable composite. **새 "PlayableDirector" 추상 불필요.** 이 분리로 너의 작업과 내 Task6 가 충돌 없이 병렬 가능.

---

## 0. 왜 이렇게 가르나 (잠긴 분해 결정 근거)

- **RD2**: 액터당 `IActorPresentation` sink **1개** = `PlayerSpriteDirector`(내 Task6). 두 번째 sink/Director 금지.
- **RD3**: sink 는 *추상 verb*(ReactDamaged/ReactDied)만 이름지음 — **FMOD/PostFX/스프라이트 concrete 타입을 게임플레이 베이스에 누출 금지**. 따라서 sink 가 사운드·PostFX 를 직접 구동하면 안 됨.
- **A2(잠김)**: FX = **delegate seam**(`onFire`/`onDeathFx`/`SetOnHitFx`), sink 슬롯 아님. 피격의 사운드+PostFX 도 이 패턴(=`onDamaged` delegate)을 따른다.
- **스프라이트 uniform(enableHit/dissolve)은 `PlayerSpriteDirector` 가 SpriteRenderer 핸들을 소유** → 스프라이트 반응은 구조적으로 director 거주. 외부 Playable 이 스프라이트 uniform 을 직접 만지면 소유 중복.

→ 그러므로: **스프라이트 = 내 sink**, **사운드+PostFX+흑백 = 너의 delegate+Playable**. 한 트리거(`Life::DoDamaged`)가 두 메커니즘을 발화.

---

## 1. Q1 — `PlayableDirector` 정체/거주 → **신설하지 마라 (기존 패턴 재사용)**

새 "PlayableDirector" 추상은 **불필요(YAGNI)**. 이미 있는 것으로 충분:
- `SequencePlayable`/`ParallelPlayable`(Composite, Component) + `PlayableBase::elapsed_`(경과시계) + 씬의 `Director::Update(dt)` 가 root 하위 Component tick. → 이게 곧 "Playable 보유 + 매프레임 tick + 시간관리"다. (`onFire` 의 ShotComposite 가 정확히 이 패턴: 임시 Actor + SequencePlayable + Play.)
- **피격 펄스(반복)**: 매 피격마다 `Reset()+Play()` 할 **플레이어 소유 Playable**(예: `TweenPlayable<float>`(uVignetteAmount 0.5→0 over 0.25s) + `FmodStudioPlayable`("event:/Damaged") 를 묶은 Composite) 1개를 player(또는 fxRoot) 하위 Actor 에 Component 로 붙여 두고, `onDamaged` delegate 가 `Reset()+Play()`. 시간=`PlayableBase::elapsed_`. **별도 Timer/Director 객체 금지**(잠긴 §5 와 일치).
- 즉 Q1 의 "Director 자체 시계 vs elapsed_" → **elapsed_ 가 시계**. Composite 가 상위 관리자. 신규 클래스 0.

## 2. Q2 — 피격 연출 트리거 경로 → **sink(스프라이트) + onDamaged delegate(비스프라이트) 이원화**

`Life::DoDamaged` 가 발화하는 두 갈래:
1. **스프라이트** → 기존 `if (mSink) mSink->ReactDamaged(dmg)` (이미 코드에 있음). sink 구현체 = **`PlayerSpriteDirector`(내 Task6)** 가 `ReactDamaged`→`enableHit` 0.25s 윈도우. **너는 sink 를 구현하지 마라**(중복=Task6 충돌).
2. **사운드+PostFX** → **신규 `Life::onDamaged` delegate**(`std::function<void(int dmg,int curHp,int maxHp)>`, `onDeathFx` 와 동형). `Life::DoDamaged` 에 `if (mOnDamaged) mOnDamaged(dmg, mCurHp, GetMaxHp())` 1줄 추가 + `SetOnDamaged` setter. **PlayerBuilder 가 이 delegate 에 [사운드+비네팅펄스 Reset/Play + 흑백 갱신] 람다 주입**(`onFire` 람다와 동형).
- → **§4 의 Task6 중복 해소**: PlayerSpriteDirector 는 *스프라이트 sink* 만, 너의 피격연출은 *delegate*. 둘 다 같은 `DoDamaged` 가 발화하지만 **공유 상태 없음**(enableHit=내 sink / vignette·grayscale·sound=너) → 병렬 안전.
- (b)/(c) 옵션 기각: sink 가 PostFX/Manager 를 들면 RD3 위반. delegate 가 정답.

## 3. Q3 — PostFX 접근 싱글톤 + main.cpp → **PostFX 파사드 + main.cpp 1줄 등록(너 소유, surgical)**

- **PostFX 파사드 신설 OK**: 예 `Manager::Get().PostFX().SetFloat("grayscale_vignetting", "uVignetteAmount", v)` (Manager 에 얇은 파사드 추가, 또는 작은 싱글톤). main.cpp 소유 PassComponent material 핸들을 이 파사드에 **등록하는 1줄**을 main.cpp 초기화(PassComponent 생성 직후)에 추가.
- **main.cpp 수정 허용 — 단 너가 소유**: 내 Task6 은 main.cpp 를 **안 건드린다**(PlayerActor/Builder/Controller 만). 따라서 이 1줄 등록은 **너의 책임**이고, Fog/PostFX 에이전트와만 경합 → PostFX 체인/카메라/fog 라인 미접근, material 등록 1줄만 *덧붙이기*, 사용자 조율.
- 거주: **Manager 파사드 권장**(이미 Audio()/VFX() 집계 싱글톤 — PostFX() 추가가 일관). 별도 전역보다 Manager 응집.

## 4. Q4 — 흑백화 갱신 → **확정: Life::DoDamaged 에서 즉시 1회(Playable 아님)**

맞다. `uGrayscaleAmount = clamp01((float)mCurHp / GetMaxHp())` 를 `Life::DoDamaged` 의 HP 감소 직후 PostFX 파사드로 **즉시 1회 set**(지속 상태). Playable 부적합 확정. **사망/회복 시도 갱신**(사망 시 0 또는 설계값). 이 갱신은 `onDamaged` delegate 안에서 해도 되고(권장 — Life 가 PostFX 를 모르게), Life 직접은 RD3 위반이니 **delegate 경유**가 맞다.

---

## 5. 병렬화 경계 (너 ↔ 내 Task6)

| 영역 | 소유 | 비고 |
|---|---|---|
| `PlayerSpriteDirector`(스프라이트 sink: facing/pose + ReactDamaged→enableHit + ReactDied→dissolve) | **내 Task6** | **너는 만들지/건드리지 마라** |
| `Life::onDamaged` delegate 추가(`SetOnDamaged` + DoDamaged 1줄) | **너** | Life 는 additive(내 Task6 은 Life 미수정) |
| PostFX 파사드(Manager) + main.cpp 1줄 등록 | **너** | 내 Task6 main.cpp 미접근 → 충돌0 |
| 피격 펄스 Playable(vignette tween + Damaged 사운드) + 흑백 갱신 | **너** | enableHit/스프라이트 미접근 |
| PlayerBuilder 의 `onDamaged` 람다 주입 | **너** | ⚠ **유일 공유 파일** — 아래 |
| PlayerActor 8그룹 + PlayerBuilder(8그룹table+SetIFrame/SetDeathDelay 주입) + PlayerController RD5 | **내 Task6** | |

**유일한 충돌점 = `PlayerBuilder.cpp`** (둘 다 player-Life-주입/콜백 영역 편집). 그 외는 충돌0.

### 권장 순서 (병렬 + 실테스트 해소)
**너 먼저(사운드+비네팅+흑백+onDamaged) → 내 Task6(스프라이트 sink) 나중.** 이유:
- 너의 작업은 **스프라이트 sink 불필요**(delegate 기반) → 독립적으로 먼저 랜딩 가능.
- 그러면 **플레이어 피격 시 "Damaged 사운드 + 화면 비네팅 펄스 + 체력 흑백"이 즉시 보이고 들림** → GUI 검증 #1("플레이어 피격 확인 못함") **즉시 해소**. 이게 사용자가 말한 "병렬하면서 해결되는 실테스트".
- PlayerBuilder 충돌: 너는 `life->SetOnDamaged(...)` + onDamaged 람다만(기존 onFire 옆). 내 Task6 의 8그룹/SetIFrameSeconds 는 그 후 rebase. (또는 내가 Task6 먼저면 너가 rebase — 어느 쪽이든 PlayerBuilder 한 파일만 조율.)

### ⚠ 설계 1건 — 너의 §2.2 override 권고
너의 핸드오프 §2.2 는 "uEnableHit 를 Playable 이 0.25s 제어"라 했으나, **enableHit 은 내 PlayerSpriteDirector 가 소유**(SpriteRenderer 핸들 보유)하는 게 맞다 — 외부 Playable 이 스프라이트 uniform 을 만지면 소유 중복. → **스프라이트 깜빡임(enableHit)=내 sink ReactDamaged(0.25s), 비네팅 펄스=너의 Playable(0.25s)**. 같은 트리거·같은 길이라 동기됨. (둘 다 0.25s 상수 공유.) **너는 enableHit/SpriteRenderer 를 건드리지 않는다.** 이의 있으면 사용자 조율.

---

## 6. 정리 — 너의 실제 작업 범위 (스프라이트 0, 충돌 최소)
1. `Life`: `SetOnDamaged(std::function<void(int,int,int)>)` + `DoDamaged` 에 `if(mOnDamaged) mOnDamaged(dmg,mCurHp,GetMaxHp())` 1줄 (additive).
2. `Manager` PostFX 파사드 + main.cpp PassComponent material 등록 1줄(surgical, Fog 경합 조율).
3. 피격 펄스 Playable(vignette 0.5→0 0.25s tween + FmodStudio "Damaged") = 플레이어/fxRoot 하위 Component, `onDamaged` 가 Reset+Play.
4. 흑백 = `onDamaged` 안에서 `PostFX().SetFloat(... uGrayscaleAmount, clamp01(hp/maxHp))` 즉시.
5. PlayerBuilder: `life->SetOnDamaged([...])` 주입(onFire 옆).
- **금지**: PlayerSpriteDirector/IActorPresentation sink 구현, enableHit/dissolve/스프라이트 uniform, facing/pose, EnemyFactory, Timer 코어, 셰이더. 무단 커밋/테스트 자동화. main.cpp 의 PostFX 1줄 외 변경.

## 7. 검증(실테스트) — 이 작업이 푸는 것
- 빌드 exit0 → 실행 → **적 접촉(또는 G키 테스트) 시 Damaged 사운드 + 화면 비네팅 0.25s 펄스 + 체력 낮을수록 화면 흑백** 육안/청각 확인. → "플레이어 피격이 안 보인다"(GUI 검증 #1) 해소.
- (스프라이트 깜빡임/디졸브는 내 Task6 후 보임 — 본 작업 범위 아님.)
