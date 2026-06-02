# Doxygen 문서화 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **진행 상태 (2026-05-08 갱신):**
> - Phase 1~6 ✅ 완료 (커밋 `d34c507` ~ `503df50`).
> - Phase 5 의 `buffer` / `layout` / `resource_management` placeholder 처리는 이후 세 모듈이 실제 활성화되며 의미가 변경됨 — 각 헤더 자체에 정상 Doxygen 주석 작성됨.
> - **Phase 7 🚧 진행 중** — `src/object/camera.h` + `src/context/context.h` 의 입력 위임 메서드 docstring 추가. 본 문서 끝부분(§Phase 7) 참조.

**Goal:** [doc/doxygen-documentation-plan.md](doxygen-documentation-plan.md) 의 6개 Phase 를 실행하여 `cmake --build build_Darwin --target doxygen` 으로 모듈 API + 다이어그램 + 빌드 가이드를 포함한 HTML 문서를 생성한다. Phase 7 에서 신규 모듈/메서드의 Doxygen 보강.

**Architecture:** Phase 1~5 는 `src/<module>/<file>.h` 에 표준 Doxygen 주석을 추가하고 `.cpp` 의 비-자명한 인라인 주석만 정리. Phase 6 은 `doc/Doxyfile.in` 에 Graphviz/페이지 옵션 추가 + `doc/pages/` 마크다운으로 메인페이지·빌드가이드·의존성 문서를 작성하고 `\dot ... \enddot` 디렉티브로 레이어/시퀀스 다이어그램을 임베드.

**Tech Stack:** Doxygen 1.9+, Graphviz (`brew install graphviz`), CMake `configure_file`, GLFW/glad/spdlog (이미 설치됨)

**언어:** 한국어. 톤은 [src/diagnostics/gl_log.h](../src/diagnostics/gl_log.h) 와 일치.

**Phase 검토 게이트:** 각 Phase 마지막 task는 사용자 검토 단계. 사용자 승인 전에 다음 Phase 진입 금지.

---

## 사전 준비 (1회만)

### Task 0: 의존성 확인

**Files:**
- Read-only: 환경 도구 점검

- [ ] **Step 0.1: Doxygen 설치 확인**

Run: `doxygen --version`
Expected: `1.9.x` 이상 출력. 설치되지 않으면 `brew install doxygen`.

- [ ] **Step 0.2: Graphviz 설치 확인 (Phase 6 전제)**

Run: `dot -V`
Expected: `dot - graphviz version 2.x` 또는 `12.x` 출력. 설치되지 않으면 `brew install graphviz`.

- [ ] **Step 0.3: 현재 Doxygen 빌드 베이스라인 확인**

Run: `cmake --build build_Darwin --target doxygen 2>&1 | tail -20`
Expected: `Generating API documentation with Doxygen` + warnings 다수 (현재 주석 누락 상태). 경고 개수를 메모해두면 Phase별 감소 추세를 비교할 수 있음.

---

## Phase 1: 기반 모듈 (common + diagnostics)

**의존성:** 없음. `CLASS_PTR` 매크로 톤이 모든 모듈에 영향.

### Task 1.1: `common.h` 에 Doxygen 주석 추가

**Files:**
- Modify: `src/common/common.h`

- [ ] **Step 1.1.1: 헤더 전체를 다음으로 교체**

```cpp
#ifndef __SJH_COMMON_H__
#define __SJH_COMMON_H__

#pragma once

#include <memory>
#include <string>
#include <optional>

/**
 * @def CLASS_PTR
 * @brief 클래스 forward declaration + std 스마트 포인터 별칭을 일괄 생성하는 매크로.
 * @details 다음 3가지 typedef 를 한 번에 정의:
 *  - @c <klassName>UPtr — @c std::unique_ptr<klassName> (단독 소유)
 *  - @c <klassName>Ptr  — @c std::shared_ptr<klassName> (공유 소유)
 *  - @c <klassName>WPtr — @c std::weak_ptr<klassName>   (약한 참조)
 * @par 예시
 * @code
 * CLASS_PTR(Shader)  // ShaderUPtr / ShaderPtr / ShaderWPtr 자동 생성
 * @endcode
 * @note 모든 @c SJH:: 네임스페이스 클래스 헤더 상단에서 사용.
 */
#define CLASS_PTR(klassName) \
class klassName; \
using klassName ## UPtr = std::unique_ptr<klassName>; \
using klassName ## Ptr = std::shared_ptr<klassName>; \
using klassName ## WPtr = std::weak_ptr<klassName>;

namespace SJH {
    /**
     * @brief 텍스트 파일을 한 번에 읽어 @c std::string 으로 반환.
     * @param filename 읽을 파일의 경로 (실행 디렉토리 기준 상대 경로 또는 절대 경로).
     * @return 성공 시 파일 내용 문자열, 실패 시 @c std::nullopt.
     * @details 실패 사유(파일 없음/권한 등)는 @c spdlog::error 로 출력.
     *          GLSL 셰이더 소스 등 텍스트 리소스 로딩에 사용.
     */
    std::optional<std::string> LoadTextFile(const std::string& filename);
}

#endif //__SJH_COMMON_H__
```

- [ ] **Step 1.1.2: 빌드 통과 확인**

Run: `cmake --build build_Darwin --target sjhopengl_common 2>&1 | tail -5`
Expected: `Built target sjhopengl_common` 출력 (에러 없음).

### Task 1.2: `common.cpp` 인라인 주석 정리

**Files:**
- Modify: `src/common/common.cpp:8`

- [ ] **Step 1.2.1: 오타 수정 + 의도 보존**

기존 `// cpp 스타일의 파일 로팅 방식이다.` 를 `// ifstream 으로 전체 내용을 stringstream 에 흘려넣는 표준 패턴` 로 교체. (오타 "로팅"→내용 명확화)

Edit: `src/common/common.cpp:8`
old: `        // cpp 스타일의 파일 로팅 방식이다.`
new: `        // ifstream 으로 전체 내용을 stringstream 에 흘려넣는 표준 패턴`

### Task 1.3: `diagnostics/gl_log.h` 검토

**Files:**
- Read-only: `src/diagnostics/gl_log.h`

- [ ] **Step 1.3.1: 현재 주석 상태 점검 — 변경 없음**

이미 완전한 Doxygen 주석 보유 (`@brief`, `@param`, `@return`, `@details`, `@note`, `@warning`, `@code`, `@par`). 본 Phase 에서 변경 작업 없음. 모범 사례로 후속 모듈이 따르는 톤 기준점.

### Task 1.4: `diagnostics/gl_log.cpp` 인라인 주석 정리

**Files:**
- Modify: `src/diagnostics/gl_log.cpp:15` (한 줄)
- Modify: `src/diagnostics/gl_log.cpp:30-31` (두 줄)

- [ ] **Step 1.4.1: 자명한 한글 주석 제거**

Edit: `src/diagnostics/gl_log.cpp:15`
old: `            //shader에 대한 정수형 정보를 얻어옴`
new: (빈 줄로 교체 — 함수명과 호출 패턴이 자명)

실제로는 빈 줄을 남기지 않고 그 한 줄을 통째 삭제:

Old:
```cpp
            GLint length = 0;
            //shader에 대한 정수형 정보를 얻어옴
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
```

New:
```cpp
            GLint length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
```

- [ ] **Step 1.4.2: FetchProgramInfoLog 의 자명한 주석 제거**

Old:
```cpp
            std::string log(static_cast<size_t>(length), '\0');
            // glGetProgramInfoLog(): program에 대한 로그를 얻어옴.
            // 링크 에러 얻어내는 용도로 사용
            glGetProgramInfoLog(program, length, nullptr, log.data());
```

New:
```cpp
            std::string log(static_cast<size_t>(length), '\0');
            glGetProgramInfoLog(program, length, nullptr, log.data());
```

함수 자체가 이미 "info log fetching" 의도가 명확하므로 인라인 주석 불필요.

### Task 1.5: Phase 1 검증 + 커밋

- [ ] **Step 1.5.1: 빌드**

Run: `cmake --build build_Darwin 2>&1 | tail -5`
Expected: 모든 타겟 빌드 성공.

- [ ] **Step 1.5.2: Doxygen 빌드 + 결과 확인**

Run: `cmake --build build_Darwin --target doxygen 2>&1 | grep -E "warning|error" | wc -l`
Expected: 베이스라인보다 감소한 경고 수.

Run: `open doc/html/index.html`
확인:
- `SJH::LoadTextFile` 페이지에 한국어 설명이 표시되는가?
- `CLASS_PTR` 매크로 페이지에 예시 코드 블록이 렌더되는가?

- [ ] **Step 1.5.3: 커밋**

```bash
git add src/common/common.h src/common/common.cpp src/diagnostics/gl_log.cpp
git commit -m "$(cat <<'EOF'
[doc] : Phase 1 — common/diagnostics doxygen comments

- common.h: CLASS_PTR 매크로 + LoadTextFile 표준 Doxygen 주석 추가
- common.cpp: ifstream 패턴 의도 명확화
- diagnostics/gl_log.cpp: 자명한 인라인 주석 정리
EOF
)"
```

### 🔒 REVIEW GATE 1 — Phase 1 완료

사용자에게 보여줄 것:
- `doc/html/index.html` 의 `SJH::LoadTextFile` 페이지 스크린샷 또는 핵심 텍스트
- `doxygen` 출력의 경고 감소량
- 다음 모듈(`shader`)의 Doxygen draft 미리보기

사용자 승인 후 Phase 2 진입.

---

## Phase 2: shader 모듈

**의존성:** common, diagnostics (Phase 1 완료 전제).

### Task 2.1: `shader.h` 의 한글 블록 코멘트를 Doxygen 으로 변환

**Files:**
- Modify: `src/shader/shader.h` (전체 교체)

- [ ] **Step 2.1.1: 헤더 전체를 다음으로 교체**

```cpp
#ifndef __SHADER_H__
#define __SHADER_H__

#pragma once

#include <glad/glad.h>
#include "common/common.h"

namespace SJH
{
    CLASS_PTR(Shader)

    /**
     * @brief OpenGL 셰이더 객체(@c GL_VERTEX_SHADER / @c GL_FRAGMENT_SHADER 등) 의 RAII 래퍼.
     * @details 팩토리 함수 @ref CreateFromFile 으로만 인스턴스 생성 가능.
     *          외부에 노출되는 인스턴스는 항상 컴파일까지 완료된 유효한 GL 핸들을 보유한다는
     *          불변식을 가진다. 소멸자에서 @c glDeleteShader 자동 호출.
     */
    class Shader
    {
    public:
        /**
         * @brief 파일에서 셰이더 소스를 읽어 컴파일 후 @c Shader 인스턴스 생성.
         * @param filename     셰이더 소스 파일 경로 (예: @c "resources/shader/simple.vs").
         * @param shader_type  GL 셰이더 타입 (@c GL_VERTEX_SHADER, @c GL_FRAGMENT_SHADER 등).
         * @return 성공 시 @c ShaderUPtr (소유권 이전), 실패(파일 없음/컴파일 에러) 시 @c nullptr.
         * @details 팩토리 패턴이 다음 3가지를 보장한다:
         *  -# **예외 없는 실패 처리** — 생성자는 실패를 신호할 방법이 없으므로 팩토리가 @c nullptr 반환으로 처리.
         *  -# **RAII 소유권 강제** — 생성자 @c private + 반환 타입 @c UPtr 의 협력:
         *     생성자 private 으로 직접 생성 차단, @c UPtr 반환으로 호출자에게 자동 소유권 이전.
         *  -# **클래스 불변식** — 외부 노출 인스턴스는 항상 유효한 GL 핸들 보유:
         *     팩토리 내부에서 빈 객체 생성 → @c TryLoadFile 로 GL 자원 획득 시도 →
         *     성공 시 소유권 이전, 실패 시 임시 UPtr 즉시 파괴 + @c nullptr 반환.
         * @note 컴파일 에러 로그는 @c diagnostics::GLObjectLog::CheckShaderCompile 가 출력.
         */
        static ShaderUPtr CreateFromFile(const std::string &filename, GLenum shader_type);

        /// @brief @c glDeleteShader 호출 (핸들이 0 이 아닐 때만).
        ~Shader();

        /// @brief 내부 GL 셰이더 핸들 반환 — @c Program::Create 가 attach 시 사용.
        GLuint GetShaderAddr() const { return mShaderAddr; }

    private:
        Shader() = default;
        bool TryLoadFile(const std::string &filename, GLenum shader_type);
        GLuint mShaderAddr{0};
    };
}
#endif // __SHADER_H__
```

- [ ] **Step 2.1.2: 빌드 통과 확인**

Run: `cmake --build build_Darwin --target sjhopengl_shader 2>&1 | tail -5`
Expected: `Built target sjhopengl_shader` 출력.

### Task 2.2: `shader.cpp` 인라인 주석 정리

**Files:**
- Modify: `src/shader/shader.cpp`

- [ ] **Step 2.2.1: 학습자 메모성 주석 제거 + 의도 주석 정제**

Edit: `src/shader/shader.cpp:9`
Old: `        // 생성자를 Private로 하였다고 해서 내부에서 호출 못하는것은 아니네?`
New: `        // private 생성자도 클래스 자신의 static 멤버에서는 호출 가능 — 팩토리 패턴의 핵심`

Edit: `src/shader/shader.cpp:13`
Old: `        // UPtr를 Move 소유권 이전.`
New: (해당 줄 삭제 — `std::move` 가 자명)

실제 변경 후 `src/shader/shader.cpp:7-15` 이 다음과 같이 되어야 함:

```cpp
    ShaderUPtr Shader::CreateFromFile(const std::string &filename, GLenum shader_type)
    {
        // private 생성자도 클래스 자신의 static 멤버에서는 호출 가능 — 팩토리 패턴의 핵심
        auto shader = std::unique_ptr<Shader>(new Shader());
        if (!shader->TryLoadFile(filename, shader_type))
            return nullptr;
        return std::move(shader);
    }
```

- [ ] **Step 2.2.2: TryLoadFile 의 자명한 단계 주석은 보존**

`TryLoadFile` 의 `// OpenGL shader object 생성`, `// shader에 소스 코드 설정`, `// 셰이더 컴파일` 주석은 OpenGL 학습자 관점에서 단계 이정표로 가치 있음 → **보존**.

변경 없음.

- [ ] **Step 2.2.3: 잘못된 include 정리**

`src/shader/shader.cpp:1` 이 `#include "context/context.h"` 인데 shader.cpp 는 context 를 사용하지 않음 → 삭제하고 올바른 헤더 include.

Edit: `src/shader/shader.cpp:1-3`
Old:
```cpp
#include "context/context.h"
#include "diagnostics/gl_log.h"
#include <memory>
```
New:
```cpp
#include "shader/shader.h"
#include "diagnostics/gl_log.h"
#include <memory>
```

- [ ] **Step 2.2.4: 빌드 재검증**

Run: `cmake --build build_Darwin 2>&1 | tail -5`
Expected: 전체 빌드 성공 (잘못된 include 제거가 회귀 없음을 확인).

### Task 2.3: Phase 2 검증 + 커밋

- [ ] **Step 2.3.1: Doxygen 빌드**

Run: `cmake --build build_Darwin --target doxygen 2>&1 | grep -E "warning" | wc -l`
Expected: Phase 1 대비 추가 감소.

Run: `open doc/html/classSJH_1_1Shader.html`
확인:
- `CreateFromFile` 페이지에 팩토리 패턴 3가지 보장 항목이 번호 매겨져 표시되는가?
- `Shader` 클래스 페이지에 RAII 불변식 설명이 보이는가?

- [ ] **Step 2.3.2: 커밋**

```bash
git add src/shader/shader.h src/shader/shader.cpp
git commit -m "$(cat <<'EOF'
[doc] : Phase 2 — shader doxygen comments

- shader.h: 팩토리 패턴 3가지 보장 + 불변식 Doxygen 변환
- shader.cpp: 학습자 메모 주석 정리 + 잘못된 include 수정 (context -> shader)
EOF
)"
```

### 🔒 REVIEW GATE 2 — Phase 2 완료

사용자에게 보여줄 것:
- `classSJH_1_1Shader.html` 의 `CreateFromFile` 섹션 핵심 텍스트
- 다음 모듈(`program`)의 Doxygen draft 미리보기 — shader 와 유사하지만 벡터 입력

사용자 승인 후 Phase 3 진입.

---

## Phase 3: program 모듈

**의존성:** shader (Phase 2 완료 전제).

### Task 3.1: `program.h` 에 Doxygen 주석 추가

**Files:**
- Modify: `src/program/program.h` (전체 교체)

- [ ] **Step 3.1.1: 헤더 전체를 다음으로 교체**

```cpp
#ifndef __SJH_PROGRAM_H__
#define __SJH_PROGRAM_H__

#include "common/common.h"
#include "shader/shader.h"
#include <vector>
#include <glad/glad.h>

namespace SJH
{
    CLASS_PTR(Program)

    /**
     * @brief 컴파일된 셰이더들을 attach + link 한 OpenGL 프로그램 객체의 RAII 래퍼.
     * @details 팩토리 함수 @ref Create 으로만 인스턴스 생성 가능.
     *          외부 노출 인스턴스는 항상 링크까지 완료된 유효한 GL 핸들을 보유한다.
     *          소멸자에서 @c glDeleteProgram 자동 호출.
     */
    class Program
    {
    public:
        /**
         * @brief 셰이더 벡터를 받아 프로그램 생성 + attach + link 일괄 수행.
         * @param shaders 링크할 셰이더 (vertex / fragment 등 — 보통 2~3개). @c ShaderPtr (shared) 사용.
         * @return 성공 시 @c ProgramUPtr, 링크 실패 시 @c nullptr.
         * @details 입력이 @c shared_ptr 이므로 호출자가 같은 셰이더를 여러 프로그램에 재사용 가능.
         *          링크 후에는 @c glDetachShader 호출 없이도 셰이더 객체 파괴 시 자동 분리됨.
         * @note 링크 에러 로그는 @c diagnostics::GLObjectLog::CheckProgramLink 가 출력.
         * @see Shader::CreateFromFile
         */
        static ProgramUPtr Create(const std::vector<ShaderPtr> &shaders);

        /// @brief @c glDeleteProgram 호출 (핸들이 0 이 아닐 때만).
        ~Program();

        /// @brief 내부 GL 프로그램 핸들 반환 — @c glUseProgram 호출 시 사용.
        GLuint GetProgramAddr() const { return mProgramAddr; }

    private:
        Program() = default;
        bool TryLink(const std::vector<ShaderPtr> &shaders);
        GLuint mProgramAddr{0};
    };
}

#endif // __SJH_PROGRAM_H__
```

- [ ] **Step 3.1.2: 빌드 통과 확인**

Run: `cmake --build build_Darwin --target sjhopengl_program 2>&1 | tail -5`
Expected: `Built target sjhopengl_program` 출력.

### Task 3.2: `program.cpp` 인라인 주석 정리

**Files:**
- Modify: `src/program/program.cpp:23-25`

- [ ] **Step 3.2.1: attach 루프에 의도 주석 추가**

Old:
```cpp
        mProgramAddr = glCreateProgram();
        for (auto &shader : shaders)
            glAttachShader(mProgramAddr, shader->GetShaderAddr());
```

New:
```cpp
        mProgramAddr = glCreateProgram();
        // 모든 셰이더를 program 에 attach — 링크 시 셰이더 단계가 결합됨
        for (auto &shader : shaders)
            glAttachShader(mProgramAddr, shader->GetShaderAddr());
```

### Task 3.3: Phase 3 검증 + 커밋

- [ ] **Step 3.3.1: Doxygen 빌드**

Run: `cmake --build build_Darwin --target doxygen 2>&1 | grep -E "warning" | wc -l`
Expected: 추가 감소.

Run: `open doc/html/classSJH_1_1Program.html`
확인: `Create` 메서드에 셰이더 vector 설명 + `@see Shader::CreateFromFile` 링크가 동작.

- [ ] **Step 3.3.2: 커밋**

```bash
git add src/program/program.h src/program/program.cpp
git commit -m "$(cat <<'EOF'
[doc] : Phase 3 — program doxygen comments

- program.h: Create 팩토리 + ShaderPtr 입력 의도 Doxygen 추가
- program.cpp: attach 루프 의도 주석 한 줄
EOF
)"
```

### 🔒 REVIEW GATE 3 — Phase 3 완료

사용자에게 보여줄 것: `classSJH_1_1Program.html` + 다음 모듈(`context`) draft.

---

## Phase 4: context 모듈

**의존성:** program (Phase 3 완료 전제).

### Task 4.1: `context.h` Doxygen 변환 + 책임 분담 블록 보존

**Files:**
- Modify: `src/context/context.h` (전체 교체)

- [ ] **Step 4.1.1: 헤더 전체를 다음으로 교체**

```cpp
#ifndef __SJH_CONTEXT_H__
#define __SJH_CONTEXT_H__

#include "common/common.h"
#include "program/program.h"
#include "shader/shader.h"

namespace SJH
{
    CLASS_PTR(Context)

    /**
     * @brief 한 씬(scene)의 OpenGL 자원과 매 프레임 draw call 시퀀스를 캡슐화하는 클래스.
     *
     * @par 책임 분담 — Context 가 담당하는 것 (Scene-specific, 고변동)
     *  - VBO/EBO 생성 (Buffer)              — @c GL_ARRAY_BUFFER / @c GL_ELEMENT_ARRAY_BUFFER
     *  - VAO + Vertex Attribute (VertexLayout) — @c glGenVertexArrays + @c glVertexAttribPointer
     *  - 셰이더/프로그램 (@ref Shader / @ref Program)
     *  - 텍스처 로드 + 바인딩 + uniform 설정
     *  - 매 프레임 draw call 시퀀스 (@ref Render)
     *  - 위 객체들의 자동 파괴 (@c UPtr 소멸 시점)
     *
     * @par Context 가 담당하지 *않는* 것 — 앱 전반(저변동, 보일러플레이트)
     *  - GLFW @c init/terminate, OpenGL 컨텍스트 생성, glad 함수 로딩
     *  - 키/마우스 입력 콜백, 프레임버퍼 리사이즈
     *  - @c glfwSwapBuffers / @c glfwPollEvents 메인 루프
     *  - 위 항목은 @c app/main.cpp 의 라이프 사이클 영역.
     */
    class Context
    {
    public:
        /**
         * @brief Context 인스턴스 생성 + 내부 GL 자원 초기화 (셰이더 컴파일/링크/VAO 생성).
         * @return 초기화 성공 시 @c ContextUPtr, 셰이더/프로그램 생성 실패 시 @c nullptr.
         * @pre 호출 전 OpenGL 컨텍스트가 활성화되어 있고 glad 가 로드된 상태여야 함.
         */
        static ContextUPtr Create();

        /// @brief 매 프레임 호출 — 프레임버퍼 클리어 + draw call.
        void Render();

    private:
        Context() = default;
        bool Init();
        ProgramUPtr mProgram;
    };
}

#endif // __SJH_CONTEXT_H__
```

(주: 기존의 `// __CONTEXT_H_` 오타도 `// __SJH_CONTEXT_H__` 로 정정됨)

- [ ] **Step 4.1.2: 빌드 통과 확인**

Run: `cmake --build build_Darwin --target sjhopengl_context 2>&1 | tail -5`
Expected: `Built target sjhopengl_context` 출력.

### Task 4.2: `context.cpp` 인라인 주석 정리

**Files:**
- Modify: `src/context/context.cpp`

- [ ] **Step 4.2.1: 슬랭/구어체 정리, 의도 주석 보존**

Edit: `src/context/context.cpp:17-22`
Old:
```cpp
        glClear(GL_COLOR_BUFFER_BIT); // 프레임 버퍼 클리어

        // 딸랑 점 하나 그리기.
        glUseProgram(mProgram->GetProgramAddr());
        glPointSize(10.0f);
        glDrawArrays(GL_POINTS, 0, 1);
```
New:
```cpp
        glClear(GL_COLOR_BUFFER_BIT);

        // 학습용 — 점 1개만 그리기 (멀티 텍스처/사각형은 마이그레이션 예정)
        glUseProgram(mProgram->GetProgramAddr());
        glPointSize(10.0f);
        glDrawArrays(GL_POINTS, 0, 1);
```

Edit: `src/context/context.cpp:27`
Old: `        // Shader 인스턴스가 unique_ptr에서 shared_ptr로 변환되었음을 유의`
New: `        // ShaderUPtr -> ShaderPtr 암묵 변환 — Program::Create 가 shared 입력을 요구`

Edit: `src/context/context.cpp:39`
Old: `        glClearColor(0.0, 0.1f, 0.2f, 0.0f); // 프레임 버퍼에 씌울 컬러 지정`
New: `        glClearColor(0.0, 0.1f, 0.2f, 0.0f);`

(자명한 주석 제거)

### Task 4.3: Phase 4 검증 + 커밋

- [ ] **Step 4.3.1: Doxygen 빌드**

Run: `cmake --build build_Darwin --target doxygen 2>&1 | grep -E "warning" | wc -l`
Expected: 추가 감소.

Run: `open doc/html/classSJH_1_1Context.html`
확인:
- "책임 분담" 두 개의 `@par` 블록이 깔끔하게 표시되는가?
- 멤버 객체 그래프(자동) 에 `Program` 이 보이는가? (Phase 6 의 collaboration graph 활성화 후 본격적으로 확인)

- [ ] **Step 4.3.2: 커밋**

```bash
git add src/context/context.h src/context/context.cpp
git commit -m "$(cat <<'EOF'
[doc] : Phase 4 — context doxygen comments

- context.h: 책임 분담 철학(@par 블록) Doxygen 변환 + include guard 오타 수정
- context.cpp: 슬랭/자명한 주석 정리, ShaderUPtr->ShaderPtr 변환 의도 명확화
EOF
)"
```

### 🔒 REVIEW GATE 4 — Phase 4 완료

사용자에게 보여줄 것: `classSJH_1_1Context.html` 의 책임 분담 섹션 + Phase 5 placeholder 처리 안내.

---

## Phase 5: placeholder 모듈 (buffer / layout / resource_management)

**의존성:** 없음 (다른 모듈에 영향 없음).

**목적:** Doxygen `EXTRACT_ALL=YES` 로 빈 namespace 도 출력되므로, "의도된 빈 상태" 임을 명시.

### Task 5.1: 세 placeholder 헤더에 동일 패턴 주석 추가

**Files:**
- Modify: `src/buffer/buffer.h`
- Modify: `src/layout/vertex_layout.h`
- Modify: `src/resource_management/resource_management.h`

- [ ] **Step 5.1.1: `buffer.h` 교체**

```cpp
/**
 * @file buffer.h
 * @brief **Placeholder 모듈** — VBO/EBO RAII 래퍼 구현 예정.
 * @details 현재 비활성. 활성화 시 @c src/CMakeLists.txt 의 @c add_subdirectory(buffer) 주석 해제.
 *          마이그레이션 계획은 [migration-plan.md](../../doc/migration-plan.md) 참조.
 */
namespace SJH {}
```

- [ ] **Step 5.1.2: `vertex_layout.h` 교체**

```cpp
/**
 * @file vertex_layout.h
 * @brief **Placeholder 모듈** — VAO + glVertexAttribPointer 래퍼 구현 예정.
 * @details 현재 비활성. 활성화 시 @c src/CMakeLists.txt 의 @c add_subdirectory(layout) 주석 해제.
 *          마이그레이션 계획은 [migration-plan.md](../../doc/migration-plan.md) 참조.
 */
namespace SJH {}
```

- [ ] **Step 5.1.3: `resource_management.h` 교체**

```cpp
#ifndef __SJH_RESOURCE_MANAGEMENT_H__
#define __SJH_RESOURCE_MANAGEMENT_H__

/**
 * @file resource_management.h
 * @brief **Placeholder 모듈** — 텍스처/이미지 등 리소스 라이프사이클 관리 예정.
 * @details 현재 비활성. 활성화 시 @c src/CMakeLists.txt 에 @c add_subdirectory(resource_management) 추가.
 */

#endif//__SJH_RESOURCE_MANAGEMENT_H__
```

- [ ] **Step 5.1.4: 빌드 통과 확인**

Run: `cmake --build build_Darwin 2>&1 | tail -5`
Expected: 빌드 성공 (이 모듈들은 add_subdirectory 안 되어 있으므로 헤더만 변경, 빌드 영향 없음).

### Task 5.2: Phase 5 검증 + 커밋

- [ ] **Step 5.2.1: Doxygen 빌드**

Run: `cmake --build build_Darwin --target doxygen 2>&1 | tail -10`
Expected: 새로운 placeholder 페이지 생성. 빈 namespace 경고 사라짐.

- [ ] **Step 5.2.2: 커밋**

```bash
git add src/buffer/buffer.h src/layout/vertex_layout.h src/resource_management/resource_management.h
git commit -m "$(cat <<'EOF'
[doc] : Phase 5 — placeholder modules @file doxygen

- buffer.h / vertex_layout.h / resource_management.h: 비활성 의도 + migration-plan 링크
EOF
)"
```

### 🔒 REVIEW GATE 5 — Phase 5 완료

사용자에게 보여줄 것: 3개 placeholder HTML 페이지가 깔끔하게 표시되는지.

---

## Phase 6: 다이어그램 + 페이지 통합

**의존성:** Phase 1~5 모든 주석 작업 완료.

### Task 6.1: `cmake/Doxygen.cmake` 에 `DOXYGEN_PAGES_DIR` 변수 추가

**Files:**
- Modify: `cmake/Doxygen.cmake`

- [ ] **Step 6.1.1: 변수 추가**

Edit: `cmake/Doxygen.cmake:12-13`
Old:
```cmake
    set(DOXYGEN_INPUT_DIR  "${PROJECT_SOURCE_DIR}/src")
    set(DOXYGEN_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/doc")
```
New:
```cmake
    set(DOXYGEN_INPUT_DIR  "${PROJECT_SOURCE_DIR}/src")
    set(DOXYGEN_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/doc")
    set(DOXYGEN_PAGES_DIR  "${PROJECT_SOURCE_DIR}/doc/pages")
```

### Task 6.2: `doc/Doxyfile.in` 갱신

**Files:**
- Modify: `doc/Doxyfile.in`

- [ ] **Step 6.2.1: INPUT 라인 갱신**

Edit: `doc/Doxyfile.in:13`
Old: `INPUT                  = @DOXYGEN_INPUT_DIR@`
New: `INPUT                  = @DOXYGEN_INPUT_DIR@ @DOXYGEN_PAGES_DIR@`

- [ ] **Step 6.2.2: 다이어그램 옵션 추가 — 파일 끝에 append**

Edit: `doc/Doxyfile.in:38` 다음 줄에 (`USE_MDFILE_AS_MAINPAGE = README.md` 뒤)에 다음 블록 추가:

```
#  메인 페이지 — README.md 가 없으면 \mainpage 가 있는 첫 번째 마크다운으로 자동 선택됨
#  현재는 doc/pages/00-mainpage.md 가 \mainpage 디렉티브 보유

#  Graphviz 자동 다이어그램
HAVE_DOT               = YES
DOT_IMAGE_FORMAT       = svg
INTERACTIVE_SVG        = YES
DOT_TRANSPARENT        = YES
CLASS_GRAPH            = YES
COLLABORATION_GRAPH    = YES
INCLUDE_GRAPH          = YES
INCLUDED_BY_GRAPH      = YES
CALL_GRAPH             = NO
CALLER_GRAPH           = NO
GRAPHICAL_HIERARCHY    = YES
DIRECTORY_GRAPH        = YES

#  마크다운 페이지 처리
MARKDOWN_SUPPORT       = YES
TOC_INCLUDE_HEADINGS   = 3
```

또한 `USE_MDFILE_AS_MAINPAGE` 를 신설할 메인페이지 마크다운으로 교체:

Edit: `doc/Doxyfile.in:38`
Old: `USE_MDFILE_AS_MAINPAGE = README.md`
New: `USE_MDFILE_AS_MAINPAGE = @DOXYGEN_PAGES_DIR@/00-mainpage.md`

### Task 6.3: `doc/pages/` 디렉토리 + 메인 페이지 작성

**Files:**
- Create: `doc/pages/00-mainpage.md`

- [ ] **Step 6.3.1: 디렉토리 생성 확인**

Run: `mkdir -p doc/pages`
Expected: 에러 없음 (idempotent).

- [ ] **Step 6.3.2: 메인 페이지 작성**

Create: `doc/pages/00-mainpage.md` with content:

````markdown
# OpenGL-With-CMake {#mainpage}

OpenGL 학습 프로젝트 — C++17 + CMake + vcpkg manifest mode.

## 모듈 레이어 다이어그램

\dot
digraph LayerArchitecture {
  rankdir=BT;
  node [shape=box, style=rounded, fontname="Helvetica"];

  subgraph cluster_app {
    label="앱 (라이프 사이클)";
    style=dashed;
    main [label="app/main.cpp"];
  }

  subgraph cluster_scene {
    label="씬 캡슐화 (Context)";
    style=dashed;
    Context;
  }

  subgraph cluster_modules {
    label="GL 모듈";
    style=dashed;
    Program; Shader;
  }

  subgraph cluster_base {
    label="기반";
    style=dashed;
    Common; Diagnostics;
  }

  subgraph cluster_external {
    label="외부 (vcpkg)";
    style=filled; fillcolor="#f0f0f0";
    glfw; glad; spdlog; fmt;
  }

  main -> Context;
  Context -> Program;
  Context -> Shader;
  Program -> Shader;
  Shader -> Common;
  Shader -> Diagnostics;
  Program -> Diagnostics;
  Context -> Diagnostics;
  Common -> spdlog;
  Diagnostics -> spdlog;
  Diagnostics -> glad;
  Shader -> glad;
  Program -> glad;
  Context -> glad;
  main -> glfw;
}
\enddot

## 렌더링 시퀀스 (high-level)

\dot
digraph RenderSequence {
  rankdir=LR;
  node [shape=box, fontname="Helvetica"];
  Start [label="main()", shape=ellipse];
  init [label="GLFW init\n+ glad load"];
  ctx [label="Context::Create()\n→ shaders → program → VAO"];
  loop [label="while (!shouldClose)\n  Context::Render()\n  swapBuffers", shape=box, style="rounded,filled", fillcolor="#fff7d6"];
  term [label="GLFW terminate", shape=ellipse];
  Start -> init -> ctx -> loop -> term;
}
\enddot

## 추가 페이지

- @ref build-system "빌드 시스템 가이드"
- @ref dependencies "의존 라이브러리"

## 핵심 모듈

| 모듈 | 클래스 | 역할 |
|------|--------|------|
| common | — | @c CLASS_PTR 매크로 + @ref SJH::LoadTextFile |
| diagnostics | @ref SJH::Diagnostics::GLObjectLog "GLObjectLog", @ref SJH::Diagnostics::GLDebug "GLDebug" | GL 에러 로깅 일원화 |
| shader | @ref SJH::Shader "Shader" | 셰이더 컴파일 RAII |
| program | @ref SJH::Program "Program" | 프로그램 링크 RAII |
| context | @ref SJH::Context "Context" | 씬 자원 + 매 프레임 draw |
````

### Task 6.4: 빌드 시스템 페이지 작성

**Files:**
- Create: `doc/pages/10-build-system.md`

- [ ] **Step 6.4.1: 페이지 작성**

Create: `doc/pages/10-build-system.md` with content:

````markdown
# 빌드 시스템 {#build-system}

> 본 페이지는 [.claude/build-system.md](../../.claude/build-system.md) 의 핵심을 발췌. 상세 트러블슈팅은 원본 참조.

## 한 줄 요약

C++17, CMake 3.21+, vcpkg manifest mode, 크로스 플랫폼(macOS arm64 / Windows x64).

## include() 순서 (루트 `CMakeLists.txt`)

\dot
digraph IncludeOrder {
  rankdir=TB;
  node [shape=box, fontname="Helvetica"];
  cxx [label="cmake/CXXStandard.cmake\n(C++17, compile_commands)"];
  doxy [label="cmake/Doxygen.cmake\n(sjhopengl_setup_doxygen)"];
  dep [label="cmake/Dependency.cmake\n(find_package vcpkg)"];
  cfg [label="cmake/Config.cmake\n(WINDOW_NAME/WIDTH/HEIGHT)"];
  src [label="add_subdirectory(src)\n(SJH:: aliases)"];
  app [label="add_subdirectory(app)\n(configure_file → config.h)"];
  test [label="add_subdirectory(test)\n(Catch2)"];
  cxx -> doxy -> dep -> cfg -> src -> app -> test;
}
\enddot

`Config.cmake` 가 `add_subdirectory(app)` 보다 먼저 include 되어야 함 — `configure_file(config.h.in config.h)` 가 `WINDOW_*` 변수에 의존.

## 빌드 명령

```bash
# macOS 첫 셋업 / 의존성 변경 시
cmake --preset debug && cmake --build build_Darwin

# 코드만 수정 (vcpkg 실행 없음)
cmake --build build_Darwin

# vcpkg 스킵 빠른 재구성
cmake --preset debug -DVCPKG_MANIFEST_INSTALL=OFF

# 의존성만 명시적 설치
cmake --build build_Darwin --target install-deps

# 본 문서 빌드
cmake --build build_Darwin --target doxygen
```

## 프리셋 매트릭스

| Preset | OS | Generator | binaryDir |
|--------|----|-----------|-----------|
| `debug` | macOS | Unix Makefiles | `build_Darwin` |
| `release` | macOS | Unix Makefiles | `build_Darwin` |
| `debug-windows` | Windows | VS 17 2022 (x64) | `build_Windows` |
| `release-windows` | Windows | VS 17 2022 (x64) | `build_Windows` |

## config.h 생성 흐름

```
cmake/Config.cmake (set WINDOW_*)
  ↓ include()
app/CMakeLists.txt: configure_file(config.h.in config.h)
  ↓
build_Darwin/app/config.h
  ↓ #include "config.h"
app/main.cpp
```

## 빌드 옵션

| 옵션 | 기본값 | 설명 |
|------|--------|------|
| `SJH_OPENGL_BUILD_TESTS` | `ON` | Catch2 단위 테스트 빌드 |
| `SJH_OPENGL_BUILD_DOCS`  | `ON` | `doxygen` 타겟 등록 |
````

### Task 6.5: 의존성 페이지 작성

**Files:**
- Create: `doc/pages/20-dependencies.md`

- [ ] **Step 6.5.1: 페이지 작성**

Create: `doc/pages/20-dependencies.md` with content:

````markdown
# 의존 라이브러리 {#dependencies}

본 프로젝트는 vcpkg manifest mode([vcpkg.json](../../vcpkg.json))로 의존성을 관리한다.
`cmake --preset <name>` 시점에 자동 설치되며, `cmake/Dependency.cmake` 가 `find_package` 호출.

## 패키지 표

| 패키지 | vcpkg 명 | CMake 타겟 | 사용처 |
|--------|----------|-----------|--------|
| **fmt** | `fmt` | `fmt::fmt` | 포매팅 (spdlog 백엔드 + 직접 사용) |
| **spdlog** | `spdlog` | `spdlog::spdlog` | 모든 모듈의 로깅 백엔드 |
| **GLFW** | `glfw3` | `glfw` (네임스페이스 없음) | 윈도우/입력/GL 컨텍스트 — `app/main.cpp` |
| **glad** | `glad` | `glad::glad` | OpenGL 함수 로더 — 모든 GL 모듈 |
| **stb** | `stb` | `${Stb_INCLUDE_DIR}` (헤더 only) | 이미지 로딩 (텍스처) — `app/` |
| **Catch2** | `catch2` | `Catch2::Catch2WithMain` | 단위 테스트 (`test/`) |

## 의존성 그래프 (모듈 → 외부)

\dot
digraph DepGraph {
  rankdir=LR;
  node [shape=box, fontname="Helvetica"];

  subgraph cluster_internal {
    label="내부 모듈";
    style=dashed;
    common; diagnostics; shader; program; context;
  }

  subgraph cluster_external {
    label="외부 (vcpkg)";
    style=filled; fillcolor="#f0f0f0";
    fmt; spdlog; glfw; glad; stb; catch2;
  }

  common -> spdlog;
  diagnostics -> spdlog;
  diagnostics -> fmt;
  diagnostics -> glad;
  shader -> glad;
  shader -> common;
  shader -> diagnostics;
  program -> glad;
  program -> shader;
  program -> diagnostics;
  context -> program;
  context -> shader;
  context -> diagnostics;
  context -> glad;
}
\enddot

## 명시적 의존성 설치 타겟

```bash
cmake --build build_Darwin --target install-deps
```

내부적으로 `$ENV{VCPKG_ROOT}/vcpkg install --triplet ${VCPKG_TARGET_TRIPLET}` 호출.
`vcpkg.json` 변경 후 재구성 없이 의존성만 갱신할 때 사용.

## 트러블슈팅

| 증상 | 원인 | 해결 |
|------|------|------|
| `ld: library 'fmt' not found` | `fmt::fmt` 가 아닌 `fmt` 로 링크 | `target_link_libraries(... fmt::fmt)` |
| `ninja required v1.13.2` | 구버전 ninja | `brew install ninja` |
| glad "OpenGL header already included" | glfw3.h가 glad보다 먼저 include | `glad/glad.h` 를 가장 위에 |

자세한 내용은 [.claude/build-system.md](../../.claude/build-system.md) §의존성 트러블슈팅.
````

### Task 6.6: `.gitignore` 갱신 (Doxygen 출력 제외)

**Files:**
- Modify or create: `.gitignore`

- [ ] **Step 6.6.1: `doc/html/` 무시 규칙 추가**

Run: `grep -q "^doc/html" .gitignore 2>/dev/null && echo "already" || echo "doc/html/" >> .gitignore`
Expected: 이미 있으면 `already`, 없으면 추가.

또한 `doc/latex/` 도 함께:

Run: `grep -q "^doc/latex" .gitignore 2>/dev/null && echo "already" || echo "doc/latex/" >> .gitignore`

### Task 6.7: 최종 빌드 검증

- [ ] **Step 6.7.1: 깨끗한 Doxygen 빌드**

Run: `rm -rf doc/html && cmake --build build_Darwin --target doxygen 2>&1 | tee /tmp/doxy.log | tail -20`
Expected: 경고 0 또는 minimal. 무경고가 이상적이지만 외부 헤더 관련 경고는 허용.

Run: `ls doc/html/ | head -10`
Expected: `index.html`, `classSJH_1_1Shader.html`, `classSJH_1_1Program.html`, `classSJH_1_1Context.html`, `mainpage.html` 등 존재.

- [ ] **Step 6.7.2: 다이어그램 SVG 생성 확인**

Run: `find doc/html -name '*.svg' | wc -l`
Expected: 5 이상 (클래스 다이어그램 + collaboration + include + 수동 \dot 블록).

- [ ] **Step 6.7.3: 메인 페이지 열기**

Run: `open doc/html/index.html`
확인:
- 메인 페이지에 "모듈 레이어 다이어그램" SVG 가 보이는가?
- "렌더링 시퀀스" 다이어그램이 보이는가?
- 사이드바에 `build-system` / `dependencies` 페이지 링크가 있는가?
- 각 클래스 페이지의 collaboration graph 가 SVG로 렌더되는가?

### Task 6.8: Phase 6 커밋

- [ ] **Step 6.8.1: 커밋**

```bash
git add cmake/Doxygen.cmake doc/Doxyfile.in doc/pages/ .gitignore
git commit -m "$(cat <<'EOF'
[doc] : Phase 6 — diagrams + integrated pages

- Doxyfile: HAVE_DOT/SVG/collaboration graph 활성화
- doc/pages/ 신설: mainpage(레이어/시퀀스 \dot) + build-system + dependencies
- cmake/Doxygen.cmake: DOXYGEN_PAGES_DIR 변수 추가
- .gitignore: doc/html, doc/latex 제외
EOF
)"
```

### 🔒 REVIEW GATE 6 — 전체 완료

사용자에게 보여줄 것:
- `doc/html/index.html` 메인 페이지 (다이어그램 포함)
- `doc/html/classSJH_1_1Context.html` (collaboration graph)
- `doc/html/build-system.html` 빌드 시스템 가이드
- 베이스라인 대비 Doxygen 경고 변화량

---

## Self-Review Checklist (계획 작성자용)

- [x] **Spec coverage**: 스펙의 §1~9 모두 task 로 매핑됨 (§9 범위 외는 의도적 미포함).
- [x] **Placeholder scan**: TBD/TODO 없음. 모든 코드 블록은 실제 적용 가능한 완전한 형태.
- [x] **Type consistency**: `ShaderUPtr`/`ShaderPtr`/`ProgramUPtr`/`ContextUPtr` 등 매크로 산출 별칭 일관 사용.
- [x] **Phase 검토 게이트**: 각 Phase 끝에 `🔒 REVIEW GATE` 명시.
- [x] **Doxyfile 변경 항목**: 스펙 §6 표의 모든 키가 Task 6.2 에 포함됨.
- [x] **다이어그램 도구**: A(자동 graphviz) + B(수동 \dot 블록 — PlantUML 대신 graphviz 통일로 의존성 단순화) 병행 채택.

---

## Phase 7: `object/camera` + `context` 입력 위임 메서드

**의존성:** Phase 4 (context 모듈 본체 Doxygen) 완료 + glm 의존성 도입 + 카메라 모듈 구현 완료 전제.

**배경:** Phase 4 작성 시점의 `context.h` 는 `Render` / `Init` 두 메서드만 가진 단순 형태. 이후 다음이 추가됨 (커밋 `3696136` 카메라 리팩토링까지 반영):
- `src/object/camera.h` — POD-like Camera 상태. m-prefix 통일: `mPos` / `mTarget` / `mCamUp`, 회전 `mEulerYaw` / `mEulerPitch`, 입력 플래그 `mIsCamControl`, 투영 `mFov` / `mAspect` / `mNearPlane` / `mFarPlane`.
- Camera 메서드: `GetFront()` (Yaw→Pitch 회전식), `GetForwardViewMatrix()` / `GetLookAtViewMatrix()` (모드 분리), `GetProjMatrix()` (인자 없음 — `mAspect` 멤버 사용), `SetAspect(w,h)` (height==0 가드).
- `Context::ProcessInput` (W/A/S/D/Q/E 키 → 카메라 위치 이동, 매 프레임 `GetFront()` 1회 캐싱).
- `Context::Reshape` (framebuffer 콜백 → `glViewport` + width/height 갱신 + `mCamera.SetAspect`).
- `Context::MouseMove` (cursor 콜백 → `mEulerYaw/Pitch` 갱신, clamp/wrap. front 직접 갱신 X).
- `Context::MouseButton` (좌클릭 → `mCamera.mIsCamControl` 토글, `mPrevMousePos` 초기화).
- 추가 멤버: `mCamera`, `mWidth`, `mHeight`, `mPrevMousePos`, `mVertexArrayObject`, `mVertexBufferObject`, `mElementBufferObject`, `mRM`.
- `app/main.cpp` — `glfwSetWindowUserPointer(window, ctx.get())` 로 Context 주입, `Handle*` 콜백이 `glfwGetWindowUserPointer` 캐스팅으로 역참조 후 `Context::*` 위임.

### Task 7.1: `src/object/camera.h` 에 Doxygen 주석 추가

**Files:**
- Modify: `src/object/camera.h` (전체 교체)

- [x] **Step 7.1.1: 클래스 docstring + 멤버별 1줄 주석 + 메서드 brief/return/note 추가**

핵심 사항:
- 클래스 docstring 의 *책임 / 비-책임 / 좌표계 가정 / 명명 컨벤션* 4블록 — `gl_log.h` 톤과 일치.
- `mIsCamControl` / `mEulerPitch` / `mEulerYaw` 등 입력으로 변경되는 필드는 *누가 갱신하는지* (`Context::*`) 를 명시.
- `mAspect` 는 `SetAspect(w,h)` 가 갱신, `GetProjMatrix()` 가 멤버 직접 사용 (인자 없음).
- `GetFront()` 의 회전 순서 (Yaw 먼저 → Pitch 적용) 를 *FPS 카메라 표준* 으로 명시.
- `GetForwardViewMatrix()` 와 `GetLookAtViewMatrix()` 는 *동시 사용 금지* — 모드 추적 안 함이 호출자 책임임을 표기 (모드 enum 도입 예정).
- `SetAspect` 의 `height==0` 가드 (윈도우 최소화 등) 동작을 docstring 에 명시.

### Task 7.2: `src/context/context.h` 에 신규 메서드/멤버 Doxygen 추가

**Files:**
- Modify: `src/context/context.h` (전체 교체)

- [x] **Step 7.2.1: 클래스 docstring 의 책임 표 갱신**

추가 책임 항목:
- 카메라 상태 보관 + 입력 → 카메라 갱신 위임 (@ref Camera).
- (비-책임) 키/마우스 *콜백 등록*은 `app/main.cpp` 가 GLFW 에 등록 → @ref ProcessInput / @ref MouseMove / @ref MouseButton 으로 위임.
- (비-책임) framebuffer 리사이즈 콜백 → @ref Reshape 위임.

- [x] **Step 7.2.2: 입력 위임 메서드 4개 docstring**

| 메서드 | 핵심 docstring 항목 |
|--------|--------------------|
| `ProcessInput(window)` | 키 매핑 표 (W/S/A/D/Q/E) + `mCamera.mIsCamControl` true 시에만 동작. 매 프레임 `GetFront()` 1회 캐싱. |
| `Reshape(width, height)` | framebuffer_size_callback 위임 진입점. `glViewport` 호출 + `mCamera.SetAspect` 호출 명시. |
| `MouseMove(x, y)` | `mEulerYaw/Pitch` 갱신 공식 + pitch `[-89, 89]` 클램프 + yaw `[0, 360)` wrap. front 직접 갱신은 제거됨 (`GetFront()` 가 매번 재계산). |
| `MouseButton(button, action, x, y)` | 좌클릭 PRESS/RELEASE 의 `mCamera.mIsCamControl` 토글 + `mPrevMousePos` 초기화. |

각 메서드 docstring 끝에 `@note GLFW <콜백명> 에서 위임받는 진입점.` 추가.

- [x] **Step 7.2.3: private 멤버별 1줄 주석**

`mProgram`, `mVertexArrayObject`, `mVertexBufferObject`, `mElementBufferObject`, `mRM`, `mCamera`, `mWidth`, `mHeight`, `mPrevMousePos` 각각 *역할 + 누가 갱신하는지* 1줄.

### Task 7.3: 빌드 검증

- [x] **Step 7.3.1: 빌드 통과 확인**

Run: `cmake --build build_Darwin --target sjhopengl_context 2>&1 | tail -10`
Expected: `Built target sjhopengl_context` 출력 (에러 없음).

- [ ] **Step 7.3.2: Doxygen 빌드 + 결과 확인**

Run: `cmake --build build_Darwin --target doxygen 2>&1 | grep -E "warning|error" | wc -l`
Expected: Phase 6 베이스라인 대비 증가하지 않음 (신규 멤버 누락 경고 0).

Run: `open doc/html/classSJH_1_1Camera.html`
확인:
- "책임 / 비-책임 / 좌표계 가정" 3블록이 보이는가?
- 멤버 변수들의 단위(degree 등) 가 명시되는가?

Run: `open doc/html/classSJH_1_1Context.html`
확인:
- 입력 위임 메서드 4개 (ProcessInput / Reshape / MouseMove / MouseButton) 가 모두 표시되는가?
- 각 메서드의 `@note GLFW ... 콜백에서 위임받는 진입점` 이 표시되는가?
- collaboration graph 에 `Camera` 가 멤버로 보이는가?

### Task 7.4: Phase 7 커밋

- [ ] **Step 7.4.1: 커밋**

```bash
git add src/object/camera.h src/context/context.h doc/doxygen-documentation-plan.md doc/doxygen-documentation-implementation-plan.md
git commit -m "$(cat <<'EOF'
[doc] : Phase 7 — object/camera + context 입력 위임 doxygen

- src/object/camera.h: Camera 책임/비-책임/좌표계 가정 + 멤버 docstring
- src/context/context.h: ProcessInput/Reshape/MouseMove/MouseButton 위임 메서드 + 추가 멤버 docstring
- doc/doxygen-documentation-{plan,implementation-plan}.md: Phase 7 추가, Phase 1~6 완료 표시
EOF
)"
```

### 🔒 REVIEW GATE 7 — Phase 7 완료

사용자에게 보여줄 것:
- `classSJH_1_1Camera.html` 클래스 페이지 (책임/비-책임 블록).
- `classSJH_1_1Context.html` 의 입력 위임 메서드 섹션.
- Doxygen 경고 변화량 (Phase 6 → Phase 7).
