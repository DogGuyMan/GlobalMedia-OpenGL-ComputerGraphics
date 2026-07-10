# SP1 — Shader/Program 리소스 통합 Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** ECS 렌더링 아키텍처 재설계(SP1~SP4) 의 SP1 — 레거시 `Engine::Program::ShaderProgram` 제거 + `SJH::Shader`/`SJH::Program` 의 RAII 의미론을 `= delete` 로 명시 + SP2 흡수 seam 을 doxygen 메모로 표기.

**Architecture:** 구조 변경 없음. (1) 컴파일 타임 `static_assert` 로 비복사/비이동을 *먼저* 검증(테스트 빨강), (2) `= delete` 추가로 통과(초록), (3) 빌드 미참여 레거시 2파일 물리 삭제, (4) SP2 seam doxygen 메모. 새 함수/클래스/테스트 프레임워크 도입 없음.

**Tech Stack:** C++17, CMake (Ninja preset), gl3w, sb7. 테스트 = TU 내 `static_assert` 로 컴파일 타임 검증 (별도 테스트 러너 없음).

**Spec:** [doc/superpowers/specs/2026-05-20-sp1-shader-program-resource-consolidation-design.md](../specs/2026-05-20-sp1-shader-program-resource-consolidation-design.md)

---

## Task 1: `SJH::Shader` RAII 의미론 명시 — `= delete` + static_assert

**Files:**
- Modify: `src/shader/shader.cpp` (static_assert 추가)
- Modify: `src/shader/shader.h` (= delete 추가)

- [ ] **Step 1: 실패하는 컴파일 타임 테스트 작성**

`src/shader/shader.cpp` 의 `#include` 직후, namespace 진입 전에 `<type_traits>` include 와 static_assert 4 개를 추가.

```cpp
#include "shader/shader.h"
#include "diagnostics/gl_log.h"
#include <memory>
#include <type_traits>

// SP1 — RAII 의미론 컴파일 타임 검증.
// glDeleteShader 이중 호출 위험 차단 — 명시적 = delete 가 필요.
static_assert(!std::is_copy_constructible_v<SJH::Shader>,
              "SJH::Shader must be non-copy-constructible (RAII)");
static_assert(!std::is_copy_assignable_v<SJH::Shader>,
              "SJH::Shader must be non-copy-assignable (RAII)");
static_assert(!std::is_move_constructible_v<SJH::Shader>,
              "SJH::Shader must be non-move-constructible (factory + UPtr only)");
static_assert(!std::is_move_assignable_v<SJH::Shader>,
              "SJH::Shader must be non-move-assignable (factory + UPtr only)");
```

- [ ] **Step 2: 빌드해서 실패 확인**

Run:
```bash
cmake --build --preset ninja --target sjhopengl_shader 2>&1 | tail -30
```
Expected: FAIL — `static_assert failed: "SJH::Shader must be non-copy-constructible (RAII)"` (4 개 중 4 개 모두 실패. `Shader` 가 현재 암묵 복사·이동 가능하므로).

- [ ] **Step 3: `= delete` 명시 추가**

`src/shader/shader.h` 의 클래스 선언 안 `public:` 영역에 4 개 줄을 추가. 위치는 `~Shader();` 위.

```cpp
        /// @brief @c glDeleteShader 호출 (핸들이 0 이 아닐 때만).
        ~Shader();

        // SP1 — 자원 핸들 이중 해제 차단. 팩토리 + UPtr 패턴이므로 외부에서
        //       복사·이동할 경로가 애초에 없음.
        Shader(const Shader&)            = delete;
        Shader& operator=(const Shader&) = delete;
        Shader(Shader&&)                 = delete;
        Shader& operator=(Shader&&)      = delete;
```

`Shader` 의 *기존* 코드 (CreateFromFile / CreateFromSource / GetShaderAddr / private ctor) 는 *건드리지 않음*.

- [ ] **Step 4: 빌드해서 통과 확인**

Run:
```bash
cmake --build --preset ninja --target sjhopengl_shader 2>&1 | tail -10
```
Expected: PASS — `sjhopengl_shader.a` 생성 성공, static_assert 4 개 모두 통과.

- [ ] **Step 5: 커밋**

```bash
git add src/shader/shader.h src/shader/shader.cpp
git commit -m "$(cat <<'EOF'
[fix] : SJH::Shader 복사·이동을 = delete 로 명시 (SP1)

자원 핸들 이중 해제 차단. static_assert 4 종으로 컴파일 타임 검증.
팩토리 + UPtr 패턴이므로 외부에서 복사·이동할 경로가 애초에 없음 —
명시화는 미래 버그를 표면에서 차단.
)"
```

---

## Task 2: `SJH::Program` RAII 의미론 명시 — `= delete` + static_assert

**Files:**
- Modify: `src/program/program.cpp` (static_assert 추가)
- Modify: `src/program/program.h` (= delete 추가)

- [ ] **Step 1: 실패하는 컴파일 타임 테스트 작성**

`src/program/program.cpp` 의 `#include` 직후, namespace 진입 전에 `<type_traits>` include 와 static_assert 4 개를 추가.

```cpp
#include "program/program.h"
#include "diagnostics/gl_log.h"
#include "diagnostics/uniform_diagnostics.h"
#include "program/program_uniforms.h"
#include <type_traits>

// SP1 — RAII 의미론 컴파일 타임 검증.
// glDeleteProgram 이중 호출 위험 차단 — 명시적 = delete 가 필요.
static_assert(!std::is_copy_constructible_v<SJH::Program>,
              "SJH::Program must be non-copy-constructible (RAII)");
static_assert(!std::is_copy_assignable_v<SJH::Program>,
              "SJH::Program must be non-copy-assignable (RAII)");
static_assert(!std::is_move_constructible_v<SJH::Program>,
              "SJH::Program must be non-move-constructible (factory + UPtr only)");
static_assert(!std::is_move_assignable_v<SJH::Program>,
              "SJH::Program must be non-move-assignable (factory + UPtr only)");
```

- [ ] **Step 2: 빌드해서 실패 확인**

Run:
```bash
cmake --build --preset ninja --target sjhopengl_program 2>&1 | tail -30
```
Expected: FAIL — `static_assert failed: "SJH::Program must be non-copy-constructible (RAII)"` (4 개 모두 실패).

- [ ] **Step 3: `= delete` 명시 추가**

`src/program/program.h` 의 클래스 선언 안 `public:` 영역, `~Program();` 직후에 4 개 줄 추가.

```cpp
        /// @brief @c Uniforms::Forget 으로 외부 캐시 정리 후 @c glDeleteProgram 호출 (핸들이 0 이 아닐 때만).
        ~Program();

        // SP1 — 자원 핸들 이중 해제 차단. 팩토리 + UPtr 패턴이므로 외부에서
        //       복사·이동할 경로가 애초에 없음.
        Program(const Program&)            = delete;
        Program& operator=(const Program&) = delete;
        Program(Program&&)                 = delete;
        Program& operator=(Program&&)      = delete;

        /// @brief 내부 GL 프로그램 핸들 반환 — @c glUseProgram / @c Uniforms 자유 함수의 키.
        GLuint GetProgramAddr() const { return mProgramAddr; }
        void Use() const;
```

기존 멤버 함수(`Create` / `CreateWithVSFS` / `GetProgramAddr` / `Use` / private ctor / `TryLink`) 는 *건드리지 않음*.

- [ ] **Step 4: 빌드해서 통과 확인**

Run:
```bash
cmake --build --preset ninja --target sjhopengl_program 2>&1 | tail -10
```
Expected: PASS — `sjhopengl_program.a` 생성 성공.

- [ ] **Step 5: 커밋**

```bash
git add src/program/program.h src/program/program.cpp
git commit -m "$(cat <<'EOF'
[fix] : SJH::Program 복사·이동을 = delete 로 명시 (SP1)

자원 핸들 이중 해제 차단 — Program copy = *prog; 같은 코드가 컴파일되어
~Program 에서 glDeleteProgram 이 2 번 호출되던 위험을 표면에서 차단.
static_assert 4 종으로 컴파일 타임 검증.
)"
```

---

## Task 3: 레거시 `Engine::Program::ShaderProgram` 물리 삭제

**Files:**
- Delete: `<src>/engine/shader_program.cpp`
- Delete: `<src>/engine/shader_program.h`

- [ ] **Step 1: 사전 확인 — 빌드 그래프에 없음 검증**

Run:
```bash
grep -n "add_subdirectory(engine)" src/CMakeLists.txt || echo "engine 미등록 — 안전"
```
Expected: `engine 미등록 — 안전` 출력.

추가로 활성 빌드 타깃에서 참조가 없는지 확인:
```bash
git grep -nl 'engine/shader_program\.h' -- 'apps/' 2>/dev/null || echo "활성 타깃에서 참조 없음"
```
Expected: `활성 타깃에서 참조 없음` (단 `samples/` 의 비활성 챕터는 무시 — SP3 직전에 재평가).

- [ ] **Step 2: 파일 삭제**

Run:
```bash
git rm <src>/engine/shader_program.cpp <src>/engine/shader_program.h
```
Expected: `rm '<src>/engine/shader_program.cpp'` / `rm '<src>/engine/shader_program.h'` 출력.

- [ ] **Step 3: 빌드 통과 검증**

Run:
```bash
cmake --build --preset ninja 2>&1 | tail -10
```
Expected: 모든 활성 타깃 빌드 성공. `src/engine/` 가 빌드 그래프 밖이므로 빌드 영향 없음.

- [ ] **Step 4: 커밋**

```bash
git commit -m "$(cat <<'EOF'
[remove] : 레거시 Engine::Program::ShaderProgram 물리 제거 (SP1)

src/engine/ 는 src/CMakeLists.txt 에 add_subdirectory(engine) 가 없어
빌드 그래프 밖에 있던 dead code. SJH::Shader + SJH::Program 으로 완전히
대체됨. src/engine/ 의 다른 잔여(camera/transform/scene_graph/model_base)
정리는 SP3 ECS 컴포넌트 재설계와 함께.
)"
```

---

## Task 4: SP2 흡수 seam — doxygen 메모

**Files:**
- Modify: `src/program/program.h` (Use() / 클래스 doxygen 블록)
- Modify: `src/program/program_uniforms.h` (namespace doxygen 블록)

- [ ] **Step 1: `Program::Use()` 위 seam 메모 추가**

`src/program/program.h` 의 `void Use() const;` 선언 *바로 위* 에 doxygen 한 줄 메모를 끼운다.

```cpp
        /// @brief 내부 GL 프로그램 핸들 반환 — @c glUseProgram / @c Uniforms 자유 함수의 키.
        GLuint GetProgramAddr() const { return mProgramAddr; }

        /// @note (SP2 seam) 향후 RenderContext 가 @c glUseProgram 의 owner 가 됨 —
        ///       호출 경로는 @c RenderContext::UseProgram(*program) 으로 이전될 예정.
        void Use() const;
```

- [ ] **Step 2: `program_uniforms.h` namespace 블록 seam 메모 추가**

`src/program/program_uniforms.h` 의 `namespace Uniforms` 블록 *직전* (혹은 docstring 의 ### 책임 섹션 끝부분) 에 SP2 흡수 예고 한 줄을 추가. 가장 단순한 위치는 `namespace Uniforms` 위:

```cpp
namespace SJH
{
    class Program;   // forward — 본 헤더는 Program 의 정의에 의존하지 않음 (의도된 decoupling).
    class DirLight;
    class PointLight;
    class SpotLight;

    /// @note (SP2 흡수 예정) 본 namespace 의 자유 함수 family 와 캐시 자료구조
    ///       @c sCacheRegistry 는 향후 @c RenderContext 의 멤버로 이전될 예정 —
    ///       호출 형태가 @c renderCtx.SetMat4(name, m) 으로 바뀐다.
    namespace Uniforms
    {
```

- [ ] **Step 3: 빌드 검증 — 주석만이므로 행위 변화 없음**

Run:
```bash
cmake --build --preset ninja --target sjhopengl_program 2>&1 | tail -5
```
Expected: PASS.

- [ ] **Step 4: 커밋**

```bash
git add src/program/program.h src/program/program_uniforms.h
git commit -m "$(cat <<'EOF'
[docs] : SP2 흡수 seam 을 doxygen 메모로 표기 (SP1)

Program::Use() 와 SJH::Uniforms namespace 에 SP2 도입 시 RenderContext 로
이전될 표면임을 명시. 코드 동작 변화 없음 — 읽는 사람을 위한 표지판.
)"
```

---

## Task 5: 전체 통합 검증

**Files:** 없음 (검증만)

- [ ] **Step 1: 모든 활성 타깃 빌드**

Run:
```bash
cmake --build --preset ninja 2>&1 | tail -20
```
Expected: 모든 활성 챕터/연습 타깃 빌드 성공.

- [ ] **Step 2: `= delete` 가 실제 차단되는지 *행위* 검증 (선택)**

임시 `_throwaway.cpp` 를 만들어 컴파일 시도 → 에러 확인 → 즉시 삭제.

```bash
cat > /tmp/sp1_double_free_test.cpp <<'EOF'
#include "program/program.h"
void f() {
    SJH::ProgramUPtr p = SJH::Program::CreateWithVSFS("x.vs", "x.fs");
    SJH::Program copy = *p;                 // 컴파일 에러 기대
    SJH::Program moved = std::move(*p);     // 컴파일 에러 기대
}
EOF
# 실제 빌드 시도 (수동 실행 — 컴파일러 옵션은 컨텍스트에 맞춰 조정 필요)
# 또는 단순히 src/program/program.cpp 안에 임시로 위 코드를 넣고
# cmake 빌드 후 4 개 에러 확인 후 되돌리는 방식도 가능.
```
Expected: 4 개의 컴파일 에러 (copy ctor / copy assign / move ctor / move assign 모두 deleted).

확인 후 임시 파일·임시 코드 모두 제거.

- [ ] **Step 3: 레거시 잔재 부재 확인**

Run:
```bash
git ls-files 'src/engine/shader_program*'
```
Expected: 출력 없음 (= tracked 파일 없음).

```bash
git grep -nl 'engine/shader_program\.h' -- ':!extern' ':!include/GL' ':!samples'
```
Expected: 출력 없음.

- [ ] **Step 4: spec / plan 체크리스트 일치 확인**

- spec §4.1 (legacy 삭제) → Task 3 ✓
- spec §4.2 (Program = delete) → Task 2 ✓
- spec §4.3 (Shader = delete) → Task 1 ✓
- spec §4.4 (seam 메모) → Task 4 ✓
- spec §5 (검증) → Task 5 ✓

---

## Out of Scope (재확인)

| 항목 | 위치 |
|------|------|
| `program_uniforms` 의 RenderContext 흡수 | SP2 |
| `glUseProgram` 의 RenderContext 이전 | SP2 |
| `RenderTarget` 추상 | SP2 |
| ECS 컴포넌트 + RenderSystem | SP3 |
| `Default`/`Texture` 다형성 → Material variants | SP3 |
| `src/engine/` 의 camera/transform/scene_graph/model_base 정리 | SP3 |
| Mesh/Model 리소스 모듈 (VAO/VBO/EBO RAII) | 분해 갱신 필요 (SP1.5 후보) |
| FrameBuffer/RenderBuffer/멀티패스/포스트프로세싱 | SP4 (스펙만) |
