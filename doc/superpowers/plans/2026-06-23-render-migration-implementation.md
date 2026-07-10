# 렌더링 시스템 마이그레이션 구현 Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> 🔴 **SUPERSEDED (2026-06-23) — 재생성 완료.** 이 Plan 은 마이그레이팅계획안 **v4** 기반(`ICommandRecorder`/`ITargetAllocator`/풀 + `DECISION 1.3`/`3.3`/`P2`)이다. v5 에서 범위 분리됨(D10~D13). **정본 구현 plan = [`2026-06-23-render-migration-implementation-v5-reviewed.md`](2026-06-23-render-migration-implementation-v5-reviewed.md)** (v5 기준 재생성 + 4-렌즈 적대적 리뷰 교정). 설계 정본 = [`doc/마이그레이팅계획안.md`](../../마이그레이팅계획안.md) (v5). **이 Plan(v4) 으로 구현하지 말 것.**

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 현 렌더 모듈을 레퍼런스 파이프라인 형태(IPassable 연쇄 + IRenderable + Flat RenderableProcessor + 역할군 Pass + PassIterator)로 *in-place* 재형성하되, **렌더 결과(픽셀) 변화 0** 을 유지한다.

**Architecture:** 비파괴 인터페이스 도입(Phase 1~2) → drawable 다형화(Phase 3) → Pass 계약 교체 + 역할군 통합(Phase 4) → main 배선 모듈화(Phase 5) → 정리(Phase 6). 각 Phase 는 빌드 GREEN + 육안 무회귀를 게이트로 순차 진행. 현 엔진이 목표의 ~90% 를 이미 구현 → 대부분 *확장(E)*, 신규(N)는 얇은 seam.

**Tech Stack:** C++17, OpenGL 4.1 Core (gl3w), GLM, CMake (Ninja preset), Slang→GLSL410 toolchain. 자원/상태는 Meyer 싱글톤(`DeviceContext`/`ResourceRegistry`), 씬은 Actor/Component.

---

## ⚠️ 이 Plan 의 프로세스 규약 (writing-plans 기본 TDD 와의 의도적 차이)

이 프로젝트는 **단위 테스트(Catch2) 전량 폐기 상태**(`test/` 부재)이고, 렌더 도메인 검증은 **사용자 빌드 + 육안 회귀**가 정본이다 (memory `no_auto_tests`, `user-parallel-git-and-builds`). writing-plans 스킬의 instruction-priority 규칙("user instructions always take precedence")에 따라 각 Task 의 검증 단계는 *failing test* 대신:

1. **빌드 = 사용자**가 직접: `export PATH="$HOME/slang/bin:$PATH" && cmake --build --preset ninja --target _MyApp_` → 컴파일 출력을 붙여주면 에이전트 육안 확인.
2. **실행 = 사용자**가 직접: `cd build_ninja/apps/_MyApp_ && ./_MyApp_` → GUI 가 *직전과 동일* 한지 육안.
3. **커밋 = 사용자**가 path-scoped (`git commit <경로>`), `git add -A` 금지, `Co-Authored-By` 미사용.

에이전트는 **구현 + 보고만** 한다. 각 Task 의 "Commit" 스텝은 *사용자에게 제안할 커밋 메시지/경로*를 명시하는 것이지 에이전트가 실행하는 것이 아니다.

---

## 🔒 가드레일 (전 Task 불변)

- **GL 격리 불변식**: *우리* `glXxx` 는 `DeviceContext`(+자원 RAII: `Texture`/`Framebuffer`) 안에만. 신규 `ICommandRecorder`/`IRenderable`/`IPassable`/`IRenderTargetPool`/`PassIterator` 헤더에 GL include ❌(전방선언/추상만). *외부 라이브러리*(Effekseer/ImGui) 호출은 `ParticlePass`/`ImGuiPass` 에 격리 + `DeviceContext::InvalidateStateCache()` 경계.
- **주석 한국어 + ASCII only** (특수문자 0 — memory `doxygen-ascii-comment-convention`). 헤더 가드 `__SJH_XXX_H__`.
- **`extern/sb7code` 수정 금지**.
- **Phase 4 는 한 Pass 씩** 전환 + 매번 빌드 GREEN + 육안 (회귀 위험 최대 구간).
- **착수 전 HEAD 재측정** (`git log/status`) — 사용자 병렬 git 가정. 직전 커밋 `68506a8 렌더링 모듈 Rename` 이후 본 plan 의 경로/심볼은 *조사 시점(2026-06-23)* 기준이므로 Task 시작 시 live 코드로 재확인.

---

## File Structure (생성/수정 맵)

### 신규 파일 (N)
| 파일 | 책임 |
|---|---|
| `<src>/render/i_command_recorder.h` | `ICommandRecorder` 순수 추상 (DeviceContext draw/state 발행 계약) |
| `<src>/resource_registry/i_target_allocator.h` | `ITargetAllocator` + `TargetDesc{w,h,format}` (RT/Texture 생성 추상) |
| `<src>/render/render_target_pool.h` / `.cpp` | `IRenderTargetPool` + `DedicatedTargetPool` (Acquire=Find??Create 위임) |
| `src/material/i_render_state_provider.h` | `IRenderStateProvider` ROP Facade 인터페이스 |
| `src/render/i_renderable.h` | `IRenderable : IRenderStateProvider` (Render/QueueLayer) |
| `src/render/pass_iterator.h` / `.cpp` | `PassIterator` (vector<IPassable\*> 소유/실행/Present/DebugPassIndex) |

### 수정 파일 (E)
| 파일 | 변경 |
|---|---|
| `src/render/device_context.h/.cpp` | `: public ICommandRecorder` + 해당 메서드 `override` |
| `src/resource_registry/resource_registry.h/.cpp` | `: public ITargetAllocator` + 위임 구현 |
| `src/material/material.h` | `: public IRenderStateProvider`, `RenderStateBlock mState` 멤버, `SetPass` seed, `GetRenderStateBlock()` |
| `src/render/mesh_renderer.h` | `MeshRenderer : public Component, public IRenderable` 구현 |
| `src/render/mesh_pass_processor.{h,cpp}` | → `RenderableProcessor` (IRenderable\* 수집/정렬; 배치 Process 보존 — 3.3 DECISION) |
| `src/render/render_passable/render_passable.h` | `IRenderPassable` → `IPassable` 계약 교체 (Draw/GetPassResult/BeforeIndex) |
| `src/render/render_passable/render_passable.impls.{h,cpp}` | `WorldPass`/`Screen... ` → 역할군 Pass 로 통합 |
| `apps/_MyApp_/src/VFX/ParticleStage.{h,cpp}` | → `ParticlePass : IPassable` |
| `apps/_MyApp_/main.cpp` | `mStages` → `PassIterator` 하나; ImGui → `ImGuiPass` |
| `src/render_bootstrap/render_pipeline.{h,cpp}` | `SetupDefaultPipeline` 가 `PassIterator` 조립 반환 |
| `src/scene/camera.h` | `bool Sees(uint64_t layerBits) const` 헬퍼 (선택) |
| 각 `CMakeLists.txt` | 신규 .cpp 배선 |

---

## Phase 1 — 기반 인터페이스 [N+E, 비파괴, 동작 0]

> 목표: seam 만 도입. 어떤 소비자도 아직 인터페이스로 호출하지 않음 → 픽셀 변화 0. 컴파일만 통과하면 성공.

### Task 1.1: `ICommandRecorder` 추출 + DeviceContext 상속

**Files:**
- Create: `<src>/render/i_command_recorder.h`
- Modify: `src/render/device_context.h:61` (class 선언), `src/render/device_context.cpp` (메서드 정의 — 시그니처 불변, override 추가)

- [ ] **Step 1: `ICommandRecorder` 헤더 생성**

`DeviceContext` 의 공개 메서드 집합과 *정확히 일치*하는 순수 추상. (live 시그니처는 `device_context.h:71-124` 에서 재확인.)

```cpp
/**
 * @file i_command_recorder.h
 * @brief GL draw/state 발행 명령의 순수 추상 - DeviceContext 가 유일 구현.
 *
 * @details
 *  레퍼런스 파이프라인의 ICommandRecorder seam. Pass/Processor 가 *구체* DeviceContext 대신
 *  이 인터페이스에 의존하게 만들어, GL 발행 경로를 단일 추상 뒤로 격리한다 (GL 격리 불변식).
 *  본 헤더는 GL 타입(GLuint/GLenum/GLbitfield/GLsizei)을 gl3w 로 노출하나 *호출만* 추상화 -
 *  실제 glXxx 는 DeviceContext(구체)에만 거주.
 */
#ifndef __SJH_I_COMMAND_RECORDER_H__
#define __SJH_I_COMMAND_RECORDER_H__

#include "GL/gl3w.h"
#include "material/pass.h"   // Pass::RenderStateBlock - ApplyRenderStateBlock 입력.

namespace SJH
{
	class Program;
	class RenderTarget;

	/// @brief GL draw/state 발행 계약 - DeviceContext 가 유일 구현 (Meyer 싱글톤).
	class ICommandRecorder
	{
	  public:
		virtual ~ICommandRecorder() = default;

		// -- bound 상태 --
		virtual void UseProgram(const Program &prog) = 0;
		virtual void BindVAO(GLuint vao) = 0;
		virtual void BindTexture(GLuint unit, GLuint tex) = 0;
		virtual void BindTarget(RenderTarget &target) = 0;
		virtual void Clear(GLbitfield mask) = 0;
		virtual void ApplyRenderStateBlock(const Pass::RenderStateBlock &want) = 0;
		virtual void InvalidateStateCache() = 0;

		// -- draw --
		virtual void DrawIndexed(GLsizei count) = 0;
		virtual void DrawArrays(GLenum mode, GLsizei count) = 0;

		// -- 패스 진입 --
		virtual void BeginFrame(RenderTarget &target) = 0;
	};
} // namespace SJH

#endif // __SJH_I_COMMAND_RECORDER_H__
```

- [ ] **Step 2: DeviceContext 가 상속**

`device_context.h` 상단 include 에 `#include "<render>/i_command_recorder.h"` 추가, 클래스 선언을 변경:

```cpp
// 변경 전: class DeviceContext
// 변경 후:
	class DeviceContext : public ICommandRecorder
```

그리고 10개 메서드 선언 끝에 `override` 추가 (시그니처는 전부 동일하므로 `;` 앞에 `override` 만):
`UseProgram` / `BindVAO` / `BindTexture` / `BindTarget` / `Clear` / `ApplyRenderStateBlock` / `InvalidateStateCache` / `DrawIndexed` / `DrawArrays` / `BeginFrame`.

> ⚠️ `device_context.cpp` 의 *정의*는 시그니처가 안 바뀌므로 수정 불필요 (override 는 선언부만). 싱글톤 사설 생성자/소멸자도 그대로.

- [ ] **Step 3: 빌드 검증 (사용자)**

Run (사용자): `export PATH="$HOME/slang/bin:$PATH" && cmake --build --preset ninja --target _MyApp_`
Expected: 성공 (vtable 추가 외 동작 변화 0). 컴파일 출력 붙이면 에이전트 확인.

- [ ] **Step 4: 실행 육안 (사용자)** — GUI 가 직전과 동일. (이 Task 는 동작 변화 0 이라 회귀 가능성 사실상 없음.)

- [ ] **Step 5: Commit (사용자, path-scoped)**

```
git commit <src>/render/i_command_recorder.h src/render/device_context.h \
  -m "[refactor] ICommandRecorder seam 도입 + DeviceContext 상속 (비파괴)"
```

---

### Task 1.2: `TargetDesc` + `ITargetAllocator` 추출 + ResourceRegistry 구현

**Files:**
- Create: `<src>/resource_registry/i_target_allocator.h`
- Modify: `src/resource_registry/resource_registry.h:61` (상속), `.cpp` (override 정의는 기존 위임 호출이라 불필요/얇음)

- [ ] **Step 1: `ITargetAllocator` + `TargetDesc` 헤더 생성**

`ResourceRegistry::CreateFramebuffer(key,w,h)` / `FindFramebuffer(key)` / `CreateTexture(key,image)` 를 추상화 (live 시그니처 `resource_registry.h:72,127,133`).

```cpp
/**
 * @file i_target_allocator.h
 * @brief RenderTarget/Texture 생성의 Factory 추상 - ResourceRegistry 가 구현.
 *
 * @details
 *  레퍼런스 파이프라인의 ITargetAllocator. IRenderTargetPool 이 이 추상 뒤에서 RT 를 확보한다.
 *  Unity RTHandleSystem / Unreal RenderTargetPool 의 할당 계층 정통. 본 추상은 *생성* 만 -
 *  캐시/재사용 정책은 IRenderTargetPool 책임 (SRP 분리).
 */
#ifndef __SJH_I_TARGET_ALLOCATOR_H__
#define __SJH_I_TARGET_ALLOCATOR_H__

#include <string>

namespace SJH
{
	class Framebuffer;
	class Texture;
	class Image;

	/// @brief 렌더 타겟 명세 - 폭/높이 + (미래) 포맷. 현 엔진은 RGBA8 고정이라 format 은 예약.
	struct TargetDesc
	{
		int Width  = 0;
		int Height = 0;
		// 미래 확장 슬롯 (HDR/Depth/MRT). 현재 ResourceRegistry::CreateFramebuffer 는 RGBA8+depth 고정.
		// enum class Format { RGBA8, RGBA16F, Depth } Format = Format::RGBA8;
	};

	/// @brief RT/Texture 생성 Factory 추상 - ResourceRegistry 가 유일 구현.
	class ITargetAllocator
	{
	  public:
		virtual ~ITargetAllocator() = default;

		/// @brief key 로 Framebuffer(FBO) 생성+캐시. 이미 있으면 nullptr (ResourceRegistry Create 계약).
		virtual Framebuffer *CreateFramebuffer(const std::string &key, int width, int height) = 0;

		/// @brief key 로 캐시된 Framebuffer 조회. 없으면 nullptr.
		virtual Framebuffer *FindFramebuffer(const std::string &key) = 0;
	};
} // namespace SJH

#endif // __SJH_I_TARGET_ALLOCATOR_H__
```

> 설계 메모: `CreateTexture`/`FindTexture` 도 allocator 후보지만, RT 풀의 직접 수요는 Framebuffer 이므로 인터페이스는 *최소*(ISP)로 FBO 2메서드만. Texture 추가는 Shadow Map/RTT 확장 때 (Out of Scope, `RenderingSystemRefactorReserch.md §7`).

- [ ] **Step 2: ResourceRegistry 가 상속**

`resource_registry.h` include 에 `#include "i_target_allocator.h"` 추가, 선언:

```cpp
// 변경 전: class ResourceRegistry
// 변경 후:
	class ResourceRegistry : public ITargetAllocator
```

`CreateFramebuffer` / `FindFramebuffer` 선언 끝에 `override` 추가. **`.cpp` 정의는 시그니처 동일 → 수정 불필요.**

- [ ] **Step 3~5: 빌드/실행/커밋 (사용자)** — Task 1.1 과 동일 형식. 커밋:

```
git commit <src>/resource_registry/i_target_allocator.h src/resource_registry/resource_registry.h \
  -m "[refactor] ITargetAllocator + TargetDesc seam 도입 + ResourceRegistry 구현 (비파괴)"
```

---

### Task 1.3: `IRenderTargetPool` + `DedicatedTargetPool` (위임 adapter)

**Files:**
- Create: `<src>/render/render_target_pool.h`, `<src>/render/render_target_pool.cpp`
- Modify: `src/render/CMakeLists.txt:1-9` (target_sources 에 `render_target_pool.cpp` 추가)

- [ ] **Step 1: 헤더 생성**

```cpp
/**
 * @file render_target_pool.h
 * @brief RenderTarget 확보/재사용 정책 추상 + 전용(영속) 구현.
 *
 * @details
 *  ITargetAllocator(생성) 위에 *재사용 정책* 을 올린 계층 (Unity RTHandleSystem / Unreal
 *  FRenderTargetPool 정통). DedicatedTargetPool 은 "한 번 만들면 계속 덮어쓴다"(transient 아님)
 *  정책 - 현 엔진의 PostFX FBO 사용 패턴(매 프레임 같은 FBO 재바인딩)과 일치.
 *  Acquire = key 로 Find, 없으면 desc 로 Create (Find??Create).
 */
#ifndef __SJH_RENDER_TARGET_POOL_H__
#define __SJH_RENDER_TARGET_POOL_H__

#include "<resource_registry>/i_target_allocator.h"
#include <string>

namespace SJH
{
	class Framebuffer;

	/// @brief RT 확보 정책 추상. Acquire 한 호출로 캐시 hit 또는 생성.
	class IRenderTargetPool
	{
	  public:
		virtual ~IRenderTargetPool() = default;

		/// @brief key+desc 로 RT 확보. 캐시에 있으면 재사용, 없으면 allocator 로 생성.
		/// @return 비소유 핸들 (owner = 하위 ITargetAllocator = ResourceRegistry).
		virtual Framebuffer *Acquire(const std::string &key, const TargetDesc &desc) = 0;
	};

	/// @brief 영속(dedicated) RT 풀 - 한 번 생성하면 lifetime 동안 재사용 (transient 미지원).
	/// @details 하위 ITargetAllocator 에 생성을 위임. 재할당/리사이즈는 호출자(또는 Framebuffer::Resize) 책임.
	class DedicatedTargetPool : public IRenderTargetPool
	{
	  public:
		explicit DedicatedTargetPool(ITargetAllocator &alloc) : mAlloc(alloc) {}

		Framebuffer *Acquire(const std::string &key, const TargetDesc &desc) override;

	  private:
		ITargetAllocator &mAlloc;  ///< 생성 위임 대상 (비소유 참조 - ResourceRegistry 싱글톤).
	};
} // namespace SJH

#endif // __SJH_RENDER_TARGET_POOL_H__
```

- [ ] **Step 2: `.cpp` 구현**

```cpp
/**
 * @file render_target_pool.cpp
 * @brief DedicatedTargetPool::Acquire - Find??Create 위임.
 */
#include "<render>/render_target_pool.h"
#include "<buffer>/framebuffer.h"

namespace SJH
{
	Framebuffer *DedicatedTargetPool::Acquire(const std::string &key, const TargetDesc &desc)
	{
		// 캐시 hit 우선 (영속 정책) - 없을 때만 생성.
		if (Framebuffer *existing = mAlloc.FindFramebuffer(key))
			return existing;
		return mAlloc.CreateFramebuffer(key, desc.Width, desc.Height);
	}
} // namespace SJH
```

- [ ] **Step 3: CMake 배선** — `src/render/CMakeLists.txt` 의 `add_library(sjhopengl_render STATIC ...)` 목록에 `render_target_pool.cpp` 한 줄 추가. (의존: `SJH::buffer`(Framebuffer) PUBLIC, `SJH::resource_registry` 의 헤더만 — i_target_allocator.h 는 resource_registry 폴더 거주이므로 `render` 가 그 헤더에 접근하려면 include 경로 확인. ⚠️ render→resource_registry 의존은 D-1 push 로 *끊어둔* 상태(`CMakeLists.txt:19`)이므로, **헤더만** 필요하면 `i_target_allocator.h` 를 `src/render/` 로 둘지 검토 → DECISION 아래.)

> **🟦 DECISION 1.3 (Task-time): `i_target_allocator.h` 거주지**
> `render` 모듈은 의도적으로 `resource_registry` 의존을 끊어둠(D-1 push). Pool 이 그 헤더를 include 하면 의존이 되살아남.
>
> | 옵션 | 거주지 | 트레이드오프 |
> |---|---|---|
> | **A (추천)** | `i_target_allocator.h` 를 `src/buffer/` 또는 신규 위치에 두고 `render`+`resource_registry` 양쪽이 PUBLIC 의존 | 의존 방향 깨끗 (둘 다 추상에만 의존, DIP). 사이클 0 |
> | B | `render_target_pool` 을 `resource_registry` 모듈에 둠 | render→rr 의존 부활 회피하나 pool 이 render 개념인데 위치 어색 |
>
> **추천 A** — 추상 헤더를 공통 하위(`buffer` 또는 신규 `render_iface`)에 두면 `render`/`resource_registry` 모두 *추상에만* 의존(DIP), 구체 의존 부활 없음. 착수 시 `ModuleDeps` 그래프로 사이클 재확인 (memory `dependency_cycles_survey`).

- [ ] **Step 4~6: 빌드/실행/커밋 (사용자)**. 커밋 경로: `src/render/render_target_pool.{h,cpp}` + `src/render/CMakeLists.txt` (+옵션 A 면 헤더 이동분).

> **Phase 1 완료 기준**: 빌드 GREEN, GUI 무변화, 3 seam(`ICommandRecorder`/`ITargetAllocator`+`TargetDesc`/`IRenderTargetPool`) 존재. 아직 아무도 *인터페이스로* 호출 안 함.

---

## Phase 2 — Material RenderStateBlock 보유 + `IRenderStateProvider` [N+E, D7]

> 목표: ROP 저장처를 Material 로 이주 + Facade seam. 현재 GL state 는 draw 시점 `DefaultRenderStateBlockOf(material->GetPass())` 로 *매번 도출*(mesh_pass_processor.cpp:255) — MeshRenderer override 는 이미 폐기됨(mesh_renderer.h:29). 따라서 저장처만 옮기면 동작 동일.

### Task 2.1: `IRenderStateProvider` 인터페이스

**Files:** Create `src/material/i_render_state_provider.h`

- [ ] **Step 1: 헤더 생성**

```cpp
/**
 * @file i_render_state_provider.h
 * @brief RenderStateBlock(ROP) 접근의 Facade 인터페이스 - Material 과 IRenderable 이 둘 다 구현.
 *
 * @details
 *  D7 - RenderStateBlock 은 Material 이 *저장*, IRenderable(mesh)은 Material 로 *위임*.
 *  RenderableProcessor 는 구체 타입을 모른 채 GetRenderStateBlock() 으로 ROP 를 얻어 GL state 적용.
 *  D9 - efk/ImGui 처럼 ROP 를 못 꺼내는 외부 라이브러리 경로는 기본 RenderStateBlock{}(중립) 반환.
 */
#ifndef __SJH_I_RENDER_STATE_PROVIDER_H__
#define __SJH_I_RENDER_STATE_PROVIDER_H__

#include "material/pass.h"  // Pass::RenderStateBlock

namespace SJH
{
	/// @brief ROP(RenderStateBlock) 조회 Facade - Material(저장)/IRenderable(위임)이 구현.
	class IRenderStateProvider
	{
	  public:
		virtual ~IRenderStateProvider() = default;

		/// @brief 적용할 GL fixed-function 상태 묶음. 못 꺼내면 중립 기본값(D9).
		virtual const Pass::RenderStateBlock &GetRenderStateBlock() const = 0;
	};
} // namespace SJH

#endif // __SJH_I_RENDER_STATE_PROVIDER_H__
```

- [ ] **Step 2~4: 빌드/육안/커밋 (사용자)** — 헤더만이라 동작 0. 커밋: `src/material/i_render_state_provider.h`.

### Task 2.2: Material 이 RenderStateBlock 저장 + Facade 구현

**Files:** Modify `src/material/material.h`

- [ ] **Step 1: 상속 + 멤버 추가**

include 에 `#include "material/i_render_state_provider.h"` 추가. 선언:

```cpp
// 변경 후:
	class Material : public IRenderStateProvider
```

`private` 멤버 영역(`mPassKind` 근처, material.h:189)에 추가:

```cpp
		/// @brief PassKind 에서 seed 된 GL fixed-function 상태 (D7 저장처).
		/// @details SetPass(k) 가 DefaultRenderStateBlockOf(k) 로 seed. draw 시점 매번 도출하던
		///          mesh_pass_processor.cpp 의 DefaultRenderStateBlockOf(GetPass()) 호출을 본 멤버로 대체.
		Pass::RenderStateBlock mState = Pass::DefaultRenderStateBlockOf(Pass::RenderQueue::Opaque);
```

- [ ] **Step 2: `SetPass` 가 seed + `GetRenderStateBlock()` 구현**

`SetPass` (material.h:122) 본문을 seed 하도록 변경:

```cpp
		Material &SetPass(Pass::RenderQueue k)
		{
			mPassKind = k;
			mState    = Pass::DefaultRenderStateBlockOf(k);  // D7 - seed (저장처 = 본 멤버)
			return *this;
		}
```

public 영역에 Facade 구현 추가:

```cpp
		/// @brief D7 Facade - PassKind 에서 seed 된 RenderStateBlock 반환.
		const Pass::RenderStateBlock &GetRenderStateBlock() const override
		{
			return mState;
		}
```

> ⚠️ `CopyFrom` (material.h:182) 에 `mState = other.mState;` 한 줄 추가 (Clone 시 ROP 승계). 기본 생성 Material(`mPassKind=Opaque`)은 멤버 초기화로 이미 Opaque ROP seed 됨 — 정합.

- [ ] **Step 3: 소비처 1곳 교체 (의미 보존)**

`mesh_pass_processor.cpp:255` 의 도출을:

```cpp
// 변경 전:
const Pass::RenderStateBlock passState = Pass::DefaultRenderStateBlockOf(material->GetPass());
rc.ApplyRenderStateBlock(passState);
// 변경 후:
rc.ApplyRenderStateBlock(material->GetRenderStateBlock());
```

(ScreenQuad 경로 mesh_pass_processor.cpp:172 의 `DefaultRenderStateBlockOf(RenderQueue::Screen)` 은 Material 없는 blit 이라 *그대로 유지* — bypass/passthrough 는 material 의존 안 함.)

- [ ] **Step 4: 빌드 검증 (사용자)** — seed 가 기존 도출과 *값 동일*하므로 무회귀가 기대. 컴파일 출력 확인.
- [ ] **Step 5: 실행 육안 (사용자)** — Opaque/Transparent/Skybox/Outline/AlphaTest 객체가 직전과 동일하게 렌더되는지. (특히 healthbar 반투명·shadow 알파 — memory `blend-func-cache-default-mismatch` 회귀 주의.)
- [ ] **Step 6: Commit (사용자)**

```
git commit src/material/material.h <src>/render/mesh_pass_processor.cpp \
  -m "[refactor] Material RenderStateBlock 보유 + IRenderStateProvider Facade (D7)"
```

> **Phase 2 완료 기준**: 빌드 GREEN, GUI 무회귀, ROP 저장처 = Material. Processor 가 `material->GetRenderStateBlock()` 으로 소비.

---

## Phase 3 — `IRenderable` + Flat `RenderableProcessor` [E, D5/D8-파인]

> 목표: drawable 을 `IRenderable` 다형으로, `MeshPassProcessor`→`RenderableProcessor` 일반화. **여기서 가장 큰 설계 fork 가 발생** (아래 DECISION 3.3) — 현 Process 는 program-switch 배치 최적화를 *중앙집중* (mesh_pass_processor.cpp:218 lastProg/lastMat).

### Task 3.1: `IRenderable` 인터페이스

**Files:** Create `src/render/i_renderable.h`

- [ ] **Step 1: 헤더 생성** (LSP-최소 3메서드 — D5)

```cpp
/**
 * @file i_renderable.h
 * @brief Pass 안에서 함께 정렬되는 drawable 의 순수 추상 (mesh-family).
 *
 * @details
 *  D5 - "한 Pass 안에서 함께 정렬되는 것" = IRenderable. mesh-family(MeshRenderer, SpriteRenderer,
 *  글리프 Text)가 본진. Particle/ImGui 은 *역할 Pass* 라 IRenderable 안 섞음.
 *  D8-파인 - QueueLayer 가 Pass 내부 정렬 키. ROP 는 IRenderStateProvider(Facade) 로 위임.
 *  인터페이스는 *3개 고정* (LSP-최소) - 어떤 서브타입도 쓸데없는 메서드 stub 안 함.
 */
#ifndef __SJH_I_RENDERABLE_H__
#define __SJH_I_RENDERABLE_H__

#include "material/i_render_state_provider.h"

namespace SJH
{
	class ICommandRecorder;
}
namespace SJH::Scene
{
	class Camera;
}

namespace SJH
{
	/// @brief Pass 안에서 함께 정렬/발행되는 drawable. ROP 는 Facade 위임(IRenderStateProvider).
	class IRenderable : public IRenderStateProvider
	{
	  public:
		/// @brief 자기 자신을 발행 (per-draw). DECISION 3.3 의 옵션에 따라 호출 주체/범위가 결정됨.
		virtual void Render(ICommandRecorder &rec, const Scene::Camera &cam) const = 0;

		/// @brief Pass 내부 정렬 키 (작을수록 먼저). Material PassKind queue + QueueOffset 도출.
		virtual int QueueLayer() const = 0;
		// GetRenderStateBlock() <- IRenderStateProvider
	};
} // namespace SJH

#endif // __SJH_I_RENDERABLE_H__
```

- [ ] **Step 2~3: 빌드/커밋 (사용자)** — 헤더만, 동작 0. 커밋: `src/render/i_renderable.h`.

### Task 3.2: MeshRenderer 가 IRenderable 충족

**Files:** Modify `src/render/mesh_renderer.h`

- [ ] **Step 1: 상속 + Facade 위임 + QueueLayer**

include 에 `#include "render/i_renderable.h"` 추가. (Material/Mesh 전방선언 이미 존재.)

```cpp
// 변경 후 (mesh_renderer.h:51):
	class MeshRenderer : public Component, public IRenderable
```

public 영역에 구현 추가 (Material 로 ROP 위임 — D7):

```cpp
		/// @brief D7 Facade 위임 - Material 이 ROP 저장처. Material null 이면 중립 기본값(D9 안전).
		const Pass::RenderStateBlock &GetRenderStateBlock() const override
		{
			static const Pass::RenderStateBlock kNeutral{};
			return Material ? Material->GetRenderStateBlock() : kNeutral;
		}

		/// @brief 최종 queue = Material.PassKind queue + QueueOffset (mesh_renderer.h:18 모델).
		int QueueLayer() const override
		{
			return Material ? Pass::QueueOf(Material->GetPass(), QueueOffset) : QueueOffset;
		}
```

> ⚠️ `Material->GetRenderStateBlock()`/`GetPass()` 호출 위해 `mesh_renderer.h` 가 `material.h` full include 필요(현재 전방선언만, mesh_renderer.h:39). **inline 정의를 헤더에 두면** material.h include 가 필요 → `mesh_renderer.h` 에 `#include "material/material.h"` 추가. (clangd imgui cascade 거짓에러와 무관, 빌드 기준 — memory `clangd-imgui-cascade-false-errors`.)
>
> `Render(rec, cam)` 의 구현은 **DECISION 3.3** 에 종속 → 그 Task 에서 작성.

- [ ] **Step 2~4: 빌드/육안/커밋 (사용자)**. 이 시점 `Render` 가 아직 순수가상이면 MeshRenderer 는 추상 → 인스턴스화 불가. 따라서 **Task 3.2 와 3.3 은 한 커밋으로 묶어** `Render` 까지 구현해야 빌드 통과. (3.2 Step 들은 3.3 결정 후 함께 적용.)

### Task 3.3: `MeshPassProcessor` → `RenderableProcessor` 일반화 + Render 발행 주체 결정

> **🟥 DECISION 3.3 (Task-time, 이 plan 최대 분기) — `Render` 발행 주체와 배치 최적화 보존**
>
> 현 `MeshPassProcessor::Process`(mesh_pass_processor.cpp:132)는 *중앙집중 배치*:
> program 전환 시에만 FrameBlock(view/proj) UBO 업로드(:218), material 전환 시에만 BindSamplers(:235), per-draw MaterialBlock/DrawBlock UBO(:243,:259), ScreenQuad 별도 dispatch(:158). 레퍼런스 모델(`마이그레이팅계획안.md §0.5`)은 `for r: rec.ApplyRenderStateBlock(r->GetRenderStateBlock()); r->Render(rec,cam)` — *per-renderable 발행*.
>
> 두 모델은 충돌: 순진하게 `r->Render` 로 내리면 program-switch 배치(상태 전환 최소화)와 ScreenQuad 직교 경로를 잃어 **동작/성능 회귀 위험**.
>
> | 옵션 | Render 발행 주체 | 배치 최적화 | IRenderable 인터페이스 | 회귀 위험 | 비고 |
> |---|---|---|---|---|---|
> | **A (추천)** | `RenderableProcessor` 가 *기존 배치 Process 보존*. `IRenderable` 은 *정렬/ROP/식별* 만 제공하고, processor 가 내부에서 program/material 전환을 구동 (per-draw 잎만 `r->Render` 로 위임 가능하나, FrameBlock/sampler 전환은 processor 가 유지) | **보존** | 3메서드 + (processor 전용) Material/Program 질의 경로 | **낮음** | "동작 0" 가드 최우선. `r->Render` 잎 = (DrawBlock UBO + BindVAO + DrawIndexed) |
> | B | `IRenderable::Render` 가 *완전 자가 발행* (program/uniform/sampler/draw 전부) | **상실** (잎마다 재바인딩) | 3메서드 | **높음** | 가장 "순수"하나 batch 손실 + light/UBO 시퀀싱 재설계 필요 |
> | C | DrawCommand 유지, processor 는 IRenderable→DrawCommand 빌드만 다형화 | 보존 | 3메서드(+빌드 헬퍼) | 낮음 | A 의 변형 — DrawCommand 를 내부 구현 디테일로 유지 |
>
> **추천 A (또는 C)** — 본 마이그레이션의 핵심 불변식은 *픽셀/성능 회귀 0*(육안 검증 기반). program-switch 배치는 의도된 최적화(mesh_pass_processor.cpp:88 정렬 정책과 짝). 따라서 Phase 3 에서는 **`MeshPassProcessor` 의 배치 Process 를 그대로 보존**하고, 일반화는 (1) 컬렉션 원소 타입을 `IRenderable*` 로, (2) 정렬 키를 `r->QueueLayer()`/`r->GetRenderStateBlock()` 로 추상화하는 데 그친다. *완전 per-renderable 자가발행(B)* 은 Out of Scope (별도 후속 — batch/light 재설계 동반).
>
> 착수 시 사용자에게 A vs C 1택 질의 (둘 다 동작 보존, C 는 DrawCommand 를 internal 로 더 숨김).

**Files:** Modify (rename) `src/render/mesh_pass_processor.{h,cpp}` → 내용상 `RenderableProcessor`, `src/render/render_passable/render_passable.impls.{h,cpp}` (SceneRenderer 의 멤버 타입), `CMakeLists.txt`.

- [ ] **Step 1: 추천 A 기준 — Processor 클래스명/멤버 일반화 (배치 Process 보존)**

`MeshPassProcessor` → `RenderableProcessor` 개명 (클래스/파일/CMake/헤더가드/include). DrawCommand 는 *internal 구현 디테일*로 유지하되, 외부 Submit 경계를 IRenderable 중심으로:
- `Submit(IRenderable*)` 추가 (mesh-family). 내부에서 ROP/QueueLayer 를 `r->`로 질의.
- ScreenQuad(PassComponent) 경로는 PostFxPass 로 이관 예정(Phase 4)이므로 Phase 3 단계에선 *기존 DrawCommand ScreenQuad 경로 유지*.
- `SortMultiStage`/`Process` 본문은 **그대로** (program/material 배치 보존). 단 정렬·ROP 도출이 `material->GetPass()`/`DefaultRenderStateBlockOf` 대신 `r->QueueLayer()`/`r->GetRenderStateBlock()` 를 쓰도록 *값 동일* 치환.

> 구체 코드 hunk 는 DECISION 3.3 의 A/C 선택 직후 확정 (DrawCommand 노출 폭이 갈림). 정렬 함수의 `std::stable_sort` 는 **반드시 유지**(결정성 — mesh_pass_processor.cpp:113, memory 골든 결정성).

- [ ] **Step 2: MeshRenderer::Render 잎 구현 (옵션 A)** — per-draw 발행 잎만:

```cpp
		/// @brief 옵션 A 잎 - processor 가 program/sampler/UBO 전환을 끝낸 뒤 호출하는 per-draw 발행.
		/// @details DrawBlock(model) UBO + VAO + DrawIndexed 만. program/material 전환은 processor 책임.
		void Render(ICommandRecorder &rec, const Scene::Camera &cam) const override;
```

(정의는 `.cpp` — 단, 옵션 A 에서 processor 가 대부분을 들고 있으면 이 잎은 얇거나 미사용일 수 있음. A/C 확정 후 최종.)

- [ ] **Step 3: SceneRenderer 멤버 타입 갱신** — `render_passable.impls.h:90` `MeshPassProcessor mProcessor;` → `RenderableProcessor mProcessor;`. include 갱신.

- [ ] **Step 4: CMake** — `mesh_pass_processor.cpp` 파일명 변경 시 `git mv` + `src/render/CMakeLists.txt:3` 갱신.

- [ ] **Step 5: 빌드 검증 (사용자)** — 컴파일 출력 확인.
- [ ] **Step 6: 실행 육안 (사용자)** — mesh/sprite/text 출력·순서·반투명 모두 직전과 동일 (이 Phase 의 핵심 회귀 게이트).
- [ ] **Step 7: Commit (사용자)** — 경로: 변경된 render 파일 + mesh_renderer.h + impls + CMake.

```
git commit src/render/i_renderable.h src/render/mesh_renderer.h \
  src/render/renderable_processor.h src/render/renderable_processor.cpp \
  src/render/render_passable/render_passable.impls.h \
  src/render/render_passable/render_passable.impls.cpp src/render/CMakeLists.txt \
  -m "[refactor] IRenderable 다형 + RenderableProcessor 일반화 (D5, 배치 Process 보존)"
```

---

## Phase 4 — `IPassable` 계약 + 역할군 Pass 통합 [E⚠️, D2/D3/D4] — *한 Pass 씩 육안*

> 목표: 최상위 stage 추상을 *연쇄형*(Draw/GetPassResult/BeforeIndex)으로 교체하고 역할군 Pass 로 통합. **회귀 위험 최대 구간** — 반드시 한 Pass 전환마다 빌드+육안.
>
> ⚠️ 현 `IRenderPassable` 계약(render_passable.h:46)은 `Render(RenderTarget&)` + 외부 체이닝(`GetLastSceneOutput`/`SetSources`, main.cpp:362). 목표 계약은 `Draw(rec, before) / GetPassResult()→const Texture* / BeforeIndex`. 이는 *체이닝 패러다임 전환*이라 main 배선(Phase 5)과 짝.

### Task 4.1: `IPassable` 계약 정의 (개명 + 시그니처 교체)

**Files:** Modify `src/render/render_passable/render_passable.h` (+ 헤더가드/개명)

- [ ] **Step 1: 인터페이스 교체**

```cpp
// 변경 후 (render_passable.h):
	class Texture;        // fwd
	class ICommandRecorder;

	/// @brief 최상위 렌더 Pass 의 순수 추상 - 연쇄형(before -> 자기 출력).
	/// @details D2/D6 - Draw 가 ICommandRecorder 로 발행, before(입력 텍스처)를 받아 GetPassResult 로
	///          자기 출력(color attachment) 반환. BeforeIndex 가 입력 Pass 를 가리킴(-1 = sceneRaw).
	class IPassable
	{
	  public:
		virtual ~IPassable() = default;

		/// @brief 이 Pass 를 발행. @p before = 입력 Pass 의 결과 텍스처(없으면 무시 가능).
		virtual void Draw(ICommandRecorder &rec, const Texture *before) = 0;

		/// @brief 이 Pass 의 출력 텍스처(=Framebuffer color attachment). backbuffer-종단 Pass 는 nullptr 허용.
		virtual const Texture *GetPassResult() const = 0;

		/// @brief 창 resize broadcast. 내부 FBO sync. 기본 no-op.
		virtual void OnResize(int /*w*/, int /*h*/) {}

		/// @brief 입력으로 삼을 Pass 의 PassIterator vector 인덱스. -1 = scene raw(입력 없음).
		int BeforeIndex = -1;
	};
```

헤더가드 `__SJH_IRENDER_PASSABLE_H__` 유지 또는 `__SJH_IPASSABLE_H__` 로 개명(D3). `IRenderPassable` → `IPassable` 전 사용처 치환 (impls, main, ParticleStage, render_pipeline). 

> 📌 `Draw(rec, const Texture* before)` 로 정한 이유: `before` 가 없을 수 있는 Pass(Skybox/World 첫 단계)가 많아 `const Texture&`(마이그레이팅계획안 §4.3 의 초안)보다 `const Texture*`(nullable)가 안전. D6 `GetPassResult()→const Texture*` 와 대칭.

- [ ] **Step 2: 빌드 — 이 단계는 *전 사용처 동시 치환* 필요** (인터페이스 깨짐). 따라서 4.1 은 4.2~4.6 과 묶여 한 번에 컴파일 통과해야 함. → **Task 4.1 은 단독 커밋 불가**, 아래 역할군 Pass 들과 함께.

### Task 4.2~4.6: 역할군 Pass 구체화 (각각 IPassable) — *한 Pass 씩*

각 Pass 를 `IPassable` 로 만들고 `GetPassResult`(자기 FBO color attachment `.get()`, framebuffer.h:95) + `BeforeIndex` 배선. **한 Pass 전환마다 빌드+육안.**

- [ ] **4.2 `WorldPass : IPassable`** ← `SceneRenderer`+`CameraStage` 흡수. 내부 `RenderableProcessor mProc`. `Draw` 가 `BeginFrame(자기 RT)` + Actor 수집 + processor Process. `GetPassResult` = 자기 sceneFB color attachment. (현 `RenderWithCamera` 로직 이식.)
- [ ] **4.3 `SkyboxPass : IPassable`** ← 현 skybox queue. **🟦 DECISION 4.3**: 독립 Pass vs WorldPass 내부 skybox-queue IRenderable. *추천: 독립 Pass* (역할군 코스 가독성 — D4/D8). 단 현재 skybox 가 Material PassKind=Skybox(queue 2500)로 *같은 RenderableProcessor 안*에서 정렬되는 구조면, 분리 시 별 FBO/clear 비용 고려 → 착수 시 1택 질의.
- [ ] **4.4 `ParticlePass : IPassable`** ← `apps/_MyApp_/src/VFX/ParticleStage`. efk ref 는 *concrete private*. `Draw` 진입/이탈에 `rec.InvalidateStateCache()` (foreign-GL 경계 — VAO/EBO 오염 가드, memory `vao_ebo_thirdparty_corruption`). ROP 미추출 → D9 중립. `GetPassResult` = nullptr 또는 자기 합성 FBO.
- [ ] **4.5 `ImGuiPass : IPassable`** ← main.cpp:370 의 직접 `ImGui::Render()` 호출을 래핑. imgui draw data 는 concrete private. foreign-GL 경계 동일. (memory `imgui_layer_separation` 참조 — IImGuiLayer 설계와 정합 확인.)
- [ ] **4.6 `PostFxPass : IPassable`** ← `PassComponent`+`ScreenQuadStage` 통합. `Draw(rec, before)` 가 before(scene 출력)를 `uScene` 로 받아 자기 출력 FBO 에 blit. 종단이면 backbuffer 로. (현 ScreenQuadStage 의 `SetSources`/`GetLastSceneOutput` 외부 체이닝을 `before`/`GetPassResult` 내부 체이닝으로 대체.)

> 각 4.x Step 패턴: (1) 구체 클래스 IPassable 화 + Draw/GetPassResult 구현, (2) **사용자 빌드**, (3) **사용자 육안**(해당 Pass 산출이 직전과 동일), (4) **사용자 커밋**(path-scoped, 해당 Pass 파일만). Phase 4 전체는 5~6 커밋으로 쪼갬.

> **Phase 4 완료 기준**: 모든 stage 가 `IPassable`(Draw/GetPassResult/BeforeIndex). 외부 체이닝(`GetLastSceneOutput`/`SetSources`) 제거. GUI 완전 무회귀.

---

## Phase 5 — `PassIterator` 모듈화 + main 배선 + DebugPassIndex [E, D8-코스]

### Task 5.1: `PassIterator`

**Files:** Create `src/render/pass_iterator.h`, `.cpp`; Modify `src/render/CMakeLists.txt`

- [ ] **Step 1: 헤더 생성**

```cpp
/**
 * @file pass_iterator.h
 * @brief 역할군 Pass 들의 순서 보유 + 연쇄 실행 + 최종 Present + 디버그 토글.
 *
 * @details
 *  D8-코스 - vector<IPassable*> 의 순서가 "큰 그림 렌더 순서"(역할군). main 은 PassIterator 하나만 안다.
 *  각 Pass 의 BeforeIndex 로 입력 텍스처를 가져와 Draw(rec, before) 연쇄. DebugPassIndex 로
 *  특정 Pass 결과를 화면에 직접 띄우는 런타임 토글(개발 편의).
 */
#ifndef __SJH_PASS_ITERATOR_H__
#define __SJH_PASS_ITERATOR_H__

#include <vector>

namespace SJH
{
	class IPassable;
	class ICommandRecorder;
	class RenderTarget;

	/// @brief 역할군 Pass 컬렉션 실행기 - main 의 유일 렌더 진입점.
	class PassIterator
	{
	  public:
		/// @brief Pass 를 코스 순서 끝에 추가 (비소유). 반환 인덱스를 다른 Pass 의 BeforeIndex 로 사용.
		int Add(IPassable *pass);

		/// @brief 등록 순서대로 Draw 연쇄. 각 Pass 는 BeforeIndex 가 가리키는 Pass 의 GetPassResult 를 입력으로 받음.
		void Execute(ICommandRecorder &rec, RenderTarget &backbuffer);

		/// @brief 창 resize 를 모든 Pass 에 broadcast.
		void Resize(int w, int h);

		/// @brief 디버그 - 이 인덱스 Pass 의 결과만 화면에 출력 (-1 = 정상 연쇄).
		int DebugPassIndex = -1;

	  private:
		std::vector<IPassable *> mPasses;  ///< 코스 순서 (비소유 - owner = Application/bootstrap).
	};
} // namespace SJH

#endif // __SJH_PASS_ITERATOR_H__
```

- [ ] **Step 2: `.cpp` 구현** — `Execute` 가 `pass->BeforeIndex>=0 ? mPasses[idx]->GetPassResult() : nullptr` 를 before 로 `Draw` 호출. `DebugPassIndex>=0` 이면 해당 Pass 까지만/그 결과를 backbuffer 합성. (구체 blit 은 PostFxPass 재사용.)

- [ ] **Step 3: CMake 배선** (`pass_iterator.cpp`).
- [ ] **Step 4~6: 빌드/육안/커밋 (사용자)**.

### Task 5.2: main.cpp 배선 교체

**Files:** Modify `apps/_MyApp_/main.cpp` (mStages→PassIterator), `src/render_bootstrap/render_pipeline.{h,cpp}` (조립 반환)

- [ ] **Step 1**: `std::vector<std::unique_ptr<IRenderPassable>> mStages`(main.cpp:447) → `PassIterator mPassIterator` + Pass 소유 멤버들. `SetupDefaultPipeline`(render_pipeline.cpp:37) 이 PassIterator 를 조립해 반환하도록 확장.
- [ ] **Step 2**: render() 루프(main.cpp:365) `for(s:mStages) s->Render(...)` → `mPassIterator.Execute(DeviceContext::Get(), *mDefaultTarget);`. 외부 `SetActivePrograms`/`SetSources`/`GetLastSceneOutput` 배선(main.cpp:356-363) 제거 (Pass 내부 BeforeIndex 로 대체). `ImGui::Render()` 직접 호출(main.cpp:370) 제거 → ImGuiPass 가 PassIterator 안에서.
- [ ] **Step 3**: 코스 순서 = `[WorldPass(또는 CameraStage 들), SkyboxPass, ParticlePass, PostFxPass, ImGuiPass]` (현 main.cpp:193-198 insert 순서를 역할군 순서로 명시). 각 BeforeIndex 배선.
- [ ] **Step 4: 빌드 (사용자)**.
- [ ] **Step 5: 실행 육안 (사용자)** — 평소 화면 동일 + DebugPassIndex 토글 키로 Pass 별 결과 확인.
- [ ] **Step 6: Commit (사용자)** — `apps/_MyApp_/main.cpp` + `src/render_bootstrap/*` (ParticleStage 등 client 파일은 별 커밋).

> **Phase 5 완료 기준**: main 은 PassIterator 하나만 안다. 코스 순서가 역할군으로 명시. DebugPassIndex 동작.

---

## Phase 6 — 정리 / 네이밍 / 문서 [E]

- [ ] **Task 6.1**: 구 심볼/주석 정리 (`IRenderPassable`/`SceneRenderer`/`CameraStage`/`ScreenQuadStage`/`MeshPassProcessor` 잔재 grep=0). `[[deprecated]]` Render 경로(render_passable.impls.h:55) 제거.
- [ ] **Task 6.2**: `doc/diagrams/` 를 *현 엔진* 버전으로 갱신 (레퍼런스 다이어그램과 별도). `dot -Tsvg` 재렌더.
- [ ] **Task 6.3**: 메모리/CLAUDE.md 갱신 — 17 모듈 목록에 변경 반영, `rendering-migration-design` 메모리에 "구현 완료" change log.
- [ ] **Commit (사용자)**: 정리분 path-scoped.

---

## 검증 / Out of Scope

**검증(전부 사용자 주도)**:
- 빌드: `export PATH="$HOME/slang/bin:$PATH" && cmake --build --preset ninja --target _MyApp_` → 출력 붙이면 에이전트 육안.
- 실행: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`.
- Phase 2·3·4·5 는 **GUI 회귀 육안**(직전과 동일 출력)이 게이트. `std::stable_sort` 결정성 유지(골든 비교 가능).
- 회귀 핫스팟: 반투명 healthbar/shadow 알파(memory `blend-func-cache-default-mismatch`), Outline stencil 2-pass, Skybox depth, AlphaTest sprite cull, Effekseer/ImGui foreign-GL 경계(InvalidateStateCache).

**Out of Scope**: Shadow/Deferred/MSAA/HDR/Cubemap(미래 기능) · 실 GL 백엔드 신규(DeviceContext 가 이미) · `TextRenderable`/능력 인터페이스(다이아몬드) · data-driven JSON · 다중 BeforeIndex · **완전 per-renderable 자가발행(DECISION 3.3 옵션 B)** — batch/light 재설계 동반이라 별도 후속.

---

## Task-time DECISION 요약 (착수 시 사용자 1택 질의)

| # | 결정 | 추천 |
|---|---|---|
| 1.3 | `i_target_allocator.h` 거주지 (render↔rr 의존 부활 회피) | **A** — 추상 헤더를 공통 하위에, 양쪽 DIP |
| 3.3 | `IRenderable::Render` 발행 주체 / 배치 보존 | **A 또는 C** — 배치 Process 보존, 완전 자가발행(B)은 Out of Scope |
| 4.3 | SkyboxPass 독립 vs WorldPass skybox-queue | **독립 Pass** (역할군 코스 가독성) |
| (4.x) | depth 키 계산 위치 (QueueLayer 정수 vs within-layer depth) | Process 시점 계산 (카메라 필요) |

---

## Self-Review (writing-plans 체크리스트 결과)

- **Spec coverage**: D1(in-place)=전 Phase / D2·D3(IPassable 계약·개명)=Phase 4.1 / D4(역할군)=Phase 4.2-4.6 / D5(IRenderable mesh-family)=Phase 3 / D6(RenderTarget·GetPassResult→const Texture\*)=Phase 4.1 + framebuffer.h:95 / D7(Material ROP+Facade)=Phase 2 / D8(2레벨: 코스=PassIterator·파인=QueueLayer)=Phase 5+3 / D9(중립 default)=IRenderStateProvider 주석 + ParticlePass/ImGuiPass. **전 결정 매핑 확인.**
- **Placeholder scan**: 신규 인터페이스 헤더는 *완전 코드*. 수정은 정확한 before/after hunk + live `file:line`. 미확정 코드(3.3 의 Process hunk, 4.x 구체 이식)는 *placeholder 가 아니라 명시적 DECISION 종속*으로 표기 — 착수 시 1택 후 확정. (대규모 refactor 의 정직한 한계.)
- **Type consistency**: `Pass::RenderStateBlock`(네임스페이스 `SJH::Pass`, struct), `Pass::RenderQueue`(enum), `GetRenderStateBlock()`/`QueueLayer()`/`GetPassResult()`/`BeforeIndex`/`Acquire`/`TargetDesc` 명명이 전 Task 일관. `IRenderable` 3메서드(LSP) 고정.
- **Grounding**: 전 경로/시그니처를 2026-06-23 live 코드로 재측정(rename 커밋 68506a8 이후). 착수 시 재확인 경고 유지.
