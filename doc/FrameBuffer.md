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

---

# 📒 Blending / Fragment Discard / Depth Buffer / OIT 학습 노트

> migrate_demo 의 *유리창 반투명* 시나리오 (LearnOpenGL Blending / Cookbook 챕터) 작업 중 정리.
> 핵심 한 줄: **"투명 픽셀이 뒤를 가리는 건, depth test 가 alpha 를 모르고 fragment 를 통과시켜 depth buffer 를 갱신했기 때문이다."**

---

## 1단계 — 문제 제기 : 무엇이 어떻게 깨지는가

### 문제 ①  반투명 물체를 무작위 순서로 그리면 *뒤가 안 보인다*

시나리오 — Window0 (가까운 유리창), Window1 (먼 유리창) 두 장을 *카메라에서 먼 순서를 무시* 하고 그리면:

```
[draw Window0]  → depth buffer 의 해당 픽셀에 z=Z0 (가까움) 기록
                → color buffer 에 Window0 의 반투명 색 alpha-blend
[draw Window1]  → fragment depth Z1 (Z1 > Z0, 더 멈) 가 depth test 탈락
                → Window1 의 fragment 버려짐
```

**결과**: Window1 이 안 보임. 유리창인데 뒤가 안 보이는 모순.

### 문제 ②  `discard` 로는 *완전 투명* 만 해결된다

PNG 의 알파를 셰이더가 받았을 때:

| 알파 값 | discard 처리 | depth 갱신 | 결과 |
|---|---|---|---|
| `alpha = 0` (완전 투명, 창틀 구석) | ✅ `if (a < ε) discard;` | ❌ 안 함 | 뒤 픽셀 통과 — *해결* |
| `alpha = 0.5` (반투명 유리) | ❌ (그리긴 해야 함) | ✅ 함 | 뒤 픽셀 가림 — *문제 잔존* |

### 문제 ③  fragment-level 의 *순서 모순* 은 sort 로도 부족하다

- 메시 단위 sort 로 *Window0/1/2 간* 의 순서는 잡힘.
- 그러나 *한 메시 안* 의 self-overlap (오목한 알파, intersecting quad) 은 어떤 fragment 가 먼저 도착할지 알 수 없음.
- 카메라가 회전하다 두 transparent 메시가 *서로 교차* 하면 *vertex/triangle 단위 sort 도 깨짐*.

---

## 2단계 — 원인 분석 : 개념 / 이론 / API

### 개념 — *어디서 알파가 무시되는가*

```
[Vertex Shader] → [Rasterization] → [Fragment Shader] → [Depth Test] → [Blending]
                                          ↓                ↑              ↑
                                  alpha 가 계산되는 시점   여기서 alpha 무시   여기서야 alpha 가 등장
```

**핵심 — Depth Test 가 Blending 보다 *앞* 에 있다**. depth test 단계에서는 *fragment 의 z 만* 보고, alpha 는 *그 뒤 blending* 에서야 쓰인다. 따라서 *alpha 의 의미* 가 depth test 결정에 반영되지 않는다.

### 이론 — Painter's Algorithm 의 전제와 한계

**Painter's Algorithm**:
- 화가가 캔버스에 *먼 풍경 → 가까운 인물* 순서로 덧칠하듯
- GPU 에 *back-to-front* 순서로 transparent 를 그려서 alpha-blend 가 정상이 되게 함.

**전제** — *fragment-level 으로 분리 가능* 한 단위만 sort 됨.
- 메시 *간* → OK (per-mesh depth 로 sort)
- 메시 *내부* → 깨짐 (한 draw call 의 모든 fragment 가 *같은 순서로* 처리됨이 보장되지 않음)

**근본 한계**: 정렬은 *물체 단위* 인데 정합성은 *픽셀 단위* 가 필요.

### API — 각각이 무엇을 *켜고/끄는가*

| API | 동작 | 이 문제에서의 역할 |
|---|---|---|
| `glEnable(GL_DEPTH_TEST)` | depth buffer 와 비교, 통과 시 fragment 진행 | *문제의 원흉* — alpha 모르고 reject |
| `glDepthMask(GL_TRUE/FALSE)` | depth 통과한 fragment 가 buffer 에 *쓰는지* 만 분리 토글 | Transparent 만 `FALSE` 로 — depth 비교는 하지만 *덮어쓰지는 않음* → 뒤 transparent 가 자신을 가리지 않게 |
| `glEnable(GL_BLEND)` + `glBlendFunc` | depth 통과한 fragment 의 color 를 합성 | alpha-blend 가 동작하는 단계 (depth test 이후) |
| `discard;` (GLSL) | fragment 를 *완전히* 버림 | depth/color write 모두 회피 — 이진 알파 (창틀) 의 정답 |
| `gl_FragCoord.z` | *현재* fragment 의 계산된 z | 셰이더가 깊이를 *읽기* 위한 유일한 내장 (buffer 의 값 X) |

**셰이더는 depth buffer 의 *값* 을 *읽지 못한다***. *현재 fragment 의 z 만* 안다. depth 의 *비교 결과* 도 모른다 — 통과/탈락은 GPU 고정 단계가 처리.

---

## 3단계 — 해결책 3가지 + 현 프로젝트의 선택

### 해결책 A — Sort + Painter's Algorithm  *(정통, 가장 흔함)*

```
1. Opaque 먼저 그림 (front-to-back z-cull 효율)
2. Transparent 만 back-to-front sort (멀리 → 가까이)
3. Transparent draw 직전 glDepthMask(GL_FALSE) — 자기들끼리 가리지 않게
```

- **장점**: 단순, 모든 엔진 기본
- **한계**: 메시 *내부* self-overlap 해결 불가

### 해결책 B — Alpha-tested Discard  *(이진 알파 전용)*

```glsl
vec4 c = texture(uMainTex, vsTexCoord);
if (c.a < 0.01) discard;     // depth 갱신 회피 + color write 회피
fragColor = c;
```

- **장점**: sort 불필요, 단순. 잔디 / 나뭇잎 / 창틀 구석 정통
- **한계**: 부드러운 알파 (반투명 유리) 불가능 — alpha 가 0 아니면 그대로 가림

### 해결책 C — OIT (Order-Independent Transparency)  *(고급)*

| 변형 | 핵심 | 비용 |
|---|---|---|
| **Depth Peeling** | depth 를 N pass 로 *벗겨서* per-layer 누적 | N pass × N FBO |
| **Weighted Blended OIT** (McGuire 2013) | weight 함수로 단일 pass 근사 합성 | 1 pass, 약간의 부정확 |
| **Per-Pixel Linked List** | atomic 으로 fragment 를 픽셀별 리스트에 push, 셰이더가 sort 후 합성 | GL 4.2+ atomic, 메모리 대량 |

- **장점**: 순서 무관 — 정확한 transparent 합성
- **한계**: GL 4.x 의존, 메모리/시간 비용 큼, 셰이더 복잡도 ↑

### 현 프로젝트 — A + B 조합 (학습 적정)

| 구성 요소 | 위치 | 동작 |
|---|---|---|
| **Sort 인프라** | [`render_queue.cpp:89-100`](../src/render/render_queue.cpp#L89) `RenderQueue::SortMultiStage` | `queueLayer` 오름차순 → 같은 layer 내 `depth > depth` (back-to-front) |
| **Layer 컨벤션** | [`components.h`](../src/scene/components.h) `MeshRenderer::QueueLayer` | Unity 정통 — 2000 = Opaque, 3000 = Transparent |
| **Alpha discard** | 사용자 셰이더 (`texture_alpha.fs` 등) | `if (texColor.a < 0.01) discard;` 한 줄 |
| **Blend state** | [`render_context.h`](../src/render/render_context.h) `SetBlend` | `BeginFrame` 의 기본이 `SetBlend(true)` — 별도 호출 불필요 |
| **OIT** | ❌ 미구현 | 학습 범위 외 — 필요 시 별도 SP |

**사용 패턴** (migrate_demo P4 의 Window plane):
```cpp
auto win = std::make_unique<SJH::Scene::Actor>("Window0");
win->GetTransform().Translate = position;
win->AddComponent<SJH::Scene::MeshRenderer>(
    planeMesh, windowMat,
    /*queueLayer*/ 3000);   // ★ Transparent — 자동 back-to-front
dir.Root().AddChild(std::move(win));
```

→ **창틀 구석 = discard 가 처리** + **유리창 간 순서 = sort 가 처리** + **유리창 내부 self-overlap = 미해결 (학습 범위)**.

---

## 부록 : 한 눈에 보는 의사결정 트리

```
유리창 / 풀잎 / 나뭇잎 등 alpha 가 있는 텍스처?
   │
   ├─ alpha 가 *이진* (0 또는 1) 만?
   │     └─ YES → 해결책 B (discard) 단독
   │
   ├─ alpha 가 *그라데이션* (반투명)?
   │     ├─ 메시 *간* 순서만 신경 쓰면 됨?
   │     │     └─ YES → 해결책 A (sort + DepthMask=FALSE)
   │     │
   │     └─ 메시 *내부* self-overlap 도 정확해야 함?
   │           └─ YES → 해결책 C (OIT) — 비용 감수
   │
   └─ 두 종류 혼재 (창틀 + 유리)?
         └─ A + B 조합 (현 프로젝트)
```

---

## 한 줄 요약 (5줄)

1. **Depth test 가 Blending 보다 앞** — alpha 모른 채 fragment 통과/탈락 결정. *문제의 근본*.
2. **discard 는 alpha 이진 케이스의 정답** — depth 갱신 회피로 뒤 fragment 통과.
3. **반투명은 sort 필요** — Painter's Algorithm (back-to-front) + `glDepthMask(FALSE)`.
4. **fragment-level 정확도는 OIT 만 가능** — depth peeling / weighted / linked list. 비싸다.
5. **현 프로젝트는 A+B 조합** — `RenderQueue::SortMultiStage` + `queueLayer=3000` + `discard`. OIT 는 학습 범위 외.

---

# 📒 에러 핸들링 학습 노트 — Window alpha-blend 디버깅 2 케이스

> migrate_demo 의 Window plane 시각 버그 두 건 — *동일한 시각 결과* (Window 가 안 그려진다) 의 *서로 다른 원인 두 가지*. 1차 fix 가 임시 우회였고 그 뒤로도 같은 증상이 *다른 원인* 으로 재발한 진단 흐름 기록.
> 핵심 한 줄: **"증상은 같아도 원인이 다르다. 코드 흐름 끝까지 추적해야 진정한 fix."**

---

## 케이스 1 — Transparent material 이 Opaque 처럼 정렬되는 문제

### 증상

- Window 가 *정면에서는* alpha-blend OK 처럼 보임
- 카메라가 windows 사이나 뒤편으로 이동 → Window 가 *plane/box 도 덮음* (불투명한 것처럼)
- 멀리 있는 Window 가 가까운 것을 *덮어 그림* — alpha 합성 *반대* 결과

### 원인 추적 (4 단계)

#### 1차 가설 — sort 방향 부호

`RenderQueue::SortMultiStage` 의 `return a.depth > b.depth`:
- view-space z 는 카메라 forward 가 -Z 라 *카메라 앞 = 음수*. 멀수록 작은 음수.
- `a.depth > b.depth` → -1 > -10 → *가까운 것 먼저* = **front-to-back** = Painter's Algorithm 의 *반대*

**1차 fix**: 부호 반전 `<` 로 변경 → *일시적* 해결.

#### 2차 — 재발

postfx_demo / chapter 코드 정비 후 다시 같은 증상.

#### 3차 — *진정한 원인* 발견

```cpp
// MeshRenderer 의 기본값
int QueueLayer = 2000;   // Opaque 기본

// RenderSystem::CollectFromActor 의 cmd 채움
cmd.queueLayer = mr->QueueLayer;   // ← MeshRenderer 만 보고 Material 무시
```

챕터가 `mat->SetPass(Pass::Kind::Transparent)` 호출해도:
- Material.PassKind = Transparent (의도 = queue 3000)
- 그러나 *MeshRenderer 의 QueueLayer 가 기본 2000 으로 덮어씀*
- 최종 `cmd.queueLayer = 2000` → `IsTransparentQueue(2000) = false` → Opaque sort 분기 (front-to-back) → 깨짐

#### 깊은 원인 — *두 진실의 원천 충돌*

```
Material.PassKind = Transparent  →  queue 3000  ┐
                                                  ├─→ 어느 게 진실?
MeshRenderer.QueueLayer = 2000   →  queue 2000  ┘
```

같은 정보 (queue) 를 두 곳에서 결정 — *우선순위 모호* + *충돌*.

### 해결책 진화 (4 단계)

| 단계 | 접근 | 평가 |
|---|---|---|
| 1차 | sort 방향 부호 `<` 반전 | 증상 회피, 원인 미해결 (재발) |
| 2차 | `MeshRenderer.QueueLayer = 0` (sentinel — 0 이면 PassKind 사용) | 동작 OK, *책임 모호* (sentinel 우회) |
| 3차 | `MeshRenderer.QueueLayer` *제거* → `QueueOffset` 으로 의미 전환 | Unity Renderer.sortingOrder 정통 — 책임 분리 명확 |
| 4차 | `Material.PassKind` private 화 + `GetQueueLayer()` 가 `Pass::QueueOf(PassKind)` 도출 | Filament/Unreal/Cocos 정통 — 진실의 원천 *완전 단일화* |

### 최종 모델

```
Material.PassKind ──→ Pass::QueueOf(Kind) ──→ Material.GetQueueLayer() [도출 alias]
                                                            │
                                                            ▼
                                          cmd.queueLayer = GetQueueLayer() + MeshRenderer.QueueOffset
```

- **Material** = "어떤 종류" (queue base + depth/blend/cull 묶음)
- **MeshRenderer.QueueOffset** = "같은 종류 안에서 미세 순서" (Outline 의 +5 같은 경우)

### 정통 매핑

| 엔진 | "어떤 종류" | "미세 순서" |
|---|---|---|
| Unity URP | `Material.renderQueue` (auto from RenderType) | `Renderer.sortingOrder` |
| Filament | `Material.blending` | `Renderable.priority` |
| Unreal | `Material.BlendMode` | `Translucency Sort Priority` |
| **우리** | `Material.PassKind` | `MeshRenderer.QueueOffset` |

---

## 케이스 2 — Window 가 뒷면에서 *완전히 사라지는* 문제

### 증상

- 정면 (카메라가 windows 의 +Z 쪽) → alpha-blend 정상
- 카메라가 windows 의 *뒷쪽 (-Z)* 으로 이동 → **Window 가 완전히 안 보임**
- 케이스 1 fix 가 끝난 *후* 새로 드러난 증상
- 케이스 1 (덮어씀) 과 *다른 모드* — *지워진 것처럼* 사라짐

### 원인 추적

#### 1. Plane mesh 의 normal 확인

```cpp
// src/object/geometry.cpp:Plane()
BuildQuadIndexed(...,
    vmath::vec3(-0.5f, -0.5f, 0.0f),   // base — XY quad, z=0
    QUAD_FACE_INDICES);                  // counter-clockwise (front face)
```

→ Plane 은 z=0 의 XY quad. **front face normal = +Z**.

#### 2. Cull 상태 확인

`Pass::Kind::Transparent` 의 기본 `CullMode = GL_BACK` (back-face culling).

#### 3. 시나리오 매칭

| 카메라 위치 | window 가 향한 면 | `GL_BACK` cull 결과 |
|---|---|---|
| 카메라 z > window z (+Z 쪽) | front face (+Z normal 보임) | ✅ 통과 |
| 카메라 z < window z (-Z 쪽) | back face (+Z normal 등 돌림) | ❌ **culling** |

→ 카메라 위치에 따라 *back face 가 통째로 잘림*.

#### 깊은 원인 — *Transparent + Back-Cull 의 직교 충돌*

- **Back-face culling** = *솔리드 (closed) 메시* 의 성능 최적화 — 뒷면 fragment 자체 처리 skip
- **Transparent plane** (window/잎사귀/의류) = *두께 없는 면* — *어느 쪽에서 봐도 보여야* 함
- 두 의도가 *직교* 라 `GL_BACK` 기본은 *Transparent 시나리오에 부적합*

### 정통 비교

| 엔진 / 자료 | Transparent 의 Cull 기본 |
|---|---|
| LearnOpenGL Blending 챕터 | `glDisable(GL_CULL_FACE)` 명시 |
| Unity URP Lit Transparent | `Render Face = Both` (양면) |
| Filament `blending: transparent` | `doubleSided = true` |
| → 정통 결론 | **Transparent = cull off (양면)** |

### 해결

`Pass::Kind::Transparent` 의 기본 `CullMode` 를 `GL_BACK` → `0` (sentinel = cull disable):

```cpp
case Kind::Transparent:
    return State{
        /*DepthTest*/  true,      /*DepthWrite*/ false,
        /*DepthFunc*/  GL_LEQUAL, /*CullMode*/   0,       // ★ cull off — 양면
        /*BlendEnable*/true,      GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA,
        /*QueueLayer*/ QueueOf(Kind::Transparent)};
```

→ 카메라가 어디서든 *양쪽 면 모두* 그려짐.

### 직교성 보존

다른 Pass.Kind 의 CullMode 는 *각자 시나리오 정통* 유지:

| Pass::Kind | CullMode | 이유 |
|---|---|---|
| Opaque | `GL_BACK` | 일반 솔리드 메시 (뒷면 skip 성능) |
| AlphaTest | `GL_BACK` | 솔리드 + discard (잎사귀 두 면 필요 시 챕터 측 override) |
| **Transparent** | **`0` (off)** | 두께 없는 면 양쪽 표시 |
| **Skybox** | **`GL_FRONT`** | cube 안쪽 시점 — back face 가 view 향함 |

→ Pass.Kind 별 *기본값 차별화* — 변경 영향이 직교적으로 격리.

---

## 메타 학습 — 두 케이스의 *공통 패턴*

### 패턴 ① — *두 진실의 원천 충돌*
- 케이스 1: `Material.PassKind` vs `MeshRenderer.QueueLayer`
- 일반화: 같은 정보를 *두 곳에서* 결정 → 우선순위 모호 + 충돌 + 변경 시 한 쪽 망각
- **교훈**: 진실의 원천을 *단일화*. 다른 곳은 *파생/alias*. 외부 API 노출도 *단일 setter*.

### 패턴 ② — *기본값의 무게*
- 케이스 2: `CullMode = GL_BACK` 기본이 *Transparent 시나리오에 부적합*
- 일반화: *기본값* 이 *대다수 시나리오* 에 맞아야 함. 특이 케이스가 *명시 override* 해야지, 일반 케이스가 매번 *명시 override* 면 정상이 아님.
- **교훈**: 컨텍스트 별 *기본값 차별화* — `Pass::Kind::Opaque/Transparent/Skybox` 각자 다른 GL state 기본.

### 패턴 ③ — *증상 우회 vs 원인 처리*
- 케이스 1 의 1차 fix (sort 부호 반전) = *증상 우회*. 다른 코드 정비 시 재발.
- 4차 fix (PassKind private + GetQueueLayer 도출) = *원인 처리*. 진실의 원천 단일화로 *구조적으로* 재발 차단.
- **교훈**: 증상 처리는 *임시 hot fix*. *왜 그렇게 됐는지* 코드 흐름 끝까지 추적해야 다음 회귀 없음.

### 패턴 ④ — *시각 결과 같음 ≠ 원인 같음*
- 케이스 1 & 2 둘 다 시각 증상이 *"Window 가 안 그려진다"*
- 그러나 원인은 *완전히 다름* — sort/queue 충돌 vs cull 직교 충돌
- **교훈**: 시각 디버깅에서 *증상* 만으로 fix 하면 한 케이스 가려져도 다른 케이스 재발. *어느 GL state 단계에서 잘렸는가* 추적 (vertex / raster / fragment / depth / blend / write).

---

## 한 줄 요약 (4줄)

1. **케이스 1 — *queue 의 두 진실의 원천 충돌***. `MeshRenderer.QueueLayer` 가 `Material.PassKind` 의 queue 를 덮어씀. *진실의 원천 단일화* 로 해결 — Material 만 결정, MeshRenderer 는 offset 만.
2. **케이스 2 — *Transparent + back-cull 의 직교 충돌***. Plane 의 back face 가 culling 되어 카메라가 뒤편에 가면 사라짐. *Pass::Kind 별 기본값 차별화* 로 해결 — Transparent 만 `CullMode = 0`.
3. **공통 메타 패턴** — *기본값의 무게 + 진실의 원천 단일화 + 증상 vs 원인 + 같은 증상 다른 원인*.
4. **시각 디버깅 핵심** — 시각 증상만 보고 fix 하지 말고 *GL 파이프라인 어느 단계* 에서 fragment 가 잘렸는지 추적 (vertex/raster/fragment/depth/cull/blend/write). 케이스 1 = sort 단계, 케이스 2 = cull 단계 — *서로 다른 단계*.

