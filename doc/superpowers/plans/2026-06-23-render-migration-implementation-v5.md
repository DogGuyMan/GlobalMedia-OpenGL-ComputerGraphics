# 렌더링 시스템 마이그레이션 구현 Plan (v5)

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 현 렌더 모듈을 v5 목표 형태(IRenderStateProvider/IRenderable/Flat RenderableProcessor/IPassable 역할군/PassIterator)로 *in-place* 재형성하되 **렌더 결과(픽셀) 변화 0**. 고수준은 **구체 `DeviceContext&`** 사용(ICommandRecorder 없음), 배치는 **A-dedup**(DeviceContext 캐시).

**Architecture:** Material ROP(D7) → IRenderable+Flat Processor(D5/D12) → IPassable 역할군(D2/D3/D4, 한 Pass씩) → PassIterator(D8) → 정리. 백엔드 Facade+Strategy·PSO·RT풀은 **별도 후속 프로젝트**(범위 밖, D10/D11/D13).

**Tech Stack:** C++17, OpenGL 4.1 Core(gl3w), GLM, CMake(Ninja preset), Slang→GLSL410. 자원/상태 = Meyer 싱글톤(`DeviceContext`/`ResourceRegistry`), 씬 = Actor/Component.

**정본 설계:** [`doc/마이그레이팅계획안.md`](../../마이그레이팅계획안.md) (v5, D1~D13) + [`doc/handoffs/2026-06-23/2026-06-23-rendering-migration-design-resume-handoff.md`](../../handoffs/2026-06-23-rendering-migration-design-resume-handoff.md).

---

## ⚠️ 프로세스 규약 (writing-plans 기본 TDD 와의 의도적 차이)

단위 테스트(Catch2) 전량 폐기 상태, 렌더 검증은 **사용자 빌드 + 육안 회귀**가 정본(memory `no_auto_tests`, `user-parallel-git-and-builds`). 각 Task 검증:
1. **빌드 = 사용자**: `export PATH="$HOME/slang/bin:$PATH" && cmake --build --preset ninja --target _MyApp_` → 컴파일 출력 붙이면 에이전트 육안.
2. **실행 = 사용자**: `cd build_ninja/apps/_MyApp_ && ./_MyApp_` → GUI 가 직전과 동일한지 육안.
3. **커밋 = 사용자** path-scoped(`git commit <경로>`), `git add -A` ❌, `Co-Authored-By` ❌.

에이전트는 **구현 + 보고만**. "Commit" 스텝은 *제안 메시지/경로*.

## 🔒 가드레일 (전 Task 불변)

- **GL 격리**: gl* 는 `DeviceContext`(+자원 RAII Texture/Framebuffer) 안에만. 신규 인터페이스 헤더(IRenderStateProvider/IRenderable/IPassable/PassIterator)에 GL include ❌. 외부 라이브러리(Effekseer/ImGui)는 ParticlePass/ImGuiPass + `InvalidateStateCache` 경계.
- **주석 한국어 + ASCII only**(특수문자 0), 헤더 가드 `__SJH_XXX_H__`.
- **`extern/sb7code` 수정 금지**.
- **Phase 3 한 Pass씩** 전환 + 매번 빌드 GREEN + 육안 (회귀 최대 구간).
- **착수 전 HEAD 재측정**(`git log/status`). 본 plan 의 시그니처는 grounding(2026-06-23, HEAD `68506a8`) 기준 — Task 시작 시 live 재확인.

---

## File Structure (생성/수정 맵)

### 신규 (N)
| 파일 | 책임 |
|---|---|
| `src/material/i_render_state_provider.h` | `IRenderStateProvider` ROP Facade 인터페이스 |
| `src/render/i_renderable.h` | `IRenderable : IRenderStateProvider` (Render/QueueLayer) |
| `src/render/mesh_renderer.cpp` | MeshRenderer::Render 잎 구현 (신규 — 현재 헤더온리) |
| `src/render/pass_iterator.h` / `.cpp` | `PassIterator` (vector<IPassable*> 실행/Present/DebugPassIndex) |

### 수정 (E)
| 파일 | 변경 |
|---|---|
| `src/material/material.h` | `: IRenderStateProvider`, `mState` 멤버, `SetPass` seed, `GetRenderStateBlock`, `CopyFrom` |
| `src/render/mesh_renderer.h` | `: Component, IRenderable` + 3 메서드 |
| `src/render/device_context.h/.cpp` | `UseProgram` → `bool`(switched 반환, A-dedup) |
| `src/render/mesh_pass_processor.{h,cpp}` | → `RenderableProcessor`(IRenderable* + 전이기 ScreenQuad) |
| `src/render/render_passable/render_passable.h` | `IRenderPassable` → `IPassable`(Draw(DeviceContext&,const Texture*)/GetPassResult/BeforeIndex) |
| `src/render/render_passable/render_passable.impls.{h,cpp}` | `WorldPass`/`SkyboxPass`/`PostFxPass` 등 역할군 |
| `apps/_MyApp_/src/VFX/ParticleStage.{h,cpp}` | → `ParticlePass : IPassable` |
| `apps/_MyApp_/main.cpp` | `mStages` → `PassIterator`; ImGui → `ImGuiPass` |
| `src/render_bootstrap/render_pipeline.{h,cpp}` | `SetupDefaultPipeline` → PassIterator 조립 |
| `src/scene/camera.h` | `bool Sees(uint64_t) const` 헬퍼 |
| `src/render/CMakeLists.txt` | `mesh_renderer.cpp` + `pass_iterator.cpp` target_sources |

---

## Phase 1 — Material RenderStateBlock 보유 + `IRenderStateProvider` [D7]

> 비파괴. 현재 GL state 는 `mesh_pass_processor.cpp` 에서 `Pass::DefaultRenderStateBlockOf(material->GetPass())` 로 *매 draw 도출*(MeshRenderer override 폐기됨 — SP-MaterialSSoT). 저장처만 Material 로 옮기면 값 동일.

### Task 1.1: `IRenderStateProvider` 인터페이스

**Files:** Create `src/material/i_render_state_provider.h`

- [ ] **Step 1: 헤더 생성**

```cpp
/**
 * @file i_render_state_provider.h
 * @brief RenderStateBlock(ROP) 조회 Facade - Material(저장) / IRenderable(위임) 둘 다 구현 (D7).
 * @details efk/ImGui 처럼 ROP 를 못 꺼내는 경로는 중립 기본값(D9) 반환.
 */
#ifndef __SJH_I_RENDER_STATE_PROVIDER_H__
#define __SJH_I_RENDER_STATE_PROVIDER_H__

#include "material/pass.h"   // Pass::RenderStateBlock

namespace SJH
{
	/// @brief ROP 조회 Facade. Material 이 저장, IRenderable(mesh)이 Material 로 위임.
	class IRenderStateProvider
	{
	  public:
		virtual ~IRenderStateProvider() = default;
		virtual const Pass::RenderStateBlock &GetRenderStateBlock() const = 0;
	};
} // namespace SJH

#endif // __SJH_I_RENDER_STATE_PROVIDER_H__
```

- [ ] **Step 2~4: 빌드/육안/커밋(사용자)** — 헤더온리, 동작 0, CMake 무변(material 모듈이 이미 `..` PUBLIC include). 커밋: `src/material/i_render_state_provider.h`.

### Task 1.2: Material 이 RenderStateBlock 저장 + Facade 구현

**Files:** Modify `src/material/material.h`

- [ ] **Step 1: 상속 + 멤버**

include 에 `#include "material/i_render_state_provider.h"`. 선언 `class Material` → `class Material : public IRenderStateProvider`.

private 영역(`mPassKind` 근처)에 추가:
```cpp
		/// @brief PassKind 에서 seed 된 GL fixed-function 상태 (D7 저장처).
		///        draw 시점 매번 DefaultRenderStateBlockOf 도출하던 것을 본 멤버로 대체.
		Pass::RenderStateBlock mState = Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Opaque);
```

- [ ] **Step 2: SetPass seed + GetRenderStateBlock**

`SetPass` 본문을 seed 하도록:
```cpp
		Material &SetPass(Pass::RenderQueue k)
		{
			mPassKind = k;
			mState    = Pass::DefaultRenderStateBlockOf(k);   // D7 - 저장처 seed
			return *this;
		}
```
public 에 추가:
```cpp
		/// @brief D7 Facade - 저장된 ROP 반환.
		const Pass::RenderStateBlock &GetRenderStateBlock() const override { return mState; }
```
`CopyFrom`(private, Clone 경로) 에 한 줄 추가:
```cpp
			mState = other.mState;   // ROP 승계 (Clone 시 Transparent 등 유지)
```

- [ ] **Step 3: 소비처 1곳 교체** — `<src>/render/mesh_pass_processor.cpp` 의 WorldMesh 경로:
```cpp
// 변경 전: const Pass::RenderStateBlock passState = Pass::DefaultRenderStateBlockOf(material->GetPass());
//          rc.ApplyRenderStateBlock(passState);
// 변경 후:
rc.ApplyRenderStateBlock(material->GetRenderStateBlock());
```
> ScreenQuad 경로의 `DefaultRenderStateBlockOf(RenderQueue::Screen)` 은 Material 없는 blit 이라 **그대로 유지**.

- [ ] **Step 4: 빌드(사용자)** — seed 가 기존 도출과 *값 동일*(SetPass 호출 시점에 도출) → 무회귀 기대.
- [ ] **Step 5: 실행 육안(사용자)** — Opaque/Transparent/Skybox/Outline(stencil)/AlphaTest 동일. **회귀 핫스팟**: healthbar 반투명·shadow 알파(memory `blend-func-cache-default-mismatch`).
- [ ] **Step 6: Commit(사용자)**

```
git commit src/material/material.h <src>/render/mesh_pass_processor.cpp \
  -m "[refactor] Material RenderStateBlock 보유 + IRenderStateProvider Facade (D7)"
```

> ⚠️ Material 은 복사/이동 delete + Clone private-friend-ResourceRegistry. `mState` 추가는 기본생성/CopyFrom 둘 다 커버됨. 기본 Material(mPassKind=Opaque)은 멤버 초기화로 Opaque ROP seed — SetPass 미호출 머티리얼도 정합.

---

## Phase 2 — `IRenderable` + MeshRenderer 잎 + A-dedup + Flat `RenderableProcessor` [D5/D12]

> drawable 다형화 + 배치를 DeviceContext 로 흡수. **DECISION 2.4** 의 전이 처리 주의.

### Task 2.1: `IRenderable` 인터페이스

**Files:** Create `src/render/i_renderable.h`

```cpp
/**
 * @file i_renderable.h
 * @brief Pass 안에서 함께 정렬되는 drawable 순수 추상 (mesh-family, D5). LSP-최소 3 메서드.
 * @details 고수준은 구체 DeviceContext& (D10 - ICommandRecorder 없음). QueueLayer 가 파인 정렬 키(D8).
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
		/// @brief 자기 자신을 발행 (per-draw 잎). program/uniform/sampler/draw.
		virtual void Render(DeviceContext &rec, const Scene::Camera &cam) const = 0;
		/// @brief Pass 내부 정렬 키 (작을수록 먼저). Material PassKind queue + QueueOffset.
		virtual int  QueueLayer() const = 0;
		// GetRenderStateBlock() <- IRenderStateProvider
	};
} // namespace SJH

#endif // __SJH_I_RENDERABLE_H__
```
- [ ] 빌드/커밋(사용자) — 헤더온리. 커밋: `src/render/i_renderable.h`.

### Task 2.2: DeviceContext `UseProgram` → bool (A-dedup 토대)

**Files:** Modify `src/render/device_context.h`, `.cpp`

> grounding: `UseProgram` 는 현재 dedup 가드 없음(매번 glUseProgram). 반환을 bool(switched)로 만들어 잎이 FrameBlock 을 *전환 시에만* 올리게 한다(배치 보존).

- [ ] **Step 1: 헤더 시그니처**
```cpp
// 변경 전: void UseProgram(const Program &prog);
// 변경 후:
		/// @brief prog 활성화. 이미 활성이면 glUseProgram skip. @return 실제 전환했으면 true (A-dedup).
		bool UseProgram(const Program &prog);
```
- [ ] **Step 2: .cpp 본문**
```cpp
bool DeviceContext::UseProgram(const Program &prog)
{
	if (mStateInitialized && mBoundProgram == &prog)
		return false;                          // 중복 - skip (A-dedup)
	glUseProgram(prog.GetProgramAddr());
	mBoundProgram = &prog;
	return true;
}
```
> ⚠️ `mStateInitialized` 가드를 함께 봐야 함 — `InvalidateStateCache` 후엔 강제 전환(foreign GL 뒤 desync 방지). 기존 호출처(mesh_pass_processor.cpp)는 반환값 무시해도 동작 동일(전환 시 본문 동일). 단 호출처가 `if(program!=lastProg)` 로 이미 가드 → 무해.
- [ ] **Step 3~5: 빌드/육안/커밋** — 동작 0(전환 로직 동일, 반환값만 추가). 커밋: `src/render/device_context.{h,cpp}`.

### Task 2.3: MeshRenderer 가 IRenderable 충족 (잎 자가발행)

**Files:** Modify `src/render/mesh_renderer.h`; Create `src/render/mesh_renderer.cpp`; Modify `src/render/CMakeLists.txt`

- [ ] **Step 1: 헤더 — 상속 + 선언**

include 추가: `#include "render/i_renderable.h"`, `#include "material/material.h"`(Facade 위임 inline 위해 full).
```cpp
// 변경 후 (mesh_renderer.h):
	class MeshRenderer : public Component, public IRenderable
```
public 영역에 추가(나머지 멤버 Mesh/Material/Visible/QueueOffset 유지):
```cpp
		/// @brief D7 Facade 위임 - Material 이 저장처. null 이면 중립(D9 안전).
		const Pass::RenderStateBlock &GetRenderStateBlock() const override
		{
			static const Pass::RenderStateBlock kNeutral{};
			return Material ? Material->GetRenderStateBlock() : kNeutral;
		}
		/// @brief 파인 정렬 키 = Material.PassKind queue + QueueOffset.
		int QueueLayer() const override
		{
			return Material ? Pass::QueueOf(Material->GetPass(), QueueOffset) : QueueOffset;
		}
		/// @brief 자가발행 잎 - 정의는 .cpp.
		void Render(DeviceContext &rec, const Scene::Camera &cam) const override;
```
> ⚠️ Mesh/Material 은 `* const` 멤버(ctor DI). Render 는 읽기만 하므로 무관.

- [ ] **Step 2: `mesh_renderer.cpp` 생성 (잎 구현)**

grounding 의 현 Process WorldMesh 로직을 잎으로 이주. FrameBlock 은 program 전환 시에만(`UseProgram` 반환 활용).
```cpp
/**
 * @file mesh_renderer.cpp
 * @brief MeshRenderer::Render 자가발행 잎 - 구 MeshPassProcessor WorldMesh 경로 이주 (A-naive + A-dedup 토대).
 */
#include "render/mesh_renderer.h"
#include "render/device_context.h"
#include "scene/camera.h"
#include "program/program.h"
#include "object/mesh.h"
#include "texture/texture.h"
#include "material/material_property_block.h"
#include "GL/gl3w.h"
#include <<glm>/glm.hpp>

namespace SJH::Scene
{
	namespace
	{
		// 구 mesh_pass_processor.cpp 익명 헬퍼 이주 (per-draw 발행).
		void UploadMaterialUboMembers(const Program &p, const MaterialPropertyBlock &m)
		{
			for (auto &kv : m.Floats) p.UpdateUniformMember(kv.first, &kv.second, sizeof(float));
			for (auto &kv : m.Ints)   p.UpdateUniformMember(kv.first, &kv.second, sizeof(int));
			for (auto &kv : m.Vec2s)  p.UpdateUniformMember(kv.first, &kv.second, sizeof(glm::vec2));
			for (auto &kv : m.Vec3s)  p.UpdateUniformMember(kv.first, &kv.second, sizeof(glm::vec3));
			for (auto &kv : m.Vec4s)  p.UpdateUniformMember(kv.first, &kv.second, sizeof(glm::vec4));
			for (auto &kv : m.Mat4s)  p.UpdateUniformMember(kv.first, &kv.second, sizeof(glm::mat4));
		}
		void BindSamplers(DeviceContext &rc, const MaterialPropertyBlock &b, const Program &p)
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

	void MeshRenderer::Render(DeviceContext &rec, const Scene::Camera &cam) const
	{
		if (!Material || !Mesh) return;
		const Program *program = Material->GetProgram();
		if (!program) return;

		// program 전환 시에만 FrameBlock(view/proj) 갱신 (A-dedup - UseProgram 반환 활용, 배치 보존).
		if (rec.UseProgram(*program) && program->HasUniformBlocks())
		{
			const glm::mat4 view = cam.GetViewMatrix();
			const glm::mat4 proj = cam.GetProjectionMatrix();
			program->UpdateUniformBlock("FrameBlock", &view, sizeof(glm::mat4), 0);
			program->UpdateUniformBlock("FrameBlock", &proj, sizeof(glm::mat4), sizeof(glm::mat4));
			program->BindUniformBlocks();
		}

		BindSamplers(rec, Material->Properties, *program);           // per-draw (sampler dedup 은 후속)
		if (program->HasUniformBlocks())
		{
			UploadMaterialUboMembers(*program, Material->Properties);
			if (Material->Properties.Vec4s.find("baseColor") == Material->Properties.Vec4s.end())
			{
				const glm::vec4 white(1.0f);                        // 구 fallback 보존
				program->UpdateUniformMember("baseColor", &white, sizeof(glm::vec4));
			}
		}

		rec.ApplyRenderStateBlock(GetRenderStateBlock());           // Material Facade ROP (mLast dedup)

		const glm::mat4 model = GetOwner()->GetWorldMatrix();
		if (program->HasUniformBlocks())
			program->UpdateUniformBlock("DrawBlock", &model, sizeof(glm::mat4), 0);

		rec.BindVAO(Mesh->GetVAO());
		rec.DrawIndexed(Mesh->GetIndexCount());
	}
}
```
> ⚠️ `GetOwner()` 는 `Component::GetOwner()→Actor*`, `Actor::GetWorldMatrix()` (grounding 확인). `program->GetProgramAddr()`/`GetLocation`/`UpdateUniformBlock`/`UpdateUniformMember`/`HasUniformBlocks`/`BindUniformBlocks` 모두 확인된 시그니처.

- [ ] **Step 3: CMake** — `src/render/CMakeLists.txt` 의 `add_library(sjhopengl_render STATIC ...)` 목록에 `mesh_renderer.cpp` 추가.
- [ ] **Step 4~6: 빌드/육안/커밋** — 이 시점 MeshRenderer 는 IRenderable 이지만 *아직 RenderableProcessor 가 안 씀*(Process 가 여전히 DrawCommand 경로). 동작 0. 단 MeshRenderer 가 추상 아님(3 메서드 다 구현) → 인스턴스화 OK. 커밋: `src/render/mesh_renderer.{h,cpp}` + CMakeLists.

### Task 2.4: `MeshPassProcessor` → `RenderableProcessor` (Flat, IRenderable*)

> **🟦 DECISION 2.4 (Task-time): ScreenQuad 전이 처리**
> 현 `MeshPassProcessor` 는 WorldMesh + ScreenQuad(PassComponent) 둘 다 처리. v5 목표는 WorldMesh=IRenderable(RenderableProcessor), ScreenQuad=PostFxPass(Phase 3). Phase 2 에서 한 번에 분리하면 빌드 깨짐.
>
> | 옵션 | Phase 2 RenderableProcessor | Phase 3 |
> |---|---|---|
> | **A (추천)** | WorldMesh 는 `IRenderable*`(flat), ScreenQuad 는 *전이용 별 경로 유지*(SubmitScreenQuad + 내부 blit). SceneRenderer 무변(둘 다 Submit). | Phase 3 에서 ScreenQuad 경로를 PostFxPass 로 이관 후 RenderableProcessor 에서 제거 → 순수 IRenderable |
> | B | Phase 2 에서 ScreenQuad 까지 PostFxPass 로 즉시 분리 | Phase 2 가 Phase 3 와 엉킴(한 Pass씩 육안 원칙 깨짐) |
>
> **추천 A** — 전이용 dual-mode 로 Phase 2 를 비파괴 유지. 착수 시 확정.

**Files:** Modify `src/render/mesh_pass_processor.{h,cpp}` → 개명/일반화. `render_passable.impls.{h,cpp}`(멤버 타입). `CMakeLists`.

- [ ] **Step 1: 클래스 개명 + Entry/Submit 일반화 (옵션 A)**

`MeshPassProcessor` → `RenderableProcessor`. 내부:
```cpp
class RenderableProcessor
{
  public:
    // -- World (IRenderable, flat) --
    void Submit(const IRenderable *r, float viewDepth) { mWorld.push_back({r, r->QueueLayer(), viewDepth}); }
    // -- ScreenQuad (전이용 - Phase 3 에서 PostFxPass 로 이관 후 제거) --
    void SubmitScreenQuad(Framebuffer *in, Framebuffer *out, Material *passMat) { mScreen.push_back({in, out, passMat}); }
    void SetScreenQuadMesh(Mesh *m) { mScreenQuadMesh = m; }
    void SetBypassMaterial(Material *m) { mBypassMat = m; }
    const Framebuffer *GetLastOutputFB() const { return mLastOutputFB; }

    void Clear() { mWorld.clear(); mScreen.clear(); mLastOutputFB = nullptr; }
    std::size_t Size() const { return mWorld.size() + mScreen.size(); }
    void Sort();                                       // mWorld 만 정렬 (stable_sort)
    void Process(DeviceContext &rc, const Scene::Camera &cam);   // world flat + screen blit

  private:
    struct WorldEntry  { const IRenderable *r; int queueLayer; float depth; };
    struct ScreenEntry { Framebuffer *in; Framebuffer *out; Material *mat; };
    std::vector<WorldEntry>  mWorld;
    std::vector<ScreenEntry> mScreen;
    Mesh              *mScreenQuadMesh = nullptr;
    Material          *mBypassMat      = nullptr;
    const Framebuffer *mLastOutputFB   = nullptr;
};
```

- [ ] **Step 2: Sort (mWorld, stable_sort 유지 — 결정성)**
```cpp
void RenderableProcessor::Sort()
{
    std::stable_sort(mWorld.begin(), mWorld.end(),
        [](const WorldEntry &a, const WorldEntry &b) {
            if (a.queueLayer != b.queueLayer) return a.queueLayer < b.queueLayer;
            if (Pass::IsTransparentQueue(a.queueLayer)) return a.depth < b.depth;  // back-to-front
            return a.depth > b.depth;                                              // front-to-back
        });
}
```
> 구 SortMultiStage 의 program/material 그룹핑 2차/3차 키는 A-dedup(UseProgram skip)로 효과 보존되므로 생략 가능. 단 *그룹핑이 dedup 효율을 높이므로* program-포인터 보조키를 남겨도 됨(투명 아닌 경우). 착수 시 1택.

- [ ] **Step 3: Process (world flat + screen blit)**
```cpp
void RenderableProcessor::Process(DeviceContext &rc, const Scene::Camera &cam)
{
    rc.InvalidateStateCache();                         // per-Process 캐시 무효화 (D-RS-2)
    // -- World: §0.5 flat (per-renderable 자가발행) --
    for (auto &e : mWorld)
    {
        rc.ApplyRenderStateBlock(e.r->GetRenderStateBlock());
        e.r->Render(rc, cam);
    }
    // -- ScreenQuad: 전이용 (구 ScreenQuad DrawCommand 로직 이식; Phase 3 에서 PostFxPass 로 이관) --
    for (auto &e : mScreen)
    {
        if (!e.in || !e.out || !mScreenQuadMesh) continue;
        Material *mat = e.mat ? e.mat : mBypassMat;
        if (!mat) continue;
        Program *prog = const_cast<Program*>(mat->GetProgram());
        if (!prog) continue;
        rc.BeginFrame(*e.out);
        rc.ApplyRenderStateBlock(Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Screen));
        mat->Properties.Textures["uScene"] = { e.in->GetColorAttachment().get(), 0 };
        rc.UseProgram(*prog);
        if (prog->HasUniformBlocks()) { /* UploadMaterialUboMembers(*prog, mat->Properties); prog->BindUniformBlocks(); */ }
        /* BindSamplers(rc, mat->Properties, *prog); */                 // 구 로직 그대로 (헬퍼 공유 필요 - 아래 주의)
        rc.BindVAO(mScreenQuadMesh->GetVAO());
        if (auto ebo = mScreenQuadMesh->GetIndexBuffer()) ebo->Bind(); // VAO/EBO 오염 가드
        rc.DrawIndexed(mScreenQuadMesh->GetIndexCount());
        mLastOutputFB = e.out;
    }
}
```
> ⚠️ ScreenQuad 의 `UploadMaterialUboMembers`/`BindSamplers` 는 구 mesh_pass_processor.cpp 익명 헬퍼였음. Task 2.3 에서 mesh_renderer.cpp 로 이주했으므로, **이 전이용 ScreenQuad 경로가 쓰려면** 헬퍼를 *공유 위치*(예: `<src>/render/draw_ops.h` free 함수)로 빼거나, ScreenQuad 경로용으로 mesh_pass_processor.cpp(→renderable_processor.cpp) 에 사본 유지. **권장: Phase 2 에선 헬퍼를 renderable_processor.cpp 에 유지(중복 허용), Phase 3 에서 ScreenQuad→PostFxPass 이관 시 정리.** (전이 상태라 중복은 한시적.)

- [ ] **Step 4: SceneRenderer 연동** — `render_passable.impls.h:90` `MeshPassProcessor mProcessor;` → `RenderableProcessor mProcessor;`. `render_passable.impls.cpp` 의 `CollectFromActor` 에서 MeshRenderer 수집을 `mProcessor.Submit(mr, depth)`(IRenderable*), PassComponent 수집을 `mProcessor.SubmitScreenQuad(pc->InputFB, pc->OutputFB, (pc->Enabled&&pc->mMaterial)?pc->mMaterial:nullptr)` 로. depth=`(viewMat*model)[3][2]` 유지. `RenderWithCamera` 의 `SortMultiStage`→`Sort`, `Process(rc,viewMat,projMat)`→`Process(rc, cam)`.
> ⚠️ `Process(rc, cam)` 로 시그니처 변경 → cam 에서 view/proj 도출(잎이 사용). RenderWithCamera 는 이미 cam 보유.

- [ ] **Step 5: 파일/CMake** — `git mv mesh_pass_processor.{h,cpp} renderable_processor.{h,cpp}` + 헤더가드/include 갱신 + `src/render/CMakeLists.txt` target_sources 갱신. (파일명 유지하고 클래스만 개명도 가능 — 착수 시 1택; 메모리 정리 차원에선 파일명도 권장.)
- [ ] **Step 6: 빌드(사용자)**.
- [ ] **Step 7: 실행 육안(사용자)** — mesh/sprite/text 출력·순서·반투명·PostFX 동일 (Phase 2 핵심 회귀 게이트). Text 글리프(SpriteRenderer=MeshRenderer=IRenderable) 도 정상.
- [ ] **Step 8: Commit(사용자)** — 변경 render 파일 + impls + CMake.

> **Phase 2 완료**: IRenderable 다형, RenderableProcessor flat(world) + 전이 screen, A-dedup(UseProgram bool). 픽셀/성능 무회귀.

---

## Phase 3 — `IPassable` 계약 + 역할군 Pass 통합 [D2/D3/D4] — *한 Pass씩 육안*

> 회귀 위험 최대. 각 4.x 마다 빌드+육안+커밋. grounding: 현 `IRenderPassable::Render(RenderTarget&)`(반환 없음) + 외부 체이닝(`GetLastSceneOutput`/`SetSources`, main). 목표 `Draw(DeviceContext&, const Texture*)/GetPassResult()->const Texture*/BeforeIndex`.

### Task 3.1: `IPassable` 계약 (개명 + 시그니처)

**Files:** Modify `src/render/render_passable/render_passable.h`

```cpp
// 변경 후:
#ifndef __SJH_I_PASSABLE_H__
#define __SJH_I_PASSABLE_H__
namespace SJH
{
	class Texture;
	class DeviceContext;
	class IPassable
	{
	  public:
		virtual ~IPassable() = default;
		/// @brief 이 Pass 발행. before = 입력 Pass 결과(없으면 nullptr). 고수준은 구체 DeviceContext(D10).
		virtual void           Draw(DeviceContext &rec, const Texture *before) = 0;
		/// @brief 이 Pass 출력(=Framebuffer color attachment, D6). backbuffer 종단은 nullptr 허용.
		virtual const Texture *GetPassResult() const = 0;
		virtual void           OnResize(int /*w*/, int /*h*/) {}
		int BeforeIndex = -1;   // 입력 Pass vector 인덱스. -1 = sceneRaw
	};
}
#endif
```
`IRenderPassable` → `IPassable` 전 사용처 치환(impls, ParticleStage, main, render_pipeline). 헤더가드/파일명 정리.
> ⚠️ 인터페이스가 깨지므로 3.1 은 3.2~3.6 과 한 묶음으로 컴파일 통과. **각 구체를 IPassable 로 바꿀 때마다 빌드/육안.**

### Task 3.2~3.6: 역할군 Pass (각각 IPassable) — *한 Pass씩*

- [ ] **3.2 `WorldPass : IPassable`** ← `SceneRenderer`+`CameraStage` 흡수. 멤버 `RenderableProcessor mProc / LightUboUploader mUploader / Camera* mCamera / vector<Program*> mActivePrograms`. `Draw(rec, before)`: grounding 의 RenderWithCamera 본문 이식 — `cam.GetTargetRenderTarget()` null guard, NoClear 분기(BeginFrame vs BindTarget), Light 수집(dir/points[]/spots[] Enabled 필터)+`mUploader.Update`+`BindTo(mActivePrograms)`, `mProc.Clear`+`CollectFromActor`(MeshRenderer만 — PassComponent 는 PostFxPass 로 이관)+`Sort`+`Process(rec, *mCamera)`. `GetPassResult()` = `mCamera->GetTargetRenderTarget()` 의 Framebuffer color attachment `.get()`(FBO 일 때). `SetActivePrograms` 유지.
  > CollectFromActor 가 `GetComponent<MeshRenderer>()`(또는 더 일반적으로 `GetComponent<IRenderable>()` slow-path) 로 mesh 수집. PassComponent 수집은 제거(PostFxPass 가 담당).
- [ ] **3.3 `SkyboxPass : IPassable`** ← 현 skybox(Material PassKind=Skybox queue 2500). **DECISION 4.3**: 독립 Pass vs WorldPass skybox-queue IRenderable. *추천 독립*(역할군 코스 가독성). 착수 시 1택.
- [ ] **3.4 `ParticlePass : IPassable`** ← `apps/_MyApp_/src/VFX/ParticleStage`. grounding: 현재 `ParticleStage(VFXSystem*, Camera*)` + `Render(RenderTarget&)` [target 미사용] + 끝에 `InvalidateStateCache`. → `Draw(rec, before)`(before 미사용), efk 직접 호출 유지, 진입/이탈 `rec.InvalidateStateCache()`(foreign-GL, memory `vao_ebo_thirdparty_corruption`). `GetPassResult()` = nullptr(자기 합성 안 하면) 또는 worldCam RT. D9 중립 ROP.
- [ ] **3.5 `ImGuiPass : IPassable`** ← main.cpp:370 `ImGui::Render()` 흡수. `Draw(rec, before)` 가 ImGui draw data 렌더, 진입 `rec.InvalidateStateCache()`. (memory `imgui_layer_separation` 정합 확인.)
- [ ] **3.6 `PostFxPass : IPassable`** ← `PassComponent`+`ScreenQuadStage` 흡수 + Phase 2 의 전이 ScreenQuad 경로 제거. `Draw(rec, before)`: before(scene 출력)를 `uScene` 로 받아 자기 출력 FBO 에 blit(구 ScreenQuadStage::Render + Process ScreenQuad 로직 통합). `GetPassResult()` = 자기 출력 FBO attachment(종단이면 backbuffer→nullptr). 구 `BuildPostFXChain`(Actor 자식 PassComponent) → PostFxPass 가 직접 보유로 단순화. **여기서 RenderableProcessor 의 전이 ScreenQuad 경로(SubmitScreenQuad/mScreen) 삭제 → 순수 IRenderable.**

> 각 3.x: (1) 구체 IPassable 화 + Draw/GetPassResult, (2) 사용자 빌드, (3) 사용자 육안(해당 Pass 산출 동일), (4) 사용자 커밋(해당 Pass 파일만). 5~6 커밋.

> **Phase 3 완료**: 모든 stage 가 IPassable. 외부 체이닝(GetLastSceneOutput/SetSources) 제거. RenderableProcessor 순수 IRenderable. GUI 완전 무회귀.

---

## Phase 4 — `PassIterator` 모듈화 + main 배선 + DebugPassIndex [D8 코스]

### Task 4.1: `PassIterator`

**Files:** Create `src/render/pass_iterator.h`, `.cpp`; Modify `src/render/CMakeLists.txt`

```cpp
/**
 * @file pass_iterator.h
 * @brief 역할군 Pass 순서 보유 + 연쇄 실행 + Present + 디버그 토글 (D8 코스). main 의 유일 렌더 진입점.
 */
#ifndef __SJH_PASS_ITERATOR_H__
#define __SJH_PASS_ITERATOR_H__
#include <vector>
namespace SJH
{
	class IPassable; class DeviceContext; class RenderTarget;
	class PassIterator
	{
	  public:
		int  Add(IPassable *pass);                                   // 코스 순서 끝에 추가 (비소유). 반환=인덱스
		void Execute(DeviceContext &rec, RenderTarget &backbuffer);  // BeforeIndex 연쇄 Draw
		void Resize(int w, int h);
		int  DebugPassIndex = -1;                                    // >=0 이면 그 Pass 결과만 화면 출력
	  private:
		std::vector<IPassable *> mPasses;                            // 비소유 (owner = Application)
	};
}
#endif
```
`.cpp`: `Execute` 가 각 pass 에 `before = (pass->BeforeIndex>=0 ? mPasses[BeforeIndex]->GetPassResult() : nullptr)` 주입 후 `Draw(rec, before)`. `DebugPassIndex>=0` 이면 해당 결과를 backbuffer 합성(PostFxPass blit 재사용). CMake: `pass_iterator.cpp` target_sources.

### Task 4.2: main.cpp 배선

**Files:** Modify `apps/_MyApp_/main.cpp`, `src/render_bootstrap/render_pipeline.{h,cpp}`

- [ ] `std::vector<unique_ptr<IRenderPassable>> mStages`(L447) → `PassIterator mPassIterator` + Pass 소유 멤버. `SetupDefaultPipeline` 이 PassIterator 조립 반환하도록 확장.
- [ ] 렌더 루프(L366) `for(s:mStages) s->Render(*mDefaultTarget)` → `mPassIterator.Execute(DeviceContext::Get(), *mDefaultTarget);`. 외부 `SetActivePrograms`(L358)/`SetSources`(L363)/`GetLastSceneOutput` 제거(BeforeIndex 로 대체; SetActivePrograms 는 WorldPass 내부 또는 Execute 전 1회). `ImGui::Render()`(L370) 제거 → ImGuiPass.
- [ ] 코스 순서 = `[SkyboxPass, WorldPass, ParticlePass, PostFxPass, ImGuiPass]`(역할군 명시). 각 BeforeIndex 배선.
- [ ] 빌드/육안(평소 동일 + DebugPassIndex 토글 키)/커밋.

> **Phase 4 완료**: main 은 PassIterator 하나만. 코스 순서 역할군 명시. DebugPassIndex 동작.

---

## Phase 5 — 정리 / 네이밍 / 문서

- [ ] **5.1**: 구 심볼(`IRenderPassable`/`SceneRenderer`/`CameraStage`/`ScreenQuadStage`/`MeshPassProcessor`/`PassComponent` 잔재) grep=0. `[[deprecated]] SceneRenderer::Render` 제거.
- [ ] **5.2**: `doc/diagrams/engine-migration-class.*` 를 구현 후 상태로 갱신 후 `dot -Tsvg` 재렌더.
- [ ] **5.3**: 메모리/CLAUDE.md(모듈 목록) 갱신. `rendering-migration-design` 메모리에 "구현 완료" change log.
- [ ] Commit(사용자) path-scoped.

---

## 검증 / Out of Scope

**검증(사용자)**: `export PATH="$HOME/slang/bin:$PATH" && cmake --build --preset ninja --target _MyApp_` → 출력 육안. 실행 `cd build_ninja/apps/_MyApp_ && ./_MyApp_`. Phase 1·2·3·4 GUI 회귀 육안. `Sort` stable_sort 유지(결정성). **회귀 핫스팟**: 반투명 healthbar/shadow 알파(blend off→on footgun — grounding G2), Outline stencil 2-pass, Skybox depth(LEQUAL/.xyww), AlphaTest sprite cull, Effekseer/ImGui foreign-GL(InvalidateStateCache).

**Out of Scope**: 백엔드 Facade+Strategy(IGraphicsBackend: OpenGL/Vulkan/Metal) + PSO(PipelineStateDesc/BindPipelineState) + IRenderTargetPool/TransientTargetPool → **후속 멀티백엔드 프로젝트**(D10/D11/D13). · Shadow/Deferred/MSAA/HDR/Cubemap · TextRenderable/능력 인터페이스 · data-driven JSON · 다중 BeforeIndex · ICommandRecorder/ITargetAllocator.

---

## Task-time DECISION 요약 (착수 시 사용자 1택)

| # | 결정 | 추천 |
|---|---|---|
| 2.4 | RenderableProcessor ScreenQuad 전이 | **A** — Phase 2 dual-mode 유지, Phase 3 에서 PostFxPass 로 이관/제거 |
| 2.4-b | Sort 보조키(program 그룹핑) 유지? | A-dedup 으로 효과 보존되나 그룹핑이 dedup 효율↑ → 불투명에 program 보조키 권장 |
| 2.5 | mesh_pass_processor 파일명도 개명? | 권장(renderable_processor.{h,cpp}) — 메모리 정리 |
| 3.3/4.3 | SkyboxPass 독립 vs WorldPass skybox-queue | 독립 Pass(역할군 가독성) |
| 3.x | A-dedup 깊이 (sampler/UBO 멤버 dedup) | 우선 FrameBlock(UseProgram bool)만, sampler/멤버 dedup 은 프로파일 후 |
| 4.x | SetActivePrograms 위치 | WorldPass 내부 또는 Execute 전 1회 (rr pull 금지 - D-1 push 유지) |

---

## Self-Review (writing-plans 체크리스트)

- **Spec coverage**: D1(in-place)=전 Phase / D2·D3(IPassable 계약·개명)=3.1 / D4(역할군)=3.2-3.6 / D5(IRenderable mesh-family)=2.1-2.3 / D6(GetPassResult→const Texture*=GetColorAttachment().get())=3.x / D7(Material ROP+Facade)=Phase 1 / D8(코스=PassIterator·파인=QueueLayer)=4.1+2.4 / D9(중립 default)=MeshRenderer kNeutral + Particle/ImGui / D10(구체 DeviceContext)=IRenderable/IPassable 시그니처 / D11(풀 defer)=OoS / D12(A-dedup)=2.2 UseProgram bool / D13(PSO 후속)=OoS. **전 결정 매핑.**
- **Placeholder scan**: 신규 인터페이스/Material/DeviceContext/MeshRenderer.cpp/RenderableProcessor = *완전 코드*(grounding 기반). Pass 형성(3.x)은 grounded 현 코드 이식 + 명시 DECISION 종속(전이 dual-mode)으로 표기 — placeholder 아님.
- **Type consistency**: `Pass::RenderStateBlock`(SJH::Pass struct), `Pass::RenderQueue`(enum), `GetRenderStateBlock`/`QueueLayer`/`GetPassResult`/`BeforeIndex`/`Render(DeviceContext&,Camera&)`/`Draw(DeviceContext&,const Texture*)` 전 Task 일관. `UseProgram`→bool 일관. `GetProgramAddr`/`GetColorAttachment().get()`/`GetOwner()->GetWorldMatrix()` grounding 확인.
- **Grounding**: 전 시그니처 2026-06-23 live(HEAD 68506a8) 6-슬라이스 검증. 착수 시 재확인 경고 유지.
