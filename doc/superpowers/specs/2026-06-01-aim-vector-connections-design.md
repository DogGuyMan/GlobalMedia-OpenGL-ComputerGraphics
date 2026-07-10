# Spec — 조준 벡터 → 발사 / 플레이어 회전 / 손 회전 연결 (2026-06-01)

> 대상: `_MyApp_` (탑다운 슈터). 브랜치 `game/module/ingame/temp`.
> 전제: 좌클릭 마우스→Ground 픽킹(수학 ray-plane)이 이미 동작(`PlayerController`). 이 spec 은 그 조준 정보를
> **연속(매 프레임)** 으로 전환하고 3개 소비자(발사 / 플레이어 회전 / 손 회전)에 연결한다.

---

## 0. 확정 결정

| # | 결정 | 선택 |
|---|---|---|
| D1 | 조준 모델 | **연속** — 매 프레임 raycast, 손/플레이어가 커서를 상시 향함. 발사는 클릭 시 현재 조준. |
| D2 | "플레이어 회전" 의미 | **논리 facing(Transform.EulerRot.Y) + uFlipX**. (빌보드라 시각 회전은 손/flip 이 담당) |
| D3 | 발사 배선 | **`Weapon::UseWeapon(forward)` 경유**. 컨트롤러는 owner 의 Weapon 을 호출. |
| D4 | 손 방식 | **player 자식 Actor → Y facing 상속해 자동 궤도**. 손은 고정 ±벌림각 local 위치. |

---

## 1. 좌표 매핑 (불변 규칙)

`PhysicsSystem` 이 box2d → world 를 `tr.Translate = (p.x, heightOffset, -p.y)` 로 sync.
- **world.x = box2d.x**, **world.z = -box2d.y** → **box2d.y = -world.z**.
- box2d 발사 방향 `dir = (aimDir.x, -aimDir.z)`.
- 플레이어 forward = `-Z` (엔진 컨벤션). 조준각(Y, **degree**): `θ = degrees(atan2(-aimDir.x, -aimDir.z))`.
  - 검증: aim −Z → θ=0(기본), aim +X → θ=−90 (Ry(−90) 이 forward 를 +X 로).

---

## 2. 조준 소스 — `PlayerController` (연속)

### 상태 (멤버)
- `mAimPoint` (world, y≈0) — 클릭/커서 Ground 좌표.
- `mAimDirection` (world XZ, 정규화) — player → 커서 방향.
- `mAimAngleY` (degree) — `degrees(atan2(-dir.x, -dir.z))`.
- `mAimValid` (bool) — 이번 프레임 유효 교차 여부.

### `Update(float dt)` 흐름
1. 기존 이동 처리(`DoForward`) 유지.
2. `UpdateAim()` — 매 프레임 마우스→Ground raycast(기존 `TestPickGroundAndSpawnMarker` 의 raycast 코어를 분리).
   - 윈도우 `glfwGetCurrentContext()`, 커서 `glfwGetCursorPos`, 카메라 `mCamera`(이미 주입).
   - 유효 교차 없으면(평행/뒤/카메라 미주입) **직전 조준 유지**(`mAimValid=false`, 멤버 갱신 skip).
3. 플레이어 회전(§3) 적용.

> 디버그 **노란 마커는 좌클릭에서만** 스폰 유지(매 프레임 스폰 금지). 조준 갱신 자체는 로그 없음(스팸 방지).
> 기존 `[input]` 로그 중 WASD held 의 프레임당 스팸은 이 작업에서 **discrete(G/클릭)만 남기도록 정리**.

### getters
`GetAimDirection()` (world XZ) / `GetAimPoint()` / `GetAimAngleY()` — 외부 소비자용(현재는 내부에서 직접 사용).

### 좌클릭 핸들러
- `Weapon::UseWeapon((mAimDirection.x, -mAimDirection.z))` 호출 (box2d forward).
- 기존 `onFire()` (오디오/VFX Composite) 그대로 호출.
- 디버그 마커 스폰(기존).

---

## 3. 플레이어 회전 — `PlayerController::Update` 내

- `owner->GetTransform().EulerRot[1] = mAimAngleY;` (논리 facing — 자식 손이 상속).
- `SpriteRenderer.flipX = (mAimDirection.x < 0.0f);` (빌보드 좌우반전).
  - SpriteRenderer 는 player actor 의 형제 컴포넌트. **lazy lookup** — controller 는 CreatePlayerActor 에서
    sprite 보다 먼저 생성되므로 SetUp 시점엔 sprite 가 없음 → 첫 Update 에서 `GetOwner()->GetComponent<SpriteRenderer>()`
    조회 후 포인터 캐시(없으면 매번 재시도, null 이면 flip skip).

---

## 4. 발사 — `Weapon::UseWeapon`

### `Components::Weapon` 변경
- 멤버 추가: `b2World* mWorld = nullptr;` (+ ctor 또는 `SetWorld(b2World*)` 주입).
- `UseWeapon(vmath::vec2 box2dForward) const` 구현 (현 스텁 대체):
  ```
  owner world pos → box2d (x, -z)
  CreateBulletActor{ world=mWorld, pos, dir=box2dForward(정규화 가정),
                     speed(기본 15), damage=(int)Damage.GetValue(), lifetime(기본 3) }
  → SJH::Scene::Director::Get().Root().AddChild(...)
  ```
  - `mWorld==nullptr` 가드(early return + warn).
  - 구현 위치: 신규 `WeaponComponents.cpp` (bullet_factory + scene.h 무거운 include 를 헤더에서 분리). `ShowInfo()` 도 함께 정의.

### 플레이어에 Weapon 부착
- `PlayerActorConfig` 에 `WeaponCfg { int damage = 10; b2World* world = nullptr; }` 추가.
- `CreatePlayerActor`: physics 분기에서 `actor->AddComponent<Components::Weapon>(cfg.weapon.damage, "default")` + `SetWorld(cfg.weapon.world)`.
- `Bootstrap::PlayerBuilder`: `pac.weapon.damage`, `pac.weapon.world = deps.physicsWorld` 세팅.

### 컨트롤러 → Weapon 호출
- `PlayerController` 좌클릭 시 `GetOwner()->GetComponent<Components::Weapon>()->UseWeapon(box2dForward)`.
  - Weapon 은 CreatePlayerActor 에서 controller 와 같은 시점 부착 → 존재 보장(없으면 skip).

---

## 5. 손 회전 — `PlayerHands` / `PlayerSingleHand` (자식 상속 궤도)

### 구조
- 플레이어 actor 의 **자식 actor 2개**(예: "LeftHand", "RightHand"). 각 자식에 `PlayerSingleHand` + `SpriteRenderer`.
- 부모(player) `EulerRot.Y`(§3) 를 **상속** → 자식 손이 조준 방향으로 자동 궤도(scene graph WorldMatrix 합성).
- 각 손의 **local** Transform = forward(-Z) 기준 **고정 ±벌림각** 위치 (예: 반지름 r, 좌/우 ±spreadAngle):
  - `local = ( sin(±spread)*r, yOffset, -cos(±spread)*r )` (player-local, forward=-Z).
- 각도는 **부모가 담당** → `OrbitWithYAngle` 의 자체 각도 계산(`acos` 스텁) 제거. 손은 고정 local 위치만.
- **부착 주체**: `PlayerBuilder`(또는 `CreatePlayerActor`)가 player actor 에 `PlayerHands` 컴포넌트 1개 부착 →
  `PlayerHands::OnEnter` 가 자식 Hand actor 2개(각 `PlayerSingleHand`+`SpriteRenderer`)를 생성해 owner 에 AddChild.
- (선택, 후속) 조준 거리 retract: `mAimDistance`(player→click 거리) 를 노출해 r 를 클램프. 이번 범위 밖(고정 r 로 시작).

### 컴파일 정리 (현 WIP 수정)
- `PlayerSingleHand::Update()` / `PlayerHands::Update()` → **`Update(float dt) override`** (base `Component::Update(float)` 일치).
- `PlayerHands` 가 `PlayerSingleHand` 를 **값 멤버로 보유하는 현 구조 재검토**: 컴포넌트는 actor 에 부착되는 게 정통 →
  `PlayerHands` 는 OnEnter 에서 자식 Hand actor 2개를 생성·부착(각 PlayerSingleHand)하는 **조립 책임**으로 정리.

> ⚠ 손 **스프라이트 atlas/프레임/오프셋 수치** 등 비주얼 디테일은 사용자 WIP — 이 spec 은 *통합 구조(자식+상속 궤도)* 와
> *인터페이스(고정 local 위치, 부모 회전 상속)* 까지만 확정. 수치 튜닝은 구현 중 조정.

---

## 6. 영향 파일 (요약)

| 파일 | 변경 |
|---|---|
| `InputHandler/PlayerController.{h,cpp}` | 연속 aim(Update) + facing + flip + 좌클릭 Weapon 호출 + aim getters + 로그 정리 |
| `apps/_MyApp_/src/Entity/Components/WeaponComponents.h` | `mWorld` + `SetWorld` + `UseWeapon` 선언 |
| `apps/_MyApp_/src/Entity/Components/WeaponComponents.cpp` *(신규)* | `UseWeapon` / `ShowInfo` 정의 |
| `apps/_MyApp_/src/Entity/Player/PlayerActor.h` | `WeaponCfg` + CreatePlayerActor 에서 Weapon 부착 |
| `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` | `pac.weapon` 세팅 + (손 자식 부착은 PlayerHands OnEnter 가 담당 시 불필요) |
| `Entity/Player/PlayerHand.{h,cpp}` | Update 시그니처 수정 + 자식 조립 + 고정 local 위치 (사용자 WIP 협업) |
| `Entity/CMakeLists.txt` | WeaponComponents.cpp / PlayerHand.cpp 추가(필요 시) |

---

## 7. 비목표 (YAGNI / 이번 범위 밖)

- 조준 거리 기반 손 retract (고정 r 로 시작).
- 8방향 directional 스프라이트(빌보드 flip 으로 충분).
- 발사 쿨다운 / 탄종 / 재장전 (Weapon 은 단발 스폰만).
- ImGui 마우스 캡처 중 조준 억제(후속).

---

## 8. 검증 (수동)

1. 빌드 `cmake --build --preset ninja --target _MyApp_`.
2. 실행 후 마우스 이동 → 손이 커서 방향으로 상시 궤도, 플레이어 스프라이트 좌/우 flip.
3. 좌클릭 → 커서 방향으로 총알 발사 + 오디오/VFX + (디버그) 노란 마커.
4. 로그: 좌클릭 시 `[pick]`/`[aim]`, WASD/G discrete 로그(프레임 스팸 없음).
