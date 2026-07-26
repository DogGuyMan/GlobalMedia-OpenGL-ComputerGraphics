# 테스트 확장 설계 — 추가 CPU + GPU 테스트 (spec)

> ⚠️ **2026-07-26 정정 (원문 보존)** — 본 문서에 나오는 `SJH_GOLDEN_CAPTURE=1 ./_MyApp_` 실행과 `ctest --test-dir build_ninja -R golden` 은 *당시* 절차이며 현재는 **폐기**됐다. 골든 캡처가 런타임 환경 변수 -> 컴파일 정의로 바뀌어 프리셋 `ninja-golden`(빌드 디렉토리 `build_ninja-golden`) 전유가 됐고, 게임 빌드의 `_MyApp_` 는 골든을 캡처하지 않는다(실행해도 창만 뜨는 조용한 실패). 현행 절차 = `test/CLAUDE.md`. 아래 본문은 당시 기록으로 그대로 둔다.

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **날짜** 2026-06-27 · **base** game/remove-unused(315269a, CPU 28 통합 GREEN) · **상태** 작성 중(Phase 단락별)
> **계획 모근거** [`doc/tdd/2026-06-27-test-expansion-plan.md`](../../tdd/2026-06-27-test-expansion-plan.md) · **연구근거** doc/tdd v3 + catch2_pipeline
> ⚠ `doc/` = .gitignore 로컬.

## 0. 범위 + LOCKED 결정

기존 Track A(28 ctest/5모듈) 위에 **순수-CPU 6종(Track A+) + 골든 비교 게이트(B) + GL 단위테스트(C)** 추가. Track D(뮤테이션/퍼징)는 후속 마킹만.

| ID | 결정 | 값 |
|---|---|---|
| TX-1 | CPU 범위 | 1+2순위 − CheckIndices = **6종**: scene·playable·input·MultipleTimer·text·material |
| TX-2 | GPU 우선 | **(c) 둘 다** — 골든 게이트 + GL 단위테스트 |
| TX-3/TQ-1 | 골든 비교 | **pixelmatch-cpp17 + 차이픽셀 비율 5% 예산**(maxPixelFraction=0.05) |
| TX-4 | Track D | **후속단계 마킹만**(구현 안 함) |
| TQ-2 | 브랜치 | **현 game/remove-unused 직접**(worktree 아님, path-scoped) |
| TQ-3 | 실행 | **subagent-driven**(구현+spec리뷰+품질리뷰, anti-gaming) |

**공통 가드레일**: characterization(현 동작 잠금, red-first 아님) · 커밋 path-scoped(`git add -A` 금지) · Co-Authored-By 미사용 · 주석 한글 · sb7 무수정 · main.cpp 무침투 · 테스트는 **공개 API(헤더)에만** 바인딩(= "test behavior, not implementation") · 빌드는 사용자 게이트(구현 에이전트는 `--target tests`/대상 타깃만 자율 빌드+ctest 허용).

## 0.1 용어 표준화 (내가 coin 한 표현 → 표준 업계어)

> 본 spec 의 일부 표현은 내가 지어낸 비표준어다. 아래가 **표준 등가어**이며, 이후 문서의 짧은 라벨은 이 표준 개념을 가리킨다. (표준 매핑 근거: Feathers·Beck·Meszaros·Fowler. SUT/mock/spy·TDD·mutation score·golden=snapshot=approval 동의어는 외부 3-0 검증됨.)

| 내 coin | 표준 업계어 | 입문자 1줄 |
|---|---|---|
| **contract-anchoring** | **공개 API 기반 테스트 / "test behavior, not implementation"** (Fowler·Beck) | 내부 구현 말고 "겉으로 약속한 함수(헤더)"만 검사 → 리팩토링해도 안 깨짐 |
| **diagnostics-as-oracle** | **화이트박스 *상태 검증* (state verification)** (Fowler), 학술: instrumentation-based oracle | 결과 픽셀이 아니라 "내부 상태/진단 반환값"이 맞는지로 판정 |
| **noise floor** | **비결정성(nondeterminism) 잔차 / flakiness**; 처리는 **tolerance threshold** | 시드 다 고정해도 남는 미세한 들쭉날쭉 = 허용오차로 흡수 |
| **adequacy sanity** | **fault injection**(의도적 결함 주입) = 오라클 민감도 점검 | 일부러 버그 심어서 "테스트가 이걸 잡나?" 확인 |
| **green 체크포인트 / handoff-unit** | **milestone / checkpoint / vertical slice** | "여기까지 통과(green)면 안전하게 끊고 다음" 하는 단위 |

표준어 그대로인 것(coin 아님): characterization testing(Feathers) · golden master ＝ snapshot ＝ approval(동의어) · test oracle · mutation testing/score · fuzzing · property-based testing · fixture/mock/spy/SUT(Meszaros) · regression testing · red-green-refactor(Beck) · headless/offscreen rendering · quality gate.

---

## Phase A — Track A+ 추가 CPU 단위 테스트 (6종, GL 불필요)

**스타일**(기존 test_fsm/test_timer 답습): 익명 namespace 에 mock(spy 카운터) → 한글 `TEST_CASE(..., "[tag]")` → 부동소수 `REQUIRE_THAT(x, WithinAbs(v,1e-7f))` → 계약 1줄 주석. 각 파일 = `sjh_add_test(test_<x> SJH::<module>)` + `tests` 타깃 DEPENDS 추가.

### A-1. `test_scene.cpp` → `SJH::scene` (★1순위)
mock: spy Component(OnEnter/OnExit/OnUpdate 카운터) + 트리용 Actor.
계약(TEST_CASE):
- **AddChild 소유권+부모**: AddChild(unique_ptr) → GetParent()==부모, GetChildren().size 증가. (entered Actor 면 자식 OnEnter 1회 — 진입 경로는 헤더 확인.)
- **Detach vs Remove**: DetachChild → OnExit + 소유권 반환(포인터 유효), RemoveChild → OnExit + 파괴.
- **FindChild/FindChildIf**: 이름(재귀 on/off)·predicate DFS 탐색, 미존재 nullptr.
- **AddComponent 중복가드**: 동일 T 두 번 → 두 번째 정책대로 거부(D-9 단일 contract).
- **GetComponent fast/slow**: 구체 T 는 typeid fast-path, 인터페이스 기반 T 는 slow-path(dynamic_cast) 둘 다 적중.
- **GetWorldMatrix 부모체인**: 부모 translate(a) + 자식 translate(b) → 자식 world = a∘b (행렬 곱 순서 단언).
- **Update 순회**: enabled component 만 OnUpdate, disabled 스킵, 자식 재귀.
- ⚠ 구현검증: 루트 Actor 를 entered 시키는 공개 경로(OnEnter 직접 vs SetActive). 없으면 OnEnter 의존 케이스 축소.

### A-2. `test_playable.cpp` → `SJH::playable` (★1순위)
mock: leaf Playable(PlayableBase 상속, 지정 duration 후 finished 마킹 — elapsed 비교).
계약:
- **Base Play/Pause/Stop 리셋**: Stop → paused/finished/elapsed 0 복귀, Play → finished=false.
- **Sequence 순서**: Append(A)→Append(B), Update 누적 → A 끝나야 B 시작(Cursor 진행), 둘 다 끝→IsFinished.
- **Insert**: 중간 삽입 위치 반영.
- **Parallel Join**: 모든 child 동시 Update, IsFinished=전부 끝(가장 긴 것 기준).
- **Loop=true → IsFinished 영영 false**: 끝나면 자동 재시작([[sequence-playable-effekseer-hang]] 회귀가드 정신).
- **AppendInterval**: 지연(빈 시간) 삽입 후 다음 child.

### A-3. `test_input.cpp` → `SJH::input` (★1순위)
GLFW 상수(PRESS=1/RELEASE=0/REPEAT=2) 직접 사용, 창 불필요. PollHeld(window 인자)만 제외.
계약:
- **Keyboard Dispatch**: BindKey + BindPressHandler → Dispatch(key, PRESS) → 핸들러 1회, 미바인딩 키는 no-op.
- **Press/Release/Held 구분**: 각 action 이 해당 핸들러만.
- **Mouse HandleButton drag**: press → IsDragging true + 기준점, release → false.
- **Mouse HandleMove delta**: 연속 HandleMove → look 핸들러에 (dx,dy) 정확, CancelDrag 후 무효.

### A-4. `test_multiple_timer.cpp` → `SJH::timer` (★1순위, 기존 test_timer 갭)
계약:
- **다중트랙 Register/Tick**: 2 트랙 등록 → Tick(dt) → 각 핸들 개별 진행(서로 독립).
- **Unregister**: 제거 후 Tick 영향 없음, 핸들 조회 처리.
- **핸들 조회**: 등록 키로 Timer* 핸들 획득(값 보유 아님 — [[timer_centralization_convention]]).

### A-5. `test_text.cpp` → `SJH::text` (2순위) — ✅ **위험플래그 해소: 대부분 Phase C 이연**
> **판정 결과(2026-06-27, 리뷰 확인)**: glyph 맵(mGlyphs)을 채우는 *유일* 공개 경로 `LoadFromBMFont` 가 `bitmap_font.cpp:108` `CreateUniformAtlas`(=GL Texture)를 강제. **채워진 폰트의 조회·fallback hit·XML 파싱·V-flip 은 CPU 불가 → Phase C(GPU)로 이연.**
- **CPU 로 잠근 것(완료, 5케이스)**: 빈/기본 폰트만 — `IsValid()==false`·`Find→nullptr`·`FrameOf→-1`·`AdvanceOf→0`(fallback 최후 miss 분기)·`Glyph` POD 기본값.
- **→ Phase C 로 이연**: 채워진 폰트 codepoint 조회, '?'/' ' fallback *hit* 분기, XML 파싱, V-flip 행 보정. (§Phase C 참조.)

### A-6. `test_material.cpp` → `SJH::material` (2순위) ⚠일부위험
계약:
- **SetPass→GetQueueLayer**: Pass enum → queue int 매핑(순수 함수).
- **Properties typed map**: Set Float/Int/Vector3/Color → 동일 키 read back.
- **GetRootOriginal 체인**: OriginalMaterial 설정 가능하면 root 추적.
- ⚠ **구현검증**: `Clone` 이 private → IsInstance/OriginalMaterial 직접 set 가능한지(공개 필드 여부). 불가면 Clone 계약은 rr 경유라 Phase C 후보로 강등.
> **결과(2026-06-27)**: ✅ 11케이스 GREEN. `Clone`=private+friend → 호출 안 하고 **public 필드(IsInstance/OriginalMaterial)로 체인 직접 배선**해 GetRootOriginal 잠금. `SetProgram` EagerBuild prune 은 Program(GL) 필요라 생략.
> ✅ **latent 버그 수정됨(2026-06-28)**: `GetRootOriginal()` 이 진짜 root 에서 자기참조(`root->OriginalMaterial=root`)를 캐시해 **재호출 시 무한루프** 였던 것을 `if (p != this)` 가드로 차단(진짜 root 는 캐시 안 함, 반환값 불변). test_material 에 재호출 무한루프 가드 lock 케이스 추가.

### A-7. `test_diagnostics_cpu.cpp` → `SJH::diagnostics` (diagnostics-as-oracle 확장의 순수CPU 부분)
> ⚠ characterization 아님 — **신규계약(red-first 허용)**. diagnostics 를 로그기반 분석 오라클로 확장하며 추가되는 *구조화 반환값* 코드를 잠근다. 상세 근거·코드사실은 §「Phase A3 + diagnostics-as-oracle 영향」.
계약(착수 우선순위 순):
- **CheckIndices → DiagResult**(`size_t` → `struct DiagResult{vector<DiagFinding>, Count(), Clean()}`, 기존 wrapper 보존): OOB(`{0,1,99}`,vc=3→1건)·degenerate(`{0,1,1}`)·중복(`{0,1,2,2,1,0}`)·clean(`{0,1,2}`)·경계(size%3≠0 현동작 잠금). **순수CPU 확정**(헤더가 GL불필요 명시).
- **DiffStates(GLStateFields,GLStateFields) → vector\<FieldChange\>**(신규 순수CPU diff): 동일→빈, blend 2변경→2건+SymbolicName, 카테고리매핑(program→B/stride→C/depth_func→D). `GLStateFields`/`operator==`/`FieldsToString` 는 이미 존재, **`Diff()` 만 신규작성**.
- **ClassifyInfoLog(string_view) → {Clean,Warning,Error}**(gl_validate.cpp `containsBad` 람다 추출) + `CheckProgramLink` → `LinkReport{ok,infoLog,hasError}` 승격(분류부만 CPU, 로그수집은 GL→C-1).
- **GLErrorAccumulator**(폴링↔집계 분리한 순수 집계: Record/Counts/Total/Reset) — ⚠ `CaptureGLError` 의 폴링/전역dedup 분리 리팩토링 선행 → **후순위**.
- ⛔ **로그-텍스트 캡처 오라클 도입 금지**(전제조건 미충족): spdlog 커스텀 sink 0건(전역 콘솔 logger) + 전역logger 교체=병렬격리 취약 + 한국어 산문 brittle. 반환값 단언이 상위호환.

### ★HANDOFF #A1 (green): scene·playable·input·multiple_timer 4종 ctest GREEN.
### ★HANDOFF #A2 (green): text·material 2종 GREEN (위험 플래그 해소 결과 포함).
### ★HANDOFF #A3 (green): diagnostics-cpu(CheckIndices/DiffStates/ClassifyInfoLog) GREEN — CPU→GPU 핸드오프 *이전*.

배선 후 총 ctest = 28 + (6종 + diagnostics-cpu 신규 케이스). `cmake --preset ninja -DENABLE_TESTING=ON && cmake --build --preset ninja --target tests && ctest`.

---

## Phase A3 + diagnostics-as-oracle 영향

> 출처: 워크플로 `diagnostics-oracle-test-plan-delta`(7에이전트, 코드사실 게이트). **결론: plan 골격(Phase 수·골든 3장·CPU→GPU 핸드오프) 불변. C-1/C-4 의 *오라클 메커니즘*만 재구성 + 신규 CPU unit #A3 추가.**

### 책임 이동 (정확히 무엇이 옮겨가나)
- 이동은 **"골든픽셀/GL런타임거부 → 로그단언"이 아니라 "raw glGet\*·bool 단언 → production 진단코드(CaptureGLState/LinkReport)를 SSOT로 소비하는 구조화-반환 단언"**.
- **골든(Phase B)의 검증책임은 이동하지 않음** — 최종 픽셀 동등성은 진단으로 대체 불가. 진단은 골든 사각지대(v3 1.6-a 상태누수)를 *보완*할 뿐(직교 결함클래스).

### ★코드사실 게이트 (렌즈 과장 정정 — 적대적 자체검증)
1. **`gl_state_fields.h` 이미 존재** — `CaptureGLState()` + `GLStateFields` POD + `operator==` + `FieldsToString` + B/C/D 카테고리매핑. **production/test 공유 설계.** 이미 캡처: EBO(L96)·16 텍스처유닛(L104)·16 attribute(L130)·cull/colormask → C-4 plan 의 "4종 화이트리스트"는 *plan 텍스트 한계지 자산 한계 아님*.
2. **단 `Diff()` 는 부재(vaporware)** — docstring(L15)이 `<test>/support/gl_state_snapshot.h` 를 지명했으나 미존재. before/after diff·판정 계층은 **순수CPU 신규작성**. ("이미 diff 한다" 주장 → 거짓 강등.)
3. **spdlog 커스텀 sink 0건** — 전역 콘솔 logger 뿐(`set_default_logger`/`sinks::` grep 0). 로그-텍스트 캡처 오라클은 **전제조건(sink 주입 리팩토링 + 병렬격리 해소) 미충족 + 한국어 산문 brittle → 보류.** 반환값 단언이 상위호환.

### 잔존 진짜 사각지대
- **UBO binding point 미캡처** — `GLStateFields` 에 `GL_UNIFORM_BUFFER_BINDING` 필드 1개 추가로만 해소([[ubo-binding-point-semantic-slots]] 회귀 대응). 성능/메모리·비가시 파손은 어느 Phase 도 미커버(Phase D 영역).

### 순환위험 = 가짜 (3-노드 선형 DAG)
- "확장 → 확장의 CPU테스트 → 그걸 쓰는 GL테스트"는 순환 아님. **열쇠 = 확장의 순수CPU 부분(#A3)을 먼저 잠그고 GL테스트(C-1/C-4)는 검증된 헬퍼를 *소비만*.** 전체 선행 Phase 0 신설 금지(과설계 — plan 의 어느 Phase 도 현재 확장에 의존 안 함).
- 진짜 위험은 로그-sink 경로(전역 logger 병렬격리) → 회피(위 코드사실 3).

### Phase C 영향 (메커니즘 재구성, green 체크포인트는 동일)
- **C-1**: `program!=0` 단언 유지 + `CheckProgramLink`→`LinkReport` 승격분 소비(brittle sink캡처 회피). 단 승격해도 sampler `_N` 런타임오염은 못 잡음 = "링크가능성" 오라클 → 골든/C-3 여전히 필요.
- **C-4**: raw 4종 스냅샷 → `CaptureGLState()`+`operator==` 재사용 + 신규 `DiffStates()` 소비. EBO 오염([[vao_ebo_thirdparty_corruption]])·텍스처유닛 잔재([[slang-sampler-name-suffix]]) 검출 자동 합류.
- (상세는 §Phase C 의 C-1/C-4 항목에 인라인 반영.)

---

## 부가 항목 — 셰이더 컴파일/링크 실패 = fail-fast (production 강화, 테스트 아님)

> 사용자 추가(2026-06-27). 함정 A(varying)가 *조용히* program=null→투명 으로 넘어간 근본 원인 = **실패를 삼키고 계속 진행**. 이를 **즉시 큰 실패(fail-fast)**로 바꾼다. C-1(테스트=CI 회귀가드) + A3 `LinkReport`(구조화 반환) 와 함께 **3중 방어**의 production 축.

두 층으로 구분(중요 — "프로그램 빌드 실패"의 의미가 층마다 다름):
- **F-1 빌드타임 (slangc, CMake)**: `cmake/Slang.cmake`(`sjh_compile_slang`)가 slangc/`scripts/slang_compile.py` 컴파일 에러 시 **CMake 빌드를 실패**시키는지 검증·보강(exit code 전파, `COMMAND_ERROR_IS_FATAL`/`RESULT_VARIABLE` 체크). → 셰이더 *문법* 오류는 빌드 단계에서 막힘.
- **F-2 런타임 (GL compile/link)**: `SJH::Shader::CreateFromSource` / `SJH::Program::Create` 가 GL compile/link 실패 시 **null 반환으로 조용히 넘기지 말고 hard-fail** — `diagnostics::CheckShaderCompile`/`CheckProgramLink` 가 실패 감지 → 호출측 즉시 중단(명확한 진단 메시지 + 셰이더 경로). varying 이름불일치 같은 **링크 실패가 투명 대신 즉시 abort/throw** 로 드러남.
- ⚠ **결정 보류(구현 시점)**: 런타임 실패의 Debug vs Release 정책 — Debug=assert/abort, Release=throw vs log-and-degrade. (교수 제출/데모 빌드에서 abort 가 과한지 검토.)
- ⚠ 가드레일: 코어(shader/program/diagnostics) 변경 → SJH::engine consumer 재빌드. **공개 시그니처 영향**(반환 null→throw/abort) → 호출측 전수 점검 필수. path-scoped.
- 순서: A3(diagnostics LinkReport) 와 같은 코어 영역이라 **A3 와 함께/직후**, C-1 *이전* 권장(C-1 이 이 fail-fast 동작도 테스트로 잠금).

---

## Phase B — 골든 비교 게이트 (GPU 캡처 + CPU 비교)

**구조 결정**: 렌더링(GPU)은 기존 `_MyApp_` capture 모드 고정 재사용, **비교는 순수 CPU**(PNG 로드 + pixelmatch)로 분리. ctest 가 둘을 fixture 로 체인.

**⚠ 선결 조건**: `_MyApp_` capture 모드(env `SJH_GOLDEN_CAPTURE`, `src/Capture/golden_capture.*`)가 **현재 미커밋**(M main.cpp + A src/Capture/). Phase B 는 이 capture 포트가 브랜치에 **커밋/존재**해야 동작 → Phase B 착수 전 capture 커밋 선행(또는 동반).

### B-0. pixelmatch vendoring
- `<test>/third_party/pixelmatch.hpp` (pixelmatch-cpp17 단일헤더, ~300줄, 무의존). golden_compare 타깃만 include. 엔진/_MyApp_ 빌드 불침투.

### B-1. `test_golden_compare.cpp` → 순수 CPU Catch2 (GL 불필요)
- 링크: `Catch2::Catch2WithMain` + stb(`Stb_INCLUDE_DIR`) + `test/third_party` include. **GL 미링크.**
- 동작: 캡처 산출 PNG(`build_ninja/apps/_MyApp_/test/golden/golden_*.png`) + 커밋 골든(repo `test/golden/golden_*.png`) 로드(stb) → `MatchesGolden` 매처(pixelmatch, `maxPixelFraction=0.05`, per-pixel 기본 0.1) → 초과 시 FAIL + diff PNG 아티팩트(`build/.../artifacts/diff_*.png`) 기록.
- 경로 전달: 캡처/골든 디렉토리는 compile-def(`SJH_GOLDEN_CAPTURED_DIR`/`SJH_GOLDEN_REF_DIR`) 또는 CLI arg. CMake 가 절대경로 주입.
- 케이스 3: `golden_full` / `golden_no_imgui` / `golden_skybox` (각 `[golden]` 태그).
- `MatchesGolden` 매처: catch2_pipeline §3 패턴(`MatcherBase<Image>`, 크기 불일치 즉시 false, 차이픽셀/전체 > frac 면 false + 아티팩트).

### B-2. ctest 픽스처 체인
- `add_test(NAME golden_capture COMMAND $<TARGET_FILE:_MyApp_>)` + `set_tests_properties(golden_capture PROPERTIES ENVIRONMENT "SJH_GOLDEN_CAPTURE=1" WORKING_DIRECTORY <build>/apps/_MyApp_ FIXTURES_SETUP GOLDEN)`.
- golden_compare 의 3 케이스 → `FIXTURES_REQUIRED GOLDEN` (캡처 먼저, 비교 나중). catch_discover_tests 등록분에 property 부여.
- ⚠ `_MyApp_` 타깃 필요 → ENABLE_TESTING 시에도 apps 빌드 의존. capture 가 repo `test/golden/`(커밋본)이 아니라 **cwd=build 하위**에만 쓰는지 확인(오라클 오염 방지).

### B-3. 골든 갱신 게이트 (비협상, v3 1.3)
- 에이전트 자동 덮어쓰기 **금지**. 골든 갱신은 사람 승인(`/golden-update` 컨벤션 — 의도적 시각 변경 시에만 재생성+검토 diff 재커밋). CITYWALK §5.4 오라클 문제 회피.

### B-4. 오라클 사각지대 (v3 1.6)
- 골든 통과 ≠ GL 상태누수/성능/비가시 파손 없음 → Phase C(C4 상태누수)로 보완 명시.

### B-5. adequacy sanity (v3 2.3)
- 수동 1회: 셰이더/색에 1px 오프셋 버그 주입 → golden_compare **FAIL(kill)** 확인 → 골든 민감도 검증(살아남으면 임계 5% 가 과대).

### ★HANDOFF #B (green): capture→compare ctest 체인 동작(정상 시 PASS) + B-5 의도주입이 FAIL. CPU↔GPU 성격전환 경계.

## Phase C — GL 컨텍스트 단위테스트 (전체 C0~C4)

**별도 타깃** `gpu_tests`(`test/gpu/`) — CPU `unit_tests`(Catch2WithMain)와 분리. GL 컨텍스트 1회 init 위해 **`Catch2::Catch2`(커스텀 main) 또는 글로벌 리스너**. 링크: `project_deps`(GL/glfw) + 필요한 `SJH::<module>`.

### C-0. GL 픽스처 (전 GPU 테스트 enabler)
- **숨김 GLFW 창 + GL4.1 Core 컨텍스트 1회** 생성(글로벌, 테스트 전). EGL 아님 — 프로젝트 관행(메모리/Graphics-Testing-Prompt). `GLFW_VISIBLE=GLFW_FALSE`, GL 4.1 명시([[glsl_410_project_policy]]). 컨텍스트 생성 비싸고 스레드비안전 → 1회만.
- 헤드리스 우려 없음(Track B 에서 확인 — 창 잠깐 뜨고 자동 종료).

### C-1. 셰이더 링크검증 ★★ (독립 재컴파일)
- **CMake 사전스텝**: `apps/_MyApp_/shaders_slang/*.slang` 각각을 `cmake/Slang.cmake`(`sjh_compile_slang`) 로 GLSL410 재컴파일 → 테스트 출력 dir. **post-process 정규화는 `scripts/` SSOT 재사용**([[slang-glsl410-traps]] — varying/sampler `_N`/배열 brace 3대 함정 교정은 slang_compile.py 가 SSOT, **재구현 금지**).
- **테스트**: 산출 `.vert/.frag` 쌍 glob → `SJH::Shader::CreateFromFile` + `SJH::Program::Create`(또는 CreateWithVSFS) → `GetProgramAddr() != 0` 단언. **+ 링크로그는 #A3 의 `CheckProgramLink`→`LinkReport{ok,infoLog,hasError}` 승격분을 소비**(spdlog sink 텍스트 캡처 brittle 회피 — §Phase A3 코드사실 3). 로그 *수집*만 GL(여기), 분류는 #A3 의 CPU `ClassifyInfoLog`.
- **가치**: glslang 통과·macOS GL 런타임만 거부(은밀한 투명)를 빌드 GREEN 단계에서 못 잡는 갭을 메움. 메모리 3건([[slang-varying-name-mismatch]]·[[slang-sampler-name-suffix]]·[[slang-glsl410-traps]]) 직격. 단 "링크가능성" 오라클이지 sampler `_N` 런타임오염 같은 "올바른바인딩"은 못 잡음 → 골든/C-3 보완 필요.
- ⚠ "독립"=앱 POST_BUILD 비의존(테스트가 컴파일 구동)이되 **toolchain/post-process 는 프로젝트 SSOT 재사용**(별도 구현 시 함정 재현 실패).

### C-2. DeviceContext 상태캐시 (GL)
- `ApplyRenderStateBlock(want)` dirty-check + **blend func 캐시 기본값 불일치**([[blend-func-cache-default-mismatch]]: GL 기본 ONE,ZERO ≠ 캐시 기본 SRC_ALPHA,ONE_MINUS → off→on 전이서 func 강제 안 하면 Transparent 알파 무시).
- 단언: off→on 전이 후 `glGetIntegerv(GL_BLEND_SRC_*)` == 기대값. `InvalidateStateCache` 후 재적용 강제.

### C-3. roundtrip (GL)
- **buffer**: `CreateWithData` → `glGetBufferSubData` → 입력 데이터 일치.
- **texture**: `CreateTexture(Image)` → `glGetTexImage` → 픽셀 일치.
- **program**: 알려진 uniform `GetLocation` != -1.

### C-4. GL 상태누수 (v3 1.6, diagnostics-as-oracle 최대 수혜)
- raw 4종(VAO/program/blend/depth) 직접 스냅샷 **금지** → **`CaptureGLState()`(production SSOT, 이미 EBO·16텍스처유닛·16attribute·cull/colormask 캡처) 재사용** + `operator==` + 신규 `DiffStates()`(#A3 의 CPU diff) 소비.
- 패스 1회 렌더 전/후 `CaptureGLState()` 비교 → DiffStates 가 비어야 PASS, 기대 변경은 화이트리스트 문서화, 그 외 누수 FAIL. EBO 오염([[vao_ebo_thirdparty_corruption]])·텍스처유닛 잔재([[slang-sampler-name-suffix]]) 자동 합류.
- ⚠ 잔존 사각지대: UBO binding point 미캡처 → `GLStateFields` 에 `GL_UNIFORM_BUFFER_BINDING` 필드 추가 필요([[ubo-binding-point-semantic-slots]]).
- ✅ **latent 버그 수정됨(2026-06-28)**: `CaptureGLState` 가 GL4.1 Core 의 VAO=0 에서 `glGetVertexAttribiv` → GL_INVALID_OPERATION 내던 것을 `if (f.vao != 0)` 가드로 차단(VAO=0 면 attribute query skip). test_state_leak 의 ScopedVAO 워크어라운드 제거 = 검증.

### C-5. text BitmapFont GPU 부분 (Phase A 에서 이연됨)
- `LoadFromBMFont`(GL Texture 강제, `bitmap_font.cpp:108`) 로 실제 폰트 로드 후: 채워진 폰트 codepoint 조회·'?'/' ' fallback *hit* 분기·XML 파싱(scaleW/lineHeight/cell)·V-flip 행 보정 단언. (C-0 GL 픽스처 위에서. 리소스: 기존 `_MyApp_` 가 쓰는 BMFont 에셋 재사용.)

### ★HANDOFF #C1 (green): `gpu_tests` 빌드 + C-0 픽스처 + **C-1 셰이더 링크검증** GREEN.
### ★HANDOFF #C2 (green): C-2/C-3/C-4 GREEN.

---

## Phase D — 적대적/뮤테이션 (🔴 **보류 / HOLD** — 미구현, 착수 보류)

> 🔴 **2026-06-28 보류 확정 (사용자).** Mutation Test(Mull) + 퍼징은 **지금 하지 않는다.** A·B·C 완료 후 이 effort 는 종료하고, Phase D 는 별도 사용자 승인 시 재개. 무거운 툴체인(Clang+Mull LLVM 플러그인) 선행 검증 필요. **아래는 미래 작업 기록일 뿐 — 실행 대상 아님.**

- **D-1 Mull `gitDiffRef` 뮤테이션 게이트**: CPU 테스트(Track A/A+)가 실제 mutant 죽이는지(테스트 품질) 검증. Clang + `-fpass-plugin=mull-ir-frontend-N` 전용 빌드 + `mull-runner-N` + `mull.yml`(excludePaths=Catch2/third_party/extras, **GPU 골든 제외** — Mull 은 결정적 CPU 로직만). PR 변경라인만(실험적). catch2_pipeline §2.
- **D-2 RapidCheck**(`<rapidcheck>/catch.h`) 프로퍼티 + **libFuzzer** 타깃: **BMFont XML 파서**가 1순위 퍼징 후보 → 크래시 → `[regression]` Catch2 케이스 전환.
- **D-3 LLM 보조 테스트 초안**(Qwen-Coder): Mull 점수로 게이트, 생존 mutant 죽일 때만 채택. 최후, 사람 감독.

착수 시 의존: Clang+Mull 툴체인 설치 검증(catch2_pipeline §6 — LLVM 13~22, 버전접미 바이너리). **현재 안 함.**

---

## ★ HANDOFF-UNIT 지도 (메모리 `plan-handoff-boundary-markup`)

| unit | green 체크포인트 | 성격 | 비고 |
|---|---|---|---|
| #A1 | scene·playable·input·multiple_timer ctest GREEN | CPU 순수 | |
| #A2 | text·material GREEN | CPU 순수 | 위험플래그(text GL결합/material Clone) 해소 결과 |
| #A3 | diagnostics-cpu(CheckIndices→DiagResult/DiffStates/ClassifyInfoLog) GREEN | CPU 순수 | 신규계약(red 허용). 코어 변경 → SJH::engine consumer 재빌드. C-1/C-4 가 소비 |
| **★HANDOFF (CPU→GPU)** | A(A1~A3) 완료 → 핸드오프 (성격전환 필수분리) | | |
| #B | capture→compare ctest 동작 + B-5 의도주입 FAIL | GPU캡처+CPU비교 | ⚠ capture 미커밋 선결 |
| #C1 | gpu_tests + C-0 픽스처 + C-1 셰이더링크(+LinkReport 소비) GREEN | GPU/GL | ★최고가치 |
| #C2 | C-2/C-3/C-4(CaptureGLState+DiffStates 소비) GREEN | GPU/GL | |

## 실행 (TQ-3 subagent-driven)
- 각 unit = 구현 에이전트(격리 없음, 현 브랜치 path-scoped) → spec리뷰 + 품질리뷰(anti-gaming, 역할분리). [[agent-orchestration-anti-gaming]].
- 빌드: 구현 에이전트가 `--target tests`(CPU) / `gpu_tests`(GPU) / `_MyApp_`(capture) 자율 빌드+ctest. 전체 커밋은 사용자 게이트.
- 의존순서: A1 → A2 → **A3(diagnostics-cpu) + F(셰이더 fail-fast, 같은 코어 영역)** → (capture 커밋) → B → C1 → C2. A 와 B/C 는 GL 의존도 달라 병렬 가능하나 컨텍스트 분리 위해 순차 권장.
- ⚠ A3 는 **코어 모듈(SJH::diagnostics) 변경** 동반(구조화 반환 추가) → SJH::engine 우산 consumer 재빌드 대상. 공개 시그니처로만(내부 sReportedErrors/dedup 비노출 유지), path-scoped(src/diagnostics/ + test/ 분리), 로그-sink 도입 금지.

