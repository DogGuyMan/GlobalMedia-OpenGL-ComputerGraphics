# World Text (BitmapFont + TextRenderer) Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 월드 위치에 떠오르며 사라지는 데미지 텍스트를 스폰하는 프리미티브(`SJH::text` Core 모듈 + Client 스포너 + 데모 트리거)를 구현한다.

**Architecture:** Core `SJH::text`(`BitmapFont` BMFont 로더 + `TextRenderer` — 글자마다 child Actor에 기존 `SpriteRenderer` 빌보드 재사용=α). Client `MyApp::Text`(`WorldTextSystem` 폰트 보유) + `Spawns/WorldTextInstance`(`SpawnWorldText` = Actor + TweenPlayable 상승·페이드 + AutoDespawnOnFinish, `VfxInstance` 패턴 동일). 데모는 기존 마우스-피킹 핸들러에 1줄.

**Tech Stack:** C++17, CMake(Ninja/MSVC), 기존 `SJH::sprite`/`SJH::scene`/`SJH::resource_registry`, `tweeny`(Client), spdlog, minogram BMFont(PNG+XML).

**정본 spec:** [doc/superpowers/specs/2026-06-02-world-text-renderer-design.md](2026-06-02-world-text-renderer-design.md)

**검증 방침 (중요):** 사용자 선택 + memory `no_auto_tests` → **단위테스트 없음**. 각 Task는 **compile-green 게이트**, 마지막 Task는 **수동 GUI 검증**. TDD red-green 미적용.

**커밋 규약:** 프로젝트 스타일 `[dev] : <요약>`. **Co-Authored-By 미사용**(memory `next_work_playable`).

---

## File Structure

**신규 (Core `SJH::text`)**
- `src/text/CMakeLists.txt` — `SJH::text` STATIC.
- `src/text/bitmap_font.h` / `.cpp` — BMFont(PNG+XML) 로더 + codepoint→{frame,advance}.
- `src/text/text_renderer.h` / `.cpp` — 글리프 child Actor 조립 Component.

**신규 (Client)**
- `apps/_MyApp_/src/Text/WorldTextSystem.h` / `.cpp` — 폰트 1개 보유 (`MyApp::Text`).
- `apps/_MyApp_/src/Text/CMakeLists.txt` — `MyApp::Text` STATIC.
- `apps/_MyApp_/src/Spawns/WorldTextInstance.h` / `.cpp` — `WorldTextStyle` + `SpawnWorldText`.

**수정**
- `src/CMakeLists.txt` — `add_subdirectory(text)`(T1) + 우산 `SJH::text`(T3).
- `apps/_MyApp_/src/CMakeLists.txt` — `add_subdirectory(Text)` + `MyApp::Text` 링크(T4).
- `apps/_MyApp_/src/Spawns/CMakeLists.txt` — `WorldTextInstance.cpp` + `SJH::text`(T5).
- `<apps>/_MyApp_/src/Manager.h` / `.cpp` — `mWorldText` + `WorldText()` + Init(T4).
- `apps/_MyApp_/main.cpp` — Spawns include + 데모 트리거(T6).

> **include root 주의(크로스플랫폼):** Core `src/text/`(소문자)와 Client `apps/_MyApp_/src/Text/`(대문자)는 **서로 다른 include root** + 파일명 비충돌이라 대소문자 비구분 FS에서도 안전. Core는 `"text/..."`, Client는 `"Text/..."`로 일관 include.

---

### Task 1: Core `SJH::text` 모듈 + `BitmapFont`

**Files:**
- Create: `src/text/CMakeLists.txt`
- Create: `src/text/bitmap_font.h`
- Create: `src/text/bitmap_font.cpp`
- Modify: `src/CMakeLists.txt` (add_subdirectory only)

- [ ] **Step 1: `src/text/CMakeLists.txt` 작성**

```cmake
# SJH::text — BMFont 비트맵 폰트 + 월드 텍스트 렌더 (게임 무관)
# spec doc/superpowers/specs/2026-06-02-world-text-renderer-design.md
add_library(sjh_text STATIC
    bitmap_font.cpp
)
add_library(SJH::text ALIAS sjh_text)

target_include_directories(sjh_text PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# PUBLIC: 헤더에 노출되는 의존
target_link_libraries(sjh_text
    PUBLIC
        SJH::scene             # Component/Actor — TextRenderer 베이스 (T2)
        SJH::sprite            # SpriteRenderer / UniformAtlas — 글리프 렌더 (T2)
        SJH::resource_registry # UniformAtlas 캐시 — BitmapFont
        project_deps           # vmath, gl3w, OpenGL
    PRIVATE
        spdlog                 # bitmap_font.cpp 로드 실패 warn
)
```

- [ ] **Step 2: `src/text/bitmap_font.h` 작성**

```cpp
#ifndef __SJH_TEXT_BITMAP_FONT_H__
#define __SJH_TEXT_BITMAP_FONT_H__

#include <cstdint>
#include <string>
#include <unordered_map>

namespace SJH { class ResourceRegistry; }
namespace SJH::Sprite { class UniformAtlas; }

namespace SJH::Text
{
    /// @brief 한 글리프의 atlas frame index + advance(px).
    struct Glyph { int frameIndex = -1; int xadvance = 0; };

    /// @brief BMFont(AngelCode) PNG+XML 비트맵 폰트 — codepoint→{frame,advance} + 공유 UniformAtlas.
    /// @note  균일 그리드 BMFont 서브셋 전용(frostyfreeze/minogram). 범용 XML 파서 아님.
    ///        내부 UniformAtlas 는 ResourceRegistry 캐시(비소유) — 사이클 회피(spec §2.2).
    class BitmapFont
    {
      public:
        BitmapFont() = default;

        /// @brief PNG+XML 로드. 내부 UniformAtlas 는 reg 캐시(Find 우선). 실패 시 빈 폰트(IsValid()=false).
        static BitmapFont LoadFromBMFont(SJH::ResourceRegistry& reg, const std::string& key,
                                         const std::string& pngPath, const std::string& xmlPath);

        const Glyph* Find(std::uint32_t codepoint) const;
        int   FrameOf(std::uint32_t codepoint) const;    // 없으면 '?'→space fallback, 둘 다 없으면 -1(skip)
        int   AdvanceOf(std::uint32_t codepoint) const;  // px (fallback 동일)
        SJH::Sprite::UniformAtlas* GetAtlas() const { return mAtlas; }
        int   LineHeight() const { return mLineHeight; }
        int   CellW() const { return mCellW; }
        int   CellH() const { return mCellH; }
        bool  IsValid() const { return mAtlas != nullptr && !mGlyphs.empty(); }

      private:
        SJH::Sprite::UniformAtlas* mAtlas = nullptr;   // 비소유 (registry 보유)
        std::unordered_map<std::uint32_t, Glyph> mGlyphs;
        int mLineHeight = 0, mCellW = 0, mCellH = 0;
    };
}

#endif // __SJH_TEXT_BITMAP_FONT_H__
```

- [ ] **Step 3: `src/text/bitmap_font.cpp` 작성**

```cpp
#include "text/bitmap_font.h"

#include "sprite/uniform_atlas.h"
#include "resource_registry/resource_registry.h"

#include <<spdlog>/spdlog.h>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

namespace SJH::Text
{
    namespace
    {
        // BMFont 속성 추출 — `name="123"` 의 정수. atoi 가 닫는 따옴표에서 멈춤.
        int AttrInt(const std::string& s, const char* name, int def = 0)
        {
            const std::string key = std::string(name) + "=\"";
            const auto p = s.find(key);
            if (p == std::string::npos) return def;
            return std::atoi(s.c_str() + p + key.size());
        }
    }

    BitmapFont BitmapFont::LoadFromBMFont(SJH::ResourceRegistry& reg, const std::string& key,
                                          const std::string& pngPath, const std::string& xmlPath)
    {
        BitmapFont font;

        std::ifstream in(xmlPath, std::ios::binary);   // 바이너리 모드 (크로스플랫폼)
        if (!in)
        {
            spdlog::warn("[BitmapFont] XML 열기 실패: {}", xmlPath);
            return font;
        }
        std::stringstream buf;
        buf << in.rdbuf();
        const std::string content = buf.str();

        struct Raw { std::uint32_t id; int x, y, w, h, adv; };
        std::vector<Raw> chars;
        int scaleW = 0, scaleH = 0;

        std::istringstream lines(content);
        std::string line;
        while (std::getline(lines, line))
        {
            if (line.find("<common") != std::string::npos)
            {
                scaleW          = AttrInt(line, "scaleW");
                scaleH          = AttrInt(line, "scaleH");
                font.mLineHeight = AttrInt(line, "lineHeight");
            }
            else if (line.find("<char ") != std::string::npos)
            {
                Raw r;
                r.id  = static_cast<std::uint32_t>(AttrInt(line, "id"));
                r.x   = AttrInt(line, "x");
                r.y   = AttrInt(line, "y");
                r.w   = AttrInt(line, "width");
                r.h   = AttrInt(line, "height");
                r.adv = AttrInt(line, "xadvance");
                chars.push_back(r);
            }
        }

        if (chars.empty() || scaleW <= 0 || scaleH <= 0)
        {
            spdlog::warn("[BitmapFont] 파싱 실패(char 0 또는 scale 0): {}", xmlPath);
            return font;
        }
        font.mCellW = chars[0].w;
        font.mCellH = chars[0].h;
        if (font.mCellW <= 0 || font.mCellH <= 0)
        {
            spdlog::warn("[BitmapFont] cell 크기 0: {}", xmlPath);
            return font;
        }
        const int cols = scaleW / font.mCellW;
        const int rows = scaleH / font.mCellH;

        // UniformAtlas — Find 우선(중복키 공유), 없으면 Create
        font.mAtlas = reg.FindUniformAtlas(key);
        if (!font.mAtlas) font.mAtlas = reg.CreateUniformAtlas(key, pngPath, cols, rows);
        if (!font.mAtlas)
        {
            spdlog::warn("[BitmapFont] UniformAtlas 생성 실패: {} ({}x{})", pngPath, cols, rows);
            return font;
        }

        for (const auto& r : chars)
        {
            const int frame = (r.y / font.mCellH) * cols + (r.x / font.mCellW);
            font.mGlyphs[r.id] = Glyph{frame, r.adv};
        }
        return font;
    }

    const Glyph* BitmapFont::Find(std::uint32_t cp) const
    {
        auto it = mGlyphs.find(cp);
        return it == mGlyphs.end() ? nullptr : &it->second;
    }

    int BitmapFont::FrameOf(std::uint32_t cp) const
    {
        if (auto* g = Find(cp)) return g->frameIndex;
        if (auto* q = Find('?')) return q->frameIndex;   // fallback 1
        if (auto* s = Find(' ')) return s->frameIndex;   // fallback 2
        return -1;                                        // skip
    }

    int BitmapFont::AdvanceOf(std::uint32_t cp) const
    {
        if (auto* g = Find(cp)) return g->xadvance;
        if (auto* q = Find('?')) return q->xadvance;
        if (auto* s = Find(' ')) return s->xadvance;
        return mCellW;                                    // 최후 fallback
    }
}
```

- [ ] **Step 4: `src/CMakeLists.txt` 에 `add_subdirectory(text)` 추가**

`add_subdirectory(timer)` (16번째 모듈) 줄 **바로 다음**에 추가:

```cmake
add_subdirectory(timer)     # <- 추가 (SJH::timer, spec 2026-06-01)
add_subdirectory(text)      # <- 추가 (SJH::text, spec 2026-06-02 World Text)
```

- [ ] **Step 5: Core 모듈 컴파일 확인 (compile-green 게이트)**

Run:
```bash
cmake --preset ninja && cmake --build --preset ninja --target sjh_text
```
Expected: `sjh_text` 링크 성공 (`[N/N] Linking CXX static library .../libsjh_text.a`). 에러 0.

- [ ] **Step 6: 커밋**

```bash
git add src/text/CMakeLists.txt src/text/bitmap_font.h src/text/bitmap_font.cpp src/CMakeLists.txt
git commit -m "[dev] : World Text — Core SJH::text 모듈 + BitmapFont(BMFont 로더)"
```

---

### Task 2: `TextRenderer` (글리프 child 조립)

**Files:**
- Create: `src/text/text_renderer.h`
- Create: `src/text/text_renderer.cpp`
- Modify: `src/text/CMakeLists.txt` (text_renderer.cpp 추가)

- [ ] **Step 1: `src/text/text_renderer.h` 작성**

```cpp
#ifndef __SJH_TEXT_TEXT_RENDERER_H__
#define __SJH_TEXT_TEXT_RENDERER_H__

#include "scene/actor.h"   // Component 베이스 + Actor
#include <vmath.h>
#include <string>
#include <vector>

namespace SJH::Text
{
    class BitmapFont;

    /// @brief α — 문자열을 글자마다 child Actor(SpriteRenderer 빌보드)로 펼치는 Component.
    /// @details center 정렬 + 하단중앙 앵커 + 월드 X 배치(spec §3). TweenPlayable 비의존
    ///          (SetAlpha/SetColor 만 노출, 구동은 Client).
    class TextRenderer : public SJH::Scene::Component
    {
      public:
        explicit TextRenderer(const BitmapFont* font) : mFont(font) {}

        void OnEnter() override {}
        void OnExit()  override { mGlyphs.clear(); }   // child 는 owner 소멸 시 자동 파괴
        void Update(float) override {}                  // 정적 — 틱 없음

        void SetText(const std::string& s);             // 글리프 child 재구성(Rebuild)
        void SetColor(const vmath::vec4& rgba);          // 모든 글리프 tint
        void SetAlpha(float a);                          // fade — 모든 글리프 tint.a
        void SetCharHeight(float worldH) { mCharHeight = worldH; }  // SetText 전 설정

      private:
        void Rebuild();

        const BitmapFont* mFont = nullptr;
        std::string       mText;
        float             mCharHeight = 0.5f;
        vmath::vec4       mColor = vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        std::vector<SJH::Scene::Actor*> mGlyphs;         // 비소유 (owner children 소유)
    };
}

#endif // __SJH_TEXT_TEXT_RENDERER_H__
```

- [ ] **Step 2: `src/text/text_renderer.cpp` 작성**

```cpp
#include "text/text_renderer.h"

#include "text/bitmap_font.h"
#include "sprite/sprite_component.h"   // SJH::Sprite::SpriteRenderer
#include "sprite/uniform_atlas.h"

#include <memory>

namespace SJH::Text
{
    void TextRenderer::SetText(const std::string& s)
    {
        mText = s;
        Rebuild();
    }

    void TextRenderer::SetColor(const vmath::vec4& rgba)
    {
        mColor = rgba;
        for (auto* g : mGlyphs)
            if (auto* sr = g->GetComponent<SJH::Sprite::SpriteRenderer>())
                sr->tint = mColor;
    }

    void TextRenderer::SetAlpha(float a)
    {
        mColor[3] = a;
        for (auto* g : mGlyphs)
            if (auto* sr = g->GetComponent<SJH::Sprite::SpriteRenderer>())
                sr->tint[3] = a;
    }

    void TextRenderer::Rebuild()
    {
        auto* owner = GetOwner();
        if (!owner) return;

        // 기존 글리프 제거 (re-SetText) — 스폰/SetText 시점(Update 트리 순회 밖, iterator 안전)
        for (auto* g : mGlyphs) owner->RemoveChild(g);
        mGlyphs.clear();

        if (!mFont || !mFont->GetAtlas() || mFont->CellH() <= 0) return;

        const float worldPerPx = mCharHeight / static_cast<float>(mFont->CellH());
        const float glyphW      = static_cast<float>(mFont->CellW()) * worldPerPx;

        float totalW = 0.0f;
        for (unsigned char c : mText)
            totalW += static_cast<float>(mFont->AdvanceOf(c)) * worldPerPx;
        float penX = -totalW * 0.5f;   // center

        for (unsigned char c : mText)
        {
            const int   frame = mFont->FrameOf(c);
            const float advW  = static_cast<float>(mFont->AdvanceOf(c)) * worldPerPx;
            if (frame >= 0)
            {
                auto* glyph = owner->AddChild(std::make_unique<SJH::Scene::Actor>("glyph"));
                auto& t = glyph->GetTransform();
                t.Translate = vmath::vec3(penX + glyphW * 0.5f, mCharHeight * 0.5f, 0.0f); // 하단중앙
                t.Scale     = vmath::vec3(glyphW, mCharHeight, 1.0f);                       // 빌보드 sx/sy
                auto* sr = glyph->AddComponent<SJH::Sprite::SpriteRenderer>(mFont->GetAtlas());
                sr->frameIdx = frame;
                sr->tint     = mColor;
                mGlyphs.push_back(glyph);
            }
            penX += advW;
        }
    }
}
```

- [ ] **Step 3: `src/text/CMakeLists.txt` 에 `text_renderer.cpp` 추가**

```cmake
add_library(sjh_text STATIC
    bitmap_font.cpp
    text_renderer.cpp      # <- 추가 (T2)
)
```

- [ ] **Step 4: Core 모듈 컴파일 확인 (compile-green 게이트)**

Run:
```bash
cmake --preset ninja && cmake --build --preset ninja --target sjh_text
```
Expected: `sjh_text` 링크 성공. 에러 0.

- [ ] **Step 5: 커밋**

```bash
git add src/text/text_renderer.h src/text/text_renderer.cpp src/text/CMakeLists.txt
git commit -m "[dev] : World Text — TextRenderer(글리프 child SpriteRenderer 조립)"
```

---

### Task 3: `SJH::engine` 우산 배선 + 전체 빌드 그린

**Files:**
- Modify: `src/CMakeLists.txt` (우산 INTERFACE 에 `SJH::text`)

- [ ] **Step 1: `src/CMakeLists.txt` 우산 타겟에 `SJH::text` 추가**

`target_link_libraries(sjhopengl_engine INTERFACE ...)` 의 `SJH::timer` 줄 **다음**에 추가:

```cmake
    SJH::timer              # 16 모듈 — 게임플레이 타이머 (spec 2026-06-01)
    SJH::text               # 17 모듈 — World Text (spec 2026-06-02)
)
```

- [ ] **Step 2: 전체 데모 빌드 확인 (compile-green 게이트)**

Run:
```bash
cmake --preset ninja && cmake --build --preset ninja --target _MyApp_
```
Expected: `_MyApp_` 링크 성공 (SJH::text 가 우산에 합류해도 기존 동작 무변경 — consumer 0). 에러 0.

- [ ] **Step 3: 커밋**

```bash
git add src/CMakeLists.txt
git commit -m "[dev] : World Text — SJH::engine 우산에 SJH::text 합류 (16→17 모듈)"
```

---

### Task 4: Client `MyApp::Text` `WorldTextSystem` + Manager 배선

**Files:**
- Create: `apps/_MyApp_/src/Text/WorldTextSystem.h`
- Create: `apps/_MyApp_/src/Text/WorldTextSystem.cpp`
- Create: `apps/_MyApp_/src/Text/CMakeLists.txt`
- Modify: `apps/_MyApp_/src/CMakeLists.txt` (add_subdirectory(Text) + Manager/Client 링크)
- Modify: `<apps>/_MyApp_/src/Manager.h`
- Modify: `<apps>/_MyApp_/src/Manager.cpp`

- [ ] **Step 1: `apps/_MyApp_/src/Text/WorldTextSystem.h` 작성**

```cpp
#ifndef __TOPDOWNSHOOTER_TEXT_WORLD_TEXT_SYSTEM_H__
#define __TOPDOWNSHOOTER_TEXT_WORLD_TEXT_SYSTEM_H__

#include "text/bitmap_font.h"

namespace SJH { class ResourceRegistry; }

namespace TopdownShooter::Text
{
    /// @brief minogram BMFont 1개를 보유하는 얇은 시스템 (VFX/Audio 시스템 평행). Manager 보유.
    class WorldTextSystem
    {
      public:
        void Init(SJH::ResourceRegistry& reg);
        SJH::Text::BitmapFont* GetFont() { return mLoaded ? &mFont : nullptr; }

      private:
        SJH::Text::BitmapFont mFont;
        bool mLoaded = false;
    };
}

#endif // __TOPDOWNSHOOTER_TEXT_WORLD_TEXT_SYSTEM_H__
```

- [ ] **Step 2: `apps/_MyApp_/src/Text/WorldTextSystem.cpp` 작성**

```cpp
#include "apps/_MyApp_/src/Text/WorldTextSystem.h"

#include "resource_registry/resource_registry.h"

namespace TopdownShooter::Text
{
    void WorldTextSystem::Init(SJH::ResourceRegistry& reg)
    {
        mFont = SJH::Text::BitmapFont::LoadFromBMFont(
            reg, "minogram_6x10",
            "resources/font/minogram_6x10.png",
            "resources/font/minogram_6x10.xml");
        mLoaded = mFont.IsValid();
    }
}
```

- [ ] **Step 3: `apps/_MyApp_/src/Text/CMakeLists.txt` 작성**

```cmake
# MyApp::Text — WorldTextSystem (minogram BMFont 보유). Manager 가 소유.
add_library(myapp_worldtext STATIC
    WorldTextSystem.cpp
)
add_library(MyApp::Text ALIAS myapp_worldtext)

target_include_directories(myapp_worldtext
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(myapp_worldtext
    PUBLIC
        SJH::text              # BitmapFont (헤더 노출)
        SJH::resource_registry # ResourceRegistry (Init 인자)
)
target_compile_features(myapp_worldtext PUBLIC cxx_std_17)
```

- [ ] **Step 4: `apps/_MyApp_/src/CMakeLists.txt` — Text 서브디렉토리 + 링크**

`add_subdirectory(Tween)` 다음(= Manager 정의보다 앞)에 추가:

```cmake
add_subdirectory(Tween)
add_subdirectory(Text)        # <- 추가 (MyApp::Text — Manager 가 의존, Manager 앞)
```

그리고 `MyApp::Manager` 링크 그룹(`MyApp::Audio`/`VFX`/`Physics`)에 추가:

```cmake
        MyApp::Audio
        MyApp::VFX
        MyApp::Physics
        MyApp::Text       # <- 추가 (WorldTextSystem)
```

그리고 `MyApp::Client` 우산 링크 목록에 추가:

```cmake
    MyApp::Tween
    MyApp::Text       # <- 추가 (WorldTextSystem)
```

- [ ] **Step 5: `<apps>/_MyApp_/src/Manager.h` — 멤버 + 접근자 + include**

`#include "apps/_MyApp_/src/VFX/VFXSystem.h"` 다음에 include 추가:

```cpp
#include "apps/_MyApp_/src/VFX/VFXSystem.h"
#include "apps/_MyApp_/src/Text/WorldTextSystem.h"   // <- 추가
```

`SceneRenderer()` 접근자 다음에 추가:

```cpp
		SJH::SceneRenderer  &SceneRenderer() { return mScenesRender; }
		Text::WorldTextSystem &WorldText() { return mWorldText; }   // <- 추가
```

`mPhysics` 멤버 다음에 추가:

```cpp
		Physics::PhysicsSystem  mPhysics;
		Text::WorldTextSystem   mWorldText;   // <- 추가
```

- [ ] **Step 6: `<apps>/_MyApp_/src/Manager.cpp` — Init 에서 폰트 로드**

`#include <<spdlog>/spdlog.h>` 다음에 include 추가:

```cpp
#include <<spdlog>/spdlog.h>
#include "resource_registry/resource_registry.h"   // <- 추가 (ResourceRegistry::Get)
```

`Manager::Init()` 본문에 `mPhysics.Init();` 다음 추가:

```cpp
		mPhysics.Init();
		mWorldText.Init(SJH::ResourceRegistry::Get());   // <- 추가 (minogram BMFont 로드)
		spdlog::info("[Director] init OK (Audio + VFX + Physics + WorldText)");
```

(기존 `spdlog::info("[Director] init OK (Audio + VFX + Physics)");` 줄을 위 문자열로 교체.)

- [ ] **Step 7: 빌드 확인 (compile-green 게이트)**

Run:
```bash
cmake --preset ninja && cmake --build --preset ninja --target _MyApp_
```
Expected: `_MyApp_` 링크 성공. `MyApp::Text` 가 Manager 에 합류. 에러 0.

- [ ] **Step 8: 커밋**

```bash
git add apps/_MyApp_/src/Text/ apps/_MyApp_/src/CMakeLists.txt <apps>/_MyApp_/src/Manager.h <apps>/_MyApp_/src/Manager.cpp
git commit -m "[dev] : World Text — MyApp::Text WorldTextSystem + Manager 배선"
```

---

### Task 5: `Spawns/WorldTextInstance` (`SpawnWorldText`)

**Files:**
- Create: `apps/_MyApp_/src/Spawns/WorldTextInstance.h`
- Create: `apps/_MyApp_/src/Spawns/WorldTextInstance.cpp`
- Modify: `apps/_MyApp_/src/Spawns/CMakeLists.txt`

- [ ] **Step 1: `apps/_MyApp_/src/Spawns/WorldTextInstance.h` 작성**

```cpp
#ifndef __TOPDOWNSHOOTER_SPAWNS_WORLD_TEXT_INSTANCE_H__
#define __TOPDOWNSHOOTER_SPAWNS_WORLD_TEXT_INSTANCE_H__

#include <vmath.h>
#include <string>

namespace SJH
{
    namespace Scene { class Actor; }
    namespace Text  { class BitmapFont; }
}

namespace TopdownShooter::Spawns
{
    /// @brief 월드 텍스트 외형/모션 파라미터 (spec §4.2).
    struct WorldTextStyle
    {
        vmath::vec4 color       = vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        float       charHeight  = 0.5f;    // 월드 단위 (글리프 높이)
        float       riseHeight  = 0.7f;    // 월드 +Y 상승량
        float       durationSec = 0.9f;    // 수명
        float       fadeStart   = 0.45f;   // 진행률 0..1 — 이 지점부터 alpha 1→0
    };

    /// @brief 월드 위치에 떠오르며 사라지는 텍스트 spawn (VfxInstance 패턴). font==nullptr → no-op.
    ///        Actor + TextRenderer + TweenPlayable(상승·페이드) + AutoDespawnOnFinish + Play.
    ///        despawn 은 기존 SweepFinishedChildren(fxParent) 가 수행.
    void SpawnWorldText(SJH::Scene::Actor& fxParent, SJH::Text::BitmapFont* font,
                        const vmath::vec3& worldPos, const std::string& text,
                        const WorldTextStyle& style);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_WORLD_TEXT_INSTANCE_H__
```

- [ ] **Step 2: `apps/_MyApp_/src/Spawns/WorldTextInstance.cpp` 작성**

```cpp
#include "apps/_MyApp_/src/Spawns/WorldTextInstance.h"

#include "apps/_MyApp_/src/Spawns/AutoDespawnOnFinish.h"
#include "apps/_MyApp_/src/Tween/TweenPlayable.h"
#include "text/text_renderer.h"
#include "scene/actor.h"

#include <cstdint>
#include <memory>

namespace TopdownShooter::Spawns
{
    void SpawnWorldText(SJH::Scene::Actor& fxParent, SJH::Text::BitmapFont* font,
                        const vmath::vec3& worldPos, const std::string& text,
                        const WorldTextStyle& style)
    {
        if (!font) return;   // 폰트 미존재 — no-op (VfxInstance 동일)

        auto* a = fxParent.AddChild(std::make_unique<SJH::Scene::Actor>("WorldText"));
        a->GetTransform().Translate = worldPos;        // 앵커 = 하단중앙

        auto* tr = a->AddComponent<SJH::Text::TextRenderer>(font);
        tr->SetCharHeight(style.charHeight);           // SetText 전 설정(크기 bake)
        tr->SetColor(style.color);
        tr->SetText(text);                              // 글리프 child 빌드

        // 단일 progress 트윈 0→1 — 상승+페이드 동시 (one-shot). tweeny step(int32_t ms) 강제.
        auto tween = tweeny::from(0.0f).to(1.0f)
                         .during(static_cast<std::int32_t>(style.durationSec * 1000.0f));
        const float baseY = worldPos[1];
        auto* tw = a->AddComponent<Tween::TweenPlayable<float>>(
            std::move(tween),
            [a, tr, baseY, style](float t) {
                const float e = 1.0f - (1.0f - t) * (1.0f - t);            // easeOutQuad (팝→감속)
                a->GetTransform().Translate[1] = baseY + style.riseHeight * e;
                const float alpha = (t < style.fadeStart)
                                        ? 1.0f
                                        : 1.0f - (t - style.fadeStart) / (1.0f - style.fadeStart);
                tr->SetAlpha(alpha);                                       // 후반 페이드
            });
        tw->SetIsLoop(false);                           // t≥1 → finished_

        a->AddComponent<AutoDespawnOnFinish>(tw);       // 종료 감지 (기존 sweeper 가 RemoveChild)
        tw->Play();
    }
}
```

- [ ] **Step 3: `apps/_MyApp_/src/Spawns/CMakeLists.txt` — 소스 + 링크 추가**

`add_library(myapp_spawns STATIC ...)` 목록에 추가:

```cmake
    CombatSequences.cpp
    AmbientSequences.cpp
    WorldTextInstance.cpp      # <- 추가 (T5)
)
```

PUBLIC 링크 그룹에 `SJH::text` 추가:

```cmake
    PUBLIC
        SJH::scene
        SJH::playable
        SJH::text          # <- 추가 (TextRenderer — WorldTextInstance.cpp)
        MyApp::Entity
        MyApp::Audio
        MyApp::VFX
        MyApp::Tween
```

- [ ] **Step 4: 빌드 확인 (compile-green 게이트)**

Run:
```bash
cmake --preset ninja && cmake --build --preset ninja --target _MyApp_
```
Expected: `_MyApp_` 링크 성공. `SpawnWorldText` 가 빌드됨(아직 미호출). 에러 0.

- [ ] **Step 5: 커밋**

```bash
git add apps/_MyApp_/src/Spawns/WorldTextInstance.h apps/_MyApp_/src/Spawns/WorldTextInstance.cpp apps/_MyApp_/src/Spawns/CMakeLists.txt
git commit -m "[dev] : World Text — SpawnWorldText(트윈 상승·페이드 + 자동 despawn)"
```

---

### Task 6: 데모 트리거 (main.cpp) + 수동 GUI 검증

**Files:**
- Modify: `apps/_MyApp_/main.cpp`

- [ ] **Step 1: main.cpp — Spawns include 추가**

`#include "apps/_MyApp_/src/Spawns/VfxInstance.h"` (line 28 부근) 다음에 추가:

```cpp
#include "apps/_MyApp_/src/Spawns/OneShotSweeper.h"
#include "apps/_MyApp_/src/Spawns/VfxInstance.h"
#include "apps/_MyApp_/src/Spawns/WorldTextInstance.h"   // <- 추가 (데모 트리거)
```

- [ ] **Step 2: main.cpp — `SetGroundClickCallback` 람다에 데모 스폰 추가**

기존 람다(main.cpp:311 부근) 본문을 아래로 교체 (VFX 스폰 블록 유지 + WorldText 추가):

```cpp
					pc->SetGroundClickCallback([this](const vmath::vec3 &p) {
						// PlayerController 의 마우스→Ground raycast 결과(p)에 선택 이펙트를 단발 스폰.
						if (mVfxLayer && mFxRoot)
							if (auto *fx = mVfxLayer->GetSelectedEffect())
								TopdownShooter::Spawns::SpawnVfxInstance(
								    *mFxRoot, &TopdownShooter::Manager::Get().VFX(), fx, p);

						// 데모 — 클릭 지점에 데미지 텍스트 (World Text 검증)
						if (mFxRoot)
							if (auto *font = TopdownShooter::Manager::Get().WorldText().GetFont())
							{
								static int demoN = 0;
								const std::string dmg = "-" + std::to_string(10 + (demoN++ % 90));
								TopdownShooter::Spawns::SpawnWorldText(
								    *mFxRoot, font, p, dmg,
								    TopdownShooter::Spawns::WorldTextStyle{});
							}
					});
```

(`<string>` 은 main.cpp 가 이미 STL 다수 사용 — 미포함 시 `#include <string>` 추가.)

- [ ] **Step 3: 빌드 (compile-green 게이트)**

Run:
```bash
cmake --preset ninja && cmake --build --preset ninja --target _MyApp_
```
Expected: `_MyApp_` 링크 성공. 에러 0.

- [ ] **Step 4: 수동 GUI 검증 (run + observe)**

Run:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
확인 체크리스트:
- [ ] 월드를 **좌클릭** → 클릭 지점에 데미지 숫자(`-10`, `-11`, …)가 나타난다.
- [ ] 텍스트가 **위로 떠오르며**(상승) **후반부에 서서히 사라진다**(페이드).
- [ ] 약 **0.9초 후 사라진다**(despawn) — 화면에 잔류 없음.
- [ ] 여러 번 클릭해도 정상(다수 동시 표시 → 각자 독립 despawn).
- [ ] 숫자 글자가 **또렷**(픽셀아트 NEAREST), 가로로 가운데 정렬.

(크기/상승량이 어색하면 `WorldTextStyle{}` 기본값 `charHeight`/`riseHeight` 를 게임 스케일에 맞게 1차 튜닝 — spec §10.)

- [ ] **Step 5: (선택) 누수 확인**

Run (macOS):
```bash
sh <shell>/CMakeExecute.sh debug _MyApp_ leaks
```
Expected: despawn 후 글리프 Actor 회수 — WorldText 관련 누수 0.

- [ ] **Step 6: 커밋**

```bash
git add apps/_MyApp_/main.cpp
git commit -m "[dev] : World Text — 데모 트리거(마우스 클릭 지점 데미지 텍스트)"
```

---

## Self-Review (작성자 체크)

**1. Spec coverage:**
- §2 BitmapFont → T1 ✓ / §3 TextRenderer → T2 ✓ / §4.3 WorldTextSystem → T4 ✓ / §4.2 WorldTextStyle + §4.4 SpawnWorldText → T5 ✓ / §4.6 데모 트리거 → T6 ✓ / §5 CMake 배선 → T1·T3·T4·T5 ✓ / §7 검증 → T6 ✓.
- 미존재 글리프 '?' fallback(§2.5) → T1 `FrameOf` ✓. per-glyph advance(§2.6) → T1 `AdvanceOf` ✓. 하단중앙 앵커·월드 X(§3.3) → T2 `Rebuild` ✓. 단일 progress 트윈·easeOut·후반 fade(§4.4) → T5 ✓. 기존 sweeper 재사용(§4.5) → T5 `AutoDespawnOnFinish` + 기존 main.cpp:354 ✓.

**2. Placeholder scan:** TODO/TBD/"적절히"류 없음. 모든 코드/명령/경로 구체. ✓

**3. Type consistency:**
- `Transform.Translate`(실제 필드, `.Position` 아님) — T2·T5 일관 ✓.
- `SJH::Sprite::SpriteRenderer` public `frameIdx`/`tint`(vec4) — T2 일관 ✓.
- `BitmapFont::{FrameOf,AdvanceOf,GetAtlas,CellW,CellH,IsValid}` — T1 정의 = T2·T4 사용 일치 ✓.
- `TextRenderer::{SetText,SetColor,SetAlpha,SetCharHeight}` — T2 정의 = T5 사용 일치 ✓.
- `WorldTextSystem::{Init,GetFont}` — T4 정의 = T6 사용 일치 ✓.
- `SpawnWorldText(Actor&, BitmapFont*, vec3, string, WorldTextStyle)` — T5 정의 = T6 호출 일치 ✓.
- `Component` 순수가상 3종(OnEnter/OnExit/Update) — T2 모두 구현 ✓.
- `AddComponent<T>` 타입당 1개 — TextRenderer/TweenPlayable<float>/AutoDespawnOnFinish 서로 다른 타입, 글리프는 child Actor(각 SpriteRenderer 1개) ✓.

---

## 의존 순서 / 게이트
```
T1 (BitmapFont) → T2 (TextRenderer) → T3 (우산) ── Core 그린
                                          └→ T4 (WorldTextSystem+Manager) → T5 (SpawnWorldText) → T6 (데모+GUI 검증)
```
- **컴파일 게이트**: T1·T2 = `sjh_text`, T3·T4·T5·T6 = `_MyApp_`.
- **수동 검증 게이트**: T6 Step 4 (run + click).
