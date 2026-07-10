# Stage FSM → FMOD 에이전트 핸드오프 (`2026-06-03`)

> **용도**: Stage FSM의 State 전이에서 FMOD가 수행해야 하는 작업을 정의.
> **읽기 선행**: `doc/superpowers/specs/2026-06-03-stage-fsm-design.md` (FSM 전체 설계 정본)
> **이 문서의 범위**: Stage State Enter/Exit/Update 각 지점에서 발생해야 하는 FMOD 동작만.

---

## §1 FMOD 에이전트가 알아야 할 FSM 구조

### State 전이 그래프

```
[Title] ──(아무 클릭)──→ [CombatPlay] ──(ESC)──→ [Pause] ──(ESC)──→ [CombatPlay]
                              │
                         (Player 사망)
                              ↓
                        [GameOver] → <NONE> (terminal)
```

### State별 dt 동작

| State | Director/Manager::Update 호출 | 효과 |
|-------|-------------------------------|------|
| Title | ❌ 미호출 | 게임 로직 전면 정지 |
| CombatPlay | ✅ 실제 dt | 게임 정상 진행 |
| Pause | ❌ 미호출 | 게임 로직 전면 정지 |
| GameOver | ❌ 미호출 | 게임 로직 전면 정지 |

---

## §2 FMOD 연동 위치 — State별

### BGM_STATE FMOD 파라미터

BGM 이벤트(`event:/BGM`)의 `BGM_STATE` 파라미터:

| State | BGM_STATE 값 | 의미 |
|-------|-------------|------|
| Title | `0.0f` | Title 분위기 |
| CombatPlay | `1.0f` | Combat 분위기 |
| Boss | `2.0f` | Boss 분위기 (미래) |
| Pause | BGM 일시정지 (`setPaused(true)`) | — |
| GameOver | BGM 정지 (`stop(FMOD_STUDIO_STOP_IMMEDIATE)`) | — |

### 각 State Hook에서 수행할 FMOD 작업

```
TitleState::OnEnter(rootActor):
  ▶ bgmInstance->setParameterByName("BGM_STATE", 0.0f)
  ▶ [선택] 볼륨 정상화 (mBgmVolume = 1.0f)

CombatPlayState::OnEnter(rootActor):
  ▶ bgmInstance->setParameterByName("BGM_STATE", 1.0f)
  ▶ bgmInstance->setPaused(false)  ← Pause에서 복귀 시 resume

CombatPlayState::OnUpdate(rootActor, dt):
  ▶ Player HP 비율 계산: ratio = player->GetLife()->GetHP() / player->GetLife()->GetMaxHP()
  ▶ mStudioSystem->setParameterByName("Health", ratio)  ← global parameter

PauseState::OnEnter(rootActor):
  ▶ bgmInstance->setPaused(true)

PauseState::OnExit(rootActor):
  ▶ bgmInstance->setPaused(false)  ← CombatPlay 복귀 시

GameOverState::OnEnter(rootActor):
  ▶ bgmInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE)
  ▶ bgmInstance->release()  ← instance 정리
```

---

## §3 BGM 3D 감쇠 제거

**문제**: `main.cpp:287` 에서 `SetListener(카메라 위치)` 가 매 프레임 호출됨.
BGM event가 3D mode이면 카메라 거리에 따라 볼륨이 변함.

**원인**: `AudioSystem::SetListener(pos, forward, up)` → `mStudioSystem->setListenerAttributes(0, &attr)` → FMOD 3D positional audio 활성화.

**수정 방법 (2가지 중 선택):**

**방법 A** (권장 — FMOD Studio 설정): BGM 이벤트를 FMOD Studio에서 2D로 변경 (Spatializer 제거).
코드 변경 없음. bank 재export 필요.

**방법 B** (코드 측 workaround): BGM EventInstance 생성 후 3D position을 listener와 동기화:

```cpp
// FmodStudioPlayable::OnPlay() 또는 BGM 재생 직후:
FMOD_3D_ATTRIBUTES neutral = {};
neutral.position = {0.0f, 0.0f, 0.0f};
neutral.forward  = {0.0f, 0.0f, 1.0f};
neutral.up       = {0.0f, 1.0f, 0.0f};
bgmInstance->set3DAttributes(&neutral);
// 이후 SetListener가 어떤 위치를 전달해도 BGM은 origin에 고정 → 감쇠 없음
```

---

## §4 bgmInstance 접근 방법

현재 BGM은 `FmodStudioPlayable` Component로 "BgmActor"에 부착되어 있음:

```cpp
// startup() 에서:
auto *bgmActor = dir.Root().AddChild(std::make_unique<SJH::Scene::Actor>("BgmActor"));
auto *bgm = bgmActor->AddComponent<Audio::FmodStudioPlayable>(bgmEvent);
bgm->SetIsLoop(true);
bgm->Play();
```

State에서 접근하려면 2가지 방법:

**방법 1** (권장): `GameContextComponent`에 `bgmPlayable` 필드 추가.
startup()에서 `mCtxComp->bgmPlayable = bgm;` 저장.
State에서: `owner.GetComponent<GameContextComponent>()->bgmPlayable->SetParameter("BGM_STATE", 1.0f)`

`FmodStudioPlayable`에 다음 메서드 추가 필요:
```cpp
// FmodStudioPlayable.h
void SetParameter(const std::string& name, float value) {
    if (mInstance) mInstance->setParameterByName(name.c_str(), value);
}
void SetPaused(bool paused) {
    if (mInstance) mInstance->setPaused(paused);
}
```

**방법 2**: Scene graph 탐색:
```cpp
auto* bgmActor = SJH::Scene::Director::Get().Root().FindChildByName("BgmActor");
auto* bgm = bgmActor ? bgmActor->GetComponent<Audio::FmodStudioPlayable>() : nullptr;
if (bgm) bgm->SetParameter("BGM_STATE", 1.0f);
```

---

## §5 AudioSystem::SetListener 호출 현황

```
main.cpp:287:
TopdownShooter::Manager::Get().Audio().SetListener(mCamera->GetOwner()->GetTransform().Translate);
```

이 호출은 CombatPlayState::OnUpdate 내부로 이동됨 (render() 슬림화로 인해).
CombatPlay 상태에서만 SetListener가 호출되므로, Pause/GameOver에서는 3D 위치 업데이트가 자동 중단됨.
단, BGM 3D 감쇠 문제는 §3의 방법으로 별도 처리 필요.

---

## §6 AudioSystem에 필요한 신규/변경 API

| 변경 | 위치 | 내용 |
|------|------|------|
| `FmodStudioPlayable::SetParameter(name, value)` | `Audio/FmodStudioPlayable.h` | 신설 |
| `FmodStudioPlayable::SetPaused(bool)` | `Audio/FmodStudioPlayable.h` | 신설 |
| `GameContextComponent::bgmPlayable` | `Stage/Components/GameContextComponent.h` | 필드 추가 |

---

## §7 검증 항목

- [ ] Title 화면에서 BGM_STATE=0 의 BGM 재생 확인 (Title 분위기 음악)
- [ ] 클릭 → CombatPlay 진입 시 BGM_STATE=1 로 전환 (Combat 분위기)
- [ ] ESC → Pause 시 BGM 일시정지 (음악 멈춤)
- [ ] ESC 재입력 → CombatPlay 복귀 시 BGM 재개
- [ ] Player 사망 → GameOver 진입 시 BGM 즉시 정지
- [ ] 이동 중 BGM 볼륨이 거리에 따라 변하지 않음 (3D 감쇠 제거 확인)
- [ ] Health FMOD 파라미터가 Player HP에 따라 [0,1] 범위로 업데이트됨

---

**handoff end.**
