# Entity Ground Decals — 그림자 + 피격범위 XZ 평면 (설계)

- **날짜**: 2026-06-04
- **대상**: `apps/_MyApp_` (탑다운 슈터)
- **상태**: 설계 확정 — 구현 대기
- **관련 메모리**: `[[uniform_atlas_delegates_image_texture]]`, `[[stb_image_owner_resource_registry]]`, `[[pass_component_postfx_pattern]]`

## 1. 목표

Player / Enemy 엔티티의 발밑에 **두 장의 XZ 바닥 평면(MeshRenderer)** 을 렌더링한다.

1. **그림자** — `EntityShadow.png` (256² RGBA, 중앙 검정 + 가장자리 알파 페이드) 로 가짜 드롭섀도.
2. **피격범위 원** — `Circle_albedo.png` (2048² RGBA, 흰 원 + 외부 알파) 를 반투명 적색 틴트로 `PhysicsComponent` 의 실제 충돌 반경 위에 **항상 표시**.

기존에 만들어진 `SJH::Mesh::CreatePlane()` 평면 메시를 **재사용**하고, 단순 baseColor 멀티플라이 셰이더(`simple_texture.vs/.fs`)로 그린다.

## 2. 사전 발견 — 셰이더 버그 (반드시 선행 수정)

`apps/_MyApp_/resources/shaders/simple_texture.vs` 는 **현재 컴파일 불가**다. `vsPosition` / `vsNormal` / `vsTexCoord` 에 값을 쓰지만 `out` 선언이 빠져 있다 (FS 는 같은 이름을 `in` 으로 받음). 아래 3줄을 추가한다:

```glsl
out vec3 vsPosition;
out vec3 vsNormal;
out vec2 vsTexCoord;
```

`simple_texture.fs` 의 `fragColor = texture(uTex, vsTexCoord) * baseColor;` 로직은 정상 — 무수정.

엔진 호환: `uModel`(per-draw) / `uView` / `uProj`(per-program) 는 `SceneRenderer` 가 자동 송신. `baseColor` / `uTex` 는 Material 프로퍼티로 송신. 따라서 이 셰이더는 수정 후 그대로 사용 가능.

두 텍스처 모두 RGBA(알파 보유) 이므로 **`Pass::Kind::Transparent`** (알파블렌드 + depth-write off + cull off) 로 그려야 보인다.

## 3. 계층 구조 — depth +1 (사용자 확정)

데칼을 엔티티 root 에 직접 붙이지 않는다. root 아래 **렌더 전용 자식 / 바닥 데칼 전용 자식** 을 분리해, Tween 워블 회전·스케일과 그림자·피격원의 Transform 을 독립시킨다.

### Enemy
```
Enemy(root)         physics / Life / AI / PlayableDirector / 체력바   ── translate·facing만, scale=(1,1,1)
 ├─ renderActor     SpriteRenderer + ParallelPlayable 워블(scale-pulse ∥ z회전)
 └─ groundActor     MeshRenderer ×2  (그림자 / 피격원)                ── 워블 무관, 독립 Transform
```

### Player
이미 8개 방향 그룹이 root 직속 자식이고 root 는 워블이 없으므로 `renderActor` 불필요. `groundActor` 만 추가한다.
```
PlayerSprite(root)  ├─ player_dir_*(8그룹) ├─ Hands ├─ groundActor(그림자 / 피격원)
```

### 연출 클러스터(`AttachEntityPresentation`) 무수정 — 핵심 근거
`SpriteFxPlayable.cpp` 의 `ForEachSpriteRenderer(root)` 는 **root 자신 + 직속 자식** 의 `SpriteRenderer` 를 훑는다. director 를 root 에 두고 hit/dissolve 타깃을 root 로 유지하면, sprite 가 `renderActor`(root 직속 자식) 로 내려가도 hit-flash·dissolve 가 그대로 도달한다. `groundActor` 는 `SpriteRenderer` 가 없어 자동 skip 된다. → `AttachEntityPresentation` / `SpriteFxPlayable` 수정 불필요.

### 스케일 이중적용 없음 — 핵심 근거
`CreateEnemyActor` / `BuildPlayer` 모두 root `Transform.Scale = (1,1,1)` 기본이다. 따라서 `renderActor`(시각 베이스 + 워블) 와 `groundActor`(반경 기반 스케일) 가 root 스케일과 곱해지지 않는다. 체력바는 root(스케일 1) 자식이라 영향 없음.

## 4. 구현 방식 — 헬퍼 파일 없이 빌더 내부 인라인 (사용자 확정)

`GroundDecalFactory.{h,cpp}` 같은 별도 공유 모듈을 **만들지 않는다.** `PlayerBuilder.cpp` / `EnemyBuilder.cpp` 각각의 내부에 데칼 부착 코드를 직접 인라인 작성한다. 파일 내 중복은 각 빌더의 익명 네임스페이스(`namespace { ... }`) file-local 함수로 정리할 수 있다(선택). 두 빌더가 동일 `ResourceRegistry` 키를 쓰므로 자원 자체는 1회만 생성되고 이후 공유된다.

### 4.1 공유 자원 (find-or-create — 스폰마다 호출돼도 1회 생성)

`ResourceRegistry::Create*` 는 "이미 있으면 nullptr" 이므로 **Find 먼저, 없으면 Create** 패턴 필수.

| 종류 | 키 | 생성 |
|---|---|---|
| Program | `"simple_texture"` | `CreateProgram(key, "./resources/shaders/simple_texture.vs", "./resources/shaders/simple_texture.fs")` |
| Mesh | `"_ground_plane"` | `RegisterMesh(key, SJH::Mesh::CreatePlane())` |
| Texture | `"entity_shadow"` | `CreateTexture(key, SJH::Image::Load(key, "resources/texture/EntityShadow.png").get())` |
| Texture | `"hit_range_circle"` | `CreateTexture(key, SJH::Image::Load(key, "resources/texture/Circle_albedo.png").get())` |
| Material(shared) | `"shadow_decal_mat"` | 아래 셋업 |
| Material(shared) | `"hitrange_decal_mat"` | 아래 셋업 |

`Image::Load(...).get()` 의 임시 `unique_ptr<Image>` 는 `CreateTexture` 호출이 끝나는 full-expression 까지 살아 GPU 업로드가 완료된다(`SpriteFxPlayable.cpp` 의 dissolve.png 패턴과 동일).

### 4.2 공유 머티리얼 셋업 (find-or-create 블록 내부 1회)

```cpp
SJH::Material* shadowMat = reg.FindSharedMaterial("shadow_decal_mat");
if (!shadowMat) {
    shadowMat = reg.CreateSharedMaterial("shadow_decal_mat");
    shadowMat->SetProgram(prog);
    shadowMat->SetPass(SJH::Pass::Kind::Transparent);
    shadowMat->Properties.Textures["uTex"]    = { shadowTex, 0 };
    shadowMat->Properties.Vec4s["baseColor"]  = vmath::vec4(1.0f, 1.0f, 1.0f, 0.5f); // 텍스처 흑 RGB 유지, 알파 0.5
}
// hitrange_decal_mat: 동일 패턴, uTex=circleTex, baseColor=(1,0,0,0.45) 반투명 적
```

크기는 머티리얼이 아니라 **자식 액터 Transform.Scale** 로 처리하므로 머티리얼은 모든 엔티티가 공유 가능(틴트·텍스처 동일).

### 4.3 groundActor + 데칼 자식 생성

```cpp
auto groundOwned = std::make_unique<SJH::Scene::Actor>("groundActor");
auto* ground = root.AddChild(std::move(groundOwned));   // root = 엔티티 액터

// 그림자 데칼
auto shadowOwned = std::make_unique<SJH::Scene::Actor>("decal_shadow");
auto* shadow = ground->AddChild(std::move(shadowOwned));
shadow->AddComponent<SJH::Scene::MeshRenderer>(plane, shadowMat, /*queueOffset*/ 0);
shadow->GetTransform().EulerRot[0] = -90.0f;                 // XY → XZ 로 눕힘 (degree)
shadow->GetTransform().Scale       = vmath::vec3(shadowD, 1.0f, shadowD);
shadow->GetTransform().Translate   = vmath::vec3(0.0f, 0.02f, 0.0f); // 바닥 z-fight 회피

// 피격범위 데칼 (그림자보다 위 = 더 큰 QueueOffset)
auto circleOwned = std::make_unique<SJH::Scene::Actor>("decal_hitrange");
auto* circle = ground->AddChild(std::move(circleOwned));
circle->AddComponent<SJH::Scene::MeshRenderer>(plane, hitrangeMat, /*queueOffset*/ 1);
circle->GetTransform().EulerRot[0] = -90.0f;
circle->GetTransform().Scale       = vmath::vec3(hitD, 1.0f, hitD);
circle->GetTransform().Translate   = vmath::vec3(0.0f, 0.03f, 0.0f);
```

> `CreatePlane()` 는 1×1 XY quad(원점 중심, 정점 x·y ∈ [-0.5, 0.5], z=0, normal +Z, UV 0..1). `EulerRot[0] = -90°` 로 XZ 바닥에 눕혀 위(+Y)를 보게 한다. Scale = (d, 1, d) 로 지름 d 만큼 확대.

### 4.4 크기 자동 산출 — 물리 fixture 반경 (사용자 확정)

`FindPhysics(&root)->GetBody()` 의 **첫 fixture** shape 에서 반경을 읽는다 (빌더 시점엔 body 가 eager 생성 완료).

```cpp
float hitRadius = 0.5f; // fallback
if (auto* phys = TopdownShooter::Physics::Components::FindPhysics(&root)) {
    if (b2Body* body = phys->GetBody()) {
        if (b2Fixture* fx = body->GetFixtureList()) {
            const b2Shape* sh = fx->GetShape();
            if (sh->GetType() == b2Shape::e_circle) {
                hitRadius = sh->m_radius;
            } else if (sh->GetType() == b2Shape::e_polygon) {
                auto* poly = static_cast<const b2PolygonShape*>(sh);
                float maxExt = 0.0f;
                for (int i = 0; i < poly->m_count; ++i) {
                    maxExt = std::max(maxExt, poly->m_vertices[i].Length());
                }
                hitRadius = maxExt;
            }
        }
    }
}
const float hitD    = hitRadius * 2.0f;        // 피격원 = 지름
const float shadowD = hitRadius * 2.0f * 1.2f; // 그림자 = 약간 크게
```

- Enemy: `CircleBody(ENEMY_RADIUS)` → `e_circle`, `m_radius = ENEMY_RADIUS`.
- Player: `BoxBody(size 1×1)` → `e_polygon`, half-extent 0.5 → 정점 거리 √0.5 ≈ 0.707 → hitD ≈ 1.41. (정점 대각 거리 기준. 박스 변 기준으로 맞추려면 `m_vertices[i].x` 등 축별 max 사용 — 구현 시 결정.)

> 박스의 반경 정의(대각 vs 변)는 구현 단계에서 시각 확인 후 택일. 기본은 대각(`Length()`) 으로 시작.

## 5. 빌더별 변경 요약

### `apps/_MyApp_/resources/shaders/simple_texture.vs`
- `out` 3줄 추가 (§2).

### `EnemyBuilder.cpp` ([현재 41행 `AttachSpriteLayer(*enemy, ...)`](apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp#L41))
1. `renderActor` 자식 생성 후, `AttachSpriteLayer(*enemy, ...)` → `AttachSpriteLayer(*renderActor, ...)`.
2. 워블 블록(현재 50–83행): `self = enemy.get()` → `self = renderActor`, `baseScale = renderActor->GetTransform().Scale`, `ParallelPlayable` 도 `renderActor->AddComponent` (또는 enemy 에 둬도 무방 — 람다 타깃만 renderActor 면 됨. 일관성 위해 renderActor 권장).
3. `AttachEntityPresentation(*enemy, pres)` — **무수정** (director 는 root, ForEachSpriteRenderer 가 renderActor 도달).
4. §4 인라인 데칼 코드 추가 (`AttachGroundDecals` 헬퍼 없이 직접).

> ⚠ `renderActor` 는 root 의 **직속 자식** 이어야 `ForEachSpriteRenderer` 가 도달한다. groundActor 와 형제.

### `PlayerBuilder.cpp` ([`BuildPlayer`](apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp#L129))
1. `renderActor` 불필요 (방향 그룹이 이미 자식, root 워블 없음).
2. §4 인라인 데칼 코드 추가 — `spriteActor`(root) 에 groundActor 자식 부착. Physics 는 `spriteActor` 의 `BoxBody`.

## 6. 그리기 순서 / 블렌딩

- 두 데칼 모두 `Pass::Kind::Transparent` → queue ≥ 2500, **back-to-front 깊이 정렬**, `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`, depth-write off, cull off.
- 동일 깊이(바닥 평면) 동률은 `QueueOffset` 으로 분리: 그림자(0) < 피격원(1) → 피격원이 그림자 위.
- `Translate.y` 소량(0.02 / 0.03) 으로 바닥 메시와의 z-fighting 회피.

## 7. YAGNI — 의도적 제외

- **디버그 토글** — 피격원은 항상 표시(사용자 확정). Visible 플래그 토글 훅 없음.
- **per-fixture N장** — 첫 fixture 만 사용. 다중 fixture 엔티티 미지원.
- **데칼 dissolve 동조** — groundActor 는 `SpriteRenderer` 가 없어 death dissolve 에 안 엮임. root despawn 시 자식과 함께 사라짐(`OnExit`).
- **공유 헬퍼 모듈** — 인라인 작성(사용자 확정).

## 8. 검증

- 빌드 GREEN (`cmake --build --preset ninja --target _MyApp_`).
- 셰이더 컴파일 성공 (수정 전엔 링크/컴파일 에러).
- 육안: 플레이어/적 발밑에 반투명 그림자 + 충돌반경에 맞는 반투명 적색 원. 적 워블 시 그림자·원은 **회전·스케일 안 함**(독립 확인). 적 이동 시 데칼이 따라옴(translate 상속).

---

# Round 2 — Player 데칼 spin 해결 + renderActor/aimPivot 분리 + Y offset Constants (2026-06-04 추가)

## R2.0 배경 — 육안 검증 후 피드백

Round 1 구현 후 육안 확인: 데칼 둘 다 보이지만 (1) **플레이어 데칼이 커서 따라 회전(spin)**, (2) **적 데칼 Y offset 조절 필요**(하드코딩 → Constants 이전, 사용자 직접 튜닝).

## R2.1 결정적 발견 — 무엇이 실제로 회전하는가 (탐색 워크플로 6-fan-out)

| 요소 | 셰이더 | root `EulerRot[1]=mAimAngleY` 영향 |
|---|---|---|
| 방향 스프라이트(32 레이어) | `billboard_atlas` (카메라 정면, X/Y회전 무시·Z롤만) | **시각 회전 없음** (빌보드) |
| 그림자/피격원 데칼 | `simple_texture` (uModel 전체 적용) | **그대로 회전 → spin** ❌ |
| 손(Hand) | `billboard_atlas` | 위치만 궤도(의도), 스프라이트는 카메라 정면 |

- `PlayerController.cpp:231` `owner->GetTransform().EulerRot[1] = mAimAngleY` 의 **유일 소비자 = 손 궤도**(WorldMatrix 상속). 무기 발사 방향·discrete facing은 별개 경로(`mAimAngleY`는 weapon에 별도 vec2 전달, facing은 `SetFacing` visibility 토글).
- 즉 spin의 실체 = **groundActor(데칼)가 회전하는 root의 자식**이라서. 스프라이트는 빌보드라 원래 안 돎.
- Transform: `GetLocalMatrix = T * Rz·Ry·Rx * S` (EulerRot degrees, XYZ), `world = parent_world * local` (`actor.cpp:80-89`). 자식이 부모 Y회전 상속.
- `ForEachSpriteRenderer`(`SpriteFxPlayable.cpp`)는 **root + 직속자식 1단계만** 훑음 → 스프라이트가 renderActor 하위(손자)로 가면 hit-flash/dissolve 미도달.

## R2.2 목표 계층 (사용자 확정 — aimPivot 회전이전 + 손 재부모 + renderActor 추가)

```
PlayerSprite(root)   게임로직(Physics/Movement/Weapon/Life/Controller/Director/HpGrayscalePostFX/PlayerHands) — 비회전
 ├─ renderActor      32 방향 스프라이트 레이어 (빌보드; SetFacing이 visibility 토글) — 비회전
 ├─ aimPivot         EulerRot[1]=mAimAngleY ← 손 child 2개가 여기서 궤도
 │     └─ LeftHand / RightHand
 └─ groundActor      그림자/피격원 데칼 — 비회전 ✅ (spin 해결)
```

Enemy 계층은 Round 1 그대로 (`Enemy(root) → renderActor(sprite+워블) · groundActor(데칼)`).

## R2.3 핵심 설계 결정 2건 (탐색으로 단순화)

**① 손은 child 액터만 재부모, `PlayerHands` 컴포넌트는 root 유지.**
controller↔hands 양방향 결합: hands→controller(`GetAimScreenT`, PlayerHand.cpp:99) + controller→hands(`TriggerFire`, PlayerController.cpp:106). 컴포넌트를 옮기면 둘 다 sibling 조회가 깨짐. → **컴포넌트는 root, OnEnter가 만드는 손 *child 액터*만 aimPivot 아래 부착**. 양방향 조회 무수정. (orbit parent를 ctor 주입; 미주입 시 owner fallback.)

**② `ForEachSpriteRenderer`를 서브트리 전체 재귀로.**
스프라이트가 renderActor 손자가 되므로 1단계 훑기로는 hit-flash/dissolve 미도달. 재귀화 → 도달. **영향 집합은 현행과 동일**(현재도 root 직속 32스프라이트+손 전부 적용 중; 재귀해도 동일 집합 + groundActor 데칼은 SpriteRenderer 없어 skip). 회귀 없음. `AttachEntityPresentation` target=root 무수정.

## R2.4 변경 목록

1. **`apps/_MyApp_/src/Playable/SpriteFxPlayable.cpp`** — `ForEachSpriteRenderer` 재귀화(서브트리 DFS).
2. **`apps/_MyApp_/src/InputHandler/PlayerController.{h,cpp}`** — `SetFacingPivot(SJH::Scene::Actor*)` 주입 + member `mFacingPivot`. line 231: `mFacingPivot` 있으면 그것을, 없으면 owner(root) 회전(하위호환 fallback). 나머지 게임로직(UpdateAim/DoForward/OnFirePressed/mAimAngleY 계산) 무수정.
3. **`apps/_MyApp_/src/Entity/Player/PlayerHand.{h,cpp}`** — `PlayerHands` ctor에 orbit parent 주입(또는 setter). `OnEnter`가 손 child를 `mOrbitParent ?: owner` 아래 부착. 컴포넌트는 root, Update/TriggerFire 조회 무수정.
4. **`apps/_MyApp_/src/Bootstrap/Constants.h`** — `PLAYER_DECAL_Y=0.02f` / `ENEMY_DECAL_Y=0.02f` / `DECAL_CIRCLE_Y_DELTA=0.01f` (namespace `TopdownShooter::Bootstrap`).
5. **`AttachGroundDecals` 시그니처 `(Actor& root, float baseY)`** — Player/Enemy 두 빌더의 인라인 helper 동일 변경. shadow `Translate.y = baseY`, circle `Translate.y = baseY + DECAL_CIRCLE_Y_DELTA`. 호출: PlayerBuilder `AttachGroundDecals(*spriteActor, PLAYER_DECAL_Y)`, EnemyBuilder `AttachGroundDecals(*enemy, ENEMY_DECAL_Y)`. PlayerBuilder는 `apps/_MyApp_/src/Bootstrap/Constants.h` include 추가.
6. **`apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp`** 배선:
   - `spriteActor = CreatePlayerActor` 직후 `renderActor` + `aimPivot` = `spriteActor->AddChild(...)`.
   - `BuildPlayerDirectionalGroups(*spriteActor, ...)` → `(*renderActor, ...)`.
   - controller 주입 블록에 `controller->SetFacingPivot(aimPivot)`.
   - `AddComponent<PlayerHands>()` → `AddComponent<PlayerHands>(aimPivot)`.

## R2.5 정합성 (회귀 없음 근거)

- **데칼 spin 해결**: root 비회전 → groundActor(자식) 비회전. root `EulerRot[1]` 유일 소비자였던 손은 aimPivot로 이전.
- **손 궤도 유지**: 손 child가 aimPivot(회전) 아래 → 궤도 유지. `PlayerHands`는 root → 양방향 조회 무수정.
- **hit-flash/dissolve 유지**: 재귀 `ForEachSpriteRenderer`(target=root) → renderActor 하위 32 스프라이트 + aimPivot 하위 손 전부 도달 (현행과 동일 집합).
- **discrete facing 유지**: `DirGroup`가 `SpriteRenderer*` 보유 → 부모 위치 무관, `SetFacing`/`RefreshDirectional` 무수정 동작.
- **root EulerRot[1] 타 소비자 0**: 무기는 별도 vec2, raycast는 카메라, movement는 입력 — root 회전 미참조 (구현 시 grep 재확인).
- **billboard 무영향**: renderActor/aimPivot 회전은 빌보드 스프라이트 시각에 무영향(Z롤만 반영). 손 스프라이트는 카메라 정면 유지, 위치만 궤도.

## R2.6 검증 (Round 2)

- 빌드 GREEN.
- 육안: ①플레이어 데칼(그림자·원)이 커서 회전 시 **spin 안 함** ②손은 여전히 조준 방향으로 궤도 ③방향 스프라이트 facing 정상(8방향 토글) ④피격 hit-flash/사망 dissolve 정상 ⑤적 데칼 Y가 `ENEMY_DECAL_Y` 상수로 조절됨.

## R2.7 YAGNI / 비-범위 (사용자 확정)

- facing 계산(`QuantizeByThreshold`/`InRange`, lines 35-59)의 **물리적 컴포넌트 추출 안 함** — 계산은 PlayerController에 유지, 효과(SetFacing)만 renderActor 하위 대상. (`// ! 리팩토링` TODO는 다음 기회.)
- renderActor에 wobble 미추가 (구조만; 향후 여지).
