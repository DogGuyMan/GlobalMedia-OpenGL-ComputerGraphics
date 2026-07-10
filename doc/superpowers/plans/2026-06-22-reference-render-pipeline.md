# Reference Render Pipeline Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 모범 엔진(Unity/Unreal/Godot/Cocos) 대조 연구([`RenderingSystemRefactorReserch.md`](../../RenderingSystemRefactorReserch.md))의 결론만으로, *어떤 기존 코드베이스에도 의존하지 않는* 독립 클린룸 렌더 파이프라인 레퍼런스를 빌드한다. (나중에 실제 프로젝트 구현체와 비교해 2차 적용 설계에 쓴다.)

**Architecture:** 렌더링을 **3 직교축**(`IRenderable` 대상 × `Camera` 시점 × `RenderTarget` 표면)으로 분해하고, **`IPassable`** 이 그 셋을 묶는 *결합자*가 된다. 패스 순서는 **Flat `std::vector<IPassable*>` + 수동 `BeforeIndex`**(연구 C6/C7 안 A), 출력 텍스처는 **`IRenderTargetPool` seam** 뒤에서 `DedicatedTargetPool`(각자 텍스처, 디버깅)으로 공급하고 `TransientTargetPool`(aliasing)을 같은 seam 으로 *비파괴 추가*해 진화 경로를 증명한다. GPU 는 연구 Q2(SRP)/§7 결론대로 **`ITargetAllocator`(자원 생성 = Device/RHI facet) + `ICommandRecorder`(명령 발행 = CommandList facet)** 두 좁은 인터페이스로 쪼개고, **`IRenderBackend`** 는 둘의 *우산*(`: ITargetAllocator, ICommandRecorder`)으로 둔다 — consumer 는 좁은 facet 에만 의존(Pool→`ITargetAllocator`, Pass/Renderable/Iterator→`ICommandRecorder`). 이 추상 덕에 *헤드리스로 단위 테스트* 가능 (실제 GL 없이 구조 검증 — 비교의 핵심).

**Tech Stack:** C++17, CMake (FetchContent 로 Catch2 v3), STL 전용(외부 그래픽 의존 0 — `Mat4 = std::array<float,16>`). 실제 GL 백엔드는 *범위 밖*(seam 만 제공).

---

## 핵심 어휘 (연구 §6 네이밍 확정 반영)

| 이름 | 역할 | 연구 근거 |
|---|---|---|
| `IRenderable` | 그려질 대상 (`LayerMask` + `Render`) | C5 종합 — 3축 中 "무엇을" |
| `Camera` | 시점 (view/proj + `cullMask` + `Sees`) | C5 — `View` 개명 철회, `Camera` 유지(4엔진 中 3) |
| `RenderTarget`(=`Texture`) | 표면 (목적지 텍스처) | C5 — Camera 와 분리 |
| `IPassable` | 그리는 단위(결합자) — `Draw(before)` / `GetPassResult` / `BeforeIndex` | C1 + C7 안 A |
| `PassIterator` | Flat vector 실행자 | C6 |
| `IRenderTargetPool` | 출력 RT 대여 seam | C4 |
| `DedicatedTargetPool` | 각자 텍스처(비-aliasing) — **지금** | C4 결정 |
| `TransientTargetPool` | aliasing 공유 — **진화 증명** | C4 결정 |
| `ITargetAllocator` | 자원 생성 Factory (CreateTarget/DestroyTarget) = Device/RHI facet | 연구 §7 / Q2 (Unreal `FDynamicRHI`) |
| `ICommandRecorder` | 명령 발행 (BindTarget/DrawMesh/Blit/Present) = CommandList facet | 연구 Q2 (Unreal `FRHICommandList`, Unity `CommandBuffer`) |
| `IRenderBackend` | 위 둘의 *우산* (`: ITargetAllocator, ICommandRecorder`). 구체 backend/`MockBackend` 가 구현 | 헤드리스 검증 |

> ⚠️ `Canvas` 명칭은 안 씀(Unity `Canvas`=UI 혼동, 연구 §6.2). `RenderTarget`/`Texture` 사용.
> ⚠️ SRP/ISP(연구 Q2): `IRenderBackend` 는 *자체 책임 0* (두 facet 의 합집합일 뿐). consumer 는 *좁은* facet 에 의존 — Pool 은 `ICommandRecorder` 를 모르고, Pass/Renderable 은 `CreateTarget` 을 모른다.

---

## 🔒 의존성 격리 불변식 — Graphics API 는 *구체 backend 만* 의존 (절대 잊지 말 것)

> **INVARIANT (불변식):**
> **Graphics API 라이브러리(OpenGL / Vulkan / Metal / D3D)에 `#include`·링크하는 모듈은 *구체 backend 클래스 단 하나* (`GlBackend`, `VulkanBackend` …) 뿐이다.**
> 나머지 *전 모듈* (`Camera` · `IRenderable`/`MeshRenderable` · `IRenderTargetPool`/`Dedicated`/`Transient` · `IPassable`/`WorldPass`/`PostFxPass` · `PassIterator`)은 **추상 인터페이스 + STL + `math.h` 만** 의존 — **`#include <GL/*>` / `-lGL` / `find_package(OpenGL)` 0건.**

| 모듈 | Graphics API(GL/VK/Metal) 의존? |
|---|---|
| **`GlBackend`(.cpp)** *(2차 / 본 plan Out-of-Scope)* | ✅ **유일** — `glXxx` 직접 호출. GL 헤더/링크가 *여기에만* 존재 |
| `MockBackend` (현재) | ❌ 0 — 호출 기록만 (헤드리스 증명용) |
| `Camera` / `IRenderable` / Pool 3종 / `IPassable` / Pass 2종 / `PassIterator` | ❌ 0 — *추상 인터페이스만* 의존 |

**왜 이 격리가 생명인가:**
1. **깨지는 순간 = Pass/Pool 어딘가에 `#include <GL/gl3w.h>` 가 새는 순간** → (a) 헤드리스 단위 테스트 불가(GL 컨텍스트·창 필요), (b) Vulkan/Metal 이식 시 그 모듈까지 재작성. **두 이점 동시 상실.**
2. **컴파일이 곧 증명** — `refrender` INTERFACE 라이브러리에 GL 의존을 *전혀 안 걸고* `MockBackend` 로 전 테스트가 빌드/통과한다 = "어떤 Pass/Pool 도 API 의존을 안 흘렸다"는 **구조적 보증**. 누가 GL include 를 넣으면 *테스트 빌드가 깨져 즉시 발각*.
3. **`modular-build-discipline`** — Graphics API 링크는 *구체 backend 모듈의 PRIVATE*. consumer 로 절대 전파 금지(transitive leak = 시한폭탄).
4. **API 분기 = 다형성, `#if` 아님** — GL/VK/Metal 차이는 `GlBackend`/`VulkanBackend`/… *별도 클래스*로 표현(Unreal RHI `FD3D12DynamicRHI`/`FVulkanDynamicRHI` 정통). `#if/#else` 는 "어느 backend 를 *생성*할지" 한 지점 + 플랫폼 헤더에만.

> 🧠 **기억 트리거**: *"이 줄에 `glXxx`(또는 `vkXxx`)를 쓰고 싶다 → 나는 지금 `GlBackend` 안에 있는가? 아니라면 잘못된 위치다."*

---

## File Structure

```
reference-render/
  CMakeLists.txt                      # 독립 프로젝트, Catch2 v3 FetchContent
  include/refrender/
    math.h            # Mat4 = array<float,16>, Identity(), Mul()
    texture.h         # TextureId, Texture, TargetDesc
    render_backend.h  # ITargetAllocator + ICommandRecorder + IRenderBackend(우산)
    camera.h          # Camera (view/proj + cullMask + Sees)
    renderable.h      # IRenderable + MeshRenderable
    render_target_pool.h  # IRenderTargetPool + DedicatedTargetPool + TransientTargetPool
    passable.h        # IPassable
    passes.h          # WorldPass, PostFxPass
    pass_iterator.h   # PassIterator (+ 디버그 오버레이)
  test/
    mock_backend.h    # MockBackend : IRenderBackend (호출 기록)
    test_camera.cpp
    test_pool.cpp
    test_pass_iterator.cpp
    test_passes.cpp
    test_debug_overlay.cpp
```

전부 헤더-온리(인라인) — 레퍼런스 규모에 적합 + 각 Task 가 자기완결. `.cpp` 는 테스트 파일과 `main` 없는 Catch2 자동 main 만.

---

## Task 0: 프로젝트 스캐폴드 (CMake + Catch2)

**Files:**
- Create: `reference-render/CMakeLists.txt`
- Create: `<reference-render>/include/refrender/math.h`
- Create: `<reference-render>/test/test_smoke.cpp`

- [ ] **Step 1: `math.h` 작성 (의존 0 수학)**

Create `<reference-render>/include/refrender/math.h`:
```cpp
#ifndef __REFRENDER_MATH_H__
#define __REFRENDER_MATH_H__
#include <array>
namespace refrender {
    using Mat4 = std::array<float, 16>;
    inline Mat4 Identity() {
        return {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    }
    // 열 우선 4x4 곱 (a * b). 레퍼런스용 — 정확성보다 구조 검증 목적.
    inline Mat4 Mul(const Mat4& a, const Mat4& b) {
        Mat4 r{};
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row) {
                float s = 0.0f;
                for (int k = 0; k < 4; ++k) s += a[k * 4 + row] * b[c * 4 + k];
                r[c * 4 + row] = s;
            }
        return r;
    }
}
#endif
```

- [ ] **Step 2: `CMakeLists.txt` 작성**

Create `reference-render/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.16)
project(reference_render CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

add_library(refrender INTERFACE)
target_include_directories(refrender INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(refrender INTERFACE cxx_std_17)

enable_testing()
file(GLOB REF_TESTS ${CMAKE_CURRENT_SOURCE_DIR}/test/test_*.cpp)
add_executable(refrender_tests ${REF_TESTS})
target_link_libraries(refrender_tests PRIVATE refrender Catch2::Catch2WithMain)
target_include_directories(refrender_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test)

include(CTest)
include(Catch)
catch_discover_tests(refrender_tests)
```

- [ ] **Step 3: smoke 테스트 작성**

Create `<reference-render>/test/test_smoke.cpp`:
```cpp
#include <<catch2>/catch_test_macros.hpp>
#include "<refrender>/math.h"
using namespace refrender;

TEST_CASE("identity matrix diagonal is 1") {
    Mat4 m = Identity();
    REQUIRE(m[0] == 1.0f);
    REQUIRE(m[5] == 1.0f);
    REQUIRE(m[10] == 1.0f);
    REQUIRE(m[15] == 1.0f);
}
TEST_CASE("identity is multiplicative neutral") {
    Mat4 a = {2,0,0,0, 0,3,0,0, 0,0,4,0, 5,6,7,1};
    REQUIRE(Mul(a, Identity()) == a);
    REQUIRE(Mul(Identity(), a) == a);
}
```

- [ ] **Step 4: 구성 + 빌드 + 테스트 실패→통과 확인**

Run:
```bash
cd reference-render && cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
```
Expected: 빌드 성공, 2 테스트 PASS.

- [ ] **Step 5: Commit**
```bash
git add reference-render/CMakeLists.txt <reference-render>/include/refrender/math.h <reference-render>/test/test_smoke.cpp
git commit -m "chore(refrender): scaffold standalone reference project with Catch2"
```

---

## Task 1: Texture / TargetDesc 값 타입

**Files:**
- Create: `src/texture/texture.h`
- Test: `<reference-render>/test/test_texture.cpp`

- [ ] **Step 1: 실패 테스트 작성**

Create `<reference-render>/test/test_texture.cpp`:
```cpp
#include <<catch2>/catch_test_macros.hpp>
#include "src/texture/texture.h"
using namespace refrender;

TEST_CASE("invalid texture has id 0") {
    Texture t;
    REQUIRE(t.id == kInvalidTexture);
}
TEST_CASE("TargetDesc equality compares width and height") {
    REQUIRE(TargetDesc{1920,1080} == TargetDesc{1920,1080});
    REQUIRE_FALSE(TargetDesc{1920,1080} == TargetDesc{1280,720});
}
```

- [ ] **Step 2: 실패 확인**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R texture --output-on-failure`
Expected: FAIL — `texture.h` not found.

- [ ] **Step 3: 구현**

Create `src/texture/texture.h`:
```cpp
#ifndef __REFRENDER_TEXTURE_H__
#define __REFRENDER_TEXTURE_H__
#include <cstdint>
namespace refrender {
    using TextureId = std::uint32_t;
    constexpr TextureId kInvalidTexture = 0;

    // GPU 텍스처를 가리키는 불투명 핸들 (연구 §C5 "RID 란?" — GLuint/VkImage 동격).
    struct Texture {
        TextureId id = kInvalidTexture;
        int width = 0;
        int height = 0;
    };

    // 렌더 타깃 할당 명세 (풀이 같은 desc 끼리 재사용 판단에 사용).
    struct TargetDesc {
        int width = 0;
        int height = 0;
        bool operator==(const TargetDesc& o) const {
            return width == o.width && height == o.height;
        }
    };
}
#endif
```

- [ ] **Step 4: 통과 확인**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R texture --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit**
```bash
git add src/texture/texture.h <reference-render>/test/test_texture.cpp
git commit -m "feat(refrender): Texture handle + TargetDesc value types"
```

---

## Task 2: IRenderBackend + MockBackend (테스트 더블)

**Files:**
- Create: `<reference-render>/include/refrender/render_backend.h`
- Create: `<reference-render>/test/mock_backend.h`
- Test: `<reference-render>/test/test_backend.cpp`

- [ ] **Step 1: 실패 테스트 작성**

Create `<reference-render>/test/test_backend.cpp`:
```cpp
#include <<catch2>/catch_test_macros.hpp>
#include "mock_backend.h"
using namespace refrender;

TEST_CASE("MockBackend hands out distinct texture ids") {
    MockBackend be;
    Texture a = be.CreateTarget({64,64});
    Texture b = be.CreateTarget({64,64});
    REQUIRE(a.id != kInvalidTexture);
    REQUIRE(a.id != b.id);
    REQUIRE(be.createCount == 2);
}
TEST_CASE("MockBackend records draws against bound target") {
    MockBackend be;
    Texture t = be.CreateTarget({64,64});
    be.BindTarget(t);
    be.DrawMesh(7, Identity());
    REQUIRE(be.draws.size() == 1);
    REQUIRE(be.draws[0].target == t.id);
    REQUIRE(be.draws[0].meshTag == 7);
}
TEST_CASE("MockBackend records blit and present") {
    MockBackend be;
    Texture s = be.CreateTarget({64,64});
    Texture d = be.CreateTarget({64,64});
    be.Blit(s, d);
    be.Present(d);
    REQUIRE(be.blits.size() == 1);
    REQUIRE(be.blits[0].first == s.id);
    REQUIRE(be.blits[0].second == d.id);
    REQUIRE(be.presented == d.id);
}
TEST_CASE("MockBackend satisfies both narrow facets (ISP)") {
    MockBackend be;
    ITargetAllocator& alloc = be;   // 자원 생성 facet (Pool 이 보는 타입)
    ICommandRecorder&  rec   = be;   // 명령 발행 facet (Pass/Renderable 이 보는 타입)
    Texture t = alloc.CreateTarget({8,8});
    rec.Present(t);
    REQUIRE(be.presented == t.id);
}
```

- [ ] **Step 2: 실패 확인**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R backend --output-on-failure`
Expected: FAIL — `render_backend.h` / `mock_backend.h` not found.

- [ ] **Step 3: 인터페이스 구현**

Create `<reference-render>/include/refrender/render_backend.h`:
```cpp
#ifndef __REFRENDER_RENDER_BACKEND_H__
#define __REFRENDER_RENDER_BACKEND_H__
#include "src/texture/texture.h"
#include "<refrender>/math.h"
namespace refrender {
    // ── 자원 생성 facet (= Device/RHI). 연구 §7: ITargetAllocator Factory 추상.
    //    Unreal FDynamicRHI(RHICreateTexture) / Cocos backend::Device / Unity device 대응.
    class ITargetAllocator {
    public:
        virtual ~ITargetAllocator() = default;
        virtual Texture CreateTarget(const TargetDesc& desc) = 0;  // 렌더 타깃 텍스처 생성
        virtual void    DestroyTarget(Texture t) = 0;
    };

    // ── 명령 발행 facet (= CommandList/CommandBuffer). 연구 Q2: SRP 분리.
    //    Unreal FRHICommandList / Unity CommandBuffer / Cocos backend::CommandBuffer 대응.
    class ICommandRecorder {
    public:
        virtual ~ICommandRecorder() = default;
        virtual void BindTarget(Texture t) = 0;                    // 이후 DrawMesh 의 목적지
        virtual void DrawMesh(std::uint32_t meshTag, const Mat4& mvp) = 0;
        virtual void Blit(Texture src, Texture dst) = 0;           // src -> dst 복사
        virtual void Present(Texture src) = 0;                     // src -> 백버퍼/화면
    };

    // ── GPU 추상 우산 = 두 facet 의 합집합 (구체 backend 는 둘 다 구현). 자체 책임 0.
    //    consumer 는 좁은 facet 에 의존: Pool -> ITargetAllocator, Pass/Renderable/Iterator -> ICommandRecorder.
    class IRenderBackend : public ITargetAllocator, public ICommandRecorder {};
}
#endif
```

- [ ] **Step 4: MockBackend 구현**

Create `<reference-render>/test/mock_backend.h`:
```cpp
#ifndef __REFRENDER_TEST_MOCK_BACKEND_H__
#define __REFRENDER_TEST_MOCK_BACKEND_H__
#include "<refrender>/render_backend.h"
#include <vector>
#include <utility>
namespace refrender {
    // 호출을 전부 기록하는 테스트 더블. GL 없이 구조(순서/타깃/큐)를 검증.
    class MockBackend : public IRenderBackend {
    public:
        struct DrawRecord { TextureId target; std::uint32_t meshTag; };

        int createCount = 0;
        int destroyCount = 0;
        TextureId boundTarget = kInvalidTexture;
        TextureId presented = kInvalidTexture;
        std::vector<DrawRecord> draws;
        std::vector<std::pair<TextureId, TextureId>> blits;

        Texture CreateTarget(const TargetDesc& desc) override {
            ++createCount;
            return Texture{ mNextId++, desc.width, desc.height };
        }
        void DestroyTarget(Texture) override { ++destroyCount; }
        void BindTarget(Texture t) override { boundTarget = t.id; }
        void DrawMesh(std::uint32_t meshTag, const Mat4&) override {
            draws.push_back({ boundTarget, meshTag });
        }
        void Blit(Texture src, Texture dst) override {
            blits.emplace_back(src.id, dst.id);
        }
        void Present(Texture src) override { presented = src.id; }
    private:
        TextureId mNextId = 1;  // 0 = kInvalidTexture 예약
    };
}
#endif
```

- [ ] **Step 5: 통과 확인 + Commit**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R backend --output-on-failure`
Expected: PASS.
```bash
git add <reference-render>/include/refrender/render_backend.h <reference-render>/test/mock_backend.h <reference-render>/test/test_backend.cpp
git commit -m "feat(refrender): IRenderBackend abstraction + recording MockBackend"
```

---

## Task 3: Camera (시점 + cullMask) — 3축 中 "어느 눈으로"

연구 C2 축 A: cullMask 는 **시점(Camera)** 속성. C5: `View` 개명 철회, `Camera` 유지.

**Files:**
- Create: `src/scene/camera.h`
- Test: `<reference-render>/test/test_camera.cpp`

- [ ] **Step 1: 실패 테스트 작성**

Create `<reference-render>/test/test_camera.cpp`:
```cpp
#include <<catch2>/catch_test_macros.hpp>
#include "src/scene/camera.h"
using namespace refrender;

TEST_CASE("camera with full mask sees every layer") {
    Camera cam;  // 기본 cullMask = 전체
    REQUIRE(cam.Sees(0x1));
    REQUIRE(cam.Sees(0x80000000u));
}
TEST_CASE("camera sees only layers in its cull mask (bit AND)") {
    Camera cam;
    cam.cullMask = 0b0101;          // 레이어 0,2 만
    REQUIRE(cam.Sees(0b0001));      // 레이어0 → 보임
    REQUIRE(cam.Sees(0b0100));      // 레이어2 → 보임
    REQUIRE_FALSE(cam.Sees(0b0010));// 레이어1 → 안 보임
    REQUIRE(cam.Sees(0b0110));      // 레이어1+2 → 교집합 있음 → 보임
}
```

- [ ] **Step 2: 실패 확인**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R camera --output-on-failure`
Expected: FAIL — `camera.h` not found.

- [ ] **Step 3: 구현**

Create `src/scene/camera.h`:
```cpp
#ifndef __REFRENDER_CAMERA_H__
#define __REFRENDER_CAMERA_H__
#include "<refrender>/math.h"
#include <cstdint>
namespace refrender {
    // 시점 — "무엇을, 어느 각도로". Target(표면)과 분리(연구 C5). cullMask 귀속(연구 C2 축 A).
    struct Camera {
        Mat4 view = Identity();
        Mat4 proj = Identity();
        std::uint32_t cullMask = 0xFFFFFFFFu;   // 어떤 레이어를 그릴지 (시점 쪽 필터)

        Mat4 ViewProj() const { return Mul(proj, view); }
        // 대상의 layerMask 와 교집합이 있으면 이 카메라가 그린다.
        bool Sees(std::uint32_t layerMask) const { return (layerMask & cullMask) != 0u; }
    };
}
#endif
```

- [ ] **Step 4: 통과 확인 + Commit**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R camera --output-on-failure`
Expected: PASS.
```bash
git add src/scene/camera.h <reference-render>/test/test_camera.cpp
git commit -m "feat(refrender): Camera (viewpoint + cullMask Sees) — axis 'which eye'"
```

---

## Task 4: IRenderable + MeshRenderable — 3축 中 "무엇을"

**Files:**
- Create: `<reference-render>/include/refrender/renderable.h`
- Test: `<reference-render>/test/test_renderable.cpp`

- [ ] **Step 1: 실패 테스트 작성**

Create `<reference-render>/test/test_renderable.cpp`:
```cpp
#include <<catch2>/catch_test_macros.hpp>
#include "<refrender>/renderable.h"
#include "mock_backend.h"
using namespace refrender;

TEST_CASE("mesh renderable exposes its layer mask") {
    MeshRenderable m{ /*meshTag*/3, /*layerMask*/0b0010, Identity() };
    REQUIRE(m.LayerMask() == 0b0010u);
}
TEST_CASE("mesh renderable issues one DrawMesh with its tag") {
    MockBackend be;
    Camera cam;
    MeshRenderable m{ 42, 0b0001, Identity() };
    m.Render(be, cam);
    REQUIRE(be.draws.size() == 1);
    REQUIRE(be.draws[0].meshTag == 42);
}
```

- [ ] **Step 2: 실패 확인**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R renderable --output-on-failure`
Expected: FAIL — `renderable.h` not found.

- [ ] **Step 3: 구현**

Create `<reference-render>/include/refrender/renderable.h`:
```cpp
#ifndef __REFRENDER_RENDERABLE_H__
#define __REFRENDER_RENDERABLE_H__
#include "<refrender>/render_backend.h"
#include "src/scene/camera.h"
#include <cstdint>
namespace refrender {
    // 그려질 대상 — "무엇을". 자기 layer 와 자기 그리기만 안다 (target 바인딩은 Pass 책임).
    class IRenderable {
    public:
        virtual ~IRenderable() = default;
        virtual std::uint32_t LayerMask() const = 0;
        virtual void Render(ICommandRecorder& rec, const Camera& cam) const = 0;  // 명령 발행 facet 만 의존
    };

    // 가장 단순한 구현 — meshTag 하나를 현재 바인딩된 타깃에 그린다.
    class MeshRenderable : public IRenderable {
    public:
        MeshRenderable(std::uint32_t meshTag, std::uint32_t layerMask, Mat4 model)
            : mMeshTag(meshTag), mLayerMask(layerMask), mModel(model) {}

        std::uint32_t LayerMask() const override { return mLayerMask; }
        void Render(ICommandRecorder& rec, const Camera& cam) const override {
            rec.DrawMesh(mMeshTag, Mul(cam.ViewProj(), mModel));
        }
    private:
        std::uint32_t mMeshTag;
        std::uint32_t mLayerMask;
        Mat4 mModel;
    };
}
#endif
```

- [ ] **Step 4: 통과 확인 + Commit**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R renderable --output-on-failure`
Expected: PASS.
```bash
git add <reference-render>/include/refrender/renderable.h <reference-render>/test/test_renderable.cpp
git commit -m "feat(refrender): IRenderable + MeshRenderable — axis 'what to draw'"
```

---

## Task 5: IRenderTargetPool + DedicatedTargetPool (seam, 지금 구현)

연구 C4 결정: Pass 는 출력 텍스처를 풀에서 *대여*. `DedicatedTargetPool` = 키마다 전용 텍스처, `Release` 는 no-op(프레임 끝까지 생존 → 디버깅).

**Files:**
- Create: `<reference-render>/include/refrender/render_target_pool.h`
- Test: `<reference-render>/test/test_pool.cpp`

- [ ] **Step 1: 실패 테스트 작성 (Dedicated 부분만)**

Create `<reference-render>/test/test_pool.cpp`:
```cpp
#include <<catch2>/catch_test_macros.hpp>
#include "<refrender>/render_target_pool.h"
#include "mock_backend.h"
using namespace refrender;

TEST_CASE("DedicatedTargetPool: same key returns same texture (created once)") {
    MockBackend be;
    DedicatedTargetPool pool(be);
    Texture a = pool.Acquire("World", {64,64});
    Texture b = pool.Acquire("World", {64,64});
    REQUIRE(a.id == b.id);
    REQUIRE(be.createCount == 1);
}
TEST_CASE("DedicatedTargetPool: different keys get distinct textures") {
    MockBackend be;
    DedicatedTargetPool pool(be);
    Texture w = pool.Acquire("World", {64,64});
    Texture s = pool.Acquire("Skybox", {64,64});
    REQUIRE(w.id != s.id);
    REQUIRE(be.createCount == 2);
}
TEST_CASE("DedicatedTargetPool: Release is a no-op (texture stays alive for inspection)") {
    MockBackend be;
    DedicatedTargetPool pool(be);
    Texture a = pool.Acquire("World", {64,64});
    pool.Release("World");
    Texture b = pool.Acquire("World", {64,64});   // 여전히 같은 것 — 안 버림
    REQUIRE(a.id == b.id);
    REQUIRE(be.createCount == 1);
    REQUIRE(be.destroyCount == 0);
}
```

- [ ] **Step 2: 실패 확인**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R pool --output-on-failure`
Expected: FAIL — `render_target_pool.h` not found.

- [ ] **Step 3: 구현 (인터페이스 + Dedicated; Transient 는 Task 9)**

Create `<reference-render>/include/refrender/render_target_pool.h`:
```cpp
#ifndef __REFRENDER_RENDER_TARGET_POOL_H__
#define __REFRENDER_RENDER_TARGET_POOL_H__
#include "<refrender>/render_backend.h"
#include <string>
#include <unordered_map>
namespace refrender {
    // Pass 에게 출력 RT 를 대여하는 seam (연구 C4). Pass 코드는 구현을 모른다.
    class IRenderTargetPool {
    public:
        virtual ~IRenderTargetPool() = default;
        virtual Texture Acquire(const std::string& key, const TargetDesc& desc) = 0;
        virtual void    Release(const std::string& key) = 0;
    };

    // 각자 텍스처 — 키마다 전용, Release no-op (프레임 끝까지 생존 → 시각 디버깅).
    // Unreal r.RDG.TransientAllocator OFF 에 대응 (연구 C4 결정).
    // 의존: 좁은 ITargetAllocator 만 (ICommandRecorder 모름 — ISP, 연구 Q2).
    class DedicatedTargetPool : public IRenderTargetPool {
    public:
        explicit DedicatedTargetPool(ITargetAllocator& alloc) : mAlloc(alloc) {}

        Texture Acquire(const std::string& key, const TargetDesc& desc) override {
            auto it = mByKey.find(key);
            if (it != mByKey.end()) return it->second;
            Texture t = mAlloc.CreateTarget(desc);
            mByKey.emplace(key, t);
            return t;
        }
        void Release(const std::string&) override { /* no-op — 계속 소유 */ }

    private:
        ITargetAllocator& mAlloc;   // 좁은 Factory facet 만 의존 (ISP)
        std::unordered_map<std::string, Texture> mByKey;
    };
}
#endif
```

- [ ] **Step 4: 통과 확인 + Commit**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R pool --output-on-failure`
Expected: PASS (3 케이스).
```bash
git add <reference-render>/include/refrender/render_target_pool.h <reference-render>/test/test_pool.cpp
git commit -m "feat(refrender): IRenderTargetPool seam + DedicatedTargetPool (each-own, debug)"
```

---

## Task 6: IPassable + WorldPass — 3축 결합자

연구 C5 종합: Pass = (IRenderable[] × Camera × Target) 결합자. C7 안 A: `Draw(before)` + `BeforeIndex`. cullMask 가 IRenderable 선택자.

**Files:**
- Create: `<reference-render>/include/refrender/passable.h`
- Create: `<reference-render>/include/refrender/passes.h` (WorldPass 부분)
- Test: `<reference-render>/test/test_passes.cpp` (WorldPass 부분)

- [ ] **Step 1: 실패 테스트 작성 (WorldPass)**

Create `<reference-render>/test/test_passes.cpp`:
```cpp
#include <<catch2>/catch_test_macros.hpp>
#include "<refrender>/passes.h"
#include "<refrender>/render_target_pool.h"
#include "mock_backend.h"
using namespace refrender;

TEST_CASE("WorldPass draws only renderables the camera sees (cullMask filter)") {
    MockBackend be;
    DedicatedTargetPool pool(be);
    Camera cam; cam.cullMask = 0b0001;          // 레이어0 만

    MeshRenderable visible{ 1, 0b0001, Identity() };   // 보임
    MeshRenderable hidden { 2, 0b0010, Identity() };   // 안 보임
    WorldPass world(cam, &pool, {64,64});
    world.Add(&visible);
    world.Add(&hidden);

    world.Draw(be, Texture{});                  // World 는 before 무시 (BeforeIndex=-1)

    REQUIRE(be.draws.size() == 1);
    REQUIRE(be.draws[0].meshTag == 1);          // hidden 은 안 그려짐
    REQUIRE(world.GetPassResult().id != kInvalidTexture);
    REQUIRE(be.draws[0].target == world.GetPassResult().id);  // 자기 타깃에 그림
}
TEST_CASE("WorldPass default BeforeIndex is -1 (source pass)") {
    MockBackend be; DedicatedTargetPool pool(be); Camera cam;
    WorldPass world(cam, &pool, {64,64});
    REQUIRE(world.BeforeIndex == -1);
}
```

- [ ] **Step 2: 실패 확인**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R passes --output-on-failure`
Expected: FAIL — `passable.h` / `passes.h` not found.

- [ ] **Step 3: IPassable 인터페이스 구현**

Create `<reference-render>/include/refrender/passable.h`:
```cpp
#ifndef __REFRENDER_PASSABLE_H__
#define __REFRENDER_PASSABLE_H__
#include "<refrender>/render_backend.h"
#include <string>
namespace refrender {
    // 그리는 단위 = 3직교축(IRenderable x Camera x Target)을 묶는 결합자 (연구 C5 종합).
    // 완전 함수형 1-in-1-out: before 텍스처를 읽어 자기 출력 텍스처에 쓴다 (연구 C4/C7 안 A).
    class IPassable {
    public:
        virtual ~IPassable() = default;
        virtual void    Draw(ICommandRecorder& rec, const Texture& before) = 0;  // 명령 발행 facet 만 의존
        virtual Texture GetPassResult() const = 0;
        virtual std::string Key() const = 0;   // 풀 키 + 디버그 라벨

        int BeforeIndex = -1;   // 입력을 가져올 앞 Pass 의 vector 인덱스. -1 = scene raw (연구 C6 안 A)
    };
}
#endif
```

- [ ] **Step 4: WorldPass 구현**

Create `<reference-render>/include/refrender/passes.h`:
```cpp
#ifndef __REFRENDER_PASSES_H__
#define __REFRENDER_PASSES_H__
#include "<refrender>/passable.h"
#include "<refrender>/render_target_pool.h"
#include "<refrender>/renderable.h"
#include "src/scene/camera.h"
#include <vector>
#include <string>
namespace refrender {
    // 씬 메시들을 한 Camera 로 보고 자기 출력 타깃에 그리는 패스. before 무시(source).
    class WorldPass : public IPassable {
    public:
        WorldPass(Camera cam, IRenderTargetPool* pool, TargetDesc desc, std::string key = "World")
            : mCamera(cam), mPool(pool), mDesc(desc), mKey(std::move(key)) {}

        void Add(IRenderable* r) { mRenderables.push_back(r); }

        void Draw(ICommandRecorder& rec, const Texture& /*before*/) override {
            mOutput = mPool->Acquire(mKey, mDesc);   // 풀(=ITargetAllocator 보유)에서 출력 RT 대여
            rec.BindTarget(mOutput);
            for (IRenderable* r : mRenderables)
                if (mCamera.Sees(r->LayerMask()))     // cullMask = 시점이 대상 선택 (연구 C2 축 A)
                    r->Render(rec, mCamera);
        }
        Texture GetPassResult() const override { return mOutput; }
        std::string Key() const override { return mKey; }

    private:
        Camera mCamera;
        IRenderTargetPool* mPool;
        TargetDesc mDesc;
        std::string mKey;
        std::vector<IRenderable*> mRenderables;   // IRenderable 풀 (비소유 참조)
        Texture mOutput;
    };
}
#endif
```

- [ ] **Step 5: 통과 확인 + Commit**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R passes --output-on-failure`
Expected: PASS.
```bash
git add <reference-render>/include/refrender/passable.h <reference-render>/include/refrender/passes.h <reference-render>/test/test_passes.cpp
git commit -m "feat(refrender): IPassable combinator + WorldPass (Camera x IRenderable[] x Target)"
```

---

## Task 7: PostFxPass (1-in-1-out, before → 자기 출력)

PostFx 는 화면 quad 1장을 입력 텍스처(before)로 효과 적용. 레퍼런스에선 효과 = `Blit(before, output)`(실제는 셰이더).

**Files:**
- Modify: `<reference-render>/include/refrender/passes.h` (PostFxPass 추가)
- Modify: `<reference-render>/test/test_passes.cpp` (PostFxPass 테스트 추가)

- [ ] **Step 1: 실패 테스트 추가**

Append to `<reference-render>/test/test_passes.cpp`:
```cpp
TEST_CASE("PostFxPass reads 'before' and writes its own output via blit") {
    MockBackend be;
    DedicatedTargetPool pool(be);
    Texture sceneColor = be.CreateTarget({64,64});   // 가짜 입력

    PostFxPass bloom(&pool, {64,64}, "Bloom");
    bloom.Draw(be, sceneColor);

    REQUIRE(bloom.GetPassResult().id != kInvalidTexture);
    REQUIRE(bloom.GetPassResult().id != sceneColor.id);     // 자기 텍스처에 씀
    REQUIRE(be.blits.size() == 1);
    REQUIRE(be.blits[0].first == sceneColor.id);            // before 를 읽고
    REQUIRE(be.blits[0].second == bloom.GetPassResult().id);// 자기 출력에 씀
}
```

- [ ] **Step 2: 실패 확인**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R passes --output-on-failure`
Expected: FAIL — `PostFxPass` not declared.

- [ ] **Step 3: PostFxPass 구현 (passes.h 의 `}` namespace 닫기 전에 추가)**

In `<reference-render>/include/refrender/passes.h`, add before the closing `}` of `namespace refrender`:
```cpp
    // 화면 quad 1장에 입력(before)으로 효과 적용 → 자기 출력 (1-in-1-out). 효과 = blit(레퍼런스).
    class PostFxPass : public IPassable {
    public:
        PostFxPass(IRenderTargetPool* pool, TargetDesc desc, std::string key)
            : mPool(pool), mDesc(desc), mKey(std::move(key)) {}

        void Draw(ICommandRecorder& rec, const Texture& before) override {
            mOutput = mPool->Acquire(mKey, mDesc);
            rec.Blit(before, mOutput);   // 실제 엔진: before 를 샘플링하는 풀스크린 셰이더
        }
        Texture GetPassResult() const override { return mOutput; }
        std::string Key() const override { return mKey; }

    private:
        IRenderTargetPool* mPool;
        TargetDesc mDesc;
        std::string mKey;
        Texture mOutput;
    };
```

- [ ] **Step 4: 통과 확인 + Commit**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R passes --output-on-failure`
Expected: PASS.
```bash
git add <reference-render>/include/refrender/passes.h <reference-render>/test/test_passes.cpp
git commit -m "feat(refrender): PostFxPass (1-in-1-out, before -> own output)"
```

---

## Task 8: PassIterator — Flat vector + BeforeIndex 해석 (연구 C6 안 A)

**Files:**
- Create: `src/render/pass_iterator.h`
- Test: `<reference-render>/test/test_pass_iterator.cpp`

- [ ] **Step 1: 실패 테스트 작성**

Create `<reference-render>/test/test_pass_iterator.cpp`:
```cpp
#include <<catch2>/catch_test_macros.hpp>
#include "src/render/pass_iterator.h"
#include "<refrender>/passes.h"
#include "<refrender>/render_target_pool.h"
#include "mock_backend.h"
using namespace refrender;

TEST_CASE("PassIterator executes in vector order and chains via BeforeIndex") {
    MockBackend be;
    DedicatedTargetPool pool(be);
    Camera cam;

    MeshRenderable mesh{ 1, 0xFFFFFFFFu, Identity() };
    WorldPass world(cam, &pool, {64,64}, "World");   // idx 0, BeforeIndex=-1 (sceneRaw)
    world.Add(&mesh);
    PostFxPass bloom(&pool, {64,64}, "Bloom");        // idx 1
    bloom.BeforeIndex = 0;                            // 입력 = World 출력

    PassIterator it;
    it.Add(&world);
    it.Add(&bloom);

    Texture sceneRaw = be.CreateTarget({64,64});
    Texture finalTex = it.Execute(be, sceneRaw);

    // bloom 의 입력은 world.GetPassResult() 여야 한다
    REQUIRE(be.blits.size() == 1);
    REQUIRE(be.blits[0].first == world.GetPassResult().id);
    // 최종 결과 = 마지막 패스 출력
    REQUIRE(finalTex.id == bloom.GetPassResult().id);
}
TEST_CASE("PassIterator: BeforeIndex -1 resolves to sceneRaw") {
    MockBackend be;
    DedicatedTargetPool pool(be);
    PostFxPass copy(&pool, {64,64}, "Copy");
    copy.BeforeIndex = -1;                            // sceneRaw 직접 입력

    PassIterator it;
    it.Add(&copy);
    Texture sceneRaw = be.CreateTarget({64,64});
    it.Execute(be, sceneRaw);

    REQUIRE(be.blits.size() == 1);
    REQUIRE(be.blits[0].first == sceneRaw.id);
}
TEST_CASE("PassIterator: empty returns sceneRaw") {
    MockBackend be;
    PassIterator it;
    Texture sceneRaw = be.CreateTarget({64,64});
    REQUIRE(it.Execute(be, sceneRaw).id == sceneRaw.id);
}
```

- [ ] **Step 2: 실패 확인**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R pass_iterator --output-on-failure`
Expected: FAIL — `pass_iterator.h` not found.

- [ ] **Step 3: 구현**

Create `src/render/pass_iterator.h`:
```cpp
#ifndef __REFRENDER_PASS_ITERATOR_H__
#define __REFRENDER_PASS_ITERATOR_H__
#include "<refrender>/passable.h"
#include <vector>
namespace refrender {
    // 순서 SSOT = Flat vector. before/after = 인덱스 (연구 C6: 포인터 아님, 수동 인덱스).
    class PassIterator {
    public:
        void Add(IPassable* p) { mPasses.push_back(p); }
        std::size_t Size() const { return mPasses.size(); }

        // 각 패스를 vector 순서대로 실행, BeforeIndex 로 입력 텍스처 해석 (안 A).
        Texture Execute(ICommandRecorder& rec, const Texture& sceneRaw) {
            for (std::size_t i = 0; i < mPasses.size(); ++i) {
                int bi = mPasses[i]->BeforeIndex;
                Texture before = (bi < 0) ? sceneRaw : mPasses[bi]->GetPassResult();
                mPasses[i]->Draw(rec, before);
            }
            return mPasses.empty() ? sceneRaw : mPasses.back()->GetPassResult();
        }

    private:
        std::vector<IPassable*> mPasses;   // 비소유 — 순서 단일 진실원천
    };
}
#endif
```

- [ ] **Step 4: 통과 확인 + Commit**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R pass_iterator --output-on-failure`
Expected: PASS (3 케이스).
```bash
git add src/render/pass_iterator.h <reference-render>/test/test_pass_iterator.cpp
git commit -m "feat(refrender): PassIterator flat-vector loop + BeforeIndex resolution"
```

---

## Task 9: 디버그 오버레이 — 임의 Pass 결과를 화면에 (연구 C4 결정 ③·④)

`DedicatedTargetPool` 이라 모든 중간 결과가 살아있음 → 인덱스 선택해 Present. 런타임 토글(컴파일 분리 ❌).

**Files:**
- Modify: `src/render/pass_iterator.h` (`Present` + `DebugPassIndex` 추가)
- Test: `<reference-render>/test/test_debug_overlay.cpp`

- [ ] **Step 1: 실패 테스트 작성**

Create `<reference-render>/test/test_debug_overlay.cpp`:
```cpp
#include <<catch2>/catch_test_macros.hpp>
#include "src/render/pass_iterator.h"
#include "<refrender>/passes.h"
#include "<refrender>/render_target_pool.h"
#include "mock_backend.h"
using namespace refrender;

static void buildChain(PassIterator& it, WorldPass& world, PostFxPass& bloom) {
    bloom.BeforeIndex = 0;
    it.Add(&world);
    it.Add(&bloom);
}

TEST_CASE("Present without debug shows final pass output") {
    MockBackend be; DedicatedTargetPool pool(be); Camera cam;
    MeshRenderable mesh{1, 0xFFFFFFFFu, Identity()};
    WorldPass world(cam, &pool, {64,64}, "World"); world.Add(&mesh);
    PostFxPass bloom(&pool, {64,64}, "Bloom");
    PassIterator it; buildChain(it, world, bloom);

    Texture sceneRaw = be.CreateTarget({64,64});
    it.Present(be, sceneRaw);
    REQUIRE(be.presented == bloom.GetPassResult().id);   // 기본 = 마지막
}
TEST_CASE("Present with DebugPassIndex shows that pass's intermediate result") {
    MockBackend be; DedicatedTargetPool pool(be); Camera cam;
    MeshRenderable mesh{1, 0xFFFFFFFFu, Identity()};
    WorldPass world(cam, &pool, {64,64}, "World"); world.Add(&mesh);
    PostFxPass bloom(&pool, {64,64}, "Bloom");
    PassIterator it; buildChain(it, world, bloom);

    it.DebugPassIndex = 0;                               // World 중간 결과 보기
    Texture sceneRaw = be.CreateTarget({64,64});
    it.Present(be, sceneRaw);
    REQUIRE(be.presented == world.GetPassResult().id);   // World 결과가 화면에
}
```

- [ ] **Step 2: 실패 확인**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R debug_overlay --output-on-failure`
Expected: FAIL — `Present` / `DebugPassIndex` not declared.

- [ ] **Step 3: 구현 (PassIterator public 섹션에 추가)**

In `src/render/pass_iterator.h`, inside `class PassIterator` public section (after `Execute`):
```cpp
        // 디버그 오버레이: -1 이면 최종 출력, 0..Size()-1 이면 그 패스의 중간 결과를 Present.
        // 런타임 토글 (모든 빌드 — 연구 C4 결정 ①). Dedicated 라 중간 결과가 살아있음.
        int DebugPassIndex = -1;

        void Present(ICommandRecorder& rec, const Texture& sceneRaw) {
            Texture out = Execute(rec, sceneRaw);
            if (DebugPassIndex >= 0 && DebugPassIndex < static_cast<int>(mPasses.size()))
                out = mPasses[DebugPassIndex]->GetPassResult();
            rec.Present(out);
        }
```

- [ ] **Step 4: 통과 확인 + Commit**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R debug_overlay --output-on-failure`
Expected: PASS.
```bash
git add src/render/pass_iterator.h <reference-render>/test/test_debug_overlay.cpp
git commit -m "feat(refrender): debug overlay — present any pass result via DebugPassIndex"
```

---

## Task 10: TransientTargetPool — seam 비파괴 진화 증명 (연구 C4 결정 ③)

같은 `IRenderTargetPool` 인터페이스로 aliasing 구현 추가 → Pass/Iterator 코드 무수정으로 교체 가능함을 테스트로 증명.

**Files:**
- Modify: `<reference-render>/include/refrender/render_target_pool.h` (`TransientTargetPool` 추가)
- Modify: `<reference-render>/test/test_pool.cpp` (Transient 테스트 추가)

- [ ] **Step 1: 실패 테스트 추가**

Append to `<reference-render>/test/test_pool.cpp`:
```cpp
TEST_CASE("TransientTargetPool: released texture is reused for same desc (aliasing)") {
    MockBackend be;
    TransientTargetPool pool(be);
    Texture a = pool.Acquire("PassA", {64,64});
    pool.Release("PassA");                       // 수명 종료 → free list
    Texture b = pool.Acquire("PassB", {64,64});  // 수명 안 겹침 → 재사용
    REQUIRE(a.id == b.id);                        // 같은 메모리 alias
    REQUIRE(be.createCount == 1);                 // 새로 안 만듦
}
TEST_CASE("TransientTargetPool: concurrent live passes get distinct textures") {
    MockBackend be;
    TransientTargetPool pool(be);
    Texture a = pool.Acquire("PassA", {64,64});  // 살아있음
    Texture b = pool.Acquire("PassB", {64,64});  // 동시 생존 → 공유 불가
    REQUIRE(a.id != b.id);
    REQUIRE(be.createCount == 2);
}
TEST_CASE("TransientTargetPool: drop-in for IRenderTargetPool (same seam)") {
    MockBackend be;
    TransientTargetPool pool(be);
    IRenderTargetPool& seam = pool;              // Pass 가 보는 타입
    Texture t = seam.Acquire("X", {32,32});
    REQUIRE(t.id != kInvalidTexture);
}
```

- [ ] **Step 2: 실패 확인**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R pool --output-on-failure`
Expected: FAIL — `TransientTargetPool` not declared.

- [ ] **Step 3: 구현 (render_target_pool.h 의 namespace 닫기 전에 추가)**

In `<reference-render>/include/refrender/render_target_pool.h`, add before the closing `}` of `namespace refrender` (and add `#include <vector>` at top):
```cpp
    // aliasing 공유 — 수명 안 겹치는 자원끼리 메모리 재사용 (연구 C4: "disjoint lifetimes share memory").
    // Unreal r.RDG.TransientAllocator ON 대응. 같은 seam 이라 Pass 코드 무수정 교체.
    class TransientTargetPool : public IRenderTargetPool {
    public:
        explicit TransientTargetPool(ITargetAllocator& alloc) : mAlloc(alloc) {}

        Texture Acquire(const std::string& key, const TargetDesc& desc) override {
            // free list 에서 desc 일치하는 놀고 있는 텍스처 재사용
            for (std::size_t i = 0; i < mFree.size(); ++i) {
                if (mFree[i].width == desc.width && mFree[i].height == desc.height) {
                    Texture t = mFree[i];
                    mFree.erase(mFree.begin() + i);
                    mInUse[key] = t;
                    return t;
                }
            }
            Texture t = mAlloc.CreateTarget(desc);
            mInUse[key] = t;
            return t;
        }
        void Release(const std::string& key) override {
            auto it = mInUse.find(key);
            if (it == mInUse.end()) return;
            mFree.push_back(it->second);   // 메모리를 free list 로 — 다음 Acquire 가 alias
            mInUse.erase(it);
        }

    private:
        ITargetAllocator& mAlloc;   // 좁은 Factory facet (ISP) — Dedicated 와 동일 seam
        std::unordered_map<std::string, Texture> mInUse;
        std::vector<Texture> mFree;
    };
```

- [ ] **Step 4: 통과 확인 + Commit**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build --output-on-failure`
Expected: 전체 테스트 PASS.
```bash
git add <reference-render>/include/refrender/render_target_pool.h <reference-render>/test/test_pool.cpp
git commit -m "feat(refrender): TransientTargetPool (aliasing) proves non-destructive seam evolution"
```

---

## Task 11: 통합 데모 — 3축 결합 + MiniMap 으로 표현력 증명 (연구 C5 표현력 표)

같은 `IRenderable` 풀을 두 Camera(메인 perspective + 미니맵 ortho)로 *다른 Target* 에 그려, "한 축만 바꿔 끼움" 을 증명.

**Files:**
- Test: `<reference-render>/test/test_integration.cpp`

- [ ] **Step 1: 통합 테스트 작성**

Create `<reference-render>/test/test_integration.cpp`:
```cpp
#include <<catch2>/catch_test_macros.hpp>
#include "src/render/pass_iterator.h"
#include "<refrender>/passes.h"
#include "<refrender>/render_target_pool.h"
#include "mock_backend.h"
using namespace refrender;

TEST_CASE("same renderables, two cameras (main + minimap) -> two distinct targets") {
    MockBackend be;
    DedicatedTargetPool pool(be);

    MeshRenderable a{1, 0xFFFFFFFFu, Identity()};
    MeshRenderable b{2, 0xFFFFFFFFu, Identity()};

    Camera main; main.proj = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};   // perspective 가정
    Camera mini; mini.proj = {2,0,0,0, 0,2,0,0, 0,0,1,0, 0,0,0,1};   // ortho 가정

    WorldPass world(main, &pool, {800,600}, "World");
    world.Add(&a); world.Add(&b);
    WorldPass minimap(mini, &pool, {200,200}, "MiniMap");            // 같은 대상, 다른 시점/표면
    minimap.Add(&a); minimap.Add(&b);

    PassIterator it; it.Add(&world); it.Add(&minimap);
    Texture sceneRaw = be.CreateTarget({800,600});
    it.Execute(be, sceneRaw);

    REQUIRE(world.GetPassResult().id != minimap.GetPassResult().id);  // 다른 Target
    REQUIRE(be.draws.size() == 4);                                    // 2 mesh x 2 pass
}
TEST_CASE("cullMask routes renderables to different cameras") {
    MockBackend be; DedicatedTargetPool pool(be);
    MeshRenderable world3d{1, 0b01, Identity()};   // 레이어0
    MeshRenderable uiquad {2, 0b10, Identity()};   // 레이어1

    Camera worldCam; worldCam.cullMask = 0b01;     // 3D 만
    Camera uiCam;    uiCam.cullMask    = 0b10;     // UI 만
    WorldPass worldPass(worldCam, &pool, {800,600}, "World");
    worldPass.Add(&world3d); worldPass.Add(&uiquad);
    WorldPass uiPass(uiCam, &pool, {800,600}, "UI");
    uiPass.Add(&world3d); uiPass.Add(&uiquad);

    PassIterator it; it.Add(&worldPass); it.Add(&uiPass);
    Texture sceneRaw = be.CreateTarget({800,600});
    it.Execute(be, sceneRaw);

    // World 패스는 world3d(tag1) 만, UI 패스는 uiquad(tag2) 만
    int tag1 = 0, tag2 = 0;
    for (auto& d : be.draws) { if (d.meshTag == 1) ++tag1; if (d.meshTag == 2) ++tag2; }
    REQUIRE(tag1 == 1);
    REQUIRE(tag2 == 1);
}
```

- [ ] **Step 2: 실패→통과 확인**

Run: `cmake --build reference-render/build && ctest --test-dir reference-render/build -R integration --output-on-failure`
Expected: 처음엔 빌드만으로 PASS (새 프로덕션 코드 불필요 — 기존 조각의 조합). 만약 컴파일 에러면 헤더 누락 점검.

- [ ] **Step 3: Commit**
```bash
git add <reference-render>/test/test_integration.cpp
git commit -m "test(refrender): integration — 3-axis recombination (multi-camera, cullMask routing)"
```

---

## Self-Review (작성자 체크)

**Spec coverage** (연구 문서 → Task 매핑):
- C1 (IPassable 다형) → Task 6 `IPassable` + WorldPass/PostFxPass(Task 7).
- C2 (cullMask=시점) → Task 3 `Camera::Sees` + Task 6 WorldPass 필터 + Task 11 라우팅.
- C3 (Pass 2단: 정렬+ROP) → ⚠️ *부분*: 본 레퍼런스는 *구조축*(3축 결합 + 풀 + 순서)에 집중하고 ROP/정렬은 `IRenderBackend` 뒤로 추상화(범위 밖 명시). 정렬은 PassIterator 의 vector 순서로 대표. → 의도된 단순화(아래 Out of scope).
- C4 (텍스처 소유) → Task 5 Dedicated + Task 10 Transient(seam 진화) + Task 9 디버그 오버레이.
- C5 (Camera/Target 분리 + 3축 결합) → Task 3/6 분리, Task 11 표현력.
- C6 (Flat vector + BeforeIndex) → Task 8 PassIterator.
- C7 (안 A: Draw(before)) → Task 6 `IPassable::Draw(backend, before)`.

**Placeholder scan:** 모든 step 에 실제 코드/명령/기대출력 포함. TBD 없음.

**Type consistency 점검:**
- `Texture{id,width,height}`, `TargetDesc{width,height}` — Task1 정의, 전 Task 일관.
- `ITargetAllocator`(CreateTarget/DestroyTarget) + `ICommandRecorder`(BindTarget/DrawMesh/Blit/Present) + `IRenderBackend`(우산 `: 둘`) — Task2 정의. Pool→`ITargetAllocator&`, Pass/Renderable/Iterator→`ICommandRecorder&`, `MockBackend` 가 둘 다 구현. (연구 Q2 SRP/ISP)
- `IRenderTargetPool::Acquire(key, desc)` / `Release(key)` — Task5 정의. 구현(Dedicated/Transient)은 `ITargetAllocator&` 보유. WorldPass/PostFxPass 가 `mPool` 로 소유.
- `IPassable::Draw(rec, before)` / `GetPassResult()` / `Key()` / `BeforeIndex` — Task6 정의 (`rec` = `ICommandRecorder&`), PostFx/PassIterator 동일.
- `PassIterator::Add/Execute(rec,..)/Present(rec,..)/DebugPassIndex` — Task8/9 일관 (`rec` = `ICommandRecorder&`).

## Out of Scope (명시)

- **실제 GL 백엔드 (`GlBackend`)** — `IRenderBackend`(=`ITargetAllocator`+`ICommandRecorder`) seam 만 제공. GL 구현은 비교 단계(2차)에서 작성. 🔒 **이 `GlBackend` 가 Graphics API(OpenGL/Vulkan/Metal)에 의존·링크하는 *유일한* 모듈** (위 §의존성 격리 불변식). 작성 시 GL 헤더/`find_package(OpenGL)`/`-lGL` 는 *오직 `GlBackend` 타깃의 PRIVATE* 로 격리할 것.
- **ROP 상세**(stencil/depth/blend/colormask) + **다단계 정렬키**(queue|material|depth) — `IRenderBackend::DrawMesh` 뒤로 추상화. 연구 C3 는 검증됐으나 *구조 레퍼런스*의 초점(3축/풀/순서)에서 제외. 2차에서 `RenderStateBlock` 으로 확장.
- **다중 입력**(`vector<int> Inputs`) — 단일 `BeforeIndex` 만. 연구 C6 의 확장 트리거 도달 시 추가.
- **data-driven(JSON)** — 코드 수동 배선만. 연구 C6 후속 ②.

## Execution Handoff

**Plan complete and saved to `doc/superpowers/plans/2026-06-22-reference-render-pipeline.md`. Two execution options:**

**1. Subagent-Driven (recommended)** — 태스크마다 새 subagent, 태스크 간 리뷰, 빠른 반복.

**2. Inline Execution** — 이 세션에서 executing-plans 로 배치 실행 + 체크포인트.

**Which approach?**
