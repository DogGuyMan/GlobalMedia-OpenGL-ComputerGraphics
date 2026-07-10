# Resume Handoff — 렌더링 시스템 마이그레이션 *설계* 완료 → 구현 착수 직전 (v5)

> 📌 **구현이 이미 시작됨 (Phase 1~2 완료).** *구현 재개*는 이 설계 핸드오프가 아니라 **[`2026-06-23-render-migration-EXECUTION-resume-handoff.md`](2026-06-23-render-migration-EXECUTION-resume-handoff.md)** 를 entry 로 쓸 것. 본 문서는 *설계 근거* 참조용으로 유지.

> 작성 2026-06-23, **v5 갱신**(범위 분리). 이 문서 하나로 다른 Claude 에이전트가 *맥락 0* 에서 이어받을 수 있게 자기완결.
> **단일 진입점 = [`doc/마이그레이팅계획안.md`](../마이그레이팅계획안.md) (v5)**. 본 핸드오프가 임계 경로를 인라인(§4~§8) — 링크는 깊이용 optional.

---

## 0. ⚠️ 먼저 알아야 할 3가지

1. **`doc/` 전체가 gitignored** (`.gitignore:10:doc/`). 이 세션 산출물(설계 문서 전부 + 본 핸드오프)은 **로컬 전용, git 추적 안 됨**. 이 머신 파일시스템에만 존재. (코드 `src/` 는 정상 추적.)
2. **이 세션은 코드를 한 줄도 안 바꿈.** 전부 설계/문서. 빌드 = 직전 커밋 그대로 GREEN(가정). 구현 미착수.
3. **빌드/커밋은 사용자.** 에이전트는 구현+보고. 커밋 path-scoped, `Co-Authored-By` 미사용, `git add -A` 금지 (§9).

---

## 1. TL;DR + 다음 액션

- **상태**: 렌더링 시스템 *in-place 마이그레이션* 설계가 **v5 로 확정**(D1~D13 lock). **v4→v5 에서 범위가 갈림**: 이번 = *렌더 아키텍처만* / 백엔드 Facade+Strategy·PSO·RT풀 = **별도 후속 프로젝트**(§ 후속). 현 엔진이 목표의 ~90% 구현 → 대부분 *확장(E)*.
- **다음 액션**: **정본 구현 plan = [`doc/superpowers/plans/2026-06-23-render-migration-implementation-v5-reviewed.md`](../superpowers/plans/2026-06-23-render-migration-implementation-v5-reviewed.md)** (v5 기준 재생성 + 4-렌즈 적대적 리뷰 교정 완료). 그 plan 의 **Phase 1 Task 1.1(Material RenderStateBlock + IRenderStateProvider)** 착수. (subagent-driven 또는 inline.)
- **착수 전 필수**: §10 grounding — 직전 커밋 `68506a8` 이후 심볼/파일명 이동 가능. plan 은 grounding 워크플로우 6-슬라이스 검증했으나 각 Phase 착수 시 live 재확인.

> 📄 **구현 plan 산출물**: 정본 = `...-v5-reviewed.md`(적대적 리뷰 교정: UseProgram void+dedup·잎 ApplyRenderStateBlock 중복 제거·Camera 네임스페이스·Task 선행 그래프·draw_ops.h 공유·Phase 3.0 원자 교체·Pass 스켈레톤·PassIterator 완전 코드). 프리스틴 초안 = `...-v5.md`(보존). 🔴 SUPERSEDED = `...-render-migration-implementation.md`(v4, 구현 금지).

---

## 2. 세계 상태 (재측정 2026-06-23)

- **branch**: `game/slang-phase2-ubo`
- **최근 커밋**: `68506a8 [refactor] 렌더링 모듈 Rename` (직전, 재검증 대상) · `c0388b3 [refactor] Slang 마이그레이팅 + PropertyBlockSetter/uniform_cache 삭제` · `3228f71 [dev] UBO 매핑함수 추가`
- **이 세션 변경**: 코드 0. 문서만(전부 gitignored):
  - `doc/마이그레이팅계획안.md` — ★ **진입점 v5** (범위 분리)
  - `doc/RenderingSystemRefactorReserch.md` (447줄) — 4엔진 연구(C1~C7 + §6 네이밍 + §7 allocator)
  - `doc/렌더링시스템 흐름 리펙토링 계획안.md` (155줄) — 사용자 원안
  - `doc/superpowers/plans/2026-06-22-reference-render-pipeline.md` — 클린룸 레퍼런스(명세용, 실엔진엔 안 만듦)
  - `doc/superpowers/plans/2026-06-23-render-migration-implementation.md` — 🔴 v4 기반 SUPERSEDED
  - `doc/diagrams/engine-migration-class.{dot,svg,png}` — ★ **v5 예상 클래스 다이어그램** (⚠️ `doc/` 이지 `doc/` 아님)
  - `doc/diagrams/refrender-class-dependency.*` — 구 레퍼런스 다이어그램(클린룸용)

---

## 3. 이 설계가 무엇인가 (배경)

레퍼런스 plan(2026-06-22)은 *헤드리스 클린룸*이라 **실 엔진엔 안 만든다**(D1). 그 *목표 형태*를 현 엔진에 *in-place 재형성*. 4엔진(Unity/Unreal/Godot/Cocos) context7 로 구조 검증.

**v4→v5 핵심 전환** (세션 후반): 멀티백엔드 seam 은 *고수준 인터페이스*가 아니라 *DeviceContext Facade 아래 Strategy* 에 산다(Unreal FRHICommandList+FDynamicRHI / Godot RenderingDevice+driver). → 이번엔 **렌더 아키텍처만**, 백엔드/PSO/풀은 *별도 후속*. 지금 공짜 forward-compat = gl* 격리 + RenderStateBlock-as-data(둘 다 이미 있음).

---

## 4. 확정 모델 (자기완결 — 청사진)

### 4.1 두 레벨 순서 (D8)
- **코스 = Pass 순서** (`PassIterator` 의 `vector<IPassable*>`): 역할군 `[SkyboxPass, WorldPass, ParticlePass, ImGuiPass, PostFxPass]`.
- **파인 = QueueLayer(+Z)** (Pass 내부 `RenderableProcessor`): WorldPass 안 Mesh/Sprite/Text 교차.

### 4.2 `IRenderable` (D5) — 고수준은 구체 DeviceContext
```cpp
class IRenderable : public IRenderStateProvider {     // 3 메서드 (LSP-최소)
    virtual void Render(DeviceContext& rec, const Scene::Camera& cam) const = 0;   // ★ 구체 DeviceContext (D10)
    virtual int  QueueLayer() const = 0;
    // GetRenderStateBlock() <- IRenderStateProvider
};
```
- mesh-family = 이미 `MeshRenderer` 단일 경로(`SpriteRenderer:MeshRenderer`; `TextRenderer`가 글자마다 SpriteRenderer; `CollectFromActor`가 `GetComponent<MeshRenderer>()`). → Mesh=Sprite=Text 추가작업 0. ⚠️ live 재확인(§10).
- Particle/ImGui 은 IRenderable 에 안 섞고 역할 Pass(4.3).

### 4.3 `IPassable` 하나 + 역할군 (D4) — 고수준은 구체 DeviceContext
```cpp
class IPassable {
    virtual void           Draw(DeviceContext& rec, const Texture* before) = 0;   // ★ 구체 (D10), before nullable
    virtual const Texture* GetPassResult() const = 0;   // = Framebuffer color attachment (D6)
    virtual void           OnResize(int w, int h) {}
    int BeforeIndex = -1;                               // 입력 Pass vector 인덱스. -1 = sceneRaw
};
// SkyboxPass / WorldPass{RenderableProcessor mProc} / ParticlePass{efk} / ImGuiPass{imgui} / PostFxPass
```
`PassIterator 는 vector<IPassable*> 만`, `main 은 PassIterator 하나만` 안다.

### 4.4 ROP Facade (D7) + D9
```cpp
struct IRenderStateProvider { virtual const Pass::RenderStateBlock& GetRenderStateBlock() const = 0; };
class  Material : public IRenderStateProvider { Pass::RenderStateBlock mState; };   // ★ 저장처 (SetPass=seed)
// IRenderable(mesh): GetRenderStateBlock() -> mMaterial->GetRenderStateBlock()
```
- **D9**: efk/ImGui ROP 못 꺼내면 `RenderStateBlock{}`(중립) → 순서 Layer+Z 만. 실 GL state 는 라이브러리 내부 + `InvalidateStateCache` 경계.
- `RenderStateBlock` = PSO 디스크립터 씨앗(§ 후속이 재사용).

### 4.5 Flat `RenderableProcessor` + A-dedup (D12)
```cpp
class RenderableProcessor {     // mesh 특수 0
    struct Entry { const IRenderable* r; int queueLayer; float depth; };   // depth = collect 때 카메라로 동봉
    std::vector<Entry> mItems;
    void Submit(const IRenderable* r, float viewDepth); void Sort();   // stable_sort: QueueLayer -> (투명)depth
    void Process(DeviceContext& rec, const Scene::Camera& cam) {        // §0.5 flat
        rec.InvalidateStateCache();
        for (auto& e : mItems) { rec.ApplyRenderStateBlock(e.r->GetRenderStateBlock()); e.r->Render(rec, cam); }
    }
};
```
- **A-dedup(D12)**: 배치(직전과 같은 program/state skip)는 루프가 아니라 `DeviceContext` 캐시(`mBoundProgram`/`mLast`, D-RS-1 연장)가 흡수. Unity SRP Batcher / Godot UniformSetCacheRD 정통.

### 4.6 `DeviceContext` 구체 유지 (D10)
gl* 유일 소유자 + A-dedup 캐시. 고수준이 직접 받음. 멀티백엔드(IGraphicsBackend Strategy)는 후속.

---

## 5. Locked 결정 D1~D13 (재논의 금지)

| ID | 결정 | 선택 |
|----|------|------|
| D1 | 방식 | **in-place 재형성** |
| D2 | IPassable 연쇄 | `Draw(DeviceContext&, const Texture* before)` / `GetPassResult()->const Texture*` / `BeforeIndex` |
| D3 | 네이밍 | `IRenderPassable`→**`IPassable`** |
| D4 | Pass 능력 | **IPassable 하나 + 역할군 분리** (능력 인터페이스/다중상속 ❌ 다이아몬드) |
| D5 | IRenderable | **mesh-family**(MeshRenderer=Sprite=Text). Particle/ImGui=역할 Pass |
| D6 | RenderTarget | **쓰기 대상 추상**. Texture=attachment. `GetPassResult()`→`const Texture*` |
| D7 | RenderStateBlock | **Material 보유 + `IRenderStateProvider` Facade** |
| D8 | 정렬 2레벨 | **코스=Pass 순서 / 파인=QueueLayer** |
| D9 | ROP 미추출 | **general default** (중립 → Layer+Z) |
| **D10** | **백엔드 seam** | **ICommandRecorder 폐기 → 고수준은 구체 `DeviceContext&`**. 멀티백엔드 = DeviceContext Facade 아래 `IGraphicsBackend` Strategy(후속). 근거 Unreal FRHICommandList+FDynamicRHI/Godot RenderingDevice+driver |
| **D11** | **자원/풀 IF** | **`ITargetAllocator`/`IRenderTargetPool`/`DedicatedTargetPool`/`TargetDesc` defer** (Facade 모델에선 얕음). ResourceRegistry 구체 유지 |
| **D12** | **배치** | **A-dedup** — Processor flat §0.5, program/state 배치는 DeviceContext 캐시 |
| **D13** | **PSO/멀티백엔드** | **별도 후속 프로젝트**. 이번 forward-compat = gl* 격리 + RenderStateBlock-as-data |

> 폐기된 사고(재논의 금지): ① 능력 인터페이스 다중구현(다이아몬드) ② 모든 Pass 를 ScenePass 하나로 ③ Texture 가 RenderTarget 상위(GL 함정) ④ MeshRenderer 데이터+Processor 분리(→IRenderable 다형) ⑤ **고수준 ICommandRecorder/형제-인터페이스 RHI**(→D10 Facade+Strategy 후속) ⑥ **ITargetAllocator/풀 이번에**(→D11 defer).

---

## 6. 매핑 인벤토리 — 현 모듈 ↔ 목표 (v5)

> ⚠️ 파일명/심볼은 rename 커밋(68506a8) 이후 *재검증* 필수.

| 목표 | 현 모듈 (조사 시점) | 판정 |
|---|---|---|
| `IRenderStateProvider` | (없음) | **N(얇음)** |
| `RenderStateBlock` | `Pass::RenderStateBlock` (`src/material/pass.h`) | 🟢정의유지→**Material 이주** |
| `Material` | `Material` (`src/material/material.h`) | **E** (`mState` + IRenderStateProvider) |
| `IRenderable` | `MeshRenderer` (`src/render/mesh_renderer.h`) + `SpriteRenderer:MeshRenderer` | **E** |
| `RenderableProcessor`(Flat) | `MeshPassProcessor` (`src/render/mesh_pass_processor.h`) | **E→일반화**(A-dedup) |
| `IPassable` ⚠️ | `IRenderPassable` (`src/render/render_passable/render_passable.h`) | **E⚠️** 개명+계약 |
| `WorldPass` | `SceneRenderer`+`CameraStage` (`render_passable.impls.{h,cpp}`) | **E→통합** |
| `SkyboxPass` | (skybox queue / CameraStage) | **E** |
| `ParticlePass` | `ParticleStage` (`apps/_MyApp_/src/VFX/`) | **E** (foreign-GL) |
| `ImGuiPass` | main 직접 ImGui | **E/N** (foreign-GL) |
| `PostFxPass` | `PassComponent`(`src/render/pass_component.h`)+`ScreenQuadStage` | **E→통합** |
| `PassIterator` | `main.cpp` `mStages` | **E→모듈화** |
| `DeviceContext` | `DeviceContext` (`src/render/device_context.h`) | **🟢구체 유지** (gl* 단일 소유자 + A-dedup) |
| `Camera` | `Scene::Camera` (`src/scene/camera.h`) | **E🟢** (Sees() 헬퍼만) |
| `RenderTarget`/`Texture` | `RenderTarget`/`DefaultRenderTarget`(`src/buffer/render_target.h`)/`Framebuffer`(`src/buffer/framebuffer.h`, `GetColorAttachment()`)/`Texture`(`src/texture/texture.h`) | 🟢유지 |
| ~~ICommandRecorder / ITargetAllocator / IRenderTargetPool / DedicatedTargetPool / TargetDesc / PipelineStateDesc~~ | — | **defer → 후속 멀티백엔드 프로젝트** (D10/D11/D13) |

> 🟢 보너스 자산: `Pass::RenderStateBlock`(PSO 씨앗) · `DeviceContext` 상태캐싱(D-RS-1 = A-dedup 토대) · `SceneContext`(`src/scene/scene.h`) · Text=Sprite=Mesh.

---

## 7. Phase 계획 (in-place, 5단계 — Phase 1 기반인터페이스 제거)

1. **Material RenderStateBlock + `IRenderStateProvider` [D7]**: `Material` 이 `mState` 보유(`SetPass`=seed). 소비처 `mesh_pass_processor.cpp` 의 `DefaultRenderStateBlockOf(material->GetPass())` → `material->GetRenderStateBlock()`. seed=기존도출 무회귀.
2. **`IRenderable` + Flat `RenderableProcessor`(A-dedup) [D5/D12]**: MeshRenderer 가 IRenderable 충족(잎 자가발행). MeshPassProcessor→RenderableProcessor(IRenderable* 제네릭, §0.5 flat). 배치는 DeviceContext dedup.
3. **`IPassable` 계약 + 역할군 Pass 통합 [D2/D3/D4] — 한 Pass씩 육안**: IRenderPassable→IPassable(`Draw(DeviceContext&, const Texture*)`); WorldPass(←SceneRenderer+CameraStage)/SkyboxPass/ParticlePass/ImGuiPass/PostFxPass.
4. **`PassIterator` 모듈화 + main 배선 + DebugPassIndex [D8]**: main 은 PassIterator 하나. ImGui 직접호출→ImGuiPass. 각 BeforeIndex 배선.
5. **정리/네이밍/문서**: 구 심볼 정리, 다이어그램, 메모리/CLAUDE.md.

---

## 8. 검증 / Out of Scope

- **검증(사용자)**: `export PATH="$HOME/slang/bin:$PATH" && cmake --build --preset ninja --target _MyApp_` → 출력 붙이면 에이전트 육안. 실행 `cd build_ninja/apps/_MyApp_ && ./_MyApp_`. Phase 1·2·3·4 GUI 회귀 육안. `Sort` stable_sort 유지.
- **Out of Scope (이번 밖)**: **백엔드 Facade+Strategy(IGraphicsBackend: OpenGL/Vulkan/Metal) + PSO(PipelineStateDesc/BindPipelineState) + IRenderTargetPool/TransientTargetPool → 후속 멀티백엔드 프로젝트** · Shadow/Deferred/MSAA/HDR/Cubemap · TextRenderable/능력 인터페이스 · data-driven JSON · 다중 BeforeIndex.

---

## 9. 가드레일 & 컨벤션 (위반 금지)

- **빌드·커밋 = 사용자.** 커밋 path-scoped(`git commit <경로>`), `git add -A` ❌, `Co-Authored-By` ❌.
- 🔒 **GL 격리**: *우리* glXxx 는 `DeviceContext`(+자원 RAII Texture/Framebuffer) 안에만. IPassable/IRenderable/PassIterator 에 GL include ❌. 외부 라이브러리(efk/ImGui)는 ParticlePass/ImGuiPass + `InvalidateStateCache`.
- **`extern/sb7code` 수정 금지** · **주석 한국어+ASCII only**(특수문자 0) · **Phase 3 한 Pass씩** · **사용자 병렬 git** → 시작 시 git log/status 재측정.

---

## 10. Grounding 주의 (재검증 필수)

직전 `68506a8 [refactor] 렌더링 모듈 Rename` 으로 심볼/파일명 이동 가능. 착수 전:
- `src/render/render_passable/` 의 `IRenderPassable`/`SceneRenderer`/`CameraStage`/`ScreenQuadStage` 실제 파일·클래스명.
- `src/material/pass.h` 의 `RenderQueue` + `RenderStateBlock` 필드 + `DefaultRenderStateBlockOf`/`QueueOf`.
- `MeshPassProcessor`(`Process`의 lastProg/lastMat 배치 = A-dedup 이주 대상) / `DeviceContext`(mBoundProgram/mLast/ApplyRenderStateBlock/InvalidateStateCache) / `Material`(SetPass/GetPass) / `Camera` / `MeshRenderer` / `SpriteRenderer`(sprite_component.h) 현 시그니처.
- 다이어그램은 `doc/diagrams/` (not `doc/`).
> "보고를 믿지 말고 live 코드로 재확인."

## 11. Task 때 재검토(설득) 지점 — 구현 중 결정
- SkyboxPass 독립 vs WorldPass skybox-queue (역할군이면 독립).
- ParticlePass/ImGuiPass 내부 IRenderable 사용 여부 (역할 Pass 라 직접호출이면 불요).
- RenderableProcessor 공개 폭 (Submit/Sort/Process 최소).
- depth 키 = collect(Submit) 시점 동봉 (WorldPass 가 카메라 앎). IRenderable 3 메서드 유지.
- A-dedup 깊이: 우선 UseProgram dedup 만, FrameBlock/sampler 멱등 가드는 프로파일 후.

## 12. 포인터 (깊이 — optional)
- 진입점: `doc/마이그레이팅계획안.md` (v5).
- 연구: `doc/RenderingSystemRefactorReserch.md` (C1~C7 + §7 allocator — 단 ITargetAllocator/풀은 v5 에서 defer).
- 레퍼런스 명세: `doc/superpowers/plans/2026-06-22-reference-render-pipeline.md` (클린룸).
- 사용자 원안: `doc/렌더링시스템 흐름 리펙토링 계획안.md`.
- 다이어그램: `doc/diagrams/engine-migration-class.{svg,png}` (★ v5).
- 🔴 SUPERSEDED: `doc/superpowers/plans/2026-06-23-render-migration-implementation.md` (v4 기반, 재생성 필요).
- (모두 gitignored 로컬 — §0-1)

## 13. Change log
- 2026-06-23 작성. 설계 v4+D9 확정 시점.
- 2026-06-23 **v5 갱신**: 범위 분리(렌더 아키텍처만 / 백엔드·PSO·풀=후속). D10(ICommandRecorder 폐기, 구체 DeviceContext) · D11(ITargetAllocator/풀 defer) · D12(A-dedup) · D13(PSO/멀티백엔드=후속) 추가. Phase 1(기반 인터페이스) 제거 → 5단계. 다이어그램 `engine-migration-class.*` 신규. impl plan(render-migration-implementation) SUPERSEDED.
