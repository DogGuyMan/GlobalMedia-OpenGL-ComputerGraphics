# Stage FSM 구현 — Resume Handoff (2026-06-03)

> **단일 진입점**: 이 문서. 이전 stage-fsm 관련 문서는 없음 (신규 작업).
> **다음 에이전트 첫 액션**: §4 구현 순서 S1부터 순서대로 실행.

---

## TL;DR + 다음 액션

**현재 위치**: Stage FSM 설계 완료, 구현 미시작. 스텁 파일 3개가 컴파일 불가 상태.

**즉시 해야 할 것**: `Stage.h` 의 `EStageStatus` 를 확장하는 것부터 시작 (S1). 이후 S2~S10 순서로.

---

## §1 세계 상태 (실측 — 2026-06-03)

**브랜치**: `game/module/ingame/temp`

**최근 커밋 (git log 실측)**:
```
efdd72c [dev] : 타이머 통합 및 VFX 정합 일체화
eb1b956 [refactor] : PostFX 설정 모듈화
da8d3ca [refactor] : WaveController spawn 간격 Timer 객체화
a63a560 [refactor] : PlayerController attack 윈도 Timer 핸들
128be8d [refactor] : Life optional<Timer> -> BaseEntity 중앙 핸들
```

**미커밋 변경 (git status 실측)**:
| 파일 | 상태 | 비고 |
|------|------|------|
| `apps/_MyApp_/main.cpp` | M | startup/render 변경 예정 대상 |
| `apps/_MyApp_/src/Entity/Player/PlayerHand.cpp/h` | M | 이 작업과 무관 — 건드리지 말것 |
| `apps/_MyApp_/src/InputHandler/PlayerController.cpp/h` | M | 이 작업과 무관 |
| `apps/_MyApp_/src/Entity/Constants.h` | M | 이 작업과 무관 |
| `doc/design/2026-06-03-stage-fsm-fmod-handoff.md` | ?? (untracked) | FMOD 에이전트용 |

**신설 예정 문서 (이미 작성됨, git untracked)**:
- `doc/superpowers/specs/2026-06-03-stage-fsm-design.md` — 정본 spec (v2, Q&A 반영 완료)
- `doc/design/2026-06-03-stage-fsm-fmod-handoff.md` — FMOD 에이전트 전용 handoff

---

## §2 Task 상태 표

| # | Task | 상태 | SHA |
|---|------|------|-----|
| S0 | Spec 문서 작성 + Q&A 반영 | ✅ 완료 | (untracked) |
| S1 | Stage.h — EStageStatus 확장 | 🔲 미시작 | — |
| S2 | StageFSMState.h 수정 | 🔲 미시작 | — |
| S3 | StageStateMachine.h 수정 | 🔲 미시작 | — |
| S4 | UI/StateOverlayLayer.h 신설 | 🔲 미시작 | — |
| S5 | Stage/Components/GameContextComponent.h 신설 | 🔲 미시작 | — |
| S6 | StageState.Impl.h — 5종 State 구현 (Boot+4종) | 🔲 미시작 | — |
| S7 | main.cpp startup() 최소화 + FSM 초기화 | 🔲 미시작 | — |
| S8 | main.cpp render() 슬림화 | 🔲 미시작 | — |
| S9 | UI/UiBootstrap 연결 | 🔲 미시작 | — |
| S10 | 빌드 + 시각 검증 | 🔲 미시작 | — |

---

## §3 확정 결정 (재협상 금지)

| 결정 | 내용 | 이유 |
|------|------|------|
| **TOwner** | `SJH::Scene::Actor` (Root Actor) | "StateMachine의 Owner는 Root Actor" — 사용자 확정 |
| **EStageStatus** | Boot=1<<5 추가. NONE=0 유지(엔진 sentinel). FSM 시작: Boot→Title ForceTransit | FSM I1 불변식(`if(curr==0) abort`) 우회용 Bridge State |
| **FSM 역참조** | State 생성자에 `StageStateMachine*` 주입 | `IFsmState<TOwner>` 에 GetFsm() 없음 |
| **OnEnter 순수성** | CombatPlayState::OnEnter = UI/Audio만. WaveController 리셋 없음 | Pause Resume 시 wave 상태 보존 필수 |
| **startup 이전** | 리소스 로딩 전체를 TitleState::OnEnter로 이전. startup()은 GL+FSM 초기화만 | 사용자 "이번 sprint 포함 — 완전 이전" 확정 |
| **DDD 리팩토링** | GameContextComponent God Object 분리는 **이번 sprint 외** — 별도 작업 | 안정화 우선 |

---

## §4 구현 상세 (S1~S9)

### S1 — `apps/_MyApp_/src/Stage/Stage.h`

현재: `Title=1<<0, Combat=1<<1, Boss=1<<2` (NONE 없음)  
변경 후:
```cpp
enum class EStageStatus : uint64_t {
    NONE       = 0,           // FSM 엔진 sentinel — 전이 불가
    Title      = 1ull << 0,
    CombatPlay = 1ull << 1,   // Combat 에서 rename
    Boss       = 1ull << 2,
    Pause      = 1ull << 3,
    GameOver   = 1ull << 4,
    Boot       = 1ull << 5,   // 유효 시작 State. ForceTransit(Title) 즉시
};
```

### S2 — `apps/_MyApp_/src/Stage/State/StageFSMState.h`

현재: `IFsmState<Stage>` (Stage=namespace → 컴파일 불가)  
변경 후:
```cpp
#ifndef __TOPDOWNSHOOTER_STAGE_FSM_H__
#define __TOPDOWNSHOOTER_STAGE_FSM_H__

#include "fsm/fsm_state.h"
#include "Stage/Stage.h"
#include "scene/actor.h"
#include <cstdint>

namespace TopdownShooter::Stage
{
    class StageStateMachine;  // fwd

    class BaseStageFsmState : public SJH::FSM::IFsmState<SJH::Scene::Actor>
    {
    protected:
        EStageStatus       mStateFlag;
        EStageStatus       mTransitFlag;
        StageStateMachine* mFsm = nullptr;

    public:
        explicit BaseStageFsmState(StageStateMachine* fsm,
                                   EStageStatus stateFlag,
                                   EStageStatus transitFlag)
            : mFsm(fsm), mStateFlag(stateFlag), mTransitFlag(transitFlag) {}

        uint64_t GetStateFlag()   const override { return static_cast<uint64_t>(mStateFlag);   }
        uint64_t GetTransitFlag() const override { return static_cast<uint64_t>(mTransitFlag); }
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_FSM_H__
```

### S3 — `apps/_MyApp_/src/Stage/State/StageStateMachine.h`

현재: `StateMachine<EStageStatus, Stage>` (컴파일 불가)  
변경 후:
```cpp
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
```

### S4 — `apps/_MyApp_/src/UI/StateOverlayLayer.h` (신설)

```cpp
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
            ImGui::PushStyleColor(ImGuiCol_WindowBg,     ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Border,       ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));
            ImGui::Begin("##state_overlay", nullptr,
                ImGuiWindowFlags_NoTitleBar    | ImGuiWindowFlags_NoResize  |
                ImGuiWindowFlags_NoMove        | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoBringToDisplayOnFocus);
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
```

### S5 — `apps/_MyApp_/src/Stage/Components/GameContextComponent.h` (신설)

```cpp
#ifndef __TOPDOWNSHOOTER_STAGE_COMPONENTS_GAME_CONTEXT_H__
#define __TOPDOWNSHOOTER_STAGE_COMPONENTS_GAME_CONTEXT_H__

#include "scene/actor.h"
#include "resource_registry/texture.h"
#include "UI/StateOverlayLayer.h"
// 입력 fwd
#include "InputHandler/PlayerController.h"
#include "input/input.h"

class GLFWwindow;

namespace TopdownShooter::Stage { class WaveController; }
namespace TopdownShooter::Stage { class StageStateMachine; }

namespace TopdownShooter::Stage::Components
{
    class GameContextComponent : public SJH::Scene::Component
    {
    public:
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float) override {}

        // 오버레이 제어
        UI::StateOverlayLayer*  overlay      = nullptr;
        const SJH::Texture*    titleTex     = nullptr;
        const SJH::Texture*    pauseTex     = nullptr;
        const SJH::Texture*    gameOverTex  = nullptr;

        // 입력
        SJH::KeyboardInput<Controller::PlayerController::Action>* keyboard = nullptr;
        SJH::MouseInput*        mouse        = nullptr;

        // 게임 오브젝트 refs
        Stage::WaveController*  waveCtrl     = nullptr;
        SJH::Scene::Actor*      playerActor  = nullptr;

        // FMOD — FMOD 에이전트가 채움
        // Audio::FmodStudioPlayable* bgmPlayable = nullptr;
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_COMPONENTS_GAME_CONTEXT_H__
```

### S6 — `apps/_MyApp_/src/Stage/State/StageState.Impl.h` (현재 빈 파일 → 5종 State)

현재 파일: 1줄 (완전히 빔)  
전체 내용으로 교체:

```cpp
#ifndef __TOPDOWNSHOOTER_STAGE_STATE_IMPL_H__
#define __TOPDOWNSHOOTER_STAGE_STATE_IMPL_H__

#include "Stage/State/StageFSMState.h"
#include "Stage/State/StageStateMachine.h"
#include "Stage/Components/GameContextComponent.h"
#include "Stage/WaveController.h"
#include "scene/actor.h"
#include "scene/director.h"

// TopdownShooter Manager — Audio/VFX/Physics 집계
#include "Manager.h"       // TopdownShooter::Manager::Get()

// ImGui 마우스 클릭 폴링
#include <imgui.h>
#include <GLFW/glfw3.h>

namespace TopdownShooter::Stage
{
    // ── 헬퍼 ──────────────────────────────────────────────────────────
    inline Components::GameContextComponent* GetCtx(SJH::Scene::Actor& root)
    {
        auto* ctxActor = root.FindChildByName("GameContext");
        return ctxActor ? ctxActor->GetComponent<Components::GameContextComponent>() : nullptr;
    }

    // ── BootState ────────────────────────────────────────────────────
    struct BootState : BaseStageFsmState
    {
        BootState(StageStateMachine* fsm)
            : BaseStageFsmState(fsm, EStageStatus::Boot, EStageStatus::Title) {}

        void OnEnter (SJH::Scene::Actor&) override { mFsm->ForceTransit(EStageStatus::Title); }
        void OnUpdate(SJH::Scene::Actor&, float) override {}
        void OnExit  (SJH::Scene::Actor&) override {}
    };

    // ── TitleState ───────────────────────────────────────────────────
    struct TitleState : BaseStageFsmState
    {
        TitleState(StageStateMachine* fsm)
            : BaseStageFsmState(fsm, EStageStatus::Title, EStageStatus::CombatPlay) {}

        void OnEnter(SJH::Scene::Actor& root) override
        {
            // ── 리소스 로딩 (startup 이전 내용) ──────────────────────
            // 구현자 주의: 현재 main.cpp startup()의 모든 리소스 로딩 코드를
            // GameContextComponent를 통해 여기서 실행.
            // → GetCtx(root)->window, keyboard, mouse, overlay 등 활용.
            // 자세한 이전 내역: doc/superpowers/specs/2026-06-03-stage-fsm-design.md §6

            // ── 오버레이 활성화 ───────────────────────────────────────
            if (auto* ctx = GetCtx(root))
                if (ctx->overlay) ctx->overlay->Show(ctx->titleTex);

            // [FMOD] BGM_STATE=0 — FMOD 에이전트 담당
        }

        void OnUpdate(SJH::Scene::Actor& root, float /*dt*/) override
        {
            // 아무 마우스 클릭 → CombatPlay
            if (ImGui::GetIO().MouseClicked[0] || ImGui::GetIO().MouseClicked[1])
                mFsm->ForceTransit(EStageStatus::CombatPlay);
        }

        void OnExit(SJH::Scene::Actor& root) override
        {
            if (auto* ctx = GetCtx(root))
                if (ctx->overlay) ctx->overlay->Hide();
        }
    };

    // ── CombatPlayState ───────────────────────────────────────────────
    struct CombatPlayState : BaseStageFsmState
    {
        CombatPlayState(StageStateMachine* fsm)
            : BaseStageFsmState(fsm, EStageStatus::CombatPlay,
                                static_cast<EStageStatus>(
                                    static_cast<uint64_t>(EStageStatus::Pause) |
                                    static_cast<uint64_t>(EStageStatus::GameOver))) {}

        void OnEnter(SJH::Scene::Actor& root) override
        {
            if (auto* ctx = GetCtx(root))
                if (ctx->overlay) ctx->overlay->Hide();
            // [FMOD] BGM_STATE=1, BGM resume — FMOD 에이전트 담당
            // WaveController 리셋 없음 — Pause Resume 시 상태 보존
        }

        void OnUpdate(SJH::Scene::Actor& root, float dt) override
        {
            // ── 게임 로직 (render()에서 이전된 내용) ─────────────────
            if (auto* ctx = GetCtx(root))
            {
                if (ctx->keyboard)
                    ctx->keyboard->PollHeld(
                        glfwGetCurrentContext()); // GLFW window 접근 필요 — ctx에 window* 추가 필요
            }

            TopdownShooter::Manager::Get().Audio().SetListener(/* camera pos */);
            TopdownShooter::Manager::Get().Update(dt);
            SJH::Scene::Director::Get().Update(dt);
            TopdownShooter::Manager::Get().Physics().SyncToTransform(
                SJH::Scene::Director::Get().Root());

            // [FMOD] Health = player->GetHP()/GetMaxHP() — FMOD 에이전트 담당

            // ── 전이 감지 ─────────────────────────────────────────────
            // ESC → Pause
            if (ImGui::GetIO().KeysDown[GLFW_KEY_ESCAPE])
                mFsm->TryTransit(EStageStatus::Pause);

            // Player 사망 → GameOver
            auto* ctx = GetCtx(root);
            if (ctx && ctx->playerActor && !ctx->playerActor->IsActive())
                mFsm->ForceTransit(EStageStatus::GameOver);
        }

        void OnExit(SJH::Scene::Actor&) override {}
    };

    // ── PauseState ────────────────────────────────────────────────────
    struct PauseState : BaseStageFsmState
    {
        PauseState(StageStateMachine* fsm)
            : BaseStageFsmState(fsm, EStageStatus::Pause, EStageStatus::CombatPlay) {}

        void OnEnter(SJH::Scene::Actor& root) override
        {
            if (auto* ctx = GetCtx(root))
                if (ctx->overlay) ctx->overlay->Show(ctx->pauseTex);
            // [FMOD] BGM pause — FMOD 에이전트 담당
        }

        void OnUpdate(SJH::Scene::Actor& /*root*/, float /*dt*/) override
        {
            if (ImGui::GetIO().KeysDown[GLFW_KEY_ESCAPE])
                mFsm->ForceTransit(EStageStatus::CombatPlay);
        }

        void OnExit(SJH::Scene::Actor& root) override
        {
            if (auto* ctx = GetCtx(root))
                if (ctx->overlay) ctx->overlay->Hide();
            // [FMOD] BGM resume — FMOD 에이전트 담당
        }
    };

    // ── GameOverState ─────────────────────────────────────────────────
    struct GameOverState : BaseStageFsmState
    {
        GameOverState(StageStateMachine* fsm)
            : BaseStageFsmState(fsm, EStageStatus::GameOver,
                                static_cast<EStageStatus>(0)) {} // terminal

        void OnEnter(SJH::Scene::Actor& root) override
        {
            if (auto* ctx = GetCtx(root))
                if (ctx->overlay) ctx->overlay->Show(ctx->gameOverTex);
            // [FMOD] BGM stop — FMOD 에이전트 담당
        }

        void OnUpdate(SJH::Scene::Actor&, float) override {} // terminal
        void OnExit  (SJH::Scene::Actor&) override {}
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_STATE_IMPL_H__
```

### S7 — `apps/_MyApp_/main.cpp` startup() 재구성

**변경 전**: startup()에 모든 리소스 로딩 포함 (약 150줄)  
**변경 후**: startup()은 최소화. 리소스 로딩은 TitleState::OnEnter로 이전.

startup() 구조:
```cpp
void startup() override {
    // 1. GL context 최소 설정 (기존 gl3w init 등 — sb7 base가 처리)
    auto& dir = SJH::Scene::Director::Get();
    dir.Enter();  // 씬 그래프 초기화

    // 2. GameContext Actor + Component 생성
    auto* ctxActor = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>("GameContext"));
    mCtxComp = ctxActor->AddComponent<Stage::Components::GameContextComponent>();
    mCtxComp->keyboard = &mKeyboard;
    mCtxComp->mouse    = &mMouse;
    // window, overlay 등은 TitleState::OnEnter에서 채워짐

    // 3. StateOverlayLayer 생성 + ImGui stack 등록
    auto overlayLayer = std::make_unique<UI::StateOverlayLayer>();
    mCtxComp->overlay = overlayLayer.get();
    mImGuiStack.Push(std::move(overlayLayer));

    // 4. StageStateMachine 생성 + Boot→Title 진입
    mStageFsm = std::make_unique<Stage::StageStateMachine>(dir.Root());
    mStageFsm->RegisterState(std::make_unique<Stage::BootState>(mStageFsm.get()));
    mStageFsm->RegisterState(std::make_unique<Stage::TitleState>(mStageFsm.get()));
    mStageFsm->RegisterState(std::make_unique<Stage::CombatPlayState>(mStageFsm.get()));
    mStageFsm->RegisterState(std::make_unique<Stage::PauseState>(mStageFsm.get()));
    mStageFsm->RegisterState(std::make_unique<Stage::GameOverState>(mStageFsm.get()));
    mStageFsm->OnEnter();
    // BootState::OnEnter → ForceTransit(Title) → TitleState::OnEnter (리소스 로딩)
}
```

**game_application 멤버 추가**:
```cpp
std::unique_ptr<Stage::StageStateMachine>       mStageFsm;
Stage::Components::GameContextComponent*        mCtxComp = nullptr;  // 비소유 raw
```

### S8 — `apps/_MyApp_/main.cpp` render() 슬림화

**제거할 줄** (main.cpp:264-268):
```cpp
TopdownShooter::Manager::Get().Audio().SetListener(...);   // → CombatPlayState
TopdownShooter::Manager::Get().Update(dt);                 // → CombatPlayState
SJH::Scene::Director::Get().Update(dt);                   // → CombatPlayState
TopdownShooter::Manager::Get().Physics().SyncToTransform(...); // → CombatPlayState
```

**추가할 줄**:
```cpp
mStageFsm->Update(realDt);   // 게임 로직 전체 위탁. CombatPlayState::OnUpdate가 dt 소비
```

**render() 최종 구조** (렌더링 부분은 그대로):
```cpp
void render(double currentTime) override {
    const float realDt = static_cast<float>(SJH::DeltaTime(currentTime));
    // 리사이즈 처리 (기존 유지)
    ImGui_ImplGlfwGL3_NewFrame();
    mStageFsm->Update(realDt);              // ← 추가
    // mStages 렌더링 루프 (기존 유지)
    // VFX::Draw (기존 유지)
    // ImGui RenderAll + Render (기존 유지)
}
```

### S9 — `apps/_MyApp_/src/UI/UiBootstrap.h/cpp`

`GameUiDeps`에 `StateOverlayLayer*` 필드 추가:
```cpp
struct GameUiDeps {
    GLFWwindow*            window;
    SJH::ResourceRegistry* reg;
    ImGuiLayerStack*       stack;
    std::vector<PassDebugEntry> debugEntries;
    float*                 gamma;
    // StateOverlayLayer는 startup()이 직접 Push — UiBootstrap 경유 불필요
};
```
실제로는 startup()에서 직접 `mImGuiStack.Push(overlayLayer)` 하므로 UiBootstrap 변경 최소화.

---

## §5 스코프 경계 + 충돌 매트릭스

| 파일 | 소유 | 제약 |
|------|------|------|
| `Stage/Stage.h` | **이 작업** | EStageStatus 확장 |
| `Stage/State/StageFSMState.h` | **이 작업** | TOwner 수정 |
| `Stage/State/StageStateMachine.h` | **이 작업** | TOwner 수정 |
| `Stage/State/StageState.Impl.h` | **이 작업** | 5종 State 신설 |
| `Stage/Components/GameContextComponent.h` | **이 작업** | 신설 |
| `UI/StateOverlayLayer.h` | **이 작업** | 신설 |
| `main.cpp` | **이 작업** | startup/render 구조 변경 |
| `Entity/Player/PlayerHand.cpp/h` | **건드리지 말 것** | 다른 미커밋 변경 존재 |
| `InputHandler/PlayerController.cpp/h` | **건드리지 말 것** | 다른 미커밋 변경 존재 |
| `Entity/Constants.h` | **건드리지 말 것** | 다른 미커밋 변경 존재 |
| `Audio/AudioSystem.cpp/h` | **FMOD 에이전트 전담** | `doc/design/2026-06-03-stage-fsm-fmod-handoff.md` 참조 |
| `src/fsm/state_machine.h` | **건드리지 말 것** | 엔진 코어 — NONE=0 가드 변경 금지 |

---

## §6 가드레일

1. **커밋은 사용자 요청 시만** — `git add <경로>` 경로 스코프 필수. `git add -A` 금지
2. **단위 테스트 자동 추가 금지** — 사용자 요청 시만 (`no_auto_tests` memory)
3. **return std::move(local) 수정 금지** — 의도된 학습 코드 (`pessimizing_move_intentional` memory)
4. **Actor 비상속** — Enemy/Bullet도 compound_actor factory 패턴 (`compound_actor_pattern` memory)
5. **헤더 가드 형식**: `__TOPDOWNSHOOTER_STAGE_XXX_H__` (대소문자+언더스코어). `#pragma once` 미사용
6. **주석 언어**: 한국어
7. **ImGui v1.53 핀** — `ImGuiWindowFlags_NoBringToDisplayOnFocus` 가 v1.53에 있는지 확인. 없으면 제거
8. **탭 indent (TabWidth=4)** — `.clang-format` Microsoft 스타일

---

## §7 빌드/검증 명령

```bash
# 빌드
cmake --build --preset ninja --target _MyApp_

# 실행 (리소스 상대경로 때문에 cd 필수)
cd build_ninja/apps/_MyApp_ && ./_MyApp_

# clean + configure + build + run
sh shell/CMakeALL.sh debug _MyApp_
```

**시각 검증 체크리스트**:
- [ ] 시작 시 Title.png 전체화면 오버레이 표시
- [ ] 클릭 → 오버레이 사라짐 + 게임 시작 (Wave 적 스폰)
- [ ] ESC → Pause.png 오버레이 + 게임 정지 (적 이동 멈춤)
- [ ] ESC 재입력 → Pause 해제 + 게임 재개 (Wave 카운터 보존)
- [ ] Player 사망 → GameOver.png 오버레이 + 게임 정지
- [ ] GameOver에서 추가 전이 없음

---

## §8 포인터 (참조 문서)

| 문서 | 용도 |
|------|------|
| `doc/superpowers/specs/2026-06-03-stage-fsm-design.md` | 정본 spec (v2, Q&A 반영) |
| `doc/design/2026-06-03-stage-fsm-fmod-handoff.md` | FMOD 에이전트 전용 (건드리지 말것) |
| `src/fsm/state_machine.h` | FSM 엔진 — NONE=0 가드 위치 확인용 |
| `apps/_MyApp_/src/UI/ExitButtonLayer.h` | StateOverlayLayer 미러 패턴 |
| `apps/_MyApp_/src/UI/ImGuiLayerStack.h` | Enabled 플래그 동작 확인용 |

---

## §9 변경 이력 (append-only)

| 날짜 | 내용 |
|------|------|
| 2026-06-03 | 초안 작성 (Q&A 반영: Boot State, fsm* 주입, OnEnter 순수성, startup 이전) |
