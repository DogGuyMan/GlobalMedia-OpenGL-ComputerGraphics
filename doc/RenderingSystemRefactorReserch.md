# 렌더링 시스템 리팩토링 — 모범 엔진 대조 연구

> 작성: 2026-06-22. 대상: [`렌더링시스템 흐름 리펙토링 계획안.md`](렌더링시스템%20흐름%20리펙토링%20계획안.md) (v2).
> 방법: Unity URP RenderGraph / Unreal RDG / Godot Compositor·RenderingServer 를 context7 MCP 로 조회, 계획안 핵심 주장과 대조.
> 표기: ✅검증(엔진 정통과 일치) · ⚠️어긋남(엔진과 반대 — 트레이드오프 명시 필요) · 💭주관 판단 · 🔵context7 출처 인용 · 🟡현 엔진(SJH).
> 확신도: **확정**(출처 직접 인용) / *대체로*(관용적 통념) / 추정(미검증, 표시).

---

## 0. 요약 (TL;DR)

| # | 계획안 주장 | 판정 | 한 줄 근거 |
|---|---|---|---|
| C1 | Pass = `IPassable` 다형 인터페이스 | ✅ **검증** | 3엔진 모두 Pass 를 추상 단위로 둠 (ScriptableRenderPass / AddPass / CompositorEffect) |
| C2 | Cull Layer(BitFlag) + Target CullingMask | ✅ **검증** | Unity cullingMask, Godot `camera_set_cull_mask` + `instance_set_layer_mask` 비트마스크 정통 |
| C3 | Pass 내부 2단 (Queue 정렬 + ROP override) | ✅ **검증** | Unity SortingCriteria, Unreal `AddDrawScreenPass(BlendState, DepthStencilState)` |
| C4 | **각 Pass 가 자기 출력 텍스처 소유 (공유 누적 아님, 완전 함수형 1-in-1-out)** | ⚠️ **어긋남** | 3엔진 모두 *transient 풀링/aliasing* 으로 메모리 **공유** — 계획안은 정반대(메모리 최대 비용) |
| C5 | **Camera 폐기 → Camera = Target(Canvas)** | ⚠️ **어긋남** | 3엔진 모두 Camera(시점·culling)와 Target(목적지 텍스처)를 *분리*, attach 로 연결 |
| C6 | 순서 SSOT = Flat `Vector` + 수동 `BeforeIndex`/`AfterIndex` | 💭 **중간** | 엔진은 선언된 I/O 로 의존을 *자동 도출*(RDG) — 수동 인덱스는 그 단순화판. 학습용으론 타당 |
| C7 | 입력 배선 안 A(BeforeIndex+`Draw(Texture)`) vs B(SetBeforePass) | ✅ **A 지지** | "입력 텍스처 선언"이 곧 RDG 의존 도출의 씨앗 — A 가 엔진 정통에 가까움 |

**핵심 메시지**: 계획안의 *구조 골격*(Pass 다형 단위 + Layer culling + 2단 내부)은 엔진 정통과 일치한다. 다만 **C4(각자 텍스처)** 와 **C5(Camera=Target)** 두 가지는 모범 엔진이 *의도적으로 반대로* 설계한 지점이므로, 채택하려면 "왜 우리는 다르게 가는가"를 트레이드오프로 명문화해야 한다.

---

## 0.5 계획안 수렴 노트 — v2 원안이 연구로 어떻게 수렴했나

> 이 연구는 계획안 v2 의 6개 주장을 4엔진(Unity/Unreal/Godot/Cocos) context7 대조로 *유지 / 교정 / 확정* 으로 수렴시켰다. 추적:

| 계획안 v2 원안 | 연구가 드러낸 것 | 수렴 결과 |
|---|---|---|
| C1 Pass=`IPassable` 다형 | 4엔진 정통(ScriptableRenderPass/AddPass/CompositorEffect/RenderCommand) | ✅ **유지** |
| C2 대상 BitFlag + "**Target** CullingMask" | 마스크는 *시점(Camera)* 속성 (Unity/Godot/Cocos 전부) | 🔧 BitFlag 유지, "Target"→**Camera 귀속** 교정 |
| C3 Pass 내부 2단(정렬+ROP) | Unity SortingCriteria + Unreal Blend/DepthStencil | ✅ **유지** |
| C4 각자 텍스처(1-in-1-out) | 엔진은 transient **aliasing 공유** | 🔧 `RenderTargetPool` seam + `DedicatedTargetPool`(지금)/`TransientTargetPool`(나중) |
| C5 **Camera 폐기 = Target** | 3/4 엔진이 Camera 유지 + Target 분리(attach) | 🔧 **Camera 유지**, Target 분리, "**Stage**" 명칭만 폐기 |
| C6 Flat Vector + 수동 인덱스 | RDG 는 I/O 로 자동도출; 학습엔 수동 타당 | ✅ 유지, "지역성"→"**단순성**" 근거 교정 |
| C7 입력 배선 A vs B (열린) | 엔진(UseTexture 선언)이 A 지지 | ✅ **A 확정** |

**네이밍 수렴**: `OwningProvider`/`PoolingProvider`(1차 내 임시) → `DedicatedTargetPool`/`TransientTargetPool`(엔진 정통, §6). `View`(Camera 개명안, 내 1차 제안) → **`Camera` 유지로 철회**(§6.1).

> 핵심: 계획안의 *골격은 정통* — 교정된 것은 **세 군데**뿐. ① cullMask 의 귀속처(Target→Camera, C2/C5), ② 텍스처 소유 모델(각자→seam 뒤 교체 가능, C4), ③ "Camera 폐기"의 과잉(→ "Stage" 명칭만 폐기, C5). 나머지는 유지/확정.

---

## 1. 모범 엔진 렌더 파이프라인 — context7 조회 요약

### 1.1 Unity URP — RenderGraph

🔵 출처: `docs.unity3d.com/Manual/urp/render-graph-*`

- **Pass 단위**: `ScriptableRenderPass`(추상) 를 상속, `RecordRenderGraph(RenderGraph, ContextContainer)` override. `renderer.EnqueuePass(pass)` 로 등록.
- **순서**: `RenderPassEvent`(예: `AfterRenderingOpaques`, `AfterRenderingPostProcessing`) 라는 *고정 삽입 지점* + EnqueuePass 순서. → 정렬된 리스트.
- **입력/출력 선언**: `builder.SetRenderAttachment(tex, idx, AccessFlags.Write)`(출력), `builder.UseTexture(tex, AccessFlags.Read)`(입력). → 패스가 읽고/쓰는 텍스처를 *명시적으로 선언*.
- **리소스 종류** (🔵 직접 인용):
  - *Internal(transient)* — "created within a render pass, managed by the system, cannot be accessed outside ... or passed between frames/cameras." 단일 프레임용.
  - *External(imported)* — "existing assets like the camera back buffer ... lifetime is not managed by the render graph." `renderGraph.ImportTexture(...)`.
- **메모리 최적화** (🔵 직접 인용): "optimizes GPU memory by **reusing textures with similar properties**", "avoids allocating unused resources, **removes render passes whose output is not used**", 타일 기반 플랫폼에선 "merge passes into a single native pass."

### 1.2 Unreal — RDG (Render Dependency Graph)

🔵 출처: `dev.epicgames.com/.../render-dependency-graph-in-unreal-engine`

- **Pass 단위**: `FRDGBuilder::AddPass(RDG_EVENT_NAME(...), PassParameters, ERDGPassFlags, lambda)`. 기록 후 `GraphBuilder.Execute()` 로 일괄 실행 — "deferred until Execute. May execute in **parallel** with other passes."
- **입력/출력 선언 → 의존 자동 도출** (🔵 직접 인용): "Pass parameter structs extend shader parameter structs with RDG resources, **enabling RDG to derive dependencies** and manage transient resource lifetimes." 셰이더 파라미터 구조체의 `SHADER_PARAMETER_RDG_TEXTURE`(읽기) / `_UAV`(쓰기) 선언이 곧 그래프 엣지.
- **Transient 리소스** (🔵 직접 인용): "RDG uses a **transient resource allocator** during graph compilation ... allowing resources with **disjoint lifetimes to share memory** ... significantly reduce the GPU memory watermark." `CreateTexture` 는 "No GPU memory is allocated at this point, just the descriptor" — 실제 메모리는 그래프 컴파일이 aliasing 결정 후 배정.
- **스크린 패스**: `AddDrawScreenPass(..., InputViewport, OutputViewport, VertexShader, PixelShader, BlendState, DepthStencilState, ...)` — 입력/출력 뷰포트 + ROP 상태(Blend/DepthStencil)를 인자로.

### 1.3 Godot — Compositor + RenderingServer

🔵 출처: `godotengine/godot-docs` (`class_compositoreffect`, `tutorials/rendering/compositor`, `class_renderingserver`)

- **Pass 삽입**: `CompositorEffect`(추상) 의 `_render_callback(effect_callback_type, render_data)` override. `effect_callback_type` 가 *고정 5단계 삽입 지점* (`PRE_OPAQUE` → `POST_OPAQUE` → `POST_SKY` → `PRE_TRANSPARENT` → `POST_TRANSPARENT`).
- **버퍼 접근 (in-place)**: `render_scene_buffers.get_color_layer(view)` 로 *공유* 씬 컬러/뎁스 버퍼를 직접 읽고 compute 로 in-place 갱신 — 효과마다 자기 텍스처를 만들지 않음.
- **Camera ≠ Viewport ≠ Target** (🔵 직접 인용, 3개의 별도 RID):
  - `viewport_attach_camera(viewport, camera)` — 카메라를 뷰포트에 *부착*(둘은 별개).
  - `viewport_get_render_target(viewport)` — 뷰포트의 *렌더 타깃* RID.
  - `viewport_get_texture(viewport)` — 렌더 결과 텍스처 RID.
  - `camera_set_cull_mask(camera, layers)` — culling 은 *카메라* 속성.
  - `instance_set_layer_mask(instance, mask)` — 대상의 레이어(= `VisualInstance3D.layers`).

### 1.4 Cocos2d-x — Renderer + RenderQueue + Camera

🔵 출처: `/cocos2d/cocos2d-x` (v4)

- **시점**: `Camera` — `Scene::_cameras` 에 `onEnter()` 시 자가 등록(🔵 `Camera::setScene`). culling 은 `setCameraMask`/`CameraFlag`(비트) + Node 별 cameraMask, light 는 `lightmask`/`getLightFlag()`. → **마스크가 시점(Camera) 쪽** = C2 축 A 4번째 엔진 재확인.
- **그리는 단위/순서**: `Renderer` + `RenderQueue` + `RenderCommand`(`MeshCommand` 등). 계획안의 "Queue Layer 정렬" = Cocos `RenderQueue` 정통.
- **렌더 타깃**: `RenderTexture`(off-screen RTT) / `experimental::FrameBuffer`.
- **Material 계층**: `Material → Technique → Pass` (🔵 `_material->getTechniques()` / `technique->getPasses()`). ⚠️ Cocos 의 `Pass` 는 *셰이더 패스*(material 내부)이지 *렌더 패스*가 아님 — 계획안 `IPassable`(렌더 패스)와 **다른 층**. 이름 충돌 주의.

---

## 2. 주장별 상세 검증

### C1 — Pass = `IPassable` 다형 인터페이스 ✅ 검증

세 엔진 모두 "Pass" 를 *추상 단위 + 구현 override* 로 다룬다 (Unity `ScriptableRenderPass`, Unreal `AddPass` lambda, Godot `CompositorEffect`). `design-decision-discipline` 의 다형성 3조건 점검:

1. 공통 인터페이스 의미 있음 — `Draw()`/`GetPassResult()` 가 모든 Pass 에 합리적. ✅
2. 호출자가 base 로 dispatch — `PassIterator` 가 `passes[i].Draw()`. ✅
3. runtime swap 실재 — 매 프레임 Pass 집합이 추가/제거(Scene→Render Hierarchy 재구축). ✅

→ 세 조건 충족. **다형 도입 정당**. (현 엔진 `IRenderPassable` 와 동일 방향 — 이름만 통일됨.)

### C2 — Cull Layer + CullingMask ✅ 검증

🔵 Godot `camera_set_cull_mask(camera, int layers)` + `instance_set_layer_mask(instance, int mask)` = 정확히 계획안의 "대상 BitFlag ∧ Target CullingMask" 구조. Unity cullingMask 비트마스크도 동일. → **비트마스크 culling 은 정통**, 일반화 타당.

#### 두 개의 독립된 축 — 혼동 주의

이 주제는 *서로 다른 두 질문*이 겹쳐 있다. 분리해서 봐야 한다.

**축 A — 마스크는 *어느 쪽*에 붙나? (Unity·Godot 동일)**

🔵 Unity Culling Mask 는 **Camera** 속성(`class-Camera.html`), Light 도 자기 cullingMask 보유("어느 레이어를 비출지"). 🔵 Godot `camera_set_cull_mask` 도 **Camera** RID 속성. → 두 엔진 다 마스크를 *시점(보는 쪽)* 에 단다.

> **손전등 비유**: "이 손전등(시점)이 빨강·파랑 물건만 비춘다"는 자연스럽지만, "이 벽(목적지 텍스처)이 빨강·파랑만 받는다"는 어색하다. **마스크는 *보는 쪽*의 필터**이지 *그려지는 표면*의 속성이 아니다.

→ ⚠️ 계획안은 이걸 "**Target** CullingMask"라 부른다. 하지만 마스크는 Target(목적지 텍스처)이 아니라 **View(시점)** 에 귀속돼야 한다. 계획안이 Camera 와 Target 을 한 덩어리로 합치면서(C5) 필터가 잘못된 절반(Target)에 붙은 것 → **C5 의 "Camera/Target 분리" 교정과 같은 뿌리**. 분리하면 CullingMask 는 자연히 View 쪽으로 간다.

**축 B — 객체는 *몇 개* 레이어에 속하나? (Unity ↔ Godot 다름)**

| | 객체(대상) 쪽 | 시점(Camera) 쪽 | 가시성 판정 |
|---|---|---|---|
| **Unity** 🔵 | `gameObject.layer` = **단일 int** (32개 중 *하나*) | `cullingMask` = 비트마스크(집합) | `(1 << obj.layer) & cam.cullingMask` |
| **Godot** 🔵 | `instance` layer_mask = **비트마스크** (*여러* 레이어 동시 소속) | `cull_mask` = 비트마스크(집합) | `obj.layer_mask & cam.cull_mask` |

🔵 Unity: `gameObject.layer = LayerMask.NameToLayer("UI")` — 객체는 정확히 *한* 레이어. 🔵 Godot: `instance_set_layer_mask(instance, mask)` — 객체가 *여러* 레이어 동시 소속 가능. 시점 쪽은 둘 다 비트마스크.

💭 판단: Godot 모델(양쪽 다 집합 → 순수 `AND`)이 더 **대칭/직교**적이고 유연하다(다중 소속). Unity 의 단일 소속은 추론은 쉽지만(이건 UI, 저건 Enemy) 32개 한계 + 다중 불가의 알려진 불편이 있다. **계획안 ③의 "Cull Layer (BitFlag)"= 대상이 비트마스크 → 이미 Godot 모델 채택**이며 일관됨. 이 축은 손댈 것 없음 — 교정 대상은 *축 A*(명명: Target→View)뿐이다.

### C3 — Pass 내부 2단 (정렬 + ROP) ✅ 검증

🔵 Unreal `AddDrawScreenPass(..., FRHIBlendState*, FRHIDepthStencilState*, ...)` — ROP 상태를 패스 인자로 받음. Unity 는 SortingCriteria 로 정렬 + RenderStateBlock. 계획안의 "① Queue Layer+Z 정렬 → ② Scissor/Stencil/Depth/Blend/ColorMask override" 2단은 정통. 현 엔진 `MeshPassProcessor::SortMultiStage` + `Pass::RenderStateBlock` 이 이미 이 구조.

### C4 — 각 Pass 자기 텍스처 소유 ⚠️ **어긋남 (가장 중요)**

**계획안**: "완전 함수형 1-in → 1-out. World/Skybox/Particle 도 *각자 텍스처*를 가짐 (누적 공유 아님)."

**엔진 정통 (정반대)**:
- 🔵 Unreal: transient allocator 가 "resources with **disjoint lifetimes to share memory**" — 수명이 안 겹치는 패스끼리 *메모리를 공유(aliasing)*. 의도가 "reduce GPU memory watermark".
- 🔵 Unity: "optimizes GPU memory by **reusing textures with similar properties**". Internal 리소스는 단일 프레임 + 시스템이 풀 관리.
- 🔵 Godot: 효과가 *공유* 씬 버퍼에 in-place.

**함의 (메모리 비용 정량)**:

| 방식 | N개 Pass 의 FBO/텍스처 | 1080p RGBA8 기준 (≈8MB/장) |
|---|---|---|
| 계획안 "각자 텍스처" | **N장 동시 상주** | 8 Pass → ≈64MB 상주 |
| 엔진 aliasing 풀 | 동시 *생존* 수만큼 (보통 2~3장 ping-pong) | ≈16~24MB |

💭 **판단**: "각자 텍스처"는 *디버깅·개념 단순성*(각 Pass 결과를 언제든 그대로 들여다봄)에서 이점이 있고, 학습/소규모 데모에선 메모리 64MB 도 무해하다 (`design-decision-discipline` YAGNI — aliasing 풀은 *지금* 필요 없음). 단 **모범 엔진과 반대 방향임을 명시**하고, "성능 확장 시 transient 풀로 진화" 트리거를 적어둘 것. 그러지 않으면 나중에 "왜 우리만 메모리를 N배 쓰지?" 라는 잘못된 디버깅이 생긴다.

> 🟡 현 엔진과의 정합: 현 PostFX `PassComponent` 는 이미 FBO→FBO *ping-pong*(공유 2장 재사용)에 가깝다. 계획안의 "각자 텍스처"는 오히려 현 엔진보다 메모리를 더 쓰는 방향 — 이 점도 트레이드오프로 인지 필요.

#### C4 결정 — `RenderTargetPool` seam + 런타임 토글 (Unreal transient allocator 정통)

C4 의 "각자 vs 공유" 는 **Pass 가 출력 텍스처를 *어디서 얻느냐*** 의 문제이지 Pass 자신의 문제가 아니다 → 바꿔 끼울 seam 은 **`RenderTargetPool`** (Pass 에게 출력 RT 를 대여; 🔵 Unreal `IPooledRenderTarget`/Unity RTHandle 정통). Pass(`IPassable`) 코드는 두 모드에서 **동일**.

| 구현 | 메모리 | Unreal 대응 | 용도 | 도입 |
|---|---|---|---|---|
| **`DedicatedTargetPool`** | Pass 당 전용 (N장 동시 생존) | `r.RDG.TransientAllocator` **OFF** | 시각 디버깅 — 모든 중간 결과 살아있어 blit 으로 확인 | ✅ **지금 (이것만)** |
| **`TransientTargetPool`** | 수명 안 겹치면 공유(aliasing) | `r.RDG.TransientAllocator` **ON** | 릴리즈 메모리 최적화 | 미래 (측정 후) |

**확정 사항**:
1. **컴파일 타임 Debug/Release 분리 ❌ → 런타임 토글 ✅** (모든 빌드 공통). 이유: provider 호출은 프레임당 ~Pass 수(≈8회)뿐이라 성능 무관(💭 가상 호출 수 ns); 컴파일 분리는 *얻는 것 0 + aliasing 버그를 Release 에서만 터지게 하는 함정*. → 평소 Dedicated, 필요 시 토글 = 🔵 Unreal cvar 용법 그대로.
2. **`DedicatedTargetPool` 단독 구현** — 목표 ①(매 프레임 각 Pass 결과 육안 확인)이 *이것을 요구*(모든 텍스처가 프레임 끝까지 생존). `TransientTargetPool` 은 그 디버깅과 *상충*(메모리 재사용 시 보려는 순간 덮어써짐)하므로 더더욱 나중.
3. **seam 만 먼저, 구현은 하나** — `RenderTargetPool` 인터페이스만 박아두면 `TransientTargetPool` 추가가 *비파괴적*(`design-decision-discipline §4` YAGNI 절충).
4. **시각 디버깅 = 디버그 오버레이** — PassIterator 루프 끝에서 선택 인덱스의 `GetPassResult()` 를 화면에 blit(F1~F8 식 전환). 런타임 변수 하나, Pass/Pool 코드 무수정.

> ⚠️ **오해 교정 — "단일 공유 1장" 불가**: `Draw(before)` 는 입력을 *읽으며* 자기 출력에 *쓴다* → 입력·출력 동시 생존이라 같은 텍스처일 수 없음. 최적화(`TransientTargetPool`)도 최소 **ping-pong 2장**, 일반적으로 *동시 생존 최대 수* N장. aliasing 은 "N→1" 이 아니라 "*수명 안 겹치는* 자원끼리만 재사용"(🔵 Unreal "disjoint lifetimes to share memory").

### C5 — Camera 폐기 → Camera = Target ⚠️ **어긋남**

**계획안 [ISSUE]**: "카메라=눈, 눈은 자기 자신을 못 봄 → Stage 명칭 어긋남. Camera 폐기 → Target(Canvas) 중심. 카메라는 View/Proj 가진 Target 일 뿐."

**엔진 정통 (분리 유지)**:
- 🔵 Godot 은 **Camera / Viewport / RenderTarget 을 3개의 별도 RID** 로 두고 `viewport_attach_camera` 로 *연결*. culling 은 Camera, 결과 텍스처는 `viewport_get_texture`.
- 🔵 Unity Camera 는 `cullingMask`(무엇을 볼지) + `targetTexture`(어디에 그릴지)를 *둘 다* 가지지만 이는 "Camera 가 Target 을 *가리킨다*"이지 "Camera = Target" 이 아님.

💭 **판단**: 계획안의 *문제 진단*("Stage(무대) 은유가 카메라에 안 맞다")은 **타당**하다. 하지만 *해법*("Camera 를 Target 으로 흡수")은 엔진 정통과 어긋난다. 엔진은 두 개념을 **분리한 채 attach** 로 해결한다:

- **Camera/View** = *시점* — View·Proj 행렬 + CullingMask. "무엇을, 어느 각도에서".
- **Target/Canvas** = *목적지 표면* — FBO/Texture. "어디에 픽셀이 쌓이나".
- 한 Camera 가 여러 Target 에, 한 Target 이 여러 Camera 결과를 받을 수 있어 1:1 이 아님 → 합치면 표현력 손실.

→ 💭 권고 (§6.1 네이밍 재확인 반영): **`Camera` 이름은 *유지***. 4엔진 중 3(Unity `Camera`/Godot `Camera3D`/Cocos `Camera`)이 시점을 그대로 "Camera" 라 부르고 Unreal 만 `FSceneView` — 다수 정통은 `Camera`. **폐기 대상은 "Camera"가 아니라 "Stage" 명칭**이다. 핵심은 *시점(`Camera`)과 `Target` 을 분리 유지*하고 attach 로 잇는 것. CullingMask 는 이 `Camera` 에 귀속(C2 축 A — Unity cullingMask / Godot camera_set_cull_mask / Cocos setCameraMask 전부 카메라 속성).

> 🔵 **RID 란?** Godot 의 `viewport_attach_camera(viewport: RID, camera: RID)` 에서 RID(Resource ID)는 *서버 내부 자원을 가리키는 불투명 핸들* — GL 의 `GLuint`, Vulkan 의 `VkImage`, Unreal 의 `FRHITexture*` 와 동격. **두 개의 별도 RID 를 받는다는 사실 자체**가 "Godot 에서 Camera 와 Viewport(Target)는 별개 자원" 이라는 증거(한 덩어리였다면 핸들이 하나).

#### C5 종합 — Pass = 3 직교축의 *결합자* (★ 핵심 통찰)

C5(View↔Target 분리) + C2(cullMask=시점 속성) + C1(Pass 다형) 을 합치면, **하나의 Pass(IPassable)는 4번째 명사 축이 아니라 *3개의 직교 명사 축을 하나씩 골라 묶는 결합자(동사)*** 라는 구조가 드러난다. 계획안이 처음 "Stage = 어떤 IRenderable 을 그릴지" 로 본 것은 *3축 중 1축만* 본 것.

```
IPassable  (= Pass, 결합자 / 동사 "그려라")
   ├─ IRenderable[]  (대상 — "무엇을")          ┐
   ├─ View*          (시점 — "어느 눈/각도로")    ├─ 3 직교 명사 (입력)
   └─ Target*        (표면 — "어디에 쌓나")       ┘
   + ROP override    (2단계: scissor→stencil→depth→blend→colormask)

Draw() = rasterize( IRenderable[] 를 View 행렬로 ) → Target 에 ROP 적용해 씀
```

🔵 세 엔진 모두 Pass 안에 셋이 다 들어있음: Unity `ScriptableRenderPass`(camera data=**View** + `SetRenderAttachment`=**Target** + 레이어로 거른 renderers=**IRenderable**), Unreal `FMeshPassProcessor`(`FSceneView`=**View** + mesh draw command=**IRenderable** + render target=**Target**), Godot 콜백(viewport camera + scene buffers + 보이는 instances).

**미묘함 — 셋이 한 곳에서 연결된다 (cullMask = View→IRenderable 선택자)**:

```
실제 그릴 대상 = IRenderable 풀  ∧  (renderable.layerMask & View.cullMask)
                  └ 무엇이 존재하나(독립)     └ 시점이 거른다(View 에 종속) — C2 축 A
```

IRenderable *풀*(씬에 뭐가 있나)은 View/Target 과 독립이지만, *"이 Pass 가 실제로 그리는 부분집합"* 은 View.cullMask 가 선택한다. → cullMask 가 Target 이 아니라 **View** 에 있어야 하는 이유의 코드적 근거.

**표현력 — 왜 3축이어야 하나** (한 축만 바꿔 끼워 모든 Pass 표현):

| Pass | 대상 (IRenderable) | 시점 (View) | 표면 (Target) |
|---|---|---|---|
| WorldPass | 씬 메시들 | 메인 perspective | output A |
| MiniMapPass | *같은* 씬 메시들 | 탑다운 ortho | output B |
| PostFxPass | 풀스크린 quad 1장 | screen ortho | 다음 Target |

→ 셋 다 같은 `IPassable` 인데 *어느 축을 바꿔 끼우냐*로 World/MiniMap/PostFx 가 갈린다. "Stage = IRenderable 한 축" 으로만 보면 *같은 대상을 다른 시점/표면에 그리는* 경우(메인+미니맵, World→PostFx)를 표현 못 한다 — **3축 결합자로 봐야 표현력이 산다**.

### C6 — Flat Vector + 수동 before/after 인덱스 💭 중간

**계획안**: 순서 SSOT = `Vector<IPassable*>`, before/after = *인덱스*(지역성). PassGroup·템플릿메서드 폐기, PassIterator 가 직접 루프.

**엔진 정통**:
- 🔵 Unreal RDG: 순서를 *수동 리스트로 두지 않고* 선언된 리소스 의존에서 **자동 도출** ("enabling RDG to derive dependencies"). 병렬 실행까지 그래프가 스케줄.
- 🔵 Unity: `RenderPassEvent` 고정 슬롯 + EnqueuePass 순서 (정렬 리스트) + 리소스 의존(UseTexture/SetRenderAttachment).
- 🔵 Godot: `effect_callback_type` 고정 enum 슬롯.

💭 **판단**: 계획안의 "수동 BeforeIndex" 는 *Unreal 이 자동 도출하는 의존 엣지를 손으로 적는* 형태다. 즉 `BeforeIndex` = "나는 passes[k] 의 출력을 입력으로 읽는다" = RDG 의 `UseTexture(read)` 한 줄과 동치. 학습 엔진 규모에선 그래프 컴파일러를 만들 이유가 없으므로(`design-decision-discipline` YAGNI) **수동 인덱스는 합리적 단순화**다. 다만:

- 🟡 "Flat Vector + 지역성" 주장은 *연속 메모리 = 캐시 친화*가 맞지만, Pass 수가 한 자릿수(8 내외)인 본 엔진에선 캐시 효과는 측정 불가 수준 — **지역성은 부차적 근거**, 진짜 이점은 "PassGroup 계층 제거로 인한 단순성"이다. 근거를 "지역성"보다 "평면 구조의 가독성/유연 배치"로 두는 게 정직하다 (`confidence-and-sourcing` — 검증 불가한 성능 주장 격하).
- 향후 Pass 가 많아지고 비-인접 다중 입력(blend 가 T0+T3)이 흔해지면 → 인덱스 배열(`vector<int> inputs`)로 확장, 그때가 mini-RDG 도입 트리거.

#### C6 정리 — 3 층으로 분해 (혼동 방지)

"이전 Pass 결과에 접근하는가" 질문은 *세 층*이 겹쳐 있다. 분리하면 명확:

**층 1 — 접근 수단 있나? → ✅ 있다.** Bloom 이 World 출력을 입력으로 받아야 하므로 당연. 없으면 체인 불성립.

**층 2 — 포인터냐 인덱스냐? → 인덱스 (계획안 선택).**

```cpp
// ① 포인터(링크드리스트식) — 계획안이 안 씀
IPassable* beforePass;  Texture in = beforePass->GetPassResult();

// ② 인덱스(Vector 위치) — 계획안 선택 ✅
int BeforeIndex = 1;    Texture in = passes[BeforeIndex]->GetPassResult();   // ← 이전 Pass 접근 지점
```

PassIterator 해석:
```cpp
std::vector<IPassable*> passes = { &skybox, &world, &bloom, &imgui };  // idx 0,1,2,3
bloom.BeforeIndex = 1;   // bloom 입력 = world(passes[1]) 출력
for (int i = 0; i < passes.size(); ++i) {
    Texture in = (passes[i]->BeforeIndex < 0) ? sceneRaw
               : passes[ passes[i]->BeforeIndex ]->GetPassResult();   // ★ 접근
    passes[i]->Draw(in);
}
```
→ 접근은 **있고**, raw 포인터가 아니라 **Vector 인덱스**로 한다. 💭 인덱스의 진짜 이점은 "지역성"(8 Pass 에선 측정 불가)이 아니라 **Vector=순서 SSOT + 직렬화/재배치 용이**.

**층 3 — 인덱스를 누가 정하나? → 사람이 손으로 (vs Unreal 자동).**

```cpp
// 본 엔진(수동): 사람이 적음        bloom.BeforeIndex = 1;
// Unreal RDG(자동): "뭘 읽는지"만 선언  PassParameters->SceneColorTexture = SceneColor;
//   → RDG 가 "누가 SceneColor 썼지?" 추적해 순서·의존 자동 도출 (BeforeIndex 손으로 안 적음)
```
즉 `BeforeIndex` 한 줄 = Unreal 의 자동 의존 도출을 *수동 대체*. 학습 규모에선 그래프 컴파일러 오버킬(YAGNI) → 수동 인덱스 합리적.

#### C6 후속 — "data-driven" 의 두 직교 축 (별개 자동화)

"추후 data-driven 화" 안에 *서로 다른 두 자동화*가 섞여 있다. 갈라야 함:

| 축 | 질문 | 지금 | 다음 |
|---|---|---|---|
| **A. 배선이 *어디* 사나** | 코드 vs 데이터 | C++ 리터럴 `BeforeIndex=1` | **JSON** `{"bloom":{"before":1}}` ← 이게 "data-driven" |
| **B. 순서를 *누가* 정하나** | 사람 vs 프레임워크 | 사람(인덱스 명시) | 자동 도출(mini-RDG, I/O 선언서 추론) |

→ **"data-driven" = 보통 축 A** (배선 외부화). 축 B(자동 도출)와 *직교* — 조합 가능(data-driven + 수동 BeforeIndex, 또는 data-driven + 자동도출).

🔵 **엔진 정통(축 A 검증)**: Unity `ScriptableRendererData`= ScriptableObject(**데이터 에셋**)로 Pass 구성, Godot `CompositorEffect`= Resource(.tres **데이터**)로 편집. → "Pass 파이프라인을 데이터로 기술" 은 정통.

**진화 경로**:
```
① 코드 수동    bloom.BeforeIndex = 1;             ← 지금 (학습)
② 데이터 수동  passes.json: [{name, before, ...}]  ← data-driven (순서는 여전히 명시)
③ 자동 도출    "SceneColor 읽어" 선언만 → 순서 추론   ← mini-RDG (대형 엔진)
```
세 단계 모두 **같은 PassIterator + BeforeIndex seam** 위에서 진화 — ②는 "Vector 를 *코드*로 채우던 걸 *JSON 파서*로" 바꿀 뿐 Pass/Iterator 무수정.

💭 **트리거 (YAGNI)**: ②(data-driven)는 *재컴파일 없이 파이프라인 변경*이 필요하거나 *에디터로 파이프라인 편집*(→ 본 프로젝트의 마스터데이터/WebEditor 외부화 방향과 합류)할 때 정당화. Pass 가 고정 8개면 ①로 충분. ③(자동도출)은 더 나중.

### C7 — 입력 배선 채널 A vs B ✅ A 지지 (엔진 근거)

계획안 열린 결정: **A**(`int BeforeIndex` + `Draw(Texture before)`) vs **B**(`void Draw()` + `SetBeforePass(Texture)`).

🔵 **엔진 근거가 A 를 지지**: Unreal/Unity 모두 패스가 *입력 텍스처를 명시 선언*(`UseTexture(read)` / `SHADER_PARAMETER_RDG_TEXTURE`)하고, 프레임워크가 그 선언을 읽어 실제 리소스를 *주입*한다. 이는 계획안 A 의 "`BeforeIndex` 로 입력을 선언 → PassIterator 가 해석해 `Draw(before)` 로 주입"과 정확히 같은 모양이다. B 의 "직전 패스를 setter 로 밀어넣기"는 비-인접 입력을 못 다루고(엔진은 다중 입력이 일반), 선언적 의존 도출의 길도 막는다.

→ 💭 **A 채택 권고** (엔진 정통 + 향후 다중 입력 확장 경로 보존). `void Draw()` → `Draw(const Texture& before)` 시그니처 양보는 그만한 가치가 있음.

---

## 3. 종합 권고 (옵션표 + 추천)

### R1 — C4(각자 텍스처) 처리 — **결정됨** (상세는 C4 §C4 결정)

**seam `RenderTargetPool` 도입 + 구현 2종 (둘 다 같은 인터페이스, Pass 코드 무관)**:

| 구현 | 형태 | Unreal 대응 | 도입 |
|---|---|---|---|
| **`DedicatedTargetPool`** ⭐ | Pass 당 전용 텍스처 (N장 상주) — 시각 디버깅 + 단순 + 함수형 순수 | transient allocator **OFF** | ✅ **지금 (단독)** |
| **`TransientTargetPool`** | 수명 안 겹치면 메모리 공유(aliasing) — 엔진 정통 최적화 | transient allocator **ON** | 미래 (메모리 측정 후) |

**확정**: ① seam 만 먼저 박고 `DedicatedTargetPool` 하나만 구현(YAGNI 절충) ② Debug/Release 컴파일 분리 ❌ → *런타임 토글*(모든 빌드, Unreal cvar 정통) ③ 시각 디버깅 = PassIterator 끝 디버그 오버레이(선택 인덱스 `GetPassResult()` blit) ④ "단일 공유 1장" 불가 — 최소 ping-pong 2장(입력·출력 동시 생존). spec 에 "모범 엔진은 `TransientTargetPool`(aliasing) 사용 — 우리는 학습/디버깅 우선 `DedicatedTargetPool`, 메모리 N배는 의도된 트레이드오프, 측정 후 진화" 를 *Future* 로 명문화.

### R2 — C5(Camera=Target) 처리

| 옵션 | 형태 | 추천 |
|---|---|---|
| **R2-a `Camera`/Target 분리 유지** | `Camera`(View·Proj·CullingMask) + `Target`(FBO) 별개, attach 관계 | ⭐ **추천** (엔진 정통) |
| R2-b 완전 통합 | 계획안 그대로 Camera=Target | 표현력 손실 (1:N 불가) |

**추천: R2-a**. **`Camera` 이름은 유지**(4엔진 중 3이 `Camera`; §6.1) — 폐기 대상은 "**Stage**" 명칭뿐. `Camera`(시점) + `Target`(표면) 분리 + attach. CullingMask 는 `Camera` 에 귀속. → 계획안 [ISSUE] 의 문제의식(Stage 은유 부적합)은 살리되, 해법만 엔진식(`Camera` 유지 + `Target` 분리)으로 교정.

### R3 — C6/C7 (Vector + 배선)

- ✅ Flat Vector + PassGroup 폐기 유지 (단순성). 근거를 "지역성"→"평면 구조 단순성"으로 정직화.
- ✅ **입력 배선 안 A 확정** (BeforeIndex 선언 + `Draw(Texture)`).
- 💭 `BeforeIndex` 단일 → 향후 `vector<int> Inputs` 다중 입력 확장 트리거 명시.

---

## 4. 계획안에 반영할 수정 포인트 (체크리스트)

- [ ] **C4**: "각자 텍스처" 옆에 ⚠️ 트레이드오프 박스 — "모범 엔진은 transient aliasing 으로 메모리 공유(Unreal `r.RDG.TransientAllocator`, Unity texture reuse). 우리는 학습 우선 각자 텍스처, 의도된 N배 비용."
- [ ] **C5**: [ISSUE] 해법 교정 — "Camera 폐기" 가 아니라 "**Camera 유지**(4엔진 중 3) + `Target` 분리(attach) + **\"Stage\" 명칭만 폐기**". Godot 3 RID 분리 근거 인용.
- [ ] **C2**: CullingMask 를 Target 이 아니라 *`Camera`(시점)* 속성으로 재배치 (C5 교정과 연동).
- [ ] **C6**: "지역성" 근거를 "평면 구조 단순성"으로 하향, 다중 입력 확장 경로(`vector<int>`) 추가.
- [ ] **C7**: 안 A 를 ✅ 확정으로 승격 (엔진 정통 근거 첨부).
- [ ] **§6 네이밍**: `View`→`Camera` 유지, `Canvas`→`Target`(Unity Canvas=UI 혼동 회피), 풀은 `RenderTargetPool`/`DedicatedTargetPool`/`TransientTargetPool` 통일.
- [ ] 신규 [참고 출처] 행: 이 연구 문서 + context7 4엔진 라이브러리 ID.

---

## 5. 출처 (context7)

| 엔진 | 라이브러리 ID | 조회 주제 |
|---|---|---|
| Unity | `/websites/unity3d_manual` | RenderGraph 리소스 풀링/aliasing, ScriptableRenderPass, RenderPassEvent, Internal/External, RTHandle/RTHandles.Alloc, GameObject.layer/cullingMask |
| Unreal | `/websites/dev_epicgames_unreal-engine` | RDG transient allocator, AddPass 의존 도출, AddDrawScreenPass, FRenderTarget/FSceneView/FSceneViewFamily |
| Godot | `/godotengine/godot-docs` | CompositorEffect 5단계 콜백, RenderingServer `viewport_attach_camera`/`viewport_get_render_target`/`camera_set_cull_mask`/`instance_set_layer_mask` |
| Cocos2d-x | `/cocos2d/cocos2d-x` | Camera(Scene._cameras)/setCameraMask, Renderer+RenderQueue+RenderCommand, RenderTexture, Material→Technique→Pass |

> 모든 ✅/⚠️ 판정은 위 조회의 *직접 인용*(🔵 표기)에 근거. 💭 표기는 그 사실 위의 주관적 설계 판단으로, 사실과 분리해 읽을 것 (`code-design-review-lenses` 객관/주관 분리).

---

## 6. 용어 교체 — 내가 지어낸 이름 → 엔진 정통 (4엔진 대조)

이 문서/계획안의 이름은 세 부류 — **[계획안]**(사용자 설계명), **[내 임시]**(내가 지어냄 = 교체 대상), **[엔진]**(이미 정통 차용). 아래는 **[내 임시]** 를 하나씩 4엔진 정통명과 대조해 확정한 결과.

### 6.1 내가 지어낸 이름 (교체 대상) — 하나씩

**① `View`** (C5 에서 Camera 개명안으로 내가 제안)
- 의미: 시점 (View/Proj 행렬 + cullMask)
- 4엔진: 🔵 Unreal `FSceneView` / 🔵 Unity `Camera` / Godot `Camera3D` / 🔵 Cocos `Camera` — **3/4 가 "Camera" 유지**
- → ❌ **개명 철회 → `Camera` 유지**. 폐기 대상은 "Camera" 가 아니라 "**Stage**" 명칭. (cullMask 는 이 `Camera` 에 귀속)

**② `RenderTargetPool`** (Pass 출력 RT 대여 seam)
- 의미: Pass 가 출력 텍스처를 받아오는 풀
- 4엔진: Unreal `IPooledRenderTarget`/`FRenderTargetPool` *(대체로 — 타입 철자 미재확인)*, 🔵 Unity `RTHandle 시스템`(`RTHandles.Alloc`), Cocos `RenderTexture`(개별, 풀 없음)
- → ✅ **`RenderTargetPool` 유지** (Unreal 정통 근접). Unity 식 선호 시 `RTHandlePool` 도 가능.

**③ `TransientTargetPool`** (aliasing 공유, 최적화)
- 4엔진: 🔵 Unreal **"Transient Resources" / transient allocator** (정확히 이 단어), 🔵 Unity **"Internal"**(transient)
- → ✅ **유지** — `Transient` 은 차용 정통어.

**④ `DedicatedTargetPool`** (각자 텍스처, 비-aliasing, 디버깅)
- 4엔진: *단일 차용 클래스명 없음*. Unreal 은 "transient allocator **OFF** / default resource pool", Unity 는 "Imported/External"(단 cross-frame 뉘앙스라 부정확)
- → ⚠️ **`DedicatedTargetPool` 유지 (서술적 — 차용명 없음 명시)**. 의미("각자 전용")가 가장 명확. 굳이 Unreal 축어면 `Transient`↔`Default` 쌍도 가능하나 "Dedicated" 가 전달력 우위.

### 6.2 계획안 이름 (사용자 설계명 — 유지) + 4엔진 대응 참고

| [계획안] 이름 | Unity | Unreal | Godot | Cocos | 비고 |
|---|---|---|---|---|---|
| `IPassable` (렌더 패스) | `ScriptableRenderPass` | `FMeshPassProcessor`/AddPass | `CompositorEffect` | `RenderCommand`(개념) | 유지 OK |
| `IRenderable` (대상) | Renderer | `FPrimitiveSceneProxy` | `VisualInstance3D` | `Node`+`RenderCommand` | 유지 OK |
| `Target` / `Canvas` | `RTHandle` | `FRenderTarget` 🔵 | RenderTarget RID | `RenderTexture` | 💭 **`Canvas` 지양** — Unity `Canvas`=UI 라 혼동. `Target`/`RenderTarget` 권장 |
| `PassIterator` (실행자) | `ScriptableRenderer` | `FRDGBuilder` | — | `Renderer` | 유지 OK |
| Queue Layer (Pass 내 순서) | renderQueue | sort key | — | `RenderQueue` 🔵 | 유지 OK |

> ⚠️ **Cocos 이름 충돌 주의**: Cocos `Pass`(Material→Technique→**Pass**)는 *셰이더 패스* — 계획안 `IPassable`(렌더 패스)와 다른 층. 같은 "Pass" 단어지만 의미 다름.

### 6.3 최종 네이밍 결론

- **교체/철회**: `View` → **`Camera` 유지** (개명 철회).
- **유지(엔진 정통)**: `RenderTargetPool` / `TransientTargetPool` (✅ 차용) / `DedicatedTargetPool` (서술적).
- **지양**: `Canvas` → `Target`/`RenderTarget` (UI 혼동).
- **계획안 핵심명 유지**: `IPassable` / `IRenderable` / `PassIterator` / `BeforeIndex` — 4엔진에 1:1 동의어는 없으나 개념 대응은 명확하므로 사용자 설계명 유지.

---

## 7. 채택 — `ITargetAllocator` Factory 추상 (allocator 개념 정식 도입)

**결정**: 텍스처 *생성*을 별도 Factory 추상으로 못 박는다.

```cpp
struct ITargetAllocator {                       // 원시 팩토리 seam (desc -> handle)
    virtual Texture CreateTarget(const TargetDesc&) = 0;
    virtual void    DestroyTarget(Texture) = 0;
};
class IRenderBackend : public ITargetAllocator { /* + BindTarget/DrawMesh/Blit/Present */ };
```

- **2층 팩토리 명문화**: ① 원시 팩토리 `ITargetAllocator::CreateTarget`(*실제 GPU 할당*) ↔ ② 정책 팩토리 `IRenderTargetPool::Acquire`(*재사용 정책* — Dedicated/Transient). ①을 *이름 있는 seam* 으로 분리.
- **ISP(인터페이스 분리)**: 풀(`IRenderTargetPool` 구현들)은 이제 *좁은* `ITargetAllocator&` 에만 의존 — `DrawMesh`/`Blit`/`Present` 를 모름. 의존 방향이 정직해지고 풀 테스트가 더 좁은 mock 으로 가능.
- 🔵 엔진 정통: Unreal `CreateRenderTarget(format,gamma,normal,size)`("creates and adds or **reuses from the pool**") + RDG `CreateTexture`(descriptor-first), Unity `RTHandles.Alloc(desc)`, Godot `RenderingDevice.texture_create(RDTextureFormat)` — 전부 "desc → factory → handle".

### 7.1 allocator 6 확장축 (전부 이 seam 뒤 비파괴)

| 축 | 내용 | 엔진 근거 🔵 |
|---|---|---|
| ① desc 풍부화 | `TargetDesc += format, flags` | Unreal format/gamma/normal, Godot RDTextureFormat |
| ② 정책 변형 | Dedicated ↔ Transient(aliasing) | Unreal default pool ↔ transient allocator |
| ③ 키 전략 | (context,name) + desc-hash | Godot `get_texture(context,name)`, Unity `Hash128(desc)` |
| ④ 동적 해상도 | scale factor → 풀 자동 리사이즈 | Unity `RTHandles.Alloc(Vector2.one)` |
| ⑤ temporal ring | N-buffered 프레임 히스토리 | Unity `BufferedRTHandleSystem`/`CameraHistoryItem` |
| ⑥ deferred | descriptor-first, realize-at-execute | Unreal RDG `CreateTexture` |

### 7.2 대표 소비자 모듈 3종 (allocator 위에 얹힘)

| 모듈 | Camera | Target(allocator) | IRenderable | 압력/확장 |
|---|---|---|---|---|
| **Shadow Map** | 광원 POV | 깊이맵(DEPTH) | caster | ① desc.format + **다중 입력**(C6 `vector<int>`) 최초 요구 |
| **Bloom PostFx** | screen ortho | mip 피라미드 | 풀스크린 quad | ④ 동적해상도 + ② ping-pong |
| **RTT/Reflection** | 보조 카메라 | off-screen RT | (같은 씬) | 3축 재조합 + ⑤ temporal |

> 💭 Shadow Map 은 "광원 Camera + 깊이 Target + caster" 한 패스 + 본 패스가 그 깊이맵을 입력으로 받는 것 — **3축 모델이 그림자까지 일반화됨**을 증명. 동시에 *desc.format* 과 *다중 입력* 두 확장을 가장 먼저 요구하는 모듈.

> 다이어그램(v2, `ITargetAllocator` 반영): [`doc/diagrams/refrender-class-dependency.{dot,svg,png}`](diagrams/refrender-class-dependency.svg) — 의존 레이어링(최다 피참조=상단, 최다 참조=하단).
