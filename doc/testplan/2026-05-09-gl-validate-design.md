# GLValidate — Mesh/Shader 정합성 진단 모듈 (Design + Test Plan)

> **Date**: 2026-05-09
> **소스 프롬프트**: [doc/inst.md](../inst.md)
> **목적**: 라이팅·메시·셰이더 마이그레이션에서 반복 발생한 6 카테고리 버그를 *호출 시점에 명시적 진단*으로 catch.

---

## 0. 위치 — 본 plan의 상위 인덱스

본 모듈은 [bug-coverage-audit.md](bug-coverage-audit.md) 트랙 C의 **`gldebug-api-extension-design.md` 후속 구현**이지만, 새 진단 *namespace*(`Diagnostics::GLValidate`)로 분리:

- 기존 `Diagnostics::GLDebug` (per-call low-level 에러 검사) — 그대로 유지
- 기존 `Diagnostics::GLStateLog` (state 한 줄 덤프) — 그대로 유지
- 신규 `Diagnostics::GLValidate` (Mesh ↔ Shader contract 진단) — 본 spec

### 잡는 카테고리 (bug-coverage-audit.md 매핑)

| Cat | 본 모듈 | audit 카테고리 |
|---|---|---|
| A | CheckIndices | C5 (정점 인덱스 오타), K1/K2/K3 (기하 오류) |
| B | CheckAttribLayout | C1-C5 (vertex attribute layout) |
| C | CheckUniformCoverage | D3 (uniform 누락), D4/D5 (값 누락 의심) |
| D | CheckSamplerBindings | E (texture 관련 미커버 영역 — gldebug-api-extension의 일부) |
| E | CaptureGLError | E (GL 상태 일반), 비-카테고리 (모든 silent GL fail) |
| F | DumpShaderInfoLogs | A1-A5 (셰이더 무음 실패) — driver warning까지 |

→ 본 모듈이 채워지면 audit 트랙 C의 `gldebug-api-extension-design.md` 의 일부 산출물 *upstream 회수*.

---

## 1. API 표면 (헤더 시그니처)

```cpp
namespace SJH::Diagnostics::GLValidate {
    size_t CheckIndices(const std::vector<uint32_t>& indices,
                        size_t vertexCount, const char* tag);
    size_t CheckAttribLayout(GLuint program, const char* tag);
    size_t CheckUniformCoverage(GLuint program, const char* tag);
    size_t CheckSamplerBindings(GLuint program, const char* tag);
    bool   CaptureGLError(const char* tag);
    void   DumpShaderInfoLogs(GLuint program, const char* tag);

    size_t RunFullSweep(GLuint program,
                        const std::vector<uint32_t>& indices,
                        size_t vertexCount,
                        const char* tag);
}
```

**반환 의미**:
- `size_t` 0 = 위반 없음, > 0 = 위반 개수
- `CaptureGLError` bool = true (clean) / false (에러 있었음, 보고됨)

---

## 2. 카테고리별 알고리즘

### Cat A — CheckIndices
- 입력: `std::vector<uint32_t> indices`, `vertexCount`
- 검사:
  1. 모든 `idx < vertexCount`
  2. triangles = `indices.size() / 3`, 각 trio가 서로 다른 인덱스 (`!= && !=`)
  3. (선택) 같은 triangle (sorted) 가 중복 등장 시 warning

### Cat B — CheckAttribLayout
- 입력: `program`
- 절차:
  1. `glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &count)`
  2. 각 active attribute에 대해 `glGetActiveAttrib` → 이름 + `GL_FLOAT_VEC3` 같은 type
  3. `glGetAttribLocation(program, name)` → loc
  4. `glGetVertexAttribiv(loc, GL_VERTEX_ATTRIB_ARRAY_ENABLED)` → 비활성이면 위반
  5. `glGetVertexAttribiv(loc, GL_VERTEX_ATTRIB_ARRAY_SIZE)` → VS type 컴포넌트 수와 다르면 위반
- 추가: VAO에 enable됐는데 VS가 안 쓰는 location (낭비) → warning

### Cat C — CheckUniformCoverage
- 입력: `program`
- 절차:
  1. `glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count)`
  2. 각 active uniform 이름 추출
  3. *현재* 보수적 진단 — 모든 active uniform 의 *현재값* 을 query해 default-zero인지 확인 (mat4 = all 0, vec3 = (0,0,0), float = 0.0, sampler = unit 0 이면 *의심*)
  4. sampler 타입은 unit 0이 *합법적*이라 별도 분기

### Cat D — CheckSamplerBindings
- 입력: `program`
- 절차:
  1. active uniform 중 type ∈ {`GL_SAMPLER_2D`, `GL_SAMPLER_CUBE`} 추림
  2. 각 sampler의 *현재 값* (`glGetUniformiv` → texture unit N) 조회
  3. `glActiveTexture(GL_TEXTURE0 + N)` 후 `glGetIntegerv(GL_TEXTURE_BINDING_2D, &id)` → id == 0 이면 위반 (unit 바인딩 없음)
  4. (선택) 같은 texture id가 여러 unit에 동시 bound → info 레벨

### Cat E — CaptureGLError
- `glGetError()` 호출 → 0이 아니면 enum → 이름 변환 후 spdlog::warn
- rate limit: `thread_local std::unordered_set<GLenum> reportedThisFrame`. 같은 에러 코드는 프레임당 1회만 (호출자가 `ResetFrame()`을 매 frame 시작 시 호출 — 또는 그냥 같은 코드+tag 조합 1회만)

### Cat F — DumpShaderInfoLogs
- `glAttachedShaders(program)` → 각 shader → `glGetShaderInfoLog`
- `glGetProgramInfoLog(program)`
- 비어있지 않으면 spdlog::info (경고가 아닐 수 있으므로 info 레벨; 단, 'error'/'warning' 키워드 포함 시 warn 으로 upgrade)

---

## 3. Test Plan — Catch2 6 카테고리 × 2-4 케이스

doc/inst.md §7 Criterion 3 의 *일부러 깨뜨리는 6 케이스* 를 *Catch2 행동 단언*으로 코드화. 수동 sabotage drill을 자동 회귀로 승격.

| Cat | 테스트 케이스 | GL ctx |
|---|---|---|
| A | normal 인덱스 → 0 위반 | ❌ |
| A | 인덱스 OOB → 위반 보고 | ❌ |
| A | degenerate triangle (`0,0,0`) → 위반 보고 | ❌ |
| B | inline VS/FS attribute match → 0 위반 | ✅ |
| B | location 2가 vec3 vs vec2 불일치 → 위반 | ✅ |
| C | declared but unset uniform → 위반 | ✅ |
| D | sampler set N, unit N bound → 0 위반 | ✅ |
| D | sampler set N, unit N empty → 위반 | ✅ |
| E | clean state → 0 보고 | ✅ |
| E | `glEnable(0xDEAD)` 후 → invalid_enum 보고 | ✅ |
| E | rate limit — 같은 코드 N회 → 1회 보고 | ✅ |
| F | clean program → 빈 출력 | ✅ |
| FullSweep | clean state → 0 위반 + "all clean" 메시지 | ✅ |

**테스트 인프라**: 기존 `gl_test_fixture` + `spdlog_capture` 재사용 — 로그 출력 substring 단언.

---

## 4. 통합 지점

| 위치 | 호출 | 빈도 |
|---|---|---|
| `Mesh::Init()` 끝 | `CheckIndices(indices, vertices.size(), "mesh_tag")` | 메시 생성 1회 |
| `Context::Init()` 끝 (`GLStateLog::Dump` 직후) | `RunFullSweep(prog, indices, vertCount, "lighting")` | 1회 |
| `Context::Render()` (옵션, deferred) | `CaptureGLError("Render")` | 매 프레임 |

본 spec에서는 **Mesh::Init / Context::Init 통합만**. Render() 통합은 *rate-limited frame trigger* 가 필요해 별도 PR (deferred).

---

## 5. Acceptance Criteria (inst.md §7 매핑)

| # | 기준 | 검증 |
|---|---|---|
| 1 | 빌드 클린 | `cmake --build build_Darwin` 0 error 0 warning |
| 2 | RunFullSweep clean state 0 위반 | ctest "FullSweep clean" PASS |
| 3 | 6 카테고리 sabotage 모두 catch | Catch2 13 케이스 PASS |
| 4 | CaptureGLError rate limit | "rate limit — 같은 코드 1회" 케이스 PASS |
| 5 | macOS arm64 + Windows x64 | 플랫폼 분기 코드 없이 standard GL |

---

## 6. anti-future-work

- ❌ Mesh::Init이 RunFullSweep 호출 — Mesh가 program을 모름. CheckIndices만.
- ❌ Render 통합 — rate-limited frame trigger 별도 spec
- ❌ FLIP / Mesa llvmpipe — Phase 2
- ❌ GLDebug Layer 1 (KHR_debug callback) — `gldebug-api-extension-design.md` 영역

---

## 7. 참조

- [doc/inst.md](../inst.md) — 원본 프롬프트
- [bug-coverage-audit.md](bug-coverage-audit.md) §3 트랙 C — `gldebug-api-extension-design.md` (본 spec의 상위 카테고리)
- [resources/shader/lighting.fs](../../resources/shader/lighting.fs) — 통합 대상 셰이더
- [src/object/mesh.cpp](../../src/object/mesh.cpp) — CheckIndices 호출 위치
