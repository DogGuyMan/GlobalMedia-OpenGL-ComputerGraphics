# 핸드오프 — 플레이어 피격 연출 **hit-FX 트랙** (PostFX·사운드 Playable)

> **상태**: foundation 정합 완료. 본 트랙은 **PlayableDirector foundation 이후** 실행한다(코드 0줄).
> **선행 의존(반드시 먼저)**: [`doc/handoffs/2026-06-02/2026-06-02-playable-director-foundation-agent-prompt.md`](2026-06-02-playable-director-foundation-agent-prompt.md) — foundation 에이전트가 PlayableDirector + PostFXRegistry + key 규약을 먼저 구현해야 본 트랙이 시작 가능.
> **정본 spec**: [`doc/superpowers/specs/2026-06-02-playable-director-foundation-design.md`](../../doc/superpowers/specs/2026-06-02-playable-director-foundation-design.md) — 본 트랙 = **§8-B "PostFX·사운드 트랙"**.
> **철학(사용자 확정)**: 게임 로직 외 모든 연출/사운드는 `PlayableDirector` 경유 `Playable` 로만. 산재 직접 uniform/sound 호출 금지.

---

## 0. 이 트랙의 범위 (spec §8-B)

foundation 위에 다음 **Playable 타입을 만들어 `"hit"`/`"death"` key Composite 에 등록**한다:
- **[2] 비네팅 펄스** — `uVignetteAmount` 0.5 → 0, 0.25초 (피격 펄스, 반복 가능)
- **[3] 체력 흑백화** — `uGrayscaleAmount = clamp01(mCurHp / GetMaxHp())` (지속 상태)
- **[1] 피격 사운드** — `FmodStudioPlayable("event:/Damaged")` 1회

**만들지 않는 것 (병렬 트랙 소유)**: 스프라이트 `HitBlinkPlayable`(enableHit 0.25s)·`DissolvePlayable`·directional = **PlayerBehavior 분해 Task6**. 본 트랙은 PostFX·사운드만.

---

## 1. 선행 의존 — foundation 이 먼저 제공하는 것

| foundation 산출물 | 본 트랙이 의존하는 부분 |
|---|---|
| `PlayableDirector`(Component + `IActorPresentation`, `apps/_MyApp_/src/Playable/`) | 플레이어 액터에 부착된 **sink**. `Register(key, Playable)` / `Play(key)` / 중앙 tick |
| key 규약 | `ReactDamaged → Play("hit")`, `ReactDied → Play("death")` (foundation 이 skeleton 제공) |
| `PostFXRegistry` 싱글톤 + main.cpp 1줄 등록 | `PostFXRegistry::Get().Material("grayscale_vignetting")` 로 PostFX material 도달 |
| PlayerBuilder 합성점 | `onFire`/`onDamage` 마이그레이션 완료 상태 — 본 트랙은 그 위에 `"hit"`/`"death"` 합성 추가 |

→ **foundation 머지 전엔 본 트랙 착수 불가**(PlayableDirector/PostFXRegistry 미존재).

---

## 2. 이전 미결정(§3) → foundation 확정 답

| 이전 질문/우려 | 확정 답 (spec) |
|---|---|
| Q1 PlayableDirector 정체/거주 | Component + `Entity::IActorPresentation`, `_MyApp_` Client, named Playable + 중앙 tick(MultipleTimer 정통) — spec §3 |
| Q2 트리거 경로 + Task6 sink 중복 | director 자체가 sink. `Life.mSink`가 캐시, `ReactDamaged→Play("hit")`. **Task6 PlayerSpriteDirector = 이 PlayableDirector 로 통합**(별도 금지) — spec §3·§7 RD2 |
| Q3 PostFX 싱글톤 + main.cpp | `PostFXRegistry`(Meyer's), **main.cpp 1줄 등록 surgical 허용** — spec §5 |
| Q4 흑백화 방식 | **확정: `ReactDamaged` 시 즉시 1회 갱신** — `GrayscaleSetPlayable`(1-shot)을 `"hit"` Composite 에 합성(foundation `ReactDamaged` 무수정). §3.2 |

---

## 3. 본 트랙이 만들 것

### 3.1 `PostFXTweenPlayable` (신설 — 비네팅 펄스 [2])
- `: SJH::Playable::PlayableBase`. 기존 `TweenPlayable<T>` leaf 를 내부에 쓰거나 직접 ramp.
- `OnUpdate(dt)`: `t` 진행(0→0.25s) 동안 `v = lerp(0.5f, 0.0f, t/0.25f)`, 0.25s 후 finished.
- 송신: `auto* m = PostFXRegistry::Get().Material("grayscale_vignetting"); if (m) m->Properties.Floats["uVignetteAmount"] = v;`
- material nullptr(미등록/헤드리스) → silent no-op.
- key `"hit"` 에서 매 발동마다 `Stop()+Play()` 로 0 부터 재생(반복).
- 거주: `apps/_MyApp_/src/Playable/` 또는 `src/VFX/` 인접(Client). 일반화 가능하면 `PostFXTweenPlayable(passName, uniformName, from, to, duration)` 파라미터화 권장.

### 3.2 체력 흑백화 [3] — `GrayscaleSetPlayable` (확정: ReactDamaged 시 즉시 1회 갱신)
- 목표: `uGrayscaleAmount = clamp01(mCurHp / GetMaxHp())` (가득=1.0 컬러, 0=흑백). 사망/음수 clamp.
- **`GrayscaleSetPlayable`(1-shot)**: `OnPlay`(또는 첫 `OnUpdate`)에 HP norm 을 `uGrayscaleAmount` 로 **1회 송신 후 즉시 finished**. loop 아님.
- **왜 1-shot 인가 — foundation 무수정 + 철학 충족**: "ReactDamaged 시 갱신"을 foundation 의 `PlayableDirector::ReactDamaged`(=`Play("hit")`)에 직접 코드로 넣으면 foundation 파일 수정 + 직접 호출(철학 위반)이 된다. 대신 이 1-shot 을 `"hit"` Composite 에 합성하면 `ReactDamaged → Play("hit")` 가 곧 흑백 갱신을 트리거 → **foundation 무수정**, Playable 경유.
- **즉시 갱신 + 지속**: 송신값은 PostFX material 에 남아 **다음 피격까지 유지**(지속 상태 충족 — 매 틱 loop 불필요).
- **HP 소스**: 생성 시 `ILivable*`(또는 `Life*`) 참조 주입 — PlayerBuilder 가 플레이어 Life 를 넘김. material/Life nullptr 시 silent no-op.

### 3.3 피격 사운드 [1]
- `Audio::FmodStudioPlayable` + `Manager::Get().Audio().LoadEvent("event:/Damaged")`(기존 패턴, PlayerBuilder onDamage 예시 참조).

### 3.4 합성 (PlayerBuilder — 3자 공유 편집점, additive)
- `dir.Register("hit", ParallelPlayable{ HitBlink(Task6) ∥ PostFXTween(본 트랙) ∥ Damaged(본 트랙) ∥ GrayscaleSet(본 트랙) })`.
- 각 트랙이 자기 Playable **타입**만 제공, PlayerBuilder 가 `ParallelPlayable.Join(...)` 으로 조립(additive). 흑백(`GrayscaleSet`)도 `"hit"` 에 함께 Join — **별도 key 불필요**(§3.2 즉시 갱신 확정).
- **순서**: foundation → (Task6 ∥ hit-FX). 셋 다 PlayerBuilder 를 건드리므로 **합성 줄만 additive 추가**, 서로의 줄 미접근.

---

## 4. 경계 / 제약 (foundation spec 준수)

- **코어 수정 금지**: `src/playable/`(IPlayable/PlayableBase/Composite/Interval), `src/timer/`. **셰이더 수정 금지**: `billboard_atlas.fs`(uEnableHit는 bool on/off 그대로 — Task6이 0.25s 제어), `grayscale_vignetting.fs`(완성).
- **PlayableDirector/PostFXRegistry 는 foundation 이 생성** — 본 트랙은 **등록/사용만**(중복 생성 금지).
- **Spawns/ 폐기 금지** (월드점 단발 기구). 본 트랙은 엔티티 지속 연출(PostFX/사운드)만.
- **main.cpp**: PostFXRegistry 등록은 foundation 담당 — 본 트랙은 **PlayerBuilder 합성만, main.cpp 미접근**(Fog 경합).
- **enemy/bullet delegate**(onDeathFx/SetOnHitFx)는 분해 Task5 조율 — 본 트랙 무관.
- **Life 수정 금지**: `DoDamaged→ReactDamaged`/`DoDie→ReactDied` 호출 이미 존재. (단 §3.2 흑백화가 `Life` HP getter 참조를 필요로 하면 *읽기 전용 접근*만.)
- 컨벤션: 커밋/git add 금지(사용자 승인), no_auto_tests(빌드+육안), 주석 한국어, 헤더가드 `__XXX_H__`(#pragma once 금지), long 금지(고정폭), 경로 슬래시, path-scoped, Co-Authored-By 미사용.

---

## 5. 검증

- **foundation 머지 후** `cmake --build --preset ninja --target _MyApp_` → [N/N] Linking executable.
- 실행 육안: 피격 시 **화면 비네팅 펄스(0.25s에 사라짐)** + **체력 낮을수록 화면 흑백** + **"Damaged" 사운드**. 발사 FX 회귀 0(foundation 책임).
- material 미등록 시 PostFX Playable no-op(크래시 없음) 확인.

---

## 6. 관련 자산

- 선행: [foundation 프롬프트](2026-06-02-playable-director-foundation-agent-prompt.md) / [foundation spec](../../doc/superpowers/specs/2026-06-02-playable-director-foundation-design.md)
- 셰이더: `apps/_MyApp_/resources/shaders/postprocess/grayscale_vignetting.fs`(uVignetteAmount·uGrayscaleAmount)
- PostFX 소유: `apps/_MyApp_/main.cpp`(`FindPassMaterial`/`mPassComponents`) → foundation 이 PostFXRegistry 로 노출
- Playable 코어: `src/playable/{iplayable,playable_base,composite_playable,interval_playable}.h` (+ 기존 `TweenPlayable<T>` leaf 위치 확인)
- 사운드: `apps/_MyApp_/src/Audio/{AudioSystem,FmodStudioPlayable}.h`, `src/Manager.h`
- sink/트리거: `apps/_MyApp_/src/Entity/Components/{Components.Interfaces.h,LifeComponents.h}` (foundation 의 PlayableDirector 가 구현)
- 합성점: `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp`

---

## 사용 메모 (오케스트레이터/사용자용)
- 본 트랙 = **foundation 이후 트랙 B(PostFX·사운드)**. Task6(스프라이트 트랙)과 병렬, **PlayerBuilder 가 3자 공유 편집점**.
- 착수 전제: foundation(PlayableDirector + PostFXRegistry + onFire/onDamage 마이그레이션) 머지 확인.
- 흑백화 방식 **확정(사용자)**: `ReactDamaged` 시 즉시 1회 갱신 — `GrayscaleSetPlayable`(1-shot)을 `"hit"` Composite 에 합성, HP 소스 = 생성 시 `Life`(`ILivable*`) 주입. **foundation `ReactDamaged` 무수정**(1-shot 이 `Play("hit")` 로 트리거).
