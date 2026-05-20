# Framebuffer ↔ Viewport 정합성 — 버그 분석 + Cat G 테스트

> 작성 2026-05-19. FBO 풀스크린 블릿이 "기울어지고 확대"되어 보이던 버그의 진단·수정·회귀 테스트 기록.
> 관련: [2026-05-09-gl-validate-design.md](2026-05-09-gl-validate-design.md), [BugReport.md](BugReport.md)

---

## 1. 증상

- FBO 에 씬을 렌더한 뒤 그 컬러 텍스처를 풀스크린 quad 로 화면에 블릿할 때,
  화면 전체를 덮어야 할 프레임이 **기울어지고 확대된 것처럼** 보임.
- 윈도우를 한 번 **리사이즈하면 정상**으로 돌아옴.
- 에러 로그 없음 — 조용한 시각 버그.

## 2. 환경

- macOS (Darwin) arm64, Retina 디스플레이.
- OpenGL 4.1 Metal 호환 레이어, GLFW + glad.
- 윈도우 논리 크기 `WINDOW_WIDTH × WINDOW_HEIGHT` = 800 × 600.
- **Retina 물리 프레임버퍼 = 1600 × 1200** (논리의 2배).

## 3. 진단 과정

### 3.1 두 후보

| 후보 | 가설 |
|---|---|
| A | 카메라 설정 (`EulerPitch=-20°`, `Pos.z=8`) 때문에 *씬 자체*가 기울어 보임 — 버그 아님 |
| B | `viewport ↔ FBO ↔ 윈도우` 크기 불일치로 풀스크린 패스가 왜곡 — 버그 |

### 3.2 후보 B 진단 — 1회 로그

`Render()` 첫 프레임에 viewport / FBO 텍스처 / `mWidth·mHeight` 세 값을 1회 출력:

```
[diagB] mWidth/mHeight  = 800 x 600
[diagB] FBO color tex   = 800 x 600
[diagB] GL viewport     = [0, 0, 800, 600]
```

세 값은 *서로* 일치 — 하지만 `GLStateLog/startup` 의 viewport 는 **1600×1200**.
즉 셋 다 `800×600` 으로 통일됐지만 **실제 윈도우(1600×1200)와 어긋남**.

리사이즈하면 `framebuffer size changed: (1612 x 1212)...` 처럼 실제 물리 픽셀로
콜백이 발동 → 모두 동기화 → 정상. **후보 B 확정.**

## 4. 근본 원인

`app/main.cpp` 가 메인 루프 진입 전 초기 `Reshape` 를 1회 명시 호출하는데,
인자로 **컴파일 상수 `WINDOW_WIDTH`/`WINDOW_HEIGHT` (논리 800×600)** 를 넘김:

```cpp
// 버그
OnFramebufferSizeChange(window, WINDOW_WIDTH, WINDOW_HEIGHT);  // 논리 800×600
```

Retina 에서 실제 기본 프레임버퍼는 `1600×1200`. 따라서:

- `glViewport` 와 FBO 텍스처가 `800×600` 으로 잡힘 (내부적으론 일관)
- 그러나 실제 윈도우 drawable 은 `1600×1200`
- Pass 2 (`BindToDefault`) 의 풀스크린 quad 가 `1600×1200` 창의 *좌하단 800×600* 기준으로만 매핑 → 비율·범위 왜곡 = "기울어짐·확대"

리사이즈가 고친 이유: 리사이즈 콜백은 *실제 물리 픽셀*을 전달하므로 모든 값이 재동기화됨.

## 5. 수정

`app/main.cpp` — 초기 seed 를 `glfwGetFramebufferSize()` (실제 물리 픽셀) 로 조회:

```cpp
// 수정
int fbWidth = 0, fbHeight = 0;
glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
OnFramebufferSizeChange(window, fbWidth, fbHeight);
```

`glfwGetFramebufferSize` 는 HiDPI 의 물리 픽셀을 반환 (`glfwGetWindowSize` 의 논리 크기와 구분).
이로써 startup 상태 = 리사이즈 후 상태 → 첫 프레임부터 정상.

## 6. 테스트 — GLValidate Cat G

회귀를 자동 탐지하기 위해 `GLValidate` 진단 모듈에 **Cat G — `CheckViewport`** 추가.

### 6.1 API

```cpp
// src/diagnostics/gl_validate.h
size_t CheckViewport(int expectedWidth, int expectedHeight, const char* tag);
```

현재 `glGetIntegerv(GL_VIEWPORT)` 결과를 caller 가 넘긴 기대 크기와 비교:
- 일치 → `info` 로그, return 0
- 불일치 → `warn` 로그, return 1

순수 GL 호출 — caller 가 ground truth 를 인자로 전달한다 (예: `glfwGetFramebufferSize` 결과).
불일치 시에만 `warn` 이므로 매 프레임 호출해도 정상일 땐 조용함.

### 6.2 회귀 가드 배치

`app/main.cpp` — 초기 seed 직후 1회 호출:

```cpp
glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
OnFramebufferSizeChange(window, fbWidth, fbHeight);
glfwSetFramebufferSizeCallback(window, OnFramebufferSizeChange);

// [Test/Cat G] seed 직후 viewport 가 실제 프레임버퍼 크기와 일치하는지 검증.
SJH::Diagnostics::GLValidate::CheckViewport(fbWidth, fbHeight, "startup-seed");
```

만약 누군가 §5 의 수정을 되돌려 다시 논리 상수로 seed 하면,
`CheckViewport` 가 `viewport 800x600 != expected 1600x1200` 로 **즉시 warn** 을 띄운다.

### 6.3 (선택) 패스별 검사

`Context::Render()` 의 각 패스 직후에도 호출 가능:

| 패스 | 호출 |
|---|---|
| FBO 패스 (`m_framebuffer->Bind()` 후) | `CheckViewport(fboTex->GetWidth(), fboTex->GetHeight(), "FBO pass")` |
| 화면 패스 (`BindToDefault()` 후) | `CheckViewport(mWidth, mHeight, "screen pass")` |

이상적으로는 각 패스가 자기 타깃 크기로 `glViewport` 를 명시 설정하고,
그 직후 Cat G 로 확인 — defense-in-depth.

## 7. 검증

수정 + Cat G 적용 후 실행 로그:

```
[main.cpp:36] framebuffer size changed: (1600 x 1200)
[GLValidate/startup-seed/Cat G] viewport 1600x1200 matches expected target
[diagB] mWidth/mHeight  = 1600 x 1200
[diagB] FBO color tex   = 1600 x 1200
[diagB] GL viewport     = [0, 0, 1600, 1200]
```

- seed 가 `1600×1200` (실제 물리 픽셀) 로 들어감 — 수정 전 `800×600` 과 대비
- Cat G `matches expected target` — 회귀 가드 통과
- 리사이즈 없이 첫 프레임부터 풀스크린 블릿 정상

## 8. 재발 방지 가이드라인

1. **초기 프레임버퍼 크기 seed 는 항상 `glfwGetFramebufferSize()`** 로 조회한다.
   `WINDOW_WIDTH/HEIGHT` 같은 *논리* 상수를 viewport/FBO 크기로 쓰지 않는다.
2. HiDPI/Retina 에서 `glfwGetWindowSize` (논리) ≠ `glfwGetFramebufferSize` (물리, 보통 2×) 임을 항상 의식한다.
3. FBO 텍스처 크기 · `glViewport` · 기본 프레임버퍼 drawable — **세 값이 같은 ground truth 에서 파생**되어야 한다.
4. 새 렌더 패스를 추가할 때, 패스 시작에 `glViewport` 를 명시 설정하고 `GLValidate::CheckViewport` 로 검증한다.
5. "리사이즈하면 고쳐지는" 시각 버그를 만나면 1순위 의심: **초기 seed 가 논리 크기로 들어갔는가**.

## 9. 변경 파일

| 파일 | 변경 |
|---|---|
| `app/main.cpp` | 초기 seed 를 `glfwGetFramebufferSize()` 로 조회 + Cat G 회귀 가드 호출 |
| `src/diagnostics/gl_validate.h` | Cat G `CheckViewport` 선언 추가 |
| `src/diagnostics/gl_validate.cpp` | Cat G `CheckViewport` 구현 추가 |

> `Render()` 의 `diagB` 1회 로그 블록은 진단 전용 — 본 문서 작성 시점 이후 제거 대상.
> 영구 회귀 가드는 Cat G 가 대신한다.
