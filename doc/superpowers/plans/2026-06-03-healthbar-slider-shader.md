# 머리 위 분절형 체력 슬라이더 바 셰이더 — 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `Life::CurHp` 비율에 따라 채워지는 *머리 위 월드 빌보드 분절형 체력바*를 커스텀 GLSL 셰이더 + `MeshRenderer(QuadMesh)` 로 `_MyApp_` 플레이어에 부착한다.

**Architecture:** Free factory `HUD::AttachHealthBar(target)` 가 — ResourceRegistry 공유 Program(`healthbar.vs/.fs`) + 공유 QuadMesh(`Geometry::Plane`) + **per-instance Material**(Transparent) + 자식 Actor(`MeshRenderer` + `HealthBarDriver`) 를 조립한다. `HealthBarDriver` 가 매 프레임 `CurHp/MaxHp` 를 Material 의 `uFill` 에 기록하고, 프래그먼트 셰이더가 분절형 바를 그린다. 빌보드는 `cameraUp` 으로 머리 위에 띄워 카메라각 무관하게 정면화.

**Tech Stack:** C++17, OpenGL 4.1 / GLSL 410, CMake(Ninja), 엔진 모듈 `SJH::engine`(Material/Mesh/Geometry/MeshRenderer/ResourceRegistry/Program/Actor) + `MyApp::Entity`(ILivable).

> **프로젝트 규칙 (스킬 기본값 override):**
> - **TDD/단위테스트 안 함** — 본 기능은 셰이더/시각물. 검증 = *빌드 성공 + 실행 후 육안 확인*.
> - **커밋 메시지** `[dev] : ...` (한국어), **Co-Authored-By 미사용** (저장소 관례).
> - 신규 파일 위주 + 기존 파일 **추가(additive)만** — `PlayerBuilder.cpp`(워킹트리 수정중)·`enemy_factory`·main.cpp Fog·Timer 코어 미접근.
> - `doc/` 는 gitignore — 본 플랜/스펙은 커밋하지 않는 로컬 노트.

---

## File Structure

| 파일 | 책임 | 신규/수정 |
|---|---|---|
| `apps/_MyApp_/resources/shaders/healthbar.vs` | 구면 빌보드 + `cameraUp` 머리 위 오프셋, UV 전달 | 신규 |
| `apps/_MyApp_/resources/shaders/healthbar.fs` | 분절형 1D 슬라이더 (segment/spacing/fill, fwidth AA) | 신규 |
| `apps/_MyApp_/src/HUD/HealthBarDriver.h` / `.cpp` | `Life(ILivable)` HP 비율 → `uFill` 매 프레임 기록 컴포넌트 | 신규 |
| `apps/_MyApp_/src/HUD/HealthBarFactory.h` / `.cpp` | `AttachHealthBar` + `HealthBarConfig` — Program/Mesh/Material/자식Actor 조립 | 신규 |
| `apps/_MyApp_/src/HUD/CMakeLists.txt` | `MyApp::HUD` STATIC 정의 | 신규 |
| `apps/_MyApp_/src/CMakeLists.txt` | `add_subdirectory(HUD)` + `myapp_client` 합류 | 수정(추가) |
| `apps/_MyApp_/src/Bootstrap/CMakeLists.txt` | Bootstrap 에 `MyApp::HUD` 링크 | 수정(추가) |
| `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` | `AttachHealthBar(*result.SpriteActor)` 호출 1지점 | 수정(추가) |

---

## Task 1: 셰이더 2종 작성 (healthbar.vs / healthbar.fs)

**Files:**
- Create: `apps/_MyApp_/resources/shaders/healthbar.vs`
- Create: `apps/_MyApp_/resources/shaders/healthbar.fs`

- [ ] **Step 1: `healthbar.vs` 작성 (구면 빌보드 + cameraUp 머리 위 오프셋)**

`billboard_atlas.vs` 의 구면 빌보드 기법을 복제하되, `uHeadOffset` 만큼 `cameraUp` 으로 앵커를 올려 화면상 항상 머리 위에 뜨게 한다 (카메라각 무관).

```glsl
#version 410 core

// Geometry::Plane XY quad: aPos (-0.5,-0.5,0)~(0.5,0.5,0), V=0 at bottom
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;   // 미사용
layout(location = 2) in vec2 aTexCoord; // (0,0)~(1,1)

uniform mat4 uModel;       // 액터 world transform (center + scale 흡수)
uniform mat4 uView;
uniform mat4 uProj;
uniform float uHeadOffset; // 화면상 '위'(cameraUp) 로 띄우는 거리 (월드 단위)

out vec2 vUv;

void main()
{
    // 카메라 right/up = view 행렬의 row vector (orthonormal rotation transpose)
    vec3 cameraRight = vec3(uView[0][0], uView[1][0], uView[2][0]);
    vec3 cameraUp    = vec3(uView[0][1], uView[1][1], uView[2][1]);

    // uModel 에서 center + size 흡수
    vec3 center = (uModel * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    float sx = length(uModel[0].xyz); // Transform.Scale.x → 바 가로폭
    float sy = length(uModel[1].xyz); // Transform.Scale.y → 바 세로높이

    // 머리 위 앵커: 화면상 cameraUp 방향으로 오프셋 (카메라각 독립)
    vec3 anchor   = center + cameraUp * uHeadOffset;
    vec3 worldPos = anchor + cameraRight * aPos.x * sx + cameraUp * aPos.y * sy;

    vUv = aTexCoord;
    gl_Position = uProj * uView * vec4(worldPos, 1.0);
}
```

- [ ] **Step 2: `healthbar.fs` 작성 (분절형 1D 슬라이더)**

레퍼런스 radial(`RadisalSegmentedHealthBarBuiltIn.shader`) 의 segment/spacing/remove + `fwidth` AA 를 가로(U)축 1D 로 이식. `bool` uniform 미사용(float/vec 만).

```glsl
#version 410 core

in vec2 vUv;

uniform float uFill;           // 체력 비율 0..1 (HealthBarDriver 가 매 프레임 갱신)
uniform vec4  uColor;          // 채워진 조각 색
uniform vec4  uBgColor;        // 빈 조각(트랙) 색
uniform float uSegmentCount;   // 조각 수
uniform float uSegmentSpacing; // 조각 내 half-gap 비율 (fract 단위)

out vec4 fragColor;

void main()
{
    float u = clamp(vUv.x, 0.0, 1.0);
    float N = max(uSegmentCount, 1.0);

    // 현재 조각 내부 좌표 + 조각 경계까지 거리
    float f    = fract(u * N);
    float edge = min(f, 1.0 - f);

    // fwidth 안티에일리어싱 (레퍼런스 사상)
    float aaSeg = fwidth(u * N);
    float aaU   = fwidth(u);

    // 조각 몸체 = 1, 조각 사이 간격(gap) = 0 (gap 은 투명)
    float body = smoothstep(uSegmentSpacing, uSegmentSpacing + aaSeg, edge);

    // 좌→우 채움: u < uFill 채워짐 (우측부터 비워짐), 경계 AA
    float fill = 1.0 - smoothstep(uFill - aaU, uFill + aaU, u);

    // 채워진 조각=uColor, 빈 조각=어두운 uBgColor(트랙). gap 은 alpha*body 로 투명.
    vec3  rgb = mix(uBgColor.rgb, uColor.rgb, fill);
    float a   = mix(uBgColor.a,  uColor.a,  fill) * body;

    fragColor = vec4(rgb, a);
}
```

- [ ] **Step 3: 파일/헤더 확인**

Run: `head -1 apps/_MyApp_/resources/shaders/healthbar.vs apps/_MyApp_/resources/shaders/healthbar.fs`
Expected: 두 파일 모두 첫 줄 `#version 410 core`.

(선택 — glslangValidator 설치 시 문법 사전 검사: `glslangValidator -S vert apps/_MyApp_/resources/shaders/healthbar.vs` / `glslangValidator -S frag apps/_MyApp_/resources/shaders/healthbar.fs`. desktop builtin 관련 경고는 무시 가능. **실제 GLSL 컴파일 검증은 Task 3 런타임** — 엔진이 셰이더 InfoLog 를 콘솔에 출력.)

- [ ] **Step 4: 커밋**

```bash
git add apps/_MyApp_/resources/shaders/healthbar.vs apps/_MyApp_/resources/shaders/healthbar.fs
git commit -m "[dev] : HealthBar 셰이더 — 구면 빌보드 vs + 분절형 1D 슬라이더 fs"
```

---

## Task 2: HUD 모듈 (Driver + Factory + CMake 배선, 호출 전)

이 태스크 종료 시 `_MyApp_` 가 **컴파일·링크 성공**해야 한다 (아직 `AttachHealthBar` 호출 없음 — HUD lib 의 컴파일 정합성만 검증).

**Files:**
- Create: `apps/_MyApp_/src/HUD/HealthBarDriver.h`
- Create: `apps/_MyApp_/src/HUD/HealthBarDriver.cpp`
- Create: `apps/_MyApp_/src/HUD/HealthBarFactory.h`
- Create: `apps/_MyApp_/src/HUD/HealthBarFactory.cpp`
- Create: `apps/_MyApp_/src/HUD/CMakeLists.txt`
- Modify: `apps/_MyApp_/src/CMakeLists.txt` (line 11 직후 `add_subdirectory(HUD)`; client 우산 합류)
- Modify: `apps/_MyApp_/src/Bootstrap/CMakeLists.txt` (PRIVATE 링크에 `MyApp::HUD`)

- [ ] **Step 1: `HealthBarDriver.h` 작성**

```cpp
#ifndef __TOPDOWNSHOOTER_HUD_HEALTHBAR_DRIVER_H__
#define __TOPDOWNSHOOTER_HUD_HEALTHBAR_DRIVER_H__

#include "apps/_MyApp_/src/Physics/Components.Interfaces.h" // TopdownShooter::Entity::ILivable
#include "scene/actor.h"                             // SJH::Scene::Component

namespace SJH
{
	class Material;
} // namespace SJH

namespace TopdownShooter::HUD
{
	/// @brief 타깃 액터 Life(ILivable) 의 HP 비율(CurHp/MaxHp, [0,1])을 매 프레임 체력바 Material 의
	///        `uFill` 에 기록. HpGrayscalePostFX 패턴 — 참조는 ctor 주입(팩토리가 시점 보장).
	class HealthBarDriver : public SJH::Scene::Component
	{
	  public:
		HealthBarDriver(Entity::ILivable *life, SJH::Material *material);

		void OnEnter() override {}
		void OnExit() override {}
		void Update(float dt) override;

	  private:
		Entity::ILivable *mLife = nullptr; // 비소유
		SJH::Material    *mMat  = nullptr; // 비소유 (per-instance)
	};
} // namespace TopdownShooter::HUD

#endif // __TOPDOWNSHOOTER_HUD_HEALTHBAR_DRIVER_H__
```

- [ ] **Step 2: `HealthBarDriver.cpp` 작성**

```cpp
#include "apps/_MyApp_/src/HUD/HealthBarDriver.h"

#include "material/material.h" // SJH::Material::Properties.Floats

namespace TopdownShooter::HUD
{
	HealthBarDriver::HealthBarDriver(Entity::ILivable *life, SJH::Material *material)
	    : mLife(life), mMat(material)
	{
	}

	void HealthBarDriver::Update(float /*dt*/)
	{
		if (!mLife || !mMat) return;
		const int maxHp = mLife->GetMaxHp();
		if (maxHp <= 0) return;

		float ratio = static_cast<float>(mLife->GetHp()) / static_cast<float>(maxHp);
		if (ratio < 0.0f) ratio = 0.0f;
		else if (ratio > 1.0f) ratio = 1.0f;

		mMat->Properties.Floats["uFill"] = ratio;
	}
} // namespace TopdownShooter::HUD
```

- [ ] **Step 3: `HealthBarFactory.h` 작성**

```cpp
#ifndef __TOPDOWNSHOOTER_HUD_HEALTHBAR_FACTORY_H__
#define __TOPDOWNSHOOTER_HUD_HEALTHBAR_FACTORY_H__

#include <vmath.h>

namespace SJH::Scene
{
	class Actor;
} // namespace SJH::Scene

namespace TopdownShooter::HUD
{
	/// @brief 머리 위 분절형 체력바 외형/배치 설정 (전부 기본값 보유).
	struct HealthBarConfig
	{
		vmath::vec4 fillColor      = vmath::vec4(0.13f, 1.0f, 0.0f, 1.0f); // 채워진 조각 (레퍼런스 녹색)
		vmath::vec4 bgColor        = vmath::vec4(0.0f, 0.0f, 0.0f, 0.55f); // 빈 조각 트랙
		float       segmentCount   = 5.0f;
		float       segmentSpacing = 0.08f;
		float       headOffset     = 1.2f;                  // cameraUp 방향 머리 위 거리
		vmath::vec2 size           = vmath::vec2(1.2f, 0.18f); // 바 가로×세로
	};

	/// @brief target 의 Life(ILivable) 에 묶인 체력바 자식 Actor 를 생성·부착.
	///        Program/Mesh 는 ResourceRegistry 공유 캐시, Material 은 per-instance.
	///        target 에 ILivable 이 없거나 셰이더/메시 확보 실패 시 no-op (silent).
	void AttachHealthBar(SJH::Scene::Actor &target, const HealthBarConfig &cfg = {});
} // namespace TopdownShooter::HUD

#endif // __TOPDOWNSHOOTER_HUD_HEALTHBAR_FACTORY_H__
```

- [ ] **Step 4: `HealthBarFactory.cpp` 작성**

```cpp
#include "apps/_MyApp_/src/HUD/HealthBarFactory.h"

#include "apps/_MyApp_/src/HUD/HealthBarDriver.h"

#include "apps/_MyApp_/src/Physics/Components.Interfaces.h" // TopdownShooter::Entity::ILivable
#include "GL/gl3w.h"                                 // GL_TRIANGLES
#include "material/material.h"
#include "material/pass.h"
#include "object/geometry.h"
#include "object/mesh.h"
#include "render/mesh_renderer.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"

#include <memory>
#include <string>

namespace TopdownShooter::HUD
{
	namespace
	{
		constexpr char kQuadKey[]    = "ui_quad";
		constexpr char kProgramKey[] = "healthbar";
		constexpr char kVsPath[]     = "./resources/shaders/healthbar.vs";
		constexpr char kFsPath[]     = "./resources/shaders/healthbar.fs";

		// per-instance Material 키 고유화 — 다중 액터/중복 이름 충돌 회피.
		int gInstanceCounter = 0;
	} // namespace

	void AttachHealthBar(SJH::Scene::Actor &target, const HealthBarConfig &cfg)
	{
		auto *life = target.GetComponent<Entity::ILivable>();
		if (!life) return; // ILivable 없으면 부착 의미 없음 — silent no-op

		auto &reg = SJH::ResourceRegistry::Get();

		// 1) 공유 Program (healthbar.vs/.fs) — 없으면 생성.
		SJH::Program *prog = reg.FindProgram(kProgramKey);
		if (!prog)
			prog = reg.CreateProgram(kProgramKey, kVsPath, kFsPath);
		if (!prog) return; // 셰이더 컴파일 실패 (콘솔 InfoLog) — 부착 포기

		// 2) 공유 QuadMesh (Geometry::Plane — XY quad, z=0) — 없으면 등록.
		SJH::Mesh *quad = reg.FindMesh(kQuadKey);
		if (!quad)
		{
			SJH::MeshData data = SJH::Geometry::Plane();
			quad = reg.RegisterMesh(kQuadKey, SJH::Mesh::Create(data.vertices, data.indices, GL_TRIANGLES));
		}
		if (!quad) return;

		// 3) per-instance Material — 고유 키, Transparent pass, 초기 uniform.
		const std::string matKey =
		    "healthbar_" + target.GetName() + "_" + std::to_string(gInstanceCounter++);
		SJH::Material *mat = reg.CreateSharedMaterial(matKey);
		if (!mat) return;
		mat->SetProgram(prog);
		mat->SetPass(SJH::Pass::Kind::Transparent);
		mat->Properties.Vec4s["uColor"]           = cfg.fillColor;
		mat->Properties.Vec4s["uBgColor"]         = cfg.bgColor;
		mat->Properties.Floats["uSegmentCount"]   = cfg.segmentCount;
		mat->Properties.Floats["uSegmentSpacing"] = cfg.segmentSpacing;
		mat->Properties.Floats["uHeadOffset"]     = cfg.headOffset;
		mat->Properties.Floats["uFill"]           = 1.0f;

		// 4) 자식 Actor — local Translate=0 (빌보드가 부모 center 에서 cameraUp 으로 띄움),
		//    Scale 이 바 가로×세로. 부모 world transform 이 center 를 결정.
		auto bar = std::make_unique<SJH::Scene::Actor>("HealthBar");
		bar->GetTransform().Scale = vmath::vec3(cfg.size[0], cfg.size[1], 1.0f);
		auto *barPtr = target.AddChild(std::move(bar));

		// 5) MeshRenderer(QuadMesh, Material, +10) + Driver(Life→uFill).
		//    QueueOffset +10 — 같은 Transparent 큐(3000) 안에서 스프라이트 등 위로 정렬.
		barPtr->AddComponent<SJH::Scene::MeshRenderer>(quad, mat, /*queueOffset*/ 10);
		barPtr->AddComponent<TopdownShooter::HUD::HealthBarDriver>(life, mat);
	}
} // namespace TopdownShooter::HUD
```

- [ ] **Step 5: `HUD/CMakeLists.txt` 작성** (Playable 모듈 패턴 복제)

```cmake
# 머리 위 분절형 체력바 — HealthBarDriver(Life→uFill) + AttachHealthBar 팩토리.
# 신규 STATIC lib. SJH::engine(Material/Mesh/Geometry/MeshRenderer/ResourceRegistry/Program/Actor) +
# MyApp::Entity(ILivable) 의존.
add_library(myapp_hud STATIC
    HealthBarDriver.cpp
    HealthBarFactory.cpp
)
add_library(MyApp::HUD ALIAS myapp_hud)

target_include_directories(myapp_hud
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(myapp_hud
    PUBLIC
        SJH::engine    # Actor/Component/Material/Mesh/Geometry/MeshRenderer/ResourceRegistry/Program/vmath
        MyApp::Entity  # TopdownShooter::Entity::ILivable
)
target_compile_features(myapp_hud PUBLIC cxx_std_17)
```

- [ ] **Step 6: `apps/_MyApp_/src/CMakeLists.txt` 에 HUD 등록**

`add_subdirectory(Playable)` (현 11행) 바로 다음 줄에 추가:

```cmake
add_subdirectory(Playable)   # 연출 foundation — PlayableDirector + PostFXRegistry (Spawns/Entity 뒤)
add_subdirectory(HUD)        # <- 추가 (MyApp::HUD — 체력바, Bootstrap 앞)
```

그리고 `myapp_client` INTERFACE 링크 목록(`MyApp::Playable` 줄 다음)에 추가:

```cmake
    MyApp::Playable # 연출 foundation — PlayableDirector + PostFXRegistry
    MyApp::HUD      # <- 추가 (체력바)
    MyApp::Bootstrap # Warmup 보일러플레이트 추출 빌더 (WorldScene/Player/Audio)
```

- [ ] **Step 7: `apps/_MyApp_/src/Bootstrap/CMakeLists.txt` 에 HUD 링크 추가**

`target_link_libraries(myapp_bootstrap ...)` 의 `PRIVATE` 블록에 추가 (`MyApp::Playable` 줄 다음):

```cmake
        MyApp::Playable      # PlayableDirector (PlayerBuilder onFire/onDamage 마이그레이션)
        MyApp::HUD           # <- 추가 (AttachHealthBar — PlayerBuilder .cpp)
        game_deps            # box2d / tweeny / fmod / effekseer (.cpp 전용)
```

- [ ] **Step 8: 재configure + 빌드 (HUD 컴파일/링크 검증)**

Run:
```bash
cmake --preset ninja
cmake --build --preset ninja --target _MyApp_
```
Expected: 에러 없이 `myapp_hud` 컴파일 + `_MyApp_` 링크 성공 (마지막에 `Linking CXX executable ... _MyApp_`). `AttachHealthBar` 는 아직 미호출이지만 HUD lib 의 .cpp 가 컴파일되어 타입/시그니처 정합성이 검증된다.

- [ ] **Step 9: 커밋 (path-scoped — 인덱스에 무관한 staged 파일들이 있으므로 절대 bare commit 금지)**

```bash
git add apps/_MyApp_/src/HUD apps/_MyApp_/src/CMakeLists.txt apps/_MyApp_/src/Bootstrap/CMakeLists.txt
git commit -m "[dev] : HUD 모듈 — HealthBarDriver + AttachHealthBar 팩토리 (배선 전)" -- \
  apps/_MyApp_/src/HUD apps/_MyApp_/src/CMakeLists.txt apps/_MyApp_/src/Bootstrap/CMakeLists.txt
```
> ⚠ `-- <paths>` 로 *내 파일만* 커밋. 인덱스에 병렬 작업의 staged 파일이 있어 bare `git commit` 는 그것까지 쓸어담는다.

---

## Task 3: PlayerBuilder 배선 + 실행 후 육안 확인

**Files:**
- Modify: `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` (include 1줄 + 호출 1줄, 추가만)

- [ ] **Step 1: include 추가**

`apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` 의 include 블록에서 `#include "apps/_MyApp_/src/Playable/SpriteFxPlayable.h"` (현 15행) 다음 줄에 추가:

```cpp
#include "apps/_MyApp_/src/Playable/SpriteFxPlayable.h"    // hit-flash / dissolve 스프라이트 연출(Task6)
#include "apps/_MyApp_/src/HUD/HealthBarFactory.h"         // <- 추가 (머리 위 분절형 체력바)
```

- [ ] **Step 2: `AttachHealthBar` 호출 추가**

`PlayerBuilder.cpp` 의 `result.SpriteActor->AddComponent<TopdownShooter::Entity::PlayerHands>();` (현 211행) **다음 줄**에 추가 (액터가 이미 entered + Life 부착 완료 시점):

```cpp
		result.SpriteActor->AddComponent<TopdownShooter::Entity::PlayerHands>();

		// 머리 위 분절형 체력바 — Life(ILivable) HP 비율을 uFill 로 구동 (월드 빌보드, Transparent).
		TopdownShooter::HUD::AttachHealthBar(*result.SpriteActor);
```

- [ ] **Step 3: 빌드**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: 에러 없이 링크 성공 (`Linking CXX executable ... _MyApp_`).

- [ ] **Step 4: 커밋 (path-scoped — bare commit 금지)**

```bash
git add apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp
git commit -m "[dev] : HealthBar 배선 — PlayerBuilder 에 AttachHealthBar (플레이어 머리 위 체력바)" -- \
  apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp
```
> ⚠ `-- <path>` 로 *PlayerBuilder.cpp 만* 커밋. 인덱스의 병렬 작업 staged 파일을 건드리지 않는다.

- [ ] **Step 5: 실행 + 육안 확인**

Run:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
Expected (육안):
1. **셰이더 컴파일 성공** — 콘솔에 `healthbar.vs`/`.fs` 관련 InfoLog 에러가 없음. (에러 나오면 GLSL 문법 수정 후 Task 1 재커밋.)
2. **체력바 표시** — 플레이어 스프라이트 *머리 위* 에, 카메라를 향한(빌보드) 가로 바. 5개 **녹색 조각** + 조각 사이 얇은 투명 간격 + 빈 영역은 **어두운 트랙**(가득 찼을 땐 전부 녹색). 플레이어가 이동/카메라가 따라가도 항상 정면 + 머리 위 유지.
3. **HP 감소 반응** — 적(WaveController 스폰)이 플레이어에 접촉하면 `Life::DoDamaged` 로 HP 감소 → 바의 **우측 조각부터** 녹색 → 어두운 트랙으로 비워짐. (HP 비율은 기존 화면 grayscale 효과와 동일 소스라, 무채색화와 동시에 진행되는지 비교 가능. `G` 키는 hit FX(비네팅)만 — HP 는 안 깎임에 유의.)

> **튜닝 (필요 시 — `HealthBarConfig` 기본값 조정):** 바가 너무 작/크거나 위치가 어긋나면
> `apps/_MyApp_/src/HUD/HealthBarFactory.h` 의 `headOffset`(머리 위 거리) / `size`(가로×세로) /
> `segmentCount` / `fillColor` 를 조정 후 재빌드. 조정했다면 한 줄 커밋
> (`git commit -m "[dev] : HealthBar 시각 튜닝 — headOffset/size 조정"`).

---

## Self-Review 체크

- **Spec 커버리지:** D1 빌보드(Task1 vs)·D2 분절형(Task1 fs)·D3 어두운 트랙(fs `uBgColor`)·D4 PlayerBuilder 배선(Task3)·D5 HUD 모듈(Task2 CMake)·D6 factory+자식+MeshRenderer+Driver(Task2)·D7 Program/Mesh 공유+Material per-instance(Factory)·D8 bool 미사용(fs float만)·D9 색 그라데이션 OFF(단색 uColor) — 전부 태스크에 매핑됨.
- **플레이스홀더:** 없음 — 모든 코드/명령/기대출력 명시.
- **타입 정합성:** `HealthBarDriver(Entity::ILivable*, SJH::Material*)` ctor 가 Factory 의 `AddComponent<HealthBarDriver>(life, mat)` 와 일치. `Mesh::Create(vertices, indices, GL_TRIANGLES)` 의 indices 는 `std::vector<uint32_t>`(MeshData.indices 와 동일 타입). `Properties.Vec4s`/`Floats` 키명(`uColor`/`uBgColor`/`uSegmentCount`/`uSegmentSpacing`/`uHeadOffset`/`uFill`)이 vs/fs uniform 명과 1:1 일치. `Pass::Kind::Transparent` 존재 확인됨. `MeshRenderer(Mesh*, Material*, int)` 시그니처 일치.
- **참고(스펙과의 차이):** 머리 위 오프셋을 child Transform.Translate(월드축) → **`uHeadOffset`(cameraUp) 셰이더 오프셋**으로 정밀화 (카메라각 독립 — 스펙이 "오프셋 축 시각 튜닝 필요"로 플래그한 항목을 view-independent 로 해소).
