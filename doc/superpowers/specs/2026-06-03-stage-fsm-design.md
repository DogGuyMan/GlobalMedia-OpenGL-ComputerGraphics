# Stage FSM 설계 (2026-06-03)

> **브랜치**: `game/module/ingame/temp`
> **인수인계**: `doc/handoffs/2026-06-03/2026-06-03-stage-fsm-fmod-handoff.md` (FMOD 에이전트용)

---

## §0 목표

`Stage/State/` 에 존재하는 스텁 FSM (컴파일 불가 상태) 을 완성한다.

| 항목 | 목표 |
|------|------|
| TOwner 수정 | `Stage` (namespace) → `SJH::Scene::Actor` (Root Actor) |
| EStageStatus | Title / CombatPlay / Pause / GameOver (+Boss 미래) |
| State 4종 | TitleState / CombatPlayState / PauseState / GameOverState |
| render() 슬림화 | `mStageFsm.Update(dt)` 한 줄 — 게임 로직 CombatPlayState 위탁 |
| dt 제어 | Title/Pause/GameOver → Director::Update 미호출 (freeze), CombatPlay → 실제 dt |
| 오버레이 | `StateOverlayLayer : IImGuiLayer` — Enabled 플래그로 show/hide |
| FMOD | 별도 에이전트 핸드오프 (State Enter/Exit에서 파라미터 설정) |

---

## §1 확정 결정 (9건) — v2 (2026-06-03 Q&A 반영)

| # | 항목 | 결정 |
|---|------|------|
| D1 | **TOwner** | `SJH::Scene::Actor` (Root Actor). StateMachine은 game_application 멤버로 직접 보유 (Actor에 AddComponent 안 함) |
| D2 | **EStageStatus** | NONE=0(엔진 sentinel 유지), Title=1<<0, CombatPlay=1<<1, Boss=1<<2, Pause=1<<3, GameOver=1<<4, **Boot=1<<5** (유효 시작 State — ForceTransit(Title) 가능) |
| D3 | **전이 트리거** | **Boot→Title: 즉시(startup 완료 직후)** / Title→CombatPlay: 아무 마우스 클릭 / CombatPlay→Pause: ESC / Pause→CombatPlay: ESC / CombatPlay→GameOver: Player isDead |
| D4 | **GameOver** | terminal (`GetTransitFlag()=0`). GameOver→<NONE>. 재시작 없음 |
| D5 | **dt 제어** | CombatPlayState::OnUpdate만 Manager::Update+Director::Update 호출. 나머지 States는 미호출 → 자동 freeze |
| D6 | **오버레이** | `StateOverlayLayer : IImGuiLayer` (Enabled=true/false 토글). Full-screen ImGUI Image. PostFx 이후 자동 렌더 |
| D7 | **GameContext** | Root Actor에 `GameContextComponent` 부착. States가 `owner.GetComponent<GameContextComponent>()` 로 접근. God Object 분리(DDD)는 별도 리팩토링으로 보류 |
| D8 | **FSM 역참조** | 각 State **생성자에 `StageStateMachine*` 주입**. `mFsm->ForceTransit(GameOver)` 패턴 |
| D9 | **startup 이전** | startup() 전체를 TitleState::OnEnter로 이전 (이번 sprint 포함). startup()은 최소화: GL context + GameContextComponent + StateMachine 생성 + Boot→Title ForceTransit만 |

---

## §2 파일 레이아웃

```
apps/_MyApp_/src/Stage/
├─ Stage.h                          ← D2: EStageStatus 확장 (CombatPlay/Pause/GameOver 추가)
├─ State/
│   ├─ StageFSMState.h              ← D1: TOwner → SJH::Scene::Actor
│   ├─ StageStateMachine.h          ← D1: StateMachine<EStageStatus, SJH::Scene::Actor>
│   └─ StageState.Impl.h            ← 현재 빈 파일 → 4종 State 구현
│
├─ Components/
│   └─ GameContextComponent.h       ← 신설: States가 접근할 refs 보관
│
└─ (기존 유지)

apps/_MyApp_/src/UI/
└─ StateOverlayLayer.h              ← 신설: IImGuiLayer full-screen image
```

---

## §3 시그니처

### §3.1 EStageStatus (Stage.h)

```cpp
enum class EStageStatus : uint64_t {
    NONE       = 0,           // FSM 엔진 sentinel — 전이 불가. StateMachine 미초기화 표식
    Title      = 1ull << 0,
    CombatPlay = 1ull << 1,
    Boss       = 1ull << 2,   // 미래 — 현재 미사용
    Pause      = 1ull << 3,
    GameOver   = 1ull << 4,
    Boot       = 1ull << 5,   // 유효 시작 State. GetTransitFlag=Title. startup 완료 즉시 ForceTransit(Title)
};
```

### §3.1b BootState + 초기화 패턴

```cpp
// startup() 에서 (최소화):
mStageFsm = std::make_unique<Stage::StageStateMachine>(dir.Root());
// BootState 등록 — Title 로 즉시 전이하는 bridge state
mStageFsm->RegisterState(std::make_unique<Stage::BootState>(mStageFsm.get()));
mStageFsm->RegisterState(std::make_unique<Stage::TitleState>(mStageFsm.get()));
mStageFsm->RegisterState(std::make_unique<Stage::CombatPlayState>(mStageFsm.get()));
mStageFsm->RegisterState(std::make_unique<Stage::PauseState>(mStageFsm.get()));
mStageFsm->RegisterState(std::make_unique<Stage::GameOverState>(mStageFsm.get()));
// OnEnter() 수동 호출 → BootState::OnEnter → ForceTransit(Title) → TitleState::OnEnter (리소스 로딩)
mStageFsm->OnEnter();
```

`BootState`:
```cpp
struct BootState : BaseStageFsmState {
    BootState(StageStateMachine* fsm) : mFsm(fsm) {
        mStateFlag   = EStageStatus::Boot;
        mTransitFlag = EStageStatus::Title;
    }
    void OnEnter(SJH::Scene::Actor&) override { mFsm->ForceTransit(EStageStatus::Title); }
    void OnUpdate(SJH::Scene::Actor&, float) override {}
    void OnExit(SJH::Scene::Actor&) override {}
    StageStateMachine* mFsm;
};
```

---

### §3.2 GameContextComponent (신설)

```cpp
// apps/_MyApp_/src/Stage/Components/GameContextComponent.h
namespace TopdownShooter::Stage::Components
{
    class GameContextComponent : public SJH::Scene::Component
    {
    public:
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float) override {}

        // Overlay control (Task 5)
        UI::StateOverlayLayer*  overlay      = nullptr;
        const SJH::Texture*    titleTex     = nullptr;
        const SJH::Texture*    pauseTex     = nullptr;
        const SJH::Texture*    gameOverTex  = nullptr;

        // Input refs (State transition 감지)
        SJH::KeyboardInput<Controller::PlayerController::Action>* keyboard = nullptr;
        SJH::MouseInput*        mouse        = nullptr;

        // WaveController (CombatPlay 활성화 제어)
        Stage::WaveController*  waveCtrl     = nullptr;

        // FMOD refs — FMOD 에이전트가 추가
        // Audio::FmodStudioPlayable* bgmPlayable = nullptr;
    };
}
```

### §3.3 StageFSMState (수정)

```cpp
// apps/_MyApp_/src/Stage/State/StageFSMState.h
class BaseStageFsmState : public SJH::FSM::IFsmState<SJH::Scene::Actor> {
protected:
    EStageStatus mStateFlag;
    EStageStatus mTransitFlag;
public:
    uint64_t GetStateFlag()   const override { return (uint64_t)mStateFlag; }
    uint64_t GetTransitFlag() const override { return (uint64_t)mTransitFlag; }
    // OnEnter / OnExit / OnUpdate = pure virtual (IFsmState 정의)
};
```

### §3.4 StageStateMachine (수정)

```cpp
// apps/_MyApp_/src/Stage/State/StageStateMachine.h
class StageStateMachine : public SJH::FSM::StateMachine<EStageStatus, SJH::Scene::Actor>
{
public:
    StageStateMachine(SJH::Scene::Actor& root);
    // game_application 멤버로 보유 — Actor에 AddComponent 안 함
    // Update(dt) 는 StateMachine base 의 것 사용 (render() 에서 직접 호출)
};
```

### §3.5 StateOverlayLayer (신설)

```cpp
// apps/_MyApp_/src/UI/StateOverlayLayer.h
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
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::PushStyleColor(ImGuiCol_WindowBg,     ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Border,       ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0,0,0,0));
            ImGui::Begin("##state_overlay", nullptr,
                ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize  |
                ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoBringToDisplayOnFocus);
            if (mTex)
                ImGui::Image(
                    (ImTextureID)(intptr_t)mTex->GetTextureID(),
                    ImGui::GetIO().DisplaySize);
            ImGui::End();
            ImGui::PopStyleColor(3);
        }
    private:
        const SJH::Texture* mTex = nullptr;
    };
}
```

---

## §4 State 전이 표

| State | GetStateFlag | GetTransitFlag | OnEnter | OnUpdate | OnExit |
|-------|-------------|----------------|---------|----------|--------|
| **BootState** | Boot | Title | mFsm->ForceTransit(Title) 즉시 호출 | — | — |
| **TitleState** | Title | CombatPlay | **리소스 로딩 전체** (startup 이전 내용) + overlay->Show(titleTex), [FMOD] BGM_STATE=0 | 아무 클릭 → mFsm->ForceTransit(CombatPlay) | overlay->Hide() |
| **CombatPlayState** | CombatPlay | Pause \| GameOver | overlay->Hide(), [FMOD] BGM_STATE=1. **WaveController 리셋 없음** | Manager::Update(dt), Director::Update(dt), Physics::Sync, ESC→Pause, PlayerDead→GameOver, [FMOD] Health param | — |
| **PauseState** | Pause | CombatPlay | overlay->Show(pauseTex), [FMOD] BGM pause. **WaveController 상태 보존** | ESC → mFsm->ForceTransit(CombatPlay) | overlay->Hide(), [FMOD] BGM resume |
| **GameOverState** | GameOver | NONE (0) | overlay->Show(gameOverTex), [FMOD] BGM stop | (terminal) | — |

> **중요**: State 생성자 서명: `XxxState(StageStateMachine* fsm)` — 모든 State가 `mFsm` 멤버 보유.  
> `[FMOD]` 태그 항목 = FMOD 에이전트 핸드오프 (`doc/handoffs/2026-06-03/2026-06-03-stage-fsm-fmod-handoff.md`)

> `[FMOD]` 태그 항목 = FMOD 에이전트 핸드오프 (`doc/handoffs/2026-06-03/2026-06-03-stage-fsm-fmod-handoff.md`)

---

## §5 render() 변경 후 구조

```cpp
void render(double currentTime) override
{
    const float realDt = SJH::DeltaTime(currentTime);

    // ① 리사이즈 감지 (기존 유지)
    handleResize();

    // ② ImGUI 프레임 시작
    ImGui_ImplGlfwGL3_NewFrame();

    // ③ 입력 폴링
    mKeyboard.PollHeld(window);

    // ④ Stage FSM — 게임 로직 전체 위탁
    //    CombatPlayState::OnUpdate → Manager::Update + Director::Update + Physics::Sync
    //    Title/Pause/GameOver → 미호출 (dt=0 효과)
    mStageFsm.Update(realDt);

    // ⑤ 렌더링 (항상 실행 — State와 무관)
    for (auto& s : mStages)
        s->Render(*mDefaultTarget);

    // ⑥ VFX 렌더 (Effekseer — 카메라 참조)
    if (mCamera) {
        vmath::mat4 view = mCamera->GetViewMatrix();
        vmath::mat4 proj = mCamera->GetProjectionMatrix();
        TopdownShooter::Manager::Get().VFX().Draw(&view[0][0], &proj[0][0]);
    }

    // ⑦ ImGUI (PostFx 이후 최상위 렌더 — StateOverlayLayer 포함)
    mImGuiStack.RenderAll(mShowEditor);
    ImGui::Render();
}
```

**제거되는 render() 코드:**
- `TopdownShooter::Manager::Get().Update(dt)` ← CombatPlayState::OnUpdate 이동
- `SJH::Scene::Director::Get().Update(dt)` ← CombatPlayState::OnUpdate 이동
- `Physics().SyncToTransform(...)` ← CombatPlayState::OnUpdate 이동

---

## §6 startup() 변경

startup() 마지막에 FSM 초기화 추가:

```cpp
// startup() 끝에서:

// --- GameContextComponent 생성 및 Root Actor 부착 ---
auto* ctxActor = dir.Root().AddChild(
    std::make_unique<SJH::Scene::Actor>("GameContext"));
mCtxComp = ctxActor->AddComponent<Stage::Components::GameContextComponent>();

// 오버레이 레이어 생성 + ImGui stack 등록 + ctx에 저장
auto overlayLayer = std::make_unique<UI::StateOverlayLayer>();
mCtxComp->overlay = overlayLayer.get();
mImGuiStack.Push(std::move(overlayLayer));

// 오버레이용 텍스처 로드
mCtxComp->titleTex    = reg.CreateTexture("title_tex",
    SJH::Image::Load("title_tex", "resources/texture/Title.png").get());
mCtxComp->pauseTex    = reg.CreateTexture("pause_tex",
    SJH::Image::Load("pause_tex", "resources/texture/Pause.png").get());
mCtxComp->gameOverTex = reg.CreateTexture("gameover_tex",
    SJH::Image::Load("gameover_tex", "resources/texture/GameOver.png").get());

// 입력 refs
mCtxComp->keyboard = &mKeyboard;
mCtxComp->mouse    = &mMouse;

// WaveController ref (waveSpawner Actor에서 가져옴)
mCtxComp->waveCtrl = waveSpawner->GetComponent<Stage::WaveController>();

// --- StageStateMachine 생성 ---
mStageFsm = std::make_unique<Stage::StageStateMachine>(dir.Root());
mStageFsm->RegisterState(std::make_unique<Stage::TitleState>());
mStageFsm->RegisterState(std::make_unique<Stage::CombatPlayState>());
mStageFsm->RegisterState(std::make_unique<Stage::PauseState>());
mStageFsm->RegisterState(std::make_unique<Stage::GameOverState>());
mStageFsm->ForceTransit(EStageStatus::Title);  // OnEnter(TitleState) 즉시 발화
```

**game_application 신규 멤버:**
```cpp
std::unique_ptr<Stage::StageStateMachine> mStageFsm;
Stage::Components::GameContextComponent*  mCtxComp = nullptr;  // 비소유 raw
```

---

## §7 Player isDead 감지 메커니즘

CombatPlayState::OnUpdate에서 Player 사망을 감지해야 합니다. 방법:

```cpp
// CombatPlayState::OnUpdate 내부
auto* ctx    = owner.GetComponent<GameContextComponent>();
auto* player = SJH::Scene::Director::Get().Root().FindChildByName("PlayerSprite");
if (player && !player->IsActive())
{
    // Player가 비활성 = 사망
    GetFsm()->ForceTransit(EStageStatus::GameOver);
    return;
}
```

> `GetFsm()` = `IFsmState<TOwner>` 에서 FSM 역참조 — 현재 API에 없으면 추가 필요.
> 대안: State 생성자에 `StageStateMachine*` 전달.

---

## §8 구현 순서 (의존 그래프)

```
S1. Stage.h — EStageStatus NONE 추가 + CombatPlay/Pause/GameOver 신설
S2. StageFSMState.h — TOwner = SJH::Scene::Actor 수정
S3. StageStateMachine.h — TOwner = SJH::Scene::Actor 수정
S4. apps/_MyApp_/src/UI/StateOverlayLayer.h — 신설
S5. apps/_MyApp_/src/Stage/Components/GameContextComponent.h — 신설
S6. StageState.Impl.h — 4종 State 구현 (TitleState/CombatPlayState/PauseState/GameOverState)
S7. main.cpp startup() — GameContextComponent + Overlay 텍스처 + FSM 초기화 추가
S8. main.cpp render() — mStageFsm.Update(dt) 추가, Manager/Director Update 제거
S9. apps/_MyApp_/src/UI/UiBootstrap.h/cpp — GameUiDeps 에 overlay* 추가
S10. 빌드 검증: cmake --build --preset ninja --target _MyApp_
S11. 시각 검증: Title화면 표시 → 클릭 → 게임 시작 → ESC → Pause → ESC → 재개 → Player 사망 → GameOver
```

---

## §9 시각 검증 체크리스트

- [ ] 시작 시 Title.png 전체화면 오버레이 표시
- [ ] 클릭 → 오버레이 사라짐 + 게임 시작 (Wave Controller 작동)
- [ ] ESC → Pause.png 오버레이 + 게임 정지 (적 이동 멈춤)
- [ ] ESC 재입력 → Pause 해제 + 게임 재개
- [ ] Player 사망 → GameOver.png 오버레이 + 게임 정지
- [ ] GameOver에서 추가 전이 없음 (terminal)

---

## §10 Out of Scope

| 항목 | 이유 |
|------|------|
| FMOD BGM_STATE / Health 파라미터 | FMOD 에이전트 전담 (handoff 문서 참조) |
| BGM 3D 감쇠 제거 | FMOD 에이전트 전담 |
| startup() 코드 TitleState::OnEnter 이전 | GameContextComponent 구조 안정화 후 차기 리팩토링 |
| Boss State 구현 | 미래 마일스톤 |
| GameOver 재시작 | 미래 마일스톤 |

---

**spec end.**
