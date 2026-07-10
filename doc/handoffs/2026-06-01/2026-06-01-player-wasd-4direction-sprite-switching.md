# Handoff — PlayerActor WASD 4방향 스프라이트 전환

> **수신자**: 본 프로젝트의 다른 Claude Code (또는 작업 재개 시점의 나)
> **목적**: 플레이어 바디 스프라이트를 *FRONT_MOVE 한 방향(완료)* → *WASD 기반 4방향 IDLE/MOVE 전환* 으로 확장. 이 문서는 **완료된 선행 작업 + brainstorming 의사결정(3건 확정) + 제안 설계 + 미결정 1건**을 손실 없이 인계한다.
> **작성 시점**: 2026-06-01. 브랜치 `game/module/ingame/temp`, HEAD = `26cfc3a`.
> **진행 단계**: brainstorming 스킬 — *explore + clarify(Q1~Q3 확정) 완료 → 설계 제시 → 사용자 최종 승인 직전*. (코드 0줄 안 건드림. spec 미작성.)

---

## 0. 작업 요청 원문 (사용자)

> 8방향이 아니라 **WASD 방향으로 4방향 스프라이트 애니메이션**이었다 (사용자 정정).
>
> **[1]** A, D 는 `Constants.h` 의 `Flip` 에 대응: **A → Flip:false, D → Flip:true**.
>
> **[2]** `PlayerBehavior.cpp` 의 `Move()` 가 WASD 에 대응하여, `PlayerBehavior.h` 의 슬롯으로 **올바른 SpritePlayable 를 Play** 하게 해야 한다. 따라서 전방향에 대해 `PlayerBuilder.cpp#106-130` 를 확장해야 한다.

(주: 사용자는 PlayerBehavior 를 지목했으나, 아래 §2 의 *아키텍처 충돌* 때문에 Q3 에서 **신규 컴포넌트**로 결정됨.)

---

## 1. 완료된 선행 작업 (커밋됨 — 변경 금지)

직전 세션에서 **FRONT_MOVE 한 방향 4-레이어 합성**을 완료·커밋했다 (별도 spec/plan: `doc/superpowers/{specs,plans}/2026-06-01-player-4layer-sprite-composition*` — gitignore 로컬 전용).

| 커밋 | 내용 |
|---|---|
| `1d8cfe6` | `SpriteSequencePlayable` **값-소유 ctor** 추가 — `(SpriteRenderer*, SpriteFrameClip)` (멤버 `ownedClip_` → `clip_=&ownedClip_`). 비소유 포인터 footgun 제거 |
| `53a8858` | `CreatePlayerActor` 를 헤더 inline → `PlayerActor.cpp` 로 이동 + `PlayerActorConfig::SpriteCfg{direction, fps}` 추가 + **4-레이어 합성 루프** ([PlayerActor.cpp:83-118](../../apps/_MyApp_/src/Entity/Player/PlayerActor.cpp#L83)) |
| `26cfc3a` | `PlayerBuilder` 가 `pac.sprite.direction=&FRONT_MOVE` 주입, 단일 `test_pattern` 블록 + `clipStorage` + `mWholeAtlasClip` 제거 |
| `f6082ab` | **(사용자 커밋, 위 작업과 동시 진행)** 8방향 스프라이트 자산/조준 벡터/손 개발 — `Constants.h` 에 include guard·8 방향 벡터·`HAND_PART`, `PlayerHand.{h,cpp}`, `PlayerHands` wiring 등 추가 |

**검증 완료**: 빌드 clean(`-Wall -Werror`), 런타임 로그가 `FRONT_MOVE_B_2(1024×512)→2×1 grid` 등 4 파트 아틀라스 로드 확정, **사용자 시각 확인 완료**.

→ 즉 *로딩 메커니즘*("개별 PNG 를 애니로?")은 해결됨: **각 파트 PNG = 자기 UniformAtlas, 애니 파트(ColCount>1)는 가로 스트립 `SetGrid(ColCount,1)`**. 새 sprite 로더 불필요. 이 메커니즘은 **범용** (어느 방향 벡터든 `CreatePlayerActor(cfg.sprite.direction=&X)` 로 빌드됨). 본 작업은 그 위에 **런타임 4방향 전환**을 얹는다.

---

## 2. ⚠️ 핵심 아키텍처 충돌 (반드시 이해)

두 스프라이트 모델이 공존하며 **서로 호환되지 않는다**:

- **(A) 4-레이어 모델** (위 §1 에서 구현): 방향 = *4개 파트 PNG 세트(E/H/B/F)*. 각 파트 = child Actor + `SpriteRenderer`. 애니 파트(B, ColCount=2)에만 `SpriteSequencePlayable` 1개(2프레임 워크 루프). `PlayerResult.Sprite/SpriteSeq` 는 B 레이어를 가리킴.
- **(B) PlayerBehavior 멀티클립 모델** ([PlayerBehavior.cpp](../../apps/_MyApp_/src/Entity/Player/PlayerBehavior.cpp)): *단일* `mSpriteSeq` + `PlayClip(clipIdx)`, clipIdx ∈ `EPlayerClip{Idle=0, Move=1, Attack=2, Hit=3}`. 한 아틀라스 안 *프레임 구간* 전환 모델. **현재 플레이어에 부착되어 있지 않음** (PlayerBuilder 에 `AddComponent<PlayerBehavior>` 없음 — 이동은 `PlayerController → PhysicsMovement` 가 담당). M6 Task7(PlayerFactory) 가 wiring 예정이나 *블록 상태*.

**왜 (B)로 4방향을 못 하나**: 4방향 전환은 *서로 다른 PNG 세트* 전환(프레임 구간 아님)이고, 애니 파트의 *레이어 위치가 방향마다 다름* (FRONT 는 B=DrawOrder 2, BACK 은 B=DrawOrder 0 — [Constants.h](../../apps/_MyApp_/src/Playable/Constants.h) 참조). 따라서 단일 `SpriteSequencePlayable.PlayClip` 으로 표현 불가. → **신규 컴포넌트 + 그룹 토글**로 간다 (Q2/Q3).

---

## 3. 의사결정 — 사용자 응답 (3건 확정)

| # | 질문 | **사용자 응답** | 비고 |
|---|---|---|---|
| Q1 | 방향별 상태 범위? | **MOVE 4방향 + IDLE 4방향** | 이동=방향 MOVE, 정지=마지막 방향 IDLE. 8 세트(4×2) |
| Q2 | 8 세트를 어떻게 보유·전환? | **미리 빌드 + 가시성 토글** (swap-in-place 거부) | 8 그룹 미리 생성, 활성 그룹만 표시. 엔진 새 API 불필요. "애니 파트 레이어 위치 가변" 문제 자동 해결 |
| Q3 | 방향 판정·토글 구동 컴포넌트? | **신규 `PlayerSpriteDirector`** (PlayerBehavior 흡수 거부) | 작은 전용 Component. BoxBody 속도 읽어 4방향 양자화. PlayerBehavior 는 M6 Task7 용으로 그대로 |

**에이전트 제안(사용자 미확정 — 설계에 포함, 이견 시 조정)**:
- 방향 양자화 = **dominant-axis** (`|vx|>|vz|` → 좌/우, else 상/하). 부호는 실행으로 검증.
- 스폰 기본 = **FRONT_IDLE**.
- fps 기본 = `SpriteCfg::fps`(현 6.0f).

---

## 4. 제안 설계 (사용자 승인 직전 — 미승인)

### A. 상태 매트릭스 (WASD → 자산)
| 입력 | 방향 | IDLE 그룹 | MOVE 그룹 |
|---|---|---|---|
| W(위) | BACK | `BACK_IDLE` | `BACK_MOVE` |
| S(아래) | FRONT | `FRONT_IDLE` | `FRONT_MOVE` |
| A(왼) | LEFT | `LEFT_IDLE` | `LEFT_MOVE` (flip=false) |
| D(오른) | RIGHT | `RIGHT_IDLE` | `RIGHT_MOVE` (flip=true, LEFT 자산 재사용) |

### B. 씬 구조 (미리 빌드 + 토글)
```
Player (gameplay: Life/Physics/Controller/Weapon) + PlayerSpriteDirector
  ├─ Group "FRONT_IDLE"  → 4 child 레이어(E/H/B/F)   ← 스폰 시 기본 표시
  ├─ Group "FRONT_MOVE"  → 4 child (B 애니 2프레임)
  ├─ Group "BACK_IDLE", "BACK_MOVE", "LEFT_*", "RIGHT_*"   (총 8 그룹 × 4 = 32 sprite child)
  └─ PlayerHands (기존 유지)
```
- 활성 그룹만 4 레이어 `SpriteRenderer.Visible=true`, 나머지 7 그룹 `Visible=false`.
- 아틀라스 path 캐시 → RIGHT 는 LEFT PNG 재사용(새 아틀라스 0), 실 unique ≈ 24장.
- **렌더 가시성 주의**: `Actor::SetActive(false)` 는 렌더를 끄지 *않는다* ([scene_renderer.cpp:128](../../src/render/scene_renderer.cpp#L128) 가 `mr->Visible` 만 게이트, IsActive 미검사). → **그룹 숨김은 각 레이어의 `SpriteRenderer.Visible` 로** 해야 함.

### C. 코드 변경
1. **`BuildSpriteLayerGroup(const std::vector<PlayerTextureConfig>& dir, float fps) → std::unique_ptr<SJH::Scene::Actor>`** — 신규 자유함수. **[PlayerActor.cpp:83-118](../../apps/_MyApp_/src/Entity/Player/PlayerActor.cpp#L83) 의 4-레이어 합성 루프를 그대로 추출**. 그룹 actor(4 child) 반환. (find-or-create atlas + `flipX=Flip` + `QueueOffset=DrawOrder` + 애니파트 `SpriteSequencePlayable` 값-ctor + `SetIsLoop(true).Play()`.)
2. **`PlayerSpriteDirector`** (신규 Component, `apps/_MyApp_/src/Entity/Player/`):
   - 멤버: `SJH::Scene::Actor* mGroups[4][2]` (Dir×{Idle,Move}) + `int mCurDir` + `bool mCurMoving`.
   - `enum Dir { Front, Back, Left, Right }`.
   - `Update(float)`: owner 의 BoxBody → `GetComponent<Physics::Components::BoxBody>()->GetBody()->GetLinearVelocity()`. `speed<ε` → Idle(방향 유지), else Move + dominant-axis 방향. 바뀌면 `ShowGroup(dir,state)`.
   - `ShowGroup(dir,state)`: 그 그룹 4 레이어 `Visible=true`, 나머지 false (+ 선택: 활성 MOVE 그룹의 playable `Play()`).
   - setter: `SetGroup(Dir, bool moving, Actor*)` 로 8 그룹 주입.
3. **`PlayerBuilder.cpp` [#106-130](../../apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp#L106) 확장** (사용자 지시):
   - `CreatePlayerActor(pac)` 는 **게임플레이만** 생성 (`pac.sprite.direction` 미설정 → 4-레이어 미생성).
   - 이후 8개 `BuildSpriteLayerGroup(*vec, fps)` 호출 → player 에 `AddChild`, 기본(FRONT_IDLE) 외 hide.
   - `player->AddComponent<PlayerSpriteDirector>()` + 8 그룹 + 초기 표시.
   - `PlayerResult`/카메라 follow(ActorFolower)/PlayerHands 는 기존대로.
4. **`CreatePlayerActor`**: 4-레이어 합성을 `BuildSpriteLayerGroup` 로 위임. ⚠️ **미결정 §5** 참조.

### D. 전환 규칙
- 스폰 = FRONT_IDLE.
- `speed < ε` → 마지막 방향 IDLE. `speed ≥ ε` → 그 방향 MOVE.
- 대각(W+D 등) → dominant-axis 1방향 snap (탑다운 4-dir 표준).

### E. 범위 밖
- Attack/Hit 스프라이트(자산 없음), PlayerBehavior 본체 wiring(M6 Task7), 손 스프라이트(사용자 WIP), 마우스 aim 기반 방향(이번은 *이동* 기반).

---

## 5. 미결정 / 후속 (설계 마무리 시 사용자 질의 필요)

1. **⚠️ C-4 (가장 중요)** — `CreatePlayerActor` 의 단일방향 합성(`cfg.sprite.direction`, Task2/3 산출물)을 어떻게 처리?
   - (a) **`BuildSpriteLayerGroup` 로 추출 + 단일방향 빌드 제거** (8-그룹이 대체). CreatePlayerActor 는 게임플레이만. *에이전트 추천* — 한 가지 방식으로 통일.
   - (b) 추출하되 `cfg.sprite.direction` 단일 빌드도 *남겨둠* (BuildSpriteLayerGroup 1회 호출로 위임). 호환 유지, 약간 중복.
   - **재개 시 사용자에 1건 질의 후 확정.**
2. **방향 양자화 부호** — world XZ → Box2D XY 매핑 (`PhysicsMovement` 는 `b2Vec2(x, -z)` 사용, spec §4.4). W=BACK / S=FRONT / A=LEFT / D=RIGHT 의 *속도 부호*는 **실행으로 검증** 필요. dominant-axis 로직 자체는 확정.
3. **양자화 ε / dead-zone** 값 미확정 (제안 0.1).
4. **비활성 MOVE 그룹의 playable** — 계속 tick(무비용 무시) vs 활성 시에만 Play (desync 방지). 제안: 활성 시 `Play()`, 비활성은 무시.

---

## 6. 현재 코드 레퍼런스 (verbatim 시그니처)

### 6.1 `Constants.h` ([apps/_MyApp_/src/Playable/Constants.h](../../apps/_MyApp_/src/Playable/Constants.h))
- `struct PlayerTextureConfig { int DrawOrder; const char* TexturePath; int RowCount; int ColCount; bool Flip; }` (필드 순서 주의 — RowCount 가 ColCount 보다 앞).
- **8 방향 벡터** (각 4 entry): `BACK_IDLE / FRONT_IDLE / LEFT_IDLE / RIGHT_IDLE / FRONT_MOVE / BACK_MOVE / LEFT_MOVE / RIGHT_MOVE`. `RIGHT_*` = `LEFT_*` 자산 + `Flip=true`.
- **ColCount = 가로 프레임 수** (정적=1, 애니=2). 애니 파트는 각 MOVE 의 **B 파트**만 (FRONT/LEFT = DrawOrder 2 위치, BACK = DrawOrder 0 위치). IDLE 은 전부 ColCount=1(정적).
- ⚠️ 사용자가 DrawOrder 를 **음수**(`-0,-1,-2,-3`)로 바꿔 레이어 순서 튜닝 중 — `BuildSpriteLayerGroup` 는 `t.DrawOrder` 부호 무관하게 `QueueOffset` 에 대입(엔진이 `2450+DrawOrder` 로 정렬). 값은 사용자 소관.
- `HAND_PART` (단일 config) 추가됨 — 손 트랙(사용자 WIP), 본 작업 무관.

### 6.2 `SpriteRenderer` ([src/sprite/sprite_component.h](../../src/sprite/sprite_component.h))
public 멤버 직접 대입: `UniformAtlas* atlas`, `int frameIdx`, `vec4 tint`, **`bool flipX`**, (MeshRenderer 상속) **`int QueueOffset`**, `bool Visible`. `AddComponent<T>` 타입당 1개.

### 6.3 `SpriteSequencePlayable` ([src/sprite/sprite_sequence_playable.h](../../src/sprite/sprite_sequence_playable.h))
- 값-소유 ctor (`1d8cfe6` 추가): `SpriteSequencePlayable(SpriteRenderer*, SpriteFrameClip)`. 그룹 빌더가 이걸 사용.
- 멀티클립 API(`RegisterClip`/`PlayClip`)도 있으나 본 설계는 **미사용** (그룹 토글로 대체).

### 6.4 `ResourceRegistry` atlas ([src/resource_registry/resource_registry.h:132,137](../../src/resource_registry/resource_registry.h#L132))
`CreateUniformAtlas(key, png, cols, rows)` — **중복 키 시 nullptr** (경고). `FindUniformAtlas(key)`. → **find-or-create 관용구 필수** (PlayerActor.cpp:93-95 참조). key=path.

### 6.5 `Actor` ([src/scene/actor.h](../../src/scene/actor.h))
`AddChild(unique_ptr)→Actor*`, `GetChildren()→const vector<unique_ptr>&`, `GetComponent<T>()→T*`, `SetActive/IsActive`, `GetTransform()`. ⚠️ `SetActive(false)` 는 **렌더 비차단** (Visible 로 숨겨야 함, §4.B).

### 6.6 `PlayerBehavior` ([.h](../../apps/_MyApp_/src/Entity/Player/PlayerBehavior.h) / [.cpp](../../apps/_MyApp_/src/Entity/Player/PlayerBehavior.cpp)) — 본 작업 *미사용*
`EPlayerClip{Idle,Move,Attack,Hit}` + `mSpriteSeq->PlayClip` + 5 FX 슬롯(`mAttackPlayable` 등) + `Update` 속도기반 Idle/Move. **미부착**. M6 Task7 용으로 보존 — 건드리지 말 것.

### 6.7 렌더 정렬 ([scene_renderer.cpp:135](../../src/render/scene_renderer.cpp#L135))
`cmd.queueLayer = mr->Material->GetQueueLayer() + mr->QueueOffset` (AlphaTest 기본 2450). `mesh_pass_processor` stable_sort → DrawOrder 순 painter 합성.

---

## 7. 작업 재개 가이드 (brainstorming 잔여 단계)

진행 상태: **explore + clarify(Q1~Q3) 완료, 설계 제시됨**. 다음:
1. (중단점) **§5-1 (C-4) 1건 질의 → 설계 확정 → 사용자 승인.**
2. **spec 작성** → `doc/superpowers/specs/2026-06-01-player-wasd-4direction-sprite-switching-design.md` (gitignore 로컬). M6 Task7 와의 관계(PlayerBehavior 미사용 이유) 명기.
3. spec self-review → 사용자 spec 리뷰 게이트.
4. **writing-plans** 진입 (brainstorming 의 terminal state — 다른 스킬 호출 금지).

### 진입점 파일
- 확장 대상: [PlayerBuilder.cpp:106-130](../../apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp#L106) (사용자 지시).
- 추출 원본: [PlayerActor.cpp:83-118](../../apps/_MyApp_/src/Entity/Player/PlayerActor.cpp#L83) → `BuildSpriteLayerGroup`.
- 신규: `PlayerSpriteDirector.{h,cpp}` (`apps/_MyApp_/src/Entity/Player/`) — `apps/_MyApp_/src/Entity/CMakeLists.txt` 의 `myapp_entity` 에 .cpp 추가.
- 데이터: [Constants.h](../../apps/_MyApp_/src/Playable/Constants.h) 8 벡터 (수정 불필요 — 읽기만).

### 빌드/실행
```bash
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_     # 리소스 상대경로 → cd 필수. WASD 로 방향 전환 시각 확인.
```

### 절대 하지 말 것
- `PlayerBehavior` 를 wiring/리팩터 (M6 Task7 영역 — Q3 에서 제외). 본 작업은 **신규 `PlayerSpriteDirector`** 만.
- 그룹 숨김에 `SetActive(false)` 의존 (렌더 비차단 — `SpriteRenderer.Visible` 사용).
- `CreateUniformAtlas` 직접 호출 (중복키 nullptr — find-or-create).
- `Constants.h` DrawOrder/자산 수정 (사용자 동시 편집 중 — `f6082ab` 류).
- 한 Actor 에 `SpriteRenderer` 2개 (assert — child 분리).
