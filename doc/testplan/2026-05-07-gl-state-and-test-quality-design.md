# GL State Introspection + Test Quality Monitoring — Design Spec

> **Date**: 2026-05-07 (재작성: 2026-05-09 — `docs/superpowers/specs/` 위치 소실 후 `doc/testplan/`로 이동)
> **Author**: brainstorming session (Claude Code + 사용자)
> **Status**: Approved, awaiting implementation
> **Branch**: `newenv`

---

## 0. 목적과 비목적

### 목적
사용자가 식별한 두 가지 통증 — *(1) 리팩토링·렌더링 디버깅 시 GL 상태 불가시성*, *(2) 테스트 자체의 결함성 미감지* — 을 동시에 해소하는 **테스트 인프라 1차 확장**.

### 비목적 (intentionally out-of-scope)
- 시각적 회귀(Visual regression) 골든 이미지 — [testing-curriculum.md](testing-curriculum.md) Phase B2-B4의 별도 작업
- 5-에이전트 운영 파이프라인 부트스트랩 — [.claude/agents/render-*.md](../../.claude/agents/) 의 PoC는 *실제 사용 시점*에 별도 spec
- Mutation testing (mull) 자동화 — §6.3 트리거 미충족
- DirectX/Vulkan 백엔드 — [.claude/CLAUDE.md](../../.claude/CLAUDE.md) §F 진화 시점 후보
- Render pass 추상화 / RHI 도입 — 학습 프로젝트엔 YAGNI

---

## 1. 산출물 인벤토리

| # | 경로 | 종류 | 신규/수정 |
|---|---|---|---|
| 1 | `src/diagnostics/gl_state_fields.h/.cpp` | 공통 데이터 모델 + SymbolicName + CaptureGLState + FieldsToString | 신규 |
| 2 | `src/diagnostics/gl_state_log.h/.cpp` | Production 측 한 줄 덤프 | 신규 |
| 3 | `test/support/gl_state_snapshot.h/.cpp` | 테스트 측 RAII + Diff | 신규 |
| 4 | `test/support/spdlog_capture.h/.cpp` | spdlog 출력 캡처 RAII | 신규 |
| 5 | `test/test_gl_state_fields.cpp` | (1)의 회귀 테스트 | 신규 |
| 6 | `test/test_gl_state_capture.cpp` | (1)+(2)의 GL context 회귀 테스트 | 신규 |
| 7 | `test/test_gl_state_snapshot.cpp` | (3)의 회귀 테스트 | 신규 |
| 8 | `test/test_gl_state_log.cpp` | (2)의 회귀 테스트 (spdlog 캡처 사용) | 신규 |
| 9 | `test/test_uniform_diagnostics.cpp` | `SUCCEED("...crash 없음")` → SpdlogCapture 단언으로 교체 | **수정** |
| 10 | `scripts/check_test_smells.py` | 테스트 smell 정적 검사 | 신규 |
| 11 | `doc/test-quality-drill.md` | 사보타지 드릴 운영 문서 | 신규 |
| 12 | `doc/test-quality-drill/<component>.md` | 컴포넌트별 드릴 표 | 신규 (4개) |
| 13 | CMake 통합 (test/CMakeLists.txt 등) | smell linter ctest 등록 | 수정 |

**LoC 견적**: ~700 LoC (C++ ~430 + Python ~120 + Markdown ~150)
**시간 견적**: 4-6시간 (테스트 작성 포함, 사보타지 드릴 별도 ~30분/컴포넌트)
**신규 외부 의존성**: 0 (모두 glad/glfw/spdlog/Catch2/Python3 표준)

---

## 2. 컴포넌트별 API

### 2.1 `src/diagnostics/gl_state_fields.h`

```cpp
namespace SJH::Diagnostics
{
    /// 한 시점의 GL 상태. log + snapshot 양쪽이 공유.
    /// @note 캡처 단위는 GL spec 상 *동기적*(state queries are synchronous)이므로
    ///       glFinish 호출 안 함. caller 컨텍스트 보장 필요.
    struct GLStateFields
    {
        // 바인딩
        GLuint vao{0};
        GLuint program{0};
        GLuint array_buffer{0};
        GLuint element_buffer{0};
        GLuint draw_fbo{0};
        GLuint read_fbo{0};

        // 텍스처 (macOS GL 3.3 spec 상한 16, ToString은 0이 아닌 unit만 출력)
        GLenum active_texture{GL_TEXTURE0};
        std::array<GLuint, 16> texture_2d_per_unit{};

        // viewport
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
    /// @pre  GL context active (caller 책임 — fixture가 보장)
    /// @post caller가 보낸 직전 호출의 결과 — error queue를 drain 후 capture
    GLStateFields CaptureGLState();

    /// GLenum → 사람이 읽는 이름. 사전 ~40개 미적중 시 "0xXXXX" hex.
    ///
    /// @note SymbolicName(0) == "GL_ZERO" 컨벤션:
    ///       OpenGL spec 상 0은 GL_NONE/GL_ZERO/GL_FALSE/GL_POINTS 모두에 매핑됨.
    ///       본 프로젝트의 17개 캡처 필드 한정 시 *enum 컨텍스트의 0*은 blend factor
    ///       (blend_src_rgb / blend_dst_rgb)에서만 합법적으로 발생 → GL_ZERO 가 정확.
    ///       GL_TEXTURE_COMPARE_MODE 같은 GL_NONE-context 필드를 미래 추가 시
    ///       SymbolicName을 *필드별 함수 포인터*로 분기 (현재는 YAGNI).
    const char* SymbolicName(GLenum e);
}
```

**사전 등록 enum (~40)**: depth_func 8개, blend factor 15개 (GL 3.3 core), cull/front 5개, GL_TEXTUREn 동적 16. 나머지는 hex fallback.

### 2.2 `src/diagnostics/gl_state_log.h`

```cpp
namespace SJH::Diagnostics
{
    class GLStateLog
    {
    public:
        /// 현재 GL 상태를 spdlog::info로 한 번 덤프. 매 프레임 호출 금지.
        static void Dump(std::string_view tag = {});

        /// KHR_debug 콜백 활성화 시 GL_DEBUG_SEVERITY_HIGH 발생 직후 자동 Dump.
        /// macOS GL 3.3은 KHR_debug 미지원 → 1회 warn 후 no-op (std::call_once).
        static void EnableAutoOnError(bool enable);
    };
}
```

### 2.3 `test/support/gl_state_snapshot.h`

```cpp
namespace SJH::test
{
    class GLStateSnapshot
    {
    public:
        SJH::Diagnostics::GLStateFields fields;
        static GLStateSnapshot Capture();
        std::string ToString() const;
    };

    /// 변화한 필드만 출력. 변화 0건이면 "(no GL state change)\n" 단일 줄.
    /// enum 필드는 SymbolicName 적용, GLuint 핸들은 raw 정수 (의도된 비대칭).
    /// VAO=0인 경우 element_buffer 라인에 주석 추가:
    ///   "element_buffer: 0  (note: EBO state is per-VAO; with VAO=0, this is always 0)"
    std::string Diff(const GLStateSnapshot& before, const GLStateSnapshot& after);
}
```

**Catch2 통합 패턴**:
```cpp
auto before = SJH::test::GLStateSnapshot::Capture();
buf->Bind();
auto after = SJH::test::GLStateSnapshot::Capture();
INFO(SJH::test::Diff(before, after));   // PASS면 출력 X, FAIL이면 자동 출력
REQUIRE(after.fields.element_buffer == buf->Get());
```

### 2.4 `test/support/spdlog_capture.h`

```cpp
namespace SJH::test
{
class SpdlogCapture
{
public:
    SpdlogCapture();           // RAII: default logger를 ostringstream sink로 교체
    ~SpdlogCapture();          // 원래 logger 복원
    std::string Lines() const;
    bool Contains(std::string_view s) const;
private:
    std::shared_ptr<spdlog::logger> mPrev;
    std::shared_ptr<std::ostringstream> mStream;
};
}
```

---

## 3. Data Flow & Cross-Platform 가드

### 3.1 `CaptureGLState()` 호출 시퀀스

```
1. drain pre-existing errors:  while (glGetError()) {}
2. 17개 필드 query (glIsEnabled / glGetIntegerv / glGetFloatv / glGetBooleanv)
3. active_texture 보존:
       glGetIntegerv(GL_ACTIVE_TEXTURE, &saved);
       for i in [0, 16): glActiveTexture(GL_TEXTURE0+i); glGetIntegerv(GL_TEXTURE_BINDING_2D, ...)
       glActiveTexture(saved);
4. post-check: glGetError() — non-zero면 spdlog::warn (partial 데이터 반환)
```

### 3.2 글로벌 함수 매핑 표

| 필드 | 함수 | enum |
|---|---|---|
| vao | glGetIntegerv | GL_VERTEX_ARRAY_BINDING |
| program | glGetIntegerv | GL_CURRENT_PROGRAM |
| array_buffer | glGetIntegerv | GL_ARRAY_BUFFER_BINDING |
| element_buffer | glGetIntegerv | GL_ELEMENT_ARRAY_BUFFER_BINDING |
| draw_fbo / read_fbo | glGetIntegerv | GL_DRAW_FRAMEBUFFER_BINDING / GL_READ_FRAMEBUFFER_BINDING |
| active_texture | glGetIntegerv | GL_ACTIVE_TEXTURE |
| viewport (4) | glGetIntegerv | GL_VIEWPORT |
| depth_test_enabled | glIsEnabled | GL_DEPTH_TEST |
| depth_func | glGetIntegerv | GL_DEPTH_FUNC |
| depth_write_mask | glGetBooleanv | GL_DEPTH_WRITEMASK |
| blend_enabled | glIsEnabled | GL_BLEND |
| blend_src_rgb / dst_rgb | glGetIntegerv | GL_BLEND_SRC_RGB / GL_BLEND_DST_RGB |
| cull_face_enabled | glIsEnabled | GL_CULL_FACE |
| cull_face_mode | glGetIntegerv | GL_CULL_FACE_MODE |
| front_face | glGetIntegerv | GL_FRONT_FACE |
| color_write_mask (4) | glGetBooleanv | GL_COLOR_WRITEMASK |
| clear_color (4) | glGetFloatv | GL_COLOR_CLEAR_VALUE |

### 3.3 크로스 플랫폼 가드

| 환경 | KHR_debug | EnableAutoOnError 동작 |
|---|---|---|
| macOS arm64 (GL 3.3) | ❌ | std::call_once warn → no-op |
| Windows x64 | ✅ | 정상 동작 |
| llvmpipe (Linux/CI) | ✅ | 정상 동작 |

```cpp
void GLStateLog::EnableAutoOnError(bool) {
#if defined(GL_VERSION_4_3) || defined(GL_KHR_debug)
    if (glDebugMessageCallback != nullptr) { /* register */ return; }
#endif
    static std::once_flag warned;
    std::call_once(warned, [](){
        spdlog::warn("[GLStateLog] EnableAutoOnError: KHR_debug 미지원 환경 — no-op");
    });
}
```

### 3.4 결정성

`GLContextFixture`(256x256 hidden window)가 viewport를 강제 → 기존 fixture 사용 테스트는 자동으로 결정적. 추가 작업 없음.

---

## 4. Error Handling & Edge Cases

| # | 케이스 | 결정 |
|---|---|---|
| 4.1 | GL context 없이 Capture() | caller 책임 — 검사 안 함 ([.claude/architecture.md](../../.claude/architecture.md) §3 컨벤션) |
| 4.2 | glGetError non-zero (캡처 자체 또는 잔여) | drain → capture → post-check warn → **17 필드 모두 채운 채 반환** (값 정확성이 의심된다는 신호 — caller가 warn 메시지로 판단) |
| 4.3 | SymbolicName(unknown) | `"0xXXXX"` hex fallback |
| 4.4 | VAO=0 + element_buffer | ToString에 주석 출력 ("EBO state is per-VAO; with VAO=0, this is always 0") |
| 4.5 | Diff 변화 0건 | 정확히 `"(no GL state change)\n"` |
| 4.6 | 멀티스레드 / 재진입 / 메모리 부족 / overflow | **방어 안 함** (caller 보장 / 표준 예외 / Catch2 무제한) |

---

## 5. Meta Testing — 테스트 인프라 자체의 회귀 검증

### 5.1 신규 테스트 파일

| 파일 | GL ctx | 케이스 수 | 핵심 단언 |
|---|---|---|---|
| test_gl_state_fields.cpp | ❌ | ~6 | SymbolicName 사전 적중/미적중/boundary, GL_ZERO 정책 |
| test_gl_state_capture.cpp | ✅ | ~5 | 결정성(2회 byte-equal), 부수효과 0, GL_NO_ERROR, fresh default, bind 반영 |
| test_gl_state_snapshot.cpp | 일부 | ~8 | Diff 4종(0건/enum/handle/혼합), ToString VAO=0 주석 positive+negative |
| test_gl_state_log.cpp | ✅ | ~3 | Dump tag 포함, 핸들 출력, macOS warn-once |

**모두 SUCCEED-only 금지** (R2 smell 회피).

### 5.2 기존 테스트 개선 (같은 PR)

`test/test_uniform_diagnostics.cpp:16-66` 의 3개 `SUCCEED("...crash 없음")`을 **`SpdlogCapture` 단언으로 교체**:
- NotifyMissing(p, n) 1회 → Lines가 `n` 포함
- NotifyMissing(p, n) 2회 → 추가 Lines 출력 X (warn-once 검증)
- NotifyTypeMismatch 시나리오별 → Contains/미포함 단언

### 5.3 사보타지 드릴 (수동, 컴포넌트별)

각 신규 컴포넌트 머지 직전 1회. [testing-curriculum.md](testing-curriculum.md) §부록 A.5 *적대적 사고*의 절차화.

**컴포넌트별 사보타지 표 초안**:

| 컴포넌트 | 사보타지 1 | 사보타지 2 | 사보타지 3 |
|---|---|---|---|
| CaptureGLState | GL_VERTEX_ARRAY_BINDING ↔ GL_CURRENT_PROGRAM swap | unit loop `i<16` → `i<1` | glActiveTexture 복원 누락 |
| Diff | 변화 무관 항상 "(no change)" 반환 | 변화 없는 필드도 출력 | before/after 인자 swap |
| SymbolicName | unknown → 사전 첫 entry 반환 | 결과 lowercase | 사전에서 한 entry 누락 |
| GLStateSnapshot::ToString | VAO=0 주석 항상 출력 | 주석 절대 출력 안 함 | enum 자리에 raw 출력 |

**절차** (4 step):
1. git stash로 안전 상태 보존
2. 한 사보타지씩 손으로 적용 → ctest 실행 → 결과 기록 → git checkout으로 복원
3. 모든 사보타지가 ≥1 케이스 FAIL → 합격
4. 잡히지 않은 사보타지 → 그 카테고리에 케이스 추가 후 재드릴

---

## 6. Test Quality Monitoring (운영 산출물)

### 6.1 `scripts/check_test_smells.py`

| Rule | 패턴 | 등급 |
|---|---|---|
| **R1**: TEST_CASE에 단언 0개 | `TEST_CASE` body 안에 `REQUIRE\|CHECK\|REQUIRE_FALSE\|CHECK_FALSE\|REQUIRE_THROWS\|SUCCEED` 0회 | **warn** |
| **R2**: SUCCEED-only smoke | body의 유일한 단언이 `SUCCEED(...)` | **warn** |
| **R3**: tag 누락 | `TEST_CASE("...", "")` 또는 두 번째 인자 없음 | **warn** |
| **R4**: disabled/skipped | `[.]`, `[!hide]`, `[!shouldfail]` tag | **info** |

**모두 warn 등급** (빌드 차단 X) — 학습 프로젝트의 마찰 최소화.

**CMake 통합**:
- `add_custom_target(check_test_smells)` — `cmake --build build_Darwin --target check_test_smells`로 명시 호출
- `add_test(NAME test_smells)` — ctest에 등록되어 `ctest` 출력에 함께 (실패 시 warn만, error_exit 아님)

### 6.2 `doc/test-quality-drill.md` + 컴포넌트별 분리 파일

**메인 문서** (`doc/test-quality-drill.md`):
- §1 개념 (1 paragraph)
- §2 언제 실행 (트리거 3종)
- §3 절차 (4 step)
- §4 컴포넌트별 표 *링크* (테이블)
- §5 결과 해석 가이드
- §6 Mutation testing 진입 트리거 (§6.3)

**컴포넌트별 파일** (`doc/test-quality-drill/<component>.md`, 4개):
- gl_state_capture.md
- diff.md
- symbolic_name.md
- snapshot_tostring.md

각 파일은 *살아있는 표*:
```
| 사보타지 | 적용 위치 | 예상 잡힘 케이스 | 실제 잡힘 (실측) | 날짜 |
|---|---|---|---|---|
| ... | gl_state_log.cpp:LL | test_xxx.cpp "..." | test_xxx.cpp "..." | 2026-MM-DD |
```

분리 이유: 단일 파일에 4개 컴포넌트 누적 시 행 수 ~50+ → diff PR 노이즈. 컴포넌트별 파일은 git blame 추적 + grep 친화.

### 6.3 Mutation Testing 보류 — 진입 트리거

**현재**: 도입 안 함 (mull 미설치, 컴포넌트 4개로 ROI ↓).

**진입 트리거 (4개)**:
1. 신규 테스트 인프라 컴포넌트 ≥ 10개
2. production 회귀가 *기존 테스트를 통과한 채로* 슬립
3. CI 시간 < 5분 + 머신 여유
4. [.claude/agents/render-quality-gate.md](../../.claude/agents/render-quality-gate.md) 의 PoC 시점 도달

---

## 7. 구현 순서 (의존성 최소 → 최대)

```
Step 1. gl_state_fields.h/.cpp + test_gl_state_fields.cpp        (GL ctx 불필요)
   ↓
Step 2. gl_state_log.h/.cpp                                       (fields.h 의존)
   ↓
Step 3. spdlog_capture.h/.cpp                                     (독립)
   ↓
Step 4. test_uniform_diagnostics.cpp 교체                          (spdlog_capture 의존)
   ↓
Step 5. gl_state_snapshot.h/.cpp + test_gl_state_snapshot.cpp     (fields.h 의존)
   ↓
Step 6. test_gl_state_capture.cpp + test_gl_state_log.cpp        (모두 의존)
   ↓
Step 7. scripts/check_test_smells.py + CMake 통합
   ↓
Step 8. doc/test-quality-drill.md + 컴포넌트별 파일
   ↓
Step 9. 사보타지 드릴 4회 수행 + 결과 표 채우기
```

각 Step은 `cmake --build build_Darwin && ctest --test-dir build_Darwin --output-on-failure` 통과가 완료 조건.

---

## 8. Open Questions (구현 시 결정)

- **Q1**: `SpdlogCapture`가 `std::shared_ptr<spdlog::logger>` 교체 — multi-test 병렬 실행 시 race? Catch2 v3 default는 단일 스레드 실행이지만 `--reporter junit -- --order rand` 등 옵션 사용 시 검토.
- **Q2**: `GLStateFields`의 `std::array<GLuint, 16>`이 ToString 시 *0 unit 필터링* — 16개 모두 0이면 한 줄도 출력 안 할지, "all texture units empty" 메시지 출력할지. 기본은 *침묵* (필드 변화 없음과 일관).
- **Q3**: `check_test_smells.py`의 정규식이 `TEST_CASE` 매크로 다중 라인 케이스(`TEST_CASE(\n  "...",\n  "[tag]")`)를 정확히 파싱하는지 — 첫 구현은 단일 라인만, 다중 라인은 Phase 2.

---

## 9. anti-future-work (의도적으로 안 하는 것)

| 안 하는 것 | 이유 |
|---|---|
| `Result<T,E>` / `std::expected` 도입 | [.claude/architecture.md](../../.claude/architecture.md) §3 컨벤션 일관성 |
| GL state record/replay | YAGNI, 본 PoC 범위 외 |
| 매 프레임 자동 덤프 | glGet* stall 비용 + log noise |
| Vulkan/Metal 추상화 | [.claude/CLAUDE.md](../../.claude/CLAUDE.md) §F 진화 후보 |
| Mutation testing 도구 통합 | §6.3 트리거 미충족 |
| 시각적 회귀(Phase B2-B4) | [testing-curriculum.md](testing-curriculum.md) 의 별도 트랙 |

---

## 10. 참조

- [.claude/architecture.md](../../.claude/architecture.md) — 모듈 디자인 / PUBLIC vs PRIVATE / leaf 타입 컨벤션
- [.claude/CLAUDE.md](../../.claude/CLAUDE.md) — Graphics Refactoring Guardrails
- [doc/testplan/testing-curriculum.md](testing-curriculum.md) — 2-track 테스트 전략, §부록 A.5 적대적 사고
- [.claude/agents/render-quality-gate.md](../../.claude/agents/render-quality-gate.md) — Mutation testing 미래 도입 시 참조
- 사용자 auto memory `project_ebo-type-incident` (Claude Code 외부 — `~/.claude/projects/.../memory/`) — 진단 시스템의 바이트 레벨 한계 교훈, 본 spec §4.4의 VAO=0 EBO 주석 결정의 근거
- A 논문 §8 (Mutation Score caveat) — 본 spec §6.3 트리거의 근거
- Catch2 v3 docs — `Catch::Matchers::ContainsSubstring`

---

## 11. 다음 단계

본 spec의 구현 계획은 **[2026-05-07-gl-state-and-test-quality-implementation.md](2026-05-07-gl-state-and-test-quality-implementation.md)** 참조.

사용자의 [auto memory phase-implementation-mode 정책](../../.claude/MEMORY.md) 에 따라 코드는 사용자 직접 작성. Claude는 명시 요청 시 review/문제 발견만.
