# Stage FSM 구현 — 에이전트 프롬프트 (2026-06-03)

> **사용법**: 아래 코드블록 전체를 복사해 새 Claude Code 세션에 첫 메시지로 붙여넣기.
> `CLAUDE.md` + `MEMORY.md` 는 자동 로드되므로 이 프롬프트는 그 위에 추가되는 컨텍스트.

---

```
[ROLE]
너는 C++17 게임 엔진 프로젝트 `_MyApp_` 의 Stage FSM 구현 에이전트다.
워킹 디렉토리: /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
브랜치: game/module/ingame/temp
목표: Stage State Machine (Title/CombatPlay/Pause/GameOver) 완성 — 현재 컴파일 불가 스텁 3개를 동작하는 코드로 교체.

[절대 규칙 / Hard rules]
- 커밋은 사용자 명시 요청 시만. git add <경로> 경로 스코프 필수. git add -A 절대 금지.
- 단위 테스트 자동 추가 금지 (no_auto_tests memory).
- Actor 비상속 — 모든 게임 오브젝트는 compound_actor factory 패턴.
- 헤더 가드: __TOPDOWNSHOOTER_STAGE_XXX_H__ 형식. #pragma once 사용 금지.
- 주석 언어: 한국어.
- ImGui v1.53 핀 — v1.54+ API 사용 금지.
- src/fsm/state_machine.h 절대 수정 금지 (엔진 코어).
- 다음 파일 건드리지 말 것: Entity/Player/PlayerHand.*, InputHandler/PlayerController.*, Entity/Constants.h (다른 미커밋 변경 존재).

[검증된 사실 / Verified facts — 실측 2026-06-03]

현재 브랜치: game/module/ingame/temp
최신 커밋: efdd72c [dev] : 타이머 통합 및 VFX 정합 일체화

현재 컴파일 불가 파일 3개 (Stage namespace를 TOwner로 사용 중):
- apps/_MyApp_/src/Stage/State/StageStateMachine.h:12
  → StateMachine<EStageStatus, Stage> — Stage는 namespace라 컴파일 불가
- apps/_MyApp_/src/Stage/State/StageFSMState.h:9
  → IFsmState<Stage> — 동일 문제
- apps/_MyApp_/src/Stage/State/StageState.Impl.h
  → 현재 1줄 빈 파일

FSM 엔진 핵심 제약 (src/fsm/state_machine.h 실측):
- TryTransitImpl: if (curr==0) return false  ← NONE=0에서는 전이 불가 (I1 불변식)
- StateMachine 생성자: StateMachine(TOwner& owner, TState startup = TState::NONE)
- ForceTransit 실패 시 std::abort() 호출

현재 EStageStatus (Stage.h 실측):
  Title=1<<0, Combat=1<<1, Boss=1<<2  ← NONE 없음, CombatPlay/Pause/GameOver 없음

main.cpp 실측 중요 줄:
- 84: void startup() override — 리소스 로딩 전체
- 217: waveSpawner->AddComponent<Stage::WaveController>(...)
- 260: ImGui_ImplGlfwGL3_NewFrame()
- 264: Manager::Get().Audio().SetListener(...)
- 265: Manager::Get().Update(dt)
- 266: SJH::Scene::Director::Get().Update(dt)
- 268: Physics().SyncToTransform(...)

UI 패턴 실측 (ExitButtonLayer.h):
- IImGuiLayer::Enabled = false → RenderAll()에서 skip
- ImGuiLayerStack::RenderAll()은 PostFx 이후 render()에서 호출 → ImGUI는 Blur 위에 자동 렌더

[STEP 1] Stage.h — EStageStatus 확장
파일: apps/_MyApp_/src/Stage/Stage.h
현재 내용 전체를 다음으로 교체:

#ifndef __TOPDOWNSHOOTER_STAGE_H__
#define __TOPDOWNSHOOTER_STAGE_H__
#include <cstdint>
namespace TopdownShooter::Stage
{
    enum class EStageStatus : uint64_t
    {
        NONE       = 0,           // FSM 엔진 sentinel — 전이 불가
        Title      = 1ull << 0,
        CombatPlay = 1ull << 1,
        Boss       = 1ull << 2,   // 미래 미사용
        Pause      = 1ull << 3,
        GameOver   = 1ull << 4,
        Boot       = 1ull << 5,   // 유효 시작 State. ForceTransit(Title) 즉시
    };
}
#endif // __TOPDOWNSHOOTER_STAGE_H__

[STEP 2] StageFSMState.h — TOwner 수정
파일: apps/_MyApp_/src/Stage/State/StageFSMState.h
전체 교체:

#ifndef __TOPDOWNSHOOTER_STAGE_FSM_H__
#define __TOPDOWNSHOOTER_STAGE_FSM_H__
#include "fsm/fsm_state.h"
#include "Stage/Stage.h"
#include "scene/actor.h"
#include <cstdint>
namespace TopdownShooter::Stage
{
    class StageStateMachine;
    class BaseStageFsmState : public SJH::FSM::IFsmState<SJH::Scene::Actor>
    {
    protected:
        EStageStatus       mStateFlag;
        EStageStatus       mTransitFlag;
        StageStateMachine* mFsm = nullptr;
    public:
        BaseStageFsmState(StageStateMachine* fsm, EStageStatus stateFlag, EStageStatus transitFlag)
            : mFsm(fsm), mStateFlag(stateFlag), mTransitFlag(transitFlag) {}
        uint64_t GetStateFlag()   const override { return static_cast<uint64_t>(mStateFlag);   }
        uint64_t GetTransitFlag() const override { return static_cast<uint64_t>(mTransitFlag); }
    };
}
#endif // __TOPDOWNSHOOTER_STAGE_FSM_H__

[STEP 3] StageStateMachine.h — TOwner 수정
파일: apps/_MyApp_/src/Stage/State/StageStateMachine.h
전체 교체:

#ifndef __TOPDOWNSHOOTER_STAGE_STATEMACHINE_H__
#define __TOPDOWNSHOOTER_STAGE_STATEMACHINE_H__
#include "Stage/Stage.h"
#include "Stage/State/StageFSMState.h"
#include "fsm/state_machine.h"
#include "scene/actor.h"
namespace TopdownShooter::Stage
{
    class StageStateMachine : public SJH::FSM::StateMachine<EStageStatus, SJH::Scene::Actor>
    {
    public:
        explicit StageStateMachine(SJH::Scene::Actor& root)
            : SJH::FSM::StateMachine<EStageStatus, SJH::Scene::Actor>(root, EStageStatus::Boot) {}
    };
}
#endif // __TOPDOWNSHOOTER_STAGE_STATEMACHINE_H__

[STEP 4] StateOverlayLayer.h 신설
파일: apps/_MyApp_/src/UI/StateOverlayLayer.h (신규 생성):

#ifndef __MYAPP_UI_STATE_OVERLAY_LAYER_H__
#define __MYAPP_UI_STATE_OVERLAY_LAYER_H__
#include "UI/IImGuiLayer.h"
#include "resource_registry/texture.h"
#include <imgui.h>
namespace TopdownShooter::UI
{
    class StateOverlayLayer : public IImGuiLayer
    {
    public:
        ImGuiLayerKind GetKind() const override { return ImGuiLayerKind::Game; }
        void Show(const SJH::Texture* tex) { Enabled = true;  mTex = tex; }
        void Hide()                        { Enabled = false; }
        void OnBuildUI() override
        {
            const ImVec2 sz = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(sz);
            ImGui::PushStyleColor(ImGuiCol_WindowBg,     ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Border,       ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0,0,0,0));
            ImGui::Begin("##state_overlay", nullptr,
                ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoScrollbar);
            if (mTex)
                ImGui::Image((ImTextureID)(intptr_t)mTex->GetTextureID(), sz);
            ImGui::End();
            ImGui::PopStyleColor(3);
        }
    private:
        const SJH::Texture* mTex = nullptr;
    };
}
#endif // __MYAPP_UI_STATE_OVERLAY_LAYER_H__

[STEP 5] GameContextComponent.h 신설
파일: apps/_MyApp_/src/Stage/Components/GameContextComponent.h (신규 생성):

#ifndef __TOPDOWNSHOOTER_STAGE_COMPONENTS_GAME_CONTEXT_H__
#define __TOPDOWNSHOOTER_STAGE_COMPONENTS_GAME_CONTEXT_H__
#include "scene/actor.h"
#include "resource_registry/texture.h"
#include "UI/StateOverlayLayer.h"
#include "InputHandler/PlayerController.h"
#include "input/input.h"
class GLFWwindow;
namespace TopdownShooter::Stage { class WaveController; class StageStateMachine; }
namespace TopdownShooter::Stage::Components
{
    class GameContextComponent : public SJH::Scene::Component
    {
    public:
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float) override {}

        UI::StateOverlayLayer*  overlay      = nullptr;
        const SJH::Texture*    titleTex     = nullptr;
        const SJH::Texture*    pauseTex     = nullptr;
        const SJH::Texture*    gameOverTex  = nullptr;
        SJH::KeyboardInput<Controller::PlayerController::Action>* keyboard = nullptr;
        SJH::MouseInput*        mouse        = nullptr;
        GLFWwindow*             window       = nullptr;
        Stage::WaveController*  waveCtrl     = nullptr;
        SJH::Scene::Actor*      playerActor  = nullptr;
    };
}
#endif // __TOPDOWNSHOOTER_STAGE_COMPONENTS_GAME_CONTEXT_H__

[STEP 6] StageState.Impl.h — 5종 State 구현
파일: apps/_MyApp_/src/Stage/State/StageState.Impl.h
전체 교체 (현재 1줄 빈 파일).
Resume handoff 문서(doc/handoffs/2026-06-03/2026-06-03-stage-fsm-resume-handoff.md §4 S6)의 완전한
코드를 그대로 사용. 주요 패턴:
- BootState: OnEnter에서 mFsm->ForceTransit(EStageStatus::Title) 즉시 호출
- TitleState: OnEnter에서 리소스 로딩 + overlay->Show(titleTex)
  ※ 리소스 로딩은 현재 main.cpp startup() 84번째 줄 이후의 코드를 이전 — GetCtx(root)->window 등 활용
- CombatPlayState: OnUpdate에서 Manager::Update + Director::Update + Physics::Sync 호출
  ESC → TryTransit(Pause), playerActor IsActive==false → ForceTransit(GameOver)
- PauseState: OnEnter overlay->Show(pauseTex), OnUpdate ESC → ForceTransit(CombatPlay)
- GameOverState: OnEnter overlay->Show(gameOverTex), terminal (no transitions)

[STEP 7] main.cpp startup() 재구성
현재 startup() (line 84~약 230)의 모든 리소스 로딩을 TitleState::OnEnter로 이전.
startup() 최소화 (도달 목표):
1. dir.Enter()
2. "GameContext" Actor 생성 + GameContextComponent 부착 + refs 설정
3. StateOverlayLayer 생성 + mImGuiStack.Push + ctx->overlay 저장
4. 오버레이용 텍스처 3종 로드 (Title/Pause/GameOver.png) + ctx에 저장
5. StageStateMachine 생성 + 5종 State 등록 + mStageFsm->OnEnter() 호출
   → BootState→TitleState 자동 전이 → TitleState::OnEnter가 리소스 로딩 실행

game_application 멤버 추가:
  std::unique_ptr<Stage::StageStateMachine>     mStageFsm;
  Stage::Components::GameContextComponent*      mCtxComp = nullptr;

[STEP 8] main.cpp render() 슬림화
render()에서 제거 (CombatPlayState::OnUpdate로 이전):
  line 264: Manager::Get().Audio().SetListener(...)
  line 265: Manager::Get().Update(dt)
  line 266: SJH::Scene::Director::Get().Update(dt)
  line 268: Physics().SyncToTransform(...)

render()에 추가 (ImGui_ImplGlfwGL3_NewFrame() 직후):
  mStageFsm->Update(realDt);

[스코프 경계 / Boundaries]
이 작업이 소유하는 파일:
  Stage/Stage.h, Stage/State/StageFSMState.h, Stage/State/StageStateMachine.h,
  Stage/State/StageState.Impl.h, Stage/Components/GameContextComponent.h (신설),
  UI/StateOverlayLayer.h (신설), apps/_MyApp_/main.cpp

절대 건드리지 말 것:
  src/fsm/state_machine.h  — 엔진 코어, NONE=0 가드 변경 금지
  Entity/Player/PlayerHand.cpp/h  — 다른 미커밋 변경 존재
  InputHandler/PlayerController.cpp/h  — 다른 미커밋 변경 존재
  Entity/Constants.h  — 다른 미커밋 변경 존재
  Audio/AudioSystem.*  — FMOD 에이전트 전담

[검증 / Verify]
빌드 명령:
  cmake --build --preset ninja --target _MyApp_

실행 (반드시 cd 필요):
  cd build_ninja/apps/_MyApp_ && ./_MyApp_

기대 결과:
  - 빌드 에러 0 (기존 스텁 컴파일 불가 → 해소)
  - 실행 시 Title.png 전체화면 표시
  - 클릭 → Title 사라짐 + 게임 시작
  - ESC → Pause.png 표시 + 게임 정지
  - ESC 재입력 → 게임 재개 (Wave 상태 보존)
  - Player 사망 → GameOver.png 표시

[Self-review]
보고 전 체크리스트:
□ Stage.h에 Boot=1<<5 포함, NONE=0 유지
□ BaseStageFsmState가 IFsmState<SJH::Scene::Actor>를 올바르게 상속
□ StageStateMachine 생성자가 startup=EStageStatus::Boot로 초기화
□ BootState::OnEnter에서 ForceTransit(Title) 호출
□ CombatPlayState::OnUpdate에서만 Manager::Update + Director::Update 호출
□ PauseState OnEnter가 WaveController를 리셋하지 않음
□ GameOverState GetTransitFlag() == 0 (terminal)
□ StateOverlayLayer Enabled 플래그 토글 방식 정확
□ main.cpp startup()에서 dir.Enter() 호출 시점 적절
□ 빌드 에러 0 확인

[보고 / Report]
완료 후 다음 형식으로 보고:
STATUS: DONE | DONE_WITH_CONCERNS | BLOCKED
변경된 파일 목록 (경로 정확히):
빌드 결과 (에러 수):
시각 검증 결과 (체크리스트 각 항목):
미처리/연기 항목:
커밋하지 않음 확인: YES

※ FMOD 연동([FMOD] 태그 항목)은 별도 에이전트 담당 — 구현하지 말 것.
   doc/design/2026-06-03-stage-fsm-fmod-handoff.md 참조.
```

---

## 사용 메모 (오케스트레이터용)

**이 프롬프트가 완료하는 범위**: Stage FSM 뼈대 + State 구현 + render() 슬림화 + startup() 이전  
**FMOD 연동은 별도**: `doc/design/2026-06-03-stage-fsm-fmod-handoff.md` 를 FMOD 에이전트에게 전달  
**권장 커밋 메시지** (에이전트 완료 후 사용자가 직접):
```
[feat] : Stage FSM 완성 — Boot→Title→CombatPlay→Pause/GameOver + StateOverlayLayer + startup 이전
```
