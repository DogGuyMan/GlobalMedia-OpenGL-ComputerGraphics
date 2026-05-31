# Handoff — Stage PoliceTape 반투명 벽 + Raycast Ground (2026-06-01)

> 대상: `_MyApp_` (탑다운 슈터). 브랜치 `game/module/ingame/temp`.
> 범위: `StageBuilder` 의 벽/바닥 시각화 + 물리. 이번 세션에서 pickup 시각 제거 → PoliceTape 반투명 펜스 →
> 시간 스크롤 셰이더 → 알파 컷오프 → Raycast 전용 Ground body 까지.

---

## 1. 목표 / 한 줄 요약

- 기존 stage 의 **pickup 시각 Actor 를 전부 제거**하고, 벽 4개를 **PoliceTape.png 를 두른 반투명(Transparent) 펜스**로 시각화.
- 벽 텍스처는 **시간에 따라 U 축으로 흐르고**(테이프가 흐르는 효과), **알파 컷오프**로 투명 영역을 버린다.
- 마우스 클릭(SceneFrame) → 월드 좌표 추출용 **Raycast 전용 Ground body**(충돌체 아님)를 stage 에 추가.

빌드 상태: **`cmake --build --preset ninja --target _MyApp_` 통과** (이 문서 작성 시점 마지막 빌드 기준).

---

## 2. 변경/신규 파일

| 파일 | 상태 | 내용 |
|---|---|---|
| `apps/_MyApp_/resources/shaders/transparent.vs` | 신규 | unlit VS — `uvScale` 타일링 + `uTime*uScrollSpeed` U 스크롤을 VS 에서 계산 |
| `apps/_MyApp_/resources/shaders/transparent.fs` | 신규 | unlit FS — `emissive` 샘플 + **alpha 컷오프**(`color.a < 0.1` discard) |
| `apps/_MyApp_/src/Stage/Components/MaterialTimeComponent.h` | 신규 | `Components::MaterialTime` — 누적 dt 를 머티리얼 `uTime` 프로퍼티에 매 프레임 기록 |
| `apps/_MyApp_/src/Stage/Factories/wall_factory.h` | 수정 | `CreateGroundActor(name, world, halfExtent)` 추가 (Raycast 전용 quad body) |
| `apps/_MyApp_/src/Stage/StageBuilder.cpp` | 수정 | pickup 제거 / 벽 = 반투명 PoliceTape 펜스 / `spawnWall` yRot 팩토리 / Ground 스폰 |
| `apps/_MyApp_/src/Physics/filter.h` | 수정 | `PhysicsLayer::Ground = 1ull << 6` 추가 |

> 참고: 위 신규 3파일은 이미 git 에 tracked 상태. 나머지 working-tree 변경(`Entity/*`, `InputHandler/*` 등)은
> 이번 세션 범위 밖(다른 작업 잔재)이므로 커밋 시 분리 권장.

---

## 3. 핵심 설계 결정 (왜 그렇게 했나)

### 3.1 벽 = Transparent Pass + emissive sampler
- `SJH::Pass::Kind::Transparent` 한 줄로 blend(SRC_ALPHA, ONE_MINUS_SRC_ALPHA) + depthWrite off + **cull off(양면)** 자동 도출 (`src/material/pass.h`).
- sampler 이름은 `emissive` (라이팅 무관 자체발광). `SJH::Uniforms::SetTexture(*mat, "emissive", tex, 0)`.
- 텍스처 wrap 은 **REPEAT** (`EnsureWallTexture` 에서 `tex->Bind(); tex->SetWrap(GL_REPEAT, GL_REPEAT)`) — uvScale 타일링이 1 을 넘어도 반복.

### 3.2 알파 컷오프는 **alpha 채널** 기준 (휘도 아님)
- PoliceTape.png 실측: 샘플의 ~66% 가 `alpha≈0`, 그 투명 픽셀의 RGB 휘도는 0~255 로 분산(평균 ~0.137).
- 즉 "투명한데 밝은" 픽셀이 많아 **RGB 휘도 컷오프는 실패**. 반드시 `color.a` 로 컷(빌보드 셰이더 `c.a < 0.01` 선례와 동일).
- 현재 FS: `if (color.a < 0.1) discard; fragColor = color;`

### 3.3 시간 스크롤 = VS + MaterialTime Component
- UV 수식 전부 **VS** 에 집약: `vsTexCoord = aTexCoord * uvScale + vec2(uTime*uScrollSpeed, 0.0)`.
  스크롤을 타일링 *이후* 더해 벽 크기와 무관하게 일정 속도.
- 시간 공급은 **벽마다 material instance 가 달라** per-actor `Components::MaterialTime` 가 누적 dt 를 자기 인스턴스 `uTime` 에 기록.
  (스카이박스의 `render()` 루프 방식과 달리 `Director::Update(dt)` 경로로 자기완결 → main.cpp 수정 불필요.)
- 기본값: `uScrollSpeed = 0.3` (공유 머티리얼에 SetFloat, 인스턴스가 상속).

### 3.4 벽 방향/물리 = canonical + yRot 통합
- `CreatePlane` 은 XY quad. 현재 벽은 `EulerRot=(0, yRot, 0)` 로 **세로 펜스**, scale `(arena*2, 1, 1)`.
- `spawnWall(name, center, yRot)` — half(물리 박스)는 yRot 에서 자동 도출:
  `(static_cast<int>(yRot) % 180) == 0` 이면 가로 `(arena, wallH)`, 아니면 세로 `(wallH, arena)`.
- 4 호출: Top=180 / Bottom=0 / Left=270 / Right=90.
- per-wall **material instance**(`CreateMaterialInstanceFrom`, key=`stage_wall_<name>`) — uvScale override + 독립 uTime 누적.

### 3.5 Ground = **Raycast 전용** (충돌체 아님, 비주얼 없음)
- 용도: 마우스 클릭(SceneFrame) → 월드 좌표 추출 시 RayCast 타깃.
- `CreateGroundActor(name, world, halfExtent)`:
  - static body, `SetAsBox(arena, arena)` → x,z ∈ [-arena, arena] quad, 중심(0,0), heightOffset 0 → **world y=0**.
  - `isSensor=true` + `maskBits=0` → **Player/Enemy(둘 다 dynamic body)를 막지도 트리거하지도 않음**.
  - `categoryBits = PhysicsLayer::Ground` → raycast 콜백이 바닥만 식별 가능.
- **근거**: box2d `b2World::RayCast` 는 categoryBits/maskBits 필터를 **무시**하고, **센서 fixture 도 히트**시킴.
  → maskBits=0 으로 충돌을 0 으로 만들어도 raycast 로는 잡힌다.
- 비주얼은 PCB 모델(y=-1.75)이 담당하므로 Ground 는 **MeshRenderer 없음**.

---

## 4. 현재 stage 구성 (`CreateStageActor`)
1. 공유 자원: `EnsurePlane` / `EnsureWallMaterial`(Transparent+PoliceTape) / `EnsurePcbModel`.
2. Stage Actor + `StageState` Component.
3. 벽 4개 (`spawnWall` × 4) — 반투명 PoliceTape 펜스 + MeshRenderer + MaterialTime.
4. **Ground** — `CreateGroundActor("Ground", world, arena)` (물리 전용).
5. PCB 모델 Actor (`ModelSpawner::SpawnEntities`).

---

## 5. 미해결 / 다음 작업 (Open items)

1. **Raycast consumer 미구현** — Ground body 만 만들었고, *마우스 클릭 → 월드 좌표* 추출 로직은 아직 없음.
   - 권장: 화면 클릭 → NDC → 카메라 inverse VP 로 3D ray → **y=0 평면 교차(순수 수학)** 가 가장 직접적.
   - box2d `RayCast` 를 굳이 쓰려면 3D ray 를 box2d 2D 좌표/ray 로 변환 + 콜백에서
     `fixture->GetFilterData().categoryBits == Physics::ToBits(PhysicsLayer::Ground)` 로 필터.
   - world ↔ box2d 매핑: `world (x, h, -y_box)` (physics_system.cpp). 즉 worldZ = -box2d.y.
2. **FS `tintColor` 미사용** — 현재 FS 는 `fragColor = color;` 라 `tintColor` uniform 이 inactive(셰이더에서 최적화 제거).
   머티리얼은 여전히 `tintColor` 를 SetVec4 하지만 silent skip. 틴트가 필요해지면 FS 에 `* tintColor` 복구.
   (이전에 디버그용 `* 10` 밝기 부스트가 있었으나 최종본에서 제거됨.)
3. **벽 높이 / PCB 관계** — 펜스 height=`1.0`(`wallVisualH`), 중심 y=0 이라 펜스가 y∈[-0.5,0.5] 차지(절반이 바닥 아래).
   바닥 위에 세우려면 heightOffset 또는 quad 피벗 조정 필요. 현재는 의도된 임시 상태.
4. **uvScale 타일링 튜닝** — `(arena*2/tile, wallH*2/tile)`, `tile = wallH*2`. 테이프 반복 밀도가 마음에 안 들면 tile 조정.
5. **fog plan 과 무관** — 같은 시점 열려 있던 `docs/superpowers/plans/2026-05-31-depth-based-fog.md` 는 별개 작업.

---

## 6. Gotchas

- **셰이더는 런타임 컴파일** — C++ 빌드 성공 ≠ GLSL 검증. transparent.vs/.fs 오타는 실행 시 `CreateProgram` 로그로만 드러남.
- **clangd stale 경고** — wall_factory.h 시그니처 변경 직후 StageBuilder 에서 "Too many arguments" 가 잠깐 뜰 수 있음(재인덱싱 전). 실제 컴파일은 통과.
- **POST_BUILD 리소스 복사** — 새 셰이더/텍스처는 빌드 시 `build_ninja/apps/_MyApp_/resources/` 로 자동 복사됨. 실행은 그 디렉토리에서.
- **material instance vs shared** — 벽 시각은 인스턴스(`stage_wall_<name>`)가 렌더. 공유 `stage_wall` 은 템플릿일 뿐 직접 렌더 안 함.

---

## 7. 빌드 / 실행

```bash
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
