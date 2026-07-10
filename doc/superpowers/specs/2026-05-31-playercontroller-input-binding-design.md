# PlayerController 입력 바인딩 이관 설계

- **날짜**: 2026-05-31
- **대상**: `apps/_MyApp_` (탑다운 슈터)
- **범위**: `main.cpp` 의 인라인 입력 처리 블록을 `PlayerController` 경유 입력 바인딩으로 이관

## 1. 배경 / 문제

`apps/_MyApp_/main.cpp` 의 `onKey` / `onMouseButton` 에 "리펙토링 대상" 으로 표시된 인라인 입력 처리 블록 3개가 있다:

1. **F1 → `mShowEditor` 토글** (키보드 press) — 디버그/에디터 UI 상태. 게임플레이 무관.
2. **G키 → Damage Composite** (키보드 press) — `Parallel( TweenShake ∥ FmodStudio.Damaged )` 생성.
3. **좌클릭 → Shot Composite** (마우스 버튼) — `Sequence( Effekseer.distortion → Parallel( Fmod.Laser ∥ FmodStudio.Slash ) )` 생성.

이들은 `sb7::application` 콜백 안에 직접 박혀 있어 입력 의도와 application 생명주기가 뒤섞여 있다. 입력 바인딩을 `PlayerController` 로 이관해 응집한다.

## 2. 핵심 통찰

G / 좌클릭 블록이 참조하는 의존(`SJH::Scene::Director::Get()`, `TopdownShooter::Manager::Get()`, `SJH::ResourceRegistry::Get()`)은 **전부 싱글턴**이다. 따라서 Composite 생성 람다는 application 멤버 캡처 없이 자기완결적이며, `PlayerController` 가 audio/vfx/scene 서브시스템 헤더를 알 필요 없이 `std::function` 으로 주입받을 수 있다.

## 3. 확정 결정

| # | 결정 | 근거 |
|---|---|---|
| D1 | **콜백 주입** — PlayerController 는 audio/vfx/director 를 모르고 `std::function` 액션 핸들러만 보유 | 기존 `KeyboardInput::BindHeldHandler(std::function)` 패턴과 일관, 깔끔한 경계 유지 |
| D2 | **MouseInput 확장** — 좌클릭을 버튼 press 바인딩으로 | "입력 바인딩" 의도 충실, KeyboardInput 과 대칭 |
| D3 | **F1 은 main.cpp 유지** — G/좌클릭만 이관 | F1 은 디버그 UI 라 플레이어 입력 어휘 오염 방지 |
| D4 | **`Damage` 만 Action enum 추가** (Fire 는 마우스라 enum 불요) | 키보드 액션만 enum 대상. 마우스는 raw GLFW 버튼 바인딩 |
| D5 | **콜백/마우스는 Factory 한자리 wiring** — `PlayerActorConfig::ControllerCfg` 확장 | PlayerActor.h 의 기존 "Factory 한 자리 wiring" 원칙 유지 |

## 4. 변경 명세

### 4.1 `SJH::MouseInput` (`src/input/mouse_input.{h,cpp}`)
KeyboardInput 의 `BindPressHandler` 와 대칭으로 버튼 press 디스패치 추가.

```cpp
void BindButtonPressHandler(int button, std::function<void()> handler);
void UnbindButtonPress(int button);
// private:
std::unordered_map<int, std::function<void()>> mButtonPressHandlers;
```

`HandleButton` 은 기존 드래그(`mDragButton` = 우클릭) 처리를 유지하면서, **모든 버튼**의 `GLFW_PRESS` 에 대해 등록된 핸들러를 디스패치하도록 확장한다 (드래그 처리와 독립). 현재 좌클릭이 `button != mDragButton` early-return 으로 무시되던 동작을 제거 — 드래그 로직은 `if (button == mDragButton)` 블록으로 감싸 보존.

### 4.2 `PlayerController` (`apps/_MyApp_/src/InputHandler/PlayerController.{h,cpp}`)
- `Action` enum 에 `Damage` 추가 (G키).
- 신규 멤버: `SJH::MouseInput* mMouseInput`, `std::function<void()> mFireCallback`(좌클릭), `std::function<void()> mDamageCallback`(G).
- 신규 fluent setter: `SetMouseInput`, `SetFireCallback`, `SetDamageCallback` (기존 멱등 setter 패턴).
- `RegisterBindings` 추가:
  - `BindKey(Action::Damage, GLFW_KEY_G)` + `BindPressHandler(Action::Damage, [this]{ if (mDamageCallback) mDamageCallback(); })`
  - `if (mMouseInput) mMouseInput->BindButtonPressHandler(GLFW_MOUSE_BUTTON_LEFT, [this]{ if (mFireCallback) mFireCallback(); })`
- `UnregisterBindings` 추가: `UnbindKey(GLFW_KEY_G)`; `if (mMouseInput) mMouseInput->UnbindButtonPress(GLFW_MOUSE_BUTTON_LEFT)`.
- WASD 이동 경로·`SetUp()` 의 keyboard 필수 가드는 그대로. 마우스/콜백은 선택적 (null 이면 무해, 호출 시점 null-check).
- 헤더 추가: `#include "input/mouse_input.h"`, `#include <functional>`.

### 4.3 `PlayerActorConfig::ControllerCfg` (`apps/_MyApp_/src/Entity/Player/PlayerActor.h`)
```cpp
struct ControllerCfg {
    SJH::KeyboardInput<Action>* keyboard = nullptr;
    SJH::MouseInput*            mouse    = nullptr;   // 신규
    std::function<void()>       onFire;               // 좌클릭 (신규)
    std::function<void()>       onDamage;             // G키 (신규)
};
```
`CreatePlayerActor` 의 두 controller wiring 분기(physics / non-physics)에서:
`SetKeyboardInput → SetMouseInput(cfg.controller.mouse) → SetMovableTarget → SetFireCallback(cfg.controller.onFire) → SetDamageCallback(cfg.controller.onDamage) → SetUp()`.
헤더 추가: `#include "input/mouse_input.h"`, `#include <functional>`.

### 4.4 `main.cpp`
- `onKey`: G 블록(현재 283–305) 삭제. **F1 토글 + `mKeyboard.Dispatch` 유지.**
- `onMouseButton`: 좌클릭 블록(현재 319–348) 삭제. `mMouse.HandleButton(...)` 유지 → MouseInput 이 좌클릭을 PlayerController fire 콜백으로 디스패치.
- `WramupPlayer`: 삭제한 두 블록 로직을 `pac.controller.onFire` / `pac.controller.onDamage` 람다로 이동 + `pac.controller.mouse = &mMouse` 주입.

## 5. 동작 동등성 (회귀 가드)

- ImGui `WantCaptureKeyboard` / `WantCaptureMouse` 가드는 `onKey` / `onMouseButton` 진입부에 그대로 유지 — 디스패치는 그 *이후* 발생하므로 보존.
- G / 좌클릭의 Composite 생성 코드는 1:1 이동 (싱글턴 참조라 동작 불변).
- WASD 이동·드래그 look·F1 토글 동작 불변.

## 6. 비범위 (YAGNI)

- Fire 키보드 리매핑, 입력 컨피그 직렬화, PlayerBehavior 배선(M6 별건)은 본 작업 범위 밖.
