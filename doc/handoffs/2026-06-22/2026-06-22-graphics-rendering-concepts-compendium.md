# 렌더링 그래픽스 개념집 (Handoff / 문제 출제용)

> 작성: 2026-06-22 · 대상: 본 엔진(SJH OpenGL) 기여자 + 학습자. 이 대화에서 다룬 Q&A 를 *개념 정의 + 오해 교정 + 모범엔진 매핑 + 본 엔진 매핑 + 문제 시드* 로 재구성한 자기완결 문서.
> 출처 표기: 🟢 = 정설/표준 이론, 🔵 = context7 docs 검증(Unity/Unreal/Godot), 🟡 = 본 엔진 코드.
> 본 엔진 심볼은 **2026-06-22 rename 반영**: `Pass::RenderQueue`(구 Kind) / `Pass::RenderStateBlock`(구 PipelineState) / `IRenderPassable`(구 IRenderStage).

## 사용법 (문제 출제)
각 모듈 끝 **[문제 시드]** 로 5유형 출제 가능:
1. **정의형** — "X 란?"  2. **오해 교정 T/F** — 함정 문장의 참/거짓+이유  3. **엔진 매핑** — "Unity의 A = 우리의 ?"  4. **순서 배열** — 파이프라인 단계 정렬  5. **계산/추론** — 좌표·정렬방향 등.

---

## M1. "그려지는 것" 의 카테고리 — 모든 게 Mesh 인가?

🟡 본 엔진의 draw 추상 = `DrawCommand` **2종** (`WorldMesh`, `ScreenQuad`) — 둘 다 결국 *Mesh(정점/인덱스 버퍼) 하나*를 그림.

🟢 "무엇을·어떻게 그리나"의 올바른 5분류:
| 카테고리 | 정의 | 예 |
|---|---|---|
| **A. Geometry/Mesh** | CPU 정점·인덱스 버퍼 제출, vertex 변환 | 모델·스프라이트·텍스트(글리프 quad)·skybox·healthbar |
| **B. Screen-space 패스** | fragment 가 픽셀마다 일함(풀스크린) | PostFX·deferred lighting·screen-space sky |
| **C. GPU-procedural** | primitive 를 GPU 가 생성 | **파티클(Effekseer)**·instancing·ray-march 볼류메트릭 |
| **D. Framebuffer op** | scene primitive 없이 버퍼 조작 | **glClear**·blit |
| **E. Foreign/overlay** | 외부 렌더러 | **ImGui**·debug draw |

🟡 본 엔진: A 가 거의 전부(skybox·PostFX 포함). **비-Mesh = Particle(Effekseer, C/foreign) + ImGui(E) + glClear(D)**. Particle 은 `ParticleStage` 가 `mVFX->Draw()` 로 Effekseer 자체 렌더러 호출 → DrawCommand/MeshPassProcessor 우회. 그래서 `InvalidateStateCache` + EBO 재핀 방어 필요.

**[문제 시드]** ① A~E 정의 매칭 ② "스프라이트/텍스트/skybox 는 Mesh 다 (T/F)" → T(전부 quad/box Mesh) ③ "파티클은 Mesh 다" → F(Effekseer foreign).

---

## M2. Skybox — Mesh 인가 glClear 인가? (엔진 비교)

🟢 Skybox 는 **glClear 가 아니라 그려지는 실체**. 단 *어떻게* 그리느냐가 엔진별로 갈림:
| 엔진 | 방식 | Mesh? | 🔵출처 |
|---|---|---|---|
| Unity BiRP/URP | "cube 안에 Scene 을 넣고 가장 뒤에 렌더" | ✅ cube mesh | unity `sky.html` |
| Unity HDRP | skybox material *미지원*, Physically Based Sky | ❌ screen-space | unity `sky.html` |
| Unreal(modern) | **Sky Atmosphere** = deferred 산란 render pass | ❌ screen-space | epic `FSceneInterface` |
| Unreal(legacy) | BP_Sky_Sphere | ✅ sphere mesh | — |
| Godot | **`shader_type sky`** — per-pixel `EYEDIR` | ❌ screen-space sky 셰이더 | godot `sky_shader.md` |

🟡 본 엔진 = Unity BiRP 식 **고전 Mesh(scale 50 Box) + 셰이더 트릭**:
- translation 제거 `mul((float3x3)uView, aPos)` → 무한 원경.
- `pos.xyww` → `ndc.z = w/w = 1.0` = far plane 고정.
- `Pass::RenderQueue::Skybox`(2500) + DepthFunc LEQUAL + Cull FRONT + DepthWrite off → 불투명 뒤, 배경 픽셀만.

**[문제 시드]** ① "현대 3사 추세는 skybox 를 mesh→screen-space 로 옮긴다 (T/F)" → T ② `.xyww` 가 하는 일 서술 ③ Godot sky 와 우리 skybox 의 차이.

---

## M3. "Pass" 용어·철학 + 레벨 (중복 의심 해소)

🟢 **Pass 어원** = "데이터를 파이프라인에 *한 번 통과(훑기)*". 페인트 한 번 칠하기, multi-pass compiler 와 동어원. 철학: *복잡한 이미지를 여러 순차 traversal 로 분해, 각 pass 가 한 버퍼에 쓰고 합성*.

🟢🔵 "Pass" 는 어느 엔진에서나 **여러 레벨에 중복 사용** — 본 엔진 매핑:
| 레벨 | Unity | Unreal | Godot | **본 엔진** |
|---|---|---|---|---|
| L1 파이프라인 패스/스테이지 | `ScriptableRenderPass`(+RenderPassEvent) | RDG pass / `FSceneRenderer` | `CompositorEffect`(+EffectCallbackType) | **`IRenderPassable`** |
| L2 패스 데이터 | `PassData`(RenderGraph) | RDG `PassParameters` | `RenderData` | `RenderStateBlock`/PostFXStageConfig(부분) |
| L3 큐/정렬 분류 | `renderQueue` | `EMeshPass` | `render_priority` | **`Pass::RenderQueue`+QueueOffset** |
| L4 셰이더 패스 | ShaderLab `Pass{LightMode}` | (material) | (내장) | (없음, material당 1셰이더) |
| L5 렌더스테이트 블록 | `RenderStateBlock` | `FGraphicsPipelineStateInitializer` | RD pipeline state | **`Pass::RenderStateBlock`** |
| L6 per-draw 커맨드 | (내부) | **`FMeshDrawCommand`** | draw_list | **`DrawCommand`** |
| L7 패스 프로세서 | `DrawRenderers` | **`FMeshPassProcessor`** | RendererSceneRenderRD | **`MeshPassProcessor`** |

**핵심 결론**: 본 엔진의 `Stage`(L1) / `RenderQueue`(L3) / `RenderStateBlock`(L5) 는 *중복이 아니라 다른 레벨*. 모범엔진도 똑같이 분리. 혼동은 "Pass" 단어가 L3·L5 에 겹쳐서. **우리 `IRenderPassable`(L1) = Unity 가 `ScriptableRenderPass` 라 부르는 그것** (그래서 Stage=Pass 처럼 느껴짐).

**[문제 시드]** ① L1~L7 엔진 매핑 매칭 ② "Stage 와 Pass::Kind 는 기능 중복이다 (T/F)" → F(다른 레벨) ③ Vulkan `VkRenderPass` 의 정식 정의(attachment+load/store+subpass).

---

## M4. Render Target / "Canvas" — fragment 가 쓰는 대상

🟢 fragment 가 쓰여지는 대상 정식명 = **Render Target(=FBO attachment)**. **항상 2D 텍스처/버퍼**. "Mesh 표면에 직접 그리기"는 없음 — *2D 텍스처(RT)에 렌더 후 그 텍스처를 Mesh 가 샘플*(=RTT, Render-to-Texture).

🟢 두 직교 축으로 분류 (사용자가 흔히 하나로 합침):
- **축 A — 어느 RT 에 쓰나**: 화면(backbuffer/default framebuffer) vs 오프스크린 RT(render texture/FBO: shadow map·G-buffer·반사 cubemap·SSAO·bloom·portal).
- **축 B — fragment 파라미터 공간**: screen-space(`gl_FragCoord`) / surface·UV-space(texcoord) / light-space(shadow map 생성) / world·view-space.

🟢 흔한 오해 교정:
- **그림자** = "Mesh 텍스처 override" ❌ → 빛 시점 depth 를 **별도 shadow map(RT)** 에 렌더 후 shading 때 *샘플*. 🔵 Unreal "Shadow Depth Pass", Godot "shadow atlas".
- **Decal** = "Mesh 텍스처에 칠하기" ❌ → 현대는 **deferred/screen-space decal**(G-buffer projection). 🔵 Unreal "Deferred Decals", Unity "Decal Renderer Feature/DBuffer".
- *진짜로 Mesh UV 텍스처에 그리는* 기법 = **texture-space rendering / 베이킹 / Mesh Paint** (niche).

🟡 본 엔진: backbuffer(`DefaultRenderTarget`) + 오프스크린 `mSceneFB`(color+depth) + PostFX FBO 체인. **그림자 = shadow map 없음, EntityShadow.png 가짜 데칼 quad(=A,B surface-space)**. fog 가 `mSceneFB` depth RT 를 screen-space 샘플. G-buffer/deferred decal/real shadow map 없음(forward).

**[문제 시드]** ① "그림자는 mesh 텍스처에 override 된다 (T/F)" → F ② RT 두 축 분류 ③ "우리 엔진 그림자는 shadow map 이다 (T/F)" → F(가짜 데칼).

---

## M5. Backbuffer vs Swapchain image vs Present

🟢 정확한 구분:
- **Swapchain** = N개 이미지 큐 + 순환 표시 메커니즘(double=2, triple=3).
- **Swapchain image** = 그 큐의 *한 원소(리소스)*. 🔵 Vulkan `VkImage`(`vkGetSwapchainImagesKHR`) 가 정식.
- **Backbuffer** = "지금 그리는 중·미표시"라는 *역할*. 🔵 DXGI "back buffer"(`GetBuffer(0)`) 가 정식. Front=스캔아웃 중.
- 관계: **backbuffer ≈ 현재 acquire 된 swapchain image**. double 이면 1:1처럼 보이나, triple 이면 backbuffer(역할 1) ⊂ swapchain images(N).

🟢 **Present/Swap = 화면을 *통째 교체*(flip/copy), 이전 프레임과 알파 blend 아님.** double buffering 의 목적이 *반쯤/섞인 프레임을 안 보이게* 하는 것 → 오히려 blend 의 반대.

🟢 "Swapchain 이 Photoshop layer 처럼 알파 합성?" ❌:
- 알파 합성이 일어나는 곳: ① 내 렌더링 blend state(프레임 *내부*) ② OS 윈도 compositor(창↔데스크톱). **swapchain 은 ②에 이미지를 건네줄 뿐**.
- 🔵 `compositeAlpha`(Vulkan)/`DXGI_ALPHA_MODE` 는 swapchain 이 섞는 게 아니라 *compositor 에게 창 알파 해석법을 주는 힌트*(창 투명도용).
- 프레임-간 누적(모션블러/TAA)은 *내가* 직전 프레임 RT 를 blend 해서 만듦(swapchain 아님).

🟡 본 엔진: GL default framebuffer 0 = backbuffer(`GL_BACK`), `glfwSwapBuffers`=present(통째 교체). swapchain 은 GLFW/드라이버가 암묵 관리.

**[문제 시드]** ① "present 는 이전 프레임 위에 알파로 덮는다 (T/F)" → F(통째 교체) ② backbuffer vs swapchain image 차이 ③ "compositeAlpha 는 swapchain 이 직접 blend 하는 기능 (T/F)" → F(compositor 힌트).

---

## M6. 한 패스 내 draw 순서 — sort(CPU) vs depth test(GPU)

🟢 **두 메커니즘 분리가 핵심** (흔히 합침):
| | 정체 | 위치 |
|---|---|---|
| ① draw 제출 순서(sort) | *어느 순서로 draw call 을 쏘나* | CPU(정렬) |
| ② 실제 색 결정(depth test+blend) | per-fragment | GPU(고정함수) |

- **불투명**: ②(z-buffer)가 *순서 무관* 정확성 보장. ①은 **성능 최적화**(front-to-back = early-Z + state batching).
- **투명**: blend 가 순서의존 + depthWrite off → z-buffer 가 못 풀어줌 → **①(back-to-front)가 정확성의 유일 수단**(painter's algorithm).

🟢 **Sort Key**(Christer Ericson 정통, 🔵 Unreal `FMeshDrawCommand` sort key / Unity `SortingCriteria`): 64-bit 패킹 `[Queue/Layer | Material/Shader | Depth]`. 불투명 depth=front-to-back, 투명 depth=back-to-front(반전).

🟡 본 엔진 `MeshPassProcessor::SortMultiStage`: `queueLayer`(RenderQueue) → program → material → depth(불투명 `a.depth>b.depth`, 투명 `a.depth<b.depth`), `std::stable_sort`(결정성).
- 🔵 Unity renderQueue: Background 1000 / Geometry 2000 / AlphaTest 2450 / Transparent 3000 / Overlay 4000. 🟡 우리: Opaque 2000 / Skybox 2500 / Transparent 3000 / PostFX ~9000.

🟢 사용자 모델 "Layer→Z, 멀면 낮은 우선순위"의 누락:
1. Material 그룹핑 축(Layer~Depth 사이, batching).
2. **depth 방향이 투명에서 반전**(먼 것을 *먼저* 그림).
3. 실제 색 덮어쓰기는 sort 가 아니라 ②(depth test+blend).

🟢 고급: 투명 정렬은 교차/관통에서 깨짐 → **OIT**(Depth Peeling / Weighted Blended OIT / Per-Pixel Linked List).

**[문제 시드]** ① "불투명 정렬은 정확성을 위해 필수다 (T/F)" → F(성능. 정확성은 z-buffer) ② 투명/불투명 depth 정렬 방향 ③ sort key 3차원 구성.

---

## M7. Output Merger / ROP — 색 덮어쓰기의 실제 단계

🟢 fragment shader 출력 후 GPU 고정함수 순서(=색이 써지는 경로):
| 순서 | 단계 | 🟡 `RenderStateBlock` 제어 |
|---|---|---|
| 1 | Scissor test | — |
| 2 | **Stencil test** | `StencilEnable/Func/Ref` |
| 3 | **Depth test** (`DepthFunc`) — 통과해야 진행 | `DepthTest/DepthFunc` |
| 4 | **Depth write** | `DepthWrite` |
| 5 | **Blend** (`src*sf OP dst*df`; off=replace) | `BlendEnable/BlendSrc/BlendDst` |
| 6 | Color mask | — |
| 7 | Write to attachment | (RT) |

→ "가까운 게 이긴다" = **3 Depth test**. "덮기 vs 섞기" = **5 Blend**.
🟡 본 엔진: `DeviceContext::ApplyRenderStateBlock(Pass::RenderStateBlock)` 가 2~5 단계 GL state 설정. blend off→on 전이 시 `glBlendFunc` 강제(과거 healthbar/shadow 알파 버그 근본원인 — 캐시 기본값≠GL 기본값).

**[문제 시드]** ① 1~7 단계 순서 배열 ② "blend off 면 색은 어떻게 되나" → replace(덮어쓰기) ③ depth test 와 blend 중 "투명 합성"을 담당하는 것.

---

## M8. Stencil 버퍼 — 개발자가 찍는 태그 (OpenCV 분할 아님)

🟢 핵심 오해 교정:
- **OpenCV 영역분할(watershed 등) 아님** — 자동 계산·번호매김 없음. *내가 렌더링 중 직접 기록하는 per-pixel 정수 메모장*.
- **좌상단 증가순/위치기반 인덱싱 아님**. **런타임마다 바뀌지 않음**(같은 draw=결정적).
- 값이 차는 법: fragment 마다 stencil/depth test 결과로 `glStencilOp(sfail,dpfail,dppass)` 적용. op = `KEEP/ZERO/REPLACE(→glStencilFunc 의 ref 값)/INCR/DECR/INVERT`. 즉 **"내가 고른 op × 설정한 ref"**.
- "증가(INCR)"는 *그 op 을 골랐을 때만* — 대표: **Shadow Volume**(앞면 INCR/뒷면 DECR, 카운트≠0=그림자 안). 그 외: 마스킹·아웃라인·포털·거울·데칼·CSG.
- 라이프사이클: 매 프레임 `glClearStencil(0)`+clear → draw 가 재기록. 보통 8bit, `DEPTH24_STENCIL8` 로 depth 와 묶임.

🟡 본 엔진: `RenderStateBlock` 의 `StencilFunc/StencilRef/StencilOp*` 가 그 설정. `_MyApp_` 은 거의 미사용(Opaque stencil off).

**[문제 시드]** ① "스텐실은 OpenCV 분할처럼 자동 영역번호다 (T/F)" → F ② "스텐실 값은 좌상단부터 증가한다 (T/F)" → F ③ REPLACE op + ref=1 마스킹 시나리오 서술 ④ Shadow Volume 이 INCR/DECR 쓰는 이유.

---

## M9. Z-buffer — 비선형 화면공간 깊이 (선형 view-Z 아님)

🟢 핵심 오해 교정: 저장값은 **view-space 선형 거리 Z 가 아니라, perspective divide 거친 *비선형* window-space depth [0,1]**.
- 변환 사슬: `view.z(선형) → ×proj → clip.z/clip.w → ndc.z(GL [-1,1]) → window.z = ndc.z*0.5+0.5 ([0,1])`.
- `1/w` 나눗셈 → window.z 는 view-z 에 대해 **대략 1/z(쌍곡선)**. 정밀도 카메라 근처 집중 → 먼 거리 **z-fighting** 근본원인 → 현대 **Reverse-Z**/log depth.
- 쓰기: rasterizer 가 window.z 를 perspective-correct 보간 → Depth test(`GL_LESS` 기본, 작은 z=가까움 통과) → DepthWrite on 이면 기록. clear=1.0(far).

🟢 사용자 "view Z 거리순" → *방향은 맞음(가까운 게 이김)*, 단 **저장값은 선형 view-Z 아닌 비선형 window depth**.
🟡 연결: skybox `.xyww` → window.z=1.0 강제(far 고정).

**[문제 시드]** ① "z-buffer 는 view-space 선형 거리를 저장한다 (T/F)" → F(비선형 window depth) ② z-fighting 이 먼 거리에서 심한 이유 ③ 변환 사슬 순서 배열 ④ Reverse-Z 가 푸는 문제.

---

## M10. Stencil vs Depth 대조 (요약)
| | Stencil | Depth(z) |
|---|---|---|
| 채우는 주체 | 개발자(op+ref) | rasterizer(보간 window.z) |
| 값 의미 | 임의 태그/카운트 | 비선형 화면공간 깊이 |
| 자동 번호 | ❌(INCR 골랐을 때만) | ❌(투영 깊이) |
| clear 기본 | 0 | 1.0 |
| 비교 함수 | `glStencilFunc`(EQUAL 등) | `glDepthFunc`(LESS 등) |

---

## M11. 패스-간(L1) 순서의 명칭 — render-queue(L3)와 구분

🟢 "Pass 의 Order" 는 레벨로 갈림:
- **패스 *사이* 순서 (L1, IRenderPassable/ScriptableRenderPass 레벨)**:
  | 엔진 | 명칭 |
  |---|---|
  | Unity URP | 🔵 **`RenderPassEvent`** (+ event 내 정수 offset) |
  | Unreal | 🔵 **RDG(Render Dependency Graph) topological 스케줄** + 고정 `FSceneRenderer` 시퀀스 |
  | Godot | 🔵 **`effect_callback_type`** (PRE/POST_OPAQUE/SKY/TRANSPARENT) |
  | 학술/일반 | **Frame Graph / Render Graph 의 pass schedule (dependency DAG 위상정렬)** |
  | 🟡 본 엔진 | 전용명 없음 — **`mStages` 명시적 삽입 순서** (World→Particle→Screen→ScreenQuad) |
- **패스 *안* draw 순서 (L3)** = M6 의 **render queue / sort key** (`Pass::RenderQueue`). 별개 레벨.

🟢 핵심: L1(패스-간, RenderPassEvent/RenderGraph) ≠ L3(패스-내, render queue). 둘 다 "순서"지만 다른 레벨.

**[문제 시드]** ① "Unity 에서 패스 실행 순서를 정하는 enum 은?" → `RenderPassEvent` ② "패스-간 순서와 render queue 는 같다 (T/F)" → F(L1 vs L3) ③ 모던 엔진이 패스 순서를 의존성 DAG 로 스케줄하는 구조 이름 → Frame/Render Graph.

---

## 종합 오개념 체크리스트 (T/F 빠른 출제)
1. 파티클(Effekseer)은 Mesh 다 → **F** (foreign 렌더러)
2. Skybox 는 glClear 다 → **F** (그려지는 Box mesh + depth 트릭)
3. 모든 모범엔진 skybox 는 mesh 기반 → **F** (Godot/UE/HDRP screen-space)
4. Stage 와 Pass::RenderQueue 는 기능 중복 → **F** (L1 vs L3)
5. 그림자는 Mesh 텍스처에 override → **F** (별도 shadow map RT 샘플)
6. present 는 이전 프레임에 알파 blend → **F** (통째 교체)
7. compositeAlpha = swapchain 의 blend 기능 → **F** (compositor 힌트)
8. 불투명 정렬은 정확성 필수 → **F** (성능. z-buffer 가 정확성)
9. 투명도 가까운 것부터 그린다 → **F** (먼 것부터, back-to-front)
10. 스텐실은 자동 영역분할/좌상단 인덱싱 → **F** (개발자 op+ref, 결정적)
11. z-buffer = 선형 view-Z → **F** (비선형 window depth)
12. blend off 면 색이 섞인다 → **F** (replace=덮어쓰기)

## 참조
- 본 엔진 구조: [`doc/diagrams/render-architecture.md`](../diagrams/render-architecture.md) (class/sequence, IRenderPassable/RenderQueue/RenderStateBlock 반영).
- 관련 버그·교훈: [`doc/버그리포트.md`](../버그리포트.md) (blend-func 캐시, binding point 충돌 등).
- 모범엔진 사실 출처: context7 — Unity `/websites/unity3d_manual`·`/unity-technologies/graphics`, Unreal `/websites/dev_epicgames_unreal-engine`, Godot `/godotengine/godot-docs` (2026-06-21~22 조회).
