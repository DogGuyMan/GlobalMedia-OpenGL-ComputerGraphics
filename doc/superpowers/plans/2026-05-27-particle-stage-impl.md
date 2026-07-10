# ParticleStage Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Effekseer 파티클을 backbuffer 직접 Draw → sceneFB 합성으로 이동하여 PostFX 체인 (blur/gamma/invert/sharpening/sobel) 이 자동 적용되도록 `ParticleStage : IRenderStage` 1 개를 도입.

**Architecture:** 선행 SP-RenderStage 정착으로 `mStages` 컬렉션 (`std::vector<std::unique_ptr<IRenderStage>>`) 이 이미 동작 중. `[worldCam, screenCam, ScreenQuadStage]` 3 element 사이에 `ParticleStage` 를 `begin+1` 위치에 insert — 최종 `[worldCam, ParticleStage, screenCam, ScreenQuadStage]`. ParticleStage 는 Client 거주 (`apps/_MyApp_/src/VFX/`) 로 Engine 코어가 game_deps PUBLIC 합류 강제 회피.

**Tech Stack:** C++17, CMake, Effekseer 1.7.3.0 (`MyApp::VFX` 모듈), SJH::render/SJH::scene (engine 코어), spdlog (PRIVATE 진단).

**Spec:** [`doc/superpowers/specs/2026-05-27-particle-stage-design.md`](../specs/2026-05-27-particle-stage-design.md)

---

## Summary

| # | Task | 변경 파일 | 핵심 결과 | 검증 |
|---|------|-----------|-----------|------|
| T1 | ParticleStage 클래스 신규 + CMake 등록 | `ParticleStage.h/.cpp` (신규 2), `src/VFX/CMakeLists.txt` (±2) | `myapp_vfx` STATIC 라이브러리에 ParticleStage symbol 빌드 합류 | `cmake --build --preset ninja --target _MyApp_` 성공 (link 통과 — 단 main.cpp 가 미사용이라 실행은 변경 전 동작) |
| T2 | main.cpp stages 통합 + 기존 VFX.Draw 폐기 | `apps/_MyApp_/main.cpp` (±10) | stages 순회에 ParticleStage 합류, backbuffer Draw 제거 | 빌드 성공 + `_MyApp_` 실행 시 gamma 0.5 토글 시 distortion 파티클도 어두워짐 (시각 회귀 V3 통과) |

**의존 그래프**: T1 → T2 (T2 가 ParticleStage 헤더 include).

**invariant**:
- T1 commit 시점에서 *시각 동작은 변경 전과 동일* (ParticleStage 가 stages 컬렉션에 등록 안 됨).
- T2 commit 시점에서 *Effekseer 가 PostFX 적용된 모습* — gamma/sobel/invert 토글 시 파티클도 일관 처리.

---

## Task 1: ParticleStage 클래스 신규 + CMake 등록

**Files:**
- Create: `apps/_MyApp_/src/VFX/ParticleStage.h`
- Create: `apps/_MyApp_/src/VFX/ParticleStage.cpp`
- Modify: `apps/_MyApp_/src/VFX/CMakeLists.txt` (line 10 의 `add_library` 소스 목록 + line 22~28 의 `target_link_libraries` PUBLIC)

---

- [ ] **Step 1: ParticleStage 헤더 작성**

Create `apps/_MyApp_/src/VFX/ParticleStage.h`:

```cpp
#ifndef _TOPDOWNSHOOTER_VFX_PARTICLESTAGE_H__
#define _TOPDOWNSHOOTER_VFX_PARTICLESTAGE_H__

#include "<render>/render_stage.h"

namespace SJH::Scene { class Camera; }

namespace TopdownShooter::VFX
{
	class VFXSystem;

	/// @brief Effekseer 파티클을 WorldCamera 의 sceneFB 에 합성하는 렌더 stage.
	/// @details stages 컬렉션의 [worldCam, screenCam] 사이 거주.
	///          sceneFB 는 worldCam->GetTargetRenderTarget() 으로 매 프레임 동적 도출
	///          (진실의 원천 단일화 — architecture.md §11.5). resize 자동 추적.
	///          GL state 는 Effekseer 내부 BeginRendering/EndRendering 이 책임.
	/// @note    Client 거주 — Engine 코어 (SJH::render) 가 game_deps PUBLIC 합류 강제 회피.
	class ParticleStage : public SJH::IRenderStage
	{
	  public:
		/// @param vfx       VFXSystem (비소유). nullptr 시 Render 호출 무시.
		/// @param worldCam  Effekseer view/proj 출처 + sceneFB 출처 (비소유). nullptr 시 무시.
		ParticleStage(VFXSystem* vfx, SJH::Scene::Camera* worldCam);

		/// @brief sceneFB(=worldCam->GetTargetRenderTarget()) bind + Effekseer Draw.
		/// @note  target 인자는 사용하지 않음 — Camera 의 RT 사용 (CameraStage 와 동일 패턴).
		void Render(SJH::RenderTarget& target) override;

	  private:
		VFXSystem*           mVFX      = nullptr;
		SJH::Scene::Camera*  mWorldCam = nullptr;
	};
}

#endif // _TOPDOWNSHOOTER_VFX_PARTICLESTAGE_H__
```

---

- [ ] **Step 2: ParticleStage 구현 작성**

Create `apps/_MyApp_/src/VFX/ParticleStage.cpp`:

```cpp
#include "apps/_MyApp_/src/VFX/ParticleStage.h"

#include "apps/_MyApp_/src/VFX/VFXSystem.h"
#include "render/device_context.h"
#include "src/buffer/render_target.h"
#include "scene/camera.h"

#include <<spdlog>/spdlog.h>
#include <vmath.h>

namespace TopdownShooter::VFX
{
	ParticleStage::ParticleStage(VFXSystem* vfx, SJH::Scene::Camera* worldCam)
	    : mVFX(vfx), mWorldCam(worldCam)
	{
	}

	void ParticleStage::Render(SJH::RenderTarget& /*target*/)
	{
		if (!mVFX || !mWorldCam)
		{
			spdlog::warn("ParticleStage::Render — vfx/worldCam nullptr — skip.");
			return;
		}

		// 진실의 원천 단일화 — worldCam 의 RT 가 sceneFB (D-1).
		auto* rt = mWorldCam->GetTargetRenderTarget();
		if (!rt)
		{
			spdlog::warn("ParticleStage::Render — worldCam.GetTargetRenderTarget() nullptr — skip.");
			return;
		}

		// sceneFB bind (NoClear — WorldCamera 가 이미 그린 결과 보존).
		SJH::DeviceContext::Get().BindTarget(*rt);

		// Effekseer 자체 GL state setup + Draw (D-2 — state 명시 set 하지 않음).
		const vmath::mat4 view = mWorldCam->GetViewMatrix();
		const vmath::mat4 proj = mWorldCam->GetProjectionMatrix();
		mVFX->Draw(&view[0][0], &proj[0][0]);
	}
}
```

---

- [ ] **Step 3: CMakeLists.txt 에 ParticleStage.cpp 등록 + 의존성 추가**

Modify `apps/_MyApp_/src/VFX/CMakeLists.txt`:

**3-a. `add_library` 소스 목록에 추가** — 기존:

```cmake
add_library(myapp_vfx STATIC
    VFXSystem.cpp             # M5 CL2 — Effekseer Manager+Renderer owner
    EffekseerPlayable.cpp
)
```

→ 변경 후:

```cmake
add_library(myapp_vfx STATIC
    VFXSystem.cpp             # M5 CL2 — Effekseer Manager+Renderer owner
    EffekseerPlayable.cpp
    ParticleStage.cpp         # SP-ParticleStage — Effekseer→sceneFB 합성 stage
)
```

**3-b. `target_link_libraries` PUBLIC 에 SJH::render + SJH::scene 추가** — 기존:

```cmake
target_link_libraries(myapp_vfx
    PUBLIC
        SJH::playable   # PlayableBase 베이스 (헤더 노출)
        project_deps    # vmath / GL
        game_deps       # Effekseer + EffekseerRendererGL 헤더 + lib 자동 link
    PRIVATE
        spdlog
)
```

→ 변경 후:

```cmake
target_link_libraries(myapp_vfx
    PUBLIC
        SJH::playable   # PlayableBase 베이스 (헤더 노출)
        SJH::render     # IRenderStage / DeviceContext / RenderTarget (ParticleStage 헤더/구현)
        SJH::scene      # Scene::Camera (ParticleStage.cpp)
        project_deps    # vmath / GL
        game_deps       # Effekseer + EffekseerRendererGL 헤더 + lib 자동 link
    PRIVATE
        spdlog
)
```

---

- [ ] **Step 4: 빌드 확인**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected:
- 컴파일 + link 성공 (warning/error 없음).
- `myapp_vfx` STATIC 라이브러리에 `ParticleStage.o` 합류.
- `_MyApp_` 실행 파일 생성 — 단 main.cpp 가 ParticleStage 미사용이라 실행 시 시각 변경 0.

문제 발생 시 확인:
- `'<render>/render_stage.h' file not found` → Step 3-b 의 `SJH::render` PUBLIC link 누락
- `'scene/camera.h' file not found` → Step 3-b 의 `SJH::scene` PUBLIC link 누락
- `Camera::GetTargetRenderTarget()` 시그니처 불일치 → `src/scene/camera.h` 확인 후 반환 타입 일치 확인

---

- [ ] **Step 5: Commit**

Run:
```bash
git add apps/_MyApp_/src/VFX/ParticleStage.h apps/_MyApp_/src/VFX/ParticleStage.cpp apps/_MyApp_/src/VFX/CMakeLists.txt
git commit -m "[feat] : ParticleStage 신규 (Effekseer→sceneFB 합성 stage)

spec doc/superpowers/specs/2026-05-27-particle-stage-design.md T1.

- ParticleStage.h/.cpp 신규 — IRenderStage 상속, worldCam 의 RT 동적 도출
- src/VFX/CMakeLists.txt — SJH::render + SJH::scene PUBLIC link 추가
- 결정 D-1 (sceneFB 동적 도출) + D-2 (GL state 신뢰) 적용

T1 commit — main.cpp 가 ParticleStage 미사용이라 시각 동작 변경 0.
T2 에서 main.cpp 통합 + 시각 회귀 검증."
```

Expected: commit 성공, git log 에 `[feat] : ParticleStage 신규` 출력.

---

## Task 2: main.cpp stages 통합 + 기존 VFX.Draw 폐기

**Files:**
- Modify: `apps/_MyApp_/main.cpp` — include 추가 (line 18-50 사이의 include 블록), stages insert (Director.Init() 직후 = 현 line 222 직후), 기존 VFX.Draw 블록 삭제 (현 line 342-348)

---

- [ ] **Step 1: include 추가**

Modify `apps/_MyApp_/main.cpp` — 기존 include 블록 (대략 line 26~48) 의 `"apps/_MyApp_/src/VFX/EffekseerPlayable.h"` 줄 *바로 아래* 에 추가:

기존:
```cpp
#include "apps/_MyApp_/src/VFX/EffekseerPlayable.h"
#include "playable/composite_playable.h"
```

→ 변경 후:
```cpp
#include "apps/_MyApp_/src/VFX/EffekseerPlayable.h"
#include "apps/_MyApp_/src/VFX/ParticleStage.h"
#include "playable/composite_playable.h"
```

---

- [ ] **Step 2: stages insert — Director.Init() 직후**

Modify `apps/_MyApp_/main.cpp` — 기존 line 221~222 부근 (`Director::Get().Init();` 직후) 에 ParticleStage insert 추가:

기존:
```cpp
			// === M5 — Director 가 Audio + VFX + Physics 일괄 초기화 ===
			TopdownShooter::Director::Get().Init();
			auto &phys = TopdownShooter::Director::Get().Physics();
```

→ 변경 후:
```cpp
			// === M5 — Director 가 Audio + VFX + Physics 일괄 초기화 ===
			TopdownShooter::Director::Get().Init();
			auto &phys = TopdownShooter::Director::Get().Physics();

			// ── ParticleStage insert — Director.Init() 직후 (VFXSystem 사용 가능 시점) ──
			//    위치: stages.begin() + 1 (worldCam 뒤, screenCam 앞)
			//    최종 stages: [worldCam, ParticleStage, screenCam, ScreenQuadStage]
			//    spec D-5 — Director.Init() 흐름 보존을 위해 카메라 stages insert 와 분리.
			mStages.insert(
			    mStages.begin() + 1,
			    std::make_unique<TopdownShooter::VFX::ParticleStage>(
			        &TopdownShooter::Director::Get().VFX(), mCamera));
```

---

- [ ] **Step 3: 기존 backbuffer VFX.Draw 블록 삭제**

Modify `apps/_MyApp_/main.cpp` — 기존 line 342~348 의 블록 전부 삭제:

삭제 대상 (정확히 7 줄):
```cpp
			// === M5 — Effekseer 렌더 (backbuffer 합성 후, swap 전) ===
			if (mCamera)
			{
				vmath::mat4 view = mCamera->GetViewMatrix();
				vmath::mat4 proj = mCamera->GetProjectionMatrix();
				TopdownShooter::Director::Get().VFX().Draw(&view[0][0], &proj[0][0]);
			}
```

→ 변경 후: 블록 제거 — 위 `for (auto &s : mStages) s->Render(*mDefaultTarget);` 와 아래 `mImGuiStack.RenderAll(mShowEditor);` 사이가 빈 줄 1 개만 남도록.

---

- [ ] **Step 4: 빌드 확인**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
```

Expected:
- 컴파일 + link 성공.
- main.cpp 에서 `mCamera` / `mStages` 등 기존 식별자 모두 valid.

문제 발생 시 확인:
- `'apps/_MyApp_/src/VFX/ParticleStage.h' file not found` → Step 1 의 include 누락
- `'TopdownShooter::VFX::ParticleStage' is incomplete type` → Step 1 의 include 위치 확인 (다른 include 블록과 분리됐는지)
- `mStages.insert` 위치 컴파일 에러 → Step 2 의 insert 위치가 `mCamera` 가 valid (line 145 의 대입 이후) 한지 확인

---

- [ ] **Step 5: 실행 + 시각 회귀 검증**

Run:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

검증 절차:
1. **V1 (기본 동작)**: 실행 즉시 spdlog warning 없음 확인 — `ParticleStage::Render — ...` 출력이 *없어야* 함.
2. **V2 (파티클 발사)**: 마우스 좌클릭 → distortion 파티클이 화면에 표시되는지 확인 (기존과 동일).
3. **V3 (gamma 회귀 — 핵심)**: ImGui "PostFX Debug" 패널의 gamma 토글 on + 슬라이더 0.5 → 화면 전체와 함께 **파티클도 어두워짐** 확인. 변경 전엔 파티클만 원본 밝기였음.
4. **V4 (sobel 회귀)**: sobel 토글 on → 파티클도 윤곽선 검출 확인.
5. **V8 (전체 OFF 회귀)**: 모든 PostFX 토글 OFF → 기존과 시각적으로 동일 (sceneFB → ScreenQuadStage 직접 blit).
6. **V9 (ImGui 위치)**: ImGui 패널이 backbuffer 최상위 (PostFX 미적용) 확인.

Expected: 모든 시나리오 통과.

문제 발생 시 확인:
- **파티클 안 보임**: spdlog warning 확인 → ParticleStage 가 spawn 되었으나 RT nullptr 면 worldCam 의 SetTargetRenderTarget 호출 누락
- **gamma 적용 안 됨 (V3 실패)**: ParticleStage 가 *backbuffer* 에 그렸을 가능성 → ParticleStage::Render 의 `BindTarget(*rt)` 호출 확인
- **검은 화면**: DeviceContext::BindTarget 이 자동 clear 하는 경우 — sceneFB 의 WorldMesh 결과 소멸. DeviceContext 동작 확인 (NoClear semantic)

---

- [ ] **Step 6: Commit**

Run:
```bash
git add apps/_MyApp_/main.cpp
git commit -m "[refactor] : Effekseer ParticleStage 통합 + backbuffer Draw 폐기

spec doc/superpowers/specs/2026-05-27-particle-stage-design.md T2.

- main.cpp include — apps/_MyApp_/src/VFX/ParticleStage.h 추가
- Director.Init() 직후 mStages.insert(begin+1, ParticleStage) — 결정 D-5
- 기존 line 342-348 의 backbuffer VFX.Draw 블록 삭제
- 최종 stages 순서: [worldCam, ParticleStage, screenCam, ScreenQuadStage]

시각 회귀:
- gamma 0.5 시 distortion 파티클도 어두워짐 확인 (변경 전엔 원본 밝기)
- sobel 토글 시 파티클도 윤곽선 검출 확인
- 전체 PostFX OFF 시 기존과 동일 시각

ImGui 는 backbuffer 최상위 그대로 (PostFX 미적용 — Editor UI 정통)."
```

Expected: commit 성공.

---

## Out of Scope (본 plan 범위 외 — 명시적 제외)

| 항목 | 이유 |
|---|---|
| **VFXSystem 시그니처 변경** | `Draw(view, proj)` 그대로 사용. RT 바인딩은 ParticleStage 책임. |
| **EffekseerPlayable 변경** | Component 라이프사이클 무관. ParticleStage 는 Effekseer Manager 가 보유한 모든 effect 한 번에 그림. |
| **migrate_demo / audio_demo** | 둘 다 Effekseer 미사용. 영향 0. |
| **ImGui 의 PostFX 통합** | 별도 SP 후보. Editor UI 가 PostFX 미적용이 정통. |
| **다중 view (분할 화면 / 미니맵)** | spec §9.3 — Future Work. |
| **단위 테스트 추가** | 사용자 정책 `no_auto_tests`. 시각 회귀로 검증. |
| **GL state push/pop 가드** | spec D-2 — Effekseer 자체 BeginRendering/EndRendering 책임. |
| **VAO/EBO 명시 보호** | spec — `mesh_pass_processor.cpp:228` 의 `ebo->Bind()` 재핀 자동 가드. |

---

## Self-Review

**Spec coverage**:
- ✅ spec §2 D-1 (sceneFB 동적 도출) → T1 Step 2 의 ParticleStage.cpp 의 `mWorldCam->GetTargetRenderTarget()` 호출
- ✅ spec §2 D-2 (GL state 신뢰) → T1 Step 2 의 코드에서 state set 없음, BindTarget + Draw 만
- ✅ spec §2 D-3 (Client 거주) → T1 Step 1-2 파일 경로 `apps/_MyApp_/src/VFX/`
- ✅ spec §2 D-4 (SceneRenderer 의존 0) → T1 Step 2 의 include 에 `scene_renderer.h` 없음
- ✅ spec §2 D-5 (Director.Init() 직후 insert) → T2 Step 2
- ✅ spec §4.1 파일별 변경표 — T1+T2 의 모든 파일 커버
- ✅ spec §7.1 V1~V10 시각 회귀 → T2 Step 5
- ✅ spec §6 Out of Scope → 본 plan Out of Scope 일치

**Placeholder scan**: TBD/TODO/"implement later"/모호한 step 없음 — 모든 step 에 exact code + exact command + expected output.

**Type consistency**:
- `ParticleStage(VFXSystem*, SJH::Scene::Camera*)` — T1 Step 1 (헤더), T1 Step 2 (구현), T2 Step 2 (main.cpp 호출) 일치
- `mVFX` / `mWorldCam` 멤버명 — T1 Step 1, T1 Step 2 일치
- `mWorldCam->GetTargetRenderTarget()` 호출 일치 (T1 Step 2)
- `mStages.insert(mStages.begin() + 1, ...)` — T2 Step 2 일치 (spec §3 의 [0] worldCam, [1] ParticleStage 순서와 일치)

**모든 일치 확인됨**.

---

## 후속 (plan 완료 후)

- ImGui PostFX 통합 SP 후보 (Editor UI vs HUD 분리)
- spec §9.1 — Effekseer 의 sceneFB depth occlusion 검증 결과를 시각 회귀 시점에 spec/memory 로 기록
- spec §9.3 — 다중 view (분할 화면 / 미니맵) 도입 시 ParticleStage 가 카메라당 1 개 필요 검토

---

**plan 끝**.
