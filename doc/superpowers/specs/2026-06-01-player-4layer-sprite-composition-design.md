# PlayerActor 4-레이어 스프라이트 합성 + M6 개별-PNG 로딩 블록 해소 — 설계 spec

> **작성일**: 2026-06-01
> **브랜치**: `game/module/ingame/temp` (HEAD = `c2260b5 [dev] : raycast ground`)
> **성격**: brainstorming(explore→clarify→design) 결과. 6개 결정 확정.
> **선행 인계**: [`doc/handoffs/2026-06-01/2026-06-01-player-4layer-sprite-composition.md`](../../../doc/handoffs/2026-06-01/2026-06-01-player-4layer-sprite-composition.md) — *단, 핸드오프 이후 실제 코드가 변했음*(아래 §1 에서 보정).
> **해소 대상**: [`doc/superpowers/specs/2026-06-01-m6-as-built-design.md`](2026-06-01-m6-as-built-design.md) §4 Task 6/7 의 🔴 블록("개별 PNG 애니 로딩 미확정") + [`doc/superpowers/plans/2026-05-31-m6-sequencing-sound.md`](../plans/2026-05-31-m6-sequencing-sound.md) Task 7(PlayerFactory) 바디 모델 재조정.

---

## §1 컨텍스트 — 핸드오프 이후 변한 실제 코드 (보정)

본 작업은 핸드오프 문서로 시작했으나, 작성 이후 실제 코드가 **세 군데** 달라져 있어 재탐색으로 보정했다.

1. **`Constants.h` 필드 순서 swap** — 핸드오프 표기는 `{DrawOrder, Path, ColCount, RowCount, Flip}` 였으나 현 파일은 **`{DrawOrder, Path, RowCount, ColCount, Flip}`**. 그 결과 애니 레이어의 `2` 가 **`ColCount` 슬롯**에 들어가 있다(`{2, "...B_2.png", 1, 2}` → RowCount=1, **ColCount=2**). 즉 "RowCount=가로 프레임 수" 라던 핸드오프 결론은 뒤집혔고, 현재는 **`ColCount` = 가로 프레임 수**(정적=1, 애니=2), `RowCount`=1 고정이라는 자연스러운 의미가 됐다.
2. **플레이어 생성이 `Bootstrap::PlayerBuilder` 로 추출됨** — 핸드오프가 "교체 대상" 이라던 `main.cpp` 의 `WramupPlayer` 단일-스프라이트 블록은 사라졌다. 실제 통합 지점은 [`apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp`](../../../apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp) 의 `BuildPlayer(PlayerDeps)`. 그 안에서 `CreatePlayerActor(pac)`(게임플레이 Component-only) 호출 후 **단일** `test_pattern` 4×4 atlas + 단일 `SpriteRenderer` + 단일 clip(`deps.clipStorage` 에 caller-owned 저장)을 붙인다.
3. **`PlayerHand.h` 신규** — `PlayerSingleHand`/`PlayerHands` Component 스케치(조준 궤도 손). `SpriteRenderer` 를 **Component 멤버로 직접 보유**하나, 그 방식은 SceneRenderer DFS 수집 대상이 아니라 **렌더되지 않음** → 본 4-레이어 합성과 부적합. **이번 범위 밖.**

---

## §2 자산 실측 (authoritative)

`apps/_MyApp_/resources/texture/player/` 24개 PNG `sips` 측정:

| 그룹 | 파일 | 치수 | 해석 |
|---|---|---|---|
| 전 IDLE 세트 (16장) | `*_IDLE_*` | 512×512 | 단일 프레임 정적 |
| MOVE — B 파트 | `FRONT_MOVE_B_2` / `LEFT_MOVE_B_2` / `BACK_MOVE_B_0` | **1024×512** | **가로 2프레임 스트립 (애니)** |
| MOVE — 그 외 | `*_MOVE_{E,H,F}_*` | 512×512 | 단일 프레임 정적 |
| MOVE — BACK F 파트 | `BACK_MOVE_F_3` | **1×1** | 빈 placeholder (뒷모습=얼굴 없음). **FRONT_MOVE 무관** |

**결론**: "개별 PNG" 는 *프레임별* 이 아니라 **파트별(H/E/B/F 4장)**. 그리고 애니가 있는 B 파트는 **그 한 장이 이미 가로 2프레임 스트립**. 각 파트 PNG = 자기 자신이 grid atlas다.

---

## §3 결정 로그 (6건)

| # | 질문 | 결정 | 비고 |
|---|---|---|---|
| D1 | 작업 범위 | **바디 4-레이어 합성, FRONT_MOVE 한 방향만** | PlayerHand·8방향 전환은 다음 단계 |
| D2 | 4 SpriteRenderer 를 담는 구조 | **4개 child Actor** | 엔진 `AddComponent<T>` 타입당 1개 제약 회피. 코어 무수정 |
| D3 | 합성 로직 위치(1차) | **CreatePlayerActor 확장** | (D5 에서 .cpp화로 정밀화) |
| D4 | `SpriteFrameClip`(비소유 포인터) 수명 | **(c) 엔진에 값-소유 ctor 추가** | footgun 제거. `static thread_local` 핵 소멸 |
| D5 | 합성 위치(재확인, 실 구조 반영) | **CreatePlayerActor 흡수 + `.cpp`화** | 게임플레이 팩토리에 sprite 흡수, 헤더 비대 회피 위해 본문 .cpp 이동 |
| D6 | spec 범위 | **FRONT_MOVE 구현 + IDLE↔MOVE·8방향 전환 모델 설계문서화(미구현)** | M6 §4 블록 공식 종료 + M6 Task7 재조정 |

기술 디테일(에이전트 결정, 사용자 미반대): fps 기본 6 / atlas 키 = TexturePath / 레이어 순서 = `QueueOffset = DrawOrder`.

---

## §4 M6 §4 블록의 답 — 로딩 전략 (핵심)

§2 결론에 따라 M6 As-Built §4 의 🔴 블록("현 UniformAtlas 는 단일 그리드 PNG 만 지원, 개별 PNG 애니 로딩 미확정")은 **추가 로더 없이** 해소된다:

- 파트마다 **자기 PNG = 자기 `UniformAtlas`**. `ResourceRegistry::CreateUniformAtlas(key=path, path, cols=ColCount, rows=RowCount)` (key=path → LEFT/RIGHT 가 같은 PNG 공유 시 캐시 hit).
- 정적 파트 → `SetGrid(1, 1)`. 애니 B 파트 → `SetGrid(2, 1)`. **기존 `SJH::Sprite::UniformAtlas` 무변경.**
- M6 §4 후보 **(a) 프레임별 텍스처 N장 전환 / (b) 런타임 grid 패킹 / (c) 방향별 단일 atlas — 전부 불필요.** 신규 sprite 컴포넌트/로더도 불필요.
- 즉 **본 4-레이어 합성 설계 자체가 블록의 해답**이다. 엔진 sprite 변경은 §5 의 값-clip ctor 하나뿐.

---

## §5 엔진 코어 변경 — `SJH::sprite` (D4, 유일한 엔진 수정)

`SpriteSequencePlayable` 에 **값-소유 ctor 오버로드** 추가. 기존 포인터 ctor·multi-clip API 와 병존.

```cpp
// src/sprite/sprite_sequence_playable.h — public 에 오버로드 추가
SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef, SpriteFrameClip ownedClip);
// private 에 멤버 추가
SpriteFrameClip ownedClip_{};   // 값 보관 — clip_ 가 이를 가리킴 (값 ctor 사용 시)
```
```cpp
// src/sprite/sprite_sequence_playable.cpp
SpriteSequencePlayable::SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* s, SpriteFrameClip c)
    : sprite_(s), clip_(&ownedClip_), ownedClip_(c) {}
//   ↑ clip_ 는 ownedClip_ 의 *주소*만 취함(스토리지 안정) — 값 읽기는 OnUpdate(나중) 에서만 발생. UB 아님.
```

- **불변식**: Component 는 `AddComponent<T>`(`make_unique` in-place)로만 생성되고 이동/복사되지 않는다(클래스 move/copy 미선언 유지) → `clip_ = &ownedClip_` 댕글링 불가.
- **효과**: 호출자가 외부 clip 저장소를 관리할 필요 없음 → M6 Task7 의 `static thread_local` clip 핵([plan line 850/879](../plans/2026-05-31-m6-sequencing-sound.md#L850))이 **소멸**. `PlayerDeps.clipStorage` 도 제거 가능.
- **테스트**: [[no_auto_tests]] 존중 — 단위 테스트 자동 추가 안 함. (요청 시 `test_uniform_atlas` 인근에 값-ctor frameIdx 회귀 추가 가능.)
- **향후 훅(미구현)**: 상태 전환용 `SpriteSequencePlayable::SetClip(SpriteFrameClip)` 값-setter(같은 `ownedClip_` 갱신)는 §8 전환 모델의 확장점으로만 명시.

---

## §6 도메인 변경 — `CreatePlayerActor` 흡수 + `.cpp`화 (D5)

### §6.1 `PlayerActorConfig` 확장
[`apps/_MyApp_/src/Entity/Player/PlayerActor.h`](../../../apps/_MyApp_/src/Entity/Player/PlayerActor.h) 에 nested cfg 추가. **M6 Task7 의 `SpriteCfg{atlas, clips}` 를 대체**한다.

```cpp
struct SpriteCfg {
    const std::vector<TopdownShooter::Playable::PlayerTextureConfig>* direction = nullptr; // nullptr → 스프라이트 없음(게임플레이-only)
    float fps = 6.0f;
};
SpriteCfg sprite;
```
- `direction == nullptr` 이면 기존 게임플레이-only 동작 보존(다른 호출자/테스트 무해).
- 헤더 의존 추가: `apps/_MyApp_/src/Playable/Constants.h`(같은 `apps/_MyApp_/src/` 트리, myapp_entity include 경로 내) + `<vector>`. sprite/atlas 헤더는 .cpp 로 격리(§6.2).

### §6.2 팩토리 본문 `.cpp` 이동
`CreatePlayerActor` 본문(현재 [PlayerActor.h:89-153](../../../apps/_MyApp_/src/Entity/Player/PlayerActor.h#L89) inline)을 **새 `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp`** 로 이동. 헤더는 `PlayerActorConfig` + `CreatePlayerActor` *선언*만. `apps/_MyApp_/src/Entity/CMakeLists.txt` 의 `myapp_entity` STATIC 소스에 `apps/_MyApp_/src/Entity/Player/PlayerActor.cpp` 추가. (Entity 는 이미 `SJH::sprite` PRIVATE + `game_deps` link.)

### §6.3 4-레이어 합성 로직 (팩토리 끝, `direction != nullptr` 일 때)
```
for (const PlayerTextureConfig& t : *cfg.sprite.direction) {
    child = parent->AddChild(Actor(parent.name + "_L" + to_string(t.DrawOrder)));   // local (0,0,0)
    atlas = ResourceRegistry::Get().CreateUniformAtlas(t.TexturePath, t.TexturePath, t.ColCount, t.RowCount);
    if (!atlas) { spdlog::error(...); continue; }   // graceful — 누락 PNG 는 그 레이어만 skip
    spr = child->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
    spr->flipX       = t.Flip;
    spr->QueueOffset = t.DrawOrder;                 // 2450+DrawOrder → distinct 층
    if (t.ColCount > 1) {                            // 애니 파트
        auto* seq = child->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
                        spr, SJH::SpriteSequence::SpriteFrameClip{0, t.ColCount, cfg.sprite.fps});
        seq->SetIsLoop(true);
        seq->Play();
    }
}
```
- 싱글턴 `ResourceRegistry::Get()` 사용(PlayerBuilder "reg/dir 은 ::Get()" 컨벤션 — config 에 registry 주입 불필요).
- 카메라 follow 대상 = **parent**(반환 Actor) 유지.
- `Play()` 시점: 현재 단일-스프라이트도 AddChild-to-root 전에 `Play()` 하므로 동일 패턴 안전(Component tick 이 트리 진입 후 구동).

---

## §7 Bootstrap / main 변경 (최소)

### §7.1 `PlayerBuilder.{h,cpp}`
- `.cpp`: 단일-스프라이트 블록([PlayerBuilder.cpp:107-125](../../../apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp#L107)) 삭제 → `pac.sprite.direction = &TopdownShooter::Playable::FRONT_MOVE; pac.sprite.fps = 6.0f;`. 이후 `CreatePlayerActor(pac)` 가 4 child 까지 생성.
- `.h`: `PlayerDeps.clipStorage` 멤버 **제거**(D4 로 불필요). `PlayerResult.Sprite`/`SpriteSeq` 는 합성 후 **애니(B) 레이어**의 컴포넌트를 `GetComponent` 로 찾아 채움(main 변경 최소화).

### §7.2 `main.cpp` (surgical — 경합 가드레일 §10 준수)
- [main.cpp:235](../../../apps/_MyApp_/main.cpp#L235): `BuildPlayer({&mKeyboard, &mMouse, &phys.World(), mCamera, &mWholeAtlasClip})` → `&mWholeAtlasClip` 인자 **제거**.
- [main.cpp:403](../../../apps/_MyApp_/main.cpp#L403): `SJH::SpriteSequence::SpriteFrameClip mWholeAtlasClip{}` 멤버 **제거**.
- `mSprite`/`mSpriteSeq`/`mSpriteActor` (236-238, 319/323-324, 398/402/404) 유지 — render/update 미사용이라 무해. (전부 정리 원하면 옵션, 본 spec 은 최소 변경 채택.)

---

## §8 전환 모델 — **설계만 문서화 (이번 미구현, M6 Task7 재조정)**

> 본 절은 **구현하지 않는다**. M6 §4 블록을 공식 종료하고 다음 spec 에 명확한 경로를 남기기 위한 설계 기록.

- Constants.h 의 8 벡터(`BACK/FRONT/LEFT/RIGHT × IDLE/MOVE`; `RIGHT_* = LEFT_* + Flip`)가 곧 8 directional-state.
- 바디 = **4 child SpriteRenderer 영속** — 상태 전환 시 actor 재생성 안 함, **atlas/flipX/clip 만 교체**.
- 향후 `PlayerSpriteDirector` Component(parent 부착) 가 `SetDirectionalState(const std::vector<PlayerTextureConfig>& set)` 노출:
  1. 4 파트마다 `set[i]` 의 PNG atlas(캐시) 재바인딩 + `flipX = Flip`.
  2. 애니 파트(`ColCount > 1`) 식별 — **DrawOrder 가 방향마다 다름**(FRONT=B@DrawOrder2, BACK=B@DrawOrder0)에 주의. 해당 child 의 clip 을 `{0, ColCount, fps}` 로, 나머지는 `frameIdx=0` 정적.
  3. atlas 는 path 캐시라 전환 저렴(전 PNG 1회 로드).
- 구동(future): `PlayerBehavior`/`PlayerController` 가 (방향, 이동중 여부)를 입력/속도에서 산출 → 8벡터 매핑 → `SetDirectionalState`. 이는 **M6 Task7 의 `PlayClip(Idle/Move/Attack/Hit)` 바디 구동을 대체**한다. Attack/Hit 전용 바디 스프라이트는 자산에 없으므로 **FX 슬롯**(§9)이 담당하고 바디 프레임으로 표현하지 않는다.
- clip 재설정은 §5 의 향후 `SetClip(SpriteFrameClip)` 값-setter 로 (lifetime 안전하게) 처리.

---

## §9 M6 Task 6/7 재조정 (기록)

- **대체**: M6 Task7 의 바디 sprite 모델(단일 atlas + multi-clip frame-range: Idle/Move/Attack/Hit) → 본 4-레이어 합성. `SpriteCfg{atlas, clips}` → `SpriteCfg{direction, fps}`. `static thread_local` 핵 소멸(§5).
- **유효 유지**: `PlayerBehavior` + 5 FX 슬롯(`BuildPlayerAttack/Hit/Dash/Die/MoveEffect`)은 바디 비주얼과 직교 → M6 Task6 그대로 유효.
- **단순화**: 향후 `PlayerFactory`(Spawns, [plan Task7](../plans/2026-05-31-m6-sequencing-sound.md#L771)) 는 base `CreatePlayerActor`(이제 바디 sprite 포함) 호출 후 PlayerBehavior·슬롯만 추가 → **PlayerFactory 의 "sprite + multi-clip" step(plan line 845-857) 제거**.

---

## §10 렌더 합성 근거 (파이프라인 무변경)

4 child 모두 local (0,0,0) → parent world matrix 공유 → billboard 가 같은 center·scale.
[`scene_renderer.cpp:135`](../../../src/render/scene_renderer.cpp#L135) `queueLayer = Material.GetQueueLayer() + QueueOffset` → `QueueOffset=DrawOrder(0..3)` 로 AlphaTest(2450) 안에서 레이어가 **2450..2453 distinct** → 제출 순서 무관 DrawOrder 순 정렬. AlphaTest(`billboard_atlas.fs` `if (c.a < 0.01) discard;`) + `GL_LEQUAL` + blend off → 나중 레이어가 위에 덮이는 painter 합성.

---

## §11 경합 가드레일 (동시 Agent 환경 — M6 §5 계승)

- **별도 파일 우선**: `Entity/Player/PlayerActor.{h,cpp}` / `Bootstrap/PlayerBuilder.{h,cpp}` / `src/sprite/sprite_sequence_playable.{h,cpp}` 위주. `main.cpp` 는 §7.2 surgical 2곳만.
- **main.cpp 변경 즉시 커밋**(미커밋 두면 타 Agent reset 으로 유실).
- 경합 파일(`mouse_input.{h,cpp}` / `PlayerController.h`)은 **건드리지 않음**(전환 구동은 이번 미구현이라 무관).
- 착수 전 `git diff --stat` 로 baseline 확인(HEAD 이동 가능).

---

## §12 검증

```bash
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_     # 리소스 상대경로 → cd 필수
```
**기대**: 플레이어가 FRONT_MOVE 4겹(E·H·F 정적 + **B 파트 2프레임 워크 루프**)으로 표시, WASD 이동·카메라 follow 정상. (IDLE↔MOVE/8방향 전환은 본 범위 밖 — 항상 FRONT_MOVE 표시.)

---

## §13 범위 밖

8방향/IDLE↔MOVE 런타임 전환 **구현** · `PlayerHand` orbit · `BACK_MOVE_F_3` 1×1 placeholder 투명도 검증(FRONT_MOVE 무관) · M6 Task6/7 본체(PlayerBehavior + 5 FX 슬롯) · 다중 플레이어 · 단위 테스트([[no_auto_tests]]).
