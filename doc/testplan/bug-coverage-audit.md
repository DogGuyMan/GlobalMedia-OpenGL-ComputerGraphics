# Bug Coverage Audit — BugReport + STUDY_NOTE 매핑

> **작성일**: 2026-05-09
> **소스**: [BugReport.md](BugReport.md), [STUDY_NOTE.md](STUDY_NOTE.md) (Chapter 6/7 + Exercise 6 + Exercise 6 확장)
> **목적**: 사용자가 실제로 겪은 ~50개 버그 패턴 → 현재 테스트 인프라 커버리지 매핑 → 갭 분류 (자동 확장 / 후속 Task / sibling spec).

---

## 1. 버그 분류 (11 카테고리)

> 약어 표기는 본 문서 안에서만 유효. 향후 사보타지 드릴 표 작성 시 직접 인용 가능.

### A. Shader 컴파일/링크 무음 실패
| # | 패턴 | 출처 |
|---|---|---|
| **A1** | GLSL `#version 430` → macOS GL 4.1에서 거부, 무음 실패 | BugReport §3 |
| **A2** | `check_errors=false` 디폴트 (`_DEBUG`는 MSVC 전용 매크로) | BugReport §4 |
| **A3** | GLSL bool에 비트 OR (`|`) → 컴파일 실패 (체인적 무음 실패 유발) | STUDY_NOTE Ex6 §1-5 |
| **A4** | FS interface block에 `out` 키워드 (in이 정답) | STUDY_NOTE Ex6 §1-1 |
| **A5** | `glLinkProgram` 후 `GL_LINK_STATUS` 검사 누락 | STUDY_NOTE Ch7 §4-3 |

### B. 버퍼 사이즈/타입 불일치
| # | 패턴 | 출처 |
|---|---|---|
| **B1** | `sizeof(vec.size())` = sizeof(size_t) (= 8) | STUDY_NOTE Ch6 §1-1 |
| **B2** | EBO에 `sizeof(GLfloat)` 사용 — 둘 다 4바이트라 우연 통과 | STUDY_NOTE Ch7 §1-4 |
| **B3** | EBO에 vertex 데이터 자체 업로드 (target/data 매칭 실패) | STUDY_NOTE Ex6 R-2 |

### C. Vertex Attribute Layout 오류
| # | 패턴 | 출처 |
|---|---|---|
| **C1** | offset 누적 계산 오류 (`sizeof(float)` vs `4*sizeof(float)`) | STUDY_NOTE Ch6 §2-2 |
| **C2** | UV(vec2)에 size=4로 읽음 → 다음 attribute 영역까지 침범 | STUDY_NOTE Ex6 R-3 |
| **C3** | Interleaved 데이터 — 컴포넌트 단위 교차로 만들어 stride 깨짐 | STUDY_NOTE Ch6 §2-1 |
| **C4** | `glEnableVertexAttribArray` 누락 | (관용 — STUDY_NOTE 전반) |
| **C5** | 정점 데이터 인덱스 [z][y][x] 매핑 오타 (큐브 좌표 손상) | STUDY_NOTE Ch6 §2-3 |

### D. Uniform 타입 매칭 / 값 누락
| # | 패턴 | 출처 |
|---|---|---|
| **D1** | `glUniformMatrix4fv`로 vec2 업로드 → `GL_INVALID_OPERATION` 무음, 값 0 유지 | STUDY_NOTE Ex6 §1-6 ⭐ |
| **D2** | sampler2D에 `glUniform1f`로 소수점 — 무음 거절 | 동상 |
| **D3** | Uniform location -1 (warn-once already exists) | UniformDiagnostics |
| **D4** | `transformMat`에 setter 호출 누락 → zero matrix → 무한 축소 | 사용자 직접 보고 |
| **D5** | `baseColor` setter 누락 → 까만색 | 동상 |

### E. GL 상태 (depth/blend/clear/bind 등)
| # | 패턴 | 출처 |
|---|---|---|
| **E1** | `glEnable(GL_DEPTH_TEST)` 누락 | STUDY_NOTE Ch7 §2-3 |
| **E2** | depth buffer clear 누락 → 이전 프레임 depth로 occlusion 폭주 | STUDY_NOTE Ch7 §2-2 |
| **E3** | `glClearBufferfv` 후 `glClear(COLOR_BUFFER_BIT)`로 즉시 덮어쓰기 | BugReport §5 |
| **E4** | 변수명 `black`이 실제 값은 노랑 (시맨틱 오해) | BugReport §5 |
| **E5** | `glBindTexture(GL_TEXTURE_BINDING_2D, ...)` — query enum을 bind에 사용 | STUDY_NOTE Ch7 §1-3 |
| **E6** | 잘못된 VAO 바인딩 (default 0 vs 실제 모델 핸들) | STUDY_NOTE Ch6 §1-3 |

### F. C++ 객체 lifecycle / 메모리
| # | 패턴 | 출처 |
|---|---|---|
| **F1** | 베이스 ctor에서 가상 함수 호출 → pure virtual abort | STUDY_NOTE Ch7 §1-1 |
| **F2** | 멤버 미초기화 → 가비지 행렬 | STUDY_NOTE Ch7 §1-2 |
| **F3** | `mScale = (0,0,0)` 디폴트 → 모델이 점으로 찌부러짐 (REPEATED) | STUDY_NOTE Ex6 R-1 ⭐ |
| **F4** | GL 핸들을 값 멤버로 보유 + 복사 → dangling | STUDY_NOTE Ex6 §1-3 |
| **F5** | GL 객체를 application 값 멤버 → ctor 시점에 GL 컨텍스트 미존재 → SEGV | STUDY_NOTE Ex6 §1-2 |
| **F6** | 빈/부족한 vector `operator[]` UB | STUDY_NOTE Ex6 §1-4, R-4 |

### G. Camera / Matrix 수학
| # | 패턴 | 출처 |
|---|---|---|
| **G1** | View 이중 변환 (`GetModelMatrix() * lookat()`) | STUDY_NOTE Ch7 §3-1 |
| **G2** | Projection 함수가 view × perspective 반환 | STUDY_NOTE Ch7 §3-2 |
| **G3** | aspect ratio = `height / height` | STUDY_NOTE Ch7 §3-3 |
| **G4** | 모델 합성 순서 S × R × T (T × R × S가 정답) | STUDY_NOTE Ch7 §3-4 |
| **G5** | LookAt 1회 호출 (카메라 이동 시 시선 stale) | STUDY_NOTE Ch6 §4-2 |
| **G6** | degree 값을 좌표로 사용 | STUDY_NOTE Ch6 §4-4 |
| **G7** | vmath의 `vec * mat`은 row convention (= `M^T · v`) | STUDY_NOTE Ex6 §3-3 ⭐ |
| **G8** | 카메라 forward · world_up 평행 → lookAt degenerate | 사용자 직접 보고 |

### H. Draw call 시퀀스 / 상태 의존
| # | 패턴 | 출처 |
|---|---|---|
| **H1** | draw call 자체 누락 | STUDY_NOTE Ch7 §2-1 |
| **H2** | 상태 교체(텍스처 등) 루프 후에 단 1회 draw → 마지막 상태로만 그림 | STUDY_NOTE Ex6 §2-1 |
| **H3** | for-each 안에서 `vec.back()` 호출 (복붙 잔재) | STUDY_NOTE Ch7 §2-4 |
| **H4** | 범용 `Draw()`에 메쉬 상수 (6, 36, f*6) 하드코딩 → 부분만 그림 | STUDY_NOTE Ex6 §2-7 |

### I. 애니메이션 / 로직
| # | 패턴 | 출처 |
|---|---|---|
| **I1** | `Translate` (누적) vs `SetPosition` (절대) 혼동 | STUDY_NOTE Ch6 §4-1 |
| **I2** | 새 함수 작성 후 `render()`에서 호출 누락 | STUDY_NOTE Ch6 §4-3 |

### J. 셰이더 내용 (GLSL)
| # | 패턴 | 출처 |
|---|---|---|
| **J1** | `mat4`를 `vec4`에 대입 → 첫 열만 사용 | STUDY_NOTE Ch6 §1-2 |
| **J2** | uniform * position 곱셈 누락 | 동상 |

### K. 기하 / 토폴로지
| # | 패턴 | 출처 |
|---|---|---|
| **K1** | 큐브 정점 좌표 오타 ([1][1][0] = (0,0,1) 등) | STUDY_NOTE Ch6 §2-3 |
| **K2** | 면 구성 시 다른 평면 정점 혼합 | STUDY_NOTE Ch6 §2-4 |
| **K3** | 사각형 4번째 정점 D = C 중복 → 삼각형 둘 겹침 | STUDY_NOTE Ch7 §4-1 |
| **K4** | 면 winding 일관성 결여 → 텍스처 mirror | STUDY_NOTE Ex6 §2-2 |
| **K5** | Per-quad 색 (per-vertex 보간 포기) | STUDY_NOTE Ex6 §2-6 |
| **K6** | 정규화 분모 off-by-one (`numCols` vs `uRes`) | STUDY_NOTE Ex6 §2-8 |

### L. 메뉘얼·구조적 실수
| # | 패턴 | 출처 |
|---|---|---|
| **L1** | 함수 파라미터 이름 누락 (`const char*` 만) | STUDY_NOTE Ex6 §3-1 |
| **L2** | 매직 문자열/숫자 박힘 → DRY 위반 | STUDY_NOTE Ex6 §3-2 |
| **L3** | 파라메트릭 누적 변수가 잘못된 루프 스코프 → 아르키메데스 나선 | STUDY_NOTE Ex6 §2-5 |
| **L4** | 텍스처 unit 시분할 오해 (글로벌 슬롯이라는 사실 모름) | STUDY_NOTE Ex6 §3-1, §3-2 |
| **L5** | 암묵적 순서 계약 (3개+ 배열이 같은 순서로 진행) | STUDY_NOTE Ex6 §3-3 |

⭐ = 가장 통증 큰 / 반복 발생한 패턴

---

## 2. 현재 테스트 인프라 매핑

### 2.1 이미 존재하는 도구

| 도구 | 위치 | 잡는 카테고리 |
|---|---|---|
| `GLObjectLog::CheckShaderCompile/Link/Validate` | [src/diagnostics/gl_log.h](../../src/diagnostics/gl_log.h) | A1, A2, A3, A4, A5 (call site에서 호출 시) |
| `GLObjectLog::CheckExpectedUniforms/Attributes` | 동상 | D3, C4 (호출 시) |
| `GLDebug::CheckGL*` (per-call) | 동상 | E5 부분, B 부분 |
| `UniformDiagnostics::NotifyMissing/TypeMismatch` | [src/diagnostics/uniform_diagnostics.h](../../src/diagnostics/uniform_diagnostics.h) | D1, D2, D3 (setter가 호출하는 한) |

### 2.2 진행 중인 plan (Task 1-9, [implementation plan](2026-05-07-gl-state-and-test-quality-implementation.md))

| Task 결과물 | 잡는 카테고리 | 갭 |
|---|---|---|
| GLStateFields (17필드) | E1, E2, E3, E6 부분 | C, D, K 거의 못 잡음 |
| CaptureGLState | 동상 | 동상 |
| GLStateLog::Dump | 진단 출력만 | 직접 잡지 않음 |
| GLStateSnapshot::Diff | 변화 가시화 | 동상 |

### 2.3 갭 매트릭스 (커버 / 부분 / 미커버)

| 카테고리 | 현재 상태 | 비고 |
|---|---|---|
| A (셰이더 무음 실패) | ✅ 도구 존재, ⚠️ 호출 강제는 안 됨 | A1 (GLSL #version 정적 검사) 추가 가능 |
| B (버퍼 사이즈) | ❌ | 버퍼 사이즈 필드를 GLStateFields에 추가 가능 |
| C (vertex attribute) | ❌ | **GLStateFields에 attribute_layouts 추가 — 본 audit의 자동 확장 대상** |
| D (uniform 타입) | ⚠️ 부분 (UniformDiagnostics가 setter 경로 한정) | setter 호출 자체가 누락된 경우 (D4, D5)는 못 잡음 |
| E (GL 상태) | ✅ 대부분 커버 (depth/blend/cull/viewport 등) <br> ⚠️ 단, **texture 관련 GL 호출 진단 부재** (E5 외) | `glBindTexture`/`glTexImage2D`/`glActiveTexture` Check 함수 자체가 API 에 없음 → **트랙 C `gldebug-api-extension-design.md`** 후보 |
| F (C++ lifecycle) | ❌ | C++ static analyzer 영역 (clang-tidy 체크) |
| G (Camera/Matrix) | ❌ | 단위 테스트 (sibling spec) |
| H (Draw 시퀀스) | ⚠️ 부분 (state opacity로 일부 추적) | call sequence trace는 별도. `glDrawElements/glDrawArrays` 호출별 진단 → 트랙 C `gldebug-api-extension-design.md` |
| I (애니메이션) | ❌ | 통합 테스트 영역 |
| J (GLSL 내용) | ⚠️ A 도구가 컴파일 단에서 잡지만 시맨틱은 못 잡음 | shader 단위 테스트 (sibling spec) |
| K (기하) | ❌ | 시각적 회귀(testing-curriculum Phase B2-B4) |
| L (구조) | ❌ | 코드 리뷰 영역 |

---

## 3. 갭 분류 (3 트랙)

### 트랙 A — Task 1-2 자동 확장 (이번 세션, 즉시)

본 audit 결과 **GLStateFields에 vertex attribute layout 필드 추가**가 가장 효율적. 이유:
- 사용자가 가장 자주 겪은 카테고리 C (5개 패턴) 전부 커버
- `glGetVertexAttribiv` 호출은 부수효과 0 (rebind 불필요)
- ToString/Diff에 자연스럽게 통합 가능

추가될 필드:
```cpp
struct VertexAttribInfo {
    bool   enabled;        // GL_VERTEX_ATTRIB_ARRAY_ENABLED
    GLint  size;           // 1/2/3/4 (vec1/2/3/4)
    GLenum type;           // GL_FLOAT, GL_INT, ...
    bool   normalized;
    GLsizei stride;
    GLuint  buffer_binding;
};
struct GLStateFields {
    // ... 기존 17 필드 ...
    std::array<VertexAttribInfo, 16> attribute_layouts{};  // GL 3.3 spec 상한 16
};
```

신규 사보타지 드릴 후보:
- `glVertexAttribPointer(0, 3, ...)` → `(0, 2, ...)` (vec3 → vec2): "VAO 바인딩 후 attribute size 반영" 케이스가 잡아야 함
- `glEnableVertexAttribArray(0)` 누락: "enabled=true" 단언이 잡아야 함
- stride 0 (tightly packed) → 24 (잘못된 값): stride 단언

### 트랙 B — Task 3-9 plan 보강 (현재 plan 안에서 처리)

| 추가 항목 | 위치 |
|---|---|
| FieldsToString이 attribute_layouts 출력 | Task 5 (plan에 명시 필요) |
| Diff가 attribute 변화 비교 | Task 6 (plan에 명시 필요) |
| Smell linter R5: `glUniform*` 함수와 setter 시그니처 매칭 (D1) | Task 7 보강 후보 (deferred) |
| 사보타지 드릴 표에 attribute 사보타지 3개 추가 | Task 8 / Task 9 |

### 트랙 C — 별도 sibling spec (본 plan과 독립)

| Spec 후보 파일명 | 잡는 카테고리 | 우선순위 |
|---|---|---|
| `2026-05-?-shader-source-static-checks-design.md` | A1 (GLSL #version 정적 검사), J (셰이더 내용) | 中 |
| `2026-05-?-camera-math-unit-tests-design.md` | G (Camera/Matrix) — 사용자가 src/camera/ 활성 중이므로 적기 적절 | 高 |
| `2026-05-?-cpp-lifecycle-clang-tidy-design.md` | F (C++ lifecycle) | 中 |
| `2026-05-?-uniform-setter-instrumentation-design.md` | D4/D5 (setter 호출 자체 검증) | 高 |
| `2026-05-?-visual-regression-track-b-design.md` | K (기하), J 시맨틱 | 高 (testing-curriculum Phase B2-B4와 연동) |
| `2026-05-?-gldebug-api-extension-design.md` | E (texture 관련 미커버 영역), H (Draw 시퀀스), 그리고 Layer 1/2의 cross-cutting 자동 catch | **中** — 2026-05-09 audit 후속 발견 |

#### `gldebug-api-extension-design.md` 상세 (Audit 후속 — 2026-05-09 추가)

**동기**: [implementation plan §"진단 적용" audit](2026-05-07-gl-state-and-test-quality-implementation.md) 진행 중 발견 — `texture.cpp`의 `glGenTextures` / `glTexImage2D` / `glActiveTexture`, `context.cpp`의 `glDrawElements` / `glClear` / `glViewport` 등이 *적용할 진단 자체가 API에 없음*. 단순 missed application 아님 — *API extension*이 필요.

**예상 산출물**:
1. **`GLDebug` Check 함수 추가** (~10개):
   - `CheckGLGenTextures(GLuint tex)`
   - `CheckGLBindTexture(GLuint tex)` (GL_TEXTURE_BINDING_2D 같은 query enum 사용 케이스 catch)
   - `CheckGLTexImage2D(GLint w, GLint h)` (GL_OUT_OF_MEMORY 등)
   - `CheckGLActiveTexture(GLenum unit)` (GL_INVALID_ENUM)
   - `CheckGLDrawElements(GLenum mode, GLsizei count, GLenum type)` (program 미바인딩, 잘못된 type 등)
   - `CheckGLDrawArrays(GLenum mode, GLint first, GLsizei count)`
   - `CheckGLClear(GLbitfield mask)` (GL_INVALID_VALUE)
   - `CheckGLViewport(GLint w, GLint h)` (음수)
   - `CheckGLEnable/Disable(GLenum cap)` (GL_INVALID_ENUM)

2. **Layer 1: `GLDebug::Init()`** — KHR_debug callback 등록 (architecture.md §6 미구현 영역).
   - macOS GL 3.3 core profile: KHR_debug 미지원 → `std::call_once warn` 후 no-op (`GLStateLog::EnableAutoOnError`와 동일 패턴).
   - Windows / Linux / llvmpipe: 정상 등록, GL_DEBUG_SEVERITY_HIGH 시 자동 spdlog::error.

3. **Layer 2: `SJH_GL_CHECK(x)` 매크로** — 의심스러운 GL 호출 wrapping.
   - `#ifdef NDEBUG`에서 no-op → release build 비용 0.
   - debug build에서 `glGetError` 폴링 + 큐 drain.

**잡는 카테고리** (현재 plan 외 추가 cover):
- E: texture 관련 미커버 (E5 외 추가)
- H: Draw 시퀀스 (잘못된 program/VAO + draw 조합 catch)
- 그리고 **Layer 1/2의 cross-cutting 자동 catch** = *어떤 카테고리든 GL 에러로 표면화되면 자동 감지*

**제외 (의도)**:
- `CheckGLDeleteXxx` — cleanup 경로는 에러 잡아도 의미 적음 (이미 destruction 중)
- `CheckGLFinish` — 동기화는 진단 영역 아님

**도입 시점 트리거**:
- texture.cpp / context.cpp의 *현재 진단 0인 GL 호출들*이 *실제로 회귀를 만든 적이 있을 때* (사보타지 드릴 결과 또는 BugReport 후속 사건)
- 또는 [render-quality-gate.md](../../.claude/agents/render-quality-gate.md) PoC 시점에 *호출별 강제 진단*이 PR 게이트의 일부가 될 때

**관계**:
- 본 spec이 채워지면 `texture.cpp`의 모든 GL 호출에 진단 적용 가능 → audit C 트랙 (현재 sibling spec) → A 트랙 (현재 plan 흡수)으로 *upstream 회수* 가능.
- `bug-coverage-audit.md` §4.4의 "도구로 해결 불가"의 일부가 *해결 가능*으로 이전.


---

## 4. 결론 + 액션

### 4.1 즉시 진행 (트랙 A)

1. ✅ 본 audit 문서 작성 (이 파일)
2. → **GLStateFields에 attribute_layouts 추가** + CaptureGLState 확장
3. → test_gl_state_capture.cpp에 attribute 케이스 추가
4. → plan 업데이트 (Task 5/6에 attribute 처리 명시 + Task 9 사보타지 표 확장)

### 4.2 Task 3-9 진행하면서 (트랙 B)

- Task 5 작성 시 FieldsToString이 attribute 출력 (자동으로 plan 갱신본 따름)
- Task 6 작성 시 Diff가 attribute 비교
- Task 9 사보타지 드릴 시 vertex attribute 사보타지 3개 추가 실행

### 4.3 Task 9 완료 후 (트랙 C)

본 plan을 끝까지 완료 → 사보타지 드릴 결과로 *현재 도구의 진짜 회귀 감지력 측정* → 그 데이터 기반으로 트랙 C의 5개 sibling spec 우선순위 결정.

### 4.4 본 audit이 안 잡는 패턴 (도구로 해결 불가)

| 패턴 | 이유 |
|---|---|
| **F1**: 베이스 ctor에서 가상 함수 호출 | C++ 컴파일러 경고 (`-Wnon-virtual-dtor` 등)와 코드 리뷰 |
| **L1**: 함수 파라미터 이름 누락 | 컴파일 통과하므로 정적 검사 한계 (clang-tidy `readability-named-parameter`) |
| **사용자 자신의 실수 인식** | 본 audit 결과 = 절차 정착 도구. 실수 자체는 사람이 만든다. |

---

## 5. 참조

- [BugReport.md](BugReport.md) — exercise7 검정 화면 사건의 사후 분석
- [STUDY_NOTE.md](STUDY_NOTE.md) — Chapter 6/7 + Exercise 6 + Exercise 6 확장 학습 노트 (2197 lines)
- [2026-05-07-gl-state-and-test-quality-design.md](2026-05-07-gl-state-and-test-quality-design.md) — 진행 중인 spec (트랙 A 흡수)
- [2026-05-07-gl-state-and-test-quality-implementation.md](2026-05-07-gl-state-and-test-quality-implementation.md) — 진행 중인 plan (Task 1-2 완료 시점)
