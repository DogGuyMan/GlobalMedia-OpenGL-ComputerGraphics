# Spec — Entity 피격 깜빡임 + 사망 디졸브 연출

> **상태**: 설계. 코드 0줄. **별도 Claude Agent 가 "엔진+Life" 레이어 구현 예정**(아래 §6 소유 분할).
> **작성**: 2026-06-01. 브랜치 `game/module/ingame/temp`.
> **전제 — PlayerBehavior 분해와 일관**: 본 연출은 분해의 `IActorPresentation::ReactDamaged`/`ReactDied` sink(이미 `Life` 가 호출 중)에 꽂힌다. 분해 정본: [`2026-06-01-playerbehavior-decomposition-design.md`](2026-06-01-playerbehavior-decomposition-design.md) + 실행 [`doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition-execution.md`](../../../doc/handoffs/2026-06-01/2026-06-01-playerbehavior-decomposition-execution.md).

---

## 1. 목표 (사용자 요구)

`billboard_atlas` 셰이더(= 앞으로 모든 sprite Entity 가 쓰는 셰이더)를 쓰는 Entity 에 대해:
- **피격 (`Life::DoDamaged` → sink `ReactDamaged`)**: FragmentColor 가 White↔Red 로 **2회 깜빡** 후 원래 fragment 로 복귀.
- **사망 (`Life::DoDie` → sink `ReactDied`)**: **아웃라인 포함 디졸브**로 사라지며, **Actor 파괴 타이밍과 동기**(디졸브 완료 시점에 비활성).

## 2. 이미 존재하는 것 (재사용 — 변경 금지)

[`apps/_MyApp_/resources/shaders/billboard_atlas.fs`](../../../apps/_MyApp_/resources/shaders/billboard_atlas.fs) 에 연출 로직 + uniform 이 **이미 구현**돼 있다 (CPU 송신만 부재):
- 피격: `uEnableHit`(bool), `uTime`(float) → `step(0, sin(uTime*30))` 로 White/Red 극단 깜빡임.
- 디졸브: `uEnableDissolve`(bool), `uDissolveTex`(sampler2D), `uDissolveThreshold`(float 0→1), `uDissolveOutlineColor`(vec3 에미시브), `uDissolveOutlineThickness`(float) → threshold 미만 픽셀 discard + 경계 아웃라인 컬러.
- **셰이더는 손대지 않는다.** CPU 가 uniform 만 채우면 동작.
- 디졸브 노이즈 자산: [`apps/_MyApp_/resources/texture/dissolve.png`](../../../apps/_MyApp_/resources/texture/dissolve.png) **존재**.

검증된 사실(드리프트 0):
- `SpriteRenderer::Update`([sprite_component.cpp:99-106](../../../src/sprite/sprite_component.cpp#L99))는 현재 `uUvRect`/`uFlipX`/`uTint` **3개만** 송신. 신규 uniform **송신처 0개**(미사용).
- uniform API: `SJH::Uniforms::{SetFloat,SetInt,SetVec3,SetVec4,SetTexture}(Material&, name, ...)` ([material_uniforms.h:38-48](../../../src/material/material_uniforms.h#L38)). **bool=`SetInt`(0/1), sampler2D=`SetTexture(mat,name,tex,unit)`**. store-only(draw 시 PropertyBlockSetter 가 glUniform).
- SpriteRenderer 는 **per-instance MaterialInstance**(`_sprite_inst_<N>`) 보유 → sprite 별 uniform 격리 ✓ ([sprite_component.cpp:78-82](../../../src/sprite/sprite_component.cpp#L78)).
- SpriteRenderer 는 **게임 무관 POD-ish 데이터 홀더**(atlas/frameIdx/tint/flipX + Update 송신). 타이머/게임 로직 없음.
- **적(enemy)은 현재 SpriteRenderer 미부착**(비주얼 없음) → 본 연출은 **플레이어 우선**. 적 스프라이트는 별도 후속(§7).
- **PlayerSpriteDirector 미존재** = PlayerBehavior 분해 Task 6 산출물. **본 연출의 sink 구현체 = PlayerSpriteDirector.**

## 3. 아키텍처 — 3 레이어 (분해 sink seam 재사용)

```
Life::DoDamaged ──ReactDamaged(dmg)──▶ sink(PlayerSpriteDirector) ──enableHit 윈도우──▶ SpriteRenderer.enableHit ──uEnableHit──▶ shader
Life::DoDie     ──ReactDied(pos)─────▶ sink                       ──dissolveThreshold 램프──▶ SpriteRenderer.dissolve* ──uDissolve*──▶ shader
Life (사망지연) ──mDeathTimer 만료──▶ SetActive(false)  (디졸브 완료 시점과 동기)
```
- **Layer A (엔진, SpriteRenderer)**: 셰이더 uniform 을 받는 *데이터 필드* + Update 송신. 게임 무관(POD 유지). **재사용**(어떤 sprite 든 구동 가능). `uTime` 은 SpriteRenderer 자체 free-running clock(`+= dt`)으로 연속 깜빡임 제공 — sink 는 `enableHit` on/off 만.
- **Layer B (게임플레이, Life)**: 사망지연 — 디졸브가 끝나기 전 Actor 파괴 방지. `mDeathDelaySeconds`(기본 0=현행 즉시) + Update 지연 비활성. **Life 가 사망 권위 유지**(gameplay→presentation 읽기 없음 — 디졸브 길이는 공유 상수로 매칭). backward-compat.
- **Layer C (연출, sink = PlayerSpriteDirector)**: `ReactDamaged`(피격 윈도우 타이머→`enableHit`), `ReactDied`(디졸브 램프→`enableDissolve`+`dissolveThreshold` 0→1 over ~사망지연). 활성 그룹 레이어에 적용. **RD2(액터당 sink 1개) → 효과 구동은 PlayerSpriteDirector 안**(별도 sink 추가 금지).

## 4. 상세 설계

### 4.1 Layer A — `SpriteRenderer` FX uniform (엔진 — 다른 에이전트)

[`src/sprite/sprite_component.h`](../../../src/sprite/sprite_component.h) `SpriteRenderer` 에 공개 필드 추가 (기존 tint/flipX 옆, POD-ish):
```cpp
// === 피격 깜빡임 (billboard_atlas.fs uEnableHit/uTime) — sink 가 enableHit 토글, uTime 은 자체 clock ===
bool  enableHit = false;
// === 사망 디졸브 (billboard_atlas.fs uEnableDissolve/...) — sink 가 구동 ===
bool                enableDissolve           = false;
float               dissolveThreshold        = 0.0f;   // 0→1 (사라지는 정도)
float               dissolveOutlineThickness = 0.05f;
vmath::vec3         dissolveOutlineColor     = vmath::vec3(1.0f, 0.5f, 0.0f);
const SJH::Texture* dissolveTex              = nullptr; // resources/texture/dissolve.png (sink 가 ResourceRegistry 로 주입)
```
+ private free-running clock: `float mEffectClock = 0.0f;` (uTime 용). `namespace SJH { class Texture; }` 전방 선언 추가.

[`src/sprite/sprite_component.cpp`](../../../src/sprite/sprite_component.cpp) `Update(float dt)` 에 송신 추가 (기존 3 uniform 뒤, dt 사용):
```cpp
mEffectClock += dt;
// 피격
Uniforms::SetInt  (*Material, "uEnableHit", enableHit ? 1 : 0);
Uniforms::SetFloat(*Material, "uTime",      mEffectClock);
// 디졸브
Uniforms::SetInt  (*Material, "uEnableDissolve",          enableDissolve ? 1 : 0);
Uniforms::SetFloat(*Material, "uDissolveThreshold",       dissolveThreshold);
Uniforms::SetFloat(*Material, "uDissolveOutlineThickness", dissolveOutlineThickness);
Uniforms::SetVec3 (*Material, "uDissolveOutlineColor",    dissolveOutlineColor);
if (dissolveTex)
    Uniforms::SetTexture(*Material, "uDissolveTex", dissolveTex, /*unit=*/1);   // uAtlas=unit0
```
- **backward-compat**: 기본 `enableHit=false`/`enableDissolve=false` → 셰이더 분기 skip → **기존 sprite 비주얼 변화 0**. (모든 SpriteRenderer 가 매 프레임 이 uniform 을 보내지만 scalar 라 cost 무시.)
- `material_uniforms.h` + `resource_registry`(Texture) include 필요. `SetInt`/`SetVec3`/`SetTexture` 는 이미 존재.

### 4.2 Layer B — `Life` 사망지연 (게임플레이) — ✅ 완료 (Timer 마이그레이션으로 대체)

> **✅ 구현됨 (2026-06-02, `f1e1a23`)**: 본 절의 `float mDeathDelaySeconds`/`mDying`/`mDeathTimer` 설계는 **`SJH::Timer` 마이그레이션**으로 **`std::optional<SJH::Timer::Timer> mDieTimer`** 로 구현·커밋됨. 계약 동일: `SetDeathDelaySeconds(float s)`(=`ArmInactive(mDieTimer, s)` — s>0 emplace+finished 장전, s<=0 reset/즉시) + `DoDie`에서 `mDieTimer ? Reset() : SetActive(false)` + `Update`에서 `mDeathFxFired && mDieTimer` 시 Tick→IsTimesUp→SetActive(false). **i-frame 도 동형(`optional<Timer> mInvincibleTimer`)**. 현재 설정처 0(nullopt=즉시 사망) → 플레이어 0.5s 주입은 **분해 Task 6**. 아래 float 코드는 원설계 기록(현행은 optional<Timer>). 상세 = `doc/handoffs/2026-06-02/2026-06-02-timer-migration-followup-agent-prompt.md` §1.2.

[`apps/_MyApp_/src/Entity/Components/LifeComponents.h`](../../../apps/_MyApp_/src/Entity/Components/LifeComponents.h) (원설계 — 현행은 위 노트의 optional<Timer>). **additive** — 멤버 + setter + DoDie/Update 수정:
```cpp
// 멤버 추가:
float mDeathDelaySeconds = 0.0f;   // 0 = 즉시(현행). >0 = 사망 연출(디졸브) 동안 SetActive 지연
bool  mDying             = false;
float mDeathTimer        = 0.0f;
// setter 추가:
Life &SetDeathDelaySeconds(float s) { mDeathDelaySeconds = s; return *this; }
```
`Update` 수정 (i-frame 감산 뒤):
```cpp
void Update(float dt) override
{
    if (mInvincibleTimer > 0.0f) mInvincibleTimer -= dt;
    if (mDying)   // 사망 연출 진행 중 — 타이머 만료 시 비활성
    {
        mDeathTimer -= dt;
        if (mDeathTimer <= 0.0f && GetOwner()) GetOwner()->SetActive(false);
        return;
    }
    if (!mDeathFxFired && !IsAlive()) DoDie();   // 안전망
}
```
`DoDie` 수정 (즉시 SetActive → 지연 분기):
```cpp
void DoDie() override
{
    if (mDeathFxFired) return;
    mDeathFxFired = true;
    const vmath::vec3 pos = GetOwner() ? GetOwner()->GetTransform().Translate : vmath::vec3(0.0f);
    if (mSink) mSink->ReactDied(pos);            // 디졸브 시작 (sink 구동)
    if (mOnDeathFx) mOnDeathFx(pos);
    if (mDeathDelaySeconds <= 0.0f)
    {
        if (GetOwner()) GetOwner()->SetActive(false);   // 즉시 (기본/현행 — 적·지연 미설정)
    }
    else
    {
        mDying      = true;                       // 지연 — Update 가 만료 시 비활성
        mDeathTimer = mDeathDelaySeconds;
    }
}
```
- **backward-compat**: 기본 `mDeathDelaySeconds=0` → 즉시 SetActive(false) = **현행 동작**(적, 지연 미설정 플레이어). 회귀 0.
- 플레이어 0.5s 사망지연 **주입은 분해 Task 6**(PlayerBuilder, SetIFrameSeconds 옆 `SetDeathDelaySeconds(0.5f)`). 본 레이어는 *캐퍼빌리티만* 추가.
- **gameplay→presentation 읽기 없음**: Life 는 sink 의 디졸브 완료를 *묻지 않는다*. 디졸브 길이(sink)와 사망지연(Life)은 동일 상수(~0.5s)로 *독립 매칭*.

### 4.3 Layer C — sink 구동 (연출 — **PlayerBehavior 분해 Task 6 에 fold, 내가 구현**)

`PlayerSpriteDirector`(분해 Task 6 산출, `IActorPresentation`)에 효과 구동 추가. **본 spec 은 Task 6 가 포함할 내용을 명세**(다른 에이전트 작업 아님):
```cpp
// 상수
static constexpr float kHitFlashDuration = 0.42f;  // sin(uTime*30) 약 2 cycle (2π/30 ≈ 0.209s × 2)
static constexpr float kDissolveDuration = 0.5f;   // Life mDeathDelaySeconds 와 매칭
// 멤버
float mHitTimer = 0.0f;
bool  mDissolving = false;
float mDissolveTimer = 0.0f;
const SJH::Texture* mDissolveTex = nullptr;        // OnEnter 에서 ResourceRegistry 로 dissolve.png 로드

void ReactDamaged(int) override { mHitTimer = kHitFlashDuration; }
void ReactDied(vmath::vec3) override { mDissolving = true; mDissolveTimer = 0.0f; }

void Update(float dt) override   // (RD5 의 velocity-무읽기 정책 유지 — 여긴 효과 타이머만)
{
    if (mHitTimer > 0.0f) {
        mHitTimer -= dt;
        const bool on = mHitTimer > 0.0f;
        ForActiveLayers([&](SpriteRenderer* r){ r->enableHit = on; });   // off 시 복귀
    }
    if (mDissolving) {
        mDissolveTimer += dt;
        const float th = std::min(mDissolveTimer / kDissolveDuration, 1.0f);
        ForActiveLayers([&](SpriteRenderer* r){
            r->enableDissolve = true; r->dissolveThreshold = th; r->dissolveTex = mDissolveTex;
        });
    }
}
```
- `ForActiveLayers` = 현재 Visible 그룹(EFacing×EPose)의 4 레이어에 적용(다른 그룹은 invisible).
- **Task 6 의 `OnEnter` 가 `Apply()` 외에 dissolve.png 를 `ResourceRegistry` 로 1회 로드**해 `mDissolveTex` 캐시.
- PlayerBuilder Task 6 주입: `life->SetIFrameSeconds(0.5f).SetDeathDelaySeconds(0.5f)`.
- **`Quantize4`/RD5 의 "director 는 velocity 안 읽음" 정책과 무충돌** — 효과 타이머는 sink 자체 상태.

## 5. 셰이더 contract (참고 — 변경 없음)

| uniform | 타입 | 송신처(A) | 구동(C) | 의미 |
|---|---|---|---|---|
| `uEnableHit` | bool(SetInt) | SpriteRenderer | sink `ReactDamaged` 윈도우 | 깜빡임 on/off |
| `uTime` | float | SpriteRenderer(자체 clock) | — | `sin(uTime*30)` 위상 |
| `uEnableDissolve` | bool(SetInt) | SpriteRenderer | sink `ReactDied` | 디졸브 on/off |
| `uDissolveThreshold` | float | SpriteRenderer | sink 램프 0→1 | 사라짐 정도 |
| `uDissolveTex` | sampler2D(unit1) | SpriteRenderer | sink(dissolve.png) | 노이즈 |
| `uDissolveOutlineColor` | vec3 | SpriteRenderer | (기본 주황) | 아웃라인 에미시브 |
| `uDissolveOutlineThickness` | float | SpriteRenderer | (기본 0.05) | 아웃라인 두께 |

## 6. 소유 분할 (조율 — 사용자 결정 2026-06-01)

| 레이어 | 소유 | 시점 | 충돌 |
|---|---|---|---|
| **A. 엔진 SpriteRenderer FX uniform** | **다른 Claude Agent** | 지금 | 충돌 0 (분해 Task 4/5/6/7 은 SpriteRenderer 효과필드 미수정) |
| **B. Life 사망지연 캐퍼빌리티** | **다른 Claude Agent** | 지금 | additive·기본 0 (분해 Task 4/5/7 은 Life 미수정; Task 2 는 커밋됨) |
| **C. sink 구동(ReactDamaged/ReactDied) + 플레이어 주입** | **나 (분해 Task 6 에 fold)** | Task 6 시점 | Task 6 이 A·B 산출에 의존 → A·B 먼저 |

- **분리 사유**: sink 구현체 = PlayerSpriteDirector = 분해 Task 6. 다른 에이전트가 병렬로 PlayerSpriteDirector 를 만들면 **내 Task 6 과 충돌** → C 는 내가 Task 6 에서. A·B 는 독립이라 다른 에이전트가 지금 안전하게.
- **순서**: 다른 에이전트(A+B) → 내 분해 재개(Task 4→7, Task 6 이 A·B 활용).

## 7. 비목표 (Non-Goals)

- **셰이더 수정** — billboard_atlas.{vs,fs} 는 완성, 손대지 않음.
- **적(enemy) 피격/사망 연출** — 적은 SpriteRenderer 미부착(비주얼 없음). 적 스프라이트 도입 후 별도. (단 Layer A 는 재사용 가능하게 게임 무관 설계.)
- **별도 IActorPresentation sink 추가** — RD2(액터당 1개) → 효과는 PlayerSpriteDirector 안.
- **gameplay→presentation 읽기** — Life 가 sink 디졸브 완료를 묻지 않음(공유 상수 매칭).
- **단위 테스트** — `no_auto_tests`.
- **uTime 전역 동기화** — SpriteRenderer 자체 clock 으로 충분(2 cycle 윈도우).

## 8. 검증

- **Layer A/B (다른 에이전트)**: 빌드 `cmake --build --preset ninja --target _MyApp_` exit 0. **기본값(enableHit/enableDissolve=false, deathDelay=0) → 기존 비주얼·동작 변화 0**(회귀 확인). FX 시각 확인은 *임시 토글*(플레이어 SpriteRenderer 에 `enableHit=true` 또는 `enableDissolve=true`+threshold 램프+dissolveTex 강제) → White/Red 깜빡임·디졸브 렌더 육안 확인 후 **임시코드 revert**. (full 통합 = Task 6.) **main.cpp 수정 금지**(Fog Agent 경합) — 임시 토글은 PlayerBuilder 등 비경합 위치.
- **Layer C (내 Task 6)**: 적 접촉 피격 시 White/Red 2회 깜빡 후 복귀, 사망 시 0.5s 아웃라인 디졸브 후 사라짐.

## 9. self-review
- ✅ 분해 sink seam(ReactDamaged/ReactDied) 재사용 — Life/인터페이스 변경 0(Life 는 사망지연 additive만).
- ✅ A·B 독립·backward-compat(기본값 동작 변화 0). C 는 Task 6 의존 명시.
- ✅ 셰이더·SpriteRenderer POD 컨벤션·RD2·gameplay→presentation 비읽기 일관.
- ⚠ uTime 자체 clock 의 2-cycle 윈도우는 위상 임의(깔끔한 white-start 아님) — 육안상 "2회 깜빡"엔 충분. 정밀 필요 시 sink 가 hitTime 직접 피드(후속).
