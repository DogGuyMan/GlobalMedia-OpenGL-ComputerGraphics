# HANDOFF — 간헐적 SIGSEGV: Scene::Actor::Update 순회 중 dangling Actor (use-after-free 의심)

> **수신자:** 신규 Claude Code 세션/에이전트 (이 대화 컨텍스트 없음 가정). 자기완결.
> **작성:** 2026-06-21 · **상태:** 🔴 미해결 버그. 원인 *미확정* — 본 문서는 **증거 + 재현 + 가설** 만 비약 없이 전달. 수정 미착수.
> ⚠ **gitignore 로컬** (`doc/` 는 .gitignore).
> **제안 스킬:** `superpowers:systematic-debugging` (먼저 호출), 그다음 코드 정독.

---

## 0. 한 줄 요약
게임 시작 직후 'R' 궁극기(회전 히트스캔 레이저) 사용 시, `SJH::Scene::Actor::Update` 의 자식 순회 중 **이미 해제된(또는 손상된) Actor 포인터를 역참조**해 SIGSEGV. main 스레드, 매 프레임 Update 경로. **렌더(`render()`) 경로 아님.**

---

## 1. ★ FACT — 크래시 증거 (크래시 리포트 2회, 동일 시그니처)

두 리포트(`incident 59022D98...` 04:36, `8F25CA80...` 04:46) 의 스택·폴트 양상이 **완전히 동일**.

### 1.1 예외
- `EXC_BAD_ACCESS (SIGSEGV)`, `KERN_INVALID_ADDRESS`.
- 폴트 주소: 리포트1 `0x2651d8`(≈2.5M), 리포트2 `0x668ac`(≈420K). **둘 다 작은 절대주소** (정상 힙/텍스트 영역 아님 — "Bytes before following region" 으로 매핑 안 됨).
- `esr: 0x92000006 (Data Abort) byte read Translation fault` — **매핑 안 된 메모리 읽기**.
- Triggered by Thread 0, `com.apple.main-thread`.

### 1.2 크래시 스택 (Thread 0, 양 리포트 동일)
```
0  SJH::Scene::Actor::Update(float) + 28        <- 폴트 지점 (함수 진입 직후)
1  SJH::Scene::Actor::Update(float) + 292       <- 자식 Update 재귀
2  SJH::Scene::Actor::Update(float) + 292       <- 자식 Update 재귀
3  SJH::Scene::Director::Update(float) + 32
4  TopdownShooter::Stage::CombatPlayState::OnUpdate(SJH::Scene::Actor&, float) + 56
5  SJH::FSM::StateMachine<TopdownShooter::Stage::EStageStatus, SJH::Scene::Actor>::Update(float) + 192
6  TopdownShooter::game_application::render(double) + 884
7  sb7::application::run + 824
8  main + 72
```
- 스택에 **`src/render/` / Effekseer / FMOD / GL 프레임 0개**. 전부 scene/Stage/FSM Update 경로.
- `Actor::Update` 가 **자기 자신을 2단계 재귀** (root -> child -> grandchild) 후 grandchild 의 `+28` 에서 폴트.

### 1.3 레지스터 증거 (리포트2)
- `x0 = 0x66828`, `x8 = 0x66828`, `far = 0x668ac` (= `0x66828 + 0x84`). → `Actor::Update+28` 이 `this`(≈`x8=0x66828`)의 멤버(offset 0x84 부근)를 읽다 폴트. **`this` 자체가 쓰레기 작은 값** = 손상/해제된 Actor 포인터.
- crash 스레드 레지스터 컨텍스트에 심볼 `std::vector<std::unique_ptr<SJH::Scene::Actor>>::__add_alignment_assumption<...>` 노출 → **`Actor::Update` 가 `vector<unique_ptr<Actor>>`(자식 컬렉션)을 순회**하는 코드임을 시사.

### 1.4 직전 로그 (리포트1)
```
[Wave 1] Enemy spawned at (-9.4,9.5)
./shell/CMakeExecute.sh: line 60: 16675 Segmentation fault: 11
```
→ **Wave 1 적 스폰 직후** 크래시.

### 1.5 재현 (사용자 보고 — 신규)
- **게임 시작하자마자 즉시 'R' 키(궁극기) 사용 시 발동.** 간헐적(타이밍 의존).

---

## 2. ★ FACT — 무엇이 이 크래시와 *무관*한지 (근거 포함)
- **render GL-state 통합(Phase A, 2026-06-21) 과 무관.** 근거: ①크래시 스택에 `src/render/` 프레임 0개 (크래시는 `render()` 가 호출하는 *Update 단계*, GL draw 단계 아님). ②Phase A 변경은 `src/render/`(device_context/mesh_pass/render_stage) GL state + `Pass::Kind::Screen` + ParticleStage 1줄뿐 — `scene/`·`Actor`·spawn·despawn 미접근. ③Phase A 코드는 `DeviceContext::mLast`(고정크기 싱글톤 멤버)에만 쓰고 힙/Actor 미접근 → 메모리 손상 유발 경로 없음.
- ⚠ **단, "Phase A 이전부터 있던 버그"라고 단정할 증거는 없음** (작성자가 pre-Phase-A 빌드에서 재현 확인 안 함). 다음 에이전트가 git 이전 커밋 빌드로 재현해 도입 시점을 확정할 것. 현재 말할 수 있는 것은 "다른 서브시스템 + Phase A 가 건드린 코드 아님" 까지.

---

## 3. INFERENCE (추정 — 검증 필요, 단정 아님)
증거(§1)로부터의 합리적 추정:
- **use-after-free / dangling pointer**: `Actor::Update` 가 자식 `vector<unique_ptr<Actor>>` 를 순회하며 `child->Update(dt)` 호출 → 그 자식 포인터가 이미 해제됨(쓰레기 `this=0x66828`) → 폴트.
- 가장 그럴듯한 메커니즘: **Update 순회 *도중* 어떤 Actor 가 씬 트리에서 동기 제거(RemoveChild/소멸)되어 순회 중인 컬렉션이 무효화**됨 (iterator/index invalidation 또는 해제된 노드 재방문).
- 이는 [[box2d_step_lock_defer_body_changes]] 와 **동일 계열 함정** — "콜백(Step/Update) 도중 body/Actor 변경 금지, sweep 으로 defer". 적 사망 경로가 Update 밖 sweep 으로 defer 안 되고 Update 안에서 즉시 RemoveChild 하면 발생.

---

## 4. 사용자 제시 가설 2종 + 증거 적합성 (작성자 판단, 검증 필요)
| 후보 | 내용 | 증거 적합성 (작성자 의견 — 미확정) |
|---|---|---|
| **후보 2** | R 히트스캔이 *방금 스폰된* Wave1 EnemyEntity 를 생성 직후 즉사시킬 때 | **더 일치** — 크래시가 Actor 순회 + 해제된 *Actor* this + "Wave1 spawn 직후" 로그 + 적 사망=Actor 제거 경로. |
| 후보 1 | R Effekseer(궁극기 VFX) 종료 시 | **덜 일치** — Effekseer 종료라면 VFX/Effekseer 프레임이 스택에 보일 가능성이 큰데 *0개*. 단 VFX 가 Actor lifetime 에 얽혀 있으면 간접 영향 가능 — 배제는 아직 못 함. |

⚠ 둘 다 *가설*. R 궁극기는 **회전 히트스캔 레이저**(근거: `PlayerBuilder.cpp:337` `SetWorld(...) // R 궁극기 회전 히트스캔 레이저 레이캐스트용`). R 사용 시 즉시 다수 적에게 데미지 → 방금 스폰된 적의 즉사 경로가 Update 도중 발화하면 §3 메커니즘 성립.

---

## 5. 다음 에이전트가 볼 곳 (조사 진입점 — 작성자가 *읽지 않은* 파일 포함, 비약 방지 위해 직접 확인 요)
1. **`src/scene/actor.cpp` `Actor::Update`** — 자식 순회 방식 확인. index 기반인가 iterator 기반인가? 순회 *중* 자식 add/remove 를 허용/방어하는가? (재진입 안전성). `+28`/`+292` 오프셋 = 진입부 멤버 읽기 / 자식 루프.
2. **`SJH::Scene::Director::Update`** — root 부터 Update 시작점.
3. **R 궁극기 사망 경로**: `apps/_MyApp_/src/Spawns/UltimateLaser.cpp` (히트스캔) → 적 `Life::DoDamaged`/`DoDie` → Actor 제거가 **Update 안에서 즉시(RemoveChild)인지, sweep 으로 defer 인지**. 정본 패턴 = WaveController `OnEnemyDeath`↔`SweepDespawned` ([[box2d_step_lock_defer_body_changes]], [[stage-fsm-implemented]]). 즉시 제거면 그게 원인.
4. **`Actor::RemoveChild` / 자식 vector 소유** — Update 순회 중 호출 시 vector 재할당/원소 삭제로 dangling.
5. 재현: 게임 시작 + 즉시 R (간헐적이라 여러 번). `MallocStackLogging=1 leaks` 또는 ASan 빌드로 use-after-free 직접 포착 권장.

## 6. 검증·수정 가드레일
- 커밋·빌드는 **사용자 직접** ([[user-parallel-git-and-builds]], path-scoped, `Co-Authored-By` 미사용, 메시지 한국어).
- 빌드: `export PATH="$HOME/slang/bin:$PATH" && cmake --build --preset ninja --target _MyApp_`.
- 수정 방향(가설 확정 시): Actor 제거를 Update 순회 *밖* sweep 으로 defer (이미 box2d body 변경에 쓰는 패턴과 통일), 또는 Update 순회를 제거-내성(스냅샷/인덱스 가드)으로.
- **테스트 자동작성 금지** ([[no_auto_tests]]).

## 7. 첨부 — 원본 크래시 리포트
두 리포트 전문은 본 핸드오프를 띄운 대화에 붙여져 있었음 (incident `59022D98-3EB3-47E9-AE4D-3F819D17DDF1`, `8F25CA80-E319-4214-B946-34D0A4ABE0ED`). 필요 시 사용자에게 재요청. 핵심 발췌는 §1 에 비약 없이 전사함.
