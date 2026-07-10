# ImGui Layer Separation (Game GUI / Editor UI) Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `game_application` 의 ImGui 렌더 코드를 `IImGuiLayer` 추상 + `ImGuiLayerStack` 으로 분리하여, Game GUI(항상 표시)와 Editor UI(F1 토글)를 독립적으로 관리할 수 있게 한다.

**Architecture:** `IImGuiLayer` 순수 가상 인터페이스(Kind: Game/Editor) + `ImGuiLayerStack` 헤더-온리 관리자. 구체 레이어 2종(`ExitButtonLayer`, `PostFXDebugLayer`)을 생성자 주입으로 의존성 수령. 레이어들은 `apps/_MyApp_/src/UI/` 에 거주 — ImGui는 SJH::engine 모듈이 아니라 client-side compile이므로 엔진 밖 Client 거주가 정통.

**Tech Stack:** C++17, ImGui v1.53 (extern/imgui, GLFW 3.0.4 호환 마지막 태그), GLFW, SJH::render (SceneRenderer/PostFXPass/Mesh), CMake STATIC-free 패턴 (UI .cpp를 executable에 직접 추가)

---

## 전제 지식 (반드시 읽을 것)

### ImGui 스타일 스택 규칙 (v1.53)
- `Begin()` 호출 시점의 스타일 스택 크기를 내부 저장 → `End()` 시 `CheckStacksSize` 검증
- **창 배경 투명화 패턴**: Push 3종 → Begin → (콘텐츠) → End → PopStyleColor(3)
  - Push와 Pop을 Begin/End 사이에 두면 → `PushStyleColor/PopStyleColor Mismatch` Assertion 발생
- v1.53 미지원: `SetNextWindowBgAlpha`, `ImGuiWindowFlags_NoBackground`, `ImGuiWindowFlags_NoNav`, `GLFW_TRUE`

### include 경로 해석
- `apps/_MyApp_/src/` 는 `MyApp::Director` PUBLIC includes 에 의해 이미 include path 에 있음
- `#include "apps/_MyApp_/src/UI/IImGuiLayer.h"` → `apps/_MyApp_/src/UI/IImGuiLayer.h` ✓ (경로 추가 불필요)
- `<imgui.h>` 는 `_MyApp_` executable PRIVATE target_include_directories에만 있음 → UI .cpp 파일도 반드시 executable에 직접 추가해야 imgui.h를 찾을 수 있음

### 테스트 정책
이 프로젝트는 "테스트는 사용자 요청 시만 작성" 컨벤션 (memory: no_auto_tests). TDD 단계는 생략하고 빌드 검증으로 대체.

---

## 파일 맵

| 파일 | 역할 | 신규/수정 |
|------|------|---------|
| `apps/_MyApp_/src/UI/IImGuiLayer.h` | Kind enum + 순수 가상 인터페이스 | 신규 |
| `apps/_MyApp_/src/UI/ImGuiLayerStack.h` | Push/RenderAll/Clear — 헤더 온리 | 신규 |
| `<apps>/_MyApp_/src/UI/ExitButtonLayer.h` | Game kind — 종료 버튼 — 헤더 온리 | 신규 |
| `<apps>/_MyApp_/src/UI/PostFXDebugLayer.h` | Editor kind — PostFX 토글 — 선언 | 신규 |
| `<apps>/_MyApp_/src/UI/PostFXDebugLayer.cpp` | PostFXDebugLayer 구현 | 신규 |
| `apps/_MyApp_/CMakeLists.txt` | UI_SRC 변수 + add_executable 소스 추가 | 수정 |
| `apps/_MyApp_/main.cpp` | 레이어 Push + RenderAll 교체 + F1 토글 | 수정 |

---

## Task 1: IImGuiLayer.h — 순수 가상 인터페이스

**Files:**
- Create: `apps/_MyApp_/src/UI/IImGuiLayer.h`

- [ ] **Step 1: 파일 생성**

```cpp
#ifndef __MYAPP_IMGUI_LAYER_H__
#define __MYAPP_IMGUI_LAYER_H__

namespace TopdownShooter::UI
{
	enum class ImGuiLayerKind
	{
		Game,   ///< 항상 렌더 (HUD, 버튼 등)
		Editor, ///< mShowEditor == true 일 때만 렌더 (디버그 창)
	};

	class IImGuiLayer
	{
	  public:
		virtual ~IImGuiLayer()                         = default;
		virtual void           OnBuildUI()             = 0;
		virtual ImGuiLayerKind GetKind() const         = 0;
		bool                   Enabled                 = true;
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_IMGUI_LAYER_H__
```

---

## Task 2: ImGuiLayerStack.h — 헤더 온리 스택 관리자

**Files:**
- Create: `apps/_MyApp_/src/UI/ImGuiLayerStack.h`

- [ ] **Step 1: 파일 생성**

```cpp
#ifndef __MYAPP_IMGUI_LAYER_STACK_H__
#define __MYAPP_IMGUI_LAYER_STACK_H__

#include "apps/_MyApp_/src/UI/IImGuiLayer.h"
#include <memory>
#include <vector>

namespace TopdownShooter::UI
{
	class ImGuiLayerStack
	{
	  public:
		void Push(std::unique_ptr<IImGuiLayer> layer)
		{
			mLayers.push_back(std::move(layer));
		}

		/// @brief 전체 레이어 렌더.
		/// @param showEditor false 이면 Editor kind 레이어는 건너뜀.
		void RenderAll(bool showEditor)
		{
			for (auto &layer : mLayers)
			{
				if (!layer->Enabled)
					continue;
				if (layer->GetKind() == ImGuiLayerKind::Editor && !showEditor)
					continue;
				layer->OnBuildUI();
			}
		}

		void Clear() { mLayers.clear(); }

	  private:
		std::vector<std::unique_ptr<IImGuiLayer>> mLayers;
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_IMGUI_LAYER_STACK_H__
```

---

## Task 3: ExitButtonLayer.h — Game kind 종료 버튼

**Files:**
- Create: `<apps>/_MyApp_/src/UI/ExitButtonLayer.h`

**주의:** `imgui.h` 포함 — `_MyApp_` executable의 include path에서만 찾을 수 있음. 이 헤더는 `main.cpp` 또는 `_MyApp_` executable 소스에서만 include 해야 함.

- [ ] **Step 1: 파일 생성**

```cpp
#ifndef __MYAPP_EXIT_BUTTON_LAYER_H__
#define __MYAPP_EXIT_BUTTON_LAYER_H__

#include "apps/_MyApp_/src/UI/IImGuiLayer.h"
#include <GLFW/glfw3.h>
#include <imgui.h>

namespace SJH
{
class Texture;
}

namespace TopdownShooter::UI
{
	class ExitButtonLayer : public IImGuiLayer
	{
	  public:
		ExitButtonLayer(GLFWwindow *win, const SJH::Texture *tex)
		    : mWindow(win), mTex(tex)
		{
		}

		ImGuiLayerKind GetKind() const override { return ImGuiLayerKind::Game; }

		void OnBuildUI() override
		{
			ImGui::SetNextWindowPos(ImVec2(64.0f, 64.0f), ImGuiCond_Always);
			// v1.53: SetNextWindowBgAlpha / NoBackground 미지원 — PushStyleColor 투명화.
			// 3종 Push 는 Begin() 이전, Pop 은 End() 이후 (CheckStacksSize 규칙).
			ImGui::PushStyleColor(ImGuiCol_WindowBg,     ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_Border,       ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));
			ImGui::Begin("##exit_btn", nullptr,
			             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize |
			                 ImGuiWindowFlags_NoMove);

			if (mTex)
			{
				ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.25f));
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

				if (ImGui::ImageButton(
				        (ImTextureID)(intptr_t)mTex->GetTextureID(),
				        ImVec2(48.0f, 48.0f)))
					glfwSetWindowShouldClose(mWindow, 1);

				ImGui::PopStyleVar();
				ImGui::PopStyleColor(3);
			}
			ImGui::End();
			ImGui::PopStyleColor(3); // WindowBg / Border / BorderShadow
		}

	  private:
		GLFWwindow         *mWindow;
		const SJH::Texture *mTex;
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_EXIT_BUTTON_LAYER_H__
```

---

## Task 4: PostFXDebugLayer.h/.cpp — Editor kind PostFX 디버그

**Files:**
- Create: `<apps>/_MyApp_/src/UI/PostFXDebugLayer.h`
- Create: `<apps>/_MyApp_/src/UI/PostFXDebugLayer.cpp`

**설계 노트:** `SJH::Mesh*&` (pointer-to-reference) 사용 이유 — `mCachedQuadMesh` 는 `BuildPostFXChain` 이후 설정되는 포인터. 참조로 바인딩하면 레이어가 항상 최신 포인터 값을 본다.

- [ ] **Step 1: PostFXDebugLayer.h 생성**

```cpp
#ifndef __MYAPP_POSTFX_DEBUG_LAYER_H__
#define __MYAPP_POSTFX_DEBUG_LAYER_H__

#include "apps/_MyApp_/src/UI/IImGuiLayer.h"
#include "<render>/postfx_pass.h"
#include <vector>

namespace SJH
{
class SceneRenderer;
class Mesh;
} // namespace SJH

namespace TopdownShooter::UI
{
	class PostFXDebugLayer : public IImGuiLayer
	{
	  public:
		/// @param renderer  SetPostFXChain 재전달 대상
		/// @param passes    Enabled 토글 대상 (game_application 소유 vector)
		/// @param quadMesh  ref-to-ptr — BuildPostFXChain 이후 설정되므로 참조로 바인딩
		/// @param gamma     gamma.fs uniform 실시간 연동
		PostFXDebugLayer(SJH::SceneRenderer          &renderer,
		                 std::vector<SJH::PostFXPass> &passes,
		                 SJH::Mesh                   *&quadMesh,
		                 float                        &gamma);

		ImGuiLayerKind GetKind() const override { return ImGuiLayerKind::Editor; }
		void           OnBuildUI() override;

	  private:
		SJH::SceneRenderer           &mRenderer;
		std::vector<SJH::PostFXPass> &mPasses;
		SJH::Mesh                   *&mQuadMesh;
		float                        &mGamma;
	};
} // namespace TopdownShooter::UI

#endif // __MYAPP_POSTFX_DEBUG_LAYER_H__
```

- [ ] **Step 2: PostFXDebugLayer.cpp 생성**

```cpp
#include "<UI>/PostFXDebugLayer.h"
#include "<render>/scene_renderer.h"
#include <imgui.h>

namespace TopdownShooter::UI
{
	PostFXDebugLayer::PostFXDebugLayer(SJH::SceneRenderer          &renderer,
	                                   std::vector<SJH::PostFXPass> &passes,
	                                   SJH::Mesh                   *&quadMesh,
	                                   float                        &gamma)
	    : mRenderer(renderer), mPasses(passes), mQuadMesh(quadMesh), mGamma(gamma)
	{
	}

	void PostFXDebugLayer::OnBuildUI()
	{
		ImGui::Begin("PostFX Debug");
		bool chainDirty = false;

		for (auto &pass : mPasses)
		{
			if (ImGui::Checkbox(pass.Name.c_str(), &pass.Enabled))
				chainDirty = true;

			if (pass.Name == "gamma" && pass.Enabled)
			{
				if (ImGui::SliderFloat("gamma##val", &mGamma, 0.1f, 2.5f))
				{
					if (pass.Material)
						pass.Material->Properties.Floats["gamma"] = mGamma;
				}
			}
		}

		if (chainDirty && mQuadMesh)
			mRenderer.SetPostFXChain(mPasses, mQuadMesh);

		ImGui::End();
	}
} // namespace TopdownShooter::UI
```

---

## Task 5: CMakeLists.txt 수정 — UI_SRC 추가

**Files:**
- Modify: `apps/_MyApp_/CMakeLists.txt:19`

현재:
```cmake
add_executable(${CHAPTER_NAME} main.cpp ${IMGUI_SRC})
```

- [ ] **Step 1: UI_SRC 변수 추가 + add_executable 에 포함**

`add_executable` 직전(line 19 앞)에 아래 블록을 삽입:

```cmake
# ImGui 의존 UI 레이어 — client-side compile (SJH::engine 모듈 아님)
# PostFXDebugLayer.cpp 는 <imgui.h> 가 필요 → executable PRIVATE include path 상속을 위해 직접 추가
set(UI_SRC
    <src>/UI/PostFXDebugLayer.cpp
)
```

그리고 `add_executable` 라인을 아래로 교체:

```cmake
add_executable(${CHAPTER_NAME} main.cpp ${IMGUI_SRC} ${UI_SRC})
```

---

## Task 6: main.cpp 통합

**Files:**
- Modify: `apps/_MyApp_/main.cpp`

변경 포인트 4개:
1. include 3줄 추가
2. 멤버 2개 추가 (`mImGuiStack`, `mShowEditor`)
3. `startup()` 끝 — ImGui init 직후 레이어 Push 2개 추가
4. `render()` — `BuildExitWindow()` / `BuildPostFXDebugWindow()` 호출 → `mImGuiStack.RenderAll(mShowEditor)` 교체
5. `onKey()` — F1 토글 추가
6. `BuildExitWindow()` / `BuildPostFXDebugWindow()` 메서드 제거

- [ ] **Step 1: includes 추가**

파일 상단 `#include "<render>/postfx_pass.h"` 아래에 삽입:

```cpp
#include "<UI>/ExitButtonLayer.h"
#include "apps/_MyApp_/src/UI/ImGuiLayerStack.h"
#include "<UI>/PostFXDebugLayer.h"
```

- [ ] **Step 2: 멤버 추가**

멤버 섹션(`// ImGui` 블록, 현재 line ~488)을 아래로 교체:

```cpp
		// ImGui
		ImGuiContext          *mImGuiCtx   = nullptr;
		const SJH::Texture    *mExitTex    = nullptr;
		UI::ImGuiLayerStack    mImGuiStack;
		bool                   mShowEditor = true;
```

- [ ] **Step 3: startup() — ImGui init 직후 레이어 Push 삽입**

현재 `startup()` 끝(line ~213):
```cpp
			glfwSetScrollCallback(window, ImGui_ImplGlfwGL3_ScrollCallback);
			glfwSetCharCallback(window, ImGui_ImplGlfwGL3_CharCallback);
		}
```

`glfwSetCharCallback` 다음 줄 + `}` 전에 삽입:
```cpp
			// ImGui 레이어 등록 — Game(항상) / Editor(F1 토글)
			mImGuiStack.Push(std::make_unique<UI::ExitButtonLayer>(window, mExitTex));
			mImGuiStack.Push(std::make_unique<UI::PostFXDebugLayer>(
			    mRenderSys, mPostFXPasses, mCachedQuadMesh, mGamma));
```

- [ ] **Step 4: render() — BuildExitWindow/BuildPostFXDebugWindow 교체**

현재 (line ~259-260):
```cpp
			BuildExitWindow();
			BuildPostFXDebugWindow();
			ImGui::Render();
```

교체:
```cpp
			mImGuiStack.RenderAll(mShowEditor);
			ImGui::Render();
```

- [ ] **Step 5: onKey() — F1 토글 추가**

`onKey()` 안의 `if (ImGui::GetIO().WantCaptureKeyboard) return;` 바로 다음에 삽입:

```cpp
			if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
				mShowEditor = !mShowEditor;
```

- [ ] **Step 6: BuildExitWindow() / BuildPostFXDebugWindow() 메서드 제거**

`BuildPostFXChain(...)` 메서드 바로 다음에 있는 두 메서드 전체 삭제:
- `void BuildExitWindow() { ... }` (약 30줄)
- `void BuildPostFXDebugWindow() { ... }` (약 25줄)

---

## Task 7: 빌드 + 동작 검증

- [ ] **Step 1: 빌드**

```bash
cmake --build --preset ninja --target _MyApp_ 2>&1 | grep -E "error:|FAILED|Linking"
```

Expected: `Linking CXX executable apps/_MyApp_/_MyApp_; ...` (에러 없음)

- [ ] **Step 2: 실행 확인**

```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

확인 항목:
- 종료 버튼 (좌상단 +64px) 표시됨 — 클릭 시 창 닫힘
- PostFX Debug 창 표시됨 — 5개 체크박스 + gamma 슬라이더 작동
- `F1` 키 → PostFX Debug 창 토글 (종료 버튼은 그대로 표시됨)
- 기존 기능(WASD 이동, 마우스 좌클릭 VFX, G키 데미지 효과) 정상 작동

- [ ] **Step 3: Commit**

```bash
git add apps/_MyApp_/src/UI/ \
        apps/_MyApp_/CMakeLists.txt \
        apps/_MyApp_/main.cpp
git commit -m "$(cat <<'EOF'
feat(_MyApp_): ImGui Layer 분리 — Game GUI / Editor UI

IImGuiLayer(Kind: Game/Editor) + ImGuiLayerStack 도입.
ExitButtonLayer(Game) + PostFXDebugLayer(Editor) 생성자 주입 패턴.
F1 키로 Editor 레이어 전체 토글.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## 자가 검토 결과

**Spec coverage:**
- [x] `IImGuiLayer` Kind(Game/Editor) 분리 → Task 1
- [x] `ImGuiLayerStack` Push/RenderAll/Clear → Task 2
- [x] `ExitButtonLayer` 좌상단 투명 텍스처 버튼 → Task 3
- [x] `PostFXDebugLayer` 5종 토글 + gamma 슬라이더 → Task 4
- [x] CMakeLists.txt UI_SRC 등록 → Task 5
- [x] main.cpp 통합 + F1 토글 → Task 6

**Placeholder scan:** 없음 — 모든 step에 완전한 코드 포함.

**Type consistency:**
- `ImGuiLayerStack::Push` 인자: `std::unique_ptr<IImGuiLayer>` → Task 6 Step 3에서 `make_unique<UI::ExitButtonLayer>` / `make_unique<UI::PostFXDebugLayer>` ✓
- `PostFXDebugLayer` 생성자: `SJH::Mesh*&` → Task 6 Step 3에서 `mCachedQuadMesh` (멤버 타입 `SJH::Mesh* = nullptr`) ✓
- `ImGui::PopStyleColor(3)` 위치: Begin() 이전 Push 3종 → End() 이후 Pop ✓ (v1.53 CheckStacksSize 규칙 준수)
