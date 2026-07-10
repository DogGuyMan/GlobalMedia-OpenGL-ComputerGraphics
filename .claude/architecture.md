# Architecture & Module Design

## 1. 설계 철학

- **모듈 단위 STATIC 라이브러리** 구조 — 각 `src/<module>/`은 자체 `CMakeLists.txt`로 의존성을 *명시적으로* 선언한다.
- **`SJH::<module>` alias 컨벤션** — 외부 IMPORTED 타겟(`sb7`, `glfw3`, `box2d`, `spdlog` 등 `cmake/Dependency.cmake` 에서 등록한 사전 빌드 라이브러리)과 시각적으로 구분.
- **헤더에 노출되는 타입은 PUBLIC link, .cpp 내부에서만 쓰는 의존은 PRIVATE link** — CMake 레벨에서 의도된 캡슐화 강제.
- **vcpkg manifest 모드** (2026-06-20 전환, 외부 `$env{VCPKG_ROOT}` — 서브모듈 아님) — 루트 [vcpkg.json](../vcpkg.json) 이 `box2d/glm/spdlog/assimp/tweeny/stb/catch2/nlohmann-json/imgui/opencv4` 를 `find_package` 로 조달([cmake/Dependency.cmake](../cmake/Dependency.cmake)). 잔류 사전 빌드(`lib/{macos,windows}/` + `include/` 체크인)는 `glfw3/sb7/Effekseer/FMOD` 뿐 — imgui 도 코어는 vcpkg, GLFW 백엔드만 `apps/_MyApp_/third_party/imgui` vendoring 유지. Catch2 는 과거 `extern/Catch2` git 서브모듈(v3.15.0) 이었으나 현재 vcpkg `find_package(Catch2 3 CONFIG REQUIRED)` 로 전이 완료 — 서브모듈 폐기. 시스템 `find_package` 는 `OpenGL` 하나뿐(macOS 프레임워크 우선순위 가드 있음, `cmake/Dependency.cmake` 참조), 나머지 외부 의존은 vcpkg toolchain 경유.
- **크로스 플랫폼**: macOS arm64 (GL 4.1 Core / GLSL 410) + Windows x64 (GL 4.1+).
- **YAGNI 우선** — 추상화는 *반복 패턴*이 두 군데 이상 등장한 뒤에 만든다 (예: `diagnostics`는 셰이더 + 프로그램 InfoLog 패턴이 거의 동일해서 정당화됨).
- **명시적 의존성** — 모듈은 transitive 전파에 기대지 않고 자기가 진짜 쓰는 의존을 직접 선언한다. 명시 비용은 일회성/선형, 누락 비용은 누적/영구. 자세한 근거 §4.
- **생성 산출물 분리** — `configure_file` 산출물은 손으로 쓴 public 헤더(`include/`)에 섞지 않고, 단일 소비자 타겟 디렉토리(예: `app/`)에 둔다. 자세한 §8.

## 2. 모듈 패턴 (canonical template)

각 `src/<module>/CMakeLists.txt`의 표준형:

```cmake
add_library(sjhopengl_<module> STATIC <module>.cpp)
add_library(SJH::<module> ALIAS sjhopengl_<module>)

target_include_directories(sjhopengl_<module>
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>   # = src/
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# 의존성: 헤더 노출 시 PUBLIC, .cpp 내부 시 PRIVATE
target_link_libraries(sjhopengl_<module>
    PUBLIC  <헤더에 등장하는 의존>
    PRIVATE <cpp 에서만 쓰는 의존>
)

target_compile_features(sjhopengl_<module> PRIVATE cxx_std_17)
```

### 네이밍 규칙
- 타겟명: `sjhopengl_<module>` (소문자 snake_case, 충돌 방지용 prefix)
- alias: `SJH::<module>` (소비 측에서 사용)
- 라이브러리 파일: `libsjhopengl_<module>.a` (자동)

### Include 전략
- **PUBLIC `${CMAKE_CURRENT_SOURCE_DIR}/..`** = `src/` 노출 → 소비자는 `#include "<module>/<file>.h"` 형식으로 작성
- **PRIVATE `${CMAKE_CURRENT_SOURCE_DIR}`** = 모듈 내부에서 unprefixed include 가능 (`#include "shader.h"`)

### Include 종류별 규약

| 형식 | 의미 | 예 |
|---|---|---|
| `#include <<vendor>/header.h>` | 시스템 / 사전 빌드 외부 라이브러리 | `<GL/gl3w.h>`, `<GLFW/glfw3.h>`, `<sb7.h>`, `<<vendor>/spdlog.h>` |
| `#include "<module>/file.h"` | 프로젝트 내부 모듈 (PUBLIC `src/` 경로 활용) | `"common/common.h"`, `"shader/shader.h"`, `"diagnostics/gl_log.h"` |
| `#include "file.h"` | **같은 모듈 내부** (PRIVATE include path 활용) | `src/shader/` 내부에서 `"shader.h"` |

### GL 헤더 출처와 순서

- **`<GL/gl3w.h>`** — `GLenum`, `GLuint`, `GLint`, `GLfloat`, `glXxx*()` 함수 포인터. **GL 타입과 함수의 진짜 출처** (sb7 가 gl3w 를 끌어옴).
- **`<sb7.h>`** — `sb7::application` 베이스 클래스. 내부에서 gl3w + GLFW 초기화. 이 헤더가 GL 헤더를 들고 오므로 챕터 코드의 1번째 include 로 두는 게 안전.
- **`<GLFW/glfw3.h>`** — 헤드리스 테스트(test/support/gl_test_fixture)나 챕터 자체 init 에서 직접 다룰 때만. **gl3w 가 먼저 포함되어야** 함 (그렇지 않으면 GLFW 가 자체 GL 헤더를 끌어와 심볼 충돌).
- **모듈 차원의 함의**: `GLenum`/`GLuint` 만 쓰는 모듈(`SJH::shader`, `SJH::buffer` 등)은 `project_deps` (sb7 + glfw3 + OpenGL) 를 link 하면 GL 타입을 얻는다.

## 3. 클래스 디자인 컨벤션

> 🔵 *범용 설계 원칙* (변동성≠다형성 / 다형성 3조건 / composition>inheritance / 소유권 단일소유+raw참조 / 에러 철학)은 전역 Skill **`design-decision-discipline`** 에 있다. 아래는 *이 프로젝트의 구체 적용 사례* (GL 자원 수명 등). 명명 규칙은 **`personal-naming-conventions`** Skill.

### 네임스페이스
- 모든 모듈 코드는 `namespace SJH` 안에.
- 하위 도메인이 있으면 `namespace SJH::diagnostics` 같이 중첩 (C++17 nested namespace 문법).

### `CLASS_PTR` 매크로 (from [src/common/common.h](../src/common/common.h))
```cpp
CLASS_PTR(Shader)
// 자동 생성:
// using ShaderUPtr = std::unique_ptr<Shader>;
// using ShaderPtr  = std::shared_ptr<Shader>;
// using ShaderWPtr = std::weak_ptr<Shader>;
```
모든 GL 리소스 wrapper 클래스는 이 매크로로 스마트 포인터 별칭을 미리 정의.

### 팩토리 패턴 (자원 소유 객체)
```cpp
class Shader {
public:
    static ShaderUPtr CreateFromFile(const std::string& filename, GLenum type);
    ~Shader();
private:
    Shader() = default;                              // 생성자 private
    bool TryLoadFile(const std::string& f, GLenum); // 내부 fallible init
    GLuint mShader{0};
};
```
**핵심 원칙**:
1. 생성자 `private`/`= default` → 직접 인스턴스화 차단.
2. `static CreateXxx(...)` → 실패 시 `nullptr` 반환 (예외 X, std::optional X).
3. 내부 `Try*` 메서드 → bool 반환, 자원 획득 실패 시 false.
4. **소유권은 `UPtr`로만 이동** (`std::move(temp)`).

> **관찰된 예외 사례**: [src/buffer/buffer.cpp](../src/buffer/buffer.cpp) 의 내부 fallible 메서드는 `Init()` (Try* prefix 미적용). 컨벤션상 `TryInit()` 권장이지만, 본 프로젝트에선 *각 모듈의 자율*에 맡김 — buffer 가 그 사례. 강제 규칙 X, 권장에 가까움.

#### 왜 *팩토리*이지, 2-phase Init이 아닌가

C++엔 비슷해 보이는 관용구 **2-phase Init**(빈 생성자 + public `Init()`)이 있는데, 이 프로젝트의 팩토리 패턴은 *다른 문제*를 푼다.

| 패턴 | 핵심 동기 | 이 프로젝트 적합성 |
|---|---|---|
| **2-phase Init** | 생성자 안에서 가상 함수 호출 시 vtable이 base에 묶여 다형 dispatch 실패 → `Init()`을 따로 빼서 객체 완성 후 호출 | ❌ GL wrapper들은 `virtual` / 상속 hierarchy가 없음 |
| **팩토리 (Named Constructor)** | 예외 없는 실패 신호 + RAII 소유권 강제 + 클래스 불변식 보장 | ✅ `Try*` private + 생성자 private + `static Create() → UPtr` 시그니처가 정확히 이 의도 |

식별 단서 — `Init` 메서드 가시성:

| 가시성 | 의미하는 패턴 |
|---|---|
| `public Init()` | 2-phase Init — 자식/외부 호출 가능해야 함 |
| **`private TryXxx()`** | **이 프로젝트** — 외부에서 인스턴스 만드는 경로가 팩토리뿐 |

**다형성 부재는 의도된 결정**. `Shader`/`Program`/`Buffer` 등은 **leaf 타입** — virtual 0개, 상속 없음. 학습 프로젝트 규모에서 다형성 도입은 §1 YAGNI 원칙 위배.

- **재검토 트리거**: 동일 인터페이스로 다른 백엔드(Vulkan/Metal/소프트웨어 폴백 등)를 추상화해야 하는 시점. 그때 비로소 2-phase Init이 후보.
- **함의**: "왜 virtual이 없지? Init은 왜 private이지?"라는 의문은 같은 답 — **다형성을 의도적으로 도입 안 했음**. 팩토리 채택 동기는 *오로지* 자원 획득 실패의 안전한 표현 + 강제 RAII이지, 가상 dispatch 회피가 아니다.

### 반환 타입 정책
| 상황 | 반환 |
|---|---|
| 파일/리소스 로드 (실패 가능) | `std::optional<T>` |
| 자원 객체 팩토리 | `<Type>UPtr` (실패 시 `nullptr`) |
| 검증 / boolean status | `bool` (실패 사유는 spdlog 로그) |

### GL 자원 수명 정책 (셰이더 사례)

OpenGL의 `glDeleteShader`는 **즉시 파괴가 아니라 *해제 의사 표명*** 이다:
- attach되지 않은 셰이더 → 즉시 파괴.
- program에 attach된 상태 → "delete pending" mark만 남기고, 마지막 detach 또는 program 파괴 시점에 실제 해제.

이 시멘틱 때문에 두 코딩 패턴이 *기능적으로 동등*:

| 패턴 | `glDeleteShader` 호출 시점 | 비고 |
|---|---|---|
| **RAII** (이 프로젝트) | `~Shader()` — UPtr/Ptr 스코프 종료 시 | C++ 측에서 GL 핸들 소유권 추적 |
| 즉시 정리 (sb7 스타일) | `glLinkProgram` 직후 명시 호출 | 셰이더 객체를 외부에 노출 안 함 |

#### 이 프로젝트의 결정: RAII 유지

- `Program::Create(std::vector<ShaderPtr>)` API가 *셰이더 재사용*을 전제 — 동일 vertex shader를 여러 program에 attach 가능해야 함.
- 다른 GL 자원 wrapper(`Buffer`, `Texture`, `VertexLayout`)도 동일한 RAII 패턴 → 컨벤션 일관성.
- 학습 프로젝트 규모에서 attach 잔존 메모리 비용은 무의미.

#### 함의 — 실제 파괴 시점 이해

[src/shader/shader.cpp:17-21](../src/shader/shader.cpp#L17-L21)의 소멸자가 호출되어도, 셰이더가 program에 attach되어 있다면 **GPU 자원은 즉시 사라지지 않는다**. 이는 버그가 아니라 OpenGL 시멘틱이다. 실제 해제는 다음 중 하나가 일어날 때:
- `glDetachShader(program, shader)` 호출
- 해당 `program`이 `glDeleteProgram`으로 파괴됨

이 사실을 모르면 "왜 셰이더가 ShaderPtr 파괴 후에도 GPU에 남아 있지?"라는 잘못된 디버깅을 시작할 수 있음.

#### 선택 옵션 — `Program::TryLink` 끝에 `glDetachShader`

링크 성공 직후 program 측 attach 참조를 해제하면, ShaderPtr 파괴 *즉시* GPU 해제가 발화:
```cpp
glLinkProgram(mProgram);
if (!diagnostics::GLObjectLog::CheckProgramLink(mProgram, tag)) return false;
for (const auto& s : shaders) glDetachShader(mProgram, s->Get());
return true;
```
**도입 시점**: 셰이더 hot-reload / 동적 교체가 필요해질 때. 현재 Phase 2 program 마이그레이션엔 **도입 안 함** — 단순성 우선, 후속 개선 후보로 보존 (⚠ 원 참조 `<doc>/migration-plan.md` 파일 부재 확인 — 실존하지 않음. 현재 결정 스토어는 [doc/adr/README.md](../doc/adr/README.md)).

### Try-Catch 부재
- 이 코드베이스는 **예외를 던지지 않는다**. OpenGL이 예외를 안 쓰고, GLFW도 콜백 + 에러 코드 기반.
- "에러 핸들링" = *상태 폴링 + 로그 + bool/optional 반환*. → 그래서 `diagnostics` 모듈명이 "error"가 아닌 *diagnostics*.

### 변동성 ≠ 다형성 — 시간 축으로 분리

> (`context` 모듈은 이후 폐기됐다 — 아래 `Context` 사례는 *역사적 교육 사례*로 보존, 활성 모듈 서술 아님.)

흔한 직관: **"`Context`처럼 내용이 자주 바뀌는 클래스는 상속 hierarchy로 다형 동작을 지원해야 한다."**
이 추론은 **변동의 *시간 축*을 구분 안 하면 잘못된 결론에 도달한다**.

| 변동 종류 | 의미 | 적합한 도구 |
|---|---|---|
| **runtime 변동** | 실행 중 객체가 *다른 구체 타입*으로 swap (메뉴 씬 → 게임 씬) | ✅ 다형성 (virtual + base interface) |
| **edit-time 변동** | 프로젝트 진행에 따라 코드 자체가 바뀜 (오늘 사각형, 내일 큐브) | ❌ 다형성 도움 X — 그냥 *코드를 다시 씀* |

`Context`의 "scene-specific 고변동"은 **edit-time 변동** — 다형성 적용 대상 아님.

#### 다형성 정당화의 3 조건 (전부 충족 필요)

1. **공통 추상 인터페이스가 의미 있음** — 모든 변형이 합리적으로 같은 시그니처 구현.
2. **호출자가 base 포인터/참조로 dispatch** — `current->Render()` 처럼 구체 타입 모른 채 호출.
3. **runtime swap 시나리오 실재** — 메뉴↔게임 전환, 백엔드 선택 등.

이 프로젝트의 `Context` 점검:

| 조건 | 충족 여부 |
|---|---|
| 공통 인터페이스 후보 | `Render()` 하나뿐 — 빈약 |
| 호출자가 base로 dispatch | ❌ — `<app>/main.cpp` 가 구체 타입 직접 사용 |
| runtime swap | ❌ — 한 번에 Context 하나, 교체 시나리오 없음 |

→ 셋 다 불충족 → 다형성 도입 시 **기능 +0, 복잡도 +1**.

#### 안티패턴 — "코드 재사용을 위한 상속"

`class TextureDemoContext : public BaseContext` 같은 hierarchy를 *공통 setup 공유*만 위해 만들면 GoF의 **"Favor Composition Over Inheritance"** 원칙 위반. 전형적 함정:
- BaseContext에 `protected: void SetupVAO()` 같은 helper가 누적
- 자식이 부모의 *부분*만 호출 (template method 변종)
- 부모가 *추상이 아니라 포괄*로 부풀어 오름 — 어느 순간 자식 전부의 합집합

**대안**: 공통 코드는 free function (`namespace SJH::demo_helper`) 또는 helper 객체를 멤버로 가져 (composition). 상속은 **"이 객체는 *진짜로* base의 일종이다"** (is-a)일 때만 정당.

#### 권장 — leaf 타입 + 의도하면 `final`

`Context`/`Shader`/`Program`/`Buffer` 등은 **leaf 타입** 유지. 상속 의도 없음을 명시하고 싶으면 `final`:

```cpp
class Context final {  // ← 우발적 상속 시도를 컴파일러가 차단
    ...
};
```

다형성 도입할 시점이 오면 `final` 떼고 base 추출하면 됨.

#### 미래 진화 후보 — *데이터 지향* 흡수 (상속 아님)

scene 별 변동을 *상속*이 아닌 *데이터*로 흡수하는 게 자연스러운 진화 경로:

```cpp
struct SceneConfig {
    std::vector<float>    vertices;
    std::vector<uint32_t> indices;
    std::string           vertexShaderPath;
    std::string           fragmentShaderPath;
    std::vector<std::string> texturePaths;
};
static ContextUPtr Create(const SceneConfig& cfg);
```

Context 클래스는 그대로 + scene 변동을 *데이터*가 흡수 → 상속 hierarchy 없이 한 Context 코드로 여러 scene 표현 가능. Phase 4 이후 *검토 사안*이지 지금 도입 X (⚠ 원 참조 `<doc>/migration-plan.md` 파일 부재 확인 — 실존하지 않음).

#### 다형성 도입 트리거 (정리)

| 트리거 | 정당화 사례 |
|---|---|
| ✅ 같은 그리기 로직, 다른 백엔드 추상화 | Vulkan/Metal/SW fallback |
| ✅ runtime scene swap | 메뉴 ↔ 게임 ↔ GameOver |
| ✅ Render Pass 파이프라인 | 그림자→메인→포스트, 동일 인터페이스 dispatch |
| ❌ 다음 챕터 데모 코드가 다름 | edit-time 변동 — 그냥 새 Context 작성 |

## 4. 의존성 링크 정책 (PUBLIC vs PRIVATE)

> 🔵 *범용 원칙* (명시적 의존 선언 / 비용 비대칭 / PUBLIC·PRIVATE 결정 트리 / self-contained 모듈 / 크로스플랫폼 가드)은 전역 Skill **`modular-build-discipline`** 에 있다. 아래는 *이 프로젝트의 적용 사례* (`SJH::` / `spdlog` PUBLIC 결정 등).

### 왜 명시적으로 선언하는가

모듈이 `add_subdirectory`로 부모(app)에 포함되어 있다고 해서 부모/형제의 link를 빌려 쓰면 안 되는 4가지 이유:

1. **의존성은 API 계약** — `shader.h`가 `<<glad>/glad.h>`를 include한다는 건 "shader를 쓰려면 glad도 봐야 함"이라는 외부 계약. PUBLIC link는 그 계약의 코드화이지 단순 빌드 편의가 아님.
2. **Transitive leak는 시한폭탄** — "어차피 app이 glad에 link하니까 shader는 안 해도 되겠지"는 모듈 재사용/test 분리/app 의존성 정리 시점에 폭발. **에러 메시지가 진짜 원인(shader)에서 멀리 떨어진 곳에서 발생** → 디버깅 비용 폭증.
3. **빌드 그래프 정확성** — 병렬 빌드의 race condition, 증분 빌드의 stale binary, compile_commands.json 기반 IntelliSense의 헤더 누락 — 모두 의존 그래프가 정확해야 동작.
4. **Self-contained 모듈** — 모듈을 잘라내서 다른 프로젝트에 옮겼을 때 그 모듈의 CMakeLists.txt **단독으로** 빌드되어야 한다. 부모를 가정하는 순간 모듈성 깨짐.

> 명시 비용 = `(모듈 수 × 의존 수)` × **한 번**.
> 누락 비용 = `(변경 횟수 × 디버깅 시간)` × **영구**.

### 결정 트리
```
헤더(.h)에 의존성의 타입/매크로가 등장? 
    YES → PUBLIC  (소비자가 헤더 include 시점에 해당 의존이 필요)
    NO  → PRIVATE (구현 디테일, 소비자는 알 필요 없음)
```

### 실제 적용 사례 ([src/diagnostics/CMakeLists.txt](../src/diagnostics/CMakeLists.txt))

> 표기 주의: 아래 `glad::glad`/`fmt::fmt` 는 결정 트리를 보여주기 위한 예시 표기 — 실제 이 레포에 그런 타겟은 없다(GL 로더는 gl3w/sb7 경유, `project_deps` INTERFACE 타겟으로 전파). 실제 wiring 은 `project_deps`/`game_deps` — §5 표기 주의 문단 참조.

```cmake
target_link_libraries(sjhopengl_diagnostics
    PUBLIC  glad::glad        # gl_log.h 에서 GLuint/GLenum 사용
            spdlog::spdlog    # PUBLIC (의도) — 비-GL 일반 로깅에도 자유롭게 쓰이도록 전파
    PRIVATE fmt::fmt          # spdlog 가 transitive 로 끌어옴, 직접 노출 X
)
```

### `spdlog::spdlog` PUBLIC 결정의 배경
초기 설계는 PRIVATE으로 spdlog를 *은폐*하려 했으나, 다음 이유로 PUBLIC으로 변경:
- `diagnostics` 모듈의 책임은 **GPU/OpenGL/Context 런타임 에러 패턴 중앙화**이지 일반 로깅 추상화가 아님.
- 비-GL 에러(파일 로드 실패, GLFW 초기화 실패 등)는 spdlog로 직접 출력하는 것이 자연스러움.
- spdlog 자체를 wrapping하면 컴파일 타임 포맷 체크/매크로 위치 정보(`SPDLOG_*`)를 잃음.
- **결론**: `diagnostics`는 GL 진단의 *전문화된 위*층, spdlog는 *횡단* 로깅 기반시설로 공존.

## 5. 모듈 인벤토리

> **정본 위임** — 모듈 인벤토리(역할 표)의 정본은 이제 [src/CLAUDE.md](../src/CLAUDE.md), 의존 그래프 정본은 [ARCHITECTURE.md](../ARCHITECTURE.md) 다. 현재(2026-07) `src/` 는 **18개** `SJH::<module>` STATIC 모듈 — [src/CMakeLists.txt](../src/CMakeLists.txt) 참조. `SJH::context` 는 이후 **폐기**됐다(Actor/Component 씬그래프 + `SceneRenderer` 로 대체 — `src/scene`/`src/render`). 아래 11모듈 표는 **2026-05 시점 스냅샷 (archival)** — PUBLIC/PRIVATE 의존 표기 사례가 여전히 교육적 가치가 있어 원문 그대로 보존하되, *활성 모듈 목록으로 읽지 말 것*.

> **타겟 이름 표기 주의** — 아래 표의 의존 칸은 ARCHITECTURE 의도(어느 의존을 PUBLIC/PRIVATE 로 link 할지)를 보여주는 표기이고, **실제 빌드 wiring 은** `cmake/Dependency.cmake` 의 `project_deps` (sb7 + glfw3 + OpenGL + 플랫폼 프레임워크) / `game_deps` (Box2D + Effekseer + assimp + spdlog + Tweeny + stb + imgui + nlohmann-json) INTERFACE 타겟이 묶어서 전파한다. `glad::glad` / `fmt::fmt` 같은 vcpkg 스타일 alias 는 본 프로젝트에 직접 정의돼 있지 않다 (역사적 표기 — 실제 GL 로더는 gl3w, sb7 이 끌어옴). test/ 도 현재 vcpkg `find_package(Catch2 3 CONFIG REQUIRED)` 로 조달한다(과거 호스트 설치 가정 서술은 stale) — `glad/fmt` 는 이 프로젝트 어디에도 실재하지 않으므로 test/ 가 그것을 가정한다는 서술도 stale.

| 모듈 | 핵심 클래스/파일 | 역할 | 핵심 의존 |
|---|---|---|---|
| `SJH::common` | `LoadTextFile`, `CLASS_PTR` ([common.cpp](../src/common/common.cpp)) | 파일 로드 + 스마트 포인터 별칭 매크로 | spdlog (PRIVATE) |
| `SJH::diagnostics` | `GLObjectLog`, `GLDebug` ([gl_log.cpp](../src/diagnostics/gl_log.cpp)), `UniformDiagnostics` ([uniform_diagnostics.cpp](../src/diagnostics/uniform_diagnostics.cpp)) | GL 진단 — 컴파일/링크/검증, 호출별 에러 체크, uniform warn-once. namespace `SJH::Diagnostics` (대문자 D) | glad, spdlog (둘 다 PUBLIC) |
| `SJH::shader` | `Shader` ([shader.cpp](../src/shader/shader.cpp)) | 셰이더 객체 wrapper (팩토리). `CreateFromFile` + `CreateFromSource` (인라인 GLSL) | common, glad (PUBLIC) + diagnostics (PRIVATE) |
| `SJH::program` | `Program` ([program.cpp](../src/program/program.cpp)) + `SJH::Uniforms` 자유 함수 family ([program_uniforms.cpp](../src/program/program_uniforms.cpp)) | 셰이더 프로그램 RAII + uniform 캐시/setter (자유 함수 — §11). uniform 캐시는 Program *외부* TU-local static | common, shader, glad (PUBLIC) + diagnostics, object (PRIVATE) |
| `SJH::buffer` | `Buffer` ([buffer.cpp](../src/buffer/buffer.cpp)) | VBO/EBO RAII 래퍼. `CreateWithData(target, usage, data, stride, count)` — 5-arg, stride/count 분리 | common, glad (PUBLIC) + diagnostics (PRIVATE) |
| `SJH::layout` | `VertexLayout` ([vertex_layout.cpp](../src/layout/vertex_layout.cpp)) | VAO RAII + `TrySetAttrib` (attribute layout). `GLDebug::CheckGL*` 통합 | common, glad (PUBLIC) + diagnostics (PRIVATE) |
| `SJH::resource_registry` | `Image`, `Texture`, `ResourceRegistry` (`<src>/resource_registry/image.cpp`, [texture.cpp](../src/resource_registry/texture.cpp), [resource_registry.cpp](../src/resource_registry/resource_registry.cpp)) | 이름 키 캐시 — Texture/Material/Model (3개 맵). CPU `Image` 캐시 없음 (스코프 한정). `Create*(key, …)` (키 충돌 시 warn+nullptr) / `Find*(key)` (순수 조회) 동사 분리. 팩토리 `ResourceRegistry::Create()`. 모든 `Create*`/`Find*` 는 `[[nodiscard]]`. | common, glad, glm, spdlog (PUBLIC) + diagnostics (PRIVATE), stb (헤더 PRIVATE) |
| `SJH::object` | `Mesh`, `Model`, `Material`, `Camera`, `Light` (DirLight/PointLight/SpotLight), `Transform`, `SceneNode`, `SceneGraph<TId>` ([object/](../src/object/)) | 씬 객체 — 메시/모델(assimp 로드, `UPtr` 벡터 소유)/카메라/광원. `Material` 이 `const Texture*` 비소유 관찰자 보유 (`mDiffuseTexture`/`mSpecularTexture`). `RenderUnit = {MeshUPtr mesh; Material* material;}` (소유 mesh / 관찰 material). `Material::Clone() → MaterialUPtr` 지원. `Model::mTextures`/`mMaterials` = `vector<TextureUPtr>`/`vector<MaterialUPtr>`. `Model::GetMesh` 반환 `Mesh*`. `Transform`=로컬 TRS 값 객체(`GetLocalMatrix`), `SceneNode`=intrusive 씬 그래프 노드(월드 행렬 캐싱 + dirty 하향 전파 + `Attach`/`Detach` 순환 가드 + `WorldForward`/`TranslateBy`), `SceneGraph<TId>`=enum 정수 인덱싱 노드 소유 컨테이너(헤더 온리 템플릿). `Camera`=투영 렌즈 전용(배치는 씬 노드 — view 행렬은 `inverse(World(Camera))`), `Light`=색상/감쇠/cutoff 만(위치/방향은 씬 노드 Transform 소유). 설계: [transform-scene-graph](../doc/design/2026-05-19-transform-scene-graph-design.md) · [camera-light-scene-graph](../doc/design/2026-05-19-camera-light-scene-graph-design.md). | common, glad, glm, assimp (PUBLIC) + resource_registry |
| `SJH::input` | `KeyboardInput<TAction>`, `MouseInput` ([mouse_input.cpp](../src/input/mouse_input.cpp), [keyboard_input.h](../src/input/keyboard_input.h)) | 입력 디스패치 — SSU inputs 패턴 + 엔진 모범의 논리 액션 계층. `KeyboardInput<TAction>` 은 키→논리 액션→핸들러 2단(액션 enum 은 소비자 소유 — 입력 모듈은 어휘 비소유, 헤더 온리 템플릿). `MouseInput` 은 드래그→(dx,dy) 콜백. 설계: [<doc>/design/2026-05-19-input-module-design.md](../doc/design/2026-05-19-input-module-design.md). | glfw (PUBLIC) |
| `SJH::context` *(⚠ 2026-06 이후 폐기 — archival 표기, 아래 사실은 더 이상 유효하지 않음)* | `Context` (구 `<src>/context/context.cpp`, 디렉토리째 삭제됨) | 한 씬의 GL 자원 + 매 프레임 draw call. 카메라/광원/머티리얼(`MaterialUPtr`)/모델 보유. 입력은 `KeyboardInput<GameAction>`/`MouseInput` 멤버에 위임 | common, shader, program, buffer, layout, object, resource_registry, input (PUBLIC) + diagnostics (PRIVATE) |

> **`STB_IMAGE_IMPLEMENTATION` 규칙** — 단일 헤더 라이브러리의 구현 매크로는 *정확히 한 .cpp 에서만* 정의.
> 현재 [src/texture/image.cpp](../src/texture/image.cpp) 가 유일한 정의 지점(2026-06-11 `texture` 모듈 하위추출로 `resource_registry` 에서 이주 — 위 표의 `<src>/resource_registry/image.cpp` 경로는 archival). 헤더([image.h](../src/texture/image.h))에 두면
> 그 헤더를 include 하는 *모든 TU* 가 stb 함수 본문을 중복 생성 → linker `duplicate symbols`. (세션 중 실제 발생한 회귀)

## 6. `diagnostics` 모듈 — 상세 설계

### 동기 (Why)
OpenGL은 **컴파일 타임보다 런타임 에러가 압도적으로 많고**, 호출별 에러 검증 코드가 모듈 곳곳에 흩어지면 다음 문제가 발생:
- 셰이더/프로그램 InfoLog 검증 보일러플레이트가 N개 모듈에 중복.
- 로그 포맷 일관성 깨짐 (모듈마다 메시지 스타일 다름).
- spdlog 헤더(`<<vendor>/spdlog.h>` + transitive fmt)가 N개 TU에 포함 → 컴파일 시간 N배.

→ **GL 진단을 한 모듈에 가두고**, 소비자는 의도-드러내는 함수 한 줄로 호출.

### 책임 범위 (명시적 경계)
- ✅ 셰이더/프로그램 상태 쿼리 (`GL_COMPILE_STATUS`/`GL_LINK_STATUS`/`GL_VALIDATE_STATUS`)
- ✅ `glGetError()` 폴링 + 위치 정보 출력
- ✅ `glDebugMessageCallback` 등록 (KHR_debug 가능 시)
- ❌ 일반 애플리케이션 로깅 (파일 IO 실패, GLFW 초기화 등) — 이건 spdlog 직접 사용
- ❌ 예외 처리 — 이 코드베이스는 예외 미사용

### 3-Layer 에러 커버리지
```
┌─────────────────────────────────────────────────────────────┐
│ Layer 1: GLDebug::Init() — KHR_debug 자동 콜백              │
│   가능 환경에서 *모든* GL 에러를 자동 포착                  │
│   호출: <app>/main.cpp 또는 src/render/ 에서 1회 (⚠src/context 폐기) │
├─────────────────────────────────────────────────────────────┤
│ Layer 2: SJH_GL_CHECK(x) — glGetError 폴링                  │
│   콜백 미지원 환경(macOS GL 4.1)의 안전망                   │
│   NDEBUG 빌드에선 no-op (성능 비용 0)                       │
├─────────────────────────────────────────────────────────────┤
│ Layer 3: GLObjectLog::Check* — 객체 상태 검증              │
│   비동기-아닌 상태(컴파일/링크/검증)는 별도 쿼리 필수       │
│   콜백/폴링으로 못 잡는 영역                               │
└─────────────────────────────────────────────────────────────┘
```

각 레이어는 **서로 보완적**이며 단독으로 충분하지 않다.

### Public API 요약

#### `GLObjectLog` — 객체 상태 검증 (Layer 3)
| 함수 | 호출 시점 | 호출 위치 |
|---|---|---|
| `CheckShaderCompile` | `glCompileShader` 직후 | `src/shader/` |
| `CheckProgramLink` | `glLinkProgram` 직후 | `src/program/` |
| `CheckProgramValidate` | draw 직전 (디버그 빌드만) | `src/program/` 또는 renderer |
| `CheckExpectedUniforms` | program link 후, 기대 uniform 존재 검증 (1회 캐시) | program 사용처 (⚠ 과거 `src/context/` 서술은 stale — 그 모듈 폐기) |
| `CheckExpectedAttributes` | 동일 — vertex attribute 존재 검증 | `src/layout/` 또는 program 사용처 |
| `InvalidateProgramCache` | `Program` 소멸자 — stale 캐시 방지 | `src/program/` |

#### `GLDebug` — 호출별 명시 에러 검사 (Layer 2 — 풀어쓴 형태)
| 함수 | 호출 시점 | 가능 에러 |
|---|---|---|
| `CheckGLGenVertexArrays` | `glGenVertexArrays` 직후 | `GL_INVALID_VALUE` |
| `CheckGLBindVertexArray(vao)` | `glBindVertexArray` 직후 | `GL_INVALID_OPERATION` |
| `CheckGLGenBuffers(buf)` | `glGenBuffers` 직후 | `GL_INVALID_VALUE` |
| `CheckGLBindBuffer(buf)` | `glBindBuffer` 직후 | `GL_INVALID_ENUM`, `GL_INVALID_VALUE` |
| `CheckGLBufferData(size)` | `glBufferData` 직후 | `GL_INVALID_ENUM`/`VALUE`/`OPERATION`, `GL_OUT_OF_MEMORY` |
| `CheckGLEnableVertexAttribArray(idx)` | `glEnableVertexAttribArray` 직후 | `GL_INVALID_OPERATION`, `GL_INVALID_VALUE` |
| `CheckGLVertexAttribPointer({stride})` | `glVertexAttribPointer` 직후 | `GL_INVALID_VALUE`/`ENUM`/`OPERATION` |
| `Init()` | GL 컨텍스트 생성 + GL 로더(gl3w, sb7 경유) 초기화 직후 1회 (Layer 1) | KHR_debug 콜백 등록 |
| `SJH_GL_CHECK(x)` 매크로 | 의심스러운 GL 호출 wrapping | 모든 가능 에러 (큐 드레인) |

#### `UniformDiagnostics` — uniform warn-once 트래커 (static)
| 함수 | 시점 | 동작 |
|---|---|---|
| `NotifyMissing(prog, name)` | `glGetUniformLocation < 0` 발견 시 | (prog, name) 첫 호출만 spdlog::warn |
| `NotifyTypeMismatch(prog, name, expected, actual)` | setter 호출 직전 타입 비교 시 | 불일치 첫 호출만 warn (`actual==0` 은 skip) |
| `Invalidate(prog)` | `Program` 소멸자 | 트래커 정리 (stale 방지) |

상세 사용법은 [src/diagnostics/gl_log.h](../src/diagnostics/gl_log.h), [src/diagnostics/uniform_diagnostics.h](../src/diagnostics/uniform_diagnostics.h)의 Doxygen 주석 참조.

### 크로스 플랫폼 가드 패턴
```cpp
#if defined(GL_VERSION_4_3) || defined(GL_KHR_debug)
    if (glDebugMessageCallback != nullptr) { /* 등록 */ }
#endif
```
**컴파일 타임** (`#if defined(...)`) + **런타임** (`!= nullptr`) 이중 가드.
- 컴파일 타임: gl3w(sb7 이 끌어옴, ⚠ 과거 서술의 "glad" 는 이 프로젝트에 존재한 적 없음)가 4.3+ 심볼을 생성했는지.
- 런타임: 현재 컨텍스트(macOS는 3.3 또는 4.1)에서 함수 포인터가 실제로 로드됐는지.

## 7. 새 모듈 추가 / Placeholder 활성화 절차

### 새 GL 사용 모듈 추가 (예: `renderer`)
1. `src/renderer/CMakeLists.txt` — 위 §2 template 복사, `<module>` → `renderer` 치환.
2. `<src>/renderer/renderer.h`/`renderer.cpp` 생성. `namespace SJH` 사용.
3. GL 호출 있으면 `target_link_libraries(... PRIVATE SJH::diagnostics)` 추가 → `gl_log.h` include 가능.
4. 의심스러운 호출은 `SJH_GL_CHECK(...)` 로 감싸기.
5. `src/CMakeLists.txt`에 `add_subdirectory(renderer)` 추가.
6. 소비자(예: `app/CMakeLists.txt`)에 `SJH::renderer` link.

### Placeholder 모듈 활성화 (예: `program`)
1. [src/CMakeLists.txt](../src/CMakeLists.txt)의 `# add_subdirectory(program)` 주석 해제.
2. [src/program/CMakeLists.txt](../src/program/CMakeLists.txt)에 의존성 추가:
   ```cmake
   target_link_libraries(sjhopengl_program
       PUBLIC  SJH::common SJH::shader project_deps   # (⚠ glad::glad 는 역사적 표기 — 실제론 project_deps, 아래 §5 참조)
       PRIVATE SJH::diagnostics
   )
   ```
3. `program.h`/`program.cpp` 본격 구현. 클래스 디자인은 §3 컨벤션 따름.
4. `glLinkProgram` 직후 `diagnostics::GLObjectLog::CheckProgramLink(mProgram, name)` 호출.
5. (옵션) draw 직전 디버그 빌드에서 `CheckProgramValidate`.

### 체크리스트
- [ ] CMakeLists.txt가 `sjhopengl_<module>` + `SJH::<module>` alias 패턴 따름
- [ ] PUBLIC/PRIVATE 의존성이 헤더 노출 여부와 일치
- [ ] GL 호출이 있으면 diagnostics 의존 추가
- [ ] 클래스는 팩토리 패턴 + private 생성자 + `Try*` 내부 메서드
- [ ] `CLASS_PTR` 매크로로 스마트 포인터 별칭 정의
- [ ] 빌드 검증: `cmake --build build_Darwin` 성공

## 8. Generated Artifacts 배치 규칙

### 원칙
`configure_file()` 같은 빌드 시점 생성 산출물은 **손으로 쓴 public 헤더와 분리**한다.

| 산출물 종류 | 위치 | 가시성 |
|---|---|---|
| 손으로 쓴 public 헤더 (모듈 외부 공유) | [include/<group>/](../include/) | INTERFACE/PUBLIC |
| 손으로 쓴 모듈 내부 헤더 | [src/<module>/](../src/) | 모듈의 PUBLIC/PRIVATE |
| `configure_file` 생성 산출물 (`.in` → `.h`) | `.in`은 **소비자 타겟 디렉토리**, 출력은 그 타겟의 `${CMAKE_CURRENT_BINARY_DIR}` | 보통 PRIVATE |

### 사례 — [<app>/config.h.in](../app/config.h.in)이 `include/`가 아닌 `app/`에 있는 이유

1. **단일 소비자(locality)** — `WINDOW_NAME`/`WINDOW_WIDTH`/`WINDOW_HEIGHT`는 [<app>/main.cpp](../app/main.cpp)만 사용. 어떤 `SJH::*` 모듈도 모름. 유일한 소비자 옆에 두는 게 자연스러운 위치.
2. **손으로 쓴 vs 생성** — `include/`는 git-committed 안정 헤더의 자리. `.in` 템플릿 + 빌드 산출물(`.h`)은 빌드 디렉토리(`build_*/app/config.h`)에 짝지어 둠. 두 부류를 같은 디렉토리에 섞으면 "손으로 편집해도 되는가, 빌드가 덮어쓰는가" 혼란.
3. **입출력 대칭** — `configure_file(config.h.in config.h)`은 상대 경로 사용 시 `${CMAKE_CURRENT_SOURCE_DIR}`(=`app/`) ↔ `${CMAKE_CURRENT_BINARY_DIR}`(=`build_*/app/`)로 대칭. 입력만 옮기면 비대칭이 깨짐.
4. **PRIVATE scope 보존** — [app/CMakeLists.txt](../app/CMakeLists.txt)의 `target_include_directories(... PRIVATE ${CMAKE_CURRENT_BINARY_DIR})`로 `config.h`가 app 타겟에만 보이도록 격리. `include/`로 옮기면 자연스럽게 public path 노출 유혹이 생기고, 그 순간 라이브러리 모듈들이 app 전용 설정을 들여다보게 됨 — 의존 방향 역류.

### 변동성과는 무관한 결정
"config는 자주 바뀌니까 app에 둔다"는 이유는 **빗나감**. 자주 바뀌는 건 [<cmake>/Config.cmake](../cmake/Config.cmake)의 변수값이지 `.in` 템플릿 자체가 아님. 헤더 위치 기준은 **소비자 범위 + 생성/손-작성 구분 + 가시성** 세 가지.

### 예외 — 여러 모듈에 공유되는 생성 헤더
예: `version.h.in`이 라이브러리 + 앱 양쪽에서 필요해지면 → 루트 또는 별도 `cmake/generated/`에 두고 INTERFACE 라이브러리로 노출. 현재 프로젝트에는 해당 사례 없음(YAGNI — §1).

## 9. Open Decisions / Future Work

### 즉시 후속
- [ ] `<app>/main.cpp` 의 GL 로더(gl3w, sb7 경유) 초기화 직후 `SJH::diagnostics::GLDebug::Init()` 호출 추가 (실측: 2026-07 시점 호출부 없음 — 여전히 미해결).
- [x] ~~`program` 모듈 활성화~~ — 완료. 9개 모듈 전부 활성 (§5, 당시 시점) — ⚠ 이후 모듈 수는 계속 증가해 현재 18개(§5 archival 안내 참조), 이 항목은 "당시 완료" 기록으로만 유효.
- [ ] `Program::~Program` 이 `Uniforms::Forget` 과 함께 `Diagnostics::UniformDiagnostics::Invalidate` 도 호출해야 일관 (§6 Lifecycle 계약 — 현재 `Forget` 만 호출, `Invalidate` 누락).
- [x] ~~`Material` 의 dangling handle~~ — `Material` 이 `const Texture*` 관찰자를 보유, `ResourceRegistry` 가 세션 수명(per-item evict 없음)으로 모든 Material/Model 보다 오래 삶 → 구조적으로 해소됨 (§11.2).

### 결정 보류 사안
- **`common.cpp`의 spdlog 직접 사용**: 현재 [common.cpp:35](../src/common/common.cpp#L35)(⚠ 라인 번호 재확인 — 과거 `:11` 서술은 stale, 문서 주석 증가로 이동)이 `spdlog::error`로 파일 로드 실패를 출력. 이는 GL 진단이 아니므로 `diagnostics`로 옮기지 않음. 향후 *일반 로깅 facade*가 필요해지면 별도 `logging` 모듈 신설 고려 (현재는 YAGNI).
- **`GLDebug::Init()` 호출 위치**: `<app>/main.cpp`에 직접 vs 렌더 초기화 지점에 캡슐화. (⚠ 이 항목이 전제한 `context` 모듈은 이후 폐기됐다 — "context 모듈을 본격 구현할 때 결정"이라는 원 문구는 무효. 대체 후보는 `src/render/`(DeviceContext) 또는 클라이언트 부트스트랩, 여전히 미결정.)
- **에러 출력 시 호출부 위치 보존**: 현재 spdlog의 `__FILE__`/`__LINE__`이 `gl_log.cpp` 내부를 가리킴. 호출부 위치가 필요하면 `tag` 인자로 전달 (현재 방식) 또는 매크로 wrapper 도입 검토.

### 의도적으로 안 한 것 (anti-future-work)
- `Result<T, E>`/`std::expected` 도입 — 학습 프로젝트 규모에 오버킬, `nullptr`/`std::optional` 패턴이 일관됨.
- spdlog wrapper 클래스 — 컴파일 타임 포맷 체크 손실 + YAGNI.
- 예외 기반 에러 핸들링 — OpenGL/GLFW가 안 쓰므로 일관성 깨짐.

## 10. 다른 Claude Agent에게 핸드오프 시 핵심 포인트

이 프로젝트의 코드를 만질 때 반드시 인지할 것:

1. **이 코드베이스는 예외를 던지지 않는다.** "Try-Catch"라는 단어가 나오면 그건 *상태 폴링 + bool 반환*을 뜻한다.
2. **`diagnostics` 모듈은 GL 전용 진단**이지 spdlog 추상화가 아니다. 비-GL 로깅은 spdlog 직접 사용.
3. **PUBLIC vs PRIVATE link 결정 트리**(§4)를 항상 적용. 헤더에 안 보이는 의존은 PRIVATE.
4. **새 모듈은 §2 template + §7 체크리스트** 따라야 빌드 시스템과 일관됨.
5. **GL 4.1 Core / GLSL 410 강제** (⚠ 과거 "GL 3.3 혹은 4.1" 서술은 stale — 실측: `apps/_MyApp_/main.cpp` 가 `info.majorVersion=4, info.minorVersion=1` 명시. sb7 macOS 기본값(3.2)에 속지 말 것) — `glDebugMessageCallback` 등 4.3+ 기능 사용 시 컴파일 + 런타임 이중 가드 필수.
6. **placeholder 모듈** (§5, archival 스냅샷 시점 개념)들은 `namespace SJH {}`만 있는 빈 파일이던 시기가 있었음 — ⚠ 현재(18모듈)는 placeholder 없음, 전부 활성. 새 모듈 착수 초기 단계 설명으로만 유효.
7. **헤더 include 형식 구분** (§2): 시스템/vcpkg = `<<vendor>/file.h>`, 내부 모듈 = `"<module>/file.h"`. **GL 타입(`GLenum`/`GLuint`)은 `<GL/gl3w.h>` 출처**(`<GLFW/glfw3.h>` 아님, ⚠ 과거 "glad" 서술은 stale — 이 프로젝트의 GL 로더는 gl3w, sb7 이 끌어옴), 두 헤더 모두 쓸 땐 항상 gl3w(=sb7 include) 먼저.
8. **생성 산출물(`config.h` 등) 위치는 소비자 타겟 디렉토리에** (§8). `include/`에 옮기는 건 안티패턴 — `include/`는 손으로 쓴 안정 헤더 전용.
9. **명시적 의존성** (§4): 부모/형제의 transitive에 기대지 말 것. 잘 안 보이는 4가지 비용이 누적됨.
10. **동작/데이터 분리 3패턴** (§11): 자유 함수 family, lifetime ownership 분리, 값 클래스 캡슐화 — 세 가지가 최근 디자인 축.

## 11. 동작과 데이터의 분리 — 3가지 패턴

§3 의 팩토리/leaf-type 컨벤션 위에, *동작을 클래스 밖으로 빼거나 데이터의 소유를 명시 분리* 하는 3가지 패턴이 추가로 자리 잡았다.

### 11.1 자유 함수 family — 클래스 외부로 *동작* 분리 (`SJH::Uniforms` 사례)

`Program` 은 *GL program 의 lifetime + link 상태* 만 책임진다. uniform 값 설정은 *그 위에 얹는 별개 관심사* — 멤버 함수가 아니라 **`namespace SJH::Uniforms` 의 자유 함수 family** 로 분리.

| 측면 | 결정 |
|---|---|
| 호출 형태 | `Uniforms::SetMat4(prog, "name", m4)` — 첫 인자가 `Program&` (C# extension method 와 동등 효과) |
| 캐시 위치 | `Program` 의 멤버가 *아님* — [program_uniforms.cpp](../src/program/program_uniforms.cpp) 의 anonymous namespace 에 `unordered_map<GLuint, unordered_map<string, UniformEntry>>` static |
| friend 선언 | **불필요** — 자유 함수가 `Program::GetProgramAddr()` (public) 만 사용. `Program` 헤더는 `Uniforms` 의 존재를 *전혀 모름* |
| lifetime | `BuildCache` (Program::Create 가 link 직후 호출) ↔ `Forget` (Program::~Program 이 호출) 짝 — GL ID 재사용에 의한 stale 캐시 방지 |

**왜 멤버 함수가 아닌가** — 새 uniform 타입 추가 시 `Program` 헤더가 *그대로* (OCP). 멤버로 두면 매번 `Program` API 가 비대해지고, 캐시를 private 멤버로 두면 8개 friend 선언이 헤더를 오염.

**진화 이력** (세션 중 2단계): ① `class ProgramUniforms` (인스턴스 + 캐시 멤버) → ② 자유 함수 + Program 의 private 캐시 + friend 8개 → ③ **자유 함수 + 외부 static 캐시 + friend 0개**. ③ 이 최종 — Program 헤더가 Uniforms 를 모르는 완전 디커플링.

> 적용 트리거 — "이 동작이 클래스의 *본질적 책임* 인가, *위에 얹는 별개 관심사* 인가?" 후자면 자유 함수 + (가능하면 public getter 만으로) 외부 분리.

### 11.2 lifetime ownership 분리 — *관찰자* vs *객체 소유자*

GL 리소스를 참조하는 클래스가 *반드시 그 리소스를 소유할 필요는 없다*. `Material` 이 대표 사례:

| 보유 주체 | 보유 내용 | 비고 |
|---|---|---|
| `Material` | `const Texture*` 관찰자 (`mDiffuseTexture`/`mSpecularTexture`) + sampler 유닛 + 텍스처 *이름 키* | 소유 X — 비소유 관찰자 포인터 |
| `ResourceRegistry` | `Texture` UPtr (이름 키 캐시) | 이름 기반 흐름의 lifetime owner |
| `Model` | `vector<TextureUPtr> mTextures`, `vector<MaterialUPtr> mMaterials` | assimp 로드 흐름의 lifetime owner |

핵심 — *관찰자 포인터를 들고 있는 객체* 와 *그 포인터가 가리키는 객체를 살려두는 소유자* 가 분리됨.

**단일 불변식 — dangling 구조적 해소**: `ResourceRegistry` 는 **세션 수명** 을 가지며 per-item evict 가 없다 — 세션이 살아있는 한 캐시 내 `Texture`/`Material`/`Model` 은 유효. 따라서 `Material::mDiffuseTexture` (`const Texture*`) 는 `ResourceRegistry` 가 살아있는 동안 항상 유효 → *호출자 규율* 없이 구조적으로 dangling handle 해소.

멤버 선언 순서(`mTextures` → `mMaterials` → `mRenderUnit`)는 소유자가 관찰자보다 *나중에 소멸* 하도록 보장한다 (C++ 역순 소멸 규칙 활용).

**SP3 현재 상태 — Program / Mesh 는 ResourceRegistry 미지원**: `ResourceRegistry::Create*` 가 `Texture / Material / Model` 만 제공. `Program` / `Mesh` 는 *직접 factory* (`Program::CreateWithVSFS`, `Mesh::CreateBox` 등) 로 UPtr 반환 — owner 가 호출자 (현재 챕터/app 직접 보유). future work: ResourceRegistry 확장 (`CreateProgram(key, vs, fs)`, `RegisterMesh(key, MeshUPtr)`) 으로 모든 GPU 자원의 owner 를 한 곳에 응집 → §11.4 의 챕터 컨벤션이 완성됨.

### 11.3 챕터/App 의 자원 보유 컨벤션 — *씬 + 시스템* 만, *자원* 은 위탁

Cocos2D `cc::Director::getInstance()->getTextureCache()`, Unity `Resources.Load<T>`, Unreal `UAssetManager::Get()` 정통 — **챕터/app 코드는 자원 소유자가 아니다**. *씬 트리* 와 *렌더 시스템 인스턴스* 만 보유, 자원은 매니저에 위탁.

#### 권장 패턴 (정통)

```cpp
class my_chapter : public sb7::application
{
    void startup() override {
        auto& reg = ResourceRegistry::Get();           // 매니저 (싱글톤 후보)
        Material* mat = reg.CreateMaterial("box");      // 위탁 — 비소유 핸들 반환
        mat->SetProgram(reg.FindProgram("simple"));     // 핸들끼리 배선

        auto& dir = Scene::Director::Get();             // 씬 트리
        auto box = std::make_unique<Scene::Actor>("Box");
        box->AddComponent<Scene::MeshRenderer>(reg.FindMesh("box"), mat);
        dir.Root().AddChild(std::move(box));
    }

private:
    Scene::SceneRenderer mRenderSys;   // 시스템 — 자체 mQueue 보유, app 멤버로 OK
    double              mLastTime;    // 시간 상태 — app 한정 OK
    // ❌ 자원 객체 (ProgramUPtr/MeshUPtr/MaterialUPtr) 를 직접 보유 금지
};
```

#### 안티 패턴 (SP3 시점 ecs_demo)

```cpp
private:
    SJH::ProgramUPtr  mProgram;   // ❌ app 이 owner — ResourceRegistry 우회
    SJH::MeshUPtr     mBoxMesh;   // ❌ Mesh::CreateBox 결과를 직접 보유
    SJH::MaterialUPtr mBoxMat;    // ❌ ResourceRegistry::CreateMaterial 사용 가능했음
```

**왜 안티 패턴인가**:
- 자원 캐싱 정책 (key 중복 차단, 일괄 해제) 이 한 곳에 응집되지 않음
- 동일 자원 재사용 (씬 간, 챕터 간) 가능성 차단
- ddd Single Responsibility 위반 — app 클래스가 *자원 owner* + *씬 사용자* 두 역할 동시 수행

#### 현재 적용 가능 vs 미적용

| 자원 | 즉시 위탁 가능 | 비고 |
|---|---|---|
| Material | ✅ `ResourceRegistry::CreateMaterial(key)` | SP3 시점 즉시 적용 가능 |
| Texture  | ✅ `ResourceRegistry::CreateTexture(key, image)` | 동일 |
| Model    | ✅ `ResourceRegistry::CreateModel(key, file)` | 동일 |
| **Program** | ⛔ ResourceRegistry 미지원 | future work — `CreateProgram(key, vs, fs)` 추가 후 위탁 |
| **Mesh**    | ⛔ ResourceRegistry 미지원 | future work — `RegisterMesh(key, MeshUPtr)` 추가 후 위탁 |

#### Owner 슬롯 결정 (future work)

`ResourceRegistry` 인스턴스의 owner 두 후보:

- **A. `Scene::Director::Get().GetResourceRegistry()`** — Cocos 정통 (`cc::Director` 가 `TextureCache` 보유). 씬과 자원의 lifecycle 일치.
- **B. `ResourceRegistry::Get()` 자체 싱글톤** — SP2 `DeviceContext::Get()` 패턴과 일관. Director 와 독립.

판단 — **B 권장**. 이유:
- Director 가 *Actor tree owner* 본업에 집중 (SP3 Single Responsibility 결정)
- 자원은 씬보다 *오래 살 수 있음* (씬 교체 시에도 자원 캐시는 유지)
- 기존 `DeviceContext::Get()` 과 일관된 Meyer's singleton 패턴

> 컨벤션이 "현재 적용 가능" 항목은 **즉시** 적용. "ResourceRegistry 미지원" 항목은 future task (SP3.5 또는 SP4 후보) 에서 일괄 마이그레이션.

§11.2 의 인용도 참고 — `Material::mDiffuseTexture` (`const Texture*`) 가 ResourceRegistry 의 *세션 수명* 에 의해 dangling 가드. 이 보장은 자원 owner 가 단일 (ResourceRegistry) 일 때만 성립 — 챕터가 자원 owner 면 챕터 lifecycle 보다 짧은 자원 사용처에서 dangling 위험 재발.

§11.2 의 SP3 현재 상태 box (Program/Mesh 미지원) 가 이 §11.3 의 적용 한계를 결정한다.

### 11.4 값 클래스의 캡슐화 — getter/setter + 불변식 (`Material` 사례)

POD-like 값 클래스라도 *멤버 간 불변식* 이 있으면 public 멤버는 위험. `Material` 은 public 멤버 → private + getter/setter 로 전환:

- **이름 ↔ 핸들 짝** — `SetDiffuseTextureName(name)` 호출 시 `mDiffuseTexture = nullptr` 으로 *해석 캐시 무효화*. 이름과 핸들이 어긋난 stale 상태를 *구조적으로 불가능* 하게.
- **범위 불변식** — `SetShininess(v)` 가 `[2, 256]` clamp. ImGui 의 min/max 에 의존하지 않고 *클래스 자체가* 강제.
- **factory 일관성** — `Material::Create()` (private ctor + static factory). `ResourceRegistry::Create()`, `Image::Load/Create`, `Texture::CreateTexture` 도 동일 — §3 팩토리 패턴이 자원 객체뿐 아니라 *값 클래스* 까지 일관 적용.

> 적용 트리거 — "이 멤버를 *임의 값* 으로 대입하면 클래스가 모순 상태가 되는가?" 그렇다면 setter 로 감싸 불변식을 강제. 단순 독립 값(예: `Camera::mPos`)은 public 유지 가능 (§F 참조 — Camera 는 의도된 POD-like).

---

### 11.5 진실의 원천 단일화 — 동일 의미 변수 캡슐화 (`Material.PassKind` 사례)

§11.4 의 *Material 캡슐화* 가 *불변식 강제* 라면, **11.5 는 *두 변수가 동일 의미* 일 때의 처리** — 진정한 의미가 *하나뿐* 임을 타입으로 못박는다.

#### 문제 — 두 진실의 원천 충돌

SP-Pass 진행 중 발견된 버그:

```cpp
// 사용자 코드
auto* mat = registry.CreateMaterial("Window");
mat->SetPass(Pass::Kind::Transparent);     // 의도: queue 3000

renderer.QueueLayer = 2000;   // 다른 곳에 두 번째 진실의 원천이 있어 덮어씀
```

`Material.QueueLayer` 와 `MeshRenderer.QueueLayer` 가 *둘 다 public 의 동일 의미* → 어느 쪽이 진실인지 알 수 없음. MeshPassProcessor 는 `MeshRenderer.QueueLayer` 만 보므로 `Material.SetPass(Transparent)` 가 *침묵 무효화*.

#### 해법 — Material 만 진실의 원천 (Pass.Kind private), MeshRenderer 는 *직교 축*

```cpp
class Material {
    Pass::Kind mPassKind = Pass::Kind::Opaque;   // private — 진실의 원천
public:
    Material& SetPass(Pass::Kind k);
    Pass::Kind GetPass() const;
    int GetQueueLayer() const { return Pass::QueueOf(mPassKind); }  // 도출 (alias)
    // QueueLayer 멤버 없음, SetQueueLayer 없음
};

struct MeshRenderer : public Scene::Component {
    Material* Material = nullptr;
    int QueueOffset = 0;   // 직교 축 — *같은 Material 인스턴스 간 미세 순서*
};

// SceneRenderer
cmd.queueLayer = mr->Material->GetQueueLayer() + mr->QueueOffset;
```

#### 원칙 정립 — 두 변수의 관계 판별

| 관계 | 처리 |
|---|---|
| **동일 의미** (e.g., 같은 queue 정수) | 한쪽 private + 다른 쪽 도출 (alias) |
| **직교 축** (e.g., 의도 분류 vs 인스턴스 offset) | 양쪽 public 공존 가능 |

판별 질문: *"두 변수를 각각 다른 값으로 설정했을 때 모순이 가능한가?"* 가능하면 동일 의미 → 통합. 모순 불가능 (서로 다른 의도로 동시에 의미를 가짐) 이면 직교 → 분리 유지.

#### 정통 엔진 비교

- **Unity**: 3변수 (Pass + Queue + sortingOrder) — `Material.renderQueue` public 직접 조작 가능 = *escape hatch* 명시
- **Unreal / Filament / Cocos**: 2변수 (BlendMode + sortBias) — *blendMode → queue 자동* 도출
- **Bevy**: 1변수 (alpha_mode) — *완전 도출*

우리 선택: **2변수 (Filament/Unreal/Cocos 정통)**. Unity 의 3변수 분리는 *명시적 escape hatch* 가 필요할 때만 — 기본은 *진실의 원천 단일화* 가 안전.

#### 기본값의 무게 — Pass.Kind 가 7 GL state 를 자동 적용

| Kind | DepthTest | DepthWrite | DepthFunc | CullMode | Blend |
|---|---|---|---|---|---|
| Opaque | on | on | GL_LESS | GL_BACK | off |
| Skybox | on | off | **GL_LEQUAL** | **GL_FRONT** | off |
| Transparent | on | **off** | GL_LESS | **0 (off)** | **on** |

*분류 한 줄로 정상 case 의 대부분이 자동 처리*. 사용자가 *모든 디테일* 을 매번 설정하게 만들지 말 것 — Unity Standard Shader / Filament `MaterialBuilder` 정통. override 는 *예외 case* 만 (e.g., `MeshRenderer.DepthTest`).

---

### 11.6 Retina HiDPI — Physical Framebuffer 기준 (`extern/sb7code` 우회 패턴)

#### 문제 — sb7 의 logical / physical 분리 누락

`extern/sb7code` 의 `sb7::application::onResize(int, int)` 는 GLFW 의 `glfwSetWindowSizeCallback` 을 그대로 전달 → **logical size** (e.g., macOS Retina 1600×1200 → 800×600).

GL 의 `glViewport` / FBO 크기 / Camera.Aspect 는 **physical framebuffer size** 가 정답. 그대로 쓰면 Retina 에서 *좌하단 1/4 영역* 만 그려진다.

#### 해법 — 챕터 측에서 `glfwGetFramebufferSize` 로 변환

`extern/sb7code` 는 *절대 수정 금지* (memory: `sb7code_immutable`) → 패턴은 *챕터 측* 에 둔다.

```cpp
void onResize(int /*logicalW*/, int /*logicalH*/) override {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    if (w <= 0 || h <= 0) return;
    sb7::application::onResize(w, h);
    glViewport(0, 0, w, h);
    for (auto* cam : mCameras) cam->Aspect = float(w) / float(h);
    RecreateFramebuffers(w, h);
}

void render(double /*t*/) override {
    int w, h; glfwGetFramebufferSize(window, &w, &h);
    DeviceContext::Get().SetDefaultTargetSize(w, h);
    // ... (매 프레임 갱신으로 resize 콜백 누락 가드)
}
```

#### 원칙 정립 — 외부 라이브러리 한계는 *호출자* 가 보정

외부 라이브러리가 *추상화 누설* (logical/physical 미구분) 했고 *수정 불가* 일 때:
1. 호출 시점에 *변환 layer* 삽입 (`glfwGetFramebufferSize` 가 cross-platform 단일 진실)
2. *매 프레임 갱신* 으로 콜백 누락에 대한 *eventual consistency* 보장
3. *모든 챕터에 동일 패턴* — `apps/<chapter>/main.cpp` 의 `onResize` + `render()` 첫 5줄.

이는 `extern/sb7code` 처럼 *upstream 이 동결된 의존성* 을 다루는 **일반 패턴** 이다 — sb7 한정이 아님.
