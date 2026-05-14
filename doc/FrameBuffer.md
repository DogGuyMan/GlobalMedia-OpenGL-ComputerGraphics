# 📒 Framebuffer 학습 노트

> SuperBible Ch.9 — render-to-texture / multi-pass 관련 개념 총정리
> 작성일 기준: 2026-05-15

---

## 0. 한 줄로 시작

> **FBO = 액자, Texture/RBO = 도화지. 도화지 고르는 기준은 "셰이더가 sampler 로 다시 읽느냐" 단 하나.**

---

## 1. 세 개의 객체와 그 관계

| 객체 | 본질 | 단독 사용 | 누가 만드나 |
|---|---|---|---|
| **Default FBO (ID 0)** | 윈도우와 연결된 화면용 컨테이너 | (자동) | GLFW |
| **Custom FBO** | 빈 컨테이너 (attachment 필요) | ❌ | 사용자 |
| **Texture** | 샘플링 가능한 픽셀 저장소 | ⭕ (FBO 없이도 일반 텍스처로 사용 가능) | 사용자 |
| **Renderbuffer (RBO)** | 샘플링 불가능한 픽셀 저장소 | ❌ (오직 FBO attachment 용도) | 사용자 |

**관계도**

```
FBO ─┬─ COLOR_ATTACHMENT0..N  ← Texture 또는 RBO
     ├─ DEPTH_ATTACHMENT      ← Texture 또는 RBO
     └─ STENCIL_ATTACHMENT    ← Texture 또는 RBO
```

---

## 2. Texture vs RBO — 두 축의 직교

|  | Color | Depth | Stencil |
|---|---|---|---|
| **Texture** (sampler 로 읽기 ⭕) | 포스트프로세싱, 미러 | 그림자맵, SSAO | 고급 마스킹 |
| **RBO** (sampler 로 읽기 ❌) | MSAA 중간 버퍼 | 일반 z-buffering | 일반 stencil test |

→ **6칸 모두 합법**. "Color = Texture, Depth = RBO" 는 디폴트 관습일 뿐.

**결정 트리**

```
이 attachment 의 값을 셰이더에서 한 번이라도 sampler 로 읽나?
   ├─ YES → Texture
   └─ NO  → RBO (더 가볍고 빠름)
```

---

## 3. 셰이더 영역 vs 고정 파이프라인 영역

GPU 파이프라인은 두 종류 단계로 구성된다:

```
[Vertex Shader] ──────── 프로그래머블 (GLSL)
[Rasterization] ──────── 고정
[Fragment Shader] ────── 프로그래머블 (GLSL)
[Depth Test]   ◀──── Depth attachment 가 여기서 자동으로 활약 (고정)
[Stencil Test] ◀──── Stencil attachment 가 여기서 자동으로 활약 (고정)
[Blending]     ──────── 고정
[Color Write]  ◀──── Color attachment 가 여기서 자동으로 활약 (고정)
```

| 영역 | Texture | RBO |
|---|---|---|
| 셰이더 코드에 등장 | ✅ `uniform sampler2D` | ❌ 아예 안 등장 |
| 사용 주체 | 셰이더 + 고정 단계 | 오직 고정 단계 |
| 제어 수단 | GLSL + `glBindTexture` | OpenGL state 명령 (`glDepthFunc`, `glStencilFunc`, ...) |

**→ RBO 의 값은 셰이더가 접근 *안* 한다. 그게 RBO 다.**

---

## 4. 패스 분리 — 멀티패스 패턴

**Pass = "FBO 한 번 바인딩 후 draw" 의 한 단위**

```
[Pass 1] glBindFramebuffer(myFBO);  → 씬을 Texture/RBO 에 그림
[Pass 2] glBindFramebuffer(0);      → 그 Texture 를 입혀 화면에 그림
                                      (default FBO 가 자동으로 화면 표시)
```

- `glBindFramebuffer(..., 0)` 는 unbind 가 아니라 **default FBO 로 교체**
- Custom FBO 그 자체로는 오프스크린, 절대 자동으로 화면에 나오지 않음
- 화면 출력은 **반드시** default FBO(0) 를 거쳐야 함

### 화면에 출력하는 두 가지 방법

**방법 A — Blit (단순 복사)**

```cpp
glBindFramebuffer(GL_READ_FRAMEBUFFER, myFBO);
glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
glBlitFramebuffer(0,0,w,h, 0,0,w,h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
```

**방법 B — Texture 를 풀스크린 quad 로 다시 그리기**

```cpp
glBindFramebuffer(GL_FRAMEBUFFER, 0);
glUseProgram(postProcessShader);
glBindTexture(GL_TEXTURE_2D, myFBO_ColorTexture);
glDrawArrays(GL_TRIANGLES, 0, 3);
```

---

## 5. `gl_FragCoord.z` vs sampler — 셰이더에서 깊이 접근

| 원하는 것 | 가능한 수단 | FBO/RBO 필요? |
|---|---|---|
| **현재 픽셀의 계산된 깊이** 알기 | `gl_FragCoord.z` (내장) | ❌ |
| 현재 픽셀의 깊이 **덮어쓰기** | `gl_FragDepth = ...;` | ❌ |
| **이미 버퍼에 저장된 깊이값** 읽기 | sampler2D + Depth Texture | **✅ Custom FBO 필수** |
| stencil 값 읽기 | usampler2D + Stencil Texture | **✅ Custom FBO 필수** |
| 마우스 위치 깊이 (피킹) | `glReadPixels` (CPU) | ❌ (느림, 셰이더 미사용) |

> **저장된 깊이/스텐실 값을 셰이더로 읽고 싶다 = Custom FBO + Depth/Stencil Texture 가 사실상 유일한 경로.**

---

## 6. FBO 도입 시 추가로 떠안는 작업

| 단계 | 작업 |
|---|---|
| **초기화** | (1) FBO 생성 (2) Texture/RBO attachment 생성+부착 (3) `glCheckFramebufferStatus` |
| **리사이즈** | (4) 모든 attachment 를 새 크기로 재할당 ← GLFW 가 안 해줌 |
| **매 프레임** | (5) Pass 별 `glBindFramebuffer` (6) `glViewport` (7) `glClear` (8) 셰이더/유니폼 바인딩 |
| **정리** | (9) `glDelete*` |

→ 관리 부담은 **`Framebuffer` 래퍼 클래스 + `renderPassN()` 헬퍼 함수** 로 해결.

### 셋업 코드 골격

```cpp
// 1) Color Texture
GLuint colorTex;
glGenTextures(1, &colorTex);
glBindTexture(GL_TEXTURE_2D, colorTex);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

// 2) Depth RBO
GLuint depthRBO;
glGenRenderbuffers(1, &depthRBO);
glBindRenderbuffer(GL_RENDERBUFFER, depthRBO);
glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);

// 3) FBO 에 둘 다 부착
GLuint fbo;
glGenFramebuffers(1, &fbo);
glBindFramebuffer(GL_FRAMEBUFFER, fbo);
glFramebufferTexture2D   (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,  GL_RENDERBUFFER, depthRBO);

assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
```

---

## 7. 효과별 attachment 카탈로그

표기: `T_C` = Color Texture, `T_D` = Depth Texture, `R_C` = Color RBO, `R_D` = Depth RBO

| 효과 | Pass | Attachment 구성 |
|---|---|---|
| 화면에 그냥 그리기 | 1 | (default FBO 만) |
| 결과를 큐브 텍스처로 입히기 | 2 | Pass1: `T_C` + `R_D` |
| 블러 / 후처리 | 2~3 | 각 Pass: `T_C` (+ Pass1 이 3D 면 `R_D`) |
| 그림자맵 | 2 | Pass1: `T_D` only (`glDrawBuffer(GL_NONE)`) |
| SSAO | 2~3 | Pass1: `T_C` + `T_D` |
| MSAA | 2 | Pass1: `R_C(multisample)` + `R_D(multisample)`, Pass2: `T_C` (resolve) |
| 거울 반사 | 2 | Pass1: `T_C` + `R_D` |
| Deferred Rendering (G-buffer) | 2+ | Pass1: `T_C × N` (MRT) + `T_D` 또는 `R_D` |

---

## 8. 내가 가졌던 오해와 한 줄 반박 🎯

각 항목은 학습 중 실제로 가졌던 오해 → 정확한 반박이다.

| # | 오해 | 한 줄 반박 |
|---|---|---|
| **①** | "FBO 와 RBO 는 비슷한 거" | → **FBO 는 액자, RBO 는 도화지. 비교 대상이 아니라 짝꿍이다.** 진짜 비교는 Texture vs RBO. |
| **②** | "FBO 를 쓰면 RBO 가 반드시 필요" | → **Attachment 가 필요한 거다. Texture 든 RBO 든 하나면 된다.** |
| **③** | "화면에 렌더링하기 위한 RBO 가 필요" | → **화면용 버퍼는 GLFW 가 default FBO 안에 이미 만들어 뒀다. RBO 는 오직 내가 만든 custom FBO 의 부속.** |
| **④** | "Color 는 Texture, Depth 는 RBO 라는 규칙이 있다" | → **그건 가장 흔한 디폴트일 뿐. 6칸 모두 합법이다.** |
| **⑤** | "RBO 는 depth/stencil 전용 객체다" | → **RBO 도 Color/Depth/Stencil 다 담을 수 있다. 차이는 *내용*이 아니라 *샘플링 가능성*이다.** |
| **⑥** | "Custom FBO 에 그리면 화면에 자동으로 나온다" | → **Custom FBO 는 오프스크린. 화면 출력은 default FBO(0) 로 blit 하거나 Texture 를 다시 그려야 한다.** |
| **⑦** | "단일 패스면 RBO, 멀티 패스면 Texture" | → **기준은 패스 수가 아니라 "어느 패스의 셰이더가든 sampler 로 읽느냐" 다.** |
| **⑧** | "같은 패스 내 depth test 도 셰이더가 샘플링하는 것" | → **No. Depth test 는 GPU 의 고정 파이프라인이 자동으로 함. 셰이더는 그 과정에 끼지 않는다.** |
| **⑨** | "FBO/RBO 없이도 셰이더에서 깊이/스텐실을 읽을 수 있다" | → **셰이더가 보는 것은 `gl_FragCoord.z` (현재 픽셀의 계산값)뿐. *저장된* 값을 읽으려면 Depth Texture 가 사실상 유일한 길.** |
| **⑩** | "RBO 를 쓴다면 셰이더에서 어떻게 접근하지?" | → **접근 안 한다. 그게 RBO 의 정의이자 존재 이유다. 접근이 필요해진 순간 그건 더 이상 RBO 가 아니라 Texture 다.** |

---

## 9. 최종 4줄 요약

1. **FBO 는 컨테이너, Texture/RBO 는 그 안에 들어가는 실제 저장소.**
2. **둘의 선택 기준은 단 하나 — "셰이더가 sampler 로 다시 읽느냐".**
3. **RBO 는 셰이더 우회 경로다. 셰이더 코드엔 등장하지 않고 고정 파이프라인이 자동으로 쓴다.**
4. **Custom FBO 는 오프스크린이다. 화면 출력은 항상 default FBO(0) 를 거친다.**

---

## 관련 자료

- 본 학습은 `apps/chapter9/` (단일 셰이더 quad → 프레임버퍼 도입 단계) 작업 중 정리됨
- 챕터 설계 노트: `docs/superpowers/specs/2026-05-12-chapter9-single-shader-quad-design.md`
- 멀티 라이팅 + GL 진단 모듈은 `doc/멀티플라이팅.md` 참고
