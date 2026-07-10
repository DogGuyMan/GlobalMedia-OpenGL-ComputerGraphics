# 머리 위 분절형 체력 슬라이더 바 셰이더 — 설계 (2026-06-02)

`_MyApp_` 탑다운 슈터에, `Life::CurHp` 에 의존해 채워지는 **머리 위 월드 빌보드 체력바**를
커스텀 GLSL 셰이더 + `MeshRenderer(QuadMesh)` 로 구현한다. 레퍼런스
(`doc/RadisalSegmentedHealthBarBuiltIn.shader`, `doc/RadialBarShader.shadergraph`)는
**Radial** 이므로 *segment / spacing / remove* 로직만 1D(가로) 슬라이더로 이식한다.

## 1. 목표 / 비목표

**목표**
- `Life` 의 `GetHp()/GetMaxHp()` 비율을 셰이더 `uFill`(0..1) 로 매 프레임 구동.
- 레퍼런스의 **분절형(segmented)** 외형: N개 조각 + 조각 사이 간격(spacing) + 체력 감소 시
  조각이 우측부터 비워짐. 경계는 `fwidth` 안티에일리어싱.
- `Geometry::Plane()`(geometry.cpp:80-135 `BuildQuadIndexed`)로 만든 **QuadMesh** 를
  `MeshRenderer` 생성자에 주입.
- 카메라를 향하는 **구면 빌보드** (player 스프라이트 `billboard_atlas.vs` 와 동일 기법) → 탑다운
  카메라에서 항상 정면.
- `Life` 를 가진 **모든 액터에 재사용 가능한** 컴포넌트/팩토리. 이번 작업에서는 **플레이어 1개만** 실제 배선.

**비목표 (YAGNI)**
- 적(Enemy) 배선 — 팩토리는 재사용형이므로 `enemy_factory` 손대지 않고 후속에 한 줄로 채택.
- 데미지 표시 텍스트(이미 World Text 존재), 체력 회복 애니메이션/트윈, 색 그라데이션(녹→적)은
  기본 OFF (옵션 플래그 여지만 남김).
- 화면 HUD(스크린 스페이스) 모드 — 월드 빌보드로 확정.

## 2. 확정 결정

| # | 결정 | 값 |
|---|---|---|
| D1 | 카메라 공간 | **월드 빌보드** (worldCam 스테이지, 구면 빌보드) |
| D2 | 채움 스타일 | **분절형** (segment/spacing/remove, fwidth AA) |
| D3 | 빈 세그먼트 | **어두운 트랙** 표시 (`uBgColor`) — 조각 사이 간격(gap)만 투명 |
| D4 | 배선 위치 | **`PlayerBuilder.cpp` 내부** (추가만, 기존 로직 미변경) |
| D5 | 모듈 | 신설 **`apps/_MyApp_/src/HUD/`** STATIC → `MyApp::Client` |
| D6 | 구조 | **Free factory `AttachHealthBar`** + 자식 Actor + `MeshRenderer(QuadMesh, Material인스턴스)` + `HealthBarDriver` 컴포넌트 |
| D7 | Mesh/Program | **공유** (ResourceRegistry 캐시); **Material 은 per-instance** (각자 `uFill`) |
| D8 | bool uniform | **사용 안 함** — 전부 float/vec/int (알려진 `GL_BOOL` 디스패치 변동성 회피) |
| D9 | 색 그라데이션 | 기본 **OFF** (레퍼런스 단색 `_Color` 충실) |

## 3. 아키텍처

```
PlayerBuilder.cpp  ──(추가 1지점)──►  AttachHealthBar(playerActor, cfg)
                                          │
HUD::AttachHealthBar (free factory)       │  ResourceRegistry 위탁
   ├─ Program  "healthbar"      (공유, 1회 CreateProgram)
   ├─ Mesh     "ui_quad"        (공유, Geometry::Plane()→Mesh::Create)
   ├─ Material  per-instance    (SetProgram + SetPass(Transparent) + 초기 uniform)
   └─ child Actor "HealthBar"   (target.AddChild)
        ├─ Transform: 머리 위 오프셋 + 바 크기(Scale)
        ├─ MeshRenderer(uiQuad, matInstance, queueOffset=+10)
        └─ HealthBarDriver(targetILivable, matInstance)
```

- **소유**: Program/Mesh/Material 은 `ResourceRegistry` 소유(관례). 자식 Actor 는 타깃 Actor 의
  child 라 타깃과 수명 동조. `HealthBarDriver` 는 `ILivable*` 와 `Material*` 를 *비소유 참조*.
- **재사용**: `AttachHealthBar(Actor& target, const HealthBarConfig& cfg = {})` 는 `target` 의
  `Entity::ILivable` 만 알면 되므로 player/enemy 무관하게 동작.
- **Material 키**: per-instance 충돌 방지 위해 `"healthbar_" + <target 식별자>` (구현 시 타깃 이름 +
  필요 시 증가 카운터).

### 3.1 `HealthBarDriver` 컴포넌트 (`HpGrayscalePostFX` 패턴 그대로)

```cpp
class HealthBarDriver : public SJH::Scene::Component {
public:
    HealthBarDriver(Entity::ILivable* life, SJH::Material* mat);
    void OnEnter() override {}        // 참조는 ctor 주입 (factory 가 시점 보장)
    void OnExit()  override {}
    void Update(float /*dt*/) override {
        if (!mLife || !mMat) return;
        const int maxHp = mLife->GetMaxHp();
        if (maxHp <= 0) return;
        float r = float(mLife->GetHp()) / float(maxHp);
        r = r < 0.f ? 0.f : (r > 1.f ? 1.f : r);
        mMat->Properties.Floats["uFill"] = r;
    }
private:
    Entity::ILivable* mLife = nullptr;   // 비소유
    SJH::Material*    mMat  = nullptr;   // 비소유 (per-instance)
};
```

> 참조를 ctor 에서 주입하는 이유: factory 가 `target` 의 `Life` 가 이미 부착된 시점(플레이어 빌드
> 후반)에 호출되므로 `target.GetComponent<ILivable>()` 가 유효. `OnEnter` 자가해소 불필요.
> (대안으로 OnEnter 에서 `GetOwner()->GetParent()` 경유 해소도 가능하나 ctor 주입이 단순/명확.)

## 4. 셰이더 설계

엔진 유니폼 규약 고정: **`uModel` / `uView` / `uProj`** (`Const::UNI_MODEL/VIEW/PROJ`).
`MeshPassProcessor` 가 program 전환 시 `uView/uProj`, 매 draw 시 `uModel`(actor world transform)을 송신.

### 4.1 `resources/shaders/healthbar.vs` — 구면 빌보드

`billboard_atlas.vs` 기법 복제:
```glsl
#version 410 core
layout(location=0) in vec3 aPos;     // Geometry::Plane XY quad: (-0.5,-0.5,0)~(0.5,0.5,0)
layout(location=1) in vec3 aNormal;  // 미사용
layout(location=2) in vec2 aTexCoord;// (0,0)~(1,1)
uniform mat4 uModel, uView, uProj;
out vec2 vUv;
void main(){
    vec3 right  = vec3(uView[0][0], uView[1][0], uView[2][0]);
    vec3 up     = vec3(uView[0][1], uView[1][1], uView[2][1]);
    vec3 center = (uModel * vec4(0,0,0,1)).xyz;
    float sx = length(uModel[0].xyz);   // Transform.Scale.x → 바 가로폭
    float sy = length(uModel[1].xyz);   // Transform.Scale.y → 바 세로높이
    vec3 world = center + right*aPos.x*sx + up*aPos.y*sy;
    vUv = aTexCoord;
    gl_Position = uProj * uView * vec4(world, 1.0);
}
```
- atlas/flip/roll 유니폼 불필요(스프라이트와 달리 단일 절차적 바).
- 머리 위 오프셋은 **child Actor 의 Transform.Translate** 가 담당(`center` 가 흡수). 빌보드라
  오프셋은 월드 축으로 주되, 시각 튜닝으로 확정(예: 월드 up +Y 또는 카메라각 보정).

### 4.2 `resources/shaders/healthbar.fs` — 분절형 1D 슬라이더

레퍼런스 radial → 1D 이식 (radial 의 `atan2`/`polarCoordinates`/`circle` 대신 `vUv.x` 1축):

유니폼 계약:
| uniform | 타입 | 의미 | 기본값 |
|---|---|---|---|
| `uFill` | float | 체력 비율 0..1 (Driver 가 매 프레임 갱신) | 1.0 |
| `uColor` | vec4 | 채워진 조각 색 | (0.13, 1.0, 0.0, 1.0) (레퍼런스 녹색) |
| `uBgColor` | vec4 | 빈 조각(트랙) 색 (D3) | (0.0, 0.0, 0.0, 0.55) |
| `uSegmentCount` | float | 조각 수 | 5.0 |
| `uSegmentSpacing` | float | 조각 내 간격 비율(half-gap) | 0.08 |

알고리즘 (의사 GLSL):
```glsl
float u = vUv.x;                         // 0..1 바 길이축
float N = uSegmentCount;
float f = fract(u * N);                  // 현재 조각 내부 좌표 [0,1)
float edge = min(f, 1.0 - f);            // 조각 경계까지 거리
float aa = fwidth(u * N);                // 안티에일리어싱 폭 (레퍼런스 fwidth 동일 사상)
float body = smoothstep(uSegmentSpacing, uSegmentSpacing + aa, edge); // 간격=0, 몸체=1 (gap=투명)
float fill = 1.0 - smoothstep(uFill - aa, uFill + aa, u);             // u<uFill 채움, 경계 AA (우측부터 비워짐)
vec3  rgb  = mix(uBgColor.rgb, uColor.rgb, fill);
float a    = mix(uBgColor.a,  uColor.a,  fill) * body;                // 조각 사이 gap 은 투명
fragColor  = vec4(rgb, a);
```
- 결과: **채워진 조각 몸체 = `uColor`**, **빈 조각 몸체 = 어두운 `uBgColor`(트랙)**, **조각 사이 간격 =
  투명** → D2(분절형) + D3(어두운 트랙) 동시 충족.
- 레퍼런스의 연속 remove(smoothstep) 사상 유지 — 감소 중인 조각은 경계에서 부드럽게 비워짐.
- `bool` 미사용(D8). 색 그라데이션은 본 spec 범위 밖(D9) — 후속에 `mix(red,green,uFill)` 1줄로 옵션화 가능.

## 5. 머티리얼 / Pass

- `Material::SetPass(Pass::Kind::Transparent)` — 알파 블렌드 ON, depth-write OFF, back-to-front.
- `MeshRenderer.QueueOffset = +10` — 같은 Transparent 큐에서 스프라이트 등 뒤가 아닌 위로 정렬.
- 초기 uniform 은 factory 가 `Material::Properties` 에 set (uColor/uBgColor/uSegmentCount/
  uSegmentSpacing/uFill=1). 매 프레임 갱신은 `uFill` 만 (Driver).

## 6. 배선 (D4)

`PlayerBuilder.cpp` 의 플레이어 Actor 가 `Life` 부착 + `dir.Root().AddChild` 되기 **전/후**의
유효 지점에서 **추가 1지점**:
```cpp
// (Life 가 부착된 player Actor 가 준비된 직후)
TopdownShooter::HUD::AttachHealthBar(*spriteActor, /*cfg=*/{});
```
- 기존 라인 **수정 없음**(워킹트리 변경 중 파일 — additive only).
- `AttachHealthBar` 내부에서 `ResourceRegistry::Get()` 로 Program/Mesh/Material 확보.

## 7. CMake (D5)

- 신설 `apps/_MyApp_/src/HUD/CMakeLists.txt` → STATIC `MyApp_HUD` (또는 관례 ALIAS).
  - source: `HealthBarDriver.{h,cpp}`, `HealthBarFactory.{h,cpp}`.
  - link: `SJH::engine`(Material/Mesh/Geometry/MeshRenderer/ResourceRegistry/Actor) +
    Entity lib(`Entity::ILivable`).
- `apps/_MyApp_/src/CMakeLists.txt` 에 `add_subdirectory(HUD)` 추가 + `MyApp::Client` 우산에 합류.
- `Bootstrap`(PlayerBuilder 소속)이 `HUD` 를 볼 수 있도록 링크 순서/의존 확인.
- 셰이더 2종은 `resources/shaders/` 배치 → POST_BUILD 가 실행 파일 옆으로 복사(기존 관례).

## 8. 리스크 / 주의

- **PlayerBuilder.cpp 동시수정**: 현재 워킹트리 변경 중. 추가만 하고 기존 블록 미변경으로 충돌 최소화.
- **병렬 트랙**: `enemy_factory`(Task5), main.cpp Fog, Timer 코어 미접근 — 본 작업은 신규 파일 +
  PlayerBuilder 추가 1지점 + 신규 셰이더 2종 + HUD CMake 로 범위 한정.
- **VAO/EBO 오염**: WorldMesh 경로는 매 draw `BindVAO` 라 ScreenQuad 류 EBO 재핀 가드 불필요
  (해당 가드는 ScreenQuadStage 한정).
- **빌보드 오프셋 축**: 탑다운 카메라각에 따라 "머리 위" 가 월드 +Y 인지 보정 필요한지 시각 튜닝으로 확정.
- **Material per-instance 키 충돌**: 다중 액터 시 고유 키 보장(타깃 이름+카운터).

## 9. 산출물 목록

1. `apps/_MyApp_/resources/shaders/healthbar.vs` (신설)
2. `apps/_MyApp_/resources/shaders/healthbar.fs` (신설)
3. `apps/_MyApp_/src/HUD/HealthBarDriver.h` / `.cpp` (신설)
4. `apps/_MyApp_/src/HUD/HealthBarFactory.h` / `.cpp` — `AttachHealthBar` + `HealthBarConfig` (신설)
5. `apps/_MyApp_/src/HUD/CMakeLists.txt` (신설)
6. `apps/_MyApp_/src/CMakeLists.txt` — `add_subdirectory(HUD)` + Client 합류 (수정)
7. `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` — `AttachHealthBar` 호출 1지점 (추가)
