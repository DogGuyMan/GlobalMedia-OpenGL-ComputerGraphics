> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

<!-- # M2 P2 Agent Prompt — PlayerController + Camera follow + main.cpp 정리

> **사용법**: 이 파일의 `## Prompt` 섹션 아래 *전체 내용* 을 복사하여 새 Claude Code 세션 (또는 Agent tool 의 `prompt` 인자) 에 그대로 붙여넣으세요. self-contained 라 다른 컨텍스트 불요.
>
> **작업 디렉토리**: `/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics`
> **브랜치**: `game/module/sprite`
> **예상 시간**: 30~50분 (PlayerController 신설 + CameraController follow 추가 + main.cpp 정리 + 시각 검증)
> **시각 검증 요구**: 사용자가 직접 빌드 후 실행해 *WASD 로 sprite 이동 + Camera follow* 확인 필요 — subagent 는 빌드 PASS 까지만

--- -->

## Prompt

You are implementing **M2 P2** of the topdown-shooter milestone — Player Actor 가 WASD 로 XZ 이동, Camera 가 Player follow.

현 `apps/_MyApp_/main.cpp` 는 Camera Actor 에 `CameraController` 가 부착되어 *카메라 자체가 free-fly*. M2 spec (부록 D) 의도는 *Player 가 WASD 로 XZ 평면 이동, Camera follow* — 두 컨트롤러 분리:

1. **PlayerController** (신설) — sprite Actor 에 부착. WASD → Owner Transform.Translate XZ 이동
2. **CameraController** (수정) — follow target Actor 보유 + 매 Update 에서 target.Transform.Translate 추적 + Mouse yaw/pitch 는 추가 상대 회전

### 사전 정독 (코드 작성 *전*) — 필수

1. **`src/input/keyboard_input.h` 전체** — `KeyboardInput<TAction>::Bind` 의 정확한 시그니처 확인. 본 prompt 의 `K::Held` 가정이 실제와 다르면 *그 시점에서 코드 정정* (escalate 가 아니라 *기존 CameraController.cpp 의 Bind 호출을 그대로 모방*).
2. **`apps/_MyApp_/src/InputHandler/CameraController.h/.cpp` 전체** — Component + Builder + WASD/Mouse 패턴 (대칭 모방 대상).
3. **`apps/_MyApp_/main.cpp` 전체 현 상태** — sprite/camera 셋업 위치 + 디버그 spdlog 위치 (line 157-161).
4. **`src/scene/actor.h:37`** — `Component::GetOwner()` 메서드 시그니처.

### Project Conventions (필독)

**Actor 비상속 컨벤션** (user memory `compound_actor_pattern.md`):
> "PreBuilt 는 `src/scene/compound_actor.h` 의 free factory. **Actor 비상속** — 특수 속성은 Component 로만"

→ `PlayerController` 는 *반드시* `SJH::Scene::Component` 베이스 상속. 절대 *`class Player : public Actor`* 같은 Actor 상속 클래스 만들지 말 것.

**KeyboardInput<TAction> 별도 인스턴스 컨벤션:**
`KeyboardInput<Controller::CameraController::Action>` 과 `KeyboardInput<Controller::PlayerController::Action>` 은 *서로 다른 template 인스턴스* — main.cpp 가 *두 별도 인스턴스* (`mKeyboard` + `mPlayerKeyboard`) 보유 필수. 단일 인스턴스 공유 시도 시 *Action enum type 충돌* 컴파일 에러.

기타:
- 한국어 주석 OK
- Header guard `_TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__` 형태 (CameraController 와 일관)
- `.claude/CLAUDE.md` 가 cwd 기반 자동 로드됨

---

### Step 1: `apps/_MyApp_/src/InputHandler/PlayerController.h` 작성

```cpp
#ifndef _TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__
#define _TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__

#include "input/keyboard_input.h"
#include "scene/actor.h"
#include <vmath.h>

namespace TopdownShooter::Controller
{
    /// @brief Top-down 게임의 Player 이동 컨트롤러 — WASD → Owner Transform.Translate XZ 이동.
    /// @details
    ///   ### 동작
    ///   - 제어 대상은 *Component 의 owner Actor* 의 Transform (sprite Actor).
    ///   - WASD: +Z/-Z (W=앞=-Z, S=뒤=+Z, A/D 는 X)
    ///   - 카메라 회전과 독립 — 월드 축 기준 이동 (회전 적용 없음, 탑다운 컨벤션)
    class PlayerController : public SJH::Scene::Component
    {
    public:
        enum class Action : int
        {
            MoveForward = 1, // W
            MoveBack,        // S
            MoveLeft,        // A
            MoveRight,       // D
        };

        PlayerController() = default;
        PlayerController(const PlayerController&)            = delete;
        PlayerController& operator=(const PlayerController&) = delete;

        bool SetUp();

        /// @brief KeyboardInput 의존 주입. SetUp() 전 호출 필수.
        PlayerController& SetKeyboardInput(SJH::KeyboardInput<Action>* k);

        /// @brief 이동 속도 (월드 단위/프레임, default 0.05).
        PlayerController& SetMoveSpeed(float v);

        virtual void OnEnter() override;
        virtual void OnExit() override;
        virtual void Update(float dt) override;

    private:
        bool mIsInitialized = false;
        SJH::KeyboardInput<Action>* mKeyboardInput = nullptr;
        vmath::vec3 mMoveDelta = vmath::vec3(0.0f, 0.0f, 0.0f);
        float mMoveSpeed = 0.05f;

        void RegisterBindings();
        void UnregisterBindings();
    };
}

#endif // _TOPDOWNSHOOTER_INPUT_PLAYER_CONTROLLER__
```

### Step 2: `apps/_MyApp_/src/InputHandler/PlayerController.cpp` 작성

> ⚠️ **본 코드의 `RegisterBindings` 의 Bind 호출 시그니처는 *가정* 임**. `src/input/keyboard_input.h` + `CameraController.cpp` 의 실제 패턴과 다르면 *그것에 맞춰 정정*.

```cpp
#include "PlayerController.h"
#include <<spdlog>/spdlog.h>

namespace TopdownShooter::Controller
{
    PlayerController& PlayerController::SetKeyboardInput(SJH::KeyboardInput<Action>* k)
    {
        mKeyboardInput = k;
        return *this;
    }

    PlayerController& PlayerController::SetMoveSpeed(float v)
    {
        mMoveSpeed = v;
        return *this;
    }

    bool PlayerController::SetUp()
    {
        if (mIsInitialized) return true;
        if (!mKeyboardInput) {
            spdlog::error("[PlayerController] SetUp failed — KeyboardInput 미주입");
            return false;
        }
        RegisterBindings();
        mIsInitialized = true;
        return true;
    }

    void PlayerController::OnEnter() {}
    void PlayerController::OnExit()
    {
        if (mKeyboardInput) UnregisterBindings();
    }

    void PlayerController::Update(float /*dt*/)
    {
        if (!mIsInitialized) return;
        auto* owner = GetOwner();
        if (!owner) return;

        // mMoveDelta 가 KeyboardInput held handler 에서 매 프레임 누적됨.
        // 월드 축 기준 이동 (탑다운 — 카메라 회전 무관, X/Z 평면)
        auto& tr = owner->GetTransform();
        tr.Translate += mMoveDelta * mMoveSpeed;
        mMoveDelta = vmath::vec3(0.0f, 0.0f, 0.0f);
    }

    void PlayerController::RegisterBindings()
    {
        // ⚠️ CameraController.cpp 의 Bind 호출 정확 패턴 모방. 아래는 *가정* — 실제 다르면 정정.
        using K = SJH::KeyboardInput<Action>;
        mKeyboardInput->Bind(K::Held, GLFW_KEY_W, Action::MoveForward,
            [this](){ mMoveDelta[2] -= 1.0f; });   // 앞 = -Z (OpenGL forward)
        mKeyboardInput->Bind(K::Held, GLFW_KEY_S, Action::MoveBack,
            [this](){ mMoveDelta[2] += 1.0f; });
        mKeyboardInput->Bind(K::Held, GLFW_KEY_A, Action::MoveLeft,
            [this](){ mMoveDelta[0] -= 1.0f; });
        mKeyboardInput->Bind(K::Held, GLFW_KEY_D, Action::MoveRight,
            [this](){ mMoveDelta[0] += 1.0f; });
    }

    void PlayerController::UnregisterBindings()
    {
        // KeyboardInput 의 Unbind API 가 없으면 빈 구현. mIsInitialized 가드로 충분.
    }
}
```

### Step 3: `apps/_MyApp_/src/InputHandler/CameraController.{h,cpp}` 수정 — follow mode 추가

#### CameraController.h — `SetFollowTarget` + `SetFollowOffset` 추가

기존 public 메서드 다음에 추가:

```cpp
public:
    /// @brief Follow target Actor 설정. nullptr 이면 기존 free-fly 모드 유지.
    /// @details Update 가 매 프레임 target.Transform.Translate + mFollowOffset 으로 카메라 위치 갱신.
    ///          Mouse yaw/pitch 는 그대로 작동 — target 머리 위에서 *상대 회전*.
    CameraController& SetFollowTarget(SJH::Scene::Actor* t);

    /// @brief Follow 시 target → camera offset (default = vec3(0, 5, 5)).
    CameraController& SetFollowOffset(vmath::vec3 offset);

private:
    SJH::Scene::Actor* mFollowTarget = nullptr;
    vmath::vec3 mFollowOffset = vmath::vec3(0.0f, 5.0f, 5.0f);
```

#### CameraController.cpp — `Update` 의 follow 분기 추가

기존 `Update(float dt)` 본문 시작 부분에:

```cpp
void CameraController::Update(float dt)
{
    if (!mIsInitialized || !mCamera) return;
    auto* owner = mCamera->GetOwner();
    if (!owner) return;

    auto& tr = owner->GetTransform();

    // === Follow mode 분기 ===
    if (mFollowTarget) {
        const auto& targetTr = mFollowTarget->GetTransform();
        tr.Translate = targetTr.Translate + mFollowOffset;
        // Mouse yaw/pitch 누적은 그대로 — target 머리 위에서 상대 회전
        tr.EulerRot = vmath::vec3(mPitchDeg, mYawDeg, 0.0f);
        mMoveDelta = vmath::vec3(0.0f, 0.0f, 0.0f);   // free-fly WASD 누적 폐기
        return;
    }

    // === Free-fly mode (기존 로직 그대로) ===
    // ... 기존 코드 ...
}
```

`SetFollowTarget` / `SetFollowOffset` 구현:

```cpp
CameraController& CameraController::SetFollowTarget(SJH::Scene::Actor* t)
{
    mFollowTarget = t;
    return *this;
}

CameraController& CameraController::SetFollowOffset(vmath::vec3 offset)
{
    mFollowOffset = offset;
    return *this;
}
```

### Step 4: `apps/_MyApp_/src/InputHandler/CMakeLists.txt` 수정

기존 source 목록에 `PlayerController.cpp` 1줄 추가 (CameraController.cpp 다음).

### Step 5: `apps/_MyApp_/main.cpp` 수정

#### 5a. include 추가
```cpp
#include "apps/_MyApp_/src/InputHandler/PlayerController.h"
```

#### 5b. private 멤버 추가 (mKeyboard 다음 줄)
```cpp
SJH::KeyboardInput<Controller::PlayerController::Action> mPlayerKeyboard;
```

#### 5c. `startup()` — Camera Actor 셋업을 *변수로 보관*

기존 (line 96-108 부근):
```cpp
auto camActor = SJH::Scene::CreateCameraActor("MainCamera", 45.0f, aspect, 0.1f, 100.0f);
camActor->GetTransform().Translate = vmath::vec3(0.0f, 5.0f, 5.0f);
camActor->GetTransform().EulerRot = vmath::vec3(-45.0f, 0.0f, 0.0f);
auto *cam = camActor->GetComponent<SJH::Scene::Camera>();
camActor->AddComponent<Controller::CameraController>()
    ->SetKeyboardInput(&mKeyboard)
    .SetMouseInput(&mMouse)
    .SetCamera(cam)
    .SetUp();
cam->SetTargetFramebuffer(nullptr);

mCameraActor = dir.Root().AddChild(std::move(camActor));
mCamera = cam;
dir.SetActiveCamera(cam);
```

다음으로 교체 — `camCtrl` 변수 보관:
```cpp
auto camActor = SJH::Scene::CreateCameraActor("MainCamera", 45.0f, aspect, 0.1f, 100.0f);
camActor->GetTransform().Translate = vmath::vec3(0.0f, 5.0f, 5.0f);
camActor->GetTransform().EulerRot = vmath::vec3(-45.0f, 0.0f, 0.0f);
auto *cam = camActor->GetComponent<SJH::Scene::Camera>();
auto *camCtrl = camActor->AddComponent<Controller::CameraController>();
camCtrl->SetKeyboardInput(&mKeyboard)
    .SetMouseInput(&mMouse)
    .SetCamera(cam)
    .SetUp();
cam->SetTargetFramebuffer(nullptr);

mCameraActor = dir.Root().AddChild(std::move(camActor));
mCamera = cam;
dir.SetActiveCamera(cam);
```

#### 5d. `startup()` — sprite Actor 셋업 + PlayerController 부착 + Camera follow target 설정

기존 (line 113-117):
```cpp
auto spriteActor = std::make_unique<SJH::Scene::Actor>("PlayerSprite");
spriteActor->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
spriteActor->GetTransform().Scale = vmath::vec3(1.0f, 1.0f, 1.0f);
spriteActor->AddComponent<SJH::Scene::MeshRenderer>(mPlane.get(), mat);
mSpriteActor = dir.Root().AddChild(std::move(spriteActor));
```

다음으로 교체:
```cpp
auto spriteActor = std::make_unique<SJH::Scene::Actor>("PlayerSprite");
spriteActor->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
spriteActor->GetTransform().Scale = vmath::vec3(1.0f, 1.0f, 1.0f);
spriteActor->AddComponent<SJH::Scene::MeshRenderer>(mPlane.get(), mat);
spriteActor->AddComponent<Controller::PlayerController>()
    ->SetKeyboardInput(&mPlayerKeyboard)
    .SetMoveSpeed(0.05f)
    .SetUp();
mSpriteActor = dir.Root().AddChild(std::move(spriteActor));

// Camera follow target — sprite Actor 가 root 의 child 로 등록된 후
camCtrl->SetFollowTarget(mSpriteActor)
       .SetFollowOffset(vmath::vec3(0.0f, 5.0f, 5.0f));
```

#### 5e. `render()` — Player keyboard 도 poll

기존 `mKeyboard.PollHeld(window);` 다음 줄에:
```cpp
mPlayerKeyboard.PollHeld(window);
```

#### 5f. `onKey` — 디버그 spdlog *완전* 제거 + Player keyboard dispatch 추가

기존 (line 154-162):
```cpp
void onKey(int key, int action) override
{
    mKeyboard.Dispatch(key, action);
    spdlog::info("Pressed {} {} {}",
        mCameraActor->GetTransform().Translate[0],
        mCameraActor->GetTransform().Translate[1],
        mCameraActor->GetTransform().Translate[2]
    );
}
```

다음으로 교체:
```cpp
void onKey(int key, int action) override
{
    mKeyboard.Dispatch(key, action);
    mPlayerKeyboard.Dispatch(key, action);
}
```

### Step 6: 빌드 검증

```bash
cmake --build --preset ninja --target _MyApp_
```

기대: 빌드 PASS, 컴파일 에러 0.

**시각 검증은 사용자 책임** — subagent 는 빌드까지만. 사용자가:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
실행해 WASD → sprite XZ 이동, Camera follow, 마우스 우클릭 드래그 → yaw/pitch 시점 회전 확인.

### Step 7: Single commit

```bash
git add apps/_MyApp_/src/InputHandler/PlayerController.h \
        apps/_MyApp_/src/InputHandler/PlayerController.cpp \
        apps/_MyApp_/src/InputHandler/CameraController.h \
        apps/_MyApp_/src/InputHandler/CameraController.cpp \
        apps/_MyApp_/src/InputHandler/CMakeLists.txt \
        apps/_MyApp_/main.cpp
git commit -m "feat(_MyApp_): M2 Player WASD + Camera follow

- PlayerController Component 신설 — sprite Actor 부착, WASD → Transform.Translate XZ
- CameraController follow mode 추가 — SetFollowTarget(Actor*) + SetFollowOffset
  매 Update 에서 target Translate + offset 으로 카메라 위치 갱신. Mouse yaw/pitch
  는 상대 회전으로 유지.
- main.cpp: sprite Actor 에 PlayerController + Camera 에 SetFollowTarget(sprite)
- 디버그 spdlog (line 157-161) 제거
- mPlayerKeyboard 별도 인스턴스 — PlayerController::Action 용 (CameraController 와 분리)

M2 P2."
```

## DO NOT touch

- `src/fsm/`, `src/scene/*`, `src/sprite/*`, 기타 코어 모듈 — 별도 subagent (P1) 담당
- `.claude/CLAUDE.md` — 별도 subagent (P3) 담당
- `apps/_MyApp_/src/Algebraic/`, `apps/_MyApp_/src/Entity/` — 사용자 WIP, 본 task 영역 밖

## Self-Review

- PlayerController 가 KeyboardInput<TAction>::Bind 시그니처 정확 (CameraController 모방)?
- CameraController follow mode 가 *기존 free-fly mode 보존* (mFollowTarget == nullptr 일 때)?
- main.cpp 의 *Camera 먼저 셋업 + camCtrl 변수 보관, sprite 만든 후 SetFollowTarget* 순서?
- mPlayerKeyboard (별도 인스턴스) PollHeld + Dispatch 매 프레임 호출?
- 디버그 spdlog 완전 제거?
- 빌드 PASS (사용자가 시각 검증 예정) ?

## Report

Status: DONE / DONE_WITH_CONCERNS / BLOCKED / NEEDS_CONTEXT
+ Files changed (6 files)
+ Build result
+ Self-review findings (특히 KeyboardInput Bind 시그니처가 prompt 가정과 다를 시 어떻게 정정했는지)
+ Git commit SHA
+ 사용자 시각 검증 요구사항 (WASD + 마우스 작동) 명시

Work from: `/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics`
