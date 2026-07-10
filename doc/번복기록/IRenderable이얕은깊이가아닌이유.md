생각해보니 IPassable를 
IMeshPassable                     // Mesh(+Sprite) 수집+정렬 (★ 구 MeshPassProcessor 공개 메서드 이식)
ITextPassable                     // Text
IParticlePassable                 // Particle (Effekseer 컨텍스트)

얘네들이 구현하면 안될것 같다는것은 이해했다 다이아몬드 상속이 될 테니.

다시 정정한다. WorldPass 내부에 Renderable
Mesh(+Sprite) 수집+정렬 (★ 구 MeshPassProcessor 공개 메서드 이식)
Text
Particle (Effekseer 컨텍스트)

컨테이너만 들어있지. 다중 상속으로 넣는것은 실수인것 같기도 하다.

내가 너에게 제안했던 설계에 문제점을 말하고, 그래도 가져갈만한 사고가 있는지 말해줘
Show less

----
----
----
----

네 진단(다이아몬드) 맞고, 그보다 더 깊은 문제가 있습니다. 솔직히 짚고 — 살릴 사고도 분명 있습니다.

네가 제안한 "능력 인터페이스 다중구현" 의 문제
P1 — 다이아몬드 (네가 본 것): IMeshPassable : IPassable, ITextPassable : IPassable 면 WorldPass : IMeshPassable, ITextPassable 가 IPassable 을 두 번 상속 → virtual inheritance 필요 → 복잡·취약. ✅ 정확.

P2 — 호출자 없는 껍데기 인터페이스 (더 본질적): 다형 인터페이스가 정당하려면 호출자가 base 포인터로 dispatch 해야 합니다(design-decision-discipline §2). 그런데 누가 IMeshPassable* 로 dispatch 하나? PassIterator 는 오직 Draw()/GetPassResult()(=IPassable 계약)만 호출. 아무도 IMeshPassable 을 통해 호출하지 않음. → 삭제 테스트: IMeshPassable 인터페이스를 지우면 복잡도가 여러 호출자에 재등장하나? 아니. WorldPass 내부에서만 메시 수집을 씀. → IMeshPassable 은 얕은(shallow) 마커 인터페이스 — 인터페이스 ≈ 구현, 어댑터 1개뿐 = 가짜 seam.

P3 — 변동성 ≠ 다형성: "WorldPass 가 Mesh+Text 를 그린다" 는 런타임 swap 이 아니라 edit-time 고정(WorldPass 를 짤 때 메시 컨테이너+텍스트 컨테이너를 박음). 런타임에 WorldPass 가 갑자기 "이제 Particle Pass 입니다" 로 바뀌지 않음. → 인터페이스화 = 기능 +0, 복잡도 +1.

P4 — "IPassable 만 봐도 뭘 그리는지 안다" = 타입에게 묻기 안티패턴: 그 정보가 정말 필요하면 타입(dynamic_cast to IMeshPassable)이 아니라 데이터(Pass 가 가진 컨테이너가 비었나/카테고리 enum)로 묻는 게 맞음. 타입으로 능력 introspect = 냄새.

P5 — MeshPassProcessor 의 공개 메서드는 인터페이스 계약이 아님: 수집/정렬/발행은 WorldPass 의 구현 디테일(private 관심사)이지, 호출자가 알아야 할 계약이 아님. 이걸 IMeshPassable 계약으로 노출하면 구현이 인터페이스로 샘.

→ 한 줄: **"무엇을 그리는가"는 *타입(상속)*이 아니라 데이터(보유 컨테이너)로 표현해야 한다. 네 새 직관(WorldPass 안에 컨테이너만)이 정답.

그래도 가져갈 사고 3개 (살릴 것)
S1 — Renderable 의 카테고리 분리 는 유효: Mesh/Text/Particle/ImGui 가 서로 다른 그리기 메커니즘(VAO+drawElements / Effekseer Manager / ImGui draw data)이라는 통찰은 진짜. 이건 Renderable 쪽 축이고 살린다. (단 §아래 Q 참조 — 균일 Render() vs 카테고리 특화)

S2 — "Pass 가 무엇을 그리는지 선언" 도 유효 — 구조로: WorldPass 가 어떤 컨테이너를 가지냐 가 곧 "무엇을 그리는가"의 선언. 인터페이스 없이 클래스를 읽으면 보임. 네 의도("IPassable 보면 안다")를 멤버 구조로 실현 — 더 정직.

S3 — "Mesh 수집+정렬은 재사용 가능한 능력" 도 유효 — 합성으로: WorldPass·SkyboxPass·(미래 ShadowPass)가 똑같은 메시 수집+정렬 을 쓴다면 그건 진짜 공유 자산("어댑터 2개 = 진짜 seam"). 단 상속(IMeshPassable)이 아니라 합성된 멤버 객체 로: 각 Pass 가 MeshPassProcessor(또는 MeshDrawList)를 멤버로 보유. → 재사용은 얻고 다이아몬드/마커는 안 생김. 그리고 이건 이미 현 엔진이 하는 방식 — SceneRenderer 가 MeshPassProcessor mProcessor 를 멤버로 보유. (능력 인터페이스 → 합성 멤버 로 번역하면 끝)

교정된 형태 (합성)

class WorldPass : public IPassable {        // Pass 인터페이스는 IPassable *하나*
    // "무엇을 그리는가" = 보유 컨테이너 (구조가 곧 선언, S2)
    MeshPassProcessor       mMeshList;       // Mesh(+Sprite) 수집+정렬 (합성, S3 — 구 멤버 그대로)
    std::vector<TextRenderable*>  mTexts;    // Text (합성)
    // Particle 없음 → 그건 ParticlePass 가 자기 컨테이너로
public:
    void Draw(ICommandRecorder& rec, const Texture& before) override {
        auto rt = mPool->Acquire(Key(), mDesc);
        rec.BindTarget(*rt);
        mMeshList.SortAndProcess(rec, mCamera);   // 메시 수집·정렬·발행 (내부)
        for (auto* t : mTexts) t->Render(rec, mCamera);  // 텍스트
    }
    Texture GetPassResult() const override { ... }
    int BeforeIndex = -1;
};
class SkyboxPass : public IPassable { MeshPassProcessor mMeshList; ... };  // 메시 능력 = 멤버 재사용
class ParticlePass : public IPassable { /* Effekseer 컨텍스트 보유 */ ... };
Pass 인터페이스 = IPassable 단 하나. IMeshPassable/ITextPassable/IParticlePassable 폐기.
"능력" = 합성된 컨테이너/헬퍼 멤버. WorldPass=메시+텍스트 멤버, SkyboxPass=메시 멤버, ParticlePass=파티클 멤버.
재사용(메시 수집+정렬) = MeshPassProcessor 멤버 객체 (현 엔진 그대로).
→ 이건 design-decision-discipline 의 "composition > inheritance" + "변동을 데이터로 흡수" 정통. 그리고 현 SceneRenderer+MeshPassProcessor 합성 구조를 거의 그대로 WorldPass 로 옮기는 것 — 마이그레이션 비용도 ↓ (내 §3.1 권고가 Pass 쪽에선 부분 부활: MeshPassProcessor 는 멤버로 유지).

남은 결정 1개 (§8-Q 그대로)
S1 의 단서: IRenderable 을 균일 Render(rec) 로 둘지, 카테고리 특화로 둘지.

메시·텍스트는 Render(rec) 가 자연스럽지만, Particle(Effekseer)/ImGui 는 자기 컨텍스트라 균일 Render(rec) 가 어색(P5 와 같은 결). → 그 카테고리는 IRenderable 이 데이터만 들고, *해당 Pass(ParticlePass)*가 자기 컨텍스트로 그리는 게 나을 수 있음.
이 교정(능력 인터페이스 폐기 → IPassable 하나 + 합성 멤버, MeshPassProcessor 멤버 유지)으로 문서 §0 축2 + Phase 4/5 를 다시 손봐야 합니다. 이 방향 맞으면 그렇게 갱신하고, §8-Q(균일 Render vs 카테고리 특화)도 같이 확정할지 알려줘.

반영하자. 
그리고 std::vector<TextRenderable*>  mTexts
이 Text라는것도 일종의 Mesh로 이미 현 프로젝트에 구현되어 있지는 않은가를 한번 
확인해줘
Show less

----
----
----
----

Text 가 이미 Mesh 로 구현돼 있는지 먼저 확인하겠습니다 (SJH::text 모듈).

Bash Inspect text module for mesh-based rendering
IN
echo "== src/text ls ==" && ls -la src/text/ 2>/dev/null && echo "== grep Mesh/Quad/VAO/glDraw in src/text ==" && grep -rn "Mesh\|Quad\|VAO\|VertexLayout\|Buffer\|glDraw\|MeshRenderer\|Geometry" src/text/ | head -40

OUT
== src/text ls ==
total 64
drwxr-xr-x@  7 escatrgot  staff   224 Jun 19 13:27 .
drwxr-xr-x@ 23 escatrgot  staff   736 Jun 19 23:39 ..
-rw-r--r--@  1 escatrgot  staff   809 Jun 21 11:35 CMakeLists.txt
-rw-r--r--@  1 escatrgot  staff  5736 Jun 10 05:17 bitmap_font.cpp
-rw-r--r--@  1 escatrgot  staff  6382 Jun 10 05:17 bitmap_font.h
-rw-r--r--@  1 escatrgot  staff  3585 Jun 21 11:24 text_renderer.cpp
-rw-r--r--@  1 escatrgot  staff  5284 Jun 21 11:24 text_renderer.h
== grep Mesh/Quad/VAO/glDraw in src/text ==
Read text_renderer.h
핵심 발견 — Text 는 이미 Mesh(Sprite) 기반입니다. TextRenderer 가 글자마다 child Actor + SpriteRenderer 빌보드로 펼침. Sprite 가 SceneRenderer 의 mesh 경로로 수집되는지 확인:

Bash Find SpriteRenderer and how it is collected
IN
echo "== SpriteRenderer 위치/정의 ==" && grep -rn "class SpriteRenderer\|SpriteRenderer :" src/ | head; echo "== SpriteRenderer 가 MeshRenderer 인가, 별도 DrawCommand 인가 — SceneRenderer 수집 경로 ==" && grep -rn "SpriteRenderer\|MeshRenderer\|GetComponent" src/render/render_passable/render_passable.impls.cpp 2>/dev/null | head -20

OUT
== SpriteRenderer 위치/정의 ==
src/sprite/sprite_component.h:69:    class SpriteRenderer : public SJH::Scene::MeshRenderer
== SpriteRenderer 가 MeshRenderer 인가, 별도 DrawCommand 인가 — SceneRenderer 수집 경로 ==
src/render/render_passable/render_passable.impls.cpp:12: *  4. @c CollectFromActor DFS - Actor 트리를 순회하며 MeshRenderer / PassComponent 를 DrawCommand 로 변환.
src/render/render_passable/render_passable.impls.cpp:163:			if (auto *mr = actor.GetComponent<Scene::MeshRenderer>())
src/render/render_passable/render_passable.impls.cpp:179:			if (auto *pc = actor.GetComponent<Scene::PassComponent>())
결정적입니다 — SpriteRenderer : public MeshRenderer, 그리고 TextRenderer 는 글자마다 SpriteRenderer child 를 발행. CollectFromActor 는 GetComponent<MeshRenderer>() 로 수집하므로 Text = Sprite = Mesh, 전부 동일한 MeshRenderer 경로입니다. → TextRenderable/ITextPassable 불필요 (이미 Mesh). 네 통찰 정확. 이걸 반영해 문서를 v3 로 갱신합니다 (능력 인터페이스 폐기 → 합성 + Text=Mesh 단순화).

Write 마이그레이팅계획안.md
160 lines
# 렌더링 시스템 마이그레이션 계획안 (v3)

> 작성: 2026-06-23 (v3 — 능력 인터페이스 폐기→합성, Text=Mesh 발견 반영). 대상 명세 = [`doc/superpowers/plans/2026-06-22-reference-render-pipeline.md`](superpowers/plans/2026-06-22-reference-render-pipeline.md).
> 빌드/커밋 = **사용자**. 표기: **E**=확장 · **N**=신규 · **G**=갭 · ⚠️계약변경.

---

## ⚠️ 0. 내가 이해한 모델 (v3 — 교정 후)

### 0.0 발견 — **Text 는 이미 Mesh** (코드 확인됨)
- `src/sprite/sprite_component.h:69` → **`class SpriteRenderer : public SJH::Scene::MeshRenderer`** (Sprite 가 MeshRenderer).
- `src/text/text_renderer.h` → `TextRenderer` 는 글자마다 child Actor + `SpriteRenderer` 빌보드 발행.
- `render_passable.impls.cpp:163` → `CollectFromActor` 가 `GetComponent<Scene::MeshRenderer>()` 로 수집 → **Sprite·Text 가 다형으로 같은 mesh DrawCommand 경로**.
- → **결론: Mesh = Sprite = Text 는 단일 `MeshRenderer` 카테고리.** `TextRenderable`/`ITextPassable` **불필요**(추가 작업 0).

### 0.1 능력 인터페이스 폐기 (D4 교정)
이전 안(`WorldPass : IMeshPassable, ITextPassable, IParticlePassable` 다중구현)의 문제 → 폐기:
- **다이아몬드 상속** (능력 IF 가 IPassable 상속 시).
- **호출자 없는 껍데기 IF** — PassIterator 는 `Draw()`/`GetPassResult()`(=IPassable)만 호출. `IMeshPassable*` 로 dispatch 하는 호출자 없음 → 삭제 테스트 실패(얕은 마커).
- **변동성≠다형성** — "WorldPass 가 Mesh 를 그린다"는 edit-time 고정이지 runtime swap 아님.
- **MeshPassProcessor 공개 메서드 = 구현 디테일** — Pass 의 private 관심사, 인터페이스 계약 아님.

→ **"무엇을 그리는가" = 타입(상속)이 아니라 *데이터(합성 멤버)* 로 표현.**

### 0.2 최종 모델 — *두 축*

**축 1 — `IRenderable` : 그려지는 것의 카테고리 (D5)** — 단, 엔진 현실로 단순화:
```
IRenderable (개념 베이스 — 그려지는 모든 것)
├─ MeshRenderer          ← 이미 존재. Mesh + (SpriteRenderer:MeshRenderer)=Sprite + Text(글리프 스프라이트) + Skybox(mesh)
├─ ParticleRenderable    ← Effekseer 독자 컨텍스트 (별개)
└─ ImGuiRenderable       ← ImGui 독자 파이프라인 (별개)
```
> ※ Mesh 카테고리(Mesh/Sprite/Text/Skybox)는 *이미 MeshRenderer 단일 경로* — 신규 0. 진짜 별개는 Particle·ImGui 뿐.
> ※ §5-Q: 통합 베이스 `IRenderable` 이 *정말* 필요한가(아래 설득 지점) — 카테고리별 합성이면 mixed 순회가 없어 베이스가 얕을 수 있음.

**축 2 — Pass : `IPassable` *하나* + 합성 멤버 (D2/D3/D4 교정)**:
```
IPassable (Draw(rec,before) / GetPassResult()→const Texture* / BeforeIndex / OnResize)   ← 모든 Pass 공통, *유일* 인터페이스

// 구체 Pass = IPassable + "무엇을 그리는가"는 합성된 컨테이너/헬퍼 멤버:
WorldPass    : IPassable  { MeshPassProcessor mMeshList; }   // Mesh=Sprite=Text 전부 (정렬 흡수, 합성)
SkyboxPass   : IPassable  { MeshPassProcessor mMeshList; }   // mesh (또는 WorldPass 의 skybox queue 로 흡수)
ParticlePass : IPassable  { /* Effekseer Manager/Renderer 컨텍스트 */ }
PostFxPass   : IPassable  { /* before→자기 출력, 렌더러블 수집 안 함 */ }
ImGuiPass    : IPassable  { /* ImGui draw data */ }
```
- **재사용(Mesh 수집+정렬)** = `MeshPassProcessor` 를 *멤버로 합성* (구 `SceneRenderer` 가 이미 `mProcessor` 멤버 보유 — 그대로 이식). 상속 아님.
- **`PassIterator` 는 `vector<IPassable*>` 만** 안다. `main` 은 PassIterator 하나만 안다.

> 🟥 **이 v3 모델이 내 최종 이해. 어긋나면 교정 → 그 후 Phase 착수.**

---

## 1. 결정 확정표

| ID | 결정 | 선택 |
|----|------|------|
| D1 | 방식 | **in-place 재형성** (reference-render/ 독립 프로젝트 안 만듦) |
| D2 | IPassable 연쇄 | **plan 채택** (Draw/GetPassResult/BeforeIndex) |
| D3 | 네이밍 | **plan 이름** (IRenderPassable→IPassable) |
| D4 | Pass 능력 | **IPassable 하나 + 합성 멤버** (능력 인터페이스 ❌, 다이아몬드 회피) |
| D5 | IRenderable | **모든 drawable** (단 Mesh=Sprite=Text 이미 MeshRenderer; §5-Q 재검토) |
| D6 | RenderTarget | **쓰기 대상 추상** (Framebuffer/DefaultRenderTarget). Texture=attachment. `GetPassResult()`→`const Texture*` |
| D7 | RenderStateBlock | **Material 보유** (PassKind=seed + per-material override) |

---

## 2. 매핑 인벤토리 — E/N/G

| 개념 | 현 모듈 | 판정 | 작업 |
|---|---|---|---|
| `ICommandRecorder` | `DeviceContext` | **E** | 인터페이스 추출 + 상속 |
| `ITargetAllocator` | `ResourceRegistry` | **E** | 인터페이스 추출 + 구현(CreateFramebuffer/Texture 위임) |
| `IRenderTargetPool`+`DedicatedTargetPool` | (ResourceRegistry 근접) | **N(얇음)** | ITargetAllocator 위임 adapter (`Acquire`=`Find??Create`) |
| `TargetDesc` | (w/h 직접) | **N** | `{w,h,format}` |
| `IPassable` ⚠️ | `IRenderPassable` | **E⚠️** | 개명 + 계약(Draw/GetPassResult/BeforeIndex) |
| `MeshPassProcessor` (정렬) | `MeshPassProcessor` | 🟢→**합성 유지** | WorldPass/SkyboxPass 의 *멤버*로 이식 (인터페이스화 ❌) |
| `WorldPass` | `SceneRenderer`+`CameraStage` | **E→통합** | `WorldPass : IPassable { MeshPassProcessor; }` 단일 클래스. SceneRenderer 본체(BeginFrame→Collect→Sort→Process) 이식 |
| `PostFxPass` | `PassComponent`+`ScreenQuadStage` | **E→통합** | `PostFxPass : IPassable` 단일 |
| `ParticlePass` | `ParticleStage` | **E** | `IPassable` + efk 컨텍스트 멤버 |
| `ImGuiPass` | ImGui 직접 호출(main) | **E/N** | `IPassable` 로 감쌀지(§5-Q) |
| `PassIterator` | `main.cpp` `mStages` | **E→모듈화** | `vector<IPassable*>` 클래스. main 은 이것만 |
| `IRenderable`(베이스) | `MeshRenderer`(+Sprite) 존재 / Particle·ImGui 산재 | **E+N(선택)** | Mesh 카테고리=MeshRenderer(완료). Particle/ImGui 통합 베이스는 §5-Q |
| `Camera` | `Scene::Camera` | **E🟢** | CullingMask+TargetRT 보유. Sees() 헬퍼만 |
| `RenderTarget` | `RenderTarget`/`DefaultRenderTarget` | 🟢유지 | 쓰기 대상(D6) |
| `Texture`(attachment) | `Texture`/`Framebuffer::GetColorAttachment` | 🟢유지 | GetPassResult 반환 |
| `RenderStateBlock` | `Pass::RenderStateBlock` | 🟢→**Material 이주** | 정의 유지, 보유처 Material(D7) |
| `Material` | `Material` | **E** | RenderStateBlock 멤버(seed+override) |

> 🟢 보너스 자산(유지): `Pass::RenderStateBlock`(ROP) · `DeviceContext` 상태캐싱 · `SceneContext`(Camera/Light 레지스트리) · **Text=Sprite=Mesh 단일 경로**.

---

## 3. Phase 계획 (in-place, 저위험→계약→통합→배선)

### Phase 1 — Allocator/Recorder/Pool 기반 [E+N, 비파괴]
- **N** `ICommandRecorder`(DeviceContext 추출) → `DeviceContext : ICommandRecorder`.
- **N** `ITargetAllocator` → `ResourceRegistry : ITargetAllocator`.
- **N** `IRenderTargetPool`+`DedicatedTargetPool`(ITargetAllocator 위임), `TargetDesc{w,h,format}`.
- 검증: 빌드 GREEN, 동작 변화 0.

### Phase 2 — Material 이 RenderStateBlock 보유 [E, D7]
- **E** `Material`: `RenderStateBlock` 멤버 + `SetPass` 가 `DefaultRenderStateBlockOf` 로 seed + override setter. 소비처(MeshPassProcessor/DeviceContext)가 `material->GetRenderStateBlock()` 사용.
- 검증: 빌드 GREEN + 육안(seed=기존 도출 → 무회귀).

### Phase 3 — `IPassable` 계약 변경 [E⚠️, D2/D3]
- **E⚠️** `IRenderPassable`→`IPassable` 개명 + `Draw(rec,before)`/`GetPassResult()→const Texture*`/`BeforeIndex`/`OnResize`.
- 검증: 빌드 GREEN (구체 Pass 미전환 시 임시 어댑터 허용).

### Phase 4 — 구체 Pass 통합 [E→통합] — *한 Pass씩 + 육안*
- **E** `WorldPass : IPassable { MeshPassProcessor mMeshList; }` ← `SceneRenderer`+`CameraStage` 흡수. `Draw` 안에서 Pool 로 출력 RT → mesh 수집·정렬·발행(Mesh/Sprite/Text 전부). `GetPassResult()`=출력 FB color attach.
- **E** `SkyboxPass : IPassable` (mesh — 또는 WorldPass skybox queue 흡수 검토).
- **E** `ParticlePass : IPassable` ← `ParticleStage` 흡수 (efk 컨텍스트 멤버).
- **E** `PostFxPass : IPassable` ← `PassComponent`+`ScreenQuadStage` 흡수 (`before`→자기 출력).
- 검증: **Pass 하나 전환마다** 빌드 GREEN + 육안(출력 동일).

### Phase 5 — `PassIterator` 모듈화 + main 배선 + 디버그 오버레이 [E]
- **N** `PassIterator{ Add(IPassable*)/Execute(rec,sceneRaw)/Present(rec,sceneRaw)/DebugPassIndex }`.
- **E** `main.cpp`: `mStages` 직접 순회 → `mPassIterator` 하나. 각 Pass `BeforeIndex` 배선.
- 신규 `DebugPassIndex` 런타임 토글(임의 Pass 결과 화면, Dedicated 라 생존).
- 검증: 빌드 GREEN + 육안(평소 동일 + 디버그 키).

### Phase 6 — (선택) `IRenderable` 통합 베이스 [E+N, D5 — §5-Q 결정 후]
- Particle/ImGui 를 IRenderable 카테고리로 정식화할지. (Mesh 는 이미 MeshRenderer)
- §5-Q 에서 "통합 베이스가 얕다" 결론이면 *축소/생략*.

### Phase 7 — 정리/네이밍/문서 [E]
- 구 심볼 정리, `doc/diagrams/` 현 엔진 버전 갱신, 메모리/CLAUDE.md.

---

## 4. 검증 / Out of Scope / 가드레일

**검증**: Phase 후 *사용자* 빌드(`cmake --build --preset ninja --target _MyApp_`, slang PATH) + 컴파일 출력 육안. Phase 2·4·5 GUI 회귀 육안. `SortMultiStage` stable_sort 유지.

**Out of Scope**: Shadow/Deferred/MSAA/HDR/Cubemap · 실 GL 백엔드 신규(DeviceContext 가 이미) · 다중 BeforeIndex · data-driven JSON · `ITextPassable`/`TextRenderable`(Text=Mesh 라 불필요).

**가드레일**: 빌드·커밋=사용자(path-scoped, `git add -A` 금지, Co-Authored-By 미사용) · 🔒 GL 격리(glXxx 는 DeviceContext+자원 RAII 안에만) · sb7 수정 금지 · 주석 한국어+ASCII · Phase 4 한 Pass씩 · 사용자 병렬 git → HEAD 재측정.

---

## 5. Task 때 재검토(설득) 지점

- **§5-Q `IRenderable` 통합 베이스 필요성**: Pass 가 카테고리별 합성 컨테이너(WorldPass=MeshPassProcessor, ParticlePass=efk)를 가지면 *mixed `vector<IRenderable*>` 순회가 없음* → 통합 베이스 `IRenderable` 이 얕은 마커일 수 있음(삭제 테스트). D5("모든 것 IRenderable")를 *개념*으로만 두고 구현은 카테고리별로 갈지 Task 때 결정. (메시는 이미 MeshRenderer)
- **SkyboxPass 독립 vs WorldPass 흡수**: Skybox 도 mesh → 별 SkyboxPass(IPassable) vs WorldPass 내부 skybox queue. 후자가 단순할 수 있음.
- **MeshPassProcessor 인터페이스 폭**: WorldPass 멤버로 둘 때 공개 API 최소화(SortAndProcess 한두 개)로 캡슐화 강화.
- **ImGuiPass 래핑**: ImGui 를 IPassable 로 감쌀지(체인 일관) vs main 직접 호출 유지(현행). 독자 파이프라인이라 thin 래퍼로 충분할 수도.

---

## 6. 요약

- **Pass = `IPassable` 하나 + 합성 멤버** (능력 인터페이스/다이아몬드 폐기). "무엇을 그리는가"=보유 컨테이너(데이터).
- **Mesh=Sprite=Text 는 이미 `MeshRenderer` 단일 경로** → Text 작업 0, 별개는 Particle·ImGui 뿐.
- **`MeshPassProcessor` 는 WorldPass 의 합성 멤버**로 유지(현 SceneRenderer 구조 이식).
- 큰 작업 = Phase 4(SceneRenderer/CameraStage·PassComponent/ScreenQuadStage → WorldPass/PostFxPass 통합) + Phase 5(PassIterator).
- D6 RenderTarget=쓰기대상/Texture=attachment · D7 RenderStateBlock→Material.
- §0 모델 확인 → §5 설득지점 인지 → Phase 1 착수.
doc/마이그레이팅계획안.md v3 갱신 완료.

네 두 지적 반영
능력 인터페이스 폐기 (IMeshPassable/ITextPassable/IParticlePassable) → IPassable 하나 + 합성 멤버. "무엇을 그리는가"는 *타입(상속)*이 아니라 *데이터(보유 컨테이너)*로. 다이아몬드·껍데기 마커 문제 제거.
Text = Mesh 확인됨 (코드 근거):
SpriteRenderer : public MeshRenderer (sprite_component.h:69)
TextRenderer → 글자마다 SpriteRenderer child
CollectFromActor → GetComponent<MeshRenderer>() 로 다형 수집
→ Mesh = Sprite = Text 단일 MeshRenderer 경로 → TextRenderable/ITextPassable 불필요, 추가 작업 0. 진짜 별개는 Particle·ImGui 뿐.
v3 최종 모델
Pass: IPassable 유일. WorldPass{MeshPassProcessor} / ParticlePass{efk} / PostFxPass / ImGuiPass — 합성.
MeshPassProcessor = WorldPass 의 합성 멤버로 유지 (현 SceneRenderer 가 이미 mProcessor 멤버 → 그대로 이식, 인터페이스화 안 함).
Phase: 1 기반IF → 2 Material+RenderStateBlock → 3 IPassable 계약 → 4 구체 Pass 통합(한 개씩) → 5 PassIterator+디버그오버레이 → 6 (선택)IRenderable 베이스 → 7 정리.
정직하게 남긴 설득 지점(§5) — Task 때
IRenderable 통합 베이스가 정말 필요한가: 카테고리별 합성이면 mixed vector<IRenderable*> 순회가 없어 베이스가 얕은 마커일 수 있음(삭제 테스트). D5 를 개념으로만 두고 구현은 카테고리별로 갈지 — 이게 가장 재검토 필요. (메시는 이미 MeshRenderer)
SkyboxPass 독립 vs WorldPass 흡수 / MeshPassProcessor 인터페이스 폭 / ImGuiPass 래핑.
§0 v3 모델이 네 의도와 맞는지 확인해주면(특히 §5-Q 의 IRenderable 통합 베이스를 지금 확정할지 Task 때로 미룰지), Phase 1(비파괴)부터 착수하겠습니다.

MeshPassProcessor를 좀더 Flat 하게 만들어.
그렇게 하면 Skybox도 굳이 비대한 Mesh 렌더링을 할 필요 없다는 점에서
IRenderable 통합이 좀더 쉬울것이라 생각했는데..

IRenderable이 얕은 마커일 수도 있겠다고 생각이 드네
즉 공통점은 오직 Render() 이라는 함수 하나이고,

하지만 얕은 마커라고 해도 이게 가능한가?
Text, Mesh 들의 순서가 Layer 값에 따라서 앞에 그려지고 뒤에 그려지고가 가능한가? 라는 말이야
즉 무슨 Mesh 앞에 Text가 있을수도 Text앞에 Mesh가 있을수도 이렇게 유연한 Draw Order를 보장하는것 말이야.

Stencil, Depth, Blend 등등도 고려 요소인데 IRenderable이 너무 얕게 느껴진다면
실제로 어떤 순서로 그림이 덮어씌워지나에 대한 정보는 모두 IRenderable 단일로 결정될 수 있으면 좋겠어.
그리고 Material에 RenderStateBlock에 들어있던것을 그럼 어떻게 밖으로 꺼내냐 라고 한다면
그냥 Material도 RenderStateBlock 접근 인터페이스를 만들고 IRenderable도RenderStateBlock 접근 인터페이스를 만들어서 Facade 관계로 만드는것이야

인터페이스 분리를 한다는것이지.

여기서 이해 안되는 내용이 있거나, 의사결정에 있어서 추가 정보가 필요하면 질의해
Show less

----
----
----
----

네 제안이 오히려 설계를 더 깨끗하게 만들고, 네 스스로의 "얕은 마커" 우려를 해소합니다 — 핵심을 짚어보면:

네 통찰이 맞다 — 그리고 그게 IRenderable 을 얕지 않게 만든다
"유연한 Draw Order (Mesh↔Text 자유 교차)를 IRenderable 단일로 결정" 하려면, 단일 정렬 리스트에 모든 IRenderable 을 넣고 각자의 키로 정렬해야 합니다. 그러려면 IRenderable 이 다음을 반드시 노출해야 함:

Render(rec, cam) — 자기 그리기
QueueLayer() (+depth) — 그리는 순서 키
GetRenderStateBlock() — ROP(Stencil/Depth/Blend)
→ 즉 "order 를 IRenderable 단일로 결정하고 싶다"는 요구 자체가 IRenderable 에 3가지 의미를 부여 → 얕은 마커가 아님. 네 질문("얕아도 이게 가능한가")의 답: 가능하게 하려면 얕지 않아야 하고, 그 3개가 정확히 그 깊이다. 자문자답이 맞아떨어집니다.

크리스털라이즈된 모델 (네 제안 정리)

struct RenderStateBlock { /* stencil/depth/blend/cull (기존) */ };   // 값

class IRenderStateProvider {                       // ★ ROP 접근 인터페이스 (네가 말한 Facade 분리)
    virtual const RenderStateBlock& GetRenderStateBlock() const = 0;
};
class Material : public IRenderStateProvider {     // *저장처* (D7) — PassKind seed + override
    RenderStateBlock mState;
    const RenderStateBlock& GetRenderStateBlock() const override { return mState; }
};
class IRenderable : public IRenderStateProvider {  // 그려질 것 + ROP *facade*
    virtual void Render(ICommandRecorder&, const Camera&) const = 0;
    virtual int  QueueLayer() const = 0;
    // Mesh 계열: GetRenderStateBlock() → mMaterial->GetRenderStateBlock() (facade 위임)
    // (Material 없는 것: 자기 것 반환)
};
MeshPassProcessor → 제네릭 RenderableProcessor(Flat): IRenderable* 수집 → QueueLayer(+depth) 정렬 → 각자 ApplyRenderStateBlock(r->GetRenderStateBlock()) + r->Render(). Skybox = 그냥 Skybox-queue 의 평범한 IRenderable (비대한 mesh 특수경로 0). ✅ 네 의도.
이러면 Mesh/Sprite/Text 가 QueueLayer 로 자유 교차(Mesh 앞 Text, Text 앞 Mesh 모두 OK). Stencil/Depth/Blend 도 각 IRenderable 의 RenderStateBlock 으로 per-draw 결정.
여기까지는 네 설명 그대로이고 동의합니다. 딱 하나 결정이 필요합니다 — Effekseer/ImGui 의 독자 컨텍스트 때문:

AskUserQuestion
OUT
Your questions have been answered: "Particle(Effekseer)·ImGui 도 단일 정렬 리스트에 IRenderable 로 넣을까요? — 이 둘은 자체 파이프라인(Effekseer Manager 일괄 Draw, ImGui 일괄 Draw)이라 GL state 를 캐시 뒤에서 건드립니다"="이점도 고민되는 부분인데
Effekseer과 ImGUI는 외부 라이브러리이지만, 
class IRenderable : public IRenderStateProvider {  // 그려질 것 + ROP *facade*
    virtual void Render(ICommandRecorder&, const Camera&) const = 0;
    virtual int  QueueLayer() const = 0;
    // Mesh 계열: GetRenderStateBlock() → mMaterial->GetRenderStateBlock() (facade 위임)
    // (Material 없는 것: 자기 것 반환)
};

이 부분이 조작 가능하도록 라이브러리의 깊은 구현부 참조를 가져오도록 확장하도록 하고 싶어
그렇게 해야 쓸데없는 함수를 내려받는 리스코프 치환 원칙도 잘 유지할 수 있을것 같아.", "위 'Flat RenderableProcessor + IRenderable{Render/QueueLayer/GetRenderStateBlock} + Material·IRenderable 둘 다 IRenderStateProvider(Facade)' 크리스탈라이즜이 네 의도와 맞나요?"="이건 좀더 설명을 해줘". You can now continue with these answers in mind.
좋습니다 — Q1 은 통합 리스트 + 라이브러리 깊은 구현부 참조를 concrete 클래스가 보유 로 이해했고, 그게 정확히 LSP 를 지키는 방법입니다. Q2 더 설명해달라 하셨으니, 모델을 끝에서 끝까지 풀어 설명합니다.

4개 빌딩블록 + 왜

// ① 값 — ROP 상태 (이미 존재: Pass::RenderStateBlock)
struct RenderStateBlock { /* Depth(test/write/func) · Cull · Blend · Stencil */ };

// ② Facade seam — "나는 ROP 상태를 *줄 수* 있다"
struct IRenderStateProvider {
    virtual const RenderStateBlock& GetRenderStateBlock() const = 0;
    virtual ~IRenderStateProvider() = default;
};

// ③ 저장처 — Material (D7). PassKind 가 seed, 이후 per-material override
class Material : public IRenderStateProvider {
    RenderStateBlock mState;
    const RenderStateBlock& GetRenderStateBlock() const override { return mState; }
};

// ④ 그려질 것 — IRenderable (LSP-최소 인터페이스: 딱 3개)
class IRenderable : public IRenderStateProvider {
    virtual void Render(ICommandRecorder& rec, const Camera& cam) const = 0;  // 자기 그리기
    virtual int  QueueLayer() const = 0;                                       // 그리는 순서 키
    // GetRenderStateBlock() ← IRenderStateProvider 상속 (ROP)
};
Facade 관계 — 구체적으로
"Material 도 RenderStateBlock 접근 IF, IRenderable 도 접근 IF, Facade" 를 코드로:


// Mesh 계열 — ROP 를 Material 로 *위임(facade)*
class MeshRenderable : public IRenderable {
    Mesh* mMesh; Material* mMaterial;          // 비소유
    int QueueLayer() const override { return mMaterial->GetQueueLayer(); }
    const RenderStateBlock& GetRenderStateBlock() const override {
        return mMaterial->GetRenderStateBlock();   // ★ Material 이 진짜 저장처, IRenderable 은 통로
    }
    void Render(ICommandRecorder& rec, const Camera& cam) const override {
        rec.UseProgram(*mMaterial->GetProgram()); /* uniforms */ rec.BindVAO(...); rec.DrawIndexed(...);
    }
};
→ IRenderable 과 Material 이 같은 IRenderStateProvider 인터페이스를 공유하고, Mesh 계열은 자기 것을 Material 에 위임. 호출자(아래 Processor)는 "이게 Material 에서 왔는지 자체 보유인지" 모른 채 r->GetRenderStateBlock() 한 줄. = Facade.

Effekseer/ImGui — 깊은 구현부 참조를 concrete 가 보유 (Q1 + LSP)

// Particle — Effekseer 깊은 구현부 참조는 *concrete 의 private 멤버* (인터페이스엔 안 샘)
class ParticleRenderable : public IRenderable {
    Effekseer::ManagerRef mEfkManager;     // ★ 라이브러리 깊은 구현부 참조
    RenderStateBlock      mOwnState;       // 파티클 자체 ROP (Material 없음 → 자기 것 반환)
    int QueueLayer() const override { return /* particle 레이어 */; }
    const RenderStateBlock& GetRenderStateBlock() const override { return mOwnState; }
    void Render(ICommandRecorder& rec, const Camera& cam) const override {
        rec.InvalidateStateCache();        // foreign-GL 경계 (efk 가 GL state 건드림)
        mEfkManager->Draw(/* deltaFrame */);
        rec.InvalidateStateCache();
    }
};
// ImGuiRenderable 도 동일 패턴 (ImGui draw data 참조 private + 자기 ROP)
LSP 보존의 핵심 (네가 짚은 지점): IRenderable 인터페이스가 딱 3개(Render/QueueLayer/GetRenderStateBlock) 만 가지므로, Mesh·Particle·ImGui 모든 서브타입이 그 3개를 전부 의미 있게 구현. 누구도 쓸데없는 함수를 stub 하지 않음.

🚫 만약 IRenderable 에 GetMesh()/GetVAO() 를 넣었다면 → ParticleRenderable 이 return nullptr 강제 = LSP 위반.
✅ 라이브러리별 깊은 참조(efk Manager, ImGui data)는 concrete 의 private 에. 인터페이스로 안 새고, 서브타입마다 자기 방식으로 Render() 만 충족.
Flat RenderableProcessor — 구 MeshPassProcessor 의 일반화

class RenderableProcessor {                 // mesh 특수 0 — IRenderable* 만 안다
    std::vector<IRenderable*> mItems;
    void Submit(IRenderable* r) { mItems.push_back(r); }
    void Sort();                            // QueueLayer asc → (투명) depth back-to-front
    void Process(ICommandRecorder& rec, const Camera& cam) {
        for (auto* r : mItems) {
            rec.ApplyRenderStateBlock(r->GetRenderStateBlock());  // ROP (facade)
            r->Render(rec, cam);                                   // 자기 그리기
        }
    }
};
→ Skybox = Skybox-queue 의 평범한 IRenderable (비대한 mesh 특수경로 0). Mesh/Sprite/Text/Skybox/Particle/ImGui 가 한 정렬 리스트에서 QueueLayer 로 자유 교차. "어떤 순서로 덮어씌우나" = 이 리스트 단일로 결정. ✅ 네 요구.

★ 중요한 귀결 — Pass 가 더 줄어든다 (확인 필요)
모두가 IRenderable 이고 한 Processor 가 다 처리하면, WorldPass/SkyboxPass/ParticlePass/ImGuiPass 가 전부 하나로 합쳐집니다:


class ScenePass : public IPassable {       // = 3축 결합자 (Camera x IRenderable[] x Target)
    Camera* mCamera; IRenderTargetPool* mPool; RenderableProcessor mProc;
    void Draw(ICommandRecorder& rec, const Texture& before) override {
        auto rt = mPool->Acquire(Key(), desc);
        rec.BindTarget(*rt); /* clear */
        // 씬에서 IRenderable 수집(camera.cullMask 필터) → mProc.Submit
        mProc.Sort();
        mProc.Process(rec, *mCamera);       // mesh/sprite/text/skybox/particle/imgui 전부
    }
};
// 남는 별도 Pass = PostFxPass : IPassable (스크린 효과, before→출력)
WorldPass·SkyboxPass·ParticlePass·ImGuiPass → ScenePass 하나로 흡수 (전부 그 IRenderable 리스트 원소).
별도로 남는 건 PostFxPass(스크린 효과 — 렌더러블 아님, before→출력) + 카메라/타깃이 다른 추가 ScenePass(미니맵 등).
이게 연구의 3축 결합자(Camera×IRenderable×Target) 의 가장 깨끗한 실현.
end-to-end 흐름

PassIterator.Execute(rec, sceneRaw)
└─ ScenePass.Draw(rec, before)              // idx0, BeforeIndex=-1
   ├─ rt = pool.Acquire("Scene", desc); rec.BindTarget(rt)
   ├─ 씬 IRenderable 수집(cullMask) → proc.Submit (mesh/sprite/text/skybox/particle/imgui)
   ├─ proc.Sort()                            // QueueLayer+depth
   └─ proc.Process: for r → ApplyRenderStateBlock(r.GetRenderStateBlock()) → r.Render()
└─ PostFxPass.Draw(rec, before=ScenePass.GetPassResult())   // idx1, BeforeIndex=0
최종 = passes.back().GetPassResult() → Present
확인 요청
이 설명(4블록 + Facade + LSP-최소 IRenderable + Flat Processor)이 네 의도와 맞나?
★ Pass 합쳐짐 귀결: WorldPass/SkyboxPass/ParticlePass/ImGuiPass 를 ScenePass 하나로 흡수(전부 IRenderable) — 이 방향 OK? 아니면 일부는 Pass 로 남기고 싶나?
(정보) Effekseer/ImGui 의 Render() 가 외부 라이브러리 GL 을 건드리므로 그 안에서 InvalidateStateCache — 즉 그 concrete IRenderable 이 foreign-GL 경계가 됨. GL 격리 불변식은 "우리 glXxx 는 DeviceContext 안에만"이라 유지되지만, 외부 라이브러리 호출은 ParticleRenderable/ImGuiRenderable 에 격리됨. 이 점 동의?
이 3개 정리되면 마이그레이팅계획안.md 를 이 모델(ScenePass 흡수 + IRenderStateProvider Facade + Flat Processor)로 v4 갱신하겠습니다.

1. 이 설명(4블록 + Facade + LSP-최소 IRenderable + Flat Processor)가 내 의도가 맞다.
2. 모든것을 Pass로 넣으면 안된다는게 내 생각 분명 깔끔하지만, 
Pass의 순서가 정말 최종적으로 그려지는 순서를 담당한다고 생각해서
이들은 역할군으로 분리하는게 가독성이 좋을 것 같음.
정말 큼지막한 렌더링 순서 의도를 명시적으로 남겨놓는다 라는 개념이야.
3. 그런 이유로 Effekseer/ImGui 도 별개의 Pass를 남긴다는게 내 생각이다.
Show less

----
----
----
----

완벽히 이해했습니다. 이게 핵심을 두 레벨 순서로 깔끔히 가릅니다 — 그리고 이건 엔진 정통(Unity RenderPassEvent 코스 슬롯 + per-pass sort / Unreal EMeshPass + sort key)과도 일치합니다:

코스(coarse) 순서 = Pass 순서 (PassIterator) — 역할군으로 "큰 그림 렌더 순서 의도"를 명시적으로 남김. Skybox→World→Particle→ImGui→PostFx.
파인(fine) 순서 = QueueLayer (Pass 내부 RenderableProcessor) — WorldPass 안에서 Mesh/Sprite/Text 자유 교차.
→ 네 이전 요구("Mesh앞 Text, Text앞 Mesh 유연")는 파인(WorldPass 내부)에서, "큰 그림 순서 명시"는 코스(Pass 순서)에서. 둘 다 만족. Particle/ImGui 는 자기 역할 Pass → foreign-GL 경계도 그 Pass 에 격리. 이 모델로 v4 갱신합니다.