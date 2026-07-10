# SP2 — RenderContext (저수준 GL 게이트웨이) Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** ECS 렌더링 아키텍처 재설계의 SP2 — `SJH::RenderContext` 싱글톤 + `SJH::RenderTarget` 추상 신설, `SJH::Program` 에 uniform 캐시 멤버 흡수 (Pattern Y, A6), `SJH::Uniforms` 자유 함수 family 의 *시그니처는 보존* 하되 내부 구현을 `prog.GetLocation` 으로 전환.

**Architecture:** (1) 신규 `src/render/` 모듈에 RenderTarget 추상 + RenderContext 싱글톤. (2) Program 에 `mUniformCache` 멤버 + `GetLocation`/`GetType` *pure const query* 추가. (3) Uniforms 내부를 prog.GetLocation 경유로 전환, 외부 `sCacheRegistry` + 자유 함수 `BuildCache`/`Forget` 제거. (4) Program::Use() 폐기 + 활성 챕터 `rc.UseProgram(*prog)` 로 마이그레이션. ddd Rules — Separation of Concerns / Explicit Side Effects / POLA 준수.

**Tech Stack:** C++17, CMake (Ninja preset), gl3w, GL 3.3 (DSA 미채택). 테스트 = TU 내 `static_assert` (별도 테스트 러너 없음).

**Spec:** [doc/superpowers/specs/2026-05-21-sp2-render-context-design.md](../specs/2026-05-21-sp2-render-context-design.md)

---

## Summary

6 Task. 각 Task = 1 commit. 의존 순서대로 진행 (T1 → T6).

| # | Task | 변경 파일 | 핵심 결과 | 검증 |
|---|---|---|---|---|
| **T1** | `src/render/` 모듈 + RenderTarget 추상 + DefaultRenderTarget | `src/render/render_target.{h,cpp}`, `src/render/CMakeLists.txt`, `src/CMakeLists.txt` | `SJH::RenderTarget` 인터페이스 + `DefaultRenderTarget` (FBO 0) 신규. SP4 의 `FrameBufferTarget` 자리. | `cmake --build` `sjhopengl_render` PASS |
| **T2** | RenderContext 싱글톤 + GL 게이트웨이 13 메서드 | `src/render/render_context.{h,cpp}`, `src/render/CMakeLists.txt` | `SJH::RenderContext::Get()` 싱글톤. UseProgram/BindVAO/BindTexture/BindTarget/Clear/SetDepthTest/SetBlend/DrawIndexed/DrawArrays/BeginFrame/GetDefaultTarget. | `static_assert` 비복사·비이동 2 종 |
| **T3** | Program 에 uniform 캐시 멤버 + GetLocation/GetType (additive) | `src/program/program.{h,cpp}` | `mUniformCache` private 멤버. `GetLocation`/`GetType` *pure const query*. Create 가 `BuildUniformCache` 호출. 외부 캐시와 *과도기 공존*. | `static_assert` const-callable 2 종 |
| **T4** | Uniforms 내부 전환 + 레거시 자유함수 제거 | `src/program/program_uniforms.{h,cpp}`, `src/program/program.cpp` | `sCacheRegistry`/`BuildCache`/`Forget` 폐기. 각 `Set*` 가 `prog.GetLocation` + -1 fallback. *자유함수 시그니처 보존* — 외부 호출자 무수정. SP1 seam 메모 정정. | `git grep "Uniforms::BuildCache\|Uniforms::Forget"` 빈 결과 |
| **T5** | `Program::Use()` 제거 + 활성 챕터 RenderContext 마이그레이션 | `src/program/program.{h,cpp}`, `apps/<active>/main.cpp`, `apps/<active>/CMakeLists.txt` | `glUseProgram` owner 가 RenderContext. 챕터 `prog->Use()` → `rc.UseProgram(*prog)`. CMake 에 `SJH::render` 링크 추가. | 전체 빌드 + 시각 회귀 smoke test |
| **T6** | 전체 통합 검증 | (검증만) | spec §6 검증 6 단계 + §8 SP3 seam 표면 확인 | 클린 빌드, 모든 grep 통과 |

**핵심 invariant** — 매 Task 종료 시:
- 전체 빌드 PASS (SP1 종료 상태 유지)
- `Uniforms::Set*` 자유함수 시그니처 byte-identical (외부 호출자 무수정)
- `Program::GetProgramAddr` public 유지 (SP3 Material 컴포넌트 seam)
- 챕터 시각 출력 회귀 없음 (T5 마이그레이션 후 smoke test 명시)

**의존 그래프** — T1 (독립) → T2 (T1 필요: BindTarget 인자) → T3 (독립: additive) → T4 (T3 필요: GetLocation 존재) → T5 (T2 필요: UseProgram; T4 후라야 챕터가 깨지지 않음) → T6 (전수 검증).

---

## Task 1: `src/render/` 모듈 + RenderTarget 추상 + DefaultRenderTarget

**Files:**
- Create: `src/buffer/render_target.h`
- Create: `src/buffer/render_target.cpp`
- Create: `src/render/CMakeLists.txt`
- Modify: `src/CMakeLists.txt` (`add_subdirectory(render)` 추가)

- [ ] **Step 1: 신규 헤더 작성** — `src/buffer/render_target.h`

```cpp
#ifndef __SJH_RENDER_TARGET_H__
#define __SJH_RENDER_TARGET_H__

#include "GL/gl3w.h"

namespace SJH
{
    struct Size { int Width; int Height; };

    /// @brief 그릴 대상 (default framebuffer 또는 FBO) 의 추상.
    /// @note  SP4 에서 FrameBufferTarget 파생 추가 예정 — 본 인터페이스는 안정.
    class RenderTarget
    {
    public:
        virtual ~RenderTarget() = default;
        virtual void Bind() = 0;          ///< glBindFramebuffer + glViewport
        virtual Size GetSize() const = 0;
    };

    /// @brief 화면(FBO 0) 을 가리키는 기본 타깃.
    class DefaultRenderTarget : public RenderTarget
    {
    public:
        DefaultRenderTarget(int width, int height);
        void Bind() override;
        Size GetSize() const override;
    private:
        Size mSize;
    };
}

#endif // __SJH_RENDER_TARGET_H__
```

- [ ] **Step 2: 신규 구현 작성** — `src/buffer/render_target.cpp`

```cpp
#include "src/buffer/render_target.h"

namespace SJH
{
    DefaultRenderTarget::DefaultRenderTarget(int width, int height)
        : mSize{width, height} {}

    void DefaultRenderTarget::Bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, mSize.Width, mSize.Height);
    }

    Size DefaultRenderTarget::GetSize() const { return mSize; }
}
```

- [ ] **Step 3: CMake 모듈 등록** — `src/render/CMakeLists.txt`

```cmake
add_library(sjhopengl_render STATIC
    render_target.cpp
)
add_library(SJH::render ALIAS sjhopengl_render)

target_include_directories(sjhopengl_render
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sjhopengl_render
    PUBLIC  SJH::common project_deps
    PRIVATE SJH::diagnostics
)

target_compile_features(sjhopengl_render PUBLIC cxx_std_17)
```

- [ ] **Step 4: `src/CMakeLists.txt` 에 `add_subdirectory(render)` 추가**

`src/CMakeLists.txt` 의 `add_subdirectory(shader)` 줄 *직후* 에 한 줄 추가:

```cmake
add_subdirectory(buffer)
add_subdirectory(object)
add_subdirectory(common)
add_subdirectory(context)
add_subdirectory(diagnostics)
add_subdirectory(layout)
add_subdirectory(program)
add_subdirectory(material)
add_subdirectory(shader)
add_subdirectory(render)
add_subdirectory(resource_registry)
add_subdirectory(input)
```

- [ ] **Step 5: 빌드 통과 확인**

Run:
```bash
cmake --preset ninja 2>&1 | tail -5
cmake --build --preset ninja --target sjhopengl_render 2>&1 | tail -10
```
Expected: configure 성공 후 `libsjhopengl_render.a` 생성. 에러 0.

- [ ] **Step 6: 커밋**

```bash
git add src/render/ src/CMakeLists.txt
git commit -m "[feat] : RenderTarget 추상 + DefaultRenderTarget + src/render/ 모듈 신설 (SP2)

RenderTarget — Bind() + GetSize() 인터페이스.
DefaultRenderTarget — glBindFramebuffer(0) + glViewport 로 화면(FBO 0) 바인딩.
SP4 에서 FrameBufferTarget 파생 추가 예정 (인터페이스 안정 유지)."
```

---

## Task 2: RenderContext 싱글톤 + GL 게이트웨이 메서드 13 종

**Files:**
- Create: `<src>/render/render_context.h`
- Create: `<src>/render/render_context.cpp`
- Modify: `src/render/CMakeLists.txt` (`render_context.cpp` 추가, `SJH::program` 의존 추가)

- [ ] **Step 1: 싱글톤 헤더 작성** — `<src>/render/render_context.h`

```cpp
#ifndef __SJH_RENDER_CONTEXT_H__
#define __SJH_RENDER_CONTEXT_H__

#include "src/buffer/render_target.h"
#include "GL/gl3w.h"
#include <memory>

namespace SJH
{
    class Program;   // forward — 본 헤더는 Program 정의 의존 X

    /// @brief GL 호출의 단일 게이트웨이 + 현재 bound 상태 추적 (싱글톤).
    /// @details
    ///   책임 5 가지:
    ///   -# 현재 bound program 추적 (@c mBoundProgram)
    ///   -# GL 상태 변경 단일 진입점 (glUseProgram / glBindVertexArray / glBindTexture / glClear / glEnable)
    ///   -# Draw 명령 발행 (glDrawElements / glDrawArrays)
    ///   -# RenderTarget 바인딩 위임 (@c BindTarget / @c BeginFrame)
    ///   -# SP4 멀티패스 진입 지점 (@c BeginFrame 이 패스마다 다른 target 받음)
    ///
    ///   ### 순서 계약 (POLA 준수: 코드 강제 없음, 문서 명시)
    ///   - @c UseProgram(prog) 호출 후에만 그 prog 에 @c Uniforms::Set* 호출 가능.
    ///   - 디버그 빌드의 assertion 등은 *추가하지 않음* — hidden astonishment 회피.
    class RenderContext
    {
    public:
        /// @brief 싱글톤 접근. GL context 가 활성 상태일 때만 호출 유효.
        static RenderContext& Get();

        // --- bound 상태 변경 (primitive — Explicit Side Effects 준수) ---

        void UseProgram(const Program& prog);
        void BindVAO(GLuint vao);
        void BindTexture(GLuint unit, GLuint tex);
        void BindTarget(RenderTarget& target);
        void Clear(GLbitfield mask);
        void SetDepthTest(bool enabled, GLenum func = GL_LESS);
        void SetBlend(bool enabled, GLenum srcFactor = GL_SRC_ALPHA,
                                    GLenum dstFactor = GL_ONE_MINUS_SRC_ALPHA);

        // --- Draw 명령 ---

        void DrawIndexed(GLsizei count);
        void DrawArrays(GLenum mode, GLsizei count);

        // --- 편의 wrapper ---

        /// @brief BindTarget + Clear(color|depth) + SetDepthTest(true) + SetBlend(true) 의 alias.
        /// @note  Stencil pass 등 커스텀 상태가 필요하면 primitive 메서드를 *직접* 호출.
        void BeginFrame(RenderTarget& target);

        /// @brief 화면 기본 타깃 (lazy 생성, 윈도우 크기는 SetDefaultTargetSize 로 갱신).
        DefaultRenderTarget& GetDefaultTarget();

        /// @brief 윈도우 리사이즈 시 호출 — DefaultRenderTarget 크기 갱신.
        void SetDefaultTargetSize(int width, int height);

        // 복사·이동 차단 (싱글톤)
        RenderContext(const RenderContext&)            = delete;
        RenderContext& operator=(const RenderContext&) = delete;
        RenderContext(RenderContext&&)                 = delete;
        RenderContext& operator=(RenderContext&&)      = delete;

    private:
        RenderContext() = default;
        ~RenderContext() = default;

        const Program* mBoundProgram = nullptr;
        std::unique_ptr<DefaultRenderTarget> mDefaultTarget;
    };
}

#endif // __SJH_RENDER_CONTEXT_H__
```

- [ ] **Step 2: 구현 파일 작성** — `<src>/render/render_context.cpp`

```cpp
#include "<render>/render_context.h"
#include "program/program.h"
#include <type_traits>

// SP2 — 싱글톤/리소스 의미론 컴파일 타임 검증.
static_assert(!std::is_copy_constructible_v<SJH::RenderContext>,
              "SJH::RenderContext must be non-copy-constructible (singleton)");
static_assert(!std::is_move_constructible_v<SJH::RenderContext>,
              "SJH::RenderContext must be non-move-constructible (singleton)");

namespace SJH
{
    RenderContext& RenderContext::Get()
    {
        // Meyer's singleton — thread-safe in C++11+ for static local init.
        static RenderContext instance;
        return instance;
    }

    void RenderContext::UseProgram(const Program& prog)
    {
        glUseProgram(prog.GetProgramAddr());
        mBoundProgram = &prog;
    }

    void RenderContext::BindVAO(GLuint vao)            { glBindVertexArray(vao); }

    void RenderContext::BindTexture(GLuint unit, GLuint tex)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, tex);
    }

    void RenderContext::BindTarget(RenderTarget& target) { target.Bind(); }
    void RenderContext::Clear(GLbitfield mask)           { glClear(mask); }

    void RenderContext::SetDepthTest(bool enabled, GLenum func)
    {
        if (enabled) { glEnable(GL_DEPTH_TEST); glDepthFunc(func); }
        else         { glDisable(GL_DEPTH_TEST); }
    }

    void RenderContext::SetBlend(bool enabled, GLenum srcFactor, GLenum dstFactor)
    {
        if (enabled) { glEnable(GL_BLEND); glBlendFunc(srcFactor, dstFactor); }
        else         { glDisable(GL_BLEND); }
    }

    void RenderContext::DrawIndexed(GLsizei count)
    {
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
    }

    void RenderContext::DrawArrays(GLenum mode, GLsizei count)
    {
        glDrawArrays(mode, 0, count);
    }

    void RenderContext::BeginFrame(RenderTarget& target)
    {
        BindTarget(target);
        Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        SetDepthTest(true, GL_LESS);
        SetBlend(true, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    DefaultRenderTarget& RenderContext::GetDefaultTarget()
    {
        if (!mDefaultTarget)
            mDefaultTarget = std::make_unique<DefaultRenderTarget>(800, 600);
        return *mDefaultTarget;
    }

    void RenderContext::SetDefaultTargetSize(int width, int height)
    {
        mDefaultTarget = std::make_unique<DefaultRenderTarget>(width, height);
    }
}
```

- [ ] **Step 3: CMake 갱신** — `src/render/CMakeLists.txt`

```cmake
add_library(sjhopengl_render STATIC
    render_target.cpp
    render_context.cpp
)
add_library(SJH::render ALIAS sjhopengl_render)

target_include_directories(sjhopengl_render
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sjhopengl_render
    PUBLIC  SJH::common SJH::program project_deps
    PRIVATE SJH::diagnostics
)

target_compile_features(sjhopengl_render PUBLIC cxx_std_17)
```

- [ ] **Step 4: 빌드 통과 확인**

Run:
```bash
cmake --build --preset ninja --target sjhopengl_render 2>&1 | tail -10
```
Expected: PASS — `libsjhopengl_render.a` 갱신, static_assert 2 종 통과.

- [ ] **Step 5: 커밋**

```bash
git add <src>/render/render_context.h <src>/render/render_context.cpp src/render/CMakeLists.txt
git commit -m "[feat] : RenderContext 싱글톤 + GL 게이트웨이 13 메서드 (SP2)

Pattern Y (A6) 의 GL 호출 단일 진입점. 책임:
- mBoundProgram 추적
- glUseProgram / glBindVertexArray / glBindTexture / glClear / glEnable*
- glDrawElements / glDrawArrays
- RenderTarget 바인딩 위임

primitive 4 개(BindTarget/Clear/SetDepthTest/SetBlend) + BeginFrame 편의
wrapper 병존 — ddd Explicit Side Effects 준수.

복사·이동 = delete 명시 (싱글톤), static_assert 2 종 검증."
```

---

## Task 3: Program 에 uniform 캐시 멤버 + GetLocation/GetType *추가* (additive only)

**Files:**
- Modify: `src/program/program.h` (멤버 + 2 메서드 추가)
- Modify: `src/program/program.cpp` (구현 + Create 에서 BuildUniformCache 호출 추가)

- [ ] **Step 1: 헤더 수정** — `src/program/program.h`

기존 헤더에 다음 변경:
1. 상단 include 에 `<string>` + `<unordered_map>` 추가
2. 클래스 안에 `GetLocation` + `GetType` public 메서드 + `BuildUniformCache` private + `mUniformCache` 멤버 추가

수정 후 헤더의 변경 부분 (기존 코드 사이에 삽입):

```cpp
#include "common/common.h"
#include "shader/shader.h"
#include "GL/gl3w.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace SJH
{
    CLASS_PTR(Program)

    class Program
    {
    public:
        // ... 기존 Create / CreateWithVSFS / ~Program / = delete / GetProgramAddr / Use 유지 ...

        /// @brief uniform 이름 → location 조회. pure const query (캐시 read-only).
        /// @return active uniform 이면 location, 그 외(예: 비-active `arr[3]`) -1.
        /// @note  -1 반환 시 호출자가 fallback 으로 @c glGetUniformLocation 직접 호출.
        ///        본 함수는 *cache mutation 하지 않음* — POLA 준수.
        GLint GetLocation(const char* name) const;

        /// @brief uniform 이름 → GL 타입 (GL_FLOAT_MAT4 등). pure const query.
        /// @return active uniform 이면 type, 미캐시 시 0 — 진단의 타입 불일치 체크에 사용.
        GLenum GetType(const char* name) const;

    private:
        Program() = default;
        bool TryLink(const std::vector<ShaderPtr> &shaders);

        /// @brief link 직후 active uniform 전체를 mUniformCache 에 채움 (eager).
        void BuildUniformCache();

        GLuint mProgramAddr{0};

        struct UniformEntry { GLint Location; GLenum Type; };
        std::unordered_map<std::string, UniformEntry> mUniformCache;
    };
}
```

기존 `Use()` 선언은 *제거하지 않음* — Task 5 까지 보존.

- [ ] **Step 2: 구현 추가** — `src/program/program.cpp`

기존 파일 상단의 static_assert 4 종 아래에 다음을 *추가*:

```cpp
// (SP2) GetLocation / GetType 이 const 호출 가능한지 컴파일 타임 검증.
static_assert(std::is_invocable_v<decltype(&SJH::Program::GetLocation), const SJH::Program&, const char*>,
              "Program::GetLocation must be const-callable (pure query)");
static_assert(std::is_invocable_v<decltype(&SJH::Program::GetType), const SJH::Program&, const char*>,
              "Program::GetType must be const-callable (pure query)");
```

기존 `namespace SJH { ... }` 안에 다음 메서드 정의 *추가*:

```cpp
    void Program::BuildUniformCache()
    {
        GLint count = 0;
        glGetProgramiv(mProgramAddr, GL_ACTIVE_UNIFORMS, &count);
        if (count <= 0) return;

        GLint maxNameLen = 0;
        glGetProgramiv(mProgramAddr, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxNameLen);
        if (maxNameLen <= 0) return;

        std::vector<char> nameBuf(static_cast<size_t>(maxNameLen + 1), '\0');
        for (GLint i = 0; i < count; ++i)
        {
            GLsizei nameSize = 0;
            GLint   size     = 0;
            GLenum  type     = 0;
            glGetActiveUniform(mProgramAddr, static_cast<GLuint>(i),
                               maxNameLen, &nameSize, &size, &type, nameBuf.data());

            const GLint loc = glGetUniformLocation(mProgramAddr, nameBuf.data());
            mUniformCache.emplace(
                std::string(nameBuf.data(), static_cast<size_t>(nameSize)),
                UniformEntry{loc, type});
        }
    }

    GLint Program::GetLocation(const char* name) const
    {
        auto it = mUniformCache.find(name);
        if (it == mUniformCache.end()) return -1;
        return it->second.Location;
    }

    GLenum Program::GetType(const char* name) const
    {
        auto it = mUniformCache.find(name);
        if (it == mUniformCache.end()) return 0;
        return it->second.Type;
    }
```

기존 `Create` 내부에서 `Uniforms::BuildCache(*program);` 호출 *직후* 에 다음 줄 추가:

```cpp
    ProgramUPtr Program::Create(const std::vector<ShaderPtr> &shaders)
    {
        auto program = ProgramUPtr(new Program());
        if (!program->TryLink(shaders))
            return nullptr;

        // SP2 — 멤버 캐시 채움. 다음 Task 에서 Uniforms::BuildCache 호출 제거 예정.
        program->BuildUniformCache();

        // 기존 호출 — Task 4 에서 제거.
        Uniforms::BuildCache(*program);
        return program;
    }
```

(두 캐시가 *과도기적으로 공존*. Task 4 에서 외부 캐시 제거.)

- [ ] **Step 3: 빌드 통과 확인**

Run:
```bash
cmake --build --preset ninja --target sjhopengl_program 2>&1 | tail -10
```
Expected: PASS — static_assert 6 종 (SP1 의 4 + SP2 의 2) 통과, `libsjhopengl_program.a` 갱신.

- [ ] **Step 4: 커밋**

```bash
git add src/program/program.h src/program/program.cpp
git commit -m "[feat] : Program 에 uniform 캐시 멤버 + GetLocation/GetType 추가 (SP2)

Pattern Y (A6) 의 resource-attached 캐시. Additive only —
Uniforms::BuildCache 호출은 다음 Task 에서 제거 (과도기 공존).

- mUniformCache: unordered_map<string, UniformEntry>
- BuildUniformCache(): link 직후 active uniform 전체 eager 캐시
- GetLocation(name) const: pure query, 미캐시 시 -1
- GetType(name) const: pure query, 미캐시 시 0

const-callable static_assert 2 종 검증."
```

---

## Task 4: `Uniforms` 내부 전환 + 레거시 자유함수 제거

**Files:**
- Modify: `src/program/program_uniforms.h` (`BuildCache`/`Forget` 선언 제거, namespace docstring 정정)
- Modify: `src/program/program_uniforms.cpp` (`sCacheRegistry`/`LookupOrInsert` 제거, 각 `Set*` 가 `prog.GetLocation` + fallback 호출)
- Modify: `src/program/program.cpp` (`Create` 의 `Uniforms::BuildCache` 호출 제거, `~Program` 의 `Uniforms::Forget` 호출 제거)

- [ ] **Step 1: 헤더 수정** — `src/program/program_uniforms.h`

다음 두 줄 *삭제*:
```cpp
        void BuildCache(Program &prog);
        void Forget(GLuint programId);
```

`namespace Uniforms` 직전의 SP1 seam 메모 *정정* (이전: "RenderContext 의 멤버로 이전될 예정" → 정정 후):

```cpp
    /// @note (SP2 완료) 캐시는 Program 의 멤버로 이전됨. 본 namespace 의 자유 함수
    ///       family 는 *시그니처 보존* — 내부 구현이 @c prog.GetLocation 을 경유.
    ///       @c BuildCache / @c Forget 자유 함수는 폐기 (멤버 흡수).
    namespace Uniforms
    {
```

- [ ] **Step 2: 구현 전환** — `src/program/program_uniforms.cpp`

다음을 *전부 삭제*:
- anonymous namespace 의 `sCacheRegistry`, `NameToEntry` typedef, `LookupOrInsert` 함수
- `BuildCache(Program&)` 정의
- `Forget(GLuint)` 정의

각 setter (`SetMat4`/`SetVec4`/`SetVec3`/`SetVec2`/`SetFloat`/`SetInt`) 의 본문을 다음 패턴으로 *교체*:

```cpp
    void SetMat4(const Program& prog, const char* name, const vmath::mat4& m4)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0)
        {
            // Fallback: 비-active uniform (예: 배열 원소 `arr[3]`).
            // 캐시 miss 외부 fallback — Program 의 캐시는 mutation 하지 않음 (POLA).
            loc = glGetUniformLocation(pid, name);
        }
        if (loc < 0)
        {
            Diagnostics::UniformDiagnostics::NotifyMissing(pid, name);
            return;
        }
        Diagnostics::UniformDiagnostics::NotifyTypeMismatch(pid, name, GL_FLOAT_MAT4, prog.GetType(name));
        glUniformMatrix4fv(loc, 1, GL_FALSE, (const float*)m4);
    }

    void SetVec4(const Program& prog, const char* name, const vmath::vec4& v4)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(pid, name);
        if (loc < 0) { Diagnostics::UniformDiagnostics::NotifyMissing(pid, name); return; }
        Diagnostics::UniformDiagnostics::NotifyTypeMismatch(pid, name, GL_FLOAT_VEC4, prog.GetType(name));
        glUniform4fv(loc, 1, (const float*)v4);
    }

    void SetVec3(const Program& prog, const char* name, const vmath::vec3& v3)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(pid, name);
        if (loc < 0) { Diagnostics::UniformDiagnostics::NotifyMissing(pid, name); return; }
        Diagnostics::UniformDiagnostics::NotifyTypeMismatch(pid, name, GL_FLOAT_VEC3, prog.GetType(name));
        glUniform3fv(loc, 1, (const float*)v3);
    }

    void SetVec2(const Program& prog, const char* name, const vmath::vec2& v2)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(pid, name);
        if (loc < 0) { Diagnostics::UniformDiagnostics::NotifyMissing(pid, name); return; }
        Diagnostics::UniformDiagnostics::NotifyTypeMismatch(pid, name, GL_FLOAT_VEC2, prog.GetType(name));
        glUniform2fv(loc, 1, (const float*)v2);
    }

    void SetFloat(const Program& prog, const char* name, const float& v)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(pid, name);
        if (loc < 0) { Diagnostics::UniformDiagnostics::NotifyMissing(pid, name); return; }
        Diagnostics::UniformDiagnostics::NotifyTypeMismatch(pid, name, GL_FLOAT, prog.GetType(name));
        glUniform1f(loc, v);
    }

    void SetInt(const Program& prog, const char* name, const int& v)
    {
        const GLuint pid = prog.GetProgramAddr();
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(pid, name);
        if (loc < 0) { Diagnostics::UniformDiagnostics::NotifyMissing(pid, name); return; }
        // GL_INT / GL_SAMPLER_2D 등 모두 glUniform1i 라 타입 검증 생략 (SP1 정책 유지).
        glUniform1i(loc, v);
    }

    GLint Get(const Program& prog, const char* name)
    {
        GLint loc = prog.GetLocation(name);
        if (loc < 0) loc = glGetUniformLocation(prog.GetProgramAddr(), name);
        if (loc < 0)
            Diagnostics::UniformDiagnostics::NotifyMissing(prog.GetProgramAddr(), name);
        return loc;
    }
```

광원 helper (`SetDirLight`/`SetPointLight`/`SetSpotLight`) 는 *내부에서 SetVec3/SetFloat 를 재호출* 하므로 본문 변경 불요 — 그대로 둠.

- [ ] **Step 3: `Program::Create` 의 외부 캐시 호출 제거** — `src/program/program.cpp`

`Create` 본문에서 다음 줄 *삭제*:
```cpp
    Uniforms::BuildCache(*program);
```

`~Program` 본문에서 다음 줄 *삭제*:
```cpp
    Uniforms::Forget(mProgramAddr);
```

(`UniformDiagnostics::Invalidate(mProgramAddr)` 호출은 *유지* — diagnostics 모듈 무관.)

`#include "program/program_uniforms.h"` 은 `Create` 가 다른 호출이 없다면 제거 (헤더 의존 정리). 현재 `Create` 외에 호출 없으면 제거.

- [ ] **Step 4: 빌드 통과 확인**

Run:
```bash
cmake --build --preset ninja --target sjhopengl_program 2>&1 | tail -10
```
Expected: PASS — 모든 활성 챕터·라이브러리 빌드 유지.

- [ ] **Step 5: 레거시 자유 함수 부재 확인**

Run:
```bash
git grep -n 'Uniforms::BuildCache\|Uniforms::Forget' -- 'src/' 'apps/' 2>/dev/null || echo "레거시 자유 함수 호출 부재 확인"
```
Expected: `레거시 자유 함수 호출 부재 확인`.

- [ ] **Step 6: 커밋**

```bash
git add src/program/program_uniforms.h src/program/program_uniforms.cpp src/program/program.cpp
git commit -m "[refactor] : Uniforms 자유함수 내부를 prog.GetLocation 으로 전환 (SP2)

레거시 sCacheRegistry (TU-local static) 제거. 자유 함수 시그니처는 보존 —
외부 호출자(챕터, 광원 helper) 무수정. 내부 구현만 변경:

- Set* 가 prog.GetLocation 호출 (캐시 read-only)
- -1 fallback 시 glGetUniformLocation 직접 (배열 원소 등 비-active uniform)
- Uniforms::BuildCache / Forget 자유 함수 폐기 (Program 의 멤버 흡수)
- Program::Create 의 Uniforms::BuildCache 호출 제거
- ~Program 의 Uniforms::Forget 호출 제거 (mUniformCache 자동 destroy)

program_uniforms.h 의 SP1 seam 메모를 SP2 완료 상태로 정정."
```

---

## Task 5: `Program::Use()` 제거 + 활성 챕터 마이그레이션

**Files:**
- Modify: `src/program/program.h` (`Use()` 선언 제거 + Use 위의 SP1 seam 메모 제거)
- Modify: `src/program/program.cpp` (`Use()` 정의 제거)
- Modify: 활성 챕터 `main.cpp` (각 `prog->Use()` 또는 `program.Use()` → `rc.UseProgram(*prog)` 치환, `#include "<render>/render_context.h"` 추가)

- [ ] **Step 1: 활성 챕터 식별**

Run:
```bash
grep -E '^\s*add_subdirectory' apps/CMakeLists.txt | grep -v '^\s*#'
```
Expected: 활성 챕터 디렉토리 1~N 개 출력 (현재 `_MyApp_` 등). 출력된 모든 챕터가 마이그레이션 대상.

- [ ] **Step 2: 활성 챕터의 `Use()` 호출 위치 모두 식별**

Run:
```bash
# 위 Step 1 의 출력 X 마다:
git grep -n '\->Use()\|\.Use()' -- apps/X/
```
Expected: 활성 챕터의 `Program::Use()` 호출 라인들 출력. (`SJH::Program` 가 아닌 sb7::shader::* 의 `Use()` 등은 무관.)

- [ ] **Step 3: 각 활성 챕터에 RenderContext 사용 추가**

각 활성 챕터 `main.cpp` 의 include 블록에 다음 추가 (이미 있으면 skip):

```cpp
#include "<render>/render_context.h"
```

그리고 *각* `prog->Use()` 또는 `program.Use()` 호출을 다음으로 치환:

```cpp
// 이전: prog->Use();
SJH::RenderContext::Get().UseProgram(*prog);

// 또는 이미 변수로 RenderContext 를 받고 있다면:
// 이전: program.Use();
rc.UseProgram(program);
```

(`SJH::RenderContext::Get()` 을 매번 호출하는 게 부담되면 함수 진입부에서 `auto& rc = SJH::RenderContext::Get();` 한 번만 받고 재사용. 마이그레이션 시 어느 쪽이든 OK.)

- [ ] **Step 4: 활성 챕터 CMakeLists.txt 에 `SJH::render` 링크 추가**

각 활성 챕터 `apps/X/CMakeLists.txt` 의 `target_link_libraries` 에 `SJH::render` 추가:

```cmake
target_link_libraries(${CHAPTER_NAME} PRIVATE
    project_deps
    SJH::render        # 신규
    # ... 기존 의존 ...
)
```

- [ ] **Step 5: `Program::Use()` 제거** — `src/program/program.h`

다음 줄 *삭제*:
```cpp
        /// @note (SP2 seam) ...
        ///       호출 경로는 ... 으로 이전될 예정.
        void Use() const;
```

(SP1 의 doxygen seam 메모도 같이 제거 — `Use()` 가 사라지므로 메모도 사라짐.)

- [ ] **Step 6: `Program::Use()` 정의 제거** — `src/program/program.cpp`

다음 함수 정의 *전체 삭제*:
```cpp
    void Program::Use() const
    {
        glUseProgram(mProgramAddr);
    }
```

- [ ] **Step 7: 빌드 통과 확인**

Run:
```bash
cmake --build --preset ninja 2>&1 | tail -15
```
Expected: 모든 활성 챕터 빌드 성공. `Program::Use` 잔존 호출 없음.

- [ ] **Step 8: 레거시 호출 부재 확인**

Run:
```bash
git grep -n 'Program::Use\b\|\->Use()' -- 'src/' 'apps/' 2>/dev/null | grep -v 'sb7' | grep -v 'samples/' || echo "Program::Use 잔존 호출 부재"
```
Expected: `Program::Use 잔존 호출 부재` (samples/ 비활성 잔존 무시).

- [ ] **Step 9: 시각 회귀 smoke test**

각 활성 챕터를 실행해 *SP1 종료 시점과 육안 동일* 한 출력 확인:

```bash
# 활성 챕터 예시 (_MyApp_)
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_
# 라이팅/머티리얼/텍스처 표시가 회귀 없는지 시각 확인 후 종료.
cd -
```
Expected: 정상 실행 + 출력 회귀 없음.

- [ ] **Step 10: 커밋**

```bash
git add src/program/program.h src/program/program.cpp apps/
git commit -m "[refactor] : Program::Use() 제거 + 활성 챕터 RenderContext 로 마이그레이션 (SP2)

Pattern Y (A6) 의 마지막 단계 — glUseProgram owner 가 RenderContext.
활성 챕터의 prog->Use() 호출을 rc.UseProgram(*prog) 로 일괄 치환.

- src/program/program.h: Use() 선언 + SP1 seam doxygen 메모 제거
- src/program/program.cpp: Use() 정의 제거
- apps/<active>/main.cpp: include render_context.h 추가 + 호출 치환
- apps/<active>/CMakeLists.txt: SJH::render 링크 추가

시각 회귀 smoke test 수동 검증 완료."
```

---

## Task 6: 전체 통합 검증

**Files:** 없음 (검증만)

- [ ] **Step 1: 클린 빌드**

Run:
```bash
rm -rf build_ninja
cmake --preset ninja 2>&1 | tail -10
cmake --build --preset ninja 2>&1 | tail -20
```
Expected: configure + 전체 빌드 성공. (이전 SP1 종료 시점에 보고된 `src/object/CMakeLists.txt` 의 pre-existing geometry migration 이슈는 SP2 무관 — 그대로 잔존하지만 SP2 핵심 타깃 영향 없음.)

- [ ] **Step 2: SP2 핵심 라이브러리 빌드 확인**

Run:
```bash
cmake --build --preset ninja --target sjhopengl_render 2>&1 | tail -3
cmake --build --preset ninja --target sjhopengl_program 2>&1 | tail -3
cmake --build --preset ninja --target sjhopengl_shader 2>&1 | tail -3
```
Expected: 3 개 모두 PASS.

- [ ] **Step 3: SP2 spec 의 §6 검증 6 단계 일치 확인**

| Spec §6 | 본 Task 의 매핑 |
|---|---|
| 1. 빌드 통과 | Step 1, 2 |
| 2. GetLocation/GetType const 검증 | Task 3 의 static_assert |
| 3. 활성 챕터 마이그레이션 | Task 5 의 Step 1-7 |
| 4. 싱글톤 lazy init smoke | Task 5 의 Step 9 (실행 자체가 Get() 첫 호출 트리거) |
| 5. legacy 자유 함수 호출 부재 | Task 4 의 Step 5 + 본 Task Step 4 |
| 6. 시각 회귀 없음 | Task 5 의 Step 9 |

- [ ] **Step 4: legacy 자유 함수 + Program::Use 잔존 확인 (재검증)**

Run:
```bash
git grep -n 'Uniforms::BuildCache\|Uniforms::Forget\|Program::Use\b' -- 'src/' 'apps/' 2>/dev/null | grep -v 'samples/' || echo "SP2 마이그레이션 완전 — 잔존 호출 없음"
```
Expected: `SP2 마이그레이션 완전 — 잔존 호출 없음`.

- [ ] **Step 5: spec §8 SP3 seam 표 일치 확인**

다음 4 표면이 SP2 종료 시점에 *그대로 노출* 되어 있는지 코드로 확인:

```bash
echo "=== RenderContext::Get ==="
grep -n 'static RenderContext& Get' <src>/render/render_context.h
echo "=== RenderContext primitive 메서드 ==="
grep -n 'UseProgram\|BindVAO\|BindTexture\|DrawIndexed' <src>/render/render_context.h
echo "=== Program::GetProgramAddr ==="
grep -n 'GetProgramAddr() const' src/program/program.h
echo "=== Uniforms::Set* family ==="
grep -n 'void Set' src/program/program_uniforms.h
```
Expected: 4 표면 모두 노출.

---

## Out of Scope (재확인)

| 항목 | 위치 |
|------|------|
| ECS 컴포넌트 (Transform/Mesh/Material/MeshRenderer) | **SP3** |
| `RenderQueue<DrawCommand>` + 정렬 + 루프 순회 | **SP3** RenderSystem 내부 |
| `Render` 씬 순회 로직 | **SP3** |
| `FrameBufferTarget` (오프스크린) | **SP4** |
| Mesh/Model 리소스 모듈 (VAO/VBO/EBO RAII) | **별도 분해 갱신** — SP3 전 |
| DSA `glProgramUniform*` (GL 4.1+) 전환 | **폐기** — GL 3.3 유지 |
| Polymorphic Program (Default/Texture variants) | **폐기** — SP3 Material 데이터 분기 흡수 |
| GL 상태 캐싱 redundancy 제거 최적화 | **미래 최적화** |
