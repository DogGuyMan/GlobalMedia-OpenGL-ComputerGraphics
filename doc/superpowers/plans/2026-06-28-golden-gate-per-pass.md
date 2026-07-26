# 골든 게이트 세분화 (WorldPass RenderStateBlock별 + PostFX 패스별) Implementation Plan

> ⚠️ **2026-07-26 정정 (원문 보존)** — 본 문서에 나오는 `SJH_GOLDEN_CAPTURE=1 ./_MyApp_` 실행과 `ctest --test-dir build_ninja -R golden` 은 *당시* 절차이며 현재는 **폐기**됐다. 골든 캡처가 런타임 환경 변수 -> 컴파일 정의로 바뀌어 프리셋 `ninja-golden`(빌드 디렉토리 `build_ninja-golden`) 전유가 됐고, 게임 빌드의 `_MyApp_` 는 골든을 캡처하지 않는다(실행해도 창만 뜨는 조용한 실패). 현행 절차 = `test/CLAUDE.md`. 아래 본문은 당시 기록으로 그대로 둔다.

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 골든 캡처를 엔진 `src/diagnostics` 모듈로 일반화하고, PostFX 누적 8골든 + WorldPass RenderQueue별 raw-FBO 골든을 추가해 회귀를 패스/상태별로 국소화한다.

**Architecture:** 클라 캡처 로직(readback+orchestration)을 `src/diagnostics`(cycle-exempt 예외 모듈)로 이관 — `frame_capture`(glReadPixels+PNG) + `pass_capture`(PassIterator 변형 runner). `RenderableProcessor` 에 queue-range 필터 추가로 단일 RenderQueue 격리. `golden_compare` 를 glob 데이터주도로 확장. 정본 spec = `doc/superpowers/specs/2026-06-28-golden-gate-per-pass-design.md`.

**Tech Stack:** C++17, CMake/Ninja, vcpkg(stb·opencv4·catch2), OpenGL 4.1, sb7/GLFW, spdlog.

---

## 배경 — 반드시 먼저 읽기 (제로컨텍스트 대비)

- 캡처는 `apps/_MyApp_/main.cpp` 가 env `SJH_GOLDEN_CAPTURE=1` 시 startup 에서 `srand(42)`, render() 가 벽시계 대신 `mCaptureFrame/60.0`(고정-dt) 로 구동, **frame==180** 에 3변형(full/no_imgui/skybox) 캡처 후 `glfwSetWindowShouldClose`. 현 캡처 블록 = `main.cpp:470-532`.
- **PassIterator**(`src/render/pass_iterator.h`, **GL-격리 헤더**): `Add`/`Find(key)→IPassable*`/`Keys()`/`Execute(DeviceContext&, RenderTarget& backbuffer)`/`DebugPassIndex`(public int, -1=전체). Pass 접근 = `Find(key)` 후 `IPassable::Enabled`(bool, `render_passable.h:69`) 토글. 패스 키: World/Skybox/Particle/ImGui + PostFX 는 `mKey`(=POSTFX 스테이지명), present 는 "present".
- **World FBO** = `mSceneFB`(`SJH::RenderTexture`, `main.cpp:123`), backbuffer = `mDefaultTarget`(`DefaultRenderTarget`). WorldPass 포인터 = `mWorldPassPtr`(`main.cpp:245`). fbW/fbH = `main.cpp:391-392`.
- **RenderTarget**(`src/buffer/render_target.h`): `Bind()`(=glBindFramebuffer+glViewport) + `GetWidth/Height()`. `RenderTexture : RenderTarget`.
- **RenderableProcessor::Process**(`src/render/renderable_processor.cpp:61-77`): `for (const auto &e : mWorld) { rc.ApplyRenderStateBlock(e.r->GetRenderStateBlock()); e.r->Render(rc, cam); }` — 각 `e` 에 `.queueLayer` 보유(Sort 비교자 L55 사용). WorldPass 가 `mProc`(RenderableProcessor) 소유.
- **diagnostics = cycle-exempt 예외 모듈**(`.claude/CLAUDE.md`「예외 모듈 지위」) — render/buffer 상향 링크 + 순환 허용.

**공통 규칙**: 주석 한글 · **커밋 path-scoped, `git add -A` 금지** · Co-Authored-By 미사용 · 게임 로직/셰이더 무변경 · 빌드 `cmake --build --preset ninja --target _MyApp_ golden_compare` · 캡처 실행 `cd build_ninja/apps/_MyApp_ && SJH_GOLDEN_CAPTURE=1 ./_MyApp_`.

---

## File Structure

| 파일 | 책임 | 유닛 |
|---|---|---|
| `src/diagnostics/frame_capture.h/.cpp` (**신규**) | glReadPixels+vflip+PNG readback (backbuffer / RenderTarget) | GG-0 |
| `src/diagnostics/pass_capture.h/.cpp` (**신규**) | `CaptureVariant` + `RunCaptureVariants`(PassIterator 변형 runner) | GG-0/A/B |
| `src/diagnostics/CMakeLists.txt` (수정) | frame_capture/pass_capture 소스 + SJH::render/buffer/stb 링크 | GG-0 |
| `<apps>/_MyApp_/src/Capture/golden_capture.h/.cpp` (**삭제**) | → 엔진 이관 | GG-0 |
| `apps/_MyApp_/main.cpp` (수정) | 캡처블록 → 변형 LIST + `RunCaptureVariants` 1콜 | GG-0/A/B |
| `apps/_MyApp_/CMakeLists.txt` (수정) | Capture/ 소스 제거 | GG-0 |
| `src/render/renderable_processor.h/.cpp` (수정) | queue-range 필터 필드 + setter + Process skip | GG-B |
| `src/render/render_passable/render_passable.impls.h/.cpp` (수정) | WorldPass `SetQueueFilter` 전달 | GG-B |
| `test/golden_compare/golden_compare.cpp` (수정) | 하드코딩 3 → `golden_*.png` glob | GG-C |
| `test/golden/golden_*.png` (**신규 커밋**) | postfx 8 + world ~4 참조 골든(육안 승인) | GG-C |

---

# UNIT GG-0 — 캡처 facility 엔진 모듈화

### Task 0.1: `frame_capture` (diagnostics readback)

**Files:**
- Create: `src/diagnostics/frame_capture.h`
- Create: `src/diagnostics/frame_capture.cpp`

- [ ] **Step 1: 헤더 작성**

`src/diagnostics/frame_capture.h`:
```cpp
/**
 * @file frame_capture.h
 * @brief 프레임 readback -> PNG (골든 캡처). glReadPixels + 수직flip + stb_write.
 * @note diagnostics = cycle-exempt 예외 모듈 (CLAUDE.md). GL 호출은 .cpp 내부만.
 */
#ifndef __SJH_FRAME_CAPTURE_H__
#define __SJH_FRAME_CAPTURE_H__

#include <string>

namespace SJH { class RenderTarget; }

namespace SJH::Diagnostics
{
    /// @brief 백버퍼(FBO 0)를 PNG 로 저장. @return 성공 여부.
    bool CaptureBackbufferToPng(const std::string &path, int w, int h);

    /// @brief 임의 RenderTarget(FBO)을 Bind 후 PNG 로 저장 (raw World FBO 등, PostFX 전).
    bool CaptureTargetToPng(RenderTarget &target, const std::string &path, int w, int h);
}
#endif // __SJH_FRAME_CAPTURE_H__
```

- [ ] **Step 2: 구현 작성** (기존 `<apps>/_MyApp_/src/Capture/golden_capture.cpp` 로직 이관 + Target 변형)

`src/diagnostics/frame_capture.cpp`:
```cpp
/**
 * @file frame_capture.cpp
 * @brief frame_capture 구현. STB_IMAGE_WRITE_IMPLEMENTATION 을 이 TU 에서만 정의
 *        (image.cpp 의 STB_IMAGE_IMPLEMENTATION 과 별개 매크로 - 충돌 없음).
 */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "diagnostics/frame_capture.h"
#include "buffer/render_target.h"

#include <GL/gl3w.h>
#include <cstring>
#include <<spdlog>/spdlog.h>
#include <vector>

namespace SJH::Diagnostics
{
    namespace
    {
        /// @brief 현재 READ FBO 에서 [w,h] RGBA 를 읽어 수직 flip 후 PNG 저장.
        bool ReadFlipWrite(const std::string &path, int w, int h)
        {
            if (w <= 0 || h <= 0)
            {
                spdlog::error("[FrameCapture] 유효하지 않은 크기: {}x{}", w, h);
                return false;
            }
            const int stride = w * 4;
            std::vector<unsigned char> raw(static_cast<std::size_t>(h) * stride);
            glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());
            const GLenum err = glGetError();
            if (err != GL_NO_ERROR)
            {
                spdlog::error("[FrameCapture] glReadPixels 실패 0x{:X}", err);
                return false;
            }
            // ★ 상하 반전 방지: GL 프레임버퍼 원점=좌하단, PNG 원점=좌상단 이라 행 순서가 반대다.
            //   glReadPixels 결과를 그대로 저장하면 이미지가 위아래로 뒤집힌다 - 행을 역순 복사해 바로 세운다.
            //   (육안 검증이 똑바로 보이고, flip 상태로 커밋된 기존 골든과 bit-일치하기 위해 필수.)
            std::vector<unsigned char> flipped(static_cast<std::size_t>(h) * stride);
            for (int row = 0; row < h; ++row)
                std::memcpy(flipped.data() + row * stride,
                            raw.data() + (h - 1 - row) * stride,
                            static_cast<std::size_t>(stride));
            if (stbi_write_png(path.c_str(), w, h, 4, flipped.data(), stride) == 0)
            {
                spdlog::error("[FrameCapture] stbi_write_png 실패: {}", path);
                return false;
            }
            spdlog::info("[FrameCapture] PNG 저장: {} ({}x{})", path, w, h);
            return true;
        }
    }

    bool CaptureBackbufferToPng(const std::string &path, int w, int h)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return ReadFlipWrite(path, w, h);
    }

    bool CaptureTargetToPng(RenderTarget &target, const std::string &path, int w, int h)
    {
        target.Bind(); // glBindFramebuffer(target FBO) + glViewport
        return ReadFlipWrite(path, w, h);
    }
}
```

> ⚠ MSAA 주의: `mSceneFB`(RenderTexture)가 멀티샘플이면 `glReadPixels` 가 실패한다. 구현 시 `RenderTexture` 가 non-MSAA 임을 확인(현 `CreateWithDepthTexture` 는 single-sample). MSAA 면 blit-resolve FBO 경유 필요 — 그 경우 Task 0.1 에 resolve 스텝 추가 후 보고.

- [ ] **Step 3: 커밋** (배선은 Task 0.3 후 빌드 — 여기선 파일만)
```bash
git add src/diagnostics/frame_capture.h src/diagnostics/frame_capture.cpp
git commit -m "[diag] : frame_capture 신규 - 골든 readback 엔진 이관(backbuffer/target)"
```

### Task 0.2: `pass_capture` (변형 runner)

**Files:**
- Create: `src/diagnostics/pass_capture.h`
- Create: `src/diagnostics/pass_capture.cpp`

- [ ] **Step 1: 헤더 작성**

`src/diagnostics/pass_capture.h`:
```cpp
/**
 * @file pass_capture.h
 * @brief PassIterator 변형별 골든 캡처 runner. 각 변형 = 패스 Enabled 오버라이드 +
 *        DebugPassIndex + World queue-range + 캡처 대상(backbuffer/worldFbo).
 * @note diagnostics = cycle-exempt: SJH::render(PassIterator/WorldPass)+buffer 상향 링크.
 */
#ifndef __SJH_PASS_CAPTURE_H__
#define __SJH_PASS_CAPTURE_H__

#include <climits>
#include <string>
#include <utility>
#include <vector>

namespace SJH { class PassIterator; class DeviceContext; class RenderTarget; class WorldPass; }

namespace SJH::Diagnostics
{
    /// @brief 캡처 변형 1개 명세(순수 데이터). 복원은 runner 가 책임.
    struct CaptureVariant
    {
        std::string outName;                                    ///< 출력 파일명(확장자 제외).
        std::vector<std::pair<std::string, bool>> passOverride; ///< {passKey, Enabled} 오버라이드.
        int stopAtPass = -1;                                    ///< DebugPassIndex (-1=전체).
        int worldQueueMin = INT_MIN;                            ///< World 큐 필터 [min,max) 하한.
        int worldQueueMax = INT_MAX;                            ///< 상한.
        enum Target { Backbuffer, WorldFbo } target = Backbuffer;
    };

    /// @brief 변형 목록을 순회하며 각 캡처. 각 변형 전후로 상태 저장/복원.
    /// @param it       Pass 반복자(등록/실행 소유).
    /// @param rec      GL facade.
    /// @param backbuffer 최종 출력(Execute 인자).
    /// @param worldFbo World FBO(RenderTexture) - WorldFbo target 캡처 대상. nullptr 이면 그 변형 skip.
    /// @param worldPass World 큐 필터 대상. nullptr 이면 큐 필터 무시.
    /// @param variants 캡처 변형 목록.
    /// @param outDir   출력 디렉토리(자동 생성됨을 호출자가 보장).
    /// @param w,h      프레임 크기.
    void RunCaptureVariants(PassIterator &it, DeviceContext &rec, RenderTarget &backbuffer,
                            RenderTarget *worldFbo, WorldPass *worldPass,
                            const std::vector<CaptureVariant> &variants,
                            const std::string &outDir, int w, int h);
}
#endif // __SJH_PASS_CAPTURE_H__
```

- [ ] **Step 2: 구현 작성**

`src/diagnostics/pass_capture.cpp`:
```cpp
/**
 * @file pass_capture.cpp
 * @brief pass_capture 구현 - 변형별 상태 저장/설정/Execute/readback/복원.
 */
#include "diagnostics/pass_capture.h"
#include "diagnostics/frame_capture.h"

#include "render/pass_iterator.h"
#include "render/render_passable/render_passable.h"       // IPassable::Enabled
#include "render/render_passable/render_passable.impls.h" // WorldPass::SetQueueFilter

#include <<spdlog>/spdlog.h>

namespace SJH::Diagnostics
{
    void RunCaptureVariants(PassIterator &it, DeviceContext &rec, RenderTarget &backbuffer,
                            RenderTarget *worldFbo, WorldPass *worldPass,
                            const std::vector<CaptureVariant> &variants,
                            const std::string &outDir, int w, int h)
    {
        for (const auto &v : variants)
        {
            if (v.target == CaptureVariant::WorldFbo && worldFbo == nullptr)
            {
                spdlog::warn("[PassCapture] '{}' WorldFbo 대상인데 worldFbo=null - skip.", v.outName);
                continue;
            }

            // --- 상태 저장 ---
            std::vector<std::pair<IPassable *, bool>> saved; // {pass, 이전 Enabled}
            for (const auto &[key, want] : v.passOverride)
            {
                IPassable *p = it.Find(key);
                if (p)
                {
                    saved.emplace_back(p, p->Enabled);
                    p->Enabled = want;
                }
            }
            const int savedDebug = it.DebugPassIndex;
            it.DebugPassIndex = v.stopAtPass;
            const bool useQueue = worldPass && (v.worldQueueMin != INT_MIN || v.worldQueueMax != INT_MAX);
            if (useQueue)
                worldPass->SetQueueFilter(v.worldQueueMin, v.worldQueueMax);

            // --- 렌더 + 캡처 ---
            it.Execute(rec, backbuffer);
            const std::string path = outDir + "/" + v.outName + ".png";
            const bool ok = (v.target == CaptureVariant::WorldFbo)
                                ? CaptureTargetToPng(*worldFbo, path, w, h)
                                : CaptureBackbufferToPng(path, w, h);
            spdlog::info("[PassCapture] {} {}", v.outName, ok ? "OK" : "FAIL");

            // --- 복원 ---
            if (useQueue)
                worldPass->SetQueueFilter(INT_MIN, INT_MAX);
            it.DebugPassIndex = savedDebug;
            for (auto &[p, prev] : saved)
                p->Enabled = prev;
        }
    }
}
```

- [ ] **Step 3: 커밋**
```bash
git add src/diagnostics/pass_capture.h src/diagnostics/pass_capture.cpp
git commit -m "[diag] : pass_capture 신규 - PassIterator 변형별 골든 runner"
```

### Task 0.3: diagnostics CMake 배선 (cycle-exempt 링크)

**Files:**
- Modify: `src/diagnostics/CMakeLists.txt`

- [ ] **Step 1: 소스 + 링크 추가**

`src/diagnostics/CMakeLists.txt` 의 `add_library(...)` 소스 목록에 `frame_capture.cpp pass_capture.cpp` 추가하고, `target_link_libraries` 를 다음으로 수정(기존 `PUBLIC project_deps spdlog` 유지 + 추가):
```cmake
target_link_libraries(sjhopengl_diagnostics
    PUBLIC  project_deps
            spdlog
    # ★ cycle-exempt (CLAUDE.md 예외 모듈): 캡처 orchestration 이 render/buffer 상향 의존.
    #   render->diagnostics 와 순환 - CMake 가 STATIC lib 순환을 link-line 반복으로 해소.
    PRIVATE SJH::render
            SJH::buffer
            stb_extra
)
```
> 정확한 소스 나열 형식은 기존 `add_library(sjhopengl_diagnostics ...)` 를 읽어 맞춘다. `stb_extra` = stb include INTERFACE(`cmake/Dependency.cmake`).

- [ ] **Step 2: 빌드 - 순환 링크 통과 검증 (★최대 리스크)**

Run: `cmake --preset ninja -DENABLE_TESTING=ON && cmake --build --preset ninja --target SJH::diagnostics 2>&1 | tail -20`
Expected: 링크 GREEN. **만약 undefined symbol(순환 미해소) 발생 시**: CMake 는 보통 STATIC 순환을 자동 반복하나, 실패하면 최종 소비자(`SJH::engine` 우산/`_MyApp_`)에서 해소되므로 `--target _MyApp_` 로 확인. 그래도 실패 시 `LINK_INTERFACE_MULTIPLICITY` 상향 또는 diagnostics↔render 를 OBJECT 라이브러리 병합 검토 후 보고(BLOCKED).

- [ ] **Step 3: 커밋**
```bash
git add src/diagnostics/CMakeLists.txt
git commit -m "[diag] : CMake - frame_capture/pass_capture + render/buffer 링크(cycle-exempt)"
```

### Task 0.4: main.cpp 재배선 + 기존 Capture 삭제

**Files:**
- Modify: `apps/_MyApp_/main.cpp`
- Delete: `<apps>/_MyApp_/src/Capture/golden_capture.h`, `<apps>/_MyApp_/src/Capture/golden_capture.cpp`
- Modify: `apps/_MyApp_/CMakeLists.txt`

- [ ] **Step 1: include 교체** — `apps/_MyApp_/main.cpp` 상단 `#include "<Capture>/golden_capture.h"` (L63) 를 삭제하고 다음 추가:
```cpp
#include "diagnostics/pass_capture.h"
```

- [ ] **Step 2: 캡처 블록 교체** — `main.cpp:470-532` 의 `if (mCaptureMode) { if (mCaptureFrame == 180) { ... } ++mCaptureFrame; }` 전체를 다음으로 치환:
```cpp
if (mCaptureMode)
{
    if (mCaptureFrame == 180)
    {
        const std::string outDir = "test/golden";
        std::filesystem::create_directories(outDir);

        // G1 전체는 ImGui 프레임 재빌드가 필요(PassDebugLayer 제외) - 별도 처리.
        if (mPassDebugLayerPtr)
            mPassDebugLayerPtr->Enabled = false;
        ImGui_ImplGlfwGL3_NewFrame();
        mImGuiStack.RenderAll(mShowEditor);
        mPassIterator.Execute(SJH::DeviceContext::Get(), *mDefaultTarget);
        SJH::Diagnostics::CaptureBackbufferToPng(outDir + "/golden_full.png", fbW, fbH);
        if (mPassDebugLayerPtr)
            mPassDebugLayerPtr->Enabled = true;

        // 나머지 변형은 데이터주도 runner. 패스 키 = Keys() 참조.
        const int presentStop = static_cast<int>(mPassIterator.Keys().size()) - 2; // ImGui 직전
        std::vector<SJH::Diagnostics::CaptureVariant> variants = {
            {"golden_no_imgui", {}, presentStop, INT_MIN, INT_MAX,
             SJH::Diagnostics::CaptureVariant::Backbuffer},
            {"golden_skybox", {{"World", false}, {"Particle", false}}, presentStop, INT_MIN, INT_MAX,
             SJH::Diagnostics::CaptureVariant::Backbuffer},
        };
        SJH::Diagnostics::RunCaptureVariants(
            mPassIterator, SJH::DeviceContext::Get(), *mDefaultTarget,
            mSceneFB.get(), mWorldPassPtr, variants, outDir, fbW, fbH);

        glfwSetWindowShouldClose(window, 1);
    }
    ++mCaptureFrame;
}
```
> `#include <climits>` 가 main.cpp 에 없으면 `INT_MIN/INT_MAX` 위해 추가(또는 pass_capture.h 가 이미 climits 포함하므로 전파됨).

- [ ] **Step 3: 기존 Capture 삭제 + CMake 정리**
```bash
git rm <apps>/_MyApp_/src/Capture/golden_capture.h <apps>/_MyApp_/src/Capture/golden_capture.cpp
```
`apps/_MyApp_/CMakeLists.txt` 에서 `<src>/Capture/golden_capture.cpp` 를 소스 목록에서 제거(grep `Capture` 로 찾아 삭제). `Capture/` 디렉토리 비면 무시.

- [ ] **Step 4: 빌드 + 3골든 bit-동일 재생성 검증 (★회귀)**

Run:
```bash
cmake --build --preset ninja --target _MyApp_ golden_compare
cd build_ninja/apps/_MyApp_ && SJH_GOLDEN_CAPTURE=1 ./_MyApp_ ; cd -
# 새 캡처본 vs 커밋 골든 bit 비교
for f in golden_full golden_no_imgui golden_skybox; do
  cmp build_ninja/apps/_MyApp_/test/golden/$f.png test/golden/$f.png && echo "$f OK" || echo "$f DIFF"
done
```
Expected: `golden_full OK` / `golden_no_imgui OK` / `golden_skybox OK` (bit-동일 = 리팩토링 무회귀). 만약 DIFF 면 이관 중 렌더 경로 변경 - 원인 규명 후 보고(threshold 조작 금지).

- [ ] **Step 5: 전체 ctest 회귀 0**

Run: `ctest --test-dir build_ninja --output-on-failure`
Expected: `100% tests passed ... out of 122` (기존 122, 회귀 0).

- [ ] **Step 6: 커밋**
```bash
git add apps/_MyApp_/main.cpp apps/_MyApp_/CMakeLists.txt
git commit -m "[test] : 캡처 orchestration 을 diagnostics pass_capture 로 이관(main 축소)"
```

**★HANDOFF #GG-0**: 3골든 bit-동일 재생성 + ctest 122 + 순환 링크 통과. green 이면 여기서 handoff 가능.

---

# UNIT GG-A — PostFX 누적 골든 (8)

### Task A.1: postfx 누적 변형 추가

**Files:**
- Modify: `apps/_MyApp_/main.cpp` (Task 0.4 의 `variants` 목록)

- [ ] **Step 1: postfx 스테이지 키 확인** — `POSTFX_PROGRAM_CONFIGS`(`apps/_MyApp_/src/Playable/Constants.h:194`) 순서 = gamma·sharpening·bloom·fog·grayscale_vignetting·invert·blurring·sobel(depth_debug 제외). 각 PostFxPass 키 = 그 이름(PostFxPass `mKey`). Keys() 로 실제 등록 키 확인(`Find("gamma")` 등).

- [ ] **Step 2: 누적 변형 8개 추가** — Task 0.4 `variants` 초기화 뒤에 append:
```cpp
// PostFX 누적(GG-A): 스테이지 0..N 을 enable, 나머지 postfx off. 기본이 off 라 명시 enable.
static const char *kPostFx[] = {"gamma", "sharpening", "bloom", "fog",
                                "grayscale_vignetting", "invert", "blurring", "sobel"};
const int kPostFxN = 8;
for (int n = 1; n <= kPostFxN; ++n)
{
    SJH::Diagnostics::CaptureVariant v;
    v.outName = "golden_postfx_" + std::to_string(n) + "_" + kPostFx[n - 1];
    for (int i = 0; i < kPostFxN; ++i)
        v.passOverride.emplace_back(kPostFx[i], i < n); // 0..n-1 enable, 나머지 off
    v.stopAtPass = presentStop; // ImGui 제외
    v.target = SJH::Diagnostics::CaptureVariant::Backbuffer;
    variants.push_back(std::move(v));
}
```

- [ ] **Step 3: 빌드 + 캡처 + 8골든 생성 확인**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && SJH_GOLDEN_CAPTURE=1 ./_MyApp_ ; cd -
ls build_ninja/apps/_MyApp_/test/golden/golden_postfx_*.png | wc -l
```
Expected: `8` (golden_postfx_1_gamma … golden_postfx_8_sobel).

- [ ] **Step 4: 육안 검증** — 8장이 누적 효과(gamma → +sharpening → …)를 보이는지 사용자 확인. (이미지 열어 순차 변화 확인.)

- [ ] **Step 5: 커밋** (참조 골든 커밋은 GG-C 에서 일괄)
```bash
git add apps/_MyApp_/main.cpp
git commit -m "[test] : PostFX 누적 골든 8변형 캡처(GG-A)"
```

**★HANDOFF #GG-A**: 8 postfx 골든 생성 + 육안.

---

# UNIT GG-B — WorldPass RenderQueue별 raw-FBO 골든

### Task B.1: RenderableProcessor queue-range 필터

**Files:**
- Modify: `src/render/renderable_processor.h`
- Modify: `src/render/renderable_processor.cpp:70-74`

- [ ] **Step 1: 필터 필드 + setter 선언** — `renderable_processor.h` 의 `class RenderableProcessor` public 에 추가:
```cpp
/// @brief World draw 를 queueLayer [min,max) 로 제한(디버그/골든 캡처용). 기본 전범위(무영향).
void SetQueueFilter(int minQueue, int maxQueue) { mQueueMin = minQueue; mQueueMax = maxQueue; }
```
private 멤버에 추가:
```cpp
int mQueueMin = INT_MIN; ///< World 큐 필터 하한 [min,max) - 캡처 격리용(기본 무영향).
int mQueueMax = INT_MAX; ///< 상한.
```
그리고 헤더 상단에 `#include <climits>` 추가.

- [ ] **Step 2: Process 루프에 skip 삽입** — `renderable_processor.cpp:70-74` 를 치환:
```cpp
for (const auto &e : mWorld)
{
    if (e.queueLayer < mQueueMin || e.queueLayer >= mQueueMax)
        continue; // 큐 필터 밖 - 캡처 격리 (기본 전범위라 무영향).
    rc.ApplyRenderStateBlock(e.r->GetRenderStateBlock());
    e.r->Render(rc, cam);
}
```

- [ ] **Step 3: 빌드 확인 (기본 무영향)**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: GREEN. (필터 미설정 = 전범위라 기존 렌더 무변경.)

- [ ] **Step 4: 커밋**
```bash
git add src/render/renderable_processor.h src/render/renderable_processor.cpp
git commit -m "[render] : RenderableProcessor queue-range 필터(캡처 격리용, 기본 무영향)"
```

### Task B.2: WorldPass::SetQueueFilter 전달

**Files:**
- Modify: `src/render/render_passable/render_passable.impls.h` (WorldPass 클래스)
- Modify: `src/render/render_passable/render_passable.impls.cpp`

- [ ] **Step 1: WorldPass 에 전달 메서드 선언** — `render_passable.impls.h` 의 `class WorldPass` public 에 추가:
```cpp
/// @brief World draw 를 queueLayer [min,max) 로 제한 - mProc 에 전달(골든 캡처 격리용).
void SetQueueFilter(int minQueue, int maxQueue);
```

- [ ] **Step 2: 구현** — `render_passable.impls.cpp` 의 WorldPass 구현부(예: `WorldPass::GetPassResult` 근처)에 추가:
```cpp
void WorldPass::SetQueueFilter(int minQueue, int maxQueue)
{
    mProc.SetQueueFilter(minQueue, maxQueue);
}
```
> `mProc` 멤버명은 impls.cpp 의 `mProc.Process(...)`(L190) 기준. 다르면 맞춘다.

- [ ] **Step 3: 빌드 확인**

Run: `cmake --build --preset ninja --target _MyApp_`
Expected: GREEN.

- [ ] **Step 4: 커밋**
```bash
git add src/render/render_passable/render_passable.impls.h src/render/render_passable/render_passable.impls.cpp
git commit -m "[render] : WorldPass::SetQueueFilter - mProc 큐필터 전달"
```

### Task B.3: World 큐별 raw-FBO 변형 추가

**Files:**
- Modify: `apps/_MyApp_/main.cpp` (`variants` 목록)

- [ ] **Step 1: 큐 값 확인** — `SJH::Pass::RenderQueue`(`src/material/pass.h`): Skybox=2500, AlphaTest=2450, Opaque=2000, Transparent=3000. TitleState 실 콘텐츠(대략 Skybox/Transparent/AlphaTest; Opaque=불릿은 Title 부재). 각 큐 = `[queue, queue+1)` 로 단일 격리.

- [ ] **Step 2: World 큐별 변형 추가** — Task A.2 뒤에 append:
```cpp
// World 큐별 raw FBO(GG-B, D1 - PostFX 전). WorldFbo 대상 = mSceneFB.
struct { const char *name; int q; } kWorldQ[] = {
    {"golden_world_skybox", 2500},
    {"golden_world_alphatest", 2450},
    {"golden_world_opaque", 2000},
    {"golden_world_transparent", 3000},
};
for (const auto &wq : kWorldQ)
{
    SJH::Diagnostics::CaptureVariant v;
    v.outName = wq.name;
    v.worldQueueMin = wq.q;
    v.worldQueueMax = wq.q + 1; // [q, q+1) 단일 큐
    v.target = SJH::Diagnostics::CaptureVariant::WorldFbo; // mSceneFB (PostFX 전)
    variants.push_back(std::move(v));
}
```
> Skybox 는 SkyboxPass 가 별도라 WorldPass 큐 필터로 안 잡힐 수 있음(WorldPass 는 skybox 제외 수집, impls.cpp:206). 실행 후 golden_world_skybox 가 빈(검정) 이면 그 항목 제거하고 보고. 빈 큐(Opaque 등)도 검정 PNG 생성됨 - GG-C 육안에서 판정.

- [ ] **Step 3: 빌드 + 캡처 + 확인**

Run:
```bash
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && SJH_GOLDEN_CAPTURE=1 ./_MyApp_ ; cd -
ls build_ninja/apps/_MyApp_/test/golden/golden_world_*.png
```
Expected: 4개 생성(일부는 빈/검정일 수 있음 - 육안 판정).

- [ ] **Step 4: 육안 검증 + 빈 큐 정리** — 각 golden_world_* 가 그 큐 콘텐츠만 담는지 확인. 빈(검정) 큐는 변형에서 제거(TitleState 미존재 큐). 사용자 확인.

- [ ] **Step 5: 커밋**
```bash
git add apps/_MyApp_/main.cpp
git commit -m "[test] : World RenderQueue별 raw-FBO 골든(GG-B)"
```

**★HANDOFF #GG-B**: World 큐별 골든 + 육안 + 빈 큐 정리.

---

# UNIT GG-C — 비교 게이트 데이터주도(glob) + 참조 골든 커밋

### Task C.1: golden_compare glob 화

**Files:**
- Modify: `test/golden_compare/golden_compare.cpp`

- [ ] **Step 1: 하드코딩 3 케이스 → glob** — `golden_compare.cpp` 의 3 이름 하드코딩(golden_full/no_imgui/skybox) 부분을 `SJH_GOLDEN_REF_DIR` 의 `golden_*.png` 를 순회하는 코드로 교체. `<filesystem>` 사용:
```cpp
#include <filesystem>
// ...
namespace fs = std::filesystem;
std::vector<std::string> GoldenNames()
{
    std::vector<std::string> names;
    for (const auto &e : fs::directory_iterator(SJH_GOLDEN_REF_DIR))
        if (e.path().extension() == ".png" && e.path().stem().string().rfind("golden_", 0) == 0)
            names.push_back(e.path().stem().string());
    std::sort(names.begin(), names.end());
    return names;
}
```
그리고 기존 `TEST_CASE` 3개(golden_full/no_imgui/skybox 고정)를 **동적 케이스**로:
```cpp
TEST_CASE("골든 전수 비교(glob)", "[golden]")
{
    for (const auto &name : GoldenNames())
    {
        DYNAMIC_SECTION("golden: " << name)
        {
            const std::string cap = std::string(SJH_GOLDEN_CAPTURED_DIR) + "/" + name + ".png";
            const std::string ref = std::string(SJH_GOLDEN_REF_DIR) + "/" + name + ".png";
            // 기존 MatchesGolden 로직(cv::imread + absdiff + countNonZero + 5%) 재사용.
            CHECK(CompareGolden(cap, ref)); // 기존 비교 함수명에 맞춤
        }
    }
}
```
> 기존 비교 로직(cv::imread/absdiff/kMaxPixelFraction=0.05)을 `bool CompareGolden(cap, ref)` 헬퍼로 추출해 재사용. 파일명 하드코딩만 제거 - 5% 임계·absdiff 무변경(D2).

- [ ] **Step 2: 빌드 + 현 골든(3장, ref 만 있음) 통과 확인**

Run:
```bash
cmake --build --preset ninja --target golden_compare _MyApp_
ctest --test-dir build_ninja --output-on-failure -R "golden|capture"
```
Expected: capture setup 후 glob 비교 PASS (현재 ref = 커밋된 3장; 캡처본과 일치).

- [ ] **Step 3: 커밋**
```bash
git add test/golden_compare/golden_compare.cpp
git commit -m "[test] : golden_compare glob 데이터주도(하드코딩 3 제거, 5% 유지)"
```

### Task C.2: 신규 참조 골든 커밋 + adequacy

**Files:**
- Create(commit): `test/golden/golden_postfx_*.png` (8), `test/golden/golden_world_*.png` (정리 후 ~3)

- [ ] **Step 1: 캡처본을 참조 골든으로 승격** (육안 승인된 것만)
```bash
cp build_ninja/apps/_MyApp_/test/golden/golden_postfx_*.png test/golden/
cp build_ninja/apps/_MyApp_/test/golden/golden_world_*.png test/golden/   # 빈 큐 제외분만
```

- [ ] **Step 2: 전체 게이트 PASS**

Run:
```bash
cd build_ninja/apps/_MyApp_ && SJH_GOLDEN_CAPTURE=1 ./_MyApp_ ; cd -
ctest --test-dir build_ninja --output-on-failure -R golden
```
Expected: 전 골든(3 + 8 + ~3 ≈ 14) glob 비교 PASS.

- [ ] **Step 3: adequacy - 의도주입 FAIL 확인**
```bash
# 캡처본 하나를 다른 골든으로 덮어써 FAIL 유도(참조 골든 아님 - build dir)
cp test/golden/golden_full.png build_ninja/apps/_MyApp_/test/golden/golden_postfx_1_gamma.png
ctest --test-dir build_ninja --output-on-failure -R golden 2>&1 | grep -i "golden_postfx_1_gamma\|Failed" | head
# capture 재실행으로 복구
cd build_ninja/apps/_MyApp_ && SJH_GOLDEN_CAPTURE=1 ./_MyApp_ ; cd -
```
Expected: golden_postfx_1_gamma FAIL(차이>5%) → 게이트 작동. 복구 후 재PASS.

- [ ] **Step 4: 전체 ctest**

Run: `ctest --test-dir build_ninja --output-on-failure`
Expected: `100% tests passed` (기존 122 + 신규 골든 케이스).

- [ ] **Step 5: 커밋**
```bash
git add test/golden/golden_postfx_1_gamma.png test/golden/golden_postfx_2_sharpening.png \
  test/golden/golden_postfx_3_bloom.png test/golden/golden_postfx_4_fog.png \
  test/golden/golden_postfx_5_grayscale_vignetting.png test/golden/golden_postfx_6_invert.png \
  test/golden/golden_postfx_7_blurring.png test/golden/golden_postfx_8_sobel.png \
  test/golden/golden_world_transparent.png test/golden/golden_world_alphatest.png
git commit -m "[test] : 세분 참조 골든 커밋(PostFX 누적 8 + World 큐별) + glob 게이트"
```
> 실제 커밋 파일명은 GG-B 정리 후 남은 world 골든에 맞춘다. `git add -A` 금지 - 명시 경로만.

**★HANDOFF #GG-C**: glob 게이트 + 전 골든 PASS + 의도주입 FAIL 확인.

---

## Self-Review

**Spec coverage** (spec §GG-0/A/B/C 대비):
- GG-0(캡처 엔진 모듈화) → Task 0.1~0.4 ✓ (frame_capture+pass_capture, main 재배선, Capture 삭제).
- GG-A(PostFX 누적) → Task A.1 ✓.
- GG-B(World 큐별 raw FBO) → Task B.1~B.3 ✓ (queue-filter + WorldPass 전달 + 변형).
- GG-C(비교 glob) → Task C.1~C.2 ✓.
- D1(raw World FBO)=Task B.3 WorldFbo target ✓ / D2(5% 유지)=Task C.1 무변경 ✓ / D3(diagnostics 이관)=Task 0.1~0.3 ✓ / D4(2560×1440 유지)=암묵(fbW/fbH) ✓.

**Placeholder scan**: 없음. 단 실측 필요 3곳 명시(anchor+검증 스텝 포함, TBD 아님): ①diagnostics add_library 소스 형식(Task 0.3) ②WorldPass `mProc` 멤버명(Task B.2) ③golden_compare 기존 비교함수명(Task C.1). 각각 "기존 파일 읽어 맞춘다" + 빌드 검증 스텝으로 게이트.

**Type consistency**: `CaptureVariant`(pass_capture.h) 필드명이 main.cpp(Task 0.4/A.1/B.3) 사용과 일치(outName/passOverride/stopAtPass/worldQueueMin/Max/target). `SetQueueFilter(int,int)` = RenderableProcessor(B.1)·WorldPass(B.2) 동일 시그니처. `CaptureBackbufferToPng`/`CaptureTargetToPng`/`RunCaptureVariants` = frame_capture/pass_capture 선언과 호출 일치.

**리스크**: ①diagnostics↔render STATIC 순환 링크(Task 0.3 Step2 에서 최우선 검증, BLOCKED 조건 명시) ②mSceneFB MSAA 여부(Task 0.1 Step2 flag) ③Skybox 가 WorldPass 큐필터에 안 잡힘(Task B.3 Step2 flag).

## 실행 순서 / HANDOFF
GG-0 → GG-A → GG-B → GG-C. 각 ★HANDOFF 에서 green(빌드+골든+ctest) 확인 후 다음. GG-0 Task 0.3 Step2(순환 링크)가 최대 리스크 - 여기서 먼저 막히면 D3 재논의.
