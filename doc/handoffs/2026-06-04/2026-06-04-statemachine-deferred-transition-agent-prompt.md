# StateMachine 재진입 버그 수정 — 에이전트 핸드오프 (2026-06-04)

> ✅ **완료됨 (2026-06-04, 빌드 GREEN, 미커밋).** 본 핸드오프는 원 작성 세션이 직접 적용함 — 재실행 불요.
> 적용분: `src/fsm/state_machine.h`(deferred 전이 = TryTransitImpl 검증즉시+pending 큐잉, `ApplyPending()`
> 신설, `Update` 가 OnUpdate 앞에서 적용, I3 주석 갱신) + `StageState.Impl.h`(STEP 2 — IsAlive 가드 제거).
> 추가 검증: 전이 호출처 4곳 모두 deferred 호환 확인 — `TogglePause`(main.cpp:487, State() 읽고 전이) /
> `WaveController`(GameOver) / Title·Pause OnUpdate 클릭. 다른 `StateMachine<>` 소비자는 StageStateMachine 단독.
> 남은 것 = 런타임 청취검증(GameOver LPF 해제) + 사용자 커밋. 아래는 설계 기록으로 보존.

---


> **용도**: `SJH::FSM::StateMachine` 의 동기·재진입 전이를 **deferred(지연) 전이**로 재구성해,
> 전이 직후 이전 State 의 코드가 같은 call stack 에서 계속 실행되는 버그를 구조적으로 제거한다.
> **산출물 종류**: Artifact A — 새 세션에 붙여넣어 바로 실행하는 bounded task 프롬프트.
> **프레이밍**: `clean-ddd-hexagonal` 의 Aggregate Root 불변식 관점 (StateMachine = Aggregate Root,
> "tick 당 current State 의 behavior 만 실행" 은 aggregate invariant).

---

## 0. 문제 요약 (왜 고치는가)

`StateMachine::TryTransit` 가 **즉시·동기적으로** `OnExit(cur) → curState=target → OnEnter(target)` 를 실행한다
([src/fsm/state_machine.h:114-134](../../src/fsm/state_machine.h#L114)). 그런데 전이를 트리거하는 코드가
**현재 State 의 `OnUpdate` 호출 스택 내부**에서 돈다:

```
render()
└─ mStageFsm->Update(dt)                     // FSM 이 curState.OnUpdate 디스패치
   └─ CombatPlayState::OnUpdate(dt)          // ← ① 아직 이 함수 안
      ├─ Director::Get().Update(dt)
      │  └─ WaveController::Update(dt)
      │     └─ mStageFsm->TryTransit(GameOver)   // WaveController.cpp:111-114
      │        └─ TryTransitImpl: OnExit(Combat); curState=GameOver;
      │           └─ GameOverState::OnEnter()     // ← ② 동기 실행. 여기서 Health=1.0
      └─ [꼬리] ctx->audio->SetGlobalParameter("Health", 0)  // ← ③ ②를 덮어씀
```

**증상(실측)**: GameOver 진입 시 Low Pass Filter 가 안 풀림 — ②가 `Health=1.0`(LPF 해제)을 세팅해도
③의 CombatPlay 잔여 코드가 `Health=0` 으로 덮어쓰기 때문. 임시 땜빵으로 ③에 `IsAlive()` 가드를
넣어 막아둔 상태지만, 이는 **증상별 땜빵**이고 동일 종류의 재진입 버그가 다른 State/파라미터에서
재발할 수 있다.

**DDD 관점**: StateMachine 은 Aggregate Root 이고 `curState` 는 그 내부 일관성 상태다. 자식(`WaveController`)
혹은 State hook 이 **연산 도중(mid-OnUpdate) 재진입으로 aggregate 상태를 변이**시키면, 절반만 끝난 연산
(CombatPlay.OnUpdate)이 *새 상태에 대해* 계속 실행된다 → invariant 위반. Aggregate Root 는 전이를
**통제된 경계(Update 경계)로 직렬화**해 invariant 를 지켜야 한다.

**요구 사항(사용자)**: "OnEnter 로 넘어가면 그 즉시 모든 Update 도 현 State 것만 실행. State 가 넘어갔음에도
다른 State 의 코드가 실행되면 안 된다."

---

## 1. 절대 규칙 (Hard rules)

- **커밋 금지.** 사용자가 직접, 경로 지정 partial 커밋한다(`git commit <path>`, `git add -A` 금지 —
  같은 working tree 에서 사용자가 병렬 작업/staging 중). 구현 + 빌드검증 + 보고까지만.
- **커밋 메시지 트레일러 `Co-Authored-By` 미사용** (프로젝트 관례). 제안 메시지 줄 때도 빼라.
- **주석은 한국어.** 헤더 가드 `__..._H__` 형식 (`#pragma once` 금지). 멤버 `mPascalCase` /
  지역 `camelCase` / 타입·함수 `PascalCase`. `.clang-format` = Microsoft, **Tab 들여쓰기**, ColumnLimit=0.
- **단위 테스트 자동 작성 금지** (요청 시에만). 이 작업은 구현 + 빌드 GREEN + 런타임 청취검증으로 끝.
- **빌드/검증 명령(정확히 이것):**
  ```bash
  cmake --build --preset ninja --target _MyApp_
  # 실행: cd build_ninja/apps/_MyApp_ && ./_MyApp_
  ```

---

## 2. 검증된 사실 (Verified facts — 핸드오프 작성 시점 2026-06-04 기준, 재확인 권장)

> 저장소가 같은 tree 에서 병렬 편집된다. 시작 시 `git status` / 해당 파일을 다시 열어 줄번호·시그니처를
> 재확인하라. 아래는 작성 시점의 실측이다.

### 2.1 핵심 파일 = `src/fsm/state_machine.h` (단일 수정 대상)

현재 관련 멤버/메서드 (라인 근사 — 재확인):

```cpp
// 멤버 (136-138)
TOwner *mOwner;
TState  curState = TState::NONE;
std::unordered_map<StateU, std::unique_ptr<IFsmState<TOwner>>> mStates;

// 매 프레임 디스패치 (106-111)
void Update(float dt) override
{
    auto it = mStates.find((StateU)curState);
    if (it != mStates.end() && it->second)
        it->second->OnUpdate(*mOwner, dt);
}

// 즉시 전이 (114-134) — ★ 이게 버그 원천
bool TryTransitImpl(StateU targetBit)
{
    const StateU curr = (StateU)curState;
    if (curr == 0) return false;                                   // I1: NONE 전이 불가
    auto curIt = mStates.find(curr);
    if (curIt == mStates.end() || !curIt->second) return false;
    if ((curIt->second->GetTransitFlag() & targetBit) != targetBit) return false; // I2 가드
    auto targetIt = mStates.find(targetBit);
    if (targetIt == mStates.end() || !targetIt->second) return false;
    curIt->second->OnExit(*mOwner);          // ← 즉시
    curState = (TState)targetBit;            // ← 즉시
    targetIt->second->OnEnter(*mOwner);      // ← 즉시 (재진입 OnEnter)
    return true;
}

// public 진입점 (68-80): TryTransit→TryTransitImpl, ForceTransit→실패 시 std::abort()
// FSM 자체 hook (90-103): OnEnter()=curState.OnEnter, OnExit()=curState.OnExit
```

### 2.2 전이 호출처 (전부 "mid-OnUpdate" 다 — deferred 로 바뀌어도 동작해야 함)

- [src/fsm/state_machine.h:90-95](../../src/fsm/state_machine.h#L90) `StateMachine::OnEnter()` — **전이 아님**.
  startup 에서 `mStageFsm->OnEnter()` 로 초기 Title 진입에만 쓰임. **건드리지 말 것**.
- `WaveController::Update` → `mStageFsm->TryTransit(GameOver)` ([apps/_MyApp_/src/Stage/WaveController.cpp:111-114](../../apps/_MyApp_/src/Stage/WaveController.cpp#L111)).
  `Director::Update` 안에서 돌고, 그게 `CombatPlayState::OnUpdate` 안에서 호출됨.
- `TitleState::OnUpdate` → `mFsm->TryTransit(CombatPlay)` (빈 화면 클릭).
- `PauseState::OnUpdate` → `mFsm->TryTransit(CombatPlay)` (재개 클릭).
  → 위 [apps/_MyApp_/src/Stage/State/StageState.Impl.h](../../apps/_MyApp_/src/Stage/State/StageState.Impl.h).
  **이 셋 모두 OnUpdate 도중 전이** 이므로, deferred 로 바꾸면 "다음 Update 경계에서 적용"된다(1프레임 지연,
  클릭→전이는 체감 불가). 정상이다.

### 2.3 현재 땜빵 (수정 후 redundant 가 됨)

`CombatPlayState::OnUpdate` 의 Health 갱신에 `&& ctx->playerLife->IsAlive()` 가드가 붙어 있다
([StageState.Impl.h](../../apps/_MyApp_/src/Stage/State/StageState.Impl.h), CombatPlay.OnUpdate 의
`if (auto *ctx = GetCtx(root); ctx && ctx->audio && ctx->playerLife && ctx->playerLife->IsAlive())`).
deferred 전이가 들어가면 이 가드는 **불필요**해진다(아래 STEP 2 참조).

### 2.4 소비자 = StageStateMachine 단독

`SJH::FSM::StateMachine` 의 실사용처는 현재 `apps/_MyApp_/src/Stage/StageStateMachine.h` (= EStageStatus,
Root Actor) 뿐. 시작 시 `grep -rn "TryTransit\|ForceTransit\|->Update(" apps/_MyApp_/src/Stage` +
`grep -rn "StateMachine<" apps/_MyApp_` 로 다른 소비자 없음 재확인하라. (있으면 같은 deferred 의미가 적용됨.)

---

## 3. STEP 1 — `src/fsm/state_machine.h` 를 deferred 전이로 재구성

**설계**: `TryTransitImpl` 은 *검증만 즉시* 수행하고 실제 `OnExit/OnEnter` 적용은 **pending 으로 큐잉**한다.
`Update` 는 **맨 앞에서 pending 을 먼저 적용**(`ApplyPending`)한 뒤 현재 State 의 `OnUpdate` 를 디스패치한다.
이렇게 하면 어떤 `OnUpdate` 도중에 전이가 요청돼도, 그 `OnUpdate` 는 끝까지 *현재 State* 로 실행되고
(= 자기 코드가 자기 상태에서 도는 정상 동작), 전이(OnExit→OnEnter)는 **다음 Update 경계**에서 깨끗이 일어난다.
전이 직후 실행되는 것은 오직 새 State 의 `OnUpdate` 뿐 → 이전 State 코드가 새 State 위에서 도는 일이 없다.

### 3.1 멤버 추가 (136-138 근처)

```cpp
		TOwner *mOwner;
		TState  curState = TState::NONE;
		std::unordered_map<StateU, std::unique_ptr<IFsmState<TOwner>>> mStates;

		// 지연 전이 — TryTransit 은 검증만 즉시, 실제 OnExit/OnEnter 적용은 ApplyPending()(Update 경계).
		//   재진입 방지: OnUpdate 도중 전이가 요청돼도 이번 tick 의 OnUpdate 는 현재 State 로 끝까지 실행되고,
		//   전이는 다음 Update 의 ApplyPending 에서 원자적으로 일어난다 (Aggregate invariant 보존).
		bool   mHasPending   = false;
		TState mPendingState = TState::NONE;
```

### 3.2 `TryTransitImpl` 을 "검증 즉시 + 적용 지연" 으로 (114-134 교체)

```cpp
		bool TryTransitImpl(StateU targetBit)
		{
			const StateU curr = (StateU)curState;
			if (curr == 0)
				return false; // I1: NONE 에서는 전이 불가
			auto curIt = mStates.find(curr);
			if (curIt == mStates.end() || !curIt->second)
				return false; // current 미등록 — 계약 위반
			// I2: 현재 state 가 target 으로 갈 수 있는가? (검증은 *즉시* — ForceTransit 의 즉시 abort 의미 보존)
			if ((curIt->second->GetTransitFlag() & targetBit) != targetBit)
				return false;
			auto targetIt = mStates.find(targetBit);
			if (targetIt == mStates.end() || !targetIt->second)
				return false; // target 미등록
			// ★ 즉시 OnExit/OnEnter 하지 않는다 — pending 으로 큐잉. 같은 tick 내 복수 호출 시 마지막이 이김
			//   (모두 동일한 현재 state 기준으로 검증되므로 안전).
			mPendingState = (TState)targetBit;
			mHasPending   = true;
			return true;
		}

		/// @brief 큐잉된 전이를 안전한 경계(Update 시작)에서 적용 — OnExit(cur) → curState=target → OnEnter(target).
		/// @details I3 보존: 전이는 항상 이 순서. 단 *재진입이 아닌* 통제된 지점에서만 일어난다.
		///          OnEnter 안에서 또 TryTransit 하면 그 전이는 다음 Update 의 ApplyPending 에서 처리(연쇄 1단계/tick).
		void ApplyPending()
		{
			if (!mHasPending)
				return;
			mHasPending = false;
			const StateU from = (StateU)curState;
			const StateU to   = (StateU)mPendingState;
			auto fromIt = mStates.find(from);
			if (fromIt != mStates.end() && fromIt->second)
				fromIt->second->OnExit(*mOwner);
			curState = mPendingState;
			auto toIt = mStates.find(to);
			if (toIt != mStates.end() && toIt->second)
				toIt->second->OnEnter(*mOwner);
		}
```

### 3.3 `Update` 가 OnUpdate 앞에서 pending 적용 (106-111 교체)

```cpp
		/// @brief 매 프레임 — (1) 지난 tick 에 큐잉된 전이 적용 → (2) 현재 state OnUpdate 디스패치.
		///        OnUpdate 도중 TryTransit 가 호출되면 pending 에만 기록되어, 이번 OnUpdate 는 현재 state 로
		///        끝까지 실행되고 전이는 다음 Update 의 (1)에서 일어난다 (재진입 제거).
		void Update(float dt) override
		{
			ApplyPending();
			auto it = mStates.find((StateU)curState);
			if (it != mStates.end() && it->second)
				it->second->OnUpdate(*mOwner, dt);
		}
```

### 3.4 주석/불변식 갱신

상단 클래스 doc 의 **I3** 설명(26행 근처 "전이 시 항상 OnExit → curState=target → OnEnter 순서")에 한 줄 덧붙여,
"단 적용은 재진입이 아니라 `Update` 경계의 `ApplyPending()` 에서" 임을 명시하라. `TryTransit`/`ForceTransit`
doc 주석에도 "검증은 즉시, 적용은 지연" 을 반영.

> **주의 — `ForceTransit` 의미**: `ForceTransit` 은 `TryTransitImpl` 이 false 면 `std::abort()` 한다(74-80).
> 새 구조에서 검증(I1/I2/등록여부)은 여전히 *즉시* 수행되므로, **계약 위반이면 그 자리에서 abort** 된다(의미 보존).
> 적용만 지연된다. 현재 `ForceTransit` 호출처는 0개지만 의미는 유지할 것.

---

## 4. STEP 2 — 땜빵 가드 정리 (선택, 권장)

deferred 전이가 들어가면 §2.3 의 `IsAlive()` 가드는 **redundant** 다. 이유: 사망 프레임 F 에서
`WaveController::TryTransit(GameOver)` 는 pending 만 세팅하고, CombatPlay.OnUpdate 의 꼬리는 *여전히
CombatPlay 상태*로 `Health=0` 을 쓴다(정상). 다음 프레임 F+1 의 `Update` 가 `ApplyPending` →
`GameOver.OnEnter`(`Health=1.0`) 를 실행하고, 이후 CombatPlay.OnUpdate 는 **다시는 호출되지 않으므로**
1.0 을 덮을 코드가 없다 → LPF 가 F+1 에 풀린다(1프레임, 체감 불가).

- 권장: `StageState.Impl.h` CombatPlay.OnUpdate 의 Health 조건에서 `&& ctx->playerLife->IsAlive()` 를
  제거(원래 의도 = "HP 로 Health 갱신")하고, deferred 전이가 정합성을 보장하게 한다.
- 보수적으로 남겨둬도 **무해**하다(이중 안전). 판단해서 처리하되, 남길 경우 "deferred 전이로 이미 불필요하나
  방어적으로 유지" 주석을 달아라. **`GameOverState::OnEnter` 의 `Health=1.0` 세팅 자체는 절대 제거 금지**
  (그게 LPF 해제의 본체).

---

## 5. 스코프 경계 (Boundaries)

**소유(수정 OK):**
- `src/fsm/state_machine.h` — STEP 1 본체. (코어 `SJH::fsm` 모듈이지만 소비자는 StageStateMachine 단독 — §2.4 재확인.)
- `apps/_MyApp_/src/Stage/State/StageState.Impl.h` — STEP 2 가드 정리 한정(선택).
- (선택) `doc/superpowers/specs/2026-05-25-fsm-object-state-machine-design.md` 가 있으면 I3 timing 한 줄 갱신.

**금지(건드리지 말 것):**
- `WaveController.cpp` 의 `TryTransit(GameOver)` 호출 — **올바른 코드다.** 전이 *요청*은 맞고, 문제는 FSM 의
  *적용 타이밍*이었다. 호출부를 옮기거나 플래그로 바꾸지 말 것(그건 또 다른 땜빵).
- FMOD 배선 일체 — `AudioSystem`, `FmodStudioPlayable`, `GameContextComponent`(audio/bgmPlayable),
  main.cpp render 의 `Audio().Update(dt)` ungate, 각 State 의 BGM seam(BGM_STATE/SetPaused/Stop 제거/Health).
  **이미 동작/검증된 영역이다. 이 작업의 대상 아님.**
- 다른 State 들의 게임로직(overlay/blur/입력), `Manager::Update`, 셰이더/리소스.
- `StateMachine::OnEnter()`/`OnExit()` (FSM 자체 hook) — startup 초기 진입용. 시그니처/의미 유지.

**병렬 에이전트**: 사용자가 같은 tree 에서 병렬 작업/staging. 경로 지정 커밋만, `git add -A` 금지. 커밋은 사용자가.

---

## 6. 검증 (Verify)

1. **빌드 GREEN**:
   ```bash
   cmake --build --preset ninja --target _MyApp_
   ```
   기대: `gl3.h ... #warning`(프로젝트 정책상 무시) 외 에러 0, `Linking CXX executable ... _MyApp_` 까지 도달.
2. **런타임 청취/육안** (`cd build_ninja/apps/_MyApp_ && ./_MyApp_`):
   - Title: BGM 재생(클릭 전), 빈 화면 클릭 → CombatPlay 진입(1프레임 지연, 체감 무).
   - Pause 버튼 → 멈춤, 화면 클릭 → 재개.
   - **Player 사망 → GameOver: BGM 끊김 없이 지속 + Low Pass Filter 풀려 "깨끗"하게 들림** (← 본 버그의 합격선).
   - 전이 순간 멈춤/이중 재생/깜빡임 없음.
3. **재진입 부재 확인**: GameOver 진입 후 CombatPlay.OnUpdate 의 Health=0 쓰기가 1.0 을 덮지 않음
   (LPF 가 풀린 상태 유지). `IsAlive` 가드를 제거했어도 동일하게 동작해야 한다(STEP 2 했다면).

---

## 7. Self-review 체크리스트 (보고 전)

- [ ] `TryTransit`/`ForceTransit` 의 **검증은 즉시**(false/abort), **적용만 지연** 인가?
- [ ] `Update` 가 `ApplyPending()` → `OnUpdate` 순서인가? (반대로 하면 클릭 전이가 2프레임 지연되거나
      막 진입한 state 가 같은 tick 에 OnUpdate 까지 도는 미묘한 차이 — 의도대로 "앞에서 적용" 인지 확인.)
- [ ] OnUpdate 도중 TryTransit 후, 그 OnUpdate 의 잔여 코드가 **이전(현재) state** 기준으로만 도는가?
      전이 후 실행은 다음 Update 의 새 state OnUpdate 뿐인가?
- [ ] startup `mStageFsm->OnEnter()`(초기 Title) 정상 — pending 과 무관한가?
- [ ] 코어 `state_machine.h` 외 소비자 없음 재확인했는가? (§2.4 grep)
- [ ] Tab 들여쓰기/한국어 주석/헤더 가드 규칙 준수?
- [ ] 커밋 안 했는가? (사용자 gated)

---

## 8. 보고 (Report) — 구조화해서

- **상태**: DONE / DONE_WITH_CONCERNS / BLOCKED 중 하나
- **변경 파일** 목록 (경로 + 한 줄 요약)
- **빌드 결과**: 위 명령 출력의 마지막 줄(Linking 도달 여부)
- **STEP 2 처리**: IsAlive 가드 제거했는지/남겼는지 + 이유
- **deferred 동작 확인**: 클릭 전이/GameOver LPF 가 의도대로인지(런타임 가능했으면), 못 했으면 그 사실
- **유보/우려**: 연쇄 전이(OnEnter 내 TryTransit), 다른 소비자 등 발견 시
- ⚠ **커밋하지 말 것.** 사용자가 경로 지정 partial 커밋함. 제안 메시지 줄 거면 `Co-Authored-By` 빼고.
