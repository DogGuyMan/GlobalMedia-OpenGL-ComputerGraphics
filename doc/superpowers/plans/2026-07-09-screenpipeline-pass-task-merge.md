# ScreenPipeline + Pass Init Task 병합 Implementation Plan

> ⚠️ **2026-07-26 정정 (원문 보존)** — 본 문서에 나오는 `SJH_GOLDEN_CAPTURE=1 ./_MyApp_` 실행과 `ctest --test-dir build_ninja -R golden` 은 *당시* 절차이며 현재는 **폐기**됐다. 골든 캡처가 런타임 환경 변수 -> 컴파일 정의로 바뀌어 프리셋 `ninja-golden`(빌드 디렉토리 `build_ninja-golden`) 전유가 됐고, 게임 빌드의 `_MyApp_` 는 골든을 캡처하지 않는다(실행해도 창만 뜨는 조용한 실패). 현행 절차 = `test/CLAUDE.md`. 아래 본문은 당시 기록으로 그대로 둔다.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> ⚠ 이 plan 파일이 있는 `doc/` 는 **gitignored (로컬 전용)** — 커밋되지 않는다. 코드 변경만 커밋 대상.

**Goal:** `_MyApp_` 부트의 phase2 InitTask 중 `ScreenPipeline`(화면공간 자원 생성)과 `Pass`(패스 컬렉션 조립)를 하나의 `ScreenPipeline` 태스크로 병합하고, 사이클 원인인 죽은 의존 간선을 제거한다.

**Architecture:** 두 태스크는 둘 다 화면공간 렌더 파이프라인을 세팅하지만, 데이터 의존 때문에 분리돼 있었다(자원 생성은 의존 0, 패스 조립은 World의 카메라 필요). 병합하면 `ScreenPipeline → World → VfxUi → ScreenPipeline` 사이클이 생기는데, 그 마지막 간선(`VfxUi → ScreenPipeline`)이 **죽은 간선**(과거 소비자였던 PostFX 디버그 엔트리가 이미 삭제됨)이라 제거하면 무순환이 된다. 코드는 `main.cpp`에 인라인 유지(빌더 추출 안 함).

**Tech Stack:** C++17, CMake/Ninja, `SJH::engine`, InitScheduler (Kahn topo-sort), 골든 이미지 characterization test (ctest + OpenCV, bit-identical).

---

## 배경 (제로컨텍스트 대비 — 실행 에이전트 필독)

`apps/_MyApp_/main.cpp` 의 `OnSceneSetup()` (phase2) 은 `Bootstrap::InitScheduler` 로 4개 태스크를 선언적으로 등록하고 위상정렬 직렬 실행한다. 각 태스크는 `sched.Task(EInitTask::X).Needs({...}).Gl().Does([&]{ ...; return true; })` 형태의 fluent 등록이다.

현재 phase2 의존 그래프 (`X.Needs({Y})` = Y 먼저):

```
Core (phase1, 별도 hook)
phase2:
  ScreenPipeline  .Needs() 없음          = 자원 생성(screen quad/postfx FBO·머티리얼/screen camera)
  VfxUi           .Needs({ScreenPipeline}) ← ★ 죽은 간선 (VfxUi 는 ScreenPipeline 자원을 안 읽음)
  World           .Needs({VfxUi})          = WorldScene/스테이지/플레이어/웨이브. mCamera·mSkyboxRenderer 설정.
  Pass            .Needs({ScreenPipeline, World}) = PassIterator 에 Skybox/World/Particle/PostFX/present/ImGui 패스 Add
```

병합 후 목표 그래프 (무순환):

```
Core (phase1)
phase2:  VfxUi ──▶ World ──▶ ScreenPipeline(자원+패스)
phase3:  Enter ──▶ Fsm
```

**왜 죽은 간선인가 (검증됨):** `VfxUi` 태스크 본문(현 main.cpp:291-325)은 `reg.CreateEffect`(VFX 매니저), `UI::BuildGameUI`, `VfxSpawnLayer`, `PassDebugLayer(&mPassIterator)` 만 한다. `ScreenPipeline` 이 만드는 자원(`mScreenQuadMeshPtr`/`mPostFXFBs`/`mPresentMatPtr`/`mGrayscaleMatPtr`/`mScreenCamera`) 을 **하나도 읽지 않는다**. 과거 이 의존의 유일한 이유였던 PostFX 디버그 엔트리 코드는 이미 삭제됨(main.cpp:306-309 `// ! 제거 대상` 주석 흔적). `PassDebugLayer(&mPassIterator)` 는 주소만 저장하고 매 프레임 lazy 조회라 init 시점 빈 PassIterator 여도 무해.

**왜 자원 생성을 World 뒤로 밀어도 안전한가:** 화면공간 자원(quad/postfx FBO·머티리얼/screen camera)은 `World` 산출물에 데이터 의존이 없다. 패스 조립부만 `mCamera`/`mSkyboxRenderer`(World 산출)를 참조하는데, 병합 후 World 가 선행하므로 유효하다. 모든 init 은 첫 `render()` 전에 끝나므로 태스크 실행 순서 재배열은 최종 프레임에 영향이 없다 → 골든 bit-identical 유지가 안전망.

## 파일 변경 지도

| 파일 | 변경 | 책임 |
|---|---|---|
| `apps/_MyApp_/main.cpp` | Task 2: VfxUi `.Needs` 제거 / Task 3: ScreenPipeline 람다에 Pass 본문 흡수 + `.Needs({World})`, Pass 태스크 삭제 | phase2 부트 시퀀스 |
| `apps/_MyApp_/src/Bootstrap/InitTaskId.h` | Task 3: `Pass` enum 항목 + `ToString` case 제거, `ScreenPipeline` 주석 갱신 | InitTask 식별자 카탈로그 |

**변경 안 하는 것:** `InitScheduler.{h,cpp}`(제네릭 `EInitTask` 사용, 특정 값 비의존), `WorldSceneBuilder.*`, `UiBootstrap.*`, 셰이더, 골든 레퍼런스 PNG, `test/`.

## 규약 & 가드레일 (프로젝트 — 반드시 준수)

- **신규 단위 테스트 작성 금지** (no_auto_tests 규약). 이건 characterization 리팩토링 — 안전망은 기존 골든+ctest. TDD red-green 을 강제하지 말 것.
- **커밋 path-scoped**: `git add <명시 파일>` 만. **`git add -A` 절대 금지** (사용자가 같은 트리에서 병렬 staging 중일 수 있음).
- **커밋 메시지 한국어**, 형식 `[refactor] : ...`. **Co-Authored-By 미사용**.
- **주석 한국어**, 특수문자 자제 (Doxygen+ASCII 컨벤션).
- **브랜치**: 현재 `refactor/pcb-to-worldscene` 에서 작업 (worktree 불필요, 이미 feature 브랜치).
- **clangd IDE 의 missing/unused-include 진단은 알려진 거짓양성** — 빌드/ctest 가 진실.

## 검증 명령 (모든 태스크 공통)

```bash
# 1) 빌드 (sb7 'gl.h and gl3.h' 경고 1개는 정상)
cmake --build --preset ninja --target _MyApp_
# 기대: 링크까지 성공, exit 0

# 2) ctest -- 유닛 + 골든 캡처(_MyApp_ 재실행) + 골든 비교(14장 bit-identical, 임계 0)
ctest --test-dir build_ninja --output-on-failure
# 기대: "100% tests passed", 실패 0
```

`ctest` 가 골든 관련 테스트를 못 찾으면 `build_ninja` 가 테스트 미구성 상태다. 그 경우 한 번만:
```bash
cmake --preset ninja -DENABLE_TESTING=ON && cmake --build --preset ninja --target _MyApp_ golden_compare
```
그 후 다시 위 검증. **골든 비교 FAIL 시 임계(kMaxPixelFraction/kChannelDiffThreshold)를 절대 올리지 말 것** — 그건 리팩토링이 픽셀을 바꿨다는 진짜 신호이므로 원인을 조사한다.

---

## Task 1: 베이스라인 그린 확인 (변경 전)

리팩토링 전 현재 상태가 그린인지 먼저 확정한다 (characterization 리팩토링의 기준선).

**Files:** (없음 — 검증만)

- [ ] **Step 1: 빌드**

```bash
cmake --build --preset ninja --target _MyApp_
```
Expected: exit 0 (링크 성공).

- [ ] **Step 2: ctest 로 현재 골든 그린 확인**

```bash
ctest --test-dir build_ninja --output-on-failure
```
Expected: `100% tests passed`. 특히 `golden_capture` + `golden_compare` 케이스(14장)가 전부 pass. 여기서 실패하면 리팩토링과 무관한 선행 문제이므로 멈추고 사용자에게 보고.

- [ ] **Step 3: git 상태 확인**

```bash
git status --short && git branch --show-current
```
Expected: 브랜치 `refactor/pcb-to-worldscene`. working tree 가 지저분하면(사용자 병렬 편집) 어느 파일이 남의 작업인지 확인 후 진행. 커밋은 반드시 명시 경로만.

---

## Task 2: 죽은 간선 제거 (VfxUi → ScreenPipeline)

병합 선행 단계. 이 간선을 먼저 끊어야 Task 3 의 병합이 사이클을 만들지 않는다. 이 변경만으로도 위상정렬 tiebreak(enum 값 순)에 의해 실행 순서가 `VfxUi → World → ScreenPipeline → Pass` 로 이미 재배열된다 — 즉 "자원 생성이 World 뒤로 밀리는" 재배열이 여기서 먼저 일어나므로, 이 태스크의 골든이 통과하면 재배열 안전성이 병합 전에 입증된다.

**Files:**
- Modify: `apps/_MyApp_/main.cpp:291`

- [ ] **Step 1: VfxUi 태스크의 `.Needs({ScreenPipeline})` 제거**

`apps/_MyApp_/main.cpp` 에서 아래 줄(현 291):
```cpp
			sched.Task(Bootstrap::EInitTask::VfxUi).Needs({Bootstrap::EInitTask::ScreenPipeline}).Gl().Does([&] {
```
을 다음으로 교체:
```cpp
			sched.Task(Bootstrap::EInitTask::VfxUi).Gl().Does([&] {
```

(주의: `.Needs({...})` 만 지우고 `.Gl().Does([&] {` 는 그대로 둔다. VfxUi 는 phase2 에서 이제 의존 0 이 되어 가장 먼저 실행된다 — World 가 VfxUi 를 여전히 `.Needs` 하므로 "VfxUi 가 World 보다 먼저" 계약은 유지된다.)

- [ ] **Step 2: 빌드**

```bash
cmake --build --preset ninja --target _MyApp_
```
Expected: exit 0.

- [ ] **Step 3: ctest (골든 bit-identical 재확인)**

```bash
ctest --test-dir build_ninja --output-on-failure
```
Expected: `100% tests passed`. 골든 14장 여전히 bit-identical. (실행 순서가 재배열됐지만 최종 프레임 동일.) FAIL 시 재배열이 픽셀에 영향을 준 것 — 멈추고 조사(임계 올리지 말 것).

- [ ] **Step 4: 커밋 (path-scoped)**

```bash
git add apps/_MyApp_/main.cpp
git commit -m "[refactor] : VfxUi 의 죽은 ScreenPipeline 의존 간선 제거 (T2+T4 병합 선행)"
```

---

## Task 3: ScreenPipeline + Pass 태스크 병합

Pass 태스크 본문을 ScreenPipeline 람다 끝(자원 생성 뒤, `return true;` 앞)으로 옮기고, ScreenPipeline 에 `.Needs({World})` 를 추가하며, Pass 태스크와 enum 항목을 삭제한다.

**Files:**
- Modify: `apps/_MyApp_/main.cpp:148` (ScreenPipeline `.Needs` 추가)
- Modify: `apps/_MyApp_/main.cpp:209-213` (ScreenPipeline 람다 끝에 Pass 본문 삽입)
- Modify: `apps/_MyApp_/main.cpp:247-288` (Pass 태스크 전체 삭제)
- Modify: `apps/_MyApp_/src/Bootstrap/InitTaskId.h:27-28,42` (enum + ToString)

- [ ] **Step 1: ScreenPipeline 태스크에 World 의존 추가**

`apps/_MyApp_/main.cpp` 현 148 줄:
```cpp
			sched.Task(Bootstrap::EInitTask::ScreenPipeline).Gl().Does([&] {
```
을 다음으로 교체:
```cpp
			// T2 screenPipeline -- 화면공간 자원(screen quad/postfx 머티리얼·FBO/present/screen camera)
			//   생성 + Pass 컬렉션 조립까지 흡수. 패스 조립부가 World 의 mCamera/mSkyboxRenderer 를 참조하므로 World 의존.
			sched.Task(Bootstrap::EInitTask::ScreenPipeline).Needs({Bootstrap::EInitTask::World}).Gl().Does([&] {
```

(주의: 바로 위 현 147 줄의 기존 주석 `// T2 screenPipeline -- DefaultPipeline + ScreenCamera + PostFX 체인 + fog/vignette + 레지스트리.` 는 위 새 주석으로 대체되도록, 그 한 줄을 지우고 교체한다. 즉 기존 주석 1줄 + 코드 1줄 → 새 주석 2줄 + 코드 1줄.)

- [ ] **Step 2: ScreenPipeline 람다 끝에 Pass 조립 본문 삽입**

`apps/_MyApp_/main.cpp` 의 ScreenPipeline 람다 말미(현 209-214)는 이렇게 끝난다:
```cpp
				// 화면 카메라 (resize aspect 추적용 - present 자체는 PostFxPass 가 backbuffer 직접 bind).
				auto screenCamActor = SJH::Scene::CreateScreenCameraActor(ACTOR_SCREEN_CAMERA, mFbInfo.Aspect, mSceneFB.get());
				mScreenCamera = screenCamActor->GetComponent<SJH::Scene::Camera>();
				dir.Root().AddChild(std::move(screenCamActor));

				return true;
			});
```
이 `dir.Root().AddChild(std::move(screenCamActor));` 와 `return true;` 사이에, Pass 태스크 본문(현 250-286)을 삽입해 다음과 같이 만든다:
```cpp
				// 화면 카메라 (resize aspect 추적용 - present 자체는 PostFxPass 가 backbuffer 직접 bind).
				auto screenCamActor = SJH::Scene::CreateScreenCameraActor(ACTOR_SCREEN_CAMERA, mFbInfo.Aspect, mSceneFB.get());
				mScreenCamera = screenCamActor->GetComponent<SJH::Scene::Camera>();
				dir.Root().AddChild(std::move(screenCamActor));

				// -- Pass 컬렉션 조립 (구 T4 Pass 태스크 흡수) --
				//   PassIterator 가 모든 Pass 를 소유. 코스 순서대로 Add(std::move).
				//   [Skybox -> World -> Particle -> PostFX(e0..eN) -> present -> ImGui]. 비소유 관찰 포인터는 move 전에 캡처.
				// 1) Skybox - background-first (sceneFB clear+skybox 책임).
				mPassIterator.Add(std::make_unique<SJH::SkyboxPass>(mSkyboxRenderer, mCamera));

				// 2) World - skybox 위에 그림. SetClearsTarget(false) 로 clear 스킵. SetActivePrograms 주입 위해 관찰 포인터 캡처.
				auto worldPass = std::make_unique<SJH::WorldPass>(mCamera);
				mWorldPassPtr = &worldPass->SetClearsTarget(false); // move 전 캡처 (소유=PassIterator, 수명=app 동안 안정).
				mPassIterator.Add(std::move(worldPass));

				// 3) Particle - sceneFB 에 Effekseer 합성 (worldCam RT).
				mPassIterator.Add(std::make_unique<TopdownShooter::VFX::ParticlePass>(&GameSystems::Get().VFX(), mCamera));

				// 4) PostFX 효과 체인 - 각 효과는 중간 FBO 에 그림(output=fbo). before=직전 활성 결과(PassIterator lastResult).
				//    config 순서 = 체인 순서. invert/blurring/sobel 은 기본 비활성(Enabled=false, 동적 skip).
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

(핵심: Pass 조립부는 `mCamera`/`mSkyboxRenderer`(World 산출, 이제 선행 완료) + 자원 생성부가 만든 `mPostFXFBs`/`mPresentMatPtr`/`mScreenQuadMeshPtr`(같은 람다 앞부분) 를 참조한다. 순서 = 자원 먼저, 패스 조립 나중이 반드시 유지되어야 하며 위 삽입 위치가 이를 보장한다. 구 T4 의 마지막 `return true;` 는 버리고 ScreenPipeline 의 `return true;` 하나로 통합.)

- [ ] **Step 3: 구 Pass 태스크 전체 삭제**

`apps/_MyApp_/main.cpp` 에서 Pass 태스크 등록 블록 전체(현 247-288, 아래 범위)를 삭제한다:
```cpp
			// T4 stages -- PassIterator 가 모든 Pass 를 소유. 코스 순서대로 Add(std::move).
			//   [Skybox -> World -> Particle -> Grayscale(present) -> ImGui]. 비소유 관찰 포인터는 move 전에 캡처.
			sched.Task(Bootstrap::EInitTask::Pass).Needs({Bootstrap::EInitTask::ScreenPipeline, Bootstrap::EInitTask::World}).Cpu().Does([&] {
				// 1) Skybox - background-first (sceneFB clear+skybox 책임).
				mPassIterator.Add(std::make_unique<SJH::SkyboxPass>(mSkyboxRenderer, mCamera));
				... (중략: 위 Step 2 에서 옮긴 바로 그 본문) ...
				// 6) ImGui - 종단 Pass (backbuffer 위에 UI).
				mPassIterator.Add(std::make_unique<UI::ImGuiPass>());
				return true;
			});
```
삭제 후, 그 아래 `// T5 vfxUi ...` 태스크(현 290~) 가 ScreenPipeline 태스크(및 그 안에 흡수된 Pass 본문) 바로 다음에 오게 된다. `sched.RunAll();` 호출은 그대로 둔다.

(주의: Step 2 에서 이미 이 본문을 ScreenPipeline 으로 복사했으므로, 여기서는 원본 Pass 태스크 블록을 통째로 지운다. 중복이 남지 않도록 `sched.Task(Bootstrap::EInitTask::Pass)` 로 시작하는 등록이 파일에서 완전히 사라졌는지 확인.)

- [ ] **Step 4: InitTaskId.h — Pass enum 항목 삭제 + ScreenPipeline 주석 갱신**

`apps/_MyApp_/src/Bootstrap/InitTaskId.h` 의 enum(현 22-31):
```cpp
	enum class EInitTask
	{
		Core,            ///< phase1: 렌더타깃 + 시스템 init + 오디오 워밍업.
		VfxUi,           ///< phase2: VFX 이펙트(orbital 포함) 로드 + 게임 UI. World 보다 먼저.
		World,           ///< phase2: WorldScene + 스테이지 액터(orbital FindEffect) + 플레이어.
		ScreenPipeline,  ///< phase2: DefaultPipeline + ScreenCamera + PostFX.
		Pass,          	 ///< phase2: Pass 컬렉션 조립.
		Enter,           ///< phase3: Director.Enter.
		Fsm,             ///< phase3: GameContext + Stage FSM.
	};
```
을 다음으로 교체(`Pass` 줄 삭제 + `ScreenPipeline` 주석 갱신):
```cpp
	enum class EInitTask
	{
		Core,            ///< phase1: 렌더타깃 + 시스템 init + 오디오 워밍업.
		VfxUi,           ///< phase2: VFX 이펙트(orbital 포함) 로드 + 게임 UI. World 보다 먼저.
		World,           ///< phase2: WorldScene + 스테이지 액터(orbital FindEffect) + 플레이어.
		ScreenPipeline,  ///< phase2: 화면공간 자원(ScreenCamera/PostFX FBO/머티리얼) + Pass 컬렉션 조립. World 의존.
		Enter,           ///< phase3: Director.Enter.
		Fsm,             ///< phase3: GameContext + Stage FSM.
	};
```

- [ ] **Step 5: InitTaskId.h — ToString 의 Pass case 삭제**

같은 파일 `ToString`(현 34-47) 의 아래 줄:
```cpp
		case EInitTask::Pass:         return "Pass";
```
을 **삭제**한다 (다른 case 는 그대로). 삭제 후:
```cpp
		switch (id)
		{
		case EInitTask::Core:           return "Core";
		case EInitTask::ScreenPipeline: return "ScreenPipeline";
		case EInitTask::VfxUi:          return "VfxUi";
		case EInitTask::World:          return "World";
		case EInitTask::Enter:          return "Enter";
		case EInitTask::Fsm:            return "Fsm";
		}
```

(`Pass` 삭제로 `Enter`/`Fsm` 의 enum 정수값이 하나씩 줄지만, 값은 tiebreak 우선순위 + ToString 매핑에만 쓰이고 어디에도 직렬화되지 않으므로 안전. 상대 순서 `Core<VfxUi<World<ScreenPipeline<Enter<Fsm` 보존.)

- [ ] **Step 6: 빌드**

```bash
cmake --build --preset ninja --target _MyApp_
```
Expected: exit 0. `EInitTask::Pass` 참조가 남아 있으면 컴파일 에러로 즉시 노출된다(main.cpp 에서 전부 제거됐어야 함).

- [ ] **Step 7: ctest (골든 bit-identical 최종 확인)**

```bash
ctest --test-dir build_ninja --output-on-failure
```
Expected: `100% tests passed`. 골든 14장 bit-identical. FAIL 시 병합이 픽셀을 바꾼 것 — 멈추고 조사(임계 올리지 말 것).

- [ ] **Step 8: 런타임 부트 로그 확인 (사이클 없음 + 실행 순서)**

InitScheduler 는 사이클을 하드에러(abort)로 잡으므로, ctest 의 `golden_capture` 가 `_MyApp_` 를 정상 실행해 골든을 뽑았다는 것 자체가 "무순환 + 정상 부트" 의 증거다. 추가 육안 확인을 원하면:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
Expected: 창이 뜨고 Title 화면 정상 표시(abort/크래시 없음). 확인 후 창 닫기. (cwd 필수 — 리소스 상대경로.)

- [ ] **Step 9: 커밋 (path-scoped)**

```bash
git add apps/_MyApp_/main.cpp apps/_MyApp_/src/Bootstrap/InitTaskId.h
git commit -m "[refactor] : ScreenPipeline+Pass init task 병합 (화면공간 자원+패스 조립 일원화)"
```

---

## 완료 조건

- `EInitTask::Pass` 가 코드베이스에서 완전히 사라짐 (`grep -rn "EInitTask::Pass" apps/` → 0건).
- phase2 태스크 3개(VfxUi/World/ScreenPipeline) + phase1 Core + phase3 Enter/Fsm 만 존재.
- 빌드 exit 0, ctest `100% tests passed`, 골든 14장 bit-identical.
- 커밋 2개(Task 2, Task 3) — path-scoped, `[refactor] :` 형식, Co-Authored-By 없음.

## 롤백

두 커밋 모두 순수 리팩토링이라 `git revert <sha>` 로 안전 복원. 골든이 깨지면 되돌리고 원인 조사.
