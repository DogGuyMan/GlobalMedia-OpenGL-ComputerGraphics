# Handoff — Stage PoliceTape 반투명 벽 + 마우스 Ground 픽킹 (2026-06-01)

> 대상: `_MyApp_` (탑다운 슈터). 브랜치 `game/module/ingame/temp`.
> 범위: `StageBuilder` 의 벽 시각화(PoliceTape 반투명 펜스 + 시간 스크롤 + 알파 컷오프) +
> 좌클릭 → Ground 월드 좌표 픽킹(수학 ray-plane) + 테스트 하니스(로그 + 노란 마커).
>
> **갱신 메모(2차)**: 1차에 만들었던 box2d "Raycast Ground body" 는 **제거됨** — 픽킹을
> 순수 수학 ray-plane(y=0)으로 확정했기 때문(아래 §3.5). 문서도 그에 맞춰 갱신.

---

## 1. 목표 / 한 줄 요약

- stage 의 **pickup 시각 Actor 제거** → 벽 4개를 **PoliceTape.png 반투명(Transparent) 세로 펜스**로 시각화.
- 벽 텍스처는 **시간에 따라 U 축으로 흐르고**(흐르는 테이프) **알파 컷오프**로 투명 영역 제거.
- **좌클릭 → 마우스 화면좌표 → Ground(y=0) 월드 좌표** 픽킹. 테스트로 **좌표 로그 + 노란 박스 마커 스폰**.

빌드 상태: **`cmake --build --preset ninja --target _MyApp_` 통과** (문서 작성 시점 마지막 빌드 기준).

---

## 2. 변경/신규 파일

| 파일 | 상태 | 내용 |
|---|---|---|
| `apps/_MyApp_/resources/shaders/transparent.vs` | 신규 | unlit VS — `uvScale` 타일링 + `uTime*uScrollSpeed` U 스크롤을 VS 에서 계산 |
| `apps/_MyApp_/resources/shaders/transparent.fs` | 신규 | unlit FS — `emissive` 샘플 + **alpha 컷오프**(`color.a < 0.1` discard) |
| `apps/_MyApp_/src/Stage/Components/MaterialTimeComponent.h` | 신규 | `Components::MaterialTime` — 누적 dt 를 머티리얼 `uTime` 에 매 프레임 기록 |
| `apps/_MyApp_/src/Stage/StageBuilder.cpp` | 수정 | pickup 제거 / 벽 = 반투명 PoliceTape 펜스 / `spawnWall` yRot 팩토리 |
| `apps/_MyApp_/src/InputHandler/PlayerController.{h,cpp}` | 수정 | 바인딩 로그 + 좌클릭 Ground 픽킹(수학 ray-plane) + 노란 마커 스폰 + `SetWorldCamera` |
| `apps/_MyApp_/src/Entity/Player/PlayerActor.h` | 수정 | `ControllerCfg.camera` 추가 + `CreatePlayerActor` 가 `SetWorldCamera` wiring |
| `apps/_MyApp_/main.cpp` | 수정 | `WramupPlayer` 에서 `pac.controller.camera = mCamera` |

> **되돌린 것(1차 → 제거됨)**: `wall_factory.h::CreateGroundActor`, StageBuilder 의 Ground 스폰,
> `filter.h::PhysicsLayer::Ground` — 모두 삭제. 물리 레이어는 다시 Player/Enemy/Bullet*/Wall/Pickup 6종.

---

## 3. 핵심 설계 결정 (왜 그렇게 했나)

### 3.1 벽 = Transparent Pass + emissive sampler
- `SJH::Pass::Kind::Transparent` 한 줄로 blend(SRC_ALPHA, ONE_MINUS_SRC_ALPHA) + depthWrite off + **cull off(양면)** 자동 도출 (`src/material/pass.h`).
- sampler 이름 `emissive`(라이팅 무관 자체발광). 텍스처 wrap **REPEAT**(uvScale>1 반복).

### 3.2 알파 컷오프는 **alpha 채널** 기준 (휘도 아님)
- PoliceTape.png 실측: 샘플 ~66% 가 `alpha≈0`, 그 투명 픽셀 RGB 휘도는 0~255 분산.
- "투명한데 밝은" 픽셀 때문에 **RGB 휘도 컷오프는 실패** → 반드시 `color.a` 로 컷.
- 현재 FS: `if (color.a < 0.1) discard; fragColor = color;`
  (이전의 `* tintColor * 10` 디버그 부스트는 최종본에서 제거됨 → `tintColor` uniform 은 현재 미사용.)

### 3.3 시간 스크롤 = VS + MaterialTime Component
- UV 수식 전부 VS: `vsTexCoord = aTexCoord*uvScale + vec2(uTime*uScrollSpeed, 0)`.
- 시간 공급은 벽마다 material instance 가 달라 per-actor `Components::MaterialTime` 가 누적 dt 를 자기 `uTime` 에 기록 (`Director::Update(dt)` 경로, main.cpp 무수정). 기본 `uScrollSpeed=0.3`.

### 3.4 벽 방향/물리 = canonical + yRot 통합
- `CreatePlane`(XY quad) → `EulerRot=(0,yRot,0)` 세로 펜스, scale `(arena*2, 1, 1)`.
- `spawnWall(name, center, yRot)` — half(물리 박스)는 yRot 에서 자동 도출:
  `(int)yRot % 180 == 0` → 가로 `(arena, wallH)`, 아니면 세로 `(wallH, arena)`.
- 4 호출: Top=180 / Bottom=0 / Left=270 / Right=90. per-wall material instance(uvScale override + 독립 uTime).

### 3.5 마우스 Ground 픽킹 = **순수 수학 ray-plane** (box2d Ground body 폐기)
- 좌클릭 → 화면(px) → NDC → **카메라 basis(owner WorldMatrix) + fov/aspect 로 ray 직접 구성** → **y=0 평면 교차**.
- vmath 가 일반 4x4 inverse 미제공 → proj·view 역행렬 대신 fov 기반 ray 구성.
- **box2d RayCast 를 쓰지 않는 이유**: 3D 마우스 ray 를 평평한 y=0 평면용 box2d 2D ray 로 변환하는 게 부자연 →
  수학 교차가 정확·간단(모든 엔진의 표준 ground picking). 그래서 1차의 Ground box2d body 는 제거.
- 코드: `PlayerController::TestPickGroundAndSpawnMarker()`.
  - 윈도우는 `glfwGetCurrentContext()`(Client 코드라 GLFW 직접 OK), 카메라는 config 로 주입(`SetWorldCamera`).
  - `dir.y≈0`(평행) / `t<0`(카메라 뒤) 가드.

### 3.6 GLFW + gl3w 헤더 충돌 회피
- `PlayerController.h → input/mouse_input.h` 가 최상단에서 `<GLFW/glfw3.h>` 를 끌어와 GLFW 자체 GL 헤더와
  엔진 gl3w 의 `PFNGL*` 가 충돌 → `.cpp` **맨 위(모든 include 이전)** 에 `#define GLFW_INCLUDE_NONE`.

---

## 4. 현재 stage 구성 (`CreateStageActor`)
1. 공유 자원: `EnsurePlane` / `EnsureWallMaterial`(Transparent+PoliceTape) / `EnsurePcbModel`.
2. Stage Actor + `StageState` Component.
3. 벽 4개 (`spawnWall` × 4) — 반투명 PoliceTape 펜스 + MeshRenderer + MaterialTime.
4. PCB 모델 Actor (`ModelSpawner::SpawnEntities`).

> 좌클릭 픽킹/마커는 stage 가 아니라 **PlayerController(좌클릭 핸들러)** 가 런타임에 `Director::Get().Root()` 로 스폰.

---

## 5. 테스트 하니스 (현재 동작)

- WASD(held) / G(press) / 좌클릭(press) → `spdlog::info("[input] ...")`.
  - ⚠ WASD held 는 프레임마다 로그 → 콘솔 시끄러움(확인용). 줄이려면 discrete(G/클릭)만 남기면 됨.
- 좌클릭 → `[pick] screen=(..) ndc=(..) -> ground=(x,y,z)` 로그 + 그 위치에 **노란 박스 마커** 스폰.
  - 마커 = `Mesh::CreateBox` + `simple.vs/fs`(baseColor 노랑) Opaque, scale 0.4.

---

## 6. 미해결 / 다음 작업 (Open items)

1. **마커 누적** — 클릭마다 GroundMarker 가 쌓임(테스트라 의도). 정리하려면 이전 마커 교체 또는 수명/sweep.
2. **카메라 basis 출처 검증** — ray 의 카메라 회전을 **owner WorldMatrix** 컬럼에서 추출(고정 −30° pitch 가
   owner Transform 에 반영된다는 전제). follow 컨트롤러가 회전을 owner 에 안 쓰고 lookAt 만 하면 어긋날 수 있음 →
   **실제 클릭해서 마커가 클릭 지점과 정합하는지 확인**. 어긋나면 `GetViewMatrix()` 기반 basis 로 전환.
3. **FS `tintColor` 미사용** — 현재 `fragColor = color;`. 틴트가 필요해지면 FS 에 `* tintColor` 복구.
4. **벽 높이 / PCB 관계** — 펜스 height=`1.0`(`wallVisualH`), 중심 y=0 이라 절반이 바닥 아래. 의도된 임시.
5. **uvScale 타일링 튜닝** — `tile = wallH*2`. 반복 밀도 조정 여지.
6. **픽킹의 실사용 연결** — 현재는 테스트(로그+마커)만. 실제 조준/이동 목표 등으로 `ground` 좌표를 소비하는 로직은 미구현.

---

## 7. Gotchas

- **셰이더 런타임 컴파일** — C++ 빌드 성공 ≠ GLSL 검증. transparent.vs/.fs, simple.vs/fs 오타는 실행 시 `CreateProgram` 로그로만 드러남.
- **clangd stale 경고** — 헤더 시그니처/멤버 변경 직후 "Too many arguments" / "No member named ..." 가 잠깐 뜸(재인덱싱 전). 실제 컴파일은 통과.
- **POST_BUILD 리소스 복사** — 새 셰이더는 빌드 시 `build_ninja/apps/_MyApp_/resources/` 로 자동 복사. 실행은 그 디렉토리에서.
- **GLFW_INCLUDE_NONE 위치** — 반드시 *첫 include 이전*. 늦게 두면 mouse_input.h 의 GLFW 가 이미 GL 을 끌어와 무효.
- **싱글톤 스폰** — 마커는 `SJH::ResourceRegistry::Get()` + `SJH::Scene::Director::Get().Root()` 로 자기완결(추가 plumbing 없음).

---

## 8. 빌드 / 실행

```bash
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_
# WASD/G → [input] 로그, 화면 클릭 → [pick] 로그 + 바닥에 노란 박스
```
