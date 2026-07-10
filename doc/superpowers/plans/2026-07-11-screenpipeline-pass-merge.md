# ScreenPipeline + Pass init-task 병합 + VFX/UI 관심사 분리 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> ⚠ 이 문서가 있는 `doc/` 는 gitignored(로컬 전용). 코드 경로는 2026-07-11 기준.

**Goal:** `_MyApp_` phase-2 InitScheduler task 그래프에서 `ScreenPipeline(T2)`+`Pass(T4)` 를 단일 `RenderPipeline` task 로 인라인 병합하고, VFX 카탈로그 로드를 `World` 로 이관하여 `World→VfxUi→ScreenPipeline` 의존 사이클을 근본 제거한다. (렌더 결과 = 골든 14/14 bit-동일 무회귀.)

**Architecture:** 현재 phase-2 task 는 `ScreenPipeline`(no dep) · `VfxUi`(Needs ScreenPipeline) · `World`(Needs VfxUi) · `Pass`(Needs ScreenPipeline+World) 4개. `ScreenPipeline`+`Pass` 를 병합하면 병합 task 가 `World` 를 Needs 상속 → `M→World→VfxUi→M` 사이클(InitScheduler `std::abort`). 사이클의 뿌리인 `World→VfxUi`(BuildStage 의 `FindEffect("orbital_background")` 가 VfxUi 의 `TEST_EFFECTS` 선행 로드에 의존) 를 끊기 위해 **VFX 카탈로그 로드를 World 소유로 이관**한다. 그 결과 VfxUi task 는 순수 UI(`DebugUi`) 로 축소되고, 새 그래프는 선형 `Core→World→RenderPipeline→DebugUi→Enter→Fsm` 가 된다.

**Tech Stack:** C++17 · CMake(ninja preset) · InitScheduler(Kahn topo-sort, `apps/_MyApp_/src/Bootstrap/`) · PassIterator · ResourceRegistry · 골든 이미지 게이트(SJH_GOLDEN_CAPTURE) · ctest.

---

## Locked decisions (이 세션 확정 — 재론 금지)

1. **사이클 절단 = VFX 카탈로그 로드를 `World` 로 이관** (사용자 확정). `World→VfxUi` 간선의 존재 이유(orbital 선행 로드)를 뿌리에서 제거. Option A(PassDebugLayer grayscale lazy)는 불필요 — `DebugUi` 가 `RenderPipeline` 뒤에 실행되므로 grayscale 머티리얼이 이미 존재.
2. **병합 범위 = main.cpp 인라인** (사용자 확정). `RenderPipelineBuilder` 모듈 추출은 이번 범위 밖(별도 효과).
3. **VfxUi task 는 순수 Debug/게임 UI 초기화로 축소** (사용자 확정 "VFX 의존 아예 제거"). `TEST_EFFECTS` 로드는 World 로, 죽은 `VfxSpawnLayer`(+`mVfxLayer`)는 제거.
4. **네이밍** (사용자 확정): 병합 task = `RenderPipeline`, 구 VfxUi = `DebugUi`.
5. **VfxSpawnLayer 사용처 제거** (사용자 무이의). 헤더 파일 `VfxSpawnLayer.h`(`apps/_MyApp_/src/UI` 하위)는 당초 dormant 로 잔존(삭제는 후속 판단) 예정이었으나, **이 커밋에서 파일 자체가 삭제됨**(후속 판단이 삭제 쪽으로 확정). 근거: `mVfxLayer` 미읽힘 · `GetSelectedEffect()` 미호출 · `OnBuildUI` 전체 주석(무렌더).
6. **골든 bit-동일 = 무회귀 계약.** init 순서만 바뀌고 최종 씬/Pass 실행 순서는 불변 → 14/14 bit-동일 유지.

---

## 왜 안전한가 (제로컨텍스트 근거)

- **orbital VFX 유지**: 이관 후 World 가 `TEST_EFFECTS` 를 `BuildWorldScene`(내부 `BuildStage`) *전에* 로드 → `FindEffect("orbital_background")` 여전히 성공. 구 동작(VfxUi 가 World 전에 로드)과 동일 net 효과.
- **런타임 소비자 유지**: `laser`(UltimateLaser R 궁극기), `orbital_background`(BuildStage) 등 `reg.FindEffect` 소비자가 참조하는 이펙트가 계속 선행 로드됨.
- **golden_full(ImGui 포함) bit-동일**: 제거되는 `VfxSpawnLayer::OnBuildUI` 는 전체 주석 상태라 ImGui draw data 에 기여 0 → 제거해도 ImGui 출력 불변.
- **PassDebug grayscale 표시 유지**: `DebugUi.Needs({RenderPipeline})` 로 `DebugUi` 가 RenderPipeline 뒤 실행 → `PassDebugLayer` 생성자의 `mat_pass_grayscale_vignetting` 조회가 non-null(구 동작과 동일).
- **선언 순서 무관**: InitScheduler 는 모든 task 수집 후 topo-sort(enum tiebreak) → 소스 선언 순서 무관, `.Needs()` 만이 hard 순서 결정. `RenderPipeline` 을 소스 상단(구 ScreenPipeline 자리)에 두고 `Needs({World})` 로 두어도 World 선행 실행 보장.

---

## File Structure

| 파일 | 역할 | 변경 |
|---|---|---|
| `apps/_MyApp_/src/Bootstrap/InitTaskId.h` | init task 식별자 enum + ToString | enum 7→6개(VfxUi→DebugUi rename, ScreenPipeline+Pass→RenderPipeline 병합), ToString 동기 + 정렬 정정 |
| `apps/_MyApp_/main.cpp` | phase-2 task 조립(OnSceneSetup) + 멤버 | World task(+VFX 로드, -Needs), ScreenPipeline+Pass→RenderPipeline 병합, VfxUi→DebugUi strip, `VfxSpawnLayer` include/멤버 제거 |
| `VfxSpawnLayer.h`(`apps/_MyApp_/src/UI` 하위) | **삭제** | 당초 dormant 잔존 계획 → 이 커밋에서 파일째 삭제로 상향 |

병합/제거만 — 신규 파일 없음. 편집 1~4 는 함께 착지해야 컴파일(중간 상태 미컴파일 정상) → 빌드 검증은 편집 완료 후 일괄.

---

## Task 1: InitTaskId.h — enum + ToString 재구성

**Files:**
- Modify: `apps/_MyApp_/src/Bootstrap/InitTaskId.h:22-47`

- [ ] **Step 1: enum `EInitTask` 를 6개로 재작성**

기존 `enum class EInitTask { ... }` 블록([InitTaskId.h:22-31](apps/_MyApp_/src/Bootstrap/InitTaskId.h#L22))을 아래로 교체:

```cpp
	enum class EInitTask
	{
		Core,          ///< phase1: 렌더타깃 + 시스템 init + 오디오 워밍업.
		World,         ///< phase2: VFX 카탈로그 로드(orbital 포함) + WorldScene + 스테이지 + 플레이어 + 웨이브.
		RenderPipeline,///< phase2: 스크린 자원(PostFX/present/ScreenCamera) + 전체 Pass 컬렉션 조립. World 뒤.
		DebugUi,       ///< phase2: ImGui 컨텍스트 + 게임/디버그 UI 레이어(PauseButton, PassDebug). RenderPipeline 뒤.
		Enter,         ///< phase3: Director.Enter.
		Fsm,           ///< phase3: GameContext + Stage FSM.
	};
```

- [ ] **Step 2: `ToString` 을 6개로 동기 (기존 라벨 정렬 오류도 정정)**

기존 `ToString`([InitTaskId.h:34-47](apps/_MyApp_/src/Bootstrap/InitTaskId.h#L34))을 아래로 교체:

```cpp
	inline const char *ToString(EInitTask id)
	{
		switch (id)
		{
		case EInitTask::Core:           return "Core";
		case EInitTask::World:          return "World";
		case EInitTask::RenderPipeline: return "RenderPipeline";
		case EInitTask::DebugUi:        return "DebugUi";
		case EInitTask::Enter:          return "Enter";
		case EInitTask::Fsm:            return "Fsm";
		}
		return "?";
	}
```

> 검증은 Task 5(빌드)에서 일괄. 이 파일 단독 컴파일 불가(main.cpp 가 구 enum 참조 중).

---

## Task 2: main.cpp — World task 에 VFX 카탈로그 로드 이관 + Needs 제거

**Files:**
- Modify: `apps/_MyApp_/main.cpp:216-241` (World task)

- [ ] **Step 1: World task 헤더 주석 + `.Needs()` 제거 + VFX 로드 루프 삽입**

기존([main.cpp:216-218](apps/_MyApp_/main.cpp#L216)):
```cpp
			// T3 world -- WorldScene(camera/light/skybox) + 스테이지 액터 + FxRoot + spawn 컨텍스트
			//             + muzzle 이펙트 + 플레이어 + 웨이브 컨트롤러.
			sched.Task(Bootstrap::EInitTask::World).Needs({Bootstrap::EInitTask::VfxUi}).Gl().Does([&] { // vfxUi 가 TEST_EFFECTS(orbital_background) 를 선행 로드 -> 스테이지 FindEffect 의존
```
교체:
```cpp
			// T world -- VFX 카탈로그 로드(orbital 포함) + WorldScene(camera/light/skybox/PCB/stage)
			//            + FxRoot + spawn 컨텍스트 + 플레이어 + 웨이브 컨트롤러.
			//   VFX 초기화 소유 -- BuildStage 의 FindEffect("orbital_background") 및 런타임 소비자(laser 등)
			//   가 참조하는 이펙트를 이 태스크가 선행 로드 (구 VfxUi 에서 이관, World->VfxUi 사이클 제거).
			sched.Task(Bootstrap::EInitTask::World).Gl().Does([&] {
				// VFX 카탈로그 로드 -- BuildWorldScene(BuildStage->orbital) 및 UltimateLaser(laser) 등
				//   FindEffect 소비자보다 반드시 선행. 실패는 개별 warn 후 계속(효과별 no-op 허용).
				for (const auto &v : VFX::TEST_EFFECTS)
					if (!reg.CreateEffect(vfxs.GetManager(), v.key, v.path))
						spdlog::warn("[vfx] load failed: {}", v.key);
```

> 즉, `.Needs({Bootstrap::EInitTask::VfxUi})` 제거 + 여는 람다 바로 다음 줄에 `TEST_EFFECTS` 로드 루프 삽입. 이후 본문(`auto worldScene = Bootstrap::BuildWorldScene(...)` 부터 `return mCamera != nullptr && mSpriteActor != nullptr;` 까지, [main.cpp:219-240](apps/_MyApp_/main.cpp#L219))은 **그대로 유지**.

---

## Task 3: main.cpp — ScreenPipeline + Pass 를 단일 RenderPipeline task 로 병합

**Files:**
- Modify: `apps/_MyApp_/main.cpp:147-214` (구 ScreenPipeline task — RenderPipeline 로 교체)
- Delete: `apps/_MyApp_/main.cpp:243-284` (구 Pass task — 병합되어 삭제)

- [ ] **Step 1: 구 ScreenPipeline task(`T2`) 를 병합 RenderPipeline task 로 교체**

기존 `// T2 screenPipeline ...` 주석부터 해당 `});` 까지([main.cpp:147-214](apps/_MyApp_/main.cpp#L147))를 아래 블록으로 통째 교체. (스크린 자원 생성 → Pass 조립 순. 기존 코드 로직/주석 보존, `[ScreenPipeline]` 로그 태그만 `[RenderPipeline]` 로 통일. 사용자 `! 리펙토링 대상` 인라인 노트 보존.)

```cpp
			// T renderPipeline -- 스크린 자원(PostFX 체인/present/ScreenCamera) + 전체 Pass 컬렉션 조립.
			//   구 ScreenPipeline(T2) + Pass(T4) 병합. World 뒤(카메라/스카이박스 참조). 단일 람다 내부
			//   순서: 스크린 자원 생성 -> PassIterator Add(std::move). 비소유 관찰 포인터는 move 전 캡처.
			sched.Task(Bootstrap::EInitTask::RenderPipeline).Needs({Bootstrap::EInitTask::World}).Gl().Does([&] {
				// ===== (구 ScreenPipeline) 스크린 자원 생성 =====
				// 풀스크린 blit 용 screen quad mesh (PostFxPass 가 비소유 참조 - 소유=rr).
				mScreenQuadMeshPtr = reg.RegisterMesh("mesh_screen_quad", SJH::Mesh::CreateScreenQuad());
				if (!mScreenQuadMeshPtr)
				{
					spdlog::error("[RenderPipeline] screen quad Mesh 등록 실패");
					return false;
				}

				// present(passthrough) Material - 효과 체인 끝에서 마지막 활성 결과를 backbuffer 로 blit (output=nullptr present).
				auto *ptProg = reg.CreateProgram(
				    Playable::PASSTHOURH_PROGRAM_CONFIG.Name,
				    Playable::PASSTHOURH_PROGRAM_CONFIG.VertFile,
				    Playable::PASSTHOURH_PROGRAM_CONFIG.FragFile);
				if (!ptProg)
				{
					spdlog::error("[RenderPipeline] passthrough 셰이더 로드 실패");
					return false;
				}
				mPresentMatPtr = reg.CreateSharedMaterial("mat_pass_present");
				mPresentMatPtr->SetProgram(ptProg);

				// PostFX 효과별 Program + Material("mat_pass_<name>") + 중간 FBO (config 순서, 1:1).
				//   rr 키 = mat_pass_<name> (HpGrayscalePostFX / PostFXTweenPlayable / PassDebugLayer 조회 규약 일치).
				mPostFXFBs.clear();
				mPostFXFBs.reserve(Playable::POSTFX_PROGRAM_CONFIGS.size());
				for (const auto &c : Playable::POSTFX_PROGRAM_CONFIGS)
				{
					auto *prog = reg.CreateProgram(c.Name, c.VertFile, c.FragFile);
					if (!prog)
					{
						spdlog::error("[RenderPipeline] PostFX 셰이더 로드 실패: {}", c.FragFile);
						mPostFXFBs.push_back(nullptr); // configs 인덱스 정합(placeholder).
						continue;
					}
					auto *mat = reg.CreateSharedMaterial(std::string("mat_pass_") + c.Name);
					mat->SetProgram(prog);
					for (const auto &[name, value] : c.InitFloats) // D-6 data-driven float 초기값.
						mat->Properties.Floats[name] = value;
					mPostFXFBs.push_back(SJH::RenderTexture::Create(mFbInfo.Width, mFbInfo.Height)); // 중간 FBO(color sampler).
				}

				// 특수 초기값(vec3/int - InitFloats 밖) - 효과 머티리얼을 키로 조회해 set.
				if (auto *fogMat = reg.FindSharedMaterial(std::string("mat_pass_") + Playable::PASS_FOG))
				{
					fogMat->Properties.Vec3s["uFogColor"] = Playable::FOG_COLOR;
					fogMat->Properties.Ints["uFogMode"] = Playable::FOG_MODE;
				}
				// grayscale 공유 Material - HpGrayscalePostFX SSOT + PassDebugLayer read-only 표시용 캡처.
				// ! 리펙토링 대상 -> Playable::VIGNETTE_COLOR를 왜 명시적으로 넣는것이지?
				// 	! 쉐이더 자체에서 값을 빨간색으로 고정시키면 될 것 같고, 이 쉐이더는 당연히 확장성은 Slang으로 처리하기 때문에
				//	! 추후 비네팅 함수를 외부로 분리하고, 오직 HPGrayScale의 구체 쉐이더 클래스만 구체화 하는식으로 해결할 수 있고,
				//	! 아래의 코드가 굳이 필요하지 않아보인다.
				// 	! 이와 비슷한 위의 Fog 또한 그렇다.
				mGrayscaleMatPtr = reg.FindSharedMaterial(std::string("mat_pass_") + Playable::PASS_GRAYSCALE_VIGNETTING);
				if (mGrayscaleMatPtr)
					mGrayscaleMatPtr->Properties.Vec3s["uVignetteColor"] = Playable::VIGNETTE_COLOR; // vec3 초기값(InitFloats 밖).

				RebindFogUniforms(); // fog uDepth(unit1) = sceneFB depth 텍스처 바인딩.

				// 화면 카메라 (resize aspect 추적용 - present 자체는 PostFxPass 가 backbuffer 직접 bind).
				auto screenCamActor = SJH::Scene::CreateScreenCameraActor(ACTOR_SCREEN_CAMERA, mFbInfo.Aspect, mSceneFB.get());
				mScreenCamera = screenCamActor->GetComponent<SJH::Scene::Camera>();
				dir.Root().AddChild(std::move(screenCamActor));

				// ===== (구 Pass) PassIterator 조립 - 코스 순서대로 Add(std::move) =====
				//   [Skybox -> World -> Particle -> PostFx(e0..eN) -> present -> ImGui]. 관찰 포인터는 move 전 캡처.
				// 1) Skybox - background-first (sceneFB clear+skybox 책임).
				mPassIterator.Add(std::make_unique<SJH::SkyboxPass>(mSkyboxRenderer, mCamera));

				// 2) World - skybox 위에 그림. SetClearsTarget(false) 로 clear 스킵. SetActivePrograms 주입 위해 관찰 포인터 캡처.
				auto worldPass = std::make_unique<SJH::WorldPass>(mCamera);
				mWorldPassPtr = &worldPass->SetClearsTarget(false); // move 전 캡처 (소유=PassIterator, 수명=app 동안 안정).
				mPassIterator.Add(std::move(worldPass));

				// 3) Particle - sceneFB 에 Effekseer 합성 (worldCam RT).
				mPassIterator.Add(std::make_unique<TopdownShooter::VFX::ParticlePass>(&GameSystems::Get().VFX(), mCamera));

				// 4) PostFX 효과 체인 - 각 효과는 중간 FBO 에 그림(output=fbo). config 순서 = 체인 순서.
				//    invert/blurring/sobel 은 기본 비활성(Enabled=false, 동적 skip).
				for (std::size_t i = 0; i < Playable::POSTFX_PROGRAM_CONFIGS.size(); ++i)
				{
					const std::string &name = Playable::POSTFX_PROGRAM_CONFIGS[i].Name;
					auto *mat = reg.FindSharedMaterial(std::string("mat_pass_") + name);
					auto *fbo = (i < mPostFXFBs.size() && mPostFXFBs[i]) ? mPostFXFBs[i].get() : nullptr;
					if (!mat || !fbo)
						continue; // 셰이더 로드 실패 placeholder - skip(체인은 lastResult 가 재연결).
					auto fx = std::make_unique<SJH::PostFxPass>(mat, fbo, mScreenQuadMeshPtr, name);
					if (name == Playable::PASS_INVERT || name == Playable::PASS_BLURRING || name == Playable::PASS_SOBEL)
						fx->Enabled = false; // 기본 비활성.
#ifdef MENTAL_MODEL_PHASE_1
					if (name == Playable::PASS_DEPTH_DEBUG)
						fx->Enabled = false; // depth_debug = Phase1 학습용(F1 로 켬).
#endif
					mPassIterator.Add(std::move(fx));
				}

				// 5) present(passthrough, output=nullptr) - 마지막 활성 효과 결과를 backbuffer 로 합성. 매 프레임 SetBackbuffer.
				auto present = std::make_unique<SJH::PostFxPass>(mPresentMatPtr, nullptr, mScreenQuadMeshPtr, "present");
				mPresentPassPtr = present.get(); // move 전 캡처.
				mPassIterator.Add(std::move(present));

				// 6) ImGui - 종단 Pass (backbuffer 위에 UI).
				mPassIterator.Add(std::make_unique<UI::ImGuiPass>());
				return true;
			});
```

- [ ] **Step 2: 구 Pass task(`T4`) 블록 전체 삭제**

`// T4 stages -- PassIterator 가 모든 Pass 를 소유 ...` 주석부터 그 task 의 `});` 까지([main.cpp:243-284](apps/_MyApp_/main.cpp#L243)) **통째 삭제**. (내용은 Task 3 Step 1 의 "PassIterator 조립" 절로 흡수됨.)

> 결과: 구 World task 와 구 VfxUi task 사이에 있던 Pass task 가 사라지고, 병합 RenderPipeline task 는 소스 상단(구 ScreenPipeline 자리)에 위치. `Needs({World})` 가 실행 순서 보장.

---

## Task 4: main.cpp — VfxUi → DebugUi 축소(VFX 제거) + include/멤버 정리

**Files:**
- Modify: `apps/_MyApp_/main.cpp:286-321` (VfxUi task → DebugUi)
- Modify: `apps/_MyApp_/main.cpp:34` (VfxSpawnLayer include 제거)
- Modify: `apps/_MyApp_/main.cpp:664` (mVfxLayer 멤버 제거)

- [ ] **Step 1: VfxUi task 를 순수 UI `DebugUi` task 로 교체**

기존 `// T5 vfxUi ...` 주석부터 해당 `});` 까지([main.cpp:286-321](apps/_MyApp_/main.cpp#L286))를 아래로 교체:

```cpp
			// T debugUi -- ImGui 컨텍스트 + 게임/디버그 UI 레이어(순수 UI, VFX 무관).
			//   RenderPipeline 뒤 실행(Needs) -- PassDebugLayer 생성자가 mat_pass_grayscale_vignetting 을 조회하므로.
			sched.Task(Bootstrap::EInitTask::DebugUi).Needs({Bootstrap::EInitTask::RenderPipeline}).Gl().Does([&] {
				mImGuiCtx = UI::BuildGameUI({window, &reg, &mImGuiStack, [this] { TogglePause(); }});

				// Pass 활성화 토글 + PostFX 파라미터 디버그 패널(Editor kind, F1). mPassIterator 는 RenderPipeline 이
				// 채우므로 포인터만 주입(매 프레임 OnBuildUI 가 lazy 조회). capture G1 일시 비활성화용 raw 캡처(소유=스택).
				{
					auto dbg = std::make_unique<UI::PassDebugLayer>(&mPassIterator);
					mPassDebugLayerPtr = dbg.get();
					mImGuiStack.Push(std::move(dbg));
				}
				return mImGuiCtx != nullptr;
			});
```

> 제거된 것: `TEST_EFFECTS` 로드 루프(→ World 이관) · 죽은 `narrow` 문자열 · `vfxEntries` · `VfxSpawnLayer` 생성/Push · `! 제거 대상` 주석 4줄.

- [ ] **Step 2: `VfxSpawnLayer` include 제거**

[main.cpp:34](apps/_MyApp_/main.cpp#L34) 삭제 (`UI/` 하위 `VfxSpawnLayer.h`):
```cpp
#include "VfxSpawnLayer.h"
```
> (참고: 파일 자체가 이 커밋에서 완전히 삭제되어, 이 include 제거는 파일 삭제에 흡수됨.)

- [ ] **Step 3: `mVfxLayer` 멤버 제거**

[main.cpp:664](apps/_MyApp_/main.cpp#L664) 삭제:
```cpp
		UI::VfxSpawnLayer *mVfxLayer = nullptr;        // VFX 테스트 드롭다운 (비소유 — 스택이 소유) // ! 모듈화 대상 (VFX UI 레이어)
```

> `VFX::TEST_EFFECTS` 를 위한 `#include "VFX/Constants.h"`([main.cpp:27](apps/_MyApp_/main.cpp#L27))는 World 가 계속 사용하므로 **유지**.

---

## Task 5: 빌드 검증 (편집 1~4 일괄)

**Files:** (없음 — 검증만)

- [ ] **Step 1: 빌드**

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
cmake --build --preset ninja --target _MyApp_
```
Expected: `EXIT=0` GREEN. (sb7 `gl.h and gl3.h` 경고 1개는 정상.) `EInitTask::VfxUi` / `::ScreenPipeline` / `::Pass` 잔존 참조 에러가 없어야 함.

- [ ] **Step 2: 구 심볼 잔존 확인**

Run:
```bash
grep -rn -e "EInitTask::VfxUi" -e "EInitTask::ScreenPipeline" -e "EInitTask::Pass" -e "mVfxLayer" -e "VfxSpawnLayer" apps/_MyApp_/main.cpp apps/_MyApp_/src/Bootstrap/InitTaskId.h
```
Expected: **매칭 0줄** (모두 제거/치환됨).

---

## Task 6: 골든 이미지 무회귀 검증 (14/14 bit-동일)

**Files:** (없음 — 검증만)

- [ ] **Step 1: 골든 캡처 실행**

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/build_ninja/apps/_MyApp_
SJH_GOLDEN_CAPTURE=1 ./_MyApp_
```
Expected: 180 프레임 후 자동 종료. `build_ninja/apps/_MyApp_/test/golden/` 에 14 PNG 생성.

- [ ] **Step 2: 레퍼런스와 bit-동일 비교**

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
FAIL=0; for f in test/golden/*.png; do \
  b="build_ninja/apps/_MyApp_/test/golden/$(basename "$f")"; \
  if ! cmp -s "$f" "$b"; then echo "DIFF: $(basename "$f")"; FAIL=1; fi; done; \
  [ $FAIL -eq 0 ] && echo "ALL 14 BIT-IDENTICAL" || echo "REGRESSION"
```
Expected: `ALL 14 BIT-IDENTICAL`. (레퍼런스 경로가 다르면 프로젝트 골든 게이트 관행에 맞춰 조정 — 핵심은 병합 전/후 14장 bit-동일.)

> DIFF 발생 시 → 사이클/순서가 아니라 자원 생성 시점 회귀 의심. `orbital_background` 로드가 BuildStage 전인지, present/PostFX 머티리얼 키가 동일한지 우선 점검.

---

## Task 7: ctest 회귀 검증

**Files:** (없음 — 검증만)

- [ ] **Step 1: ctest 실행**

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
ctest --test-dir build_ninja --output-on-failure
```
Expected: 전 케이스 pass (병합 전 기준 120/120 — 개수는 현행 스위트에 따름). init-task 그래프는 런타임 부트라 대부분 골든/스모크가 커버.

---

## Task 8: 런타임 육안 검증 (사용자 게이트)

**Files:** (없음 — 검증만)

- [ ] **Step 1: 정상 실행 + 사이클 abort 부재 확인**

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/build_ninja/apps/_MyApp_
./_MyApp_
```
확인 항목:
- 로그에 `[InitScheduler] 의존 사이클` **없음** (abort 없이 부팅).
- Title 화면에 **orbital 배경 VFX 표시**(World 이관 후에도 유지).
- **F1** → PassDebug 패널 표시 + `uGrayscaleAmount = ... (HP-driven)` **readout 정상**(grayscale 머티리얼 조회 성공 = DebugUi 가 RenderPipeline 뒤 실행 증거).
- 스카이박스/월드/PostFX 화면 정상.

---

## Task 9: 커밋 (path-scoped)

**Files:** (없음 — 커밋만)

- [ ] **Step 1: path-scoped add + 커밋** (⚠ `git add -A` 금지 — 사용자 병렬 staging 보호)

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
git add apps/_MyApp_/main.cpp apps/_MyApp_/src/Bootstrap/InitTaskId.h
git commit -m "[refactor] : ScreenPipeline+Pass init-task 병합 + VFX 로드를 World 로 이관 (사이클 제거)"
```
Expected: 2 파일 커밋. (Co-Authored-By 미사용, 한국어 메시지.)

> 커밋은 사용자 게이트일 수 있음 — 미커밋 상태로 두고 승인 확인 권장.

---

## Self-Review

**1. Spec coverage:**
- 사이클 제거(VFX→World 이관) → Task 2. ✓
- ScreenPipeline+Pass 인라인 병합 → Task 3. ✓
- VfxUi VFX 의존 제거(순수 UI) → Task 4. ✓
- enum/ToString 동기 → Task 1. ✓
- 무회귀(골든/ctest/육안) → Task 6/7/8. ✓

**2. Placeholder scan:** 모든 코드 스텝에 실제 코드 블록 포함. "적절히 처리" 류 없음. ✓

**3. Type/식별자 일관성:**
- enum: `Core/World/RenderPipeline/DebugUi/Enter/Fsm` — Task 1(선언), Task 2(`World` no-dep), Task 3(`RenderPipeline` Needs World), Task 4(`DebugUi` Needs RenderPipeline) 전부 일치. ✓
- 멤버(`mScreenQuadMeshPtr/mPresentMatPtr/mPostFXFBs/mGrayscaleMatPtr/mScreenCamera/mPassIterator/mWorldPassPtr/mPresentPassPtr/mSkyboxRenderer/mCamera`) — 병합 task 가 모두 기존 선언 사용(신규 없음). `mVfxLayer` 만 제거. ✓
- `DebugUi.Needs({RenderPipeline})` 로 grayscale 머티리얼 선존 보장(PassDebugLayer 생성자 조회) — 논리 일관. ✓

**의존 그래프 최종:** `Core`(p1) → `World`(no dep) → `RenderPipeline`(Needs World) → `DebugUi`(Needs RenderPipeline) → `Enter`(p3) → `Fsm`(Needs Enter). 선형, acyclic. ✓

---

## Change log
- 2026-07-11: 최초 작성. 사용자 확정 = (1)VFX 로드 World 이관으로 사이클 절단, (2)main.cpp 인라인 병합, (3)네이밍 RenderPipeline/DebugUi, (4)죽은 VfxSpawnLayer 사용처 제거.
