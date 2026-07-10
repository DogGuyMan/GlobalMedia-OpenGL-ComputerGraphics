# Warmup 보일러플레이트 추출 Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** main.cpp 의 5 Warmup 메서드 ~250줄 중 *엔진 공통 보일러* ~130줄을 SJH 모듈로 추출 — main.cpp 가 *씬 구성 + 렌더 셋업 + UI 셋업 직접 수행* 에서 *Composition Root + game 도메인만 보유* 로 책임 분리.

**Architecture:** D-1 옵션 1 — 기존 모듈 확장 + 새 헤더 1개. `SJH::common::window_helper` (A1 GLFW 의존 격리) + `SJH::Scene::compound_actor` 확장 (A2/A5 PreBuilt Actor factory) + 새 헤더 `SJH::render::render_pipeline` (A3/A4 자유 함수 family). Pure factory 일관 — caller (main.cpp) 가 Director.AddChild / mStages.push_back 책임. 새 STATIC 모듈 0개 — Composition Root 가 main.cpp 유지.

**Tech Stack:** C++17, CMake, GLFW (window_helper 한정), SJH::common/scene/render/buffer/resource_registry/material, sb7 application loop.

**Spec:** [`doc/superpowers/specs/2026-05-31-warmup-boilerplate-extraction-design.md`](../specs/2026-05-31-warmup-boilerplate-extraction-design.md)

---

## File Structure

```
src/common/
├── window_helper.h          (NEW)   GLFW 의존 격리 — GLFWwindow* forward + GetFramebufferInfo
├── window_helper.cpp        (NEW)   <GLFW/glfw3.h> include + glfwGetFramebufferSize + aspect
└── CMakeLists.txt           (+1)    window_helper.cpp 등록

src/scene/
├── compound_actor.h         (+10)   CreateScreenCameraActor + CreateSkyboxActor 선언 추가
└── compound_actor.cpp       (+28)   두 factory 본구현

src/render/
├── render_pipeline.h        (NEW)   SetupDefaultPipeline + BuildPostFXChain + 두 struct
├── render_pipeline.cpp      (NEW)   본구현 — 기존 WramupSceneRenderer / WarmupPassRenderer 이주
└── CMakeLists.txt           (+1)    render_pipeline.cpp 등록

apps/_MyApp_/
└── main.cpp                 (±130)  자유 함수 호출로 교체 + Warmup 5 메서드 중 3 완전 제거
                                     (+ WarmupSkybox 축소, CreateAndRegisterWorldCamera 유지)
```

---

## Summary

| # | Task | 변경 파일 | 핵심 결과 | 검증 |
|---|------|-----------|-----------|------|
| T1 | A1 — `SJH::GetFramebufferInfo` window_helper 신규 | `window_helper.{h,cpp}` (신규) + `src/common/CMakeLists.txt` (+1) | `SJH::common` 에 GLFW window helper 합류 | 빌드 성공 (사용처 0 — T4 에서 사용) |
| T2 | A2+A5 — `Scene::CreateScreenCameraActor` + `CreateSkyboxActor` 확장 | `compound_actor.{h,cpp}` (수정) | `SJH::scene` 에 2 factory 추가 | 빌드 성공 (사용처 0) |
| T3 | A3+A4 — `Render::SetupDefaultPipeline` + `BuildPostFXChain` render_pipeline 신규 | `render_pipeline.{h,cpp}` (신규) + `src/render/CMakeLists.txt` (+1) | `SJH::render` 에 자유 함수 family 합류 | 빌드 성공 (사용처 0) |
| T4 | main.cpp 통합 — 5 자유 함수 호출 + Warmup 3 메서드 제거 + Skybox 축소 + 멤버 타입 변경 | `apps/_MyApp_/main.cpp` (±130) | startup() ~45줄 + private Warmup 메서드 -125줄 | 빌드 + 시각 회귀 V1~V11 사용자 검증 |

**의존 그래프**: T1 → T2 → T3 → T4. T1~T3 는 *독립* (병렬 가능) 하나 순차 진행 권장 — 디버깅 시 회귀 원인 task 식별 쉬움.

**invariant**:
- T1, T2, T3 commit 시점 — 시각 동작 *완전 동일* (자유 함수가 main.cpp 에서 호출 안 됨, 빌드만 합류)
- T4 commit 시점 — 시각 동작 *완전 동일* (보일러 추출 = 행위 보존)

---

## Task 1: A1 — `SJH::GetFramebufferInfo` window_helper 신규

**Files:**
- Create: `src/common/window_helper.h`
- Create: `src/common/window_helper.cpp`
- Modify: `src/common/CMakeLists.txt` (line 1~3 의 `add_library` source list)

---

- [ ] **Step 1: window_helper.h 작성**

Create `src/common/window_helper.h`:

```cpp
#ifndef __SJH_COMMON_WINDOW_HELPER_H__
#define __SJH_COMMON_WINDOW_HELPER_H__

struct GLFWwindow; // forward — common 본체가 glfw 전체 의존 받지 않도록 격리 (D-5)

namespace SJH
{
	/// @brief 프레임버퍼 크기 + aspect 1 호출 packed return.
	struct FramebufferInfo
	{
		int   Width  = 0;
		int   Height = 0;
		float Aspect = 0.0f;
	};

	/// @brief glfwGetFramebufferSize + aspect 계산.
	/// @details Retina HiDPI 의 physical framebuffer 기준. Aspect = Width/Height (Height==0 시 0).
	FramebufferInfo GetFramebufferInfo(GLFWwindow* window);
}

#endif // __SJH_COMMON_WINDOW_HELPER_H__
```

---

- [ ] **Step 2: window_helper.cpp 작성**

Create `src/common/window_helper.cpp`:

```cpp
#include "common/window_helper.h"

#include <GLFW/glfw3.h>

namespace SJH
{
	FramebufferInfo GetFramebufferInfo(GLFWwindow* window)
	{
		FramebufferInfo info;
		if (!window)
			return info;
		glfwGetFramebufferSize(window, &info.Width, &info.Height);
		info.Aspect = (info.Height > 0)
		    ? static_cast<float>(info.Width) / static_cast<float>(info.Height)
		    : 0.0f;
		return info;
	}
}
```

---

- [ ] **Step 3: CMakeLists.txt 에 window_helper.cpp 등록**

Modify `src/common/CMakeLists.txt`:

기존:
```cmake
add_library(sjhopengl_common STATIC
    common.cpp
)
```

→ 변경 후:
```cmake
add_library(sjhopengl_common STATIC
    common.cpp
    window_helper.cpp        # GLFW 의존 격리 — GetFramebufferInfo 자유 함수
)
```

`target_link_libraries` 의 PUBLIC 에 GLFW 추가 — `project_deps` 가 이미 GLFW 포함하므로 *별도 추가 불필요* (window_helper.cpp 가 `<GLFW/glfw3.h>` 만 include, 다른 consumer 는 GLFWwindow* forward 만 사용).

> **그러나 `sjhopengl_common` 의 현재 의존성에 `project_deps` 가 없다 — 이 task 에서 link 추가 필요**:

기존:
```cmake
target_link_libraries(sjhopengl_common PRIVATE spdlog)
```

→ 변경 후:
```cmake
target_link_libraries(sjhopengl_common
    PRIVATE
        spdlog
        project_deps    # window_helper.cpp 의 <GLFW/glfw3.h>
)
```

---

- [ ] **Step 4: 빌드 확인**

Run from repo root:
```bash
cmake --preset ninja
cmake --build --preset ninja --target _MyApp_
```

Expected:
- cmake configure 성공 — `sjhopengl_common` 의 `window_helper.cpp` 인식
- 컴파일 성공 — `sjhopengl_common.a` 에 GetFramebufferInfo symbol 합류
- link 성공 — `_MyApp_` 의 다른 의존 (`SJH::common` link 하는 모든 모듈) 영향 0

Troubleshooting:
- `'GLFW/glfw3.h' file not found` → Step 3 의 `project_deps` PRIVATE link 누락. `cmake/Dependency.cmake` 의 `project_deps` INTERFACE 타겟에 GLFW include path 포함 확인.

---

- [ ] **Step 5: Commit**

Run:
```bash
git add src/common/window_helper.h src/common/window_helper.cpp src/common/CMakeLists.txt
git commit -m "[feat] : SJH::GetFramebufferInfo window helper 신규

spec doc/superpowers/specs/2026-05-31-warmup-boilerplate-extraction-design.md T1 (A1).

- src/common/window_helper.{h,cpp} 신규 — GLFWwindow* forward decl 헤더 + .cpp 만 glfw 전체 의존
- struct FramebufferInfo {Width, Height, Aspect} packed return
- D-5 — SJH::common 본체가 glfw 전체 의존 받지 않도록 격리
- src/common/CMakeLists.txt — window_helper.cpp 등록 + project_deps PRIVATE link 추가

T1 commit — 사용처 0 (T4 에서 main.cpp 가 호출). 시각 동작 변경 0."
```

Expected: commit 성공, `git log --oneline -1` 출력에 `[feat] : SJH::GetFramebufferInfo`.

---

## Task 2: A2+A5 — `Scene::CreateScreenCameraActor` + `CreateSkyboxActor` 확장

**Files:**
- Modify: `src/scene/compound_actor.h` — 두 factory 선언 추가
- Modify: `src/scene/compound_actor.cpp` — 두 factory 본구현

> 참고: `src/scene/CMakeLists.txt` 는 *수정 없음* — `compound_actor.cpp` 가 이미 source list 에 등록되어 있음.

---

- [ ] **Step 1: compound_actor.h 에 선언 추가**

Modify `src/scene/compound_actor.h` — 기존 `CreateCameraActor` 선언 *바로 아래* 에 추가.

먼저 file 의 시작 include 부분에 forward decl 추가 (`SJH::Framebuffer`, `SJH::Mesh`, `SJH::Material`):

기존:
```cpp
#ifndef __SJH_SCENE_COMPOUND_ACTOR_H__
#define __SJH_SCENE_COMPOUND_ACTOR_H__

#include "scene/actor.h"
#include <memory>
#include <string>
#include <vmath.h>

namespace SJH::Scene
{
```

→ 변경 후:
```cpp
#ifndef __SJH_SCENE_COMPOUND_ACTOR_H__
#define __SJH_SCENE_COMPOUND_ACTOR_H__

#include "scene/actor.h"
#include <memory>
#include <string>
#include <vmath.h>

namespace SJH
{
	class Framebuffer;
	class Mesh;
	class Material;
}

namespace SJH::Scene
{
```

기존 `CreateCameraActor` 선언 직후 (line 30 부근 — `std::unique_ptr<Actor> CreateCameraActor(...)` 닫는 `);` 다음) 에 두 선언 추가:

```cpp
	/// @brief PostFX 2-Camera 패턴의 Orthographic ScreenCamera Actor 생성.
	/// @details IsOrthographic=true + OrthoSize=1.0 + NearZ=-1 + NoClear=true
	///          + CullingMask(UI|Screen) + SetTargetRenderTarget(sceneFB).
	///          기존 main.cpp 의 CreateAndRegisterScreenCamera() 22줄 보일러 추출.
	/// @return Actor UPtr — caller 가 Director::Root().AddChild 책임 (D-2 Pure factory).
	std::unique_ptr<Actor> CreateScreenCameraActor(
	    std::string name,
	    float aspect,
	    Framebuffer* sceneFB);

	/// @brief Skybox Actor 생성 — Mesh + 큰 scale + MeshRenderer.
	/// @details 카메라 따라가기는 *셰이더 측* (vert shader 의 view matrix translation 제거)
	///          으로 자동 처리. SyncSkyboxToCamera 자유 함수 불필요.
	/// @return Actor UPtr — caller 가 dir.Root().AddChild 책임.
	std::unique_ptr<Actor> CreateSkyboxActor(
	    Mesh* skyboxMesh,
	    Material* skyboxMat,
	    float scale = 50.0f);
```

---

- [ ] **Step 2: compound_actor.cpp 에 본구현 추가**

Modify `src/scene/compound_actor.cpp` — 파일 끝 (마지막 `}` 닫는 namespace 직전) 에 추가.

먼저 include 추가 (파일 상단):
- `#include "scene/camera.h"` — Camera 의 IsOrthographic / OrthoSize 등 멤버 접근 (이미 있을 수 있음, 확인)
- `#include "scene/layer.h"` — Layer::UI / Layer::Screen
- `#include "object/mesh.h"` — Mesh 타입 완성
- `#include "material/material.h"` — Material 타입
- `#include "<buffer>/framebuffer.h"` — Framebuffer 타입 (SetTargetRenderTarget 인자)
- `#include "<scene>/components.h"` 또는 `src/render/mesh_renderer.h` — MeshRenderer Component (CreateSkyboxActor 가 부착)

본구현 — 기존 `CreateCameraActor` 본구현 *직후* 에 추가:

```cpp
	std::unique_ptr<Actor> CreateScreenCameraActor(
	    std::string name,
	    float aspect,
	    Framebuffer* sceneFB)
	{
		// 기존 main.cpp::CreateAndRegisterScreenCamera 22줄 이주.
		// CreateCameraActor 가 Actor + Transform + Camera Component 부착.
		auto screenCamActor = CreateCameraActor(std::move(name), 45.0f, aspect, -1.0f, 1.0f);
		auto* camera = screenCamActor->GetComponent<Camera>();
		camera->IsOrthographic = true;
		camera->OrthoSize      = 1.0f;
		camera->NoClear        = true; // WorldCamera 출력 보존 — clear 없이 합성
		camera
		    ->SetCullingMask(Layer::UI | Layer::Screen)
		    .SetTargetRenderTarget(sceneFB);
		return screenCamActor;
	}

	std::unique_ptr<Actor> CreateSkyboxActor(
	    Mesh* skyboxMesh,
	    Material* skyboxMat,
	    float scale)
	{
		auto skyboxActor = std::make_unique<Actor>("Skybox");
		// 스카이박스 모델이 카메라 클리핑 범위를 벗어나지 않게 넉넉한 크기로 스케일.
		skyboxActor->GetTransform().Scale = vmath::vec3(scale, scale, scale);
		skyboxActor->AddComponent<MeshRenderer>(skyboxMesh, skyboxMat);
		return skyboxActor;
	}
```

> 주의: `MeshRenderer` Component 의 정확한 namespace + AddComponent 시그니처 확인. 본 프로젝트는 `SJH::Scene::MeshRenderer` (forward decl 필요 시 `mesh_renderer.h` include). main.cpp:583 의 사용 패턴 (`skyboxActor->AddComponent<SJH::Scene::MeshRenderer>(skyboxMesh, mSkyboxMat);`) 그대로 따름.

---

- [ ] **Step 3: 빌드 확인**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected:
- 컴파일 성공 — `sjhopengl_scene.a` 에 두 factory symbol 합류
- 의존성 traversal 정상 — Mesh / Material / Framebuffer / Layer 헤더가 cpp 안에서 인식

Troubleshooting:
- `'object/mesh.h' file not found` → src/scene/CMakeLists.txt 가 이미 `PUBLIC SJH::object` link — 정상. cpp 내부 include 누락 시.
- `'material/material.h' file not found` → Material 헤더 경로 확인. 본 프로젝트는 `material/material.h`.
- `Camera::IsOrthographic` undefined → `scene/camera.h` include 누락.

---

- [ ] **Step 4: Commit**

Run:
```bash
git add src/scene/compound_actor.h src/scene/compound_actor.cpp
git commit -m "[feat] : Scene::CreateScreenCameraActor + CreateSkyboxActor

spec doc/superpowers/specs/2026-05-31-warmup-boilerplate-extraction-design.md T2 (A2 + A5).

- compound_actor.h — 두 factory 선언 추가 + Framebuffer/Mesh/Material forward decl
- compound_actor.cpp — 본구현
  * CreateScreenCameraActor: 기존 main.cpp::CreateAndRegisterScreenCamera 22줄 이주
    (IsOrthographic + OrthoSize + NoClear + CullingMask(UI|Screen) + SetTargetRenderTarget)
  * CreateSkyboxActor: 기존 main.cpp::WarmupSkybox 의 actor 생성 부분 6줄 + scale param
- D-2 Pure factory — ActorUPtr 반환, caller 가 Director.AddChild 책임
- D-4 — Scene::compound_actor PreBuilt Actor factory 컨벤션 일치

A5 의 SyncSkyboxToCamera 자유 함수는 추가 안 함 (셰이더 측 처리 채택).
T2 commit — 사용처 0 (T4 에서 사용). 시각 동작 변경 0."
```

---

## Task 3: A3+A4 — `Render::SetupDefaultPipeline` + `BuildPostFXChain` render_pipeline 신규

**Files:**
- Create: `<src>/render/render_pipeline.h`
- Create: `<src>/render/render_pipeline.cpp`
- Modify: `src/render/CMakeLists.txt` — `render_pipeline.cpp` source list 등록

---

- [ ] **Step 1: render_pipeline.h 작성**

Create `<src>/render/render_pipeline.h`:

```cpp
#ifndef __SJH_RENDER_PIPELINE_H__
#define __SJH_RENDER_PIPELINE_H__

#include "<buffer>/framebuffer.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace SJH
{
	class ResourceRegistry;
	class SceneRenderer;
	class ScreenQuadStage;
}
namespace SJH::Scene
{
	class Actor;
	class PassComponent;
}

namespace SJH::Render
{
	/// @brief 기본 RenderPipeline 셋업의 식별자/경로 묶음.
	struct DefaultPipelineConfig
	{
		std::string PassthroughKey    = "screen_passthrough";
		std::string PassthroughVS     = "./resources/shaders/passthrough.vs";
		std::string PassthroughFS     = "./resources/shaders/passthrough.fs";
		std::string ScreenQuadMeshKey = "mesh_screen_quad";
		std::string BypassMatKey      = "mat_bypass_passthrough";
	};

	/// @brief PostFX 사용 데모의 표준 setup.
	/// @details passthrough Program 등록 + ScreenQuad Mesh 등록 + bypassMaterial 등록
	///          + SceneRenderer 에 SetScreenQuadMesh/SetBypassMaterial 주입 + ScreenQuadStage 생성.
	/// @return ScreenQuadStage UPtr — caller 가 mStages.push_back 책임 (D-2 Pure factory).
	std::unique_ptr<ScreenQuadStage> SetupDefaultPipeline(
	    ResourceRegistry& reg,
	    SceneRenderer& sceneRenderer,
	    Framebuffer* sceneFB,
	    const DefaultPipelineConfig& cfg = {});

	/// @brief PostFX 한 단계의 셰이더 + 초기 uniform 값.
	struct PostFXStageConfig
	{
		std::string Name;
		std::string VertFile;
		std::string FragFile;
		std::unordered_map<std::string, float> InitFloats; // D-6 data-driven (gamma=1.0 등)
	};

	/// @brief PostFX 체인 빌드 결과.
	struct PostFXChainResult
	{
		std::vector<FramebufferUPtr>          Framebuffers;   // owner — caller 가 멤버 vector 로 보유
		std::vector<Scene::PassComponent*>    PassComponents; // 비소유 raw — Debug UI 참조용
	};

	/// @brief PostFX 체인 빌드 — 각 stage 의 Program/Material/FB 생성
	///        + PassActor(Layer::Screen) + AddComponent<PassComponent>(prev, fb, mat)
	///        + screenCamActor->AddChild.
	/// @details 첫 stage 의 InputFB = sceneFB, 이후 stages 는 prev 단계의 OutputFB.
	///          configs 의 각 element 의 InitFloats 가 Material::Properties::Floats 에 복사.
	PostFXChainResult BuildPostFXChain(
	    ResourceRegistry& reg,
	    Scene::Actor& screenCamActor,
	    const std::vector<PostFXStageConfig>& configs,
	    Framebuffer* sceneFB,
	    int fbWidth, int fbHeight);
}

#endif // __SJH_RENDER_PIPELINE_H__
```

---

- [ ] **Step 2: render_pipeline.cpp 작성**

Create `<src>/render/render_pipeline.cpp`:

```cpp
#include "<render>/render_pipeline.h"

#include "object/mesh.h"
#include "material/material.h"
#include "program/program.h"
#include "<render>/pass_component.h"
#include "<render>/scene_renderer.h"
#include "<render>/screen_quad_stage.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/layer.h"

#include <<spdlog>/spdlog.h>

namespace SJH::Render
{
	std::unique_ptr<ScreenQuadStage> SetupDefaultPipeline(
	    ResourceRegistry& reg,
	    SceneRenderer& sceneRenderer,
	    Framebuffer* sceneFB,
	    const DefaultPipelineConfig& cfg)
	{
		// passthrough Program 등록.
		auto* passthroughProg = reg.CreateProgram(
		    cfg.PassthroughKey,
		    cfg.PassthroughVS,
		    cfg.PassthroughFS);

		// ScreenQuad Mesh 등록.
		auto* quadMesh = reg.RegisterMesh(cfg.ScreenQuadMeshKey, Mesh::CreateScreenQuad());

		// ScreenQuadStage 생성 + 초기 sources = sceneFB.
		auto screenQuadStage = std::make_unique<ScreenQuadStage>(*passthroughProg, *quadMesh);
		screenQuadStage->SetSources({sceneFB}); // 초기 sources fallback

		// bypassMaterial 등록 + SceneRenderer 주입.
		auto* bypassMat = reg.CreateSharedMaterial(cfg.BypassMatKey);
		bypassMat->SetProgram(passthroughProg);

		sceneRenderer.SetScreenQuadMesh(quadMesh);
		sceneRenderer.SetBypassMaterial(bypassMat);

		return screenQuadStage;
	}

	PostFXChainResult BuildPostFXChain(
	    ResourceRegistry& reg,
	    Scene::Actor& screenCamActor,
	    const std::vector<PostFXStageConfig>& configs,
	    Framebuffer* sceneFB,
	    int fbWidth, int fbHeight)
	{
		PostFXChainResult result;
		result.Framebuffers.reserve(configs.size());
		result.PassComponents.reserve(configs.size());

		Framebuffer* prevFB = sceneFB;
		for (const auto& def : configs)
		{
			const auto progKey = std::string("postfx_") + def.Name;
			const auto matKey  = std::string("mat_pass_") + def.Name;

			auto* prog = reg.CreateProgram(def.Name, def.VertFile, def.FragFile);
			if (!prog)
			{
				spdlog::error("[PostFXChain] 셰이더 로드 실패: {}", def.FragFile);
				continue;
			}

			auto* mat = reg.CreateSharedMaterial(matKey);
			mat->SetProgram(prog);

			// D-6 data-driven — InitFloats 가 mat->Properties.Floats 로 복사.
			for (const auto& [name, value] : def.InitFloats)
			{
				mat->Properties.Floats[name] = value;
			}

			auto fb = Framebuffer::Create(fbWidth, fbHeight);
			if (!fb)
			{
				spdlog::error("[PostFXChain] FB 생성 실패: {}", def.Name);
				continue;
			}
			auto* fbPtr = fb.get();

			auto passActor = std::make_unique<Scene::Actor>(std::string("PassActor_") + def.Name);
			passActor->SetLayer(Scene::Layer::Screen);
			auto* pc = passActor->AddComponent<Scene::PassComponent>(prevFB, fbPtr, mat);

			result.PassComponents.push_back(pc);
			result.Framebuffers.push_back(std::move(fb));
			prevFB = fbPtr;

			screenCamActor.AddChild(std::move(passActor));
		}

		return result;
	}
}
```

---

- [ ] **Step 3: CMakeLists.txt 에 render_pipeline.cpp 등록**

Modify `src/render/CMakeLists.txt` — `add_library` 의 source list 마지막 (`camera_stage.cpp` 다음) 에 추가:

기존:
```cmake
add_library(sjhopengl_render STATIC
    # ... 기존 entries ...
    camera_stage.cpp              # SP-RenderStage 완성 — 단일 Camera 를 IRenderStage 로 wrap
)
```

→ 변경 후:
```cmake
add_library(sjhopengl_render STATIC
    # ... 기존 entries ...
    camera_stage.cpp              # SP-RenderStage 완성 — 단일 Camera 를 IRenderStage 로 wrap
    render_pipeline.cpp           # SP-Warmup — Setup + BuildPostFXChain 자유 함수 family
)
```

`target_link_libraries` 변경 없음 — render_pipeline.cpp 가 사용하는 SJH::material / SJH::object / SJH::resource_registry / SJH::scene 모두 *이미 PRIVATE link* 됨. spdlog 도 transitive 합류.

---

- [ ] **Step 4: 빌드 확인**

Run:
```bash
cmake --preset ninja
cmake --build --preset ninja --target _MyApp_
```

Expected:
- cmake configure 인식
- 컴파일 성공 — `sjhopengl_render.a` 에 두 symbol 합류
- link 통과

Troubleshooting:
- `'resource_registry/resource_registry.h' file not found` → CMakeLists.txt 에 `PRIVATE SJH::resource_registry` 이미 있음 — cpp 의 include 라인 typo 확인.
- `'material/material.h' file not found` → 동일 — `PRIVATE SJH::material` 확인.
- `Mesh::CreateScreenQuad()` undefined → `object/mesh.h` 에 정의 확인.

---

- [ ] **Step 5: Commit**

Run:
```bash
git add <src>/render/render_pipeline.h <src>/render/render_pipeline.cpp src/render/CMakeLists.txt
git commit -m "[feat] : SJH::Render::SetupDefaultPipeline + BuildPostFXChain

spec doc/superpowers/specs/2026-05-31-warmup-boilerplate-extraction-design.md T3 (A3 + A4).

- render_pipeline.h — 4 declarations: DefaultPipelineConfig struct + SetupDefaultPipeline
  + PostFXStageConfig struct + PostFXChainResult struct + BuildPostFXChain
- render_pipeline.cpp — 본구현
  * SetupDefaultPipeline: 기존 main.cpp::WramupSceneRenderer 33줄 이주
    (passthrough Program + ScreenQuad Mesh + bypass Material + SceneRenderer 주입)
  * BuildPostFXChain: 기존 main.cpp::WarmupPassRenderer 47줄 이주
    (각 stage 의 Program/Material/FB + PassActor(Layer::Screen) + PassComponent
     + screenCamActor->AddChild)
- D-6 data-driven — gamma 특수 케이스 제거, PostFXStageConfig.InitFloats map 으로 일반화
- D-2 Pure factory — ScreenQuadStage UPtr / PostFXChainResult struct 반환, caller 가 wiring
- D-3 — 자유 함수 family, 별도 STATIC 모듈 분리 안 함

T3 commit — 사용처 0 (T4 에서 사용). 시각 동작 변경 0."
```

---

## Task 4: main.cpp 통합 — 자유 함수 호출 + Warmup 메서드 제거

**Files:**
- Modify: `apps/_MyApp_/main.cpp` ONLY

이번 task 가 *시각 회귀 검증의 유일한 task*. 본 task 의 commit 으로 main.cpp 가 ~130줄 감소.

---

- [ ] **Step 1: include 3개 추가 + POSTFX_PROGRAM_CONFIGS 타입 변경**

Modify `apps/_MyApp_/main.cpp`:

**1-a. include 추가** — 기존 include 블록 (line 34~52 부근) 에 3 줄 추가:

기존 (대략):
```cpp
#include "<buffer>/framebuffer.h"
#include "common/common.h"
#include "material/pass.h"
#include "object/mesh.h"
#include "program/program.h"
#include "<render>/camera_stage.h"
#include "<render>/pass_component.h"
```

→ 변경 후 (`common/window_helper.h`, `<render>/render_pipeline.h`, `scene/compound_actor.h` 추가 — compound_actor 는 이미 있을 수 있음):
```cpp
#include "<buffer>/framebuffer.h"
#include "common/common.h"
#include "common/window_helper.h"        // A1 NEW
#include "material/pass.h"
#include "object/mesh.h"
#include "program/program.h"
#include "<render>/camera_stage.h"
#include "<render>/pass_component.h"
#include "<render>/render_pipeline.h"      // A3+A4 NEW
```

**1-b. POSTFX_PROGRAM_CONFIGS 타입 변경** — anonymous namespace 의 `ProgramConfig` 그대로 두고, 두 번째 `POSTFX_PROGRAM_CONFIGS` 변수를 `SJH::Render::PostFXStageConfig` 로 변경:

기존:
```cpp
struct ProgramConfig
{
    const char *Name;
    const char *VertFile;
    const char *FragFile;
};
const ProgramConfig PASSTHOURH_PROGRAM_CONFIG = {
    "screen_passthrough",
    "./resources/shaders/passthrough.vs",
    "./resources/shaders/passthrough.fs"};

const std::vector<ProgramConfig> POSTFX_PROGRAM_CONFIGS = {
    {"blurring", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/blurring.fs"},
    {"gamma", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/gamma.fs"},
    {"invert", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/invert.fs"},
    {"sharpening", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/sharpening.fs"},
    {"sobel", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/sobel.fs"},
};
```

→ 변경 후:
```cpp
struct ProgramConfig
{
    const char *Name;
    const char *VertFile;
    const char *FragFile;
};
const ProgramConfig PASSTHOURH_PROGRAM_CONFIG = {
    "screen_passthrough",
    "./resources/shaders/passthrough.vs",
    "./resources/shaders/passthrough.fs"};

// D-6 data-driven — gamma 초기값을 InitFloats 로 명시.
const std::vector<SJH::Render::PostFXStageConfig> POSTFX_PROGRAM_CONFIGS = {
    {"blurring",   "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/blurring.fs",   {}},
    {"gamma",      "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/gamma.fs",      {{"gamma", 1.0f}}},
    {"invert",     "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/invert.fs",     {}},
    {"sharpening", "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/sharpening.fs", {}},
    {"sobel",      "./resources/shaders/postprocess/postprocess.vs", "./resources/shaders/postprocess/sobel.fs",      {}},
};
```

---

- [ ] **Step 2: startup() 의 Warmup 호출 교체**

Modify `apps/_MyApp_/main.cpp` startup() — 기존 line 105~122 영역 (fbW/fbH 계산 + manager.Init() + 4 Warmup 호출) 을 다음으로 교체:

기존:
```cpp
void startup() override
{

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    const float aspect = static_cast<float>(fbW) / static_cast<float>(fbH);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    auto &manager = TopdownShooter::Manager::Get();
    manager.Init();
    auto &reg = SJH::ResourceRegistry::Get();
    auto &dir = SJH::Scene::Director::Get();
    auto &phys = TopdownShooter::Manager::Get().Physics();
    auto &vfxs = TopdownShooter::Manager::Get().VFX();

    mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(fbW, fbH);
    WramupSceneRenderer(reg, Manager::Get().SceneRenderer(), PASSTHOURH_PROGRAM_CONFIG);
    mCamera = CreateAndRegisterWorldCamera();
    mScreenCamera = CreateAndRegisterScreenCamera();
    WarmupPassRenderer(reg, POSTFX_PROGRAM_CONFIGS);
```

→ 변경 후:
```cpp
void startup() override
{
    // A1 — GLFW window 정보 1 호출.
    const auto fb = SJH::GetFramebufferInfo(window);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    auto &manager = TopdownShooter::Manager::Get();
    manager.Init();
    auto &reg = SJH::ResourceRegistry::Get();
    auto &dir = SJH::Scene::Director::Get();
    auto &phys = TopdownShooter::Manager::Get().Physics();
    auto &vfxs = TopdownShooter::Manager::Get().VFX();

    mDefaultTarget = std::make_unique<SJH::DefaultRenderTarget>(fb.Width, fb.Height);
    mSceneFB       = SJH::Framebuffer::Create(fb.Width, fb.Height);

    // A3 — RenderPipeline 셋업 + ScreenQuadStage 받아 mStages push.
    SJH::Render::DefaultPipelineConfig pipelineCfg{
        PASSTHOURH_PROGRAM_CONFIG.Name,
        PASSTHOURH_PROGRAM_CONFIG.VertFile,
        PASSTHOURH_PROGRAM_CONFIG.FragFile,
        // ScreenQuadMeshKey / BypassMatKey 는 default
    };
    auto sqStage = SJH::Render::SetupDefaultPipeline(reg, manager.SceneRenderer(), mSceneFB.get(), pipelineCfg);
    mScreenQuadStagePtr = sqStage.get();
    mStages.push_back(std::move(sqStage));

    // World Camera 는 Client 한정 (TargetFollowableCameraController + 초기 transform) — 그대로 유지.
    mCamera = CreateAndRegisterWorldCamera();

    // A2 — ScreenCamera Pure factory.
    auto screenCamActor = SJH::Scene::CreateScreenCameraActor("ScreenCamera", fb.Aspect, mSceneFB.get());
    mScreenCamera = screenCamActor->GetComponent<SJH::Scene::Camera>();
    auto* screenCamActorPtr = dir.Root().AddChild(std::move(screenCamActor));

    // A4 — PostFX 체인 빌드.
    auto chain = SJH::Render::BuildPostFXChain(reg, *screenCamActorPtr, POSTFX_PROGRAM_CONFIGS, mSceneFB.get(), fb.Width, fb.Height);
    mPostFXFBs      = std::move(chain.Framebuffers);
    mPassComponents = std::move(chain.PassComponents);
```

> 주의: 기존 `mSceneFB` 멤버가 `WramupSceneRenderer` 안에서 생성됐었음 — 이제 startup() 의 *명시 단계로* 이동. 다른 곳에서 `mSceneFB` 가 변경되지 않는지 grep 확인.

---

- [ ] **Step 3: Warmup 메서드 3개 완전 제거 + WarmupSkybox 축소**

Modify `apps/_MyApp_/main.cpp` — private 메서드 영역 (line 363~616 부근):

**3-a. 완전 제거 (3 메서드)**:
- `SJH::Scene::Camera *CreateAndRegisterScreenCamera()` (~22줄) — A2 가 대체
- `void WramupSceneRenderer(...)` (~33줄) — A3 가 대체
- `void WarmupPassRenderer(...)` (~47줄) — A4 가 대체

**3-b. WarmupSkybox 축소** — 기존 ~23줄 → ~10줄. CreateSkyboxActor 호출로 교체.

기존:
```cpp
void WarmupSkybox(SJH::ResourceRegistry &reg, SJH::Scene::Director &dir)
{
    auto *skyboxProg = reg.CreateProgram(
        "matrix_skybox",
        "resources/shaders/matrix_skybox.vs",
        "resources/shaders/matrix_skybox.fs");

    auto *charsTex = reg.CreateTexture("chars", SJH::Image::Load("chars", "resources/texture/characters.png").get());
    auto *noiseTex = reg.CreateTexture("noise_tex", SJH::Image::Load("noise_tex", "resources/texture/matrix_noise.png").get());

    mSkyboxMat = reg.CreateSharedMaterial("mat_matrix_skybox");
    mSkyboxMat->SetProgram(skyboxProg);
    mSkyboxMat->Properties.Textures["chars"] = { charsTex };
    mSkyboxMat->Properties.Textures["noise_tex"] = { noiseTex };
    mSkyboxMat->Properties.Floats["u_time"] = 0.0f;

    auto *skyboxMesh = reg.RegisterMesh("mesh_skybox", SJH::Mesh::CreateBox());
    auto skyboxActor = std::make_unique<SJH::Scene::Actor>("MatrixSkybox");
    skyboxActor->GetTransform().Scale = vmath::vec3(50.0f, 50.0f, 50.0f);
    skyboxActor->AddComponent<SJH::Scene::MeshRenderer>(skyboxMesh, mSkyboxMat);
    mSkyboxActor = dir.Root().AddChild(std::move(skyboxActor));
}
```

→ 변경 후 (matrix_skybox shader/texture 는 *데모 한정* 보존, actor 생성만 factory 위임):
```cpp
void WarmupSkybox(SJH::ResourceRegistry &reg, SJH::Scene::Director &dir)
{
    // matrix_skybox 셰이더/텍스처 — 이 데모 한정 effect (보일러 아님).
    auto *skyboxProg = reg.CreateProgram(
        "matrix_skybox",
        "resources/shaders/matrix_skybox.vs",
        "resources/shaders/matrix_skybox.fs");
    auto *charsTex = reg.CreateTexture("chars",     SJH::Image::Load("chars",     "resources/texture/characters.png").get());
    auto *noiseTex = reg.CreateTexture("noise_tex", SJH::Image::Load("noise_tex", "resources/texture/matrix_noise.png").get());

    mSkyboxMat = reg.CreateSharedMaterial("mat_matrix_skybox");
    mSkyboxMat->SetProgram(skyboxProg);
    mSkyboxMat->Properties.Textures["chars"]     = { charsTex };
    mSkyboxMat->Properties.Textures["noise_tex"] = { noiseTex };
    mSkyboxMat->Properties.Floats["u_time"] = 0.0f;

    // A5 — actor 생성은 Pure factory.
    auto *skyboxMesh = reg.RegisterMesh("mesh_skybox", SJH::Mesh::CreateBox());
    mSkyboxActor = dir.Root().AddChild(SJH::Scene::CreateSkyboxActor(skyboxMesh, mSkyboxMat, 50.0f));
}
```

---

- [ ] **Step 4: 멤버 타입 변경 — `mPostFXFBs` array<5> → vector**

Modify `apps/_MyApp_/main.cpp` — 멤버 선언 (line 342 부근):

기존:
```cpp
std::array<SJH::FramebufferUPtr, 5> mPostFXFBs;
```

→ 변경 후:
```cpp
std::vector<SJH::FramebufferUPtr> mPostFXFBs;  // configs 가변 size 대응 (D-6)
```

> 같은 파일의 다른 곳에서 `mPostFXFBs` 가 *indexed 접근* (`mPostFXFBs[i]`) 되는지 grep 으로 확인. 만약 있다면 vector 도 동일 indexed 접근 지원 — 변경 불필요.

---

- [ ] **Step 5: 빌드 확인**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected:
- 컴파일 성공 — `_MyApp_` 실행 파일 생성
- main.cpp 가 ~130줄 감소 (3 메서드 제거 + WarmupSkybox 축소)

Troubleshooting:
- `'common/window_helper.h' file not found` → Step 1-a include 누락
- `'<render>/render_pipeline.h' file not found` → 동일
- `SJH::Render::PostFXStageConfig` undefined → Step 1-b 의 타입 변경 누락 또는 render_pipeline.h include 위치
- `mScreenQuadStagePtr` undefined / 시그니처 mismatch → mSceneFB / mStages 의 멤버 변수가 모두 valid 한지 확인
- `mPostFXFBs[i] = ...` 컴파일 에러 → vector 도 indexed 접근 OK. 단 *size 명시 init* (예: `mPostFXFBs.resize(5)`) 가 빠지면 indexed write 실패. 새 코드에서는 `chain.Framebuffers` 의 `std::move` 로 통째로 대입 → 문제 없음.

---

- [ ] **Step 6: 실행 + 시각 회귀 검증 (V1~V11)**

Run:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

검증 체크리스트 (사용자 GUI 확인):

| # | 시나리오 | 기대 |
|---|---|---|
| V1 | 실행 시 spdlog warning 0 | 정상 |
| V2 | 마우스 좌클릭 → distortion 파티클 발사 | 변경 전과 동일 표시 |
| V3 | gamma 슬라이더 0.5 | 화면 + 파티클 어두워짐 |
| V4 | sobel 토글 ON | 파티클까지 윤곽선 검출 |
| V5 | invert 토글 ON | 파티클 색까지 반전 |
| V6 | blurring 토글 ON | 블러 처리 |
| V7 | sharpening 토글 ON | 샤프닝 |
| V8 | 전체 PostFX OFF | 기존과 시각적으로 동일 |
| V9 | ImGui PostFXDebug 패널 (좌측 20,140) | 5 행 + gamma 슬라이더 동작 |
| V10 | WASD 이동 시 Skybox 따라옴 | 셰이더 측 처리 — 자동 |
| V11 | 창 리사이즈 (드래그) | mSceneFB / DefaultRenderTarget / Camera.Aspect 재생성 (기존 동작 보존) |

모두 통과 시 Step 7 진행. 실패 시:
- V1 spdlog warning → 자유 함수의 nullptr 가드 확인
- V2~V8 PostFX 미작동 → POSTFX_PROGRAM_CONFIGS 의 InitFloats 확인 (gamma 초기값)
- V9 패널 미표시 → mPassComponents 가 비어있는지 확인 (BuildPostFXChain 의 prog/fb nullptr 시 continue)

---

- [ ] **Step 7: Commit**

Run:
```bash
git add apps/_MyApp_/main.cpp
git commit -m "[refactor] : main.cpp Warmup 보일러플레이트 추출 통합

spec doc/superpowers/specs/2026-05-31-warmup-boilerplate-extraction-design.md T4.

[main.cpp 변경]
- include 추가 — common/window_helper.h, <render>/render_pipeline.h
- POSTFX_PROGRAM_CONFIGS 타입 ProgramConfig → SJH::Render::PostFXStageConfig
  (D-6 data-driven — gamma 초기값을 {{\"gamma\", 1.0f}} 로 명시)
- startup() 의 Warmup 호출 5개를 자유 함수 호출 4개로 교체:
  * GetFramebufferInfo (A1)
  * SetupDefaultPipeline (A3)
  * CreateScreenCameraActor (A2)
  * BuildPostFXChain (A4)
- private 메서드 3개 완전 제거:
  * CreateAndRegisterScreenCamera (~22줄) → A2 대체
  * WramupSceneRenderer (~33줄) → A3 대체
  * WarmupPassRenderer (~47줄) → A4 대체
- WarmupSkybox ~23줄 → ~10줄 (matrix_skybox 셰이더/텍스처는 데모 한정 보존,
  actor 생성만 CreateSkyboxActor 위임 — A5)
- 멤버 mPostFXFBs: array<5> → vector (configs 가변 size 대응)
- mSceneFB 생성을 startup() 명시 단계로 이동 (기존엔 WramupSceneRenderer 안)

[시각 회귀 V1~V11 통과 — 사용자 검증]
- distortion 파티클 + PostFX 토글 5종 + gamma 슬라이더
- ImGui PostFXDebug 패널 (좌측 20,140) + 5 행 토글
- Skybox 따라가기 (셰이더 측 자동)
- 창 리사이즈 (재생성 동작 보존)

main.cpp 순감소: ~130줄."
```

---

## Out of Scope (본 plan 범위 외)

| 항목 | 이유 |
|---|---|
| **WramupFMOD / WramupPlayer / WarmupImgui 본구현** | Client 도메인 (게임 asset/UI) — 별도 SP 후보 |
| **CreateAndRegisterWorldCamera** | TargetFollowableCameraController 게임 카메라 의도 — 별도 SP 후보 |
| **B 그룹 (ImGui bootstrap)** | UI helper 분리 — 별도 SP |
| **migrate_demo / audio_demo** | 본 패턴 사용 안 함 — 영향 0 |
| **mScreenQuadStagePtr = nullptr 초기화** | 기존 코드 — ParticleStage SP 의 잔여 Important |
| **단위 테스트 추가** | `no_auto_tests` 정책 |

---

## Self-Review

**Spec coverage**:
- ✅ D-1 (옵션 1) → T1~T3 모두 *기존 모듈 확장 + 새 헤더 1개*
- ✅ D-2 (Pure factory) → T2 의 CreateScreenCameraActor/CreateSkyboxActor 반환 ActorUPtr, T3 의 SetupDefaultPipeline 반환 ScreenQuadStage UPtr, BuildPostFXChain 반환 PostFXChainResult struct
- ✅ D-3 (render_pipeline 새 헤더) → T3 의 `src/render/render_pipeline.{h,cpp}`
- ✅ D-4 (compound_actor 확장) → T2
- ✅ D-5 (window_helper + GLFW 격리) → T1 의 헤더 forward decl + cpp 만 include
- ✅ D-6 (data-driven gamma) → T3 의 InitFloats map 처리 + T4 의 PostFXStageConfig {{"gamma", 1.0f}} 적용
- ✅ spec §4.1 파일별 변경표 — T1~T4 의 모든 파일 커버
- ✅ spec §7.1 V1~V11 시각 회귀 → T4 Step 6
- ✅ spec §6 Out of Scope → 본 plan Out of Scope 일치

**Placeholder scan**: TBD/TODO 없음. 모든 step 에 exact code + exact command + expected output.

**Type consistency**:
- `FramebufferInfo` (T1) ↔ main.cpp 의 `fb.Width / fb.Height / fb.Aspect` (T4) 일치
- `DefaultPipelineConfig` / `PostFXStageConfig` / `PostFXChainResult` (T3) ↔ T4 의 사용 시그니처 일치
- `std::unique_ptr<Actor>` (T2 의 CreateScreenCameraActor/CreateSkyboxActor 반환) ↔ T4 의 `auto screenCamActor = ...` 사용 패턴 일치
- `mPostFXFBs` 타입 변경 (array<5> → vector) ↔ T4 의 `std::move(chain.Framebuffers)` 대입 일치

**모든 일치 확인됨**.

---

## 후속 (plan 완료 후)

- 본 SP 마무리 → `finishing-a-development-branch` skill
- 후속 SP 후보: B 그룹 (ImGui bootstrap), CreateWorldCameraActor 분리, Stage::Populate 확장
- main.cpp 의 *나머지 Client 도메인* 추출 시 spec §9 future work 참조

---

**plan 끝**.
