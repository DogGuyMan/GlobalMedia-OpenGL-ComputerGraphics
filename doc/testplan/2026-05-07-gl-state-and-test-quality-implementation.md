# GL State Introspection + Test Quality Monitoring — Implementation Plan

> **재작성: 2026-05-09** — 이전 위치(`docs/superpowers/plans/`) 소실 후 `doc/testplan/`로 이동. 경로 참조 갱신.

> **For human implementer (사용자)**: 본 프로젝트의 auto memory `phase-implementation-mode` 정책에 따라 사용자가 직접 코드를 작성합니다. 본 plan의 코드 블록은 *TDD red phase 테스트*는 정확한 그대로 사용 가능, *구현 코드*는 컨벤션 가이드라인이며 변형 가능합니다. 매 task 끝 commit 단계는 **사용자 명시 요청 시에만** 수행 — 그 외엔 stage만.

**Goal:** GL 상태 불가시성을 해소하는 4개 신규 테스트 인프라 컴포넌트(GLStateFields/GLStateLog/GLStateSnapshot/SpdlogCapture)를 TDD로 추가하고, 테스트 자체의 결함성 감시(smell linter + 사보타지 드릴 운영)를 정착시킨다.

**Architecture:** Production측 `SJH::Diagnostics::GLStateLog`와 테스트측 `SJH::test::GLStateSnapshot`이 공통 데이터 모델 `GLStateFields`를 공유한다. 둘 다 `FieldsToString` 자유함수에 위임해 출력 포맷을 DRY하게 유지. spdlog 캡처 헬퍼는 RAII로 default logger를 ostringstream sink로 잠시 교체, 기존 `test_uniform_diagnostics.cpp`의 SUCCEED-only 스모크 테스트를 행동 단언으로 교체한다.

**Tech Stack:** C++17, glad, GLFW (headless context via 기존 `GLContextFixture`), spdlog (ostringstream sink), Catch2 v3, CMake + Ninja, Python 3 (smell linter)

**Spec:** [2026-05-07-gl-state-and-test-quality-design.md](2026-05-07-gl-state-and-test-quality-design.md)

---

## Implementation Notes — Plan 결함 누적 + 강화 (Task 1-7 진행 중 발견)

> 본 plan을 Task 1-7 시점에 따라가던 중 발견된 결함 14개를 카테고리별로 정리. Task 8-9 이후 진행 시 *Pre-flight checklist* (§N0)를 먼저 체크. **각 결함마다 Task 번호 명시 — plan 신뢰도 추적용**.

### N0. Pre-flight Checklist (각 Task 시작 시 확인)

Task 1-7에서 *반복 발생*한 함정을 사전 차단:

```
□ CMakeLists.txt 편집 후 → cmake --build … --target tests (ALL 아님)
□ 새 test executable 추가 시 → tests umbrella DEPENDS 목록에도 추가
□ ctest 필터는 -R "<시나리오 이름 substring>" (태그 substring 아님)
□ 신규 코드/케이스가 STATIC_REQUIRE 사용 시 → smell linter regex 갱신 확인
□ macOS GL 3.3 core profile 가정 검증 (Retina, VAO=0 driver-dependent 등)
□ Catch2 매크로(SECTION, REQUIRE_THROWS) 사용 시 → linter regex에 포함됐는지 확인
□ 캡처 시점에 process-wide static 상태 누설 가능성 (UniformDiagnostics 등)
□ Test program 핸들이 다른 테스트와 충돌하는지 (전역 static 사용 시)
```

---

### 카테고리 A — CMake / 빌드 시스템 함정

#### N1 [Task 1, 2, 3, 5, 6]: `cmake --build build_Darwin -j`만으로는 신규 add_executable이 ALL에 안 잡힘

**증상**: `[100%] Built target OpenGL-With-CMake` 만 나오고 신규 testexe는 컴파일조차 안 됨. ctest는 placeholder `test_xxx_NOT_BUILT-XXXX` 만 봄.

**원인**: 본 프로젝트의 ALL 타겟이 *기존 캐시된 타겟 목록*만 빌드. 새 add_executable이 ALL에 자동 편입 안 되는 CMake 캐시 동작.

**해결 — 빌드 명령은 두 패턴 중 하나**:
```bash
# 패턴 A (권장): tests 우산 — 모든 테스트 포함
cmake --build build_Darwin -j --target tests
# 패턴 B: 특정 테스트만
cmake --build build_Darwin -j --target test_<name>
```

**적용 완료**: plan의 모든 build 명령에 `--target tests` 명시 (13곳).

#### N5 [Task 5, 6]: incremental build cache가 stale binary 유발

**증상**: 헤더 변경 후 `cmake --build` 했는데 옛 동작이 그대로. `_NOT_BUILT-` placeholder가 ctest에 잔존.

**원인**: CMake가 일부 변경(특히 test 시나리오 이름 변경)을 의존성 추적 못함. `touch`나 `cmake --preset debug` 재실행 필요.

**해결 — 의심 시**:
```bash
touch <변경한 .cpp 파일>          # mtime 갱신 → 강제 recompile
# 또는
cmake --preset debug               # 재구성으로 의존성 그래프 갱신
```

**보강**: 사용자 [auto memory edit-tool-mtime-stale-binary](memory/) 정책과 정렬 — *동작 불일치 시 빌드 신선도 의심 우선*.

#### N6 [Task 2, 5, 6]: Tests umbrella DEPENDS 목록 누락 누적

**증상**: 신규 test_gl_state_capture / test_gl_state_log / test_gl_state_snapshot 추가 시 plan에 *"umbrella에 추가"* 명시했으나 사용자가 매번 확인 필요.

**원인**: 각 Task의 CMake 수정 단계가 *executable 등록*과 *umbrella 등록*을 별개 step으로 분리하지 않음.

**해결 — Pre-flight checklist에 명시 + Task 마지막 step에서 umbrella 확인 강제**.

---

### 카테고리 B — macOS GL 3.3 core profile 한계 (plan의 가정 오류)

#### N7 [Task 2]: macOS Retina HiDPI → viewport 직접 비교 깨짐

**증상**: `REQUIRE(f.viewport[2] == 256)` FAIL — actual 512.

**원인**: GLFW logical 256×256 → physical framebuffer 512×512 (HiDPI 2x scaling). plan은 logical 좌표 가정.

**해결 — plan 패턴 변경**: 절대값 비교 대신 *capture가 actual GL 상태를 반영*하는 형식:
```cpp
GLint actual_vp[4] = {};
glGetIntegerv(GL_VIEWPORT, actual_vp);
auto f = CaptureGLState();
REQUIRE(f.viewport[2] == actual_vp[2]);  // 자체 일관성 검증
REQUIRE(f.viewport[2] > 0);              // sanity
```

#### N8 [Task 2]: VAO=0 상태에서 attribute query는 driver-dependent

**증상**: `REQUIRE(a.size == 4)` FAIL — actual 0. (plan은 GL spec default 4 가정)

**원인**: macOS GL 3.3 core profile은 default VAO=0이 valid 하지 않음 → `glGetVertexAttribiv` 반환값이 driver-dependent.

**해결**: VAO=0 케이스에서는 `enabled == false`만 안전 invariant. 다른 필드의 spec default 검증은 *VAO 바인딩 후* 별도 케이스로 분리.

#### N9 [Task 2]: GL_FLOAT 타입의 normalized 플래그는 driver-dependent

**증상**: `glVertexAttribPointer(1, 2, GL_FLOAT, GL_TRUE, ...)` 후 capture에서 `normalized = false`.

**원인**: GL spec — "GL_FLOAT 타입에서는 normalized 플래그 ignored". Apple 구현은 false 반환.

**해결**: normalized 검증은 정수 타입 (`GL_UNSIGNED_BYTE`, `GL_INT` 등)으로만. GL_FLOAT 케이스에서는 normalized 단언 X.

---

### 카테고리 C — ctest / Catch2 인터페이스 함정

#### N2 [Task 1]: `-R` 정규식은 *태그*가 아니라 *Catch2 시나리오 이름* 매치

**증상**: `-R "state_fields"` (태그 substring) → `No tests were found!!!`.

**원인**: `catch_discover_tests`는 TEST_CASE 첫 번째 인자(시나리오 이름)을 ctest 이름으로 등록. 두 번째 인자(태그)는 ctest -R로 매치 안 됨.

**해결**: 시나리오 이름의 substring 사용 (5곳 수정):

| Task | 정규식 |
|---|---|
| 1 | `-R "SymbolicName"` |
| 2 | `-R "CaptureGLState\|fresh fixture"` |
| 4 | `-R "UniformDiagnostics"` |
| 5 | `-R "GLStateLog\|FieldsToString"` |
| 6 | `-R "Diff\|ToString\|snapshot"` |

또는 *전체 실행*: `ctest --test-dir build_Darwin --output-on-failure`.

#### N10 [Task 7]: smell linter regex가 `STATIC_REQUIRE`/`STATIC_CHECK` 누락 → false positive

**증상**: `test_glfw_utils.cpp` (constexpr STATIC_REQUIRE 사용)에 R1 false positive 6건.

**원인**: plan의 `ASSERT_RE`가 일반 `REQUIRE`/`CHECK`만 처리. compile-time 단언 매크로 누락.

**해결**: regex에 `STATIC_REQUIRE(?:_FALSE)?|STATIC_CHECK(?:_FALSE)?` 추가.

**교훈**: 새 Catch2 매크로 도입 시 linter regex 갱신 의무. Pre-flight checklist에 명시.

---

### 카테고리 D — Plan code 자체의 잘못된 가정

#### N3 [Task 2]: Stub 상태 "Expected: FAIL" 설명이 부정확

**증상**: Task 2 Step 3에서 5개 중 3 PASS / 2 FAIL (plan은 "결정성만 PASS, 나머지 FAIL" 모호 설명).

**원인**: stub `CaptureGLState() { return {}; }`은 *아무 GL 호출도 안 함* → 부수효과 0 검증과 GL_NO_ERROR 검증이 *우연히* 통과.

**해결 — Task 2 Step 3에 정확한 표 박음**:

| 시나리오 | stub 결과 | 이유 |
|---|---|---|
| "결정성 — byte-equal" | **PASS** | 둘 다 default 반환 |
| "부수효과 0" | **PASS** ⚠️ | GL 호출 안 함 (우연) |
| "후 GL_NO_ERROR" | **PASS** ⚠️ | 동일 (우연) |
| "fresh fixture default" | **FAIL** | viewport mismatch |
| "VAO 바인딩 후 반영" | **FAIL** | f.vao=0, 실제 vao=1+ |

**교훈**: stub이 *우연히 통과시킨* 케이스는 *진짜 회귀 감지력이 약한* 케이스라는 신호. Task 9 사보타지 드릴이 추적용.

#### N11 [Task 5]: SymbolicName 사전에 vertex attribute type enum 9개 누락

**증상**: `attrib[0]: vec3 0x1406, ...` (GL_FLOAT의 hex로 출력).

**원인**: audit 트랙 A 추가 시 `attribute_layouts.type` 필드 추가했지만, 그 값을 `SymbolicName`이 처리하도록 사전 확장은 빼먹음. plan 자체의 누락.

**해결**: SymbolicName에 9개 추가 — `GL_BYTE`, `GL_UNSIGNED_BYTE`, `GL_SHORT`, `GL_UNSIGNED_SHORT`, `GL_INT`, `GL_UNSIGNED_INT`, `GL_FLOAT`, `GL_DOUBLE`, `GL_HALF_FLOAT`. 사전 28 → **37개**.

**교훈**: 새 enum field 추가 시 SymbolicName 사전 *체크리스트*가 동시에 갱신 필요. 한 곳에 박혀있는 enum이 다른 곳에서 raw hex로 새는지 확인.

---

### 카테고리 E — Audit 미흡 (외부 통증 분석 누락)

#### N4 [Task 2 시점에 흡수]: BugReport + STUDY_NOTE 통증 5개 중 1.5개만 cover

**증상**: 사용자가 audit 요청 → 11 카테고리 50+ 패턴 중 현재 plan은 attribute layout / uniform 값 누락 / shader version 등 중요 카테고리 미커버.

**원인**: 본 plan의 spec 단계에서 BugReport.md / STUDY_NOTE.md 분석을 충분히 하지 않음.

**해결 — 3 트랙 분류**:
- **트랙 A (현재 plan 흡수)**: vertex attribute layout (카테고리 C) → GLStateFields::attribute_layouts 추가
- **트랙 B (Task 5/6/9에 영향)**: FieldsToString·Diff·사보타지 드릴 갱신
- **트랙 C (sibling spec 후보)**: shader, camera, uniform setter, C++ lifecycle, visual regression

**적용 완료**: [bug-coverage-audit.md](bug-coverage-audit.md) 작성 + Task 1-2 자동 확장 + Task 5/6 audit 출력 흡수.

---

### 카테고리 F — Test 격리 / 결정성

#### N12 [Task 4]: Process-wide static 상태가 테스트 간 누설

**증상**: `UniformDiagnostics::warnedMissing/warnedTypeMismatch`가 `unordered_map` 전역 static. 같은 program 핸들을 여러 테스트가 쓰면 *후속 테스트에서 warn-once 잘못 동작*.

**원인**: plan은 단순 `using SJH::Diagnostics::UniformDiagnostics;` 만 명시. 전역 static의 cross-test contamination 미인식.

**해결 — Task 4 modified plan**:
1. 각 TEST_CASE 시작/종료 시 `Invalidate(handle)` 호출 (정리)
2. 테스트별 *unique* program 핸들 사용 (충돌 회피): 42→142, 99→199, 7→207

**교훈**: 전역 static을 사용하는 production 모듈을 테스트할 때는 *fixture 또는 RAII 정리 메커니즘* 필요. Catch2의 `--order rand` 옵션 사용 시 더 중요.

#### N13 [Task 4]: Test program 핸들 충돌

**증상**: 기존 SUCCEED-only는 단언 없으니 충돌 무관. 행동 단언 도입 후 *test 1의 NotifyMissing(42)이 test 2의 NotifyMissing(42) silent로 만듦*.

**원인**: N12와 동일 — plan의 핸들 선택이 충돌 회피 미고려.

**해결 — 위와 동일 (unique 핸들 사용)**.

---

### 카테고리 G — 정책 / 워크플로우

#### N14 [Task 3-7 진행 중]: Phase-implementation-mode + 명시 권한 부여 패턴

**관찰**: 사용자 [auto memory phase-implementation-mode](memory/) 정책 ("Claude는 명시 요청 시 review/문제 발견만") + Task별 *"자동 진행해"* 권한 부여 = 하이브리드 워크플로우.

**해결 — plan의 commit 정책**:
- Plan이 권장 commit 메시지 박음, 단 *commit은 사용자 명시 요청 시에만 실행*.
- "stage" 단계까지만 자동, `git commit` 자동 X.

**교훈**: 정책 일반론 vs 그 자리 권한 부여를 구분. 사용자가 *"자동 진행"* 명시할 때마다 그 Task 한정으로 phase-mode override.

---

### 결함 → Task 매핑 표

| Task | 발견된 결함 | 결과 |
|---|---|---|
| 1 | N1, N2 | -R / build target 정상화 |
| 2 | N1, N3, N6, N7, N8, N9, N4 audit 흡수 | macOS 환경 적응 + audit 트랙 A 흡수 |
| 3 | (없음 — STATIC lib만) | 깔끔 |
| 4 | N12, N13 | 전역 static 누설 / handle 충돌 해소 |
| 5 | N1, N5, N6, N11 | SymbolicName 사전 + umbrella 보강 |
| 6 | N1, N5, N6 | umbrella 보강 |
| 7 | N10 | STATIC_REQUIRE regex 보강 |

**총 14 결함 / 7 카테고리 / 0 회귀** — 모든 결함이 plan에 흡수됨, 86/86 PASS 유지.

---

### audit 트랙 C (sibling spec 후보 — Task 9 완료 후 작성)

본 plan 외에 BugReport + STUDY_NOTE 패턴 중 *다른 도구가 필요한* 카테고리:

| 후보 spec | 잡는 카테고리 | 우선순위 |
|---|---|---|
| `2026-05-?-shader-source-static-checks-design.md` | A1 (GLSL #version), J (셰이더 내용) | 中 |
| `2026-05-?-camera-math-unit-tests-design.md` | G (Camera/Matrix) | 高 |
| `2026-05-?-uniform-setter-instrumentation-design.md` | D4/D5 (setter 호출 자체 검증) | 高 |
| `2026-05-?-cpp-lifecycle-clang-tidy-design.md` | F (C++ lifecycle) | 中 |
| `2026-05-?-visual-regression-track-b-design.md` | K (기하), J 시맨틱 | 高 |

상세는 [bug-coverage-audit.md §3](bug-coverage-audit.md) 참조.



### N1: `cmake --build build_Darwin -j`만으로는 신규 테스트 executable이 안 빌드됨

**증상**: Task 1/2의 Step "빌드 + 테스트" 실행 시 출력에 `[100%] Built target OpenGL-With-CMake` 만 나오고 신규 `test_xxx` executable은 *컴파일조차 안 됨*. ctest는 placeholder `test_xxx_NOT_BUILT-XXXX` 만 봄.

**원인**: 본 프로젝트의 ALL 타겟은 *기존 캐시된 타겟 목록*만 빌드. 새 add_executable이 ALL에 자동 편입 안 되는 CMake 캐시 동작.

**해결 — 본 plan의 모든 빌드 명령은 다음 두 패턴 중 하나 사용**:

```bash
# 패턴 A (권장): tests 우산 타겟 — 모든 테스트 포함
cmake --build build_Darwin -j --target tests

# 패턴 B (특정 테스트만 빠르게): 대상 테스트 명시
cmake --build build_Darwin -j --target test_<name>
```

→ Task 3 이후 모든 Step의 빌드 명령은 `--target tests`로 통일. 그래서 매 Task의 CMakeLists.txt 편집 시 *반드시* `tests` umbrella 의 DEPENDS 목록에 새 테스트를 추가해야 함 (이미 plan에 명시됨, 빠뜨리면 안 됨).

### N2: ctest `-R` 정규식은 *태그*가 아니라 *Catch2 시나리오 이름*을 매치

**증상**: `-R "state_fields"` (태그 substring) 실행 시 `No tests were found!!!`.

**원인**: `catch_discover_tests`는 TEST_CASE의 *첫 번째 인자*(시나리오 이름)을 ctest 이름으로 등록. `[diagnostics][state_fields]` 같은 *두 번째 인자*(태그)는 ctest -R로 매치 안 됨.

**해결**: 시나리오 이름의 substring을 사용. 본 plan의 `-R` 정규식 5곳 이미 수정 완료:

| Task | 정규식 |
|---|---|
| 1 | `-R "SymbolicName"` |
| 2 | `-R "CaptureGLState\|fresh fixture"` |
| 4 | `-R "UniformDiagnostics"` |
| 5 | `-R "GLStateLog"` |
| 6 | `-R "Diff\|ToString"` |

또는 *전체 실행* (`-R` 생략):
```bash
ctest --test-dir build_Darwin --output-on-failure
```
신규 케이스만 보고 싶으면:
```bash
ctest --test-dir build_Darwin --output-on-failure | grep -E "(SymbolicName|CaptureGLState|UniformDiagnostics|GLStateLog|Diff|ToString|FAIL|Pass)"
```

### N3: Stub 상태의 "Expected: FAIL" 설명이 부정확 — *부수효과 0인 stub*은 우연히 PASS

**증상**: Task 2 Step 3에서 *stub 상태로 ctest 실행 시 5개 중 3개가 PASS, 2개만 FAIL*.

**원인**: stub `CaptureGLState() { return {}; }`은 *아무 GL 호출도 안 함* → 부수효과 0 검증과 GL_NO_ERROR 검증이 *우연히* 통과. 실제로 *capture가 작동해야 잡히는* 검증은 "fresh fixture default"(viewport 비교)와 "VAO 바인딩 후 반영"(handle 비교) 두 케이스.

**해결**: 본 plan의 Task 2 Step 3 Expected 설명 수정 (아래 Task 2 §). 이 비대칭은 plan 결함이지만 *학습 가치*: stub이 통과시키는 케이스는 *진짜 회귀 감지력이 약한* 케이스라는 신호. Task 9 사보타지 드릴이 이 같은 blind spot을 찾는 절차.

→ Task 3 이후 모든 *FAIL 기대* Step에서 "stub이 무엇을 우연 통과시킬 수 있는지" 명시적으로 점검 후 진행.

---

## File Structure (산출물 매핑)

```
src/diagnostics/
├── gl_state_fields.h        [NEW]   GLStateFields struct + SymbolicName + CaptureGLState + FieldsToString
├── gl_state_fields.cpp      [NEW]
├── gl_state_log.h           [NEW]   GLStateLog::Dump + EnableAutoOnError
├── gl_state_log.cpp         [NEW]
└── CMakeLists.txt           [MODIFY] 새 .cpp 2개 등록

test/support/
├── gl_state_snapshot.h      [NEW]   GLStateSnapshot::Capture/ToString + Diff
├── gl_state_snapshot.cpp    [NEW]
├── spdlog_capture.h         [NEW]   SpdlogCapture RAII
└── spdlog_capture.cpp       [NEW]

test/
├── test_gl_state_fields.cpp        [NEW]   SymbolicName 회귀 (GL ctx 불필요)
├── test_gl_state_capture.cpp       [NEW]   CaptureGLState 회귀 (GL ctx 필요)
├── test_gl_state_snapshot.cpp      [NEW]   ToString/Diff 회귀 (대부분 GL ctx 불필요)
├── test_gl_state_log.cpp           [NEW]   GLStateLog::Dump 회귀 (GL ctx + SpdlogCapture)
├── test_uniform_diagnostics.cpp    [MODIFY] SUCCEED → SpdlogCapture 단언으로 3곳 교체
└── CMakeLists.txt                  [MODIFY] STATIC libs (gl_state_snapshot, spdlog_capture) + 4 새 test exe + tests umbrella + smell linter ctest 등록

scripts/
└── check_test_smells.py     [NEW]   R1-R4 정적 검사

doc/
├── test-quality-drill.md            [NEW]  메인 운영 문서
└── test-quality-drill/              [NEW]  컴포넌트별 살아있는 표
    ├── gl_state_capture.md
    ├── diff.md
    ├── symbolic_name.md
    └── snapshot_tostring.md
```

**책임 경계 (한 파일 = 하나의 책임)**:
- `gl_state_fields.{h,cpp}` = *데이터 모델 + 공통 변환* (struct, SymbolicName, CaptureGLState, FieldsToString)
- `gl_state_log.{h,cpp}` = *production 입출력 인터페이스* (Dump, EnableAutoOnError)
- `gl_state_snapshot.{h,cpp}` = *테스트 친화 wrapper + Diff* (Capture, ToString, Diff)
- `spdlog_capture.{h,cpp}` = *log 캡처 RAII* (SpdlogCapture)

---

## Task 1: SymbolicName 함수 + GLStateFields struct (no GL ctx)

**Goal**: 17 필드 데이터 모델 정의 + `SymbolicName(GLenum) → const char*` 함수. 사전 적중/미적중/GL_ZERO 정책 검증.

**Files:**
- Create: `test/test_gl_state_fields.cpp`
- Create: `src/diagnostics/gl_state_fields.h`
- Create: `src/diagnostics/gl_state_fields.cpp`
- Modify: `src/diagnostics/CMakeLists.txt` (새 .cpp 등록)
- Modify: `test/CMakeLists.txt` (새 test executable 등록)

- [ ] **Step 1: 실패 테스트 작성** — `test/test_gl_state_fields.cpp`

```cpp
/**
 * @file test_gl_state_fields.cpp
 * @brief SymbolicName 사전 적중/미적중/GL_ZERO 정책 검증.
 * @details GL context 불필요 — 순수 함수 테스트.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "diagnostics/gl_state_fields.h"
#include <glad/glad.h>

using SJH::Diagnostics::SymbolicName;
using Catch::Matchers::Equals;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("SymbolicName 사전 적중", "[diagnostics][state_fields]")
{
    REQUIRE_THAT(SymbolicName(GL_LESS),    Equals("GL_LESS"));
    REQUIRE_THAT(SymbolicName(GL_LEQUAL),  Equals("GL_LEQUAL"));
    REQUIRE_THAT(SymbolicName(GL_BACK),    Equals("GL_BACK"));
    REQUIRE_THAT(SymbolicName(GL_FRONT),   Equals("GL_FRONT"));
    REQUIRE_THAT(SymbolicName(GL_CCW),     Equals("GL_CCW"));
    REQUIRE_THAT(SymbolicName(GL_CW),      Equals("GL_CW"));
    REQUIRE_THAT(SymbolicName(GL_ONE),     Equals("GL_ONE"));
    REQUIRE_THAT(SymbolicName(GL_SRC_ALPHA), Equals("GL_SRC_ALPHA"));
}

TEST_CASE("SymbolicName(0) → GL_ZERO 정책 (blend factor 컨텍스트)", "[diagnostics][state_fields]")
{
    // 본 프로젝트 17개 캡처 필드 한정 시 0이 enum 값으로 합법 발생하는 곳은
    // blend_src_rgb / blend_dst_rgb 뿐 — GL_ZERO 가 정확.
    // 미래 GL_TEXTURE_COMPARE_MODE 등 GL_NONE-context 추가 시 필드별 분기 (현재 YAGNI).
    REQUIRE_THAT(SymbolicName(0), Equals("GL_ZERO"));
}

TEST_CASE("SymbolicName 미적중 → hex fallback", "[diagnostics][state_fields]")
{
    REQUIRE_THAT(SymbolicName(0xDEAD),  Equals("0xDEAD"));
    REQUIRE_THAT(SymbolicName(0xBEEF),  Equals("0xBEEF"));
    // 4자리 hex (대문자) — 사람이 매뉴얼 검색 가능한 형식
    REQUIRE_THAT(SymbolicName(0xABCD),  Equals("0xABCD"));
}

TEST_CASE("SymbolicName GL_TEXTUREn 동적 표현", "[diagnostics][state_fields]")
{
    // active_texture 필드는 GL_TEXTURE0..GL_TEXTURE15 범위 — 사전 16개 등록 비효율,
    // 동적으로 "GL_TEXTURE{n}" 포맷.
    REQUIRE_THAT(SymbolicName(GL_TEXTURE0),  Equals("GL_TEXTURE0"));
    REQUIRE_THAT(SymbolicName(GL_TEXTURE1),  Equals("GL_TEXTURE1"));
    REQUIRE_THAT(SymbolicName(GL_TEXTURE15), Equals("GL_TEXTURE15"));
}

TEST_CASE("SymbolicName 사전 boundary — depth_func 모든 8개", "[diagnostics][state_fields]")
{
    REQUIRE_THAT(SymbolicName(GL_NEVER),    Equals("GL_NEVER"));
    REQUIRE_THAT(SymbolicName(GL_LESS),     Equals("GL_LESS"));
    REQUIRE_THAT(SymbolicName(GL_EQUAL),    Equals("GL_EQUAL"));
    REQUIRE_THAT(SymbolicName(GL_LEQUAL),   Equals("GL_LEQUAL"));
    REQUIRE_THAT(SymbolicName(GL_GREATER),  Equals("GL_GREATER"));
    REQUIRE_THAT(SymbolicName(GL_NOTEQUAL), Equals("GL_NOTEQUAL"));
    REQUIRE_THAT(SymbolicName(GL_GEQUAL),   Equals("GL_GEQUAL"));
    REQUIRE_THAT(SymbolicName(GL_ALWAYS),   Equals("GL_ALWAYS"));
}
```

- [ ] **Step 2: 헤더 작성** — `src/diagnostics/gl_state_fields.h`

```cpp
#ifndef __SJH_DIAGNOSTICS_GL_STATE_FIELDS_H__
#define __SJH_DIAGNOSTICS_GL_STATE_FIELDS_H__

#pragma once

#include <glad/glad.h>
#include <array>
#include <string>

namespace SJH::Diagnostics
{
    /// 한 시점의 GL 상태 스냅샷. log + snapshot 양쪽이 공유.
    struct GLStateFields
    {
        // 바인딩
        GLuint vao{0};
        GLuint program{0};
        GLuint array_buffer{0};
        GLuint element_buffer{0};
        GLuint draw_fbo{0};
        GLuint read_fbo{0};

        // 텍스처 (macOS GL 3.3 spec 상한 16; 0이 아닌 unit만 ToString 출력)
        GLenum active_texture{GL_TEXTURE0};
        std::array<GLuint, 16> texture_2d_per_unit{};

        // viewport (x, y, w, h)
        std::array<GLint, 4> viewport{};

        // 픽셀 파이프라인
        bool depth_test_enabled{false};
        GLenum depth_func{GL_LESS};
        bool depth_write_mask{true};

        bool blend_enabled{false};
        GLenum blend_src_rgb{GL_ONE};
        GLenum blend_dst_rgb{GL_ZERO};

        bool cull_face_enabled{false};
        GLenum cull_face_mode{GL_BACK};
        GLenum front_face{GL_CCW};

        std::array<bool, 4> color_write_mask{true, true, true, true};
        std::array<GLfloat, 4> clear_color{0, 0, 0, 0};
    };

    /// 현재 GL 상태 캡처. 부수효과 0 (active_texture 저장→유닛 순회→복원).
    /// @pre  GL context active (caller 책임)
    /// @post 17 필드 모두 채워 반환. glGetError가 non-zero 였으면 spdlog::warn (값 정확성 의심)
    GLStateFields CaptureGLState();

    /// GLenum → 사람이 읽는 이름. ~28 사전 + GL_TEXTUREn 동적. 미적중 시 "0xXXXX".
    /// @note SymbolicName(0) == "GL_ZERO" — blend factor 컨텍스트 가정. 자세한 근거는
    ///       spec 2.1 / test_gl_state_fields.cpp "GL_ZERO 정책" 케이스 참조.
    const char* SymbolicName(GLenum e);

    /// GLStateFields → 사람이 읽는 다중라인 문자열.
    /// VAO=0인 경우 element_buffer 라인에 주석 자동 포함.
    /// enum 필드는 SymbolicName, GLuint 핸들은 raw 정수 (의도된 비대칭).
    std::string FieldsToString(const GLStateFields& fields);
}

#endif // __SJH_DIAGNOSTICS_GL_STATE_FIELDS_H__
```

- [ ] **Step 3: 구현 작성** — `src/diagnostics/gl_state_fields.cpp` (Capture와 FieldsToString은 후속 task에서 채움; 우선 SymbolicName만)

```cpp
#include "diagnostics/gl_state_fields.h"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace SJH::Diagnostics
{
    const char* SymbolicName(GLenum e)
    {
        // GL_TEXTURE0..GL_TEXTURE15 동적 영역
        if (e >= GL_TEXTURE0 && e <= GL_TEXTURE0 + 15) {
            // thread_local 정적 버퍼 — caller가 즉시 출력하면 안전, 보관 시엔 std::string 권장
            static thread_local char buf[16];
            snprintf(buf, sizeof(buf), "GL_TEXTURE%d", static_cast<int>(e - GL_TEXTURE0));
            return buf;
        }

        switch (e) {
            // depth_func / stencil_func 공용 8개
            case GL_NEVER:    return "GL_NEVER";
            case GL_LESS:     return "GL_LESS";
            case GL_EQUAL:    return "GL_EQUAL";
            case GL_LEQUAL:   return "GL_LEQUAL";
            case GL_GREATER:  return "GL_GREATER";
            case GL_NOTEQUAL: return "GL_NOTEQUAL";
            case GL_GEQUAL:   return "GL_GEQUAL";
            case GL_ALWAYS:   return "GL_ALWAYS";

            // blend factor — GL 3.3 core 한정 (SRC1 family는 4.4+ 제외)
            // 0은 GL_ZERO — blend factor 컨텍스트 가정 (spec 2.1)
            case 0:                              return "GL_ZERO";
            case GL_ONE:                         return "GL_ONE";
            case GL_SRC_COLOR:                   return "GL_SRC_COLOR";
            case GL_ONE_MINUS_SRC_COLOR:         return "GL_ONE_MINUS_SRC_COLOR";
            case GL_DST_COLOR:                   return "GL_DST_COLOR";
            case GL_ONE_MINUS_DST_COLOR:         return "GL_ONE_MINUS_DST_COLOR";
            case GL_SRC_ALPHA:                   return "GL_SRC_ALPHA";
            case GL_ONE_MINUS_SRC_ALPHA:         return "GL_ONE_MINUS_SRC_ALPHA";
            case GL_DST_ALPHA:                   return "GL_DST_ALPHA";
            case GL_ONE_MINUS_DST_ALPHA:         return "GL_ONE_MINUS_DST_ALPHA";
            case GL_CONSTANT_COLOR:              return "GL_CONSTANT_COLOR";
            case GL_ONE_MINUS_CONSTANT_COLOR:    return "GL_ONE_MINUS_CONSTANT_COLOR";
            case GL_CONSTANT_ALPHA:              return "GL_CONSTANT_ALPHA";
            case GL_ONE_MINUS_CONSTANT_ALPHA:    return "GL_ONE_MINUS_CONSTANT_ALPHA";
            case GL_SRC_ALPHA_SATURATE:          return "GL_SRC_ALPHA_SATURATE";

            // cull/front face
            case GL_FRONT:           return "GL_FRONT";
            case GL_BACK:            return "GL_BACK";
            case GL_FRONT_AND_BACK:  return "GL_FRONT_AND_BACK";
            case GL_CCW:             return "GL_CCW";
            case GL_CW:              return "GL_CW";
        }

        // 미적중 — hex fallback. 4자리 대문자 (예: "0xDEAD")
        // thread_local 버퍼: caller가 즉시 출력 가정. 영구 보관 시 fmt::format 사용 권장.
        static thread_local char buf[16];
        snprintf(buf, sizeof(buf), "0x%04X", static_cast<unsigned>(e));
        return buf;
    }

    // FieldsToString은 Task 5 (Log)에서 채움 — 이 task엔 stub
    std::string FieldsToString(const GLStateFields&) { return ""; }

    // CaptureGLState는 Task 2에서 채움 — 이 task엔 stub
    GLStateFields CaptureGLState() { return {}; }
}
```

- [ ] **Step 4: CMake 등록** — `src/diagnostics/CMakeLists.txt:1-4` 의 add_library 호출에 새 .cpp 추가

```cmake
add_library(sjhopengl_diagnostics STATIC
    gl_log.cpp
    uniform_diagnostics.cpp
    gl_state_fields.cpp
)
```

- [ ] **Step 5: test executable 등록** — `test/CMakeLists.txt`의 `test_uniform_diagnostics` 블록 (lines 67-75) 직후에 다음 추가

```cmake
#  diagnostics 모듈 — GLStateFields::SymbolicName 정책 검증
# GL context 불필요 (순수 함수).
add_executable(test_gl_state_fields test_gl_state_fields.cpp)
target_link_libraries(test_gl_state_fields PRIVATE
    Catch2::Catch2WithMain
    SJH::diagnostics
)
target_compile_features(test_gl_state_fields PRIVATE cxx_std_17)
catch_discover_tests(test_gl_state_fields)
```

그리고 `tests` umbrella target (line 143-153)에 `test_gl_state_fields` 추가:
```cmake
add_custom_target(tests DEPENDS
    test_common
    test_glfw_utils
    test_gl_fixture
    test_gl_debug
    test_uniform_diagnostics
    test_gl_state_fields  # 추가
    test_program_uniforms
    test_buffer
    test_vertex_layout
    test_texture
)
```

- [ ] **Step 6: 빌드 + 테스트 실행 → PASS 확인**

Run: `cmake --build build_Darwin -j --target tests && ctest --test-dir build_Darwin --output-on-failure -R "SymbolicName"`
Expected: 5개 케이스 모두 PASS.

> **주의** (자세한 근거는 상단 N1, N2 참조):
> - 빌드: `--target tests` 명시 필수 (그냥 `-j` 만으로는 신규 testexe가 ALL에 안 잡힘)
> - `-R`: ctest **테스트 이름**(=Catch2 시나리오명)을 매치. 태그는 매치 안 됨.

- [ ] **Step 7: stage (commit은 사용자 명시 요청 시)**

```bash
git add src/diagnostics/gl_state_fields.h \
        src/diagnostics/gl_state_fields.cpp \
        src/diagnostics/CMakeLists.txt \
        test/test_gl_state_fields.cpp \
        test/CMakeLists.txt
# 권장 commit 메시지:
# feat(diagnostics): add GLStateFields struct + SymbolicName with hex fallback
```

---

## Task 2: CaptureGLState 구현 (GL ctx 필요)

**Goal**: 17 필드 + 16 텍스처 unit을 부수효과 0으로 캡처. drain → capture → post-check.

**Files:**
- Create: `test/test_gl_state_capture.cpp`
- Modify: `src/diagnostics/gl_state_fields.cpp` (CaptureGLState stub 채움)
- Modify: `test/CMakeLists.txt` (test_gl_state_capture executable 등록)

- [ ] **Step 1: 실패 테스트 작성** — `test/test_gl_state_capture.cpp`

```cpp
/**
 * @file test_gl_state_capture.cpp
 * @brief CaptureGLState 회귀 — 결정성, 부수효과 0, GL_NO_ERROR, fresh default, bind 반영.
 */

#include <catch2/catch_test_macros.hpp>

#include "support/gl_test_fixture.h"
#include "diagnostics/gl_state_fields.h"
#include <glad/glad.h>

using SJH::Diagnostics::CaptureGLState;

namespace { void DrainGLErrors() { while (glGetError() != GL_NO_ERROR) {} } }

TEST_CASE("CaptureGLState 결정성 — 두 번 연속 호출 byte-equal", "[diagnostics][capture]")
{
    SJH::test::GLContextFixture ctx;
    DrainGLErrors();

    auto a = CaptureGLState();
    auto b = CaptureGLState();

    // struct 비교 — std::array는 == 지원
    REQUIRE(a.vao == b.vao);
    REQUIRE(a.program == b.program);
    REQUIRE(a.active_texture == b.active_texture);
    REQUIRE(a.texture_2d_per_unit == b.texture_2d_per_unit);
    REQUIRE(a.viewport == b.viewport);
    REQUIRE(a.depth_test_enabled == b.depth_test_enabled);
    REQUIRE(a.blend_enabled == b.blend_enabled);
    REQUIRE(a.clear_color == b.clear_color);
}

TEST_CASE("CaptureGLState 부수효과 0 — active_texture 변하지 않음", "[diagnostics][capture]")
{
    SJH::test::GLContextFixture ctx;
    DrainGLErrors();

    glActiveTexture(GL_TEXTURE5);  // 의도적으로 unit 5로 변경
    GLint before = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &before);

    CaptureGLState();  // 내부에서 16 unit 순회 후 복원해야 함

    GLint after = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &after);
    REQUIRE(before == after);
    REQUIRE(after == GL_TEXTURE5);
}

TEST_CASE("CaptureGLState 후 GL_NO_ERROR", "[diagnostics][capture]")
{
    SJH::test::GLContextFixture ctx;
    DrainGLErrors();

    auto fields = CaptureGLState();

    GLenum err = glGetError();
    REQUIRE(err == GL_NO_ERROR);
}

TEST_CASE("fresh fixture default — VAO=0, program=0, viewport=(0,0,256,256)",
          "[diagnostics][capture]")
{
    SJH::test::GLContextFixture ctx(256, 256);
    DrainGLErrors();

    auto f = CaptureGLState();
    REQUIRE(f.vao == 0u);
    REQUIRE(f.program == 0u);
    REQUIRE(f.viewport[0] == 0);
    REQUIRE(f.viewport[1] == 0);
    REQUIRE(f.viewport[2] == 256);
    REQUIRE(f.viewport[3] == 256);
}

TEST_CASE("CaptureGLState — VAO 바인딩 후 fields.vao 반영", "[diagnostics][capture]")
{
    SJH::test::GLContextFixture ctx;
    DrainGLErrors();

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    auto f = CaptureGLState();
    REQUIRE(f.vao == vao);

    glDeleteVertexArrays(1, &vao);
}
```

- [ ] **Step 2: test executable 등록** — `test/CMakeLists.txt`의 `test_gl_state_fields` 블록 직후

```cmake
#  diagnostics 모듈 — CaptureGLState (GL context 필요)
add_executable(test_gl_state_capture test_gl_state_capture.cpp)
target_link_libraries(test_gl_state_capture PRIVATE
    Catch2::Catch2WithMain
    gl_test_fixture
    SJH::diagnostics
)
target_compile_features(test_gl_state_capture PRIVATE cxx_std_17)
catch_discover_tests(test_gl_state_capture)
```

`tests` umbrella에도 `test_gl_state_capture` 추가.

- [ ] **Step 3: 빌드 + 테스트 → 일부 FAIL 확인** (CaptureGLState 가 stub이라 default 반환)

Run: `cmake --build build_Darwin -j --target tests && ctest --test-dir build_Darwin --output-on-failure -R "CaptureGLState|fresh fixture"`

**정확한 기대 결과 — 5개 중 3 PASS / 2 FAIL** (N3 항목 참조):

| 시나리오 | stub 결과 | 이유 |
|---|---|---|
| "결정성 — byte-equal" | **PASS** | stub은 둘 다 default 반환 → 동일 |
| "부수효과 0 — active_texture" | **PASS** ⚠️ | stub은 GL 호출 안 함 → 부수효과도 0 (우연) |
| "후 GL_NO_ERROR" | **PASS** ⚠️ | stub은 GL 호출 안 함 → 에러 0 (우연) |
| "fresh fixture default" | **FAIL** | viewport={0,0,0,0} ≠ {0,0,256,256} |
| "VAO 바인딩 후 반영" | **FAIL** | f.vao=0, 실제 vao=1+ |

⚠️ 표시는 *stub이 우연 통과시킨* 케이스 — 실제 회귀 감지력이 약한 부분이라는 신호. Task 9 사보타지 드릴이 이런 blind spot 추적용.

- [ ] **Step 4: 구현 작성** — `src/diagnostics/gl_state_fields.cpp`의 `CaptureGLState` stub을 다음으로 교체

```cpp
GLStateFields CaptureGLState()
{
    // 1. drain pre-existing errors
    while (glGetError() != GL_NO_ERROR) {}

    GLStateFields f;

    // 2. 단일값 GLuint 바인딩
    GLint tmp = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,         &tmp); f.vao            = static_cast<GLuint>(tmp);
    glGetIntegerv(GL_CURRENT_PROGRAM,              &tmp); f.program        = static_cast<GLuint>(tmp);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,         &tmp); f.array_buffer   = static_cast<GLuint>(tmp);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &tmp); f.element_buffer = static_cast<GLuint>(tmp);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,     &tmp); f.draw_fbo       = static_cast<GLuint>(tmp);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,     &tmp); f.read_fbo       = static_cast<GLuint>(tmp);

    // 3. active_texture (GLenum)
    glGetIntegerv(GL_ACTIVE_TEXTURE, &tmp);
    f.active_texture = static_cast<GLenum>(tmp);

    // 4. viewport (4 ints)
    glGetIntegerv(GL_VIEWPORT, f.viewport.data());

    // 5. 픽셀 파이프라인 — glIsEnabled / glGetIntegerv / glGetBooleanv 혼합
    f.depth_test_enabled = (glIsEnabled(GL_DEPTH_TEST) == GL_TRUE);
    glGetIntegerv(GL_DEPTH_FUNC, &tmp); f.depth_func = static_cast<GLenum>(tmp);
    GLboolean b = GL_FALSE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &b); f.depth_write_mask = (b == GL_TRUE);

    f.blend_enabled = (glIsEnabled(GL_BLEND) == GL_TRUE);
    glGetIntegerv(GL_BLEND_SRC_RGB, &tmp); f.blend_src_rgb = static_cast<GLenum>(tmp);
    glGetIntegerv(GL_BLEND_DST_RGB, &tmp); f.blend_dst_rgb = static_cast<GLenum>(tmp);

    f.cull_face_enabled = (glIsEnabled(GL_CULL_FACE) == GL_TRUE);
    glGetIntegerv(GL_CULL_FACE_MODE, &tmp); f.cull_face_mode = static_cast<GLenum>(tmp);
    glGetIntegerv(GL_FRONT_FACE,     &tmp); f.front_face     = static_cast<GLenum>(tmp);

    GLboolean cwm[4] = {};
    glGetBooleanv(GL_COLOR_WRITEMASK, cwm);
    for (int i = 0; i < 4; ++i) f.color_write_mask[i] = (cwm[i] == GL_TRUE);

    glGetFloatv(GL_COLOR_CLEAR_VALUE, f.clear_color.data());

    // 6. 텍스처 unit 16개 — active_texture 보존/복원
    GLint saved_active = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &saved_active);
    for (int i = 0; i < 16; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &tmp);
        f.texture_2d_per_unit[i] = static_cast<GLuint>(tmp);
    }
    glActiveTexture(static_cast<GLenum>(saved_active));

    // 7. post-check
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        spdlog::warn("[GLStateLog::Capture] produced GL error 0x{:X} — "
                     "all fields populated but values may be stale",
                     static_cast<unsigned>(err));
    }

    return f;
}
```

- [ ] **Step 5: 빌드 + 테스트 → 5/5 PASS 확인**

Run: `cmake --build build_Darwin -j --target tests && ctest --test-dir build_Darwin --output-on-failure -R "CaptureGLState|fresh fixture"`
Expected: 5개 케이스 모두 PASS (이전 stub 상태에서 FAIL이었던 "fresh fixture default"와 "VAO 바인딩 후 반영"이 GREEN으로 전환).

- [ ] **Step 6: stage**

```bash
git add src/diagnostics/gl_state_fields.cpp \
        test/test_gl_state_capture.cpp \
        test/CMakeLists.txt
# 권장 commit: feat(diagnostics): implement CaptureGLState with side-effect-free unit traversal
```

---

## Task 3: SpdlogCapture RAII (no GL ctx)

**Goal**: spdlog default logger를 ostringstream sink로 잠시 교체 → 테스트가 로그 출력을 단언 가능.

**Files:**
- Create: `test/support/spdlog_capture.h`
- Create: `test/support/spdlog_capture.cpp`
- Modify: `test/CMakeLists.txt` (spdlog_capture STATIC lib)

- [ ] **Step 1: 헤더 작성** — `test/support/spdlog_capture.h`

```cpp
#ifndef __SJH_TEST_SPDLOG_CAPTURE_H__
#define __SJH_TEST_SPDLOG_CAPTURE_H__

#pragma once

#include <memory>
#include <sstream>
#include <string>
#include <string_view>

namespace spdlog { class logger; }

namespace SJH::test
{
    /// RAII로 default spdlog logger를 ostringstream sink로 교체.
    /// 소멸 시 원래 logger 복원. 단일 스레드 가정 (Catch2 v3 default).
    class SpdlogCapture
    {
    public:
        SpdlogCapture();
        ~SpdlogCapture();

        SpdlogCapture(const SpdlogCapture&)            = delete;
        SpdlogCapture& operator=(const SpdlogCapture&) = delete;

        /// 캡처된 모든 출력 (newline 포함).
        std::string Lines() const;

        /// substring 포함 여부 — 가장 흔한 단언 패턴.
        bool Contains(std::string_view s) const;

    private:
        std::shared_ptr<spdlog::logger>    mPrev;
        std::shared_ptr<std::ostringstream> mStream;
    };
}

#endif // __SJH_TEST_SPDLOG_CAPTURE_H__
```

- [ ] **Step 2: 구현 작성** — `test/support/spdlog_capture.cpp`

```cpp
#include "support/spdlog_capture.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/ostream_sink.h>

namespace SJH::test
{
    SpdlogCapture::SpdlogCapture()
        : mStream(std::make_shared<std::ostringstream>())
    {
        mPrev = spdlog::default_logger();
        auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(*mStream);
        sink->set_pattern("[%l] %v");  // 결정적 출력 — 시간 제거
        auto logger = std::make_shared<spdlog::logger>("test_capture", sink);
        logger->set_level(spdlog::level::trace);
        spdlog::set_default_logger(logger);
    }

    SpdlogCapture::~SpdlogCapture()
    {
        spdlog::set_default_logger(mPrev);
    }

    std::string SpdlogCapture::Lines() const
    {
        return mStream->str();
    }

    bool SpdlogCapture::Contains(std::string_view s) const
    {
        const auto& full = mStream->str();
        return full.find(s) != std::string::npos;
    }
}
```

- [ ] **Step 3: STATIC lib 등록** — `test/CMakeLists.txt`의 `gl_test_fixture` 블록 (line 38-46) 직후

```cmake
#  spdlog 출력 캡처 RAII (Task 3)
# GL context 불필요. spdlog 의존만.
add_library(spdlog_capture STATIC support/spdlog_capture.cpp)
target_link_libraries(spdlog_capture PUBLIC
    spdlog::spdlog
)
target_include_directories(spdlog_capture PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}   # "support/spdlog_capture.h" 접근
)
target_compile_features(spdlog_capture PRIVATE cxx_std_17)
```

- [ ] **Step 4: 빌드 — STATIC lib만 빌드되는지 확인**

Run: `cmake --build build_Darwin -j --target spdlog_capture`
Expected: 빌드 PASS (사용처 없으므로 link 단계는 skip).

- [ ] **Step 5: stage**

```bash
git add test/support/spdlog_capture.h \
        test/support/spdlog_capture.cpp \
        test/CMakeLists.txt
# 권장 commit: feat(test/support): add SpdlogCapture RAII for log assertion
```

---

## Task 4: test_uniform_diagnostics.cpp의 SUCCEED 교체

**Goal**: 기존 [test/test_uniform_diagnostics.cpp:16-66](../../test/test_uniform_diagnostics.cpp#L16-L66) 의 3개 SUCCEED-only 케이스를 SpdlogCapture 단언으로 교체. 행동 회귀 감지력 확보.

**Files:**
- Modify: `test/test_uniform_diagnostics.cpp` (3개 TEST_CASE 모두)
- Modify: `test/CMakeLists.txt` (test_uniform_diagnostics에 spdlog_capture link 추가)

- [ ] **Step 1: link 의존 추가** — `test/CMakeLists.txt`의 test_uniform_diagnostics 블록 (lines 69-75)을 다음으로 교체

```cmake
#  diagnostics 모듈 — UniformDiagnostics warn-once 트래커
# GL context 불필요. SpdlogCapture로 로그 출력 행동 단언.
add_executable(test_uniform_diagnostics test_uniform_diagnostics.cpp)
target_link_libraries(test_uniform_diagnostics PRIVATE
    Catch2::Catch2WithMain
    SJH::diagnostics
    spdlog_capture
)
target_compile_features(test_uniform_diagnostics PRIVATE cxx_std_17)
catch_discover_tests(test_uniform_diagnostics)
```

- [ ] **Step 2: include 추가** — `test/test_uniform_diagnostics.cpp` 상단 (line 13 근처)

```cpp
#include "support/spdlog_capture.h"
```

- [ ] **Step 3: 케이스 1 교체** — `test/test_uniform_diagnostics.cpp:16-31` 의 첫 TEST_CASE

```cpp
TEST_CASE("UniformDiagnostics::NotifyMissing 다중 호출 — warn-once 트래커",
          "[diagnostics][uniform]")
{
    using SJH::Diagnostics::UniformDiagnostics;
    SJH::test::SpdlogCapture cap;

    // (program=42, name="uMissingA") 첫 호출 → warn 출력
    UniformDiagnostics::NotifyMissing(42, "uMissingA");
    REQUIRE(cap.Contains("uMissingA"));
    auto firstSize = cap.Lines().size();

    // 두 번째 동일 (program, name) — warn-once 로 silent (출력 길이 변화 없음)
    UniformDiagnostics::NotifyMissing(42, "uMissingA");
    REQUIRE(cap.Lines().size() == firstSize);

    // 같은 program, 다른 name — 새 warn (출력 길이 증가)
    UniformDiagnostics::NotifyMissing(42, "uMissingB");
    REQUIRE(cap.Lines().size() > firstSize);
    REQUIRE(cap.Contains("uMissingB"));

    // 다른 program, 같은 name — 다른 키, warn 발생
    UniformDiagnostics::NotifyMissing(99, "uMissingA");
    // 구현이 program 핸들을 출력에 포함한다면 99도 검출 가능.
    // 실제 src/diagnostics/uniform_diagnostics.cpp 포맷 확인 후 단언 조정.
    // REQUIRE(cap.Contains("99"));

    // 정리: 후속 테스트가 stale 트래커 상속 안 하게
    UniformDiagnostics::Invalidate(42);
    UniformDiagnostics::Invalidate(99);
}
```

- [ ] **Step 4: 케이스 2 교체** — `test/test_uniform_diagnostics.cpp:33-49`

```cpp
TEST_CASE("UniformDiagnostics::NotifyTypeMismatch 시나리오", "[diagnostics][uniform]")
{
    using SJH::Diagnostics::UniformDiagnostics;
    SJH::test::SpdlogCapture cap;
    constexpr GLuint kProgram = 42;

    // 불일치 — 첫 호출 warn
    UniformDiagnostics::NotifyTypeMismatch(kProgram, "uMat", GL_FLOAT_MAT4, GL_FLOAT_VEC4);
    REQUIRE(cap.Contains("uMat"));
    auto afterFirst = cap.Lines().size();

    // 같은 (prog, name) 재호출 — silent
    UniformDiagnostics::NotifyTypeMismatch(kProgram, "uMat", GL_FLOAT_MAT4, GL_FLOAT_VEC4);
    REQUIRE(cap.Lines().size() == afterFirst);

    // 일치 — silent (warn 안 일어남)
    UniformDiagnostics::NotifyTypeMismatch(kProgram, "uOk", GL_FLOAT, GL_FLOAT);
    REQUIRE(cap.Lines().size() == afterFirst);
    REQUIRE_FALSE(cap.Contains("uOk"));

    // actual==0 (active 정보 없음) — silent
    UniformDiagnostics::NotifyTypeMismatch(kProgram, "uUnknown", GL_FLOAT, 0);
    REQUIRE(cap.Lines().size() == afterFirst);
    REQUIRE_FALSE(cap.Contains("uUnknown"));

    UniformDiagnostics::Invalidate(kProgram);
}
```

- [ ] **Step 5: 케이스 3 교체** — `test/test_uniform_diagnostics.cpp:51-66`

```cpp
TEST_CASE("UniformDiagnostics::Invalidate 멱등 + Invalidate 후 재발 가능",
          "[diagnostics][uniform]")
{
    using SJH::Diagnostics::UniformDiagnostics;
    SJH::test::SpdlogCapture cap;
    constexpr GLuint kProgram = 7;

    // 첫 NotifyMissing — warn 발생
    UniformDiagnostics::NotifyMissing(kProgram, "uX");
    REQUIRE(cap.Contains("uX"));
    auto sizeBeforeInvalidate = cap.Lines().size();

    // Invalidate — 트래커 정리, 추가 출력 없음
    UniformDiagnostics::Invalidate(kProgram);
    UniformDiagnostics::Invalidate(kProgram);  // 멱등 (idempotent)
    UniformDiagnostics::Invalidate(99999);     // 미존재 program — 안전 (crash X, 출력 없음)
    REQUIRE(cap.Lines().size() == sizeBeforeInvalidate);

    // Invalidate 후 같은 (prog, name) NotifyMissing — 다시 warn
    UniformDiagnostics::NotifyMissing(kProgram, "uX");
    REQUIRE(cap.Lines().size() > sizeBeforeInvalidate);
}
```

- [ ] **Step 6: 빌드 + 테스트 → PASS 확인**

Run: `cmake --build build_Darwin -j --target tests && ctest --test-dir build_Darwin --output-on-failure -R "UniformDiagnostics"`
Expected: 3개 케이스 모두 PASS.

- [ ] **Step 7: stage**

```bash
git add test/test_uniform_diagnostics.cpp \
        test/CMakeLists.txt
# 권장 commit: refactor(test): replace SUCCEED smoke tests with SpdlogCapture behavioral assertions
```

---

## Task 5: GLStateLog (Production Dump + macOS no-op) + FieldsToString 본구현

**Goal**: `GLStateLog::Dump(tag)` — production 측 한 줄 덤프. `EnableAutoOnError` — macOS는 std::call_once warn 후 no-op. `FieldsToString` — 공통 포매터 본격 구현.

**Files:**
- Create: `test/test_gl_state_log.cpp`
- Create: `src/diagnostics/gl_state_log.h`
- Create: `src/diagnostics/gl_state_log.cpp`
- Modify: `src/diagnostics/gl_state_fields.cpp` (FieldsToString stub 채움)
- Modify: `src/diagnostics/CMakeLists.txt` (gl_state_log.cpp 등록)
- Modify: `test/CMakeLists.txt` (test_gl_state_log executable)

- [ ] **Step 1: 헤더 작성** — `src/diagnostics/gl_state_log.h`

```cpp
#ifndef __SJH_DIAGNOSTICS_GL_STATE_LOG_H__
#define __SJH_DIAGNOSTICS_GL_STATE_LOG_H__

#pragma once

#include <string_view>

namespace SJH::Diagnostics
{
    class GLStateLog
    {
    public:
        /// 현재 GL 상태 한 번 덤프 (spdlog::info). 매 프레임 호출 금지.
        /// @param tag 출력 prefix — 디버깅 시 위치 식별용
        static void Dump(std::string_view tag = {});

        /// KHR_debug 콜백에서 GL_DEBUG_SEVERITY_HIGH 발생 시 자동 Dump 활성화.
        /// macOS GL 3.3은 KHR_debug 미지원 → std::call_once warn 후 no-op.
        static void EnableAutoOnError(bool enable);
    };
}

#endif // __SJH_DIAGNOSTICS_GL_STATE_LOG_H__
```

- [ ] **Step 2: 구현 작성** — `src/diagnostics/gl_state_log.cpp`

```cpp
#include "diagnostics/gl_state_log.h"
#include "diagnostics/gl_state_fields.h"

#include <glad/glad.h>
#include <spdlog/spdlog.h>
#include <mutex>

namespace SJH::Diagnostics
{
    void GLStateLog::Dump(std::string_view tag)
    {
        auto fields = CaptureGLState();
        if (!tag.empty()) {
            spdlog::info("[GLStateLog/{}]\n{}", tag, FieldsToString(fields));
        } else {
            spdlog::info("[GLStateLog]\n{}", FieldsToString(fields));
        }
    }

    void GLStateLog::EnableAutoOnError(bool /*enable*/)
    {
#if defined(GL_VERSION_4_3) || defined(GL_KHR_debug)
        if (glDebugMessageCallback != nullptr) {
            // TODO(future): KHR_debug callback 등록. 현재는 macOS 우선 — 미구현.
            // 구현 시 GLDebug::Init과 통합 (architecture.md §6 Layer 1).
            spdlog::info("[GLStateLog] EnableAutoOnError: KHR_debug 콜백 등록 (TODO)");
            return;
        }
#endif
        // macOS arm64 GL 3.3 등 KHR_debug 미지원 환경
        static std::once_flag warned;
        std::call_once(warned, []() {
            spdlog::warn("[GLStateLog] EnableAutoOnError: KHR_debug 미지원 환경 — no-op");
        });
    }
}
```

- [ ] **Step 3: FieldsToString 본격 구현** — `src/diagnostics/gl_state_fields.cpp`의 stub을 다음으로 교체

상단 include 추가:
```cpp
#include <fmt/format.h>
```

stub을 다음으로 교체:
```cpp
std::string FieldsToString(const GLStateFields& f)
{
    std::string out;
    out.reserve(512);

    // 헤더 — 비대칭 (enum=symbolic, handle=raw)을 한 줄 설명
    out += "# GL state (enum=symbolic, handle=raw integer)\n";

    out += fmt::format("vao:            {}\n", f.vao);
    out += fmt::format("program:        {}\n", f.program);
    out += fmt::format("array_buffer:   {}\n", f.array_buffer);

    // VAO=0 일 때 element_buffer 라인에 주석 (spec 4.4)
    if (f.vao == 0) {
        out += fmt::format("element_buffer: {}  (note: EBO state is per-VAO; with VAO=0, this is always 0)\n",
                           f.element_buffer);
    } else {
        out += fmt::format("element_buffer: {}\n", f.element_buffer);
    }

    out += fmt::format("draw_fbo:       {}\n", f.draw_fbo);
    out += fmt::format("read_fbo:       {}\n", f.read_fbo);
    out += fmt::format("active_texture: {}\n", SymbolicName(f.active_texture));

    // 텍스처 unit — 0이 아닌 것만 출력 (spec B2 시나리오)
    bool any_unit = false;
    for (int i = 0; i < 16; ++i) {
        if (f.texture_2d_per_unit[i] != 0) {
            out += fmt::format("tex_2d[unit {}]: {}\n", i, f.texture_2d_per_unit[i]);
            any_unit = true;
        }
    }
    if (!any_unit) {
        out += "tex_2d[*]:      (all units empty)\n";
    }

    out += fmt::format("viewport:       [{}, {}, {}, {}]\n",
                       f.viewport[0], f.viewport[1], f.viewport[2], f.viewport[3]);

    out += fmt::format("depth_test:     {}\n", f.depth_test_enabled ? "ENABLED" : "disabled");
    out += fmt::format("depth_func:     {}\n", SymbolicName(f.depth_func));
    out += fmt::format("depth_write:    {}\n", f.depth_write_mask ? "true" : "false");

    out += fmt::format("blend:          {}\n", f.blend_enabled ? "ENABLED" : "disabled");
    out += fmt::format("blend_src_rgb:  {}\n", SymbolicName(f.blend_src_rgb));
    out += fmt::format("blend_dst_rgb:  {}\n", SymbolicName(f.blend_dst_rgb));

    out += fmt::format("cull_face:      {}\n", f.cull_face_enabled ? "ENABLED" : "disabled");
    out += fmt::format("cull_face_mode: {}\n", SymbolicName(f.cull_face_mode));
    out += fmt::format("front_face:     {}\n", SymbolicName(f.front_face));

    out += fmt::format("color_write:    [{}, {}, {}, {}]\n",
                       f.color_write_mask[0] ? 'R':'-',
                       f.color_write_mask[1] ? 'G':'-',
                       f.color_write_mask[2] ? 'B':'-',
                       f.color_write_mask[3] ? 'A':'-');

    out += fmt::format("clear_color:    [{:.3f}, {:.3f}, {:.3f}, {:.3f}]\n",
                       f.clear_color[0], f.clear_color[1], f.clear_color[2], f.clear_color[3]);

    return out;
}
```

- [ ] **Step 4: 실패 테스트 작성** — `test/test_gl_state_log.cpp`

```cpp
/**
 * @file test_gl_state_log.cpp
 * @brief GLStateLog::Dump + EnableAutoOnError 회귀.
 */

#include <catch2/catch_test_macros.hpp>

#include "support/gl_test_fixture.h"
#include "support/spdlog_capture.h"
#include "diagnostics/gl_state_log.h"
#include <glad/glad.h>

using SJH::Diagnostics::GLStateLog;

TEST_CASE("GLStateLog::Dump — tag 가 출력에 포함", "[diagnostics][state_log]")
{
    SJH::test::GLContextFixture ctx;
    SJH::test::SpdlogCapture cap;

    GLStateLog::Dump("after_init");

    REQUIRE(cap.Contains("after_init"));
    REQUIRE(cap.Contains("vao:"));
    REQUIRE(cap.Contains("viewport:"));
}

TEST_CASE("GLStateLog::Dump — VAO 바인딩 후 핸들 출력", "[diagnostics][state_log]")
{
    SJH::test::GLContextFixture ctx;
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    SJH::test::SpdlogCapture cap;
    GLStateLog::Dump();

    // raw 정수로 vao 핸들 출력 (비대칭 정책)
    REQUIRE(cap.Contains(std::to_string(vao)));

    glDeleteVertexArrays(1, &vao);
}

TEST_CASE("GLStateLog::EnableAutoOnError — macOS 미지원 환경에서 1회 warn",
          "[diagnostics][state_log]")
{
    SJH::test::GLContextFixture ctx;
    SJH::test::SpdlogCapture cap;

    GLStateLog::EnableAutoOnError(true);

    // macOS 환경 가정: warn 발생, 메시지에 "KHR_debug" 또는 "no-op" 포함
    // 실 환경(Windows/Linux)에서는 다른 출력 가능 — 그땐 본 케이스 [!mayfail] 태그 또는 #ifdef
#if defined(__APPLE__)
    REQUIRE((cap.Contains("KHR_debug") || cap.Contains("no-op")));
#endif
}
```

- [ ] **Step 5: CMake 등록** — `src/diagnostics/CMakeLists.txt`의 add_library에 `gl_state_log.cpp` 추가

```cmake
add_library(sjhopengl_diagnostics STATIC
    gl_log.cpp
    uniform_diagnostics.cpp
    gl_state_fields.cpp
    gl_state_log.cpp
)
```

`test/CMakeLists.txt`의 test_gl_state_capture 직후:

```cmake
#  diagnostics 모듈 — GLStateLog::Dump 출력 검증
add_executable(test_gl_state_log test_gl_state_log.cpp)
target_link_libraries(test_gl_state_log PRIVATE
    Catch2::Catch2WithMain
    gl_test_fixture
    spdlog_capture
    SJH::diagnostics
)
target_compile_features(test_gl_state_log PRIVATE cxx_std_17)
catch_discover_tests(test_gl_state_log)
```

`tests` umbrella target에 `test_gl_state_log` 추가.

- [ ] **Step 6: 빌드 + 테스트 → PASS 확인**

Run: `cmake --build build_Darwin -j --target tests && ctest --test-dir build_Darwin --output-on-failure -R "GLStateLog"`
Expected: 3개 케이스 모두 PASS.

- [ ] **Step 7: stage**

```bash
git add src/diagnostics/gl_state_log.h \
        src/diagnostics/gl_state_log.cpp \
        src/diagnostics/gl_state_fields.cpp \
        src/diagnostics/CMakeLists.txt \
        test/test_gl_state_log.cpp \
        test/CMakeLists.txt
# 권장 commit: feat(diagnostics): add GLStateLog::Dump + macOS no-op auto-error
```

---

## Task 6: GLStateSnapshot + Diff (test/support/)

**Goal**: 테스트 친화 wrapper. `Capture()` / `ToString()` (FieldsToString 위임) / `Diff(before, after)` (변화 필드만 출력).

**Files:**
- Create: `test/support/gl_state_snapshot.h`
- Create: `test/support/gl_state_snapshot.cpp`
- Create: `test/test_gl_state_snapshot.cpp`
- Modify: `test/CMakeLists.txt` (gl_state_snapshot STATIC lib + test_gl_state_snapshot executable)

- [ ] **Step 1: 헤더 작성** — `test/support/gl_state_snapshot.h`

```cpp
#ifndef __SJH_TEST_GL_STATE_SNAPSHOT_H__
#define __SJH_TEST_GL_STATE_SNAPSHOT_H__

#pragma once

#include "diagnostics/gl_state_fields.h"
#include <string>

namespace SJH::test
{
    /// production GLStateFields의 *얇은 wrapper* — Catch2 친화 메서드 추가.
    class GLStateSnapshot
    {
    public:
        SJH::Diagnostics::GLStateFields fields;

        /// CaptureGLState() 호출 — caller가 GL context 보장.
        static GLStateSnapshot Capture();

        /// 사람이 읽는 다중라인. INFO()로 던지기 좋음. FieldsToString에 위임.
        std::string ToString() const;
    };

    /// 변화한 필드만 출력. 변화 0건이면 "(no GL state change)\n".
    /// enum 필드: SymbolicName 적용.  GLuint 핸들: raw 정수 (의도된 비대칭).
    /// VAO=0 인 경우 element_buffer 라인에 주석 자동 포함 (변화 발생 시에 한해).
    std::string Diff(const GLStateSnapshot& before, const GLStateSnapshot& after);
}

#endif // __SJH_TEST_GL_STATE_SNAPSHOT_H__
```

- [ ] **Step 2: 구현 작성** — `test/support/gl_state_snapshot.cpp`

```cpp
#include "support/gl_state_snapshot.h"

#include <fmt/format.h>

namespace SJH::test
{
    GLStateSnapshot GLStateSnapshot::Capture()
    {
        return GLStateSnapshot{ SJH::Diagnostics::CaptureGLState() };
    }

    std::string GLStateSnapshot::ToString() const
    {
        return SJH::Diagnostics::FieldsToString(fields);
    }

    namespace {
        using SJH::Diagnostics::SymbolicName;

        // 단일 필드 비교 출력 — 변화 있으면 line 추가.
        template <typename T>
        void DiffField(std::string& out, const char* name, const T& a, const T& b) {
            if (a != b) out += fmt::format("  {}: {} → {}\n", name, a, b);
        }

        // enum 전용 (SymbolicName 적용)
        void DiffEnum(std::string& out, const char* name, GLenum a, GLenum b) {
            if (a != b) out += fmt::format("  {}: {} → {}\n", name, SymbolicName(a), SymbolicName(b));
        }
    }

    std::string Diff(const GLStateSnapshot& A, const GLStateSnapshot& B)
    {
        const auto& a = A.fields;
        const auto& b = B.fields;
        std::string out;

        // 핸들 (raw)
        DiffField(out, "vao",            a.vao,            b.vao);
        DiffField(out, "program",        a.program,        b.program);
        DiffField(out, "array_buffer",   a.array_buffer,   b.array_buffer);

        // element_buffer — VAO=0 시 주석 (변화 있을 때만)
        if (a.element_buffer != b.element_buffer) {
            const bool either_zero = (a.vao == 0 || b.vao == 0);
            if (either_zero) {
                out += fmt::format("  element_buffer: {} → {}  "
                                   "(note: EBO state is per-VAO; with VAO=0, this is always 0)\n",
                                   a.element_buffer, b.element_buffer);
            } else {
                out += fmt::format("  element_buffer: {} → {}\n",
                                   a.element_buffer, b.element_buffer);
            }
        }

        DiffField(out, "draw_fbo",       a.draw_fbo,       b.draw_fbo);
        DiffField(out, "read_fbo",       a.read_fbo,       b.read_fbo);
        DiffEnum (out, "active_texture", a.active_texture, b.active_texture);

        // 텍스처 unit
        for (int i = 0; i < 16; ++i) {
            if (a.texture_2d_per_unit[i] != b.texture_2d_per_unit[i]) {
                out += fmt::format("  tex_2d[unit {}]: {} → {}\n", i,
                                   a.texture_2d_per_unit[i], b.texture_2d_per_unit[i]);
            }
        }

        // viewport
        if (a.viewport != b.viewport) {
            out += fmt::format("  viewport: [{},{},{},{}] → [{},{},{},{}]\n",
                               a.viewport[0], a.viewport[1], a.viewport[2], a.viewport[3],
                               b.viewport[0], b.viewport[1], b.viewport[2], b.viewport[3]);
        }

        DiffField(out, "depth_test",     a.depth_test_enabled, b.depth_test_enabled);
        DiffEnum (out, "depth_func",     a.depth_func,         b.depth_func);
        DiffField(out, "depth_write",    a.depth_write_mask,   b.depth_write_mask);

        DiffField(out, "blend",          a.blend_enabled, b.blend_enabled);
        DiffEnum (out, "blend_src_rgb",  a.blend_src_rgb, b.blend_src_rgb);
        DiffEnum (out, "blend_dst_rgb",  a.blend_dst_rgb, b.blend_dst_rgb);

        DiffField(out, "cull_face",      a.cull_face_enabled, b.cull_face_enabled);
        DiffEnum (out, "cull_face_mode", a.cull_face_mode,    b.cull_face_mode);
        DiffEnum (out, "front_face",     a.front_face,        b.front_face);

        if (a.color_write_mask != b.color_write_mask) {
            auto fmt4 = [](const std::array<bool,4>& m){
                return fmt::format("[{},{},{},{}]",
                    m[0]?'R':'-', m[1]?'G':'-', m[2]?'B':'-', m[3]?'A':'-');
            };
            out += fmt::format("  color_write: {} → {}\n", fmt4(a.color_write_mask), fmt4(b.color_write_mask));
        }

        if (a.clear_color != b.clear_color) {
            out += fmt::format("  clear_color: [{:.3f},{:.3f},{:.3f},{:.3f}] → "
                                              "[{:.3f},{:.3f},{:.3f},{:.3f}]\n",
                a.clear_color[0], a.clear_color[1], a.clear_color[2], a.clear_color[3],
                b.clear_color[0], b.clear_color[1], b.clear_color[2], b.clear_color[3]);
        }

        if (out.empty()) return "(no GL state change)\n";
        return "GL State Diff:\n" + out;
    }
}
```

- [ ] **Step 3: 실패 테스트 작성** — `test/test_gl_state_snapshot.cpp`

```cpp
/**
 * @file test_gl_state_snapshot.cpp
 * @brief GLStateSnapshot::ToString + Diff 회귀.
 *        대부분 GL context 불필요 (struct 직접 구성으로 path 강제).
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "support/gl_state_snapshot.h"

using SJH::test::GLStateSnapshot;
using SJH::test::Diff;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::Equals;

TEST_CASE("Diff — 동일 snapshot은 '(no GL state change)' 단일 줄", "[snapshot][diff]")
{
    GLStateSnapshot a{};
    GLStateSnapshot b{};  // 둘 다 default 초기화 (모두 0/false)
    REQUIRE_THAT(Diff(a, b), Equals("(no GL state change)\n"));
}

TEST_CASE("Diff — handle 변화는 raw 정수 (비대칭)", "[snapshot][diff]")
{
    GLStateSnapshot a{}, b{};
    a.fields.vao = 3;
    b.fields.vao = 5;
    auto d = Diff(a, b);
    REQUIRE_THAT(d, ContainsSubstring("vao:"));
    REQUIRE_THAT(d, ContainsSubstring("3"));
    REQUIRE_THAT(d, ContainsSubstring("5"));
    REQUIRE_THAT(d, ContainsSubstring("→"));
}

TEST_CASE("Diff — enum 변화는 SymbolicName (비대칭)", "[snapshot][diff]")
{
    GLStateSnapshot a{}, b{};
    a.fields.depth_func = GL_LESS;
    b.fields.depth_func = GL_LEQUAL;
    auto d = Diff(a, b);
    REQUIRE_THAT(d, ContainsSubstring("depth_func"));
    REQUIRE_THAT(d, ContainsSubstring("GL_LESS"));
    REQUIRE_THAT(d, ContainsSubstring("GL_LEQUAL"));
}

TEST_CASE("Diff — element_buffer 변화 + VAO=0 시 EBO 주석", "[snapshot][diff]")
{
    GLStateSnapshot a{}, b{};
    a.fields.vao = 0;
    a.fields.element_buffer = 0;
    b.fields.vao = 0;
    b.fields.element_buffer = 7;  // 의미 없는 변화이지만 GL 가 0으로 보고할 것

    auto d = Diff(a, b);
    REQUIRE_THAT(d, ContainsSubstring("element_buffer"));
    REQUIRE_THAT(d, ContainsSubstring("EBO state is per-VAO"));
}

TEST_CASE("Diff — element_buffer 변화 + VAO≠0 이면 주석 미포함", "[snapshot][diff]")
{
    GLStateSnapshot a{}, b{};
    a.fields.vao = 3;
    a.fields.element_buffer = 0;
    b.fields.vao = 3;
    b.fields.element_buffer = 7;

    auto d = Diff(a, b);
    REQUIRE_THAT(d, ContainsSubstring("element_buffer"));
    REQUIRE_FALSE(d.find("EBO state is per-VAO") != std::string::npos);
}

TEST_CASE("Diff — texture unit 변화는 unit 번호 표시", "[snapshot][diff]")
{
    GLStateSnapshot a{}, b{};
    a.fields.texture_2d_per_unit[3] = 0;
    b.fields.texture_2d_per_unit[3] = 42;

    auto d = Diff(a, b);
    REQUIRE_THAT(d, ContainsSubstring("tex_2d[unit 3]"));
    REQUIRE_THAT(d, ContainsSubstring("42"));
}

TEST_CASE("ToString VAO=0 — 'EBO state is per-VAO' 주석 포함", "[snapshot][tostring]")
{
    GLStateSnapshot s{};
    s.fields.vao = 0;
    auto str = s.ToString();
    REQUIRE_THAT(str, ContainsSubstring("EBO state is per-VAO"));
}

TEST_CASE("ToString VAO≠0 — 주석 미포함", "[snapshot][tostring]")
{
    GLStateSnapshot s{};
    s.fields.vao = 3;
    auto str = s.ToString();
    REQUIRE_FALSE(str.find("EBO state is per-VAO") != std::string::npos);
}
```

- [ ] **Step 4: STATIC lib + executable 등록** — `test/CMakeLists.txt`의 spdlog_capture 직후

```cmake
#  GL state snapshot — 테스트 측 RAII + Diff (Task 6)
# SJH::diagnostics PUBLIC link → 소비 테스트가 GLStateFields 자동 가시.
add_library(gl_state_snapshot STATIC support/gl_state_snapshot.cpp)
target_link_libraries(gl_state_snapshot PUBLIC
    SJH::diagnostics
    glad::glad
)
target_link_libraries(gl_state_snapshot PRIVATE
    fmt::fmt
)
target_include_directories(gl_state_snapshot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_features(gl_state_snapshot PRIVATE cxx_std_17)
```

그리고 `test_gl_state_log` 직후 또는 직전에:

```cmake
#  GL state snapshot 회귀 — 대부분 GL context 불필요 (struct 직접 구성)
add_executable(test_gl_state_snapshot test_gl_state_snapshot.cpp)
target_link_libraries(test_gl_state_snapshot PRIVATE
    Catch2::Catch2WithMain
    gl_state_snapshot
)
target_compile_features(test_gl_state_snapshot PRIVATE cxx_std_17)
catch_discover_tests(test_gl_state_snapshot)
```

`tests` umbrella target에 `test_gl_state_snapshot` 추가.

- [ ] **Step 5: 빌드 + 테스트 → PASS 확인**

Run: `cmake --build build_Darwin -j --target tests && ctest --test-dir build_Darwin --output-on-failure -R "Diff|ToString"`
Expected: 8개 케이스 모두 PASS.

- [ ] **Step 6: stage**

```bash
git add test/support/gl_state_snapshot.h \
        test/support/gl_state_snapshot.cpp \
        test/test_gl_state_snapshot.cpp \
        test/CMakeLists.txt
# 권장 commit: feat(test/support): add GLStateSnapshot wrapper + Diff with VAO=0 EBO note
```

---

## Task 7: scripts/check_test_smells.py — 테스트 결함성 정적 검사

**Goal**: R1-R4 4개 규칙 (모두 warn 등급) — 신규 테스트 작성 시 SUCCEED-only / 단언 0 / tag 누락 자동 포착.

**Files:**
- Create: `scripts/check_test_smells.py`
- Modify: `test/CMakeLists.txt` (add_test로 ctest 통합)

- [ ] **Step 1: 스크립트 작성** — `scripts/check_test_smells.py`

```python
#!/usr/bin/env python3
"""
check_test_smells.py — 테스트 자체의 결함성 정적 검사.

규칙 (모두 warn 등급, 빌드/ctest 차단 X):
  R1: TEST_CASE 안에 단언 0개 (REQUIRE/CHECK/SUCCEED 모두 0)
  R2: SUCCEED-only 스모크 (body의 유일한 단언이 SUCCEED)
  R3: tag 누락 (TEST_CASE 두 번째 인자 빈 문자열 또는 누락)
  R4: disabled/skipped tag ([.] / [!hide] / [!shouldfail])

사용:
  python3 scripts/check_test_smells.py test/

근거: doc/testplan/2026-05-07-gl-state-and-test-quality-design.md §6.1
한계: 단일 라인 TEST_CASE 만 처리 (다중 라인은 Phase 2). 정규식 기반 — AST 정확도 X.
"""

import re
import sys
from pathlib import Path

# TEST_CASE("name", "[tag]") 또는 TEST_CASE("name") — 단일 라인 가정
TEST_CASE_RE = re.compile(
    r'TEST_CASE\s*\(\s*"(?P<name>[^"]*)"\s*(?:,\s*"(?P<tags>[^"]*)")?\s*\)\s*\{'
)
ASSERT_RE = re.compile(
    r'\b(REQUIRE|CHECK|REQUIRE_FALSE|CHECK_FALSE|REQUIRE_THROWS(?:_AS|_WITH|_MATCHES)?|CHECK_THROWS(?:_AS|_WITH|_MATCHES)?|SUCCEED|REQUIRE_THAT|CHECK_THAT)\s*\('
)
SUCCEED_RE = re.compile(r'\bSUCCEED\s*\(')

DISABLED_TAGS = ('[.]', '[!hide]', '[!shouldfail]')


def find_test_bodies(text):
    """단순 brace-counting으로 TEST_CASE body 추출. 단일 라인 헤더만."""
    out = []
    for m in TEST_CASE_RE.finditer(text):
        name = m.group('name')
        tags = m.group('tags') or ''
        # body 시작 = '{' 위치 + 1
        i = m.end() - 1  # '{' 위치 (regex가 \{에 매칭)
        depth = 1
        j = i + 1
        while j < len(text) and depth > 0:
            c = text[j]
            if c == '{': depth += 1
            elif c == '}': depth -= 1
            j += 1
        body = text[i+1:j-1] if depth == 0 else text[i+1:]
        line_no = text[:m.start()].count('\n') + 1
        out.append((line_no, name, tags, body))
    return out


def check_file(path):
    findings = []
    text = path.read_text(encoding='utf-8')
    for line_no, name, tags, body in find_test_bodies(text):
        asserts = ASSERT_RE.findall(body)
        succeeds = SUCCEED_RE.findall(body)

        # R1: 단언 0개
        if not asserts:
            findings.append((path, line_no, 'R1',
                f'TEST_CASE has no assertions: "{name}"'))

        # R2: SUCCEED-only (asserts에 SUCCEED만 있음)
        elif succeeds and len(asserts) == len(succeeds):
            findings.append((path, line_no, 'R2',
                f'SUCCEED-only test smell: "{name}" — '
                f'consider SpdlogCapture or behavioral assertion'))

        # R3: tag 누락
        if not tags.strip():
            findings.append((path, line_no, 'R3',
                f'TEST_CASE missing tag: "{name}" — '
                f'cannot filter via ctest -R'))

        # R4: disabled
        for d in DISABLED_TAGS:
            if d in tags:
                findings.append((path, line_no, 'R4',
                    f'TEST_CASE has disabled tag {d}: "{name}"'))
                break

    return findings


def main(argv):
    if len(argv) < 2:
        print('usage: check_test_smells.py <test_dir>', file=sys.stderr)
        return 2

    test_dir = Path(argv[1])
    if not test_dir.exists():
        print(f'error: {test_dir} not found', file=sys.stderr)
        return 2

    cpp_files = sorted(test_dir.rglob('test_*.cpp'))
    all_findings = []
    for p in cpp_files:
        all_findings.extend(check_file(p))

    if not all_findings:
        print(f'check_test_smells: 0 warnings across {len(cpp_files)} files')
        return 0

    for path, line, rule, msg in all_findings:
        print(f'{path}:{line}: warning [{rule}]: {msg}')

    print(f'\ncheck_test_smells: {len(all_findings)} warnings '
          f'across {len(cpp_files)} files (all warn-level, build not blocked)')
    return 0  # warn-only — 0 exit (R1-R4 모두 warn 정책)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
```

`chmod +x scripts/check_test_smells.py` 실행.

- [ ] **Step 2: 수동 실행 검증** — 새 테스트 파일들이 SUCCEED-only인지 직접 확인

Run: `python3 scripts/check_test_smells.py test/`
Expected (Task 1-6 완료 가정): 신규 테스트 파일들에는 R1/R2 0건. 기존 [test_uniform_diagnostics.cpp]도 Task 4 교체 후 0건.
실제로 0건이면 자가 sanity 통과.

- [ ] **Step 3: ctest 통합** — `test/CMakeLists.txt` 끝에 추가 (`tests` umbrella target 직후)

```cmake
#  Test smell linter — R1-R4 정적 검사 (warn-only, 빌드 차단 X)
# 본 ctest entry는 항상 PASS — warning은 stdout으로만.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME test_smells
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/scripts/check_test_smells.py
                ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_tests_properties(test_smells PROPERTIES LABELS "lint")
endif()
```

- [ ] **Step 4: ctest 통합 검증**

Run: `cmake --build build_Darwin -j --target tests && ctest --test-dir build_Darwin --output-on-failure -R "test_smells"`
Expected: PASS (또는 warning 출력 + PASS).

- [ ] **Step 5: stage**

```bash
git add scripts/check_test_smells.py \
        test/CMakeLists.txt
chmod +x scripts/check_test_smells.py
# 권장 commit: feat(scripts): add test smell linter (R1-R4, warn-only)
```

---

## Task 8: doc/test-quality-drill.md 메인 + 컴포넌트별 4개 파일

**Goal**: 사보타지 드릴 운영 절차 + 컴포넌트별 살아있는 표.

**Files:**
- Create: `doc/test-quality-drill.md`
- Create: `doc/test-quality-drill/gl_state_capture.md`
- Create: `doc/test-quality-drill/diff.md`
- Create: `doc/test-quality-drill/symbolic_name.md`
- Create: `doc/test-quality-drill/snapshot_tostring.md`

- [ ] **Step 1: 메인 문서 작성** — `doc/test-quality-drill.md`

```markdown
# Test Quality Drill — 사보타지 드릴 운영

> 본 문서는 [doc/testplan/2026-05-07-gl-state-and-test-quality-design.md](testplan/2026-05-07-gl-state-and-test-quality-design.md) §6.2 의 운영 산출물.

## 1. 개념

"테스트 통과"가 진짜 안전을 의미하는지 *적대적으로* 검증. 컴포넌트마다 3개의 그럴듯한 사보타지를 손으로 적용하고 ≥1 케이스 FAIL을 강제한다.

근거: testplan/testing-curriculum.md §부록 A.5 (적대적 사고) + A 논문 §8 (Mutation Score caveat).

## 2. 언제 실행하나

| 트리거 | 빈도 |
|---|---|
| 신규 테스트 인프라 컴포넌트 머지 직전 | 컴포넌트 1개당 1회 (필수) |
| 분기별 정기 — 기존 컴포넌트 대상 | 3개월마다 (선택) |
| production 회귀가 *기존 테스트를 통과한 채로* 슬립 | 그 컴포넌트 즉시 (의무) |

## 3. 절차 (4 step)

```bash
# 1. 안전 상태 확보
git status                 # clean working tree 확인
git stash --include-untracked

# 2. 한 사보타지씩 손으로 적용
$EDITOR src/diagnostics/gl_state_fields.cpp  # 표의 사보타지 1번을 직접 적용
cmake --build build_Darwin -j --target tests
ctest --test-dir build_Darwin --output-on-failure
# → 결과 기록 (어느 케이스가 FAIL했는지)

# 3. 복원
git checkout -- src/diagnostics/gl_state_fields.cpp

# 4. 컴포넌트별 표 갱신 (다음 섹션의 링크된 파일)
$EDITOR doc/test-quality-drill/gl_state_capture.md
git add doc/test-quality-drill/gl_state_capture.md
git commit -m "test: drill record for CaptureGLState (sabotage 1/3)"
```

## 4. 컴포넌트별 표 (살아있는 문서)

각 컴포넌트의 사보타지 표는 분리 파일로 관리 — diff 노이즈 최소화 + git blame 친화.

| 컴포넌트 | 표 파일 | 마지막 드릴 |
|---|---|---|
| CaptureGLState | [test-quality-drill/gl_state_capture.md](test-quality-drill/gl_state_capture.md) | (드릴 후 갱신) |
| Diff | [test-quality-drill/diff.md](test-quality-drill/diff.md) | (드릴 후 갱신) |
| SymbolicName | [test-quality-drill/symbolic_name.md](test-quality-drill/symbolic_name.md) | (드릴 후 갱신) |
| GLStateSnapshot::ToString | [test-quality-drill/snapshot_tostring.md](test-quality-drill/snapshot_tostring.md) | (드릴 후 갱신) |

## 5. 결과 해석

- **모든 사보타지 ≥1 케이스 FAIL** → 합격. 표에 FAIL한 케이스 기록.
- **어떤 사보타지가 0 케이스 FAIL** → blind spot. 그 카테고리에 케이스 추가 후 재드릴.
- **사보타지를 잡은 케이스가 *예상과 다름*** → 케이스 의도 모호 (이름/주석 보강).

## 6. Mutation Testing 보류 — 진입 트리거

현재 mull 도입 안 함 (macOS arm64 LLVM 매칭 부담, 컴포넌트 4개로 ROI ↓).

**다음 중 하나라도 발생 시 도입 검토**:

1. 신규 테스트 인프라 컴포넌트 ≥ 10개
2. production 회귀가 *기존 테스트를 통과한 채로* 슬립
3. CI 시간 < 5분 + 머신 여유
4. `.claude/agents/render-quality-gate.md` PoC 시점 도달

지금은 사람이 *어떤 변경이 그럴듯한가*를 판단하는 학습 가치를 우선.
```

- [ ] **Step 2: 컴포넌트 표 1 — `doc/test-quality-drill/gl_state_capture.md`**

```markdown
# Sabotage Drill — CaptureGLState

> [메인 문서로](../test-quality-drill.md)

## 표 (예측 vs 실측)

| 사보타지 | 적용 위치 | 예상 잡는 케이스 | 실제 잡힌 케이스 | 드릴 날짜 |
|---|---|---|---|---|
| GL_VERTEX_ARRAY_BINDING ↔ GL_CURRENT_PROGRAM swap | src/diagnostics/gl_state_fields.cpp의 vao 라인 | test_gl_state_capture.cpp "fresh fixture default" | (실측 후 기재) | YYYY-MM-DD |
| 텍스처 unit loop `i < 16` → `i < 1` | src/diagnostics/gl_state_fields.cpp 텍스처 순회부 | test_gl_state_capture.cpp "결정성" (만약 unit 1+ 가 0 아닌 경우) — 가능하면 실측 시 unit 5에 텍스처 바인딩 후 드릴 | (실측) | YYYY-MM-DD |
| `glActiveTexture(saved_active)` 복원 누락 | src/diagnostics/gl_state_fields.cpp의 텍스처 unit 순회 끝 | test_gl_state_capture.cpp "부수효과 0 — active_texture 변하지 않음" | (실측) | YYYY-MM-DD |

## 결과 노트

(드릴 실행 후 채움)

- 예측이 정확했나?
- 잡지 못한 사보타지 → 추가한 케이스
- 잡았지만 케이스명이 의도를 안 드러내면 → 리네임 기록
```

- [ ] **Step 3: 컴포넌트 표 2 — `doc/test-quality-drill/diff.md`**

```markdown
# Sabotage Drill — Diff

> [메인 문서로](../test-quality-drill.md)

| 사보타지 | 적용 위치 | 예상 잡는 케이스 | 실제 | 드릴 날짜 |
|---|---|---|---|---|
| 변화 무관 항상 `"(no GL state change)\n"` 반환 | test/support/gl_state_snapshot.cpp Diff 함수 첫 줄에 `return "(no GL state change)\n";` 강제 | test_gl_state_snapshot.cpp "Diff — handle 변화는 raw 정수" 등 변화 케이스 다수 | (실측) | YYYY-MM-DD |
| 변화 없는 필드도 출력 (DiffField에서 `if (a != b)` 제거) | test/support/gl_state_snapshot.cpp DiffField helper | test_gl_state_snapshot.cpp "Diff — 동일 snapshot은 '(no GL state change)'" — 출력이 비지 않을 것 | (실측) | YYYY-MM-DD |
| before/after 인자 swap (Diff 본문에서 a/b 교환) | test/support/gl_state_snapshot.cpp Diff | "Diff — handle 변화는 raw 정수" — `3 → 5` 가 `5 → 3`로 출력. 케이스가 substring "3"과 "5" 둘 다 검사하므로 잡힘 X 가능. **잡히지 않으면 → 케이스 강화 필요 (어느 쪽이 before, 어느 쪽이 after인지 단언)** | (실측) | YYYY-MM-DD |

## 결과 노트

(드릴 실행 후)
```

- [ ] **Step 4: 컴포넌트 표 3 — `doc/test-quality-drill/symbolic_name.md`**

```markdown
# Sabotage Drill — SymbolicName

> [메인 문서로](../test-quality-drill.md)

| 사보타지 | 적용 위치 | 예상 잡는 케이스 | 실제 | 드릴 날짜 |
|---|---|---|---|---|
| unknown enum → 사전 첫 entry 반환 (default fallback이 "GL_NEVER" 같은 식) | src/diagnostics/gl_state_fields.cpp SymbolicName의 hex fallback | test_gl_state_fields.cpp "미적중 → hex fallback" — `"0xDEAD"` 단언 깨짐 | (실측) | YYYY-MM-DD |
| 결과 lowercase (`"gl_less"`) | snprintf 또는 case의 string lit 손상 | "사전 적중" — `Equals("GL_LESS")` 정확 매칭 깨짐 | (실측) | YYYY-MM-DD |
| 사전에서 `case GL_LESS: return "GL_LESS";` 라인 삭제 | src/diagnostics/gl_state_fields.cpp | "사전 적중" + "depth_func 모든 8개" 둘 다 깨짐 | (실측) | YYYY-MM-DD |
| **(N11 from §Implementation Notes)** vertex type case 삭제 (`case GL_FLOAT: return "GL_FLOAT";`) | src/diagnostics/gl_state_fields.cpp SymbolicName | test_gl_state_log.cpp "FieldsToString — attribute enabled 시 ..." → `0x1406` 출력으로 hex fallback. attribute layout 출력에서 type이 hex로 새는지 검증. | (실측) | YYYY-MM-DD |

## 결과 노트

(드릴 실행 후)
```

- [ ] **Step 5: 컴포넌트 표 4 — `doc/test-quality-drill/snapshot_tostring.md`**

```markdown
# Sabotage Drill — GLStateSnapshot::ToString (FieldsToString)

> [메인 문서로](../test-quality-drill.md)

| 사보타지 | 적용 위치 | 예상 잡는 케이스 | 실제 | 드릴 날짜 |
|---|---|---|---|---|
| VAO=0 EBO 주석을 *항상* 출력 (vao 검사 제거) | src/diagnostics/gl_state_fields.cpp FieldsToString의 element_buffer 라인 | test_gl_state_snapshot.cpp "ToString VAO≠0 — 주석 미포함" | (실측) | YYYY-MM-DD |
| VAO=0 EBO 주석을 *절대* 출력 안 함 | 같은 위치, 주석 분기 제거 | "ToString VAO=0 — 주석 포함" | (실측) | YYYY-MM-DD |
| enum 자리에 raw 정수 출력 (SymbolicName 호출 제거) | FieldsToString의 depth_func / blend_src_rgb 등 | test_gl_state_snapshot.cpp "Diff — enum 변화는 SymbolicName" + test_gl_state_log.cpp 일부 | (실측) | YYYY-MM-DD |

## 결과 노트

(드릴 실행 후)
```

- [ ] **Step 5b: 컴포넌트 표 5 (신규) — `doc/test-quality-drill/smell_linter.md`**

본 컴포넌트는 plan 후속 발견 (N10) 결과로 추가. smell linter 자체의 회귀 감지력 검증.

```markdown
# Sabotage Drill — Test Smell Linter (check_test_smells.py)

> [메인 문서로](../test-quality-drill.md)

| 사보타지 | 적용 위치 | 예상 잡는 케이스 / 절차 | 실제 | 드릴 날짜 |
|---|---|---|---|---|
| **(N10)** ASSERT_RE에서 `STATIC_REQUIRE\|STATIC_CHECK` alternation 삭제 | scripts/check_test_smells.py ASSERT_RE | `python3 scripts/check_test_smells.py test/` → test_glfw_utils.cpp에 R1 false positive 6건 발생 | (실측) | YYYY-MM-DD |
| ASSERT_RE의 `REQUIRE` alternation 삭제 (가장 흔한 매크로 미감지) | 동상 | `python3 scripts/check_test_smells.py test/` → 거의 모든 테스트가 R1 false positive | (실측) | YYYY-MM-DD |
| TEST_CASE_RE의 tag capture를 누락 (두 번째 인자 무시) | 동상 | 모든 TEST_CASE가 R3 (tag 누락)으로 false positive 생성 | (실측) | YYYY-MM-DD |

## 결과 노트

(드릴 실행 후)

본 표는 *linter 자체의 회귀 감지*용. smell linter 코드를 수정할 때마다 본 드릴로 false positive 폭증이 없는지 검증.
```

- [ ] **Step 6: stage**

```bash
git add doc/test-quality-drill.md \
        doc/test-quality-drill/
# 권장 commit: docs: add sabotage drill ops + per-component tables
```

---

## Task 9: 사보타지 드릴 4회 수동 실행 + 결과 기록

**Goal**: 4개 컴포넌트 × 3개 사보타지 = 12회 실행. Task 8의 "(실측)" 자리를 채움.

**Files:**
- Modify: `doc/test-quality-drill/<component>.md` × 4 (실측 결과 기록)

- [ ] **Step 1: 사전 점검** — clean working tree

```bash
git status
# Expected: working tree clean (또는 untracked만)
```

clean이 아니면 stash:
```bash
git stash --include-untracked
```

- [ ] **Step 2: CaptureGLState 드릴 — 사보타지 1**

`src/diagnostics/gl_state_fields.cpp`의 CaptureGLState에서:
```cpp
glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &tmp); f.vao = ...
glGetIntegerv(GL_CURRENT_PROGRAM,      &tmp); f.program = ...
```
의 두 줄을 swap. (vao에 program이 들어가고 vice versa)

```bash
cmake --build build_Darwin -j --target tests
ctest --test-dir build_Darwin --output-on-failure -R "capture"
# Expected: ≥1 FAIL — "fresh fixture default" 또는 "VAO 바인딩 후 fields.vao 반영"
# 결과 기록 → doc/test-quality-drill/gl_state_capture.md 표의 "실제 잡힌 케이스"

git checkout -- src/diagnostics/gl_state_fields.cpp
ctest --test-dir build_Darwin --output-on-failure -R "capture"
# Expected: 모두 PASS (복원 검증)
```

- [ ] **Step 3: CaptureGLState 드릴 — 사보타지 2 (unit loop)**

`src/diagnostics/gl_state_fields.cpp`의 텍스처 순회 `for (int i = 0; i < 16; ++i)`를 `for (int i = 0; i < 1; ++i)`로 변경.

```bash
cmake --build build_Darwin -j --target tests
ctest --test-dir build_Darwin --output-on-failure -R "capture"
# Expected: 잡힐 가능성 — fresh fixture에서 모든 unit이 0이라면 *잡히지 않음*.
# 잡히지 않으면 → "blind spot" 발견 → 케이스 추가 후 재드릴.
# 케이스 추가 예: "텍스처 unit 5 바인딩 후 capture에 반영" — 사용자가 직접 추가.

git checkout -- src/diagnostics/gl_state_fields.cpp
```

- [ ] **Step 4: CaptureGLState 드릴 — 사보타지 3 (active_texture 복원 누락)**

`src/diagnostics/gl_state_fields.cpp`의 `glActiveTexture(static_cast<GLenum>(saved_active));` 라인 삭제 또는 주석 처리.

```bash
cmake --build build_Darwin -j --target tests
ctest --test-dir build_Darwin --output-on-failure -R "capture"
# Expected: "부수효과 0 — active_texture 변하지 않음" FAIL.

git checkout -- src/diagnostics/gl_state_fields.cpp
```

- [ ] **Step 5: Diff 드릴 (3 사보타지)**

`test/support/gl_state_snapshot.cpp`의 Diff 함수에 차례로 적용:
1. 함수 첫 줄에 `return "(no GL state change)\n";` 추가
2. `DiffField` helper에서 `if (a != b)` 검사 제거
3. Diff 본문에서 `const auto& a = A.fields; const auto& b = B.fields;` 의 A/B swap

매번 `cmake --build && ctest -R snapshot` 실행 후 `git checkout`. 각 결과를 [doc/test-quality-drill/diff.md](../test-quality-drill/diff.md) 에 기록.

- [ ] **Step 6: SymbolicName 드릴 (3 사보타지)**

`src/diagnostics/gl_state_fields.cpp`의 SymbolicName에 차례로 적용:
1. hex fallback의 `snprintf` 결과를 `"GL_NEVER"`로 강제 (사전 첫 entry)
2. switch case의 `return "GL_LESS";`를 `return "gl_less";`로 변경
3. `case GL_LESS: return "GL_LESS";` 라인 삭제

매번 빌드/테스트 후 [doc/test-quality-drill/symbolic_name.md](../test-quality-drill/symbolic_name.md) 갱신.

- [ ] **Step 7: ToString 드릴 (3 사보타지)**

`src/diagnostics/gl_state_fields.cpp`의 FieldsToString에 차례로 적용:
1. element_buffer 라인의 `if (f.vao == 0)` 분기를 `if (true)`로 강제
2. 같은 분기를 `if (false)`로 강제
3. `SymbolicName(f.depth_func)` 호출을 `f.depth_func`로 raw 정수 출력

매번 빌드/테스트 후 [doc/test-quality-drill/snapshot_tostring.md](../test-quality-drill/snapshot_tostring.md) 갱신.

- [ ] **Step 8: 결과 분석 + 누락 발견 시 케이스 추가**

각 표를 검토:
- 모든 12개 사보타지가 ≥1 케이스로 잡혔는가?
- 잡지 못한 사보타지 → 그 카테고리의 케이스를 *실제 테스트 파일에 추가*하고 → 다시 빌드/테스트 통과 확인 → 재드릴.
- 결과 노트 채우기 (예측 vs 실측 일치도, 발견된 blind spot, 추가한 케이스).

- [ ] **Step 9: stage**

```bash
git add doc/test-quality-drill/
# 추가 케이스가 있다면 그 test_*.cpp 도 add
# 권장 commit: test: sabotage drill results for 4 components (12 sabotages, recorded findings)
```

---

## Self-Review

### 1. Spec coverage

각 spec 섹션별 task 매핑:

| Spec § | 내용 | 매핑된 Task |
|---|---|---|
| 0 | 목적/비목적 | (전체 Plan) |
| 1 | 산출물 인벤토리 | Task 1-9 (모두 커버) |
| 2.1 | gl_state_fields | Task 1, 2, 5 (FieldsToString) |
| 2.2 | gl_state_log | Task 5 |
| 2.3 | gl_state_snapshot | Task 6 |
| 2.4 | spdlog_capture | Task 3 |
| 3 | Data flow + 가드 | Task 2 (capture 시퀀스), Task 5 (no-op 가드) |
| 4 | Edge cases | Task 2 (drain/post-check), Task 5 (no-op), Task 6 (VAO=0 주석 + Diff 0건) |
| 5.1 | 신규 테스트 4개 | Task 1, 2, 5, 6 |
| 5.2 | spdlog 캡처 + SUCCEED 교체 | Task 3 + Task 4 (같은 작업 흐름) |
| 5.3 | 사보타지 드릴 | Task 9 |
| 6.1 | smell linter | Task 7 |
| 6.2 | drill 운영 문서 | Task 8 |
| 6.3 | mutation 보류 | Task 8 §6 |
| 7 | 구현 순서 9 step | Task 1-9 |

**갭 0**.

### 2. Placeholder scan

- "TBD/TODO/implement later/fill in details": 0건. (Task 5 EnableAutoOnError 구현에 `TODO(future): KHR_debug callback 등록` 1건 — 이는 *코드 안의 의도된 future work 표시*이지 plan placeholder가 아님)
- "Add appropriate error handling": 0건.
- "Write tests for the above": 0건 (모든 테스트 코드 명시).
- "Similar to Task N": 0건.
- 정의되지 않은 함수/타입: 점검 — `FieldsToString` (Task 1 stub → Task 5 본격 구현, OK), `CaptureGLState` (Task 1 stub → Task 2 구현, OK), `SymbolicName` (Task 1 구현, OK), `GLStateSnapshot` (Task 6, OK), `Diff` (Task 6, OK), `SpdlogCapture` (Task 3, OK), `GLStateLog::Dump` / `EnableAutoOnError` (Task 5, OK).

### 3. Type consistency

- `GLStateFields`: Task 1 헤더 → Task 2/5 구현/사용 일관 (struct field names 동일).
- `SymbolicName(GLenum)` 시그니처: Task 1 헤더 → Task 6 Diff 사용 일관.
- `FieldsToString(const GLStateFields&)`: Task 1 헤더 → Task 5 구현 → Task 6 ToString 위임 일관.
- `GLStateSnapshot::fields`: Task 6 헤더 → Task 6 테스트의 `s.fields.vao = 0` 사용 일관.
- `SpdlogCapture::Contains(string_view)` / `Lines()`: Task 3 헤더 → Task 4/5 사용 일관.

**불일치 0**.

---

## 권장 워크플로우 (Plan-as-Reference)

사용자의 [auto memory phase-implementation-mode 정책](../../.claude/MEMORY.md) 에 따라:

1. **Task 1부터 순서대로** 진행 (Task 1 → 2 → 3 → ... → 9)
2. **각 Task 안의 Step 1, 2, 3, ...** 을 그대로 따름 (TDD red → green → stage 패턴)
3. Step 안의 코드 블록은:
   - **테스트 코드**: 그대로 사용 가능 (red phase의 계약)
   - **구현 코드**: 컨벤션 가이드 — 사용자가 변형 가능
4. 의문 생기면 [spec](2026-05-07-gl-state-and-test-quality-design.md) 의 해당 섹션 참조 (plan이 §번호로 가리킴)
5. Task 완료 후 ctest 결과를 Claude에 공유 → review 요청
