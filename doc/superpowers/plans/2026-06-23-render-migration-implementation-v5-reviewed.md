# 렌더링 시스템 마이그레이션 구현 Plan (v5-reviewed)

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax.
>
> 🟢 이 문서는 초안 `2026-06-23-render-migration-implementation-v5.md` 를 **4-렌즈 적대적 리뷰**(설계충실/grounding/완결성/빌드순서) 후 교정한 *정본*. 원본은 프리스틴 스냅샷으로 보존. 주요 교정: UseProgram void+dedup(bool 폐기) · 잎 ApplyRenderStateBlock 중복 제거 · Camera 네임스페이스 · Task 선행성 · draw_ops.h 공유 헬퍼 · Phase 3.0 원자 교체 분리 · Pass 스켈레톤 · PassIterator 완전 코드.

## ✅ Execution Status (2026-06-23, subagent-driven 실행 중 — Opus orchestrate / Sonnet implement)

| Phase/Task | 상태 | 커밋 |
|---|---|---|
| **Phase 1** | | |
| 1.1 IRenderStateProvider | ✅ 완료 (+gl3w 격리 fix) | `d9a2ee4` (+미커밋 fix) |
| 1.2 Material ROP + Facade | ✅ 완료 | `7853888` |
| **Phase 2** | | |
| 2.1 IRenderable | ✅ 완료 | `107f728` |
| 2.2 DeviceContext UseProgram dedup | ✅ 완료 | `107f728` |
| 2.3 draw_ops.h + MeshRenderer 잎 | ✅ 완료(커밋) | `107f728` 묶음 |
| 2.4 RenderableProcessor 전환(실제 경로) | ✅ **빌드 GREEN + 육안 무회귀 + 커밋** | `c4d11f9` |
| **Phase 3~5** | ⬜ 대기 (다음=3.0 원자 교체) | — |

- **HEAD = `c4d11f9`** · working tree CLEAN. Phase 1~2 전량 커밋, 전체 빌드 GREEN, GUI 회귀 0 확인(사용자).
- **미커밋: 없음.** Task 2.4 + gl3w 격리 fix(`i_render_state_provider.h`)는 `c4d11f9` 에 포함.
- **ℹ️ 사용자 병렬 작업**: `light_ubo_uploader.cpp`(M) · `resources/shader/*`(D 삭제, Slang 이행)를 사용자가 `c4d11f9` 에 본인 판단으로 함께 커밋 — 분리 대상 아님.
- **실행 핸드오프**: [`doc/handoffs/2026-06-23/2026-06-23-render-migration-EXECUTION-resume-handoff.md`](../../handoffs/2026-06-23-render-migration-EXECUTION-resume-handoff.md).
- **실행 교훈**: ① clangd 가 rename-stale + 헤더 compile-DB 부재로 대량 *거짓에러* 표시 — disk/빌드가 진실(무시). ② 인터페이스 헤더 GL 격리 필수(i_render_state_provider 가 pass.h→gl3w.h 누출 → Effekseer 클라 빌드 실패 → 전방선언 fix). ③ ScreenQuad 전이 부채는 `[TRANSITIONAL-3.5]` grep 태그로 마킹(Phase 3.5 제거).

---

**Goal:** 현 렌더 모듈을 v5 목표 형태로 *in-place* 재형성, **렌더 결과(픽셀) 변화 0**. 고수준은 **구체 `DeviceContext&`**(ICommandRecorder 없음), 배치는 **A-dedup**(DeviceContext 캐시).

**Architecture:** Material ROP(D7) → IRenderable+Flat Processor(D5/D12) → IPassable 역할군(D2/D3/D4) → PassIterator(D8) → 정리. 백엔드 Facade·PSO·RT풀 = **별도 후속 프로젝트**(범위 밖, D10/D11/D13).

**Tech Stack:** C++17, OpenGL 4.1(gl3w), GLM, CMake(Ninja), Slang→GLSL410. Meyer 싱글톤 `DeviceContext`/`ResourceRegistry`, Actor/Component.

**정본 설계:** [`doc/마이그레이팅계획안.md`](../../마이그레이팅계획안.md) (v5, D1~D13) + [`<doc>/handoffs/2026-06-23-...-resume-handoff.md`](../../handoffs/2026-06-23-rendering-migration-design-resume-handoff.md).

---

## ⚠️ 프로세스 규약 (writing-plans 기본 TDD 차이)

테스트(Catch2) 폐기 상태, 검증 = **사용자 빌드 + 육안 회귀**(memory `no_auto_tests`). 각 Task:
1. **빌드 = 사용자**: `export PATH="$HOME/slang/bin:$PATH" && cmake --build --preset ninja --target _MyApp_` → 출력 붙이면 에이전트 육안.
2. **실행 = 사용자**: `cd build_ninja/apps/_MyApp_ && ./_MyApp_` → GUI 직전과 동일 육안.
3. **커밋 = 사용자** path-scoped, `git add -A` ❌, `Co-Authored-By` ❌.
에이전트는 구현+보고만.

## 🔒 가드레일

- **GL 격리**: gl* 는 `DeviceContext`(+자원 RAII) 안에만. 인터페이스 헤더(IRenderStateProvider/IRenderable/IPassable/PassIterator)에 GL include ❌. 외부(Effekseer/ImGui)는 ParticlePass/ImGuiPass + `InvalidateStateCache`.
- **주석 한국어+ASCII only**. **신규 헤더 가드 = `__SJH_<FILE>_H__`** 패턴(기존 prefix `__SJH_` 일관).
- **`extern/sb7code` 수정 금지**. **Phase 3 한 Pass씩**(3.0 원자 교체 제외) 빌드+육안.
- **착수 전 HEAD 재측정**. 본 plan 시그니처 = grounding(2026-06-23, HEAD `68506a8`) 기준.

---

## Task 선행 그래프 (빌드 GREEN 순서 — 리뷰 L3/L4 교정)

```
P1: 1.1 -> 1.2
P2: 2.1(IRenderable) -> 2.2(UseProgram dedup) -> 2.3(MeshRenderer 잎 + draw_ops.h) -> 2.4(RenderableProcessor)
P3: 3.0(원자 인터페이스 교체, ONE 커밋) -> 3.1(WorldPass) -> 3.2(SkyboxPass) -> 3.3(ParticlePass) -> 3.4(ImGuiPass) -> 3.5(PostFxPass)
P4: 4.1(PassIterator) -> 4.2(main 배선)
P5: 5.x 정리
```
각 Task 는 *직전 Task 빌드 GREEN 후* 착수. P2 는 2.1->2.4 엄격 순서(컴파일 의존).

---

## File Structure

### 신규 (N)
| 파일 | 책임 |
|---|---|
| `src/material/i_render_state_provider.h` | ROP Facade 인터페이스 |
| `src/render/i_renderable.h` | `IRenderable : IRenderStateProvider` |
| `<src>/render/draw_ops.h` | 공유 free 헬퍼(UploadMaterialUboMembers/BindSamplers) — 잎+ScreenQuad 공용 |
| `src/render/mesh_renderer.cpp` | MeshRenderer::Render 잎 (현재 헤더온리) |
| `src/render/pass_iterator.h` / `.cpp` | PassIterator |

### 수정 (E)
| 파일 | 변경 |
|---|---|
| `src/material/material.h` | `: IRenderStateProvider`, `mState`, SetPass seed, GetRenderStateBlock, CopyFrom |
| `src/render/mesh_renderer.h` | `: Component, IRenderable` + 3 메서드 |
| `src/render/device_context.cpp` | `UseProgram` 내부 dedup 가드(헤더 시그니처 void 유지) |
| `src/render/mesh_pass_processor.{h,cpp}` | → `RenderableProcessor`(IRenderable* + 전이 ScreenQuad) |
| `src/render/render_passable/render_passable.h` | `IRenderPassable`→`IPassable` *in-place 클래스 개명*(파일 유지) |
| `src/render/render_passable/render_passable.impls.{h,cpp}` | `WorldPass`/`SkyboxPass`/`PostFxPass` 등 |
| `apps/_MyApp_/src/VFX/ParticleStage.{h,cpp}` | → `ParticlePass : IPassable` |
| `apps/_MyApp_/main.cpp` | `mStages`→`PassIterator`; ImGui→`ImGuiPass` |
| `src/render_bootstrap/render_pipeline.{h,cpp}` | SetupDefaultPipeline → PassIterator 조립 |
| `src/render/CMakeLists.txt` | `mesh_renderer.cpp`+`pass_iterator.cpp` target_sources |

> ※ `Camera::Sees()` 헬퍼는 **불요**(CollectFromActor 가 `actor.GetLayer() & cam.CullingMask` inline). 초안의 camera.h 행 제거(리뷰 L1).

---

## Phase 1 — Material RenderStateBlock 보유 + `IRenderStateProvider` [D7]

> **이 Phase 는 저장처 이전만**(리뷰 L2 교정): GL state 값은 여전히 PassKind 에서 도출, 단지 *매 draw `DefaultRenderStateBlockOf(GetPass())` 도출* → *Material 멤버 1회 seed* 로 옮김. 값 동일 → 무회귀. 성능 이득은 Phase 2 A-dedup.

### Task 1.1: `IRenderStateProvider`
**Files:** Create `src/material/i_render_state_provider.h`
- [ ] **Step 0(선행)**: `git log --oneline -1` 으로 HEAD 가 `68506a8` 인지 확인. 다르면 §grounding 시그니처 재확인.
- [ ] **Step 1: 헤더**
```cpp
/**
 * @file i_render_state_provider.h
 * @brief RenderStateBlock(ROP) 조회 Facade - Material(저장)/IRenderable(위임) 구현 (D7). D9 중립 fallback.
 */
#ifndef __SJH_I_RENDER_STATE_PROVIDER_H__
#define __SJH_I_RENDER_STATE_PROVIDER_H__
// ⚠️ GL 격리(가드레일): pass.h(-> GL/gl3w.h) 를 include 하지 말 것. 반환이 const& 라 전방선언으로 충분.
//    pass.h include 시 이 인터페이스 체인(i_renderable.h -> mesh_pass_processor.h -> ...)을 거쳐
//    Effekseer 클라이언트(GameSystems.cpp 등)까지 gl3w.h 가 전파돼 Apple gl3.h 와 충돌(빌드 실패).
//    실제 RenderStateBlock 을 값/멤버로 쓰는 곳(material.h 등)은 각자 material/pass.h 를 include.
namespace SJH::Pass
{
	struct RenderStateBlock;
}
namespace SJH
{
	class IRenderStateProvider
	{
	  public:
		virtual ~IRenderStateProvider() = default;
		virtual const Pass::RenderStateBlock &GetRenderStateBlock() const = 0;
	};
}
#endif
```
- [ ] **Step 2~3: 빌드/커밋** — 헤더온리, CMake 무변(material 모듈 `..` PUBLIC include). 커밋: `src/material/i_render_state_provider.h`.
> ⚠️ **GL 격리 함정(빌드 확인됨)**: 이 인터페이스 헤더는 절대 `pass.h`/GL 헤더를 include 하면 안 됨 — `Pass::RenderStateBlock` 전방선언만. (초안 v5 가 `#include "material/pass.h"` 했다가 Effekseer 클라이언트에서 `PFNGLGETPOINTERVPROC` 미정의 빌드 실패. 동일하게 i_renderable.h/i_passable.h/pass_iterator.h 도 GL-free 유지.)

### Task 1.2: Material 저장 + Facade
**Files:** Modify `src/material/material.h`
- [ ] **Step 1**: include `#include "material/i_render_state_provider.h"`. `class Material` → `class Material : public IRenderStateProvider`.
- [ ] **Step 2**: private 멤버 추가(`mPassKind` 근처):
```cpp
		/// @brief PassKind 에서 seed 된 ROP (D7 저장처). 값은 DefaultRenderStateBlockOf 와 동일 - 저장 위치만 이전.
		Pass::RenderStateBlock mState = Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Opaque);
```
- [ ] **Step 3**: SetPass seed + Facade:
```cpp
		Material &SetPass(Pass::RenderQueue k) { mPassKind = k; mState = Pass::DefaultRenderStateBlockOf(k); return *this; }
		const Pass::RenderStateBlock &GetRenderStateBlock() const override { return mState; }
```
`CopyFrom`(private) 에 `mState = other.mState;` 추가.
- [ ] **Step 4**: 소비처 — `<src>/render/mesh_pass_processor.cpp` WorldMesh 경로의 `const Pass::RenderStateBlock passState = Pass::DefaultRenderStateBlockOf(material->GetPass()); rc.ApplyRenderStateBlock(passState);` → `rc.ApplyRenderStateBlock(material->GetRenderStateBlock());`. (ScreenQuad 의 `DefaultRenderStateBlockOf(Screen)` 은 유지.)
- [ ] **Step 5~7: 빌드/육안/커밋** — 회귀 핫스팟: healthbar 반투명·shadow 알파. 커밋: `src/material/material.h <src>/render/mesh_pass_processor.cpp`.
> Material 은 복사/이동 delete + Clone private-friend. `mState` 는 기본생성(멤버 초기화)+CopyFrom 둘 다 커버.

---

## Phase 2 — `IRenderable` + 잎 + A-dedup + Flat `RenderableProcessor` [D5/D12]

> 엄격 순서 2.1->2.2->2.3->2.4 (컴파일 의존, 리뷰 L3).

### Task 2.1: `IRenderable` (선행: 없음)
**Files:** Create `src/render/i_renderable.h`
```cpp
/**
 * @file i_renderable.h
 * @brief Pass 안 함께 정렬되는 drawable 순수 추상(mesh-family, D5). LSP-최소 3. 고수준은 구체 DeviceContext(D10).
 */
#ifndef __SJH_I_RENDERABLE_H__
#define __SJH_I_RENDERABLE_H__
#include "material/i_render_state_provider.h"
namespace SJH { class DeviceContext; }
namespace SJH::Scene { class Camera; }
namespace SJH
{
	class IRenderable : public IRenderStateProvider
	{
	  public:
		virtual void Render(DeviceContext &rec, const Scene::Camera &cam) const = 0;
		virtual int  QueueLayer() const = 0;
	};
}
#endif
```
- [ ] 빌드/커밋(헤더온리). 커밋: `src/render/i_renderable.h`.

### Task 2.2: DeviceContext `UseProgram` 내부 dedup 가드 (선행: 없음)
**Files:** Modify `src/render/device_context.cpp` (헤더 시그니처 무변 — **void 유지**, 리뷰 L2/L4 교정: bool 폐기)
- [ ] **Step 1: .cpp 본문** — 내부 dedup 가드만 추가(시그니처 `void UseProgram(const Program&)` 그대로):
```cpp
void DeviceContext::UseProgram(const Program &prog)
{
	if (mStateInitialized && mBoundProgram == &prog)
		return;                                  // 중복 glUseProgram skip (A-dedup; D-RS-1 캐시 규율 연장)
	glUseProgram(prog.GetProgramAddr());
	mBoundProgram = &prog;
}
```
> ⚠️ `mStateInitialized` 가드: `InvalidateStateCache` 후엔 강제 재바인딩(foreign GL 뒤 desync 방지). 기존 호출처(mesh_pass_processor.cpp)는 동작 동일(전환 시 본문 동일, 중복 시 skip=무해).
- [ ] 빌드/육안(동작 0)/커밋: `src/render/device_context.cpp`.

### Task 2.3: 공유 헬퍼 + MeshRenderer 잎 (선행: 2.1, 2.2)
**Files:** Create `<src>/render/draw_ops.h`, `src/render/mesh_renderer.cpp`; Modify `src/render/mesh_renderer.h`, `src/render/CMakeLists.txt`

- [ ] **Step 1: `draw_ops.h` (공유 free 헬퍼 — 리뷰 L3 헬퍼 스코프 확정)**
```cpp
/**
 * @file draw_ops.h
 * @brief draw 시점 공유 헬퍼 - Material UBO 멤버 업로드 + sampler 바인딩. 잎(MeshRenderer)과 ScreenQuad 공용.
 * @details 구 mesh_pass_processor.cpp 익명 namespace 헬퍼를 공유 위치로 추출(중복 제거).
 */
#ifndef __SJH_DRAW_OPS_H__
#define __SJH_DRAW_OPS_H__
#include "GL/gl3w.h"
#include "program/program.h"
#include "material/material_property_block.h"
#include "render/device_context.h"
#include "texture/texture.h"
#include <<glm>/glm.hpp>
namespace SJH
{
	/// @brief MaterialBlock UBO 멤버를 Properties typed map 에서 author 이름 매칭 업로드(비-UBO 멤버 자동 skip).
	inline void UploadMaterialUboMembers(const Program &p, const MaterialPropertyBlock &m)
	{
		for (auto &kv : m.Floats) p.UpdateUniformMember(kv.first, &kv.second, sizeof(float));
		for (auto &kv : m.Ints)   p.UpdateUniformMember(kv.first, &kv.second, sizeof(int));
		for (auto &kv : m.Vec2s)  p.UpdateUniformMember(kv.first, &kv.second, sizeof(glm::vec2));
		for (auto &kv : m.Vec3s)  p.UpdateUniformMember(kv.first, &kv.second, sizeof(glm::vec3));
		for (auto &kv : m.Vec4s)  p.UpdateUniformMember(kv.first, &kv.second, sizeof(glm::vec4));
		for (auto &kv : m.Mat4s)  p.UpdateUniformMember(kv.first, &kv.second, sizeof(glm::mat4));
	}
	/// @brief material 의 sampler(texture)를 program 에 바인딩(셰이더 미선언 sampler 는 location<0 skip).
	inline void BindSamplers(DeviceContext &rc, const MaterialPropertyBlock &b, const Program &p)
	{
		for (auto &kv : b.Textures)
		{
			if (!kv.second.Tex) continue;
			const GLint loc = p.GetLocation(kv.first.c_str());
			if (loc < 0) continue;
			glUniform1i(loc, kv.second.Unit);
			rc.BindTexture(static_cast<GLuint>(kv.second.Unit), kv.second.Tex->GetTextureID());
		}
	}
}
#endif
```

- [ ] **Step 2: mesh_renderer.h — 상속 + 3 메서드** (리뷰 L2: `Camera`(not `Scene::Camera`) — SJH::Scene 내부)

include 추가: `#include "render/i_renderable.h"`, `#include "material/material.h"`. 선언 `class MeshRenderer : public Component, public IRenderable`. public 추가:
```cpp
		/// @brief D7 Facade 위임 - Material 저장처. null 이면 중립(D9).
		const Pass::RenderStateBlock &GetRenderStateBlock() const override
		{
			static const Pass::RenderStateBlock kNeutral{};
			return Material ? Material->GetRenderStateBlock() : kNeutral;
		}
		int QueueLayer() const override
		{
			return Material ? Pass::QueueOf(Material->GetPass(), QueueOffset) : QueueOffset;
		}
		void Render(DeviceContext &rec, const Camera &cam) const override;   // 정의 .cpp. SJH::Scene 내부라 Camera (Scene:: 금지)
```
> ⚠️ `IRenderable::Render` param 은 `SJH::Scene::Camera`. MeshRenderer 는 SJH::Scene 안이라 `Camera`==`SJH::Scene::Camera` — 오버라이드 시그니처 일치. `Scene::Camera` 라 쓰면 `SJH::Scene::Scene::Camera` 오류.

- [ ] **Step 3: mesh_renderer.cpp** (리뷰 L1: 잎은 ApplyRenderStateBlock 안 함 — Processor 가 함; L2: FrameBlock 무조건 per-draw, bool 폐기)
```cpp
/**
 * @file mesh_renderer.cpp
 * @brief MeshRenderer::Render 자가발행 잎 - 구 MeshPassProcessor WorldMesh draw 로직 이주.
 * @details ROP 적용은 RenderableProcessor::Process 루프 담당(여기서 안 함, 0.5). FrameBlock 은 per-draw 업로드
 *          (UseProgram 내부 dedup 으로 glUseProgram 만 skip; FrameBlock 멱등 dedup 은 post-profile, doc sec.6).
 */
#include "render/mesh_renderer.h"
#include "<render>/draw_ops.h"
#include "scene/camera.h"
#include "program/program.h"
#include "object/mesh.h"
#include <<glm>/glm.hpp>

namespace SJH::Scene
{
	void MeshRenderer::Render(DeviceContext &rec, const Camera &cam) const
	{
		if (!Material || !Mesh) return;
		const Program *program = Material->GetProgram();
		if (!program) return;

		rec.UseProgram(*program);                          // 내부 dedup (중복 glUseProgram skip)
		if (program->HasUniformBlocks())
		{
			const glm::mat4 view = cam.GetViewMatrix();
			const glm::mat4 proj = cam.GetProjectionMatrix();
			program->UpdateUniformBlock("FrameBlock", &view, sizeof(glm::mat4), 0);
			program->UpdateUniformBlock("FrameBlock", &proj, sizeof(glm::mat4), sizeof(glm::mat4));
		}
		BindSamplers(rec, Material->Properties, *program);
		if (program->HasUniformBlocks())
		{
			UploadMaterialUboMembers(*program, Material->Properties);
			if (Material->Properties.Vec4s.find("baseColor") == Material->Properties.Vec4s.end())
			{
				const glm::vec4 white(1.0f);               // 구 fallback 보존
				program->UpdateUniformMember("baseColor", &white, sizeof(glm::vec4));
			}
			const glm::mat4 model = GetOwner()->GetWorldMatrix();
			program->UpdateUniformBlock("DrawBlock", &model, sizeof(glm::mat4), 0);
			program->BindUniformBlocks();                  // FrameBlock/DrawBlock/MaterialBlock 결속
		}
		// ROP(ApplyRenderStateBlock) 미호출 - RenderableProcessor::Process 가 r->Render 직전 적용.
		rec.BindVAO(Mesh->GetVAO());
		rec.DrawIndexed(Mesh->GetIndexCount());
	}
}
```
> ⚠️ `GetOwner()`(Component→Actor*), `GetWorldMatrix()`(Actor), `GetProgramAddr/GetLocation/UpdateUniformBlock/UpdateUniformMember/HasUniformBlocks/BindUniformBlocks`(Program), `GetVAO/GetIndexCount`(Mesh) 전부 grounding 확인.
- [ ] **Step 4: CMake** — `src/render/CMakeLists.txt` `add_library(sjhopengl_render STATIC ...)` 에 `mesh_renderer.cpp` 추가.
- [ ] **Step 5: 빌드 — 다중상속 검증**(리뷰 L4): MeshRenderer : Component, IRenderable. IRenderable : IRenderStateProvider (**단일 경로 — 다이아몬드 아님**). 3 메서드 다 구현 → 추상 아님(인스턴스화 OK). Builder/sprite 무영향. **이 시점 RenderableProcessor 가 아직 IRenderable 안 씀 → 동작 0.**
- [ ] **Step 6~7: 육안/커밋** — 커밋: `<src>/render/draw_ops.h src/render/mesh_renderer.{h,cpp} src/render/CMakeLists.txt`.

### Task 2.4: `MeshPassProcessor` → `RenderableProcessor` (선행: 2.3)
**Files:** `git mv mesh_pass_processor.{h,cpp} renderable_processor.{h,cpp}` (DECISION 2.5=개명) + Modify `render_passable.impls.{h,cpp}`, `CMakeLists`

> **🟦 DECISION 2.4 (착수 시 1택)**: ScreenQuad 전이 — **A(추천)**: Phase 2 는 dual-mode(world=IRenderable* / screen=전이), Phase 3.5 PostFxPass 로 이관 후 screen 경로 제거. 헬퍼는 draw_ops.h 공유라 중복 0.

- [ ] **Step 1: 클래스 (개명 + dual-mode)**
```cpp
#include "render/i_renderable.h"
namespace SJH::Scene { class Camera; }
namespace SJH
{
	class RenderableProcessor
	{
	  public:
		void Submit(const IRenderable *r, float viewDepth) { mWorld.push_back({r, r->QueueLayer(), viewDepth}); }
		void SubmitScreenQuad(Framebuffer *in, Framebuffer *out, Material *passMat) { mScreen.push_back({in, out, passMat}); }
		void SetScreenQuadMesh(Mesh *m) { mScreenQuadMesh = m; }
		void SetBypassMaterial(Material *m) { mBypassMat = m; }
		const Framebuffer *GetLastOutputFB() const { return mLastOutputFB; }
		void Clear() { mWorld.clear(); mScreen.clear(); mLastOutputFB = nullptr; }
		std::size_t Size() const { return mWorld.size() + mScreen.size(); }
		void Sort();
		void Process(DeviceContext &rc, const Scene::Camera &cam);
	  private:
		struct WorldEntry  { const IRenderable *r; int queueLayer; float depth; };
		struct ScreenEntry { Framebuffer *in; Framebuffer *out; Material *mat; };
		std::vector<WorldEntry>  mWorld;
		std::vector<ScreenEntry> mScreen;
		Mesh              *mScreenQuadMesh = nullptr;
		Material          *mBypassMat      = nullptr;
		const Framebuffer *mLastOutputFB   = nullptr;
	};
}
```
- [ ] **Step 2: Sort** (stable_sort — 결정성)
```cpp
void RenderableProcessor::Sort()
{
	std::stable_sort(mWorld.begin(), mWorld.end(), [](const WorldEntry &a, const WorldEntry &b) {
		if (a.queueLayer != b.queueLayer) return a.queueLayer < b.queueLayer;
		if (Pass::IsTransparentQueue(a.queueLayer)) return a.depth < b.depth;   // back-to-front
		return a.depth > b.depth;                                              // front-to-back
	});
}
```
> DECISION 2.4-b(리뷰 L2): program 그룹핑 보조키 *생략*(Phase 2). A-dedup(UseProgram skip)이 인접 동일 program 에서 자동 효과. program 보조키는 dedup hit-rate 향상 *후속* 최적화.
- [ ] **Step 3: Process** (world flat + 전이 screen; 헬퍼 = draw_ops.h)
```cpp
#include "<render>/draw_ops.h"
void RenderableProcessor::Process(DeviceContext &rc, const Scene::Camera &cam)
{
	rc.InvalidateStateCache();
	for (auto &e : mWorld)                                  // World: 0.5 flat (ROP 는 여기서 적용)
	{
		rc.ApplyRenderStateBlock(e.r->GetRenderStateBlock());
		e.r->Render(rc, cam);
	}
	for (auto &e : mScreen)                                 // ScreenQuad: 전이 (Phase 3.5 PostFxPass 이관 후 제거)
	{
		if (!e.in || !e.out || !mScreenQuadMesh) continue;
		Material *mat = e.mat ? e.mat : mBypassMat;
		if (!mat) continue;
		const Program *prog = mat->GetProgram();
		if (!prog) continue;
		rc.BeginFrame(*e.out);
		rc.ApplyRenderStateBlock(Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Screen));
		mat->Properties.Textures["uScene"] = { e.in->GetColorAttachment().get(), 0 };
		rc.UseProgram(*prog);
		if (prog->HasUniformBlocks()) { UploadMaterialUboMembers(*prog, mat->Properties); prog->BindUniformBlocks(); }
		BindSamplers(rc, mat->Properties, *prog);
		rc.BindVAO(mScreenQuadMesh->GetVAO());
		if (auto ebo = mScreenQuadMesh->GetIndexBuffer()) ebo->Bind();   // VAO/EBO 오염 가드 (착수 시 GetIndexBuffer 존재 확인)
		rc.DrawIndexed(mScreenQuadMesh->GetIndexCount());
		mLastOutputFB = e.out;
	}
}
```
> ⚠️ `Mesh::GetIndexBuffer()` 존재 여부 착수 시 확인(구 코드 사용 추정 — grounding 미확정). 없으면 EBO 재핀 생략(현 동작 유지).
- [ ] **Step 4: SceneRenderer 연동 + 호출처 갱신**(리뷰 L4) — `render_passable.impls.h`: `MeshPassProcessor mProcessor;` → `RenderableProcessor mProcessor;`. `render_passable.impls.cpp`:
  - `RenderWithCamera`: `mProcessor.SortMultiStage()` → `mProcessor.Sort()`; `mProcessor.Process(rc, viewMat, projMat)` → `mProcessor.Process(rc, cam)`.
  - `CollectFromActor`(시그니처 `(const Actor&, const glm::mat4& viewMat, uint64_t cullingMask)` 유지): MeshRenderer/PassComponent 수집을:
```cpp
    if (auto *mr = actor.GetComponent<Scene::MeshRenderer>(); mr && mr->Visible && (actor.GetLayer() & cullingMask))
    {
        const glm::mat4 model = actor.GetWorldMatrix();
        const float depth = (viewMat * model)[3][2];        // 구 depth 계산식 보존
        mProcessor.Submit(mr, depth);                        // MeshRenderer* -> IRenderable* 암묵 upcast (slow-path dynamic_cast 회피)
    }
    if (auto *pc = actor.GetComponent<Scene::PassComponent>(); pc /* + 기존 가드 */)
        mProcessor.SubmitScreenQuad(pc->InputFB, pc->OutputFB, (pc->Enabled && pc->mMaterial) ? pc->mMaterial : nullptr);
    for (auto &child : actor.GetChildren()) CollectFromActor(*child, viewMat, cullingMask);
```
  > ⚠️ `GetComponent<Scene::MeshRenderer>()` 유지(fast-path). `mr`(MeshRenderer*) → Submit(IRenderable*) 암묵 upcast(public 상속). SpriteRenderer/Text 글리프(SpriteRenderer)도 MeshRenderer 라 동일 경로. **비-mesh IRenderable 은 현재 0**(생기면 별 역할 Pass). 기존 CollectFromActor 의 실제 가드/순서는 grounding 본문 대조 후 보존.
- [ ] **Step 5: 파일/CMake** — `git mv` + 헤더가드(`__SJH_RENDERABLE_PROCESSOR_H__`)/include 갱신 + `render/CMakeLists.txt` target_sources(`mesh_pass_processor.cpp`→`renderable_processor.cpp`).
- [ ] **Step 6~8: 빌드/육안/커밋** — mesh/sprite/text/PostFX 출력·순서·반투명 동일(Phase 2 핵심 게이트). 커밋: 변경 render 파일 + impls + CMake.

---

## Phase 3 — `IPassable` 계약 + 역할군 Pass [D2/D3/D4]

> **리뷰 L4 교정 — 인터페이스 원자 교체 분리.** Task 3.0 이 인터페이스+전 구현체를 *한 커밋*으로 바꿔 빌드 GREEN 을 만든 뒤, 3.1~3.5 가 각 Pass 내부를 *한 Pass씩* 정련.

### Task 3.0: 원자 인터페이스 교체 (ONE 커밋 — 빌드 GREEN 유지)
**Files:** `render_passable.h`, `render_passable.impls.{h,cpp}`, `apps/_MyApp_/src/VFX/ParticleStage.{h,cpp}`, `apps/_MyApp_/main.cpp` (전부 동시)

- [ ] **Step 1: `render_passable.h` — 클래스 in-place 개명**(파일 유지, DECISION: 신규 i_passable.h 안 만듦)
```cpp
#ifndef __SJH_I_PASSABLE_H__   // 구 __SJH_IRENDER_PASSABLE_H__ 개명
#define __SJH_I_PASSABLE_H__
namespace SJH
{
	class Texture; class DeviceContext;
	class IPassable
	{
	  public:
		virtual ~IPassable() = default;
		virtual void           Draw(DeviceContext &rec, const Texture *before) = 0;
		virtual const Texture *GetPassResult() const = 0;
		virtual void           OnResize(int /*w*/, int /*h*/) {}
		int BeforeIndex = -1;
	};
}
#endif
```
- [ ] **Step 2: 전 구현체 시그니처 동시 교체 (내부 로직 임시 보존)** — `IRenderPassable`→`IPassable`, `Render(RenderTarget&)`→`Draw(DeviceContext&, const Texture* before)` + `GetPassResult()` 추가, 4+ 클래스(SceneRenderer/ScreenQuadStage/CameraStage + ParticleStage) 모두. 이 단계 각 `Draw` 본문 = *기존 Render 본문 그대로* 이식하되 target 은 내부 도출(Camera RT / mCamera->GetTargetRenderTarget()), `before` 미사용, `GetPassResult()` = 자기 출력 FB attachment(`.get()`) 또는 nullptr. `main.cpp` 의 `for(s:mStages) s->Render(...)` → `s->Draw(DeviceContext::Get(), nullptr)` 임시.
- [ ] **Step 3: 빌드 — 전 구현체 동시 IPassable 이라야 GREEN.** 한 커밋.
- [ ] **Step 4: 육안**(동작 동일, Draw 본문=구 Render). **Step 5: 커밋(원자)**: `render_passable.* impls.* ParticleStage.* main.cpp`.

### Task 3.1: `WorldPass : IPassable` (선행: 3.0, 2.4)
**Files:** `render_passable.impls.{h,cpp}` (SceneRenderer+CameraStage → WorldPass)
- [ ] **Step 1: 스켈레톤**(grounding RenderWithCamera 본문 이식)
```cpp
class WorldPass : public IPassable
{
  public:
	WorldPass(Scene::Camera *cam) : mCamera(cam) {}
	void SetActivePrograms(std::vector<Program*> p) { mActivePrograms = std::move(p); }
	void Draw(DeviceContext &rec, const Texture * /*before*/) override
	{
		RenderTarget *rt = mCamera->GetTargetRenderTarget();
		if (!rt) { /* warn + return (구 RenderWithCamera null guard) */ return; }
		if (mCamera->NoClear) rec.BindTarget(*rt); else rec.BeginFrame(*rt);   // NoClear 분기 보존
		// Light 수집(dir/points[]/spots[] Enabled 필터) + mUploader.Update(...) + BindTo(mActivePrograms)  <- 구 RenderWithCamera 본문 이식
		mProc.Clear();
		CollectFromActor(/*sceneRoot*/, mCamera->GetViewMatrix(), mCamera->CullingMask);   // MeshRenderer 만 (PassComponent 제거)
		mProc.Sort();
		mProc.Process(rec, *mCamera);
	}
	const Texture *GetPassResult() const override
	{
		auto *fb = dynamic_cast<Framebuffer*>(mCamera->GetTargetRenderTarget());
		return fb ? fb->GetColorAttachment().get() : nullptr;   // D6
	}
  private:
	void CollectFromActor(const Scene::Actor &a, const glm::mat4 &view, uint64_t mask);   // MeshRenderer Submit 만
	RenderableProcessor   mProc;
	LightUboUploader      mUploader;
	std::vector<Program*> mActivePrograms;
	Scene::Camera        *mCamera = nullptr;
};
```
- [ ] **Step 2**: `CollectFromActor` 에서 PassComponent 수집 제거(MeshRenderer Submit 만, Task 2.4 Step 4 형태). PassComponent 는 Task 3.5 PostFxPass 담당.
- [ ] **Step 3~5: 빌드/육안(World 동일)/커밋.** (SkyboxPass 분리 전이라 skybox 는 아직 WorldPass 내 queue 2500 으로 그려짐 — 정상.)

### Task 3.2: `SkyboxPass : IPassable` (선행: 3.1)
- [ ] **DECISION 3.2(1택)**: 독립 SkyboxPass(추천) vs WorldPass skybox-queue. 독립 시: skybox Material/mesh 보유, `Draw` 가 자기 RT 에 queue 2500 정통. 빌드/육안/커밋.

### Task 3.3: `ParticlePass : IPassable` (선행: 3.0)
- [ ] grounding: 현 `ParticleStage(VFXSystem*, Camera*)` + 끝 InvalidateStateCache. 스켈레톤:
```cpp
void ParticlePass::Draw(DeviceContext &rec, const Texture * /*before*/) override
{
	RenderTarget *rt = mWorldCam->GetTargetRenderTarget();
	if (!rt) return;
	rec.BindTarget(*rt);
	rec.InvalidateStateCache();          // 진입: foreign GL(efk) 전 캐시 무효화
	mVFX->Render(*mWorldCam);            // efk 직접 호출(기존 ParticleStage 본문)
	rec.InvalidateStateCache();          // 이탈: efk 가 바꾼 GL state desync 차단(D-RS-2, VAO/EBO 오염)
}
const Texture *ParticlePass::GetPassResult() const override { return nullptr; }   // 자기 합성 안 함 (D9)
```
- [ ] 빌드/육안(파티클 동일)/커밋: `apps/_MyApp_/src/VFX/ParticleStage.*`.

### Task 3.4: `ImGuiPass : IPassable` (선행: 3.0)
- [ ] main.cpp `ImGui::Render()` 흡수. **NewFrame/EndFrame 시점**(리뷰 L3): `ImGui::NewFrame` + UI 빌드(PostFXDebugLayer 등)는 main 프레임 루프 유지; `ImGuiPass::Draw` 는 *발행*만:
```cpp
void ImGuiPass::Draw(DeviceContext &rec, const Texture * /*before*/) override
{
	rec.InvalidateStateCache();          // 진입
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());   // 기존 backend 호출
	rec.InvalidateStateCache();          // 이탈
}
const Texture *ImGuiPass::GetPassResult() const override { return nullptr; }
```
> ⚠️ ImGuiPass 거주 = executable target(UI .cpp 패턴, ImGui 의존). memory `imgui_layer_separation` 정합 확인.
- [ ] 빌드/육안(ImGui 동일)/커밋.

### Task 3.5: `PostFxPass : IPassable` (선행: 3.1, 2.4) — 전이 ScreenQuad 제거
**Files:** `render_passable.impls.{h,cpp}`(ScreenQuadStage+PassComponent → PostFxPass), `RenderableProcessor`(screen 경로 삭제), `render_pipeline.{h,cpp}`
- [ ] **Step 1: 구조**(리뷰 L3 — PassComponent chain 보유):
```cpp
class PostFxPass : public IPassable
{
  public:
	void SetEffects(std::vector<Scene::PassComponent*> fx) { mEffects = std::move(fx); }   // 구 BuildPostFXChain 결과
	void SetScreenQuad(Mesh *m, Material *bypass) { mQuad = m; mBypass = bypass; }
	void Draw(DeviceContext &rec, const Texture *before) override
	{
		// before(=WorldPass/Particle 합성 출력) 를 첫 입력으로, mEffects chain 순회하며 각 PassComponent OutputFB 에 blit.
		// 단계마다: BeginFrame(pc->OutputFB) + ApplyRenderStateBlock(Screen) + uScene=현재입력 + UseProgram + draw_ops + DrawIndexed.
		// (구 RenderableProcessor ScreenQuad 경로 + ScreenQuadStage::Render 통합). mLastOutput = 마지막 OutputFB.
		// mEffects 비면 before 를 backbuffer 로 passthrough blit.
	}
	const Texture *GetPassResult() const override { return mLastOutput ? mLastOutput->GetColorAttachment().get() : nullptr; }
  private:
	std::vector<Scene::PassComponent*> mEffects;
	Mesh *mQuad = nullptr; Material *mBypass = nullptr;
	const Framebuffer *mLastOutput = nullptr;
};
```
- [ ] **Step 2**: `RenderableProcessor` 의 전이 screen(`mScreen`/`ScreenEntry`/`SubmitScreenQuad`/`SetScreenQuadMesh`/`SetBypassMaterial`/`GetLastOutputFB`/`mScreenQuadMesh`/`mBypassMat`/`mLastOutputFB`/Process screen loop) **전량 삭제** → 순수 IRenderable(`mWorld` 단일). **코드에 `[TRANSITIONAL-3.5]` 태그로 마킹됨** — `grep -rn "TRANSITIONAL-3.5" src/render/` 으로 제거 지점 일괄 확인(mesh_pass_processor.{h,cpp}). WorldPass.CollectFromActor PassComponent 수집도 제거(3.1 에서). `render_pipeline.cpp` `BuildPostFXChain` 이 PostFxPass 에 PassComponent 목록 주입하도록 조정.
- [ ] **Step 3~5: 빌드/육안(PostFX 체인 동일)/커밋.**

> **Phase 3 완료**: 모든 stage IPassable. RenderableProcessor 순수 IRenderable. 외부 SetSources/GetLastSceneOutput 제거. GUI 무회귀.

---

## Phase 4 — `PassIterator` + main 배선 [D8]

### Task 4.1: `PassIterator` (선행: 3.x)
**Files:** Create `src/render/pass_iterator.h`, `.cpp`; Modify `src/render/CMakeLists.txt`
```cpp
// pass_iterator.h
#ifndef __SJH_PASS_ITERATOR_H__
#define __SJH_PASS_ITERATOR_H__
#include <vector>
namespace SJH
{
	class IPassable; class DeviceContext; class RenderTarget;
	class PassIterator
	{
	  public:
		int  Add(IPassable *pass);                                   // 비소유. 반환=인덱스
		void Execute(DeviceContext &rec, RenderTarget &backbuffer);
		void Resize(int w, int h);
		int  DebugPassIndex = -1;
	  private:
		std::vector<IPassable *> mPasses;
	};
}
#endif
```
- [ ] **Step 2: .cpp — Execute 완전 코드**(리뷰 L1/L3: bounds check + 첫 Pass nullptr + DebugPassIndex)
```cpp
#include "render/pass_iterator.h"
#include "render/render_passable/render_passable.h"   // IPassable
namespace SJH
{
	int  PassIterator::Add(IPassable *pass) { mPasses.push_back(pass); return (int)mPasses.size() - 1; }
	void PassIterator::Resize(int w, int h) { for (auto *p : mPasses) p->OnResize(w, h); }
	void PassIterator::Execute(DeviceContext &rec, RenderTarget & /*backbuffer*/)
	{
		const int n = (int)mPasses.size();
		for (int i = 0; i < n; ++i)
		{
			IPassable *p = mPasses[i];
			const int bi = p->BeforeIndex;
			const Texture *before = (bi >= 0 && bi < n) ? mPasses[bi]->GetPassResult() : nullptr;   // bounds check
			p->Draw(rec, before);
			if (DebugPassIndex == i) break;
		}
		// 첫 Pass(Skybox/World)는 BeforeIndex=-1 -> before=nullptr(scene raw). 종단 Pass(PostFx/ImGui)가 backbuffer 출력.
		// 순환 검사 불요(BeforeIndex 는 항상 앞 인덱스 - Add 순서가 코스). DebugPassIndex>=0 시 그 Pass 결과를 backbuffer 합성(PostFxPass blit 재사용 - 별 헬퍼).
	}
}
```
- [ ] CMake target_sources `pass_iterator.cpp`. 빌드/커밋.

### Task 4.2: main.cpp 배선 (선행: 4.1)
**Files:** `apps/_MyApp_/main.cpp`, `src/render_bootstrap/render_pipeline.{h,cpp}`
- [ ] **Step 0(선행)**: `apps/_MyApp_/main.cpp` 현재 상태에서 mStages/SetupDefaultPipeline/SetSources/ImGui::Render 실제 라인 재확인(본 plan 라인참조는 grounding 시점).
- [ ] **Step 1**: `std::vector<unique_ptr<IRenderPassable>> mStages` → `PassIterator mPassIterator` + Pass 소유 멤버(unique_ptr). `SetupDefaultPipeline` 이 Pass 생성+`PassIterator.Add` 조립 반환.
- [ ] **Step 2**: 렌더 루프 → `mPassIterator.Execute(DeviceContext::Get(), *mDefaultTarget);`. 외부 `SetActivePrograms`(WorldPass 에 Execute 전 1회 주입 — D-1 push 유지)/`SetSources`/`GetLastSceneOutput` 제거(BeforeIndex 로). `ImGui::Render()` 직접 호출 제거(ImGuiPass).
- [ ] **Step 3**: 코스 순서 Add `[SkyboxPass, WorldPass, ParticlePass, PostFxPass, ImGuiPass]`. PostFxPass.BeforeIndex = World/Particle 합성 인덱스.
- [ ] 빌드/육안(평소 동일 + DebugPassIndex 키)/커밋.

---

## Phase 5 — 정리 / 문서

- [ ] **5.1**: Phase 3/4 완료 *후* 실제 잔존 심볼 grep 후 판정 — `SceneRenderer`/`CameraStage`/`ScreenQuadStage`/`MeshPassProcessor`/`IRenderPassable`/`PassComponent` 폐기/유지(WorldPass/PostFxPass 흡수면 삭제). `[[deprecated]]` 잔재 제거.
- [ ] **5.2**: `doc/diagrams/engine-migration-class.*` 구현 후 갱신 + `dot -Tsvg` 재렌더.
- [ ] **5.3**: 메모리/CLAUDE.md(모듈 목록) 갱신. `rendering-migration-design` 메모리에 "구현 완료" change log.

---

## 검증 / Out of Scope

**검증(사용자)**: 빌드 `export PATH="$HOME/slang/bin:$PATH" && cmake --build --preset ninja --target _MyApp_`. 실행 `cd build_ninja/apps/_MyApp_ && ./_MyApp_`. Phase 1·2·3(각 Task)·4 GUI 회귀 육안. `Sort` stable_sort 유지. **회귀 핫스팟**: 반투명 healthbar/shadow 알파(blend off->on footgun, grounding G2), Outline stencil, Skybox depth, AlphaTest sprite cull, efk/ImGui foreign-GL(InvalidateStateCache).

**Out of Scope**: 백엔드 Facade+Strategy(IGraphicsBackend)+PSO(PipelineStateDesc)+IRenderTargetPool → **후속 멀티백엔드 프로젝트**. Shadow/Deferred/MSAA/HDR/Cubemap · TextRenderable/능력 인터페이스 · data-driven JSON · 다중 BeforeIndex · ICommandRecorder/ITargetAllocator.

---

## Task-time DECISION 요약 (착수 시 1택)

| # | 결정 | 추천 |
|---|---|---|
| 2.4 | ScreenQuad 전이 | **A** — Phase 2 dual-mode, Phase 3.5 PostFxPass 이관/제거 |
| 2.5 | mesh_pass_processor 파일 개명 | **개명**(renderable_processor.{h,cpp}) |
| 3.0 | IPassable 파일 정책 | **render_passable.h 유지 + 클래스 in-place 개명** (신규 i_passable.h 안 만듦) |
| 3.2 | SkyboxPass 독립 vs WorldPass-queue | **독립**(역할군 가독성) |
| 4.x | SetActivePrograms 위치 | Execute 전 WorldPass 에 1회 주입(D-1 push 유지) |

---

## Self-Review (writing-plans + 리뷰 반영)

- **Spec coverage**: D1~D13 전부 Phase 매핑(P1=D7, P2=D5/D12/D10, P3=D2/D3/D4/D6/D9, P4=D8; D11/D13 OoS). ✔
- **리뷰 blocker 해소**: UseProgram void+dedup(bool 폐기) ✔ / 잎 ApplyRenderStateBlock 중복 제거 ✔ / Camera 네임스페이스(`Scene::Camera`->`Camera`) ✔ / draw_ops.h 헬퍼 공유 ✔ / Task 선행 그래프 ✔ / Phase 3.0 원자 교체 분리 ✔ / Process(rc,cam) 호출처 갱신 ✔ / MeshRenderer 다중상속 단일경로(다이아몬드 아님)+upcast ✔.
- **리뷰 major 해소**: Phase 1 저장처-이전-only ✔ / Pass 스켈레톤(WorldPass/PostFxPass/ParticlePass/ImGuiPass Draw) ✔ / PassIterator Execute 완전+bounds ✔ / CollectFromActor pseudocode ✔ / foreign-GL InvalidateStateCache 위치 ✔ / GetComponent fast-path 유지 ✔.
- **잔여 Task-time 확인(정직)**: `Mesh::GetIndexBuffer()` 존재 · 기존 CollectFromActor 가드/순서 · main.cpp 실제 라인 · Phase 5 잔존 심볼 — 각 Step "착수 시 live 재확인" 명시.
- **Placeholder**: 신규 코드 전부 완전. Phase 3.2~3.5 스켈레톤+grounding 이식 지시(구 코드 본문 대조) — 대규모 refactor 정직한 입도.
- **Grounding**: 2026-06-23 HEAD 68506a8 6-슬라이스. 각 Phase 착수 전 재확인 경고.
