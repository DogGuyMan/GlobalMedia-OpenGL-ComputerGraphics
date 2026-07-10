# `Context` 란 무엇인가 — 명확한 책임의 어휘

> **한 줄 요약**: `Context` 는 *잡동사니 통* 이 아니라 **"어떤 작업/시스템이 의미를 가지려면 알아야 할 환경 + 통신 채널"** 을 표현하는 정확한 어휘다.

---

## 1. 어원 — *함께 엮인 것*

라틴어 ***con-textus*** = ***con-*** ("함께") + ***texere*** ("짜다 / 엮다"). 직역 = **"함께 엮인 상태"**.

영어 일상 사용에서도:
- *"In the context of the war, ..."* = "전쟁이라는 **환경** 안에서"
- *"context-sensitive help"* = "**현재 상황** 에 맞는 도움말"

**Context 의 본질**: ***"어떤 작업/대상이 의미를 가지려면 알아야 할 주변 정보 묶음"***. 잡동사니가 아니라 **"이 작업의 환경"** 을 정확히 표현하는 어휘.

---

## 2. Context 패턴의 4 분류

모두 *공통적으로 명확한 책임 경계* 를 가짐.

| 종류 | 의미 | 대표 사례 |
|---|---|---|
| **1. Execution Context** (실행) | "*지금 이 작업이 수행 중인 환경*" | `ThreadContext`, HTTP `RequestContext`, Kotlin `CoroutineContext` |
| **2. Resource Context** (자원) ⭐ | "*외부 시스템과 통신하는 채널 + 현재 상태*" | **OpenGL Context**, **DX11 DeviceContext**, DB `ConnectionContext` |
| **3. State Context** (GoF) | "*현재 State 객체를 위임받은 host*" | GoF *State Pattern* 의 `Context` 클래스 |
| **4. Domain Context** (DDD) | "*도메인 모델의 의미 경계*" | DDD `Bounded Context` — *Auth Context vs Billing Context* |

### 공통 원칙

> **모든 정통 Context** = ***"무엇을 위한 환경인지가 이름에 명시"*** + ***"명확한 책임 경계"***

*잡동사니 통* 이 되는 건 *Context 패턴의 오용* 이지 *Context 자체의 본질* 이 아님.

---

## 3. 그래픽스 도메인에서 `Context` 의 위상

`Context` 는 *임의 단어* 가 아니라 **그래픽스 API 의 표준 어휘**. 데이터베이스의 *Connection*, 네트워크의 *Socket* 과 *같은 위상의 정통 명사*.

| 표준 | 정의 |
|---|---|
| **OpenGL Spec** | *"An OpenGL **context** represents all of the state contained by an instance of OpenGL"* — Khronos 공식 정의. **GL state 의 총 집합 = Context** |
| **DX11** | `ID3D11DeviceContext` — *"Device와의 Context"*. Device 가 *자원 생성*, Context 가 *명령 발행 + state* |
| **Unity SRP** | `ScriptableRenderContext` — *"렌더링 명령 큐잉의 환경"*. URP/HDRP 의 핵심 진입점 |
| **Java AWT** | `GraphicsContext` — *그리기 환경* |
| **WebGL** | `WebGLRenderingContext` — *브라우저의 GL Context* |

---

## 4. *잡동사니 통* 오해의 원인 + 정정

오해는 *경험적 인식* 에서 옴 — *경계가 모호한 Context 들* 의 노이즈가 원인.

| 정통 Context (명확) | 안티 Context (잡동사니) |
|---|---|
| `OpenGL Context` — GL state 총 집합 | `GameContext` — *모든 매니저 + 모든 자원* God Object |
| `DX11 DeviceContext` — GPU 통신 | `WorldContext` — *씬 + 입력 + 카메라 + 시간 + ...* |
| Spring `ApplicationContext` — DI 컨테이너 | `AppContext` (어떤 framework) — *"필요한 거 다 여기에"* |
| React `Context` — 트리 전역 state | "*Context for everything*" 안티-패턴 |

### 구분 기준

- ✅ **정통**: *Context 가 어떤 시스템의 환경인지* 가 이름/책임에 명시 (`RenderContext` = *Render 작업의 환경*)
- ❌ **안티**: *모든 자원/모든 매니저 일괄 보유* + *경계 없음*

---

## 5. 판별법 — 접두어로 보는 *정통 vs 안티*

**Context 의 *접두어* 가 핵심**.

| 접두어 패턴 | 평가 | 예시 |
|---|---|---|
| **구체적 시스템 / 작업 단위** | ✅ 정통 | `Render` + Context, `Device` + Context, `Database` + Context |
| **추상적 영역 / 전체 범위** | ❌ 안티 위험 | `Application` + Context, `Game` + Context, `World` + Context |

판별 질문:
1. *이 Context 가 어떤 작업/시스템의 환경인가?* — 한 문장으로 답할 수 있어야 정통
2. *경계 밖에 있는 것은 무엇인가?* — 명확해야 정통
3. *책임이 5개를 넘는가?* — 그렇다면 *SRP 분해 신호*

---

## 6. 우리 프로젝트의 `DeviceContext` 사례

[src/render/device_context.h](../../src/render/device_context.h) — 옛 `RenderContext` 에서 rename.

### 책임 (Resource Context — 분류 2)

| # | 책임 | 메서드 |
|---|---|---|
| 1 | bound program 추적 (state tracker) | `mBoundProgram` |
| 2 | GL 호출 단일 게이트웨이 (state 변경) | `UseProgram` / `BindVAO` / `BindTexture` / `Clear` / `SetDepthTest` / `SetBlend` |
| 3 | Draw 명령 발행 | `DrawIndexed` / `DrawArrays` |
| 4 | RenderTarget 바인딩 | `BindTarget` / `BeginFrame` |
| 5 | Default backbuffer 보유 | `mDefaultTarget` |

-> **모두 *"GPU 라는 외부 시스템과 통신하는 채널"*** — 하나의 일관된 책임. *잡동사니가 아님*.

### 왜 `RenderContext` -> `DeviceContext` 로 rename?

| 비교 | RenderContext (옛) | DeviceContext (현) |
|---|---|---|
| **정통 출처** | Unity `ScriptableRenderContext` 의 *짧은 form* | **DX11 `ID3D11DeviceContext` 정확 매칭** |
| **검색 친화** | ⭐⭐ Unity 라인 | ⭐⭐⭐ DX11 라인 (정확한 책임 매핑) |
| **DX 라인 통일** | — | ✅ `PipelineStateSetter` (DX12) 와 **일관된 DX 어휘** |
| **의미 명확성** | "Render 작업의 환경" | "Device(GPU) 와의 통신 채널" — 더 정확 |

### 정통 4-Layer 위계 안에서의 위치

```
[High Orchestrator]   SceneRenderer        ── Unreal FSceneRenderer
[Low Orchestrator]    MeshPassProcessor    ── Unreal FMeshPassProcessor
[Applier]             PropertyBlockSetter  ── Unity MaterialPropertyBlock
                      PipelineStateSetter  ── DX12 SetPipelineState
[Primitive Wrapper]   DeviceContext        ── DX11 ID3D11DeviceContext *
```

`DeviceContext` 는 **최하위 primitive 계층** — 그 위 모든 계층이 *DeviceContext 를 통해 GPU 와 통신*.

---

## 7. 한 줄 결론

> **`Context` 는 *"X 작업의 환경 + X 시스템과의 통신 채널"* 을 표현하는 정확한 어휘** — *잡동사니 통* 이 되는 건 *접두어가 추상적일 때 의 오용* 이지 *Context 자체의 본질* 이 아님.

우리 `DeviceContext` 는 `Device` 라는 *구체적 시스템 (GPU)* + `Context` 라는 *통신 채널* 어휘로 **정통 Resource Context** 의 좋은 예시.

---

## 관련 노트

- `src/render/device_context.h` — DeviceContext 클래스 선언
- `doc/api/EngineAPI.md` §3.10 `SJH::render` 모듈
- `.claude/architecture.md` §11 — 동작/데이터 분리 패턴
