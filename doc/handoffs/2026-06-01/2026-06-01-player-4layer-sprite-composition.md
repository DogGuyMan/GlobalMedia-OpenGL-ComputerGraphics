# Handoff — PlayerActor 4-레이어 스프라이트 합성 + 방향별 애니메이션

> **수신자**: 본 프로젝트의 다른 Claude Agent (또는 작업 재개 시점의 나)
> **목적**: PlayerActor 의 스프라이트를 *단일 SpriteRenderer* → *4-레이어 합성* 으로 확장. 한 방향(FRONT_MOVE) 먼저, `RowCount==2` 레이어만 시간 애니메이션. 이 문서는 **지금까지의 탐색 결과 + 사용자가 응답한 모든 의사결정 + 제안 설계 + 미결정 항목**을 손실 없이 인계한다.
> **작성 시점**: 2026-06-01. 브랜치 `game/module/ingame/temp`, HEAD = `c2260b5 [dev] : raycast ground`.
> **진행 단계**: brainstorming 스킬 진행 중 — *clarify 완료 → 최종 설계 제시 직전* 에서 사용자가 핸드오프 작성을 요청해 중단. (아직 코드 0 줄도 안 건드림. spec 미작성. writing-plans 미진입.)

---

## 0. 작업 요청 원문 (사용자)

> PlayerActor의 Atlas 애니메이션을 확장해야 한다. 현재는 단일 SpriteRenderer + SpritePlayable이지만, **SpriteRenderer 4개를 동일 위치에 겹쳐서** `apps/_MyApp_/src/Playable/Constants.h` 의 내용에 있는 애니메이션을 넣어야 한다. **Row Count 가 2인 요소만 시간에 따른 애니메이션**이 될 예정이다.

요청과 함께 첨부된 현재 `WramupPlayer()` 코드(main.cpp 발췌)는 **단일** atlas(`test_pattern`, 4×4 grid) + 단일 `SpriteRenderer` + 단일 `SpriteSequencePlayable`(`mWholeAtlasClip` 멤버로 clip 수명 유지) 를 `spriteActor` 에 직접 붙이고, 카메라 follow 대상을 `mSpriteActor`(parent) 로 잡는 구조다. 이 블록이 **이번 작업으로 교체될 대상**이다.

(참고: 사용자가 IDE 에서 [main.cpp:346](../../apps/_MyApp_/main.cpp#L346) `onFire` 라인을 선택했지만, 본 작업과 직접 관련 없다고 명시.)

---

## 1. `Constants.h` — 데이터 구조와 의미

[`apps/_MyApp_/src/Playable/Constants.h`](../../apps/_MyApp_/src/Playable/Constants.h) — `namespace TopdownShooter::Playable`.

```cpp
struct PlayerTextureConfig
{
    const int   DrawOrder   = -1;   // 0=back(먼저 그림) → 3=front(나중·위)
    const char *TexturePath = nullptr;
    const int   ColCount    = -1;   // ⚠️ 전 항목 항상 1 — 사실상 미사용
    const int   RowCount    = -1;   // ⚠️ "가로로 배열된 애니 프레임 수" (1=정적, 2=2프레임 애니)
    const bool  Flip        = false;// RIGHT_* 가 LEFT_* 자산을 좌우반전 재사용 (flipX)
};
```

8개 방향 벡터(각 4 entry, `DrawOrder` 0..3 = H/E/B/F 파트 레이어):
`BACK_IDLE / FRONT_IDLE / LEFT_IDLE / RIGHT_IDLE / FRONT_MOVE / BACK_MOVE / LEFT_MOVE / RIGHT_MOVE`.

- `RIGHT_IDLE`, `RIGHT_MOVE` 는 디스크에 자기 PNG 없음 → **`LEFT_*` 자산을 `Flip=true`** 로 재사용 (의도된 설계 ✓).
- 한 방향 = **H/E/B/F 4개 파트 PNG 를 `DrawOrder` 순으로 같은 위치에 겹쳐** 캐릭터 한 명 구성.

### 1.1 ⚠️ 핵심 발견 — `RowCount` 의 실제 의미 (자산 치수와 교차검증)

`sips` 로 24개 PNG 실측 결과:

| PNG | Constants 표기 (Col,Row) | 실제 치수 | 해석 |
|---|---|---|---|
| 모든 `*_IDLE_*` (16개) | (1,1) | 512×512 | 단일 프레임 정적 |
| `FRONT_MOVE_B_2` | (1,2) | **1024×512** | 가로 2프레임 (512×512 ×2) — **애니** |
| `LEFT_MOVE_B_2` | (1,2) | **1024×512** | 가로 2프레임 — **애니** |
| `BACK_MOVE_B_0` | (1,2) | **1024×512** | 가로 2프레임 — **애니** |
| `BACK_MOVE_F_3` | (1,1) | **1×1** | 빈 placeholder (뒷모습=얼굴 없음 의도로 추정) |
| 그 외 `*_MOVE_*` | (1,1) | 512×512 | 단일 프레임 정적 |

**결론**: `RowCount==2` 자산은 실제로 **가로 2프레임 스트립(1024×512)**. 따라서 적재는
`UniformAtlas::SetGrid(cols = RowCount, rows = 1)` 로 해야 함 (cols=가로 프레임 수). `ColCount`(항상 1) 는 사실상 안 씀.
→ `(ColCount=1, RowCount=2)` 를 그대로 `SetGrid(1, 2)` 에 넘기면 `dv = tileSize/atlasHeight = 1024/512 = 2.0` 로 **UV 범위 초과 → 깨짐**. 반드시 `SetGrid(2, 1)`.

### 1.2 사용자가 직접 수정한 `BACK_MOVE` (현재 파일 상태 = 확정)

질의 응답 중 사용자가 `BACK_MOVE` 를 자산에 맞게 직접 정정 (현재 [Constants.h:50-55](../../apps/_MyApp_/src/Playable/Constants.h#L50)):

```cpp
const std::vector<PlayerTextureConfig> BACK_MOVE = {
    {0, "./resources/texture/player/BACK_MOVE_B_0.png", 1, 2},  // 1024×512 → 애니 (B가 backmost)
    {1, "./resources/texture/player/BACK_MOVE_H_1.png", 1, 1},  // 512×512 정적
    {2, "./resources/texture/player/BACK_MOVE_E_2.png", 1, 1},  // 512×512 정적
    {3, "./resources/texture/player/BACK_MOVE_F_3.png", 1, 1},  // 1×1 placeholder (빈 얼굴)
};
```

검증: **이제 모든 방향이 "RowCount=가로 프레임 수" 규칙과 일치**, 방향마다 애니 레이어는 정확히 **1개(B 레이어)**. 유일한 잔여 항목 = `BACK_MOVE_F_3` 1×1 placeholder (그 픽셀이 투명이면 안전, 불투명이면 단색 사각형이 덮임 — **투명 여부 미확인**).

---

## 2. 엔진 코드 탐색 결과 (시그니처 verbatim)

### 2.1 `SJH::Sprite::SpriteRenderer` — [sprite_component.h:39-56](../../src/sprite/sprite_component.h#L39), [.cpp:93-107](../../src/sprite/sprite_component.cpp#L93)
```cpp
class SpriteRenderer : public SJH::Scene::MeshRenderer {
    explicit SpriteRenderer(UniformAtlas* atlas = nullptr); // : MeshRenderer(공유 plane, per-inst Material)
    // public 멤버 (직접 대입):
    UniformAtlas* atlas = nullptr;
    int           frameIdx = 0;     // 현재 프레임 (SpriteSequencePlayable 가 매 tick 갱신)
    vmath::vec2   size  = {1,1};    // 미사용 (Transform.Scale 이 권위)
    vmath::vec4   tint  = {1,1,1,1};
    bool          flipX = false;    // ← Constants 의 Flip 매핑 대상
    void Update(float) override;    // uUvRect/uFlipX/uTint uniform 송신
};
```
- Material Pass = `Pass::Kind::AlphaTest`. 공유 plane/program/template + `_sprite_inst_<N>` per-instance Material.

### 2.2 `SJH::Sprite::UniformAtlas` — [uniform_atlas.h:29-120](../../src/sprite/uniform_atlas.h#L29)
- Fluent: `LoadFromPNG(path)` (NEAREST+CLAMP 자동), `SetGrid(cols, rows)` (`mTileSize=atlasWidth/cols`, **square tile 가정**), `SetTileSize(px)`.
- `GetUVRect(frameIdx)` → `(u,v,du,dv)`, row-major (`col=idx%cols, row=idx/cols`). `FrameCount()=cols*rows`.

### 2.3 `SpriteFrameClip` / `SpriteSequencePlayable` — [sprite_frame_clip.h:8](../../src/sprite/sprite_frame_clip.h#L8), [sprite_sequence_playable.h:13](../../src/sprite/sprite_sequence_playable.h#L13)
```cpp
struct SpriteFrameClip { int startFrame; int frameCount; float fps; };  // loop 없음 — PlayableBase 가 isLoop_ 관리

class SpriteSequencePlayable : public SJH::Playable::PlayableBase {
    SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                           const SpriteFrameClip* clip);   // ⚠️ clip = 비소유 포인터
    SpriteSequencePlayable& RegisterClip(int idx, const SpriteFrameClip*);
    SpriteSequencePlayable& RegisterOnClipEnter(int idx, IPlayable* sideEffect);
    void PlayClip(int idx); int CurrentClip() const;
    // 상속: Play/Pause/Stop, SetIsLoop(bool)/GetIsLoop(), IsFinished()
};
```
- `OnUpdate`: `raw=floor(elapsed_/frameDur)`; loop→`raw%=frameCount`, 아니면 clamp+`finished_=true`; `sprite_->frameIdx = clip->startFrame + raw`.
- **⚠️ clip 수명 footgun**: `clip_` 가 비소유 포인터 → 현재 main.cpp 는 `mWholeAtlasClip` 멤버로 수명 유지. 4-레이어/팩토리 구조에선 안정 주소 보장 필요 (§5 미결정).

### 2.4 `SJH::Scene::Actor` / `Component` — [actor.h](../../src/scene/actor.h)
- `template<T> T* AddComponent<T>(args...)` — **타입당 1개만!** `mComponents` 가 `unordered_map<type_index, unique_ptr<Component>>`. 중복 시 `assert` 크래시.
  → **한 Actor 에 SpriteRenderer 4개 직접 부착 불가** (이번 작업의 출발 제약).
- `GetComponent<T>()` exact(O(1)) → dynamic_cast first-match(O(N)). `AddChild(unique_ptr<Actor>)→Actor*`. `GetWorldMatrix()=parent.world*local`.

### 2.5 `CreatePlayerActor` / `PlayerActorConfig` — [PlayerActor.h:37-153](../../apps/_MyApp_/src/Entity/Player/PlayerActor.h#L37)
- **`inline` 팩토리(헤더 거주)**. PoD `PlayerActorConfig` (nested: `life/movement/controller/physics`). Pattern C.
- 현재 구성: `Life` + (physics.world 있으면 `BoxBody`+`PhysicsMovement` 아니면 `Movement`) + (keyboard 있으면 `PlayerController`).
- **SpriteRenderer 는 안 붙임** — main.cpp `WramupPlayer` 가 별도로 붙임. 반환 `unique_ptr<Actor>`.

### 2.6 `PlayerController` — [PlayerController.h](../../apps/_MyApp_/src/InputHandler/PlayerController.h)
- `Action`: MoveForward(W)/MoveBack(S)/MoveLeft(A)/MoveRight(D)/Damage(G).
- 매 프레임 `mInputValue`(vec3) 누적 → `mMovementPtr->DoForward({x,z}, dt)`.
- **facing/idle-move 상태를 노출 안 함** → 8방향 전환은 *신규 wiring* (이번 범위 밖, §3 Q2).

### 2.7 렌더 순서 / depth (4 레이어가 올바르게 겹치는 근거)
- [scene_renderer.cpp:115-159](../../src/render/scene_renderer.cpp#L115) `CollectFromActor`: MeshRenderer 를 **DFS/addChild 순서**로 수집. `queueLayer = Material.PassKind + MeshRenderer.QueueOffset`. `depthZ=(viewMat*model)[3][2]`.
- [mesh_pass_processor.cpp:39-84](../../src/render/mesh_pass_processor.cpp#L39) `SortMultiStage`: `std::stable_sort` — `queueLayer` → (transparent? `depth<` : `program`→`material`→`depth>`). **AlphaTest(2450) 4 child 는 모든 키 동일 → stable_sort 가 제출(addChild) 순서 보존.**
- [pass.h](../../src/material/pass.h) `AlphaTest`: `DepthTest=true, DepthWrite=true, DepthFunc=GL_LEQUAL, Blend off, queue=2450`.
- [billboard_atlas.vs](../../apps/_MyApp_/resources/shaders/billboard_atlas.vs): per-instance Z offset 없음. center + cameraRight·aPos.x·sx·uFlipX + cameraUp·aPos.y·sy.
- [billboard_atlas.fs:24](../../apps/_MyApp_/resources/shaders/billboard_atlas.fs#L24): `if (c.a < 0.01) discard;`
- **결론**: 같은 위치 4 child → 투명부 discard + GL_LEQUAL(같은 Z 통과) + blend off → 나중에 그린 레이어가 위에 덮이는 painter 합성. **addChild 를 DrawOrder 순(0→3)으로 하면 올바른 층.** 견고화: 각 SpriteRenderer `QueueOffset = DrawOrder` 명시(제출 순서 무관하게 보장). `QueueOffset` = MeshRenderer 의 직교 축 (Material=Kind 와 분리, pass.h 철학).

### 2.8 ResourceRegistry atlas API — [resource_registry.h:132](../../src/resource_registry/resource_registry.h#L132)
`Sprite::UniformAtlas* CreateUniformAtlas(const std::string& key, <path>, cols, rows);` / `FindUniformAtlas(key)`. (기존 호출 예: `CreateUniformAtlas("test_pattern", ".../TestPattern.png", 4, 4)` = (key,path,cols,rows).)

---

## 3. 의사결정 — 사용자 응답 전체 (6건, 손실 없이)

| # | 질문 | **사용자 응답** | 비고 |
|---|---|---|---|
| Q1 | 한 Actor 에 SpriteRenderer 4개 부착 불가(타입당 1개). 4-레이어를 어떤 구조로? | **4개 child Actor** (권장안) | parent 밑 레이어당 child 1개, 각 SpriteRenderer 1개; 엔진 코어 무수정 |
| Q2 | 작업 범위? | **한 방향 합성 먼저** | 8방향 전환은 다음 단계. PlayerController 상태 노출 불필요 |
| Q3 | `RowCount==2`=1024×512(가로 2프레임), `SetGrid(N,1)` 해석 맞나? | **맞다 — 가로 N프레임** | 로더가 `SetGrid(RowCount, 1)` (cols=프레임수, rows=1) |
| Q4 | `BACK_MOVE` 자산 불일치 처리? | **Constants.h 직접 수정** (§1.2) | 사용자가 직접 편집 완료·확인됨 |
| Q5 | 4-레이어 구성 로직 위치? (8방향 재사용 고려) | **CreatePlayerActor 확장** | PlayerActorConfig 에 방향 텍스처 설정 추가, 팩토리 안에서 child 까지 생성 |
| Q6 | 한 방향 데모로 어느 방향? | **FRONT_MOVE** (권장안) | B 레이어(RowCount=2)가 2프레임 애니 → 시간 애니메이션 검증 가능 |

기술 디테일은 에이전트가 결정해 제시 (사용자 미반대):
- 레이어 순서 보장 = `QueueOffset = DrawOrder`.
- 애니 fps **기본 6** (2프레임 워크사이클, 상수화).
- atlas 키 = path 기반 유니크.

---

## 4. 제안 설계 (사용자 승인 직전 — 미승인)

```
Player (parent Actor)  ── Life + BoxBody + PhysicsMovement + PlayerController  (기존 유지)
  │                        ※ parent 의 직접 SpriteRenderer 제거 (test_pattern 4×4 블록 삭제)
  ├─ Layer0 (child, local 0,0,0) ── SpriteRenderer{ atlas0, QueueOffset=0, flipX=Flip }
  ├─ Layer1 (child)              ── SpriteRenderer{ atlas1, QueueOffset=1 }
  ├─ Layer2 (child)              ── SpriteRenderer{ atlas2, QueueOffset=2 }
  └─ Layer3 (child)              ── SpriteRenderer{ atlas3, QueueOffset=3 }
       (RowCount==2 인 레이어에만 추가) + SpriteSequencePlayable(clip{0,RowCount,fps}, SetIsLoop(true).Play())
```
- **Atlas 적재**: 레이어당 `reg.CreateUniformAtlas(key, path, /*cols=*/RowCount, /*rows=*/1)`.
- **애니/정적 분기**: `RowCount==1` → 정적(playable 없음). `RowCount==2` → `SpriteFrameClip{0, RowCount, fps}` + `SpriteSequencePlayable`.
- **겹침 보장**: child 4개 모두 local (0,0,0) → parent world matrix 공유 → billboard 가 같은 center·scale → 완벽 정렬. **카메라 follow 대상은 parent 유지.**
- **데모 방향**: FRONT_MOVE (E_0 정적 / H_1 정적 / **B_2 애니(2프레임)** / F_3 정적).

---

## 5. 미결정 / 후속 검토 (설계 마무리·plan 단계에서 해결 필요)

1. **⚠️ clip 수명 (가장 중요)** — `SpriteSequencePlayable` 가 `const SpriteFrameClip*`(비소유) 보유. "CreatePlayerActor 확장"(Q5)에선 팩토리가 app 멤버에 못 넣음. 후보:
   - (a) **clip 보유용 경량 Component** 를 애니 레이어 child 에 부착(같은 actor 의 sibling, 수명 자동). Actor 비상속·Component-only 철학과 부합하나 12-byte POD 에 새 타입은 약간 과함.
   - (b) **호출자 제공 저장소** — `PlayerActorConfig` 에 `std::deque<SpriteFrameClip>*`(포인터 안정) 주입, app 이 멤버로 소유. 현재 `mWholeAtlasClip` 패턴의 일반화.
   - (c) **엔진 개선** — `SpriteSequencePlayable` 에 clip *값 소유* 생성자 오버로드 추가(`SpriteFrameClip ownedClip_;` 멤버 + `clip_=&ownedClip_`). 가장 깔끔, footgun 제거, 기존 포인터 ctor 와 호환. 단 **코어 `SJH::sprite` 수정** → 신중(테스트 게이트). **에이전트 추천 = (c), 단 사용자 결정 필요.**
2. **CreatePlayerActor 가 `inline`(헤더 거주)** — 스프라이트/atlas/Constants 헤더를 끌어오면 헤더 비대. → 팩토리 본문을 **`PlayerActor.cpp`(미존재) 또는 helper .cpp 로 이동** 후 `apps/_MyApp_/src/Entity/CMakeLists.txt`(이미 `SJH::sprite` PRIVATE + `game_deps` link, [Entity/CMakeLists.txt](../../apps/_MyApp_/src/Entity/CMakeLists.txt)) 의 `myapp_entity` 소스에 추가 검토. (PlayerActorConfig 가 `InputHandler/PlayerController.h` 의존 — include 경로 확인 필요.)
3. **`PlayerActorConfig` 확장 형태** — 방향 텍스처 nested 설정 추가, 예: `struct SpriteCfg { const std::vector<TopdownShooter::Playable::PlayerTextureConfig>* direction = &Playable::FRONT_MOVE; float fps = 6.0f; }`. (Constants.h 가 `apps/_MyApp_/src/Playable/`, include 가능한지 확인.)
4. **fps 정확값** — 기본 6 제안, 사용자 미확정.
5. **`BACK_MOVE_F_3` 1×1 placeholder 투명 여부** — 미확인. 불투명이면 뒷모습 얼굴 자리에 단색 사각형. (이번 데모는 FRONT_MOVE 라 당장 영향 없음.)

---

## 6. 작업 재개 가이드 (brainstorming 스킬 잔여 단계)

진행 상태: **explore + clarify 완료**. 다음 순서:
1. (중단점) **최종 설계 제시 → 사용자 승인.** §5-1 clip 수명 (a)/(b)/(c) 를 사용자에게 1건 질의 후 §4 설계 확정.
2. **spec 작성** → `doc/superpowers/specs/2026-06-01-player-4layer-sprite-composition-design.md` 에 저장 + commit.
3. spec self-review (placeholder/모순/범위/모호) → 사용자 spec 리뷰 게이트.
4. **writing-plans 스킬** 진입 (다른 스킬 호출 금지 — brainstorming 의 terminal state).

### 진입점 파일
- 교체 대상: [apps/_MyApp_/main.cpp](../../apps/_MyApp_/main.cpp) `WramupPlayer()` 의 `test_pattern` atlas + 단일 sprite/seq 블록.
- 확장 대상: [apps/_MyApp_/src/Entity/Player/PlayerActor.h](../../apps/_MyApp_/src/Entity/Player/PlayerActor.h) (`CreatePlayerActor`/`PlayerActorConfig`).
- 데이터: [apps/_MyApp_/src/Playable/Constants.h](../../apps/_MyApp_/src/Playable/Constants.h) (사용자 수정 완료).
- 엔진 참조: [src/sprite/](../../src/sprite/) (sprite_component / uniform_atlas / sprite_sequence_playable / sprite_frame_clip).

### 빌드/실행
```bash
cmake --preset ninja
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_     # 리소스 상대경로 → cd 필수
```

### 절대 하지 말 것 (이번 작업 한정)
- `Actor::AddComponent<SpriteRenderer>()` 를 한 Actor 에 2회 이상 (assert 크래시). 반드시 child 분리.
- `SetGrid(1, RowCount)` (UV 깨짐). 반드시 `SetGrid(RowCount, 1)`.
- parent 에 SpriteRenderer 를 남긴 채 child 도 추가 (이중 렌더). parent 의 직접 sprite 제거.
- `extern/sb7code` 수정 (불변 규칙).
