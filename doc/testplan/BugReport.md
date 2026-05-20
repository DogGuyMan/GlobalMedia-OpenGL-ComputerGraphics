# BugReport — exercise7 모델이 전혀 렌더링되지 않는 문제

## 1. 증상

- `apps/exercise7` 실행 시 화면이 완전한 검정.
- 바닥(floor), 박스(box), 광원 피라미드(light pyramid) **세 개 모두** 보이지 않음.
- 프로그램은 정상 종료(exit 0), stdout/stderr에 에러 한 줄 없음.

## 2. 환경

- macOS (Darwin), Ninja Debug 빌드.
- sb7 + GLFW + 사전 빌드된 정적 라이브러리(`lib/macos/`).
- sb7 컨텍스트 요청 버전: macOS에서 OpenGL 3.2 Core Profile ([include/sb7.h:186-192](../include/sb7.h#L186-L192)).
- Apple Core Profile 지원 상한: OpenGL **4.1** (그 이상은 Metal 정책).

## 3. 근본 원인 — 셰이더 GLSL 버전이 macOS 한계 초과

신규로 작성된 6개 셰이더가 모두 `#version 430 core`로 선언됨:

- `apps/exercise7/resources/shaders/basic_lighting_vs.glsl`
- `apps/exercise7/resources/shaders/basic_lighting_fs.glsl`
- `apps/exercise7/resources/shaders/basic_texturing_vs.glsl`
- `apps/exercise7/resources/shaders/basic_texturing_fs.glsl`
- `apps/exercise7/resources/shaders/simple_color_vs.glsl`
- `apps/exercise7/resources/shaders/simple_color_fs.glsl`

`#version 430`은 OpenGL 4.3을 요구하지만, macOS Core Profile은 4.1까지만 지원 → GLSL 컴파일러가 `#version 430`을 거부 → 세 개의 셰이더 프로그램(`shader_programs[0..2]`)이 모두 broken 상태로 남음 → `glUseProgram` + `glDrawArrays`/`glDrawElements`가 아무 픽셀도 그리지 못함.

참고: 같은 디렉토리의 잔존 파일(`default_vs.glsl`, `texture_fs.glsl`)과 chapter2/5/6/7, exercise5/6의 모든 셰이더는 `#version 410 core`로 통일되어 있었음. exercise7의 신규 셰이더만 어긋남.

## 4. 왜 에러가 출력되지 않았는가 (조용한 실패)

`sb7::shader::load` 시그니처([include/shader.h:10-16](../include/shader.h#L10-L16)):

```cpp
GLuint load(const char * filename,
            GLenum shader_type = GL_FRAGMENT_SHADER,
#ifdef _DEBUG
            bool check_errors = true);
#else
            bool check_errors = false);
#endif
```

`_DEBUG`는 **MSVC 전용** 매크로. macOS clang Debug 빌드에는 정의되지 않으므로 `check_errors`의 디폴트가 **false**가 됨 → 컴파일/링크 실패가 stderr 출력 없이 묻힘. exit 0 + 출력 없음 + 검정 화면이라는 매우 진단하기 어려운 상태가 만들어짐.

## 5. 부차적인 문제 — 배경색이 검정으로 덮어써짐

[apps/exercise7/main.cpp:256-258](../apps/exercise7/main.cpp#L256-L258):

```cpp
const GLfloat black[] = { 1.0f, 1.0f, 0.0f, 1.0f };   // 변수명은 black이지만 실제 값은 노랑
glClearBufferfv(GL_COLOR, 0, black);                   // 색 버퍼를 노랑으로 클리어
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);     // 직후 glClearColor 기본값(검정)으로 다시 클리어
```

`glClearBufferfv`로 지정한 색이 바로 다음 줄 `glClear(GL_COLOR_BUFFER_BIT | ...)`에 의해 즉시 덮어써짐. 셰이더 문제로 객체가 안 그려지는 상황과 합쳐져 화면이 100% 검정이 됨.

## 6. 수정

### 6.1 핵심 수정 — 셰이더 GLSL 버전을 410으로 다운그레이드

위 6개 셰이더의 첫 줄을 다음과 같이 변경:

```diff
-#version 430 core
+#version 410 core
```

본 코드에서 사용한 기능(`layout(location=...)`, `mat3(transpose(inverse(model)))`, `texture(sampler2D, vec2)`, 일반 uniform / out / in)은 모두 GLSL 4.10에서 그대로 지원되므로 본문 수정은 불필요.

### 6.2 권장 수정 — 셰이더 로딩 시 에러 체크 명시

[apps/exercise7/main.cpp:17,20](../apps/exercise7/main.cpp#L17) 의 호출에 `check_errors`를 명시적으로 `true`로 전달:

```cpp
GLuint vertex_shader   = sb7::shader::load(vs_file, GL_VERTEX_SHADER,   true);
GLuint fragment_shader = sb7::shader::load(fs_file, GL_FRAGMENT_SHADER, true);
```

이렇게 하면 macOS Debug 빌드에서도 셰이더 컴파일/링크 실패가 stderr로 즉시 출력되어 같은 류의 "조용한 실패"를 사전 차단 가능.

### 6.3 부차적 수정 — 배경색 덮어쓰기 제거

`glClear`에서 `GL_COLOR_BUFFER_BIT`를 제거하거나, `glClearBufferfv`를 쓰지 말고 `glClearColor` + `glClear`만 사용하도록 통일:

```cpp
// 안: 두 클리어가 충돌
const GLfloat color[] = { 0.0f, 1.0f, 0.0f, 1.0f };
glClearBufferfv(GL_COLOR, 0, color);
glClear(GL_DEPTH_BUFFER_BIT);   // 색 버퍼는 다시 클리어하지 않음

// 또는: 전통적 패턴
glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
```

## 7. 검증

- 6개 셰이더의 `#version 430 core` → `#version 410 core` 변경 후 재빌드/실행.
- 바닥, 박스, 피라미드가 정상 렌더링됨을 육안 확인.

## 8. 재발 방지 가이드라인

1. **macOS 대상 셰이더는 `#version 410 core`를 상한**으로 사용한다 (sb7가 `__APPLE__`에서 GL 3.2 Core를 요청하는 한).
2. 새 챕터/연습문제 셰이더를 추가할 때는 동일 디렉토리의 기존 셰이더 `#version` 라인과 일치시킨다.
3. macOS 환경에서 `sb7::shader::load`를 호출할 때 `check_errors=true`를 명시한다 (`_DEBUG`는 MSVC 전용 매크로이므로 디폴트로는 클랭에서 비활성).
4. 검정 화면 + 무에러 + exit 0 조합을 만났을 때의 1순위 의심: **셰이더가 조용히 실패했는가**. `glGetError`나 `glGetProgramiv(GL_LINK_STATUS)` 직접 검사로 빠르게 확인 가능.
