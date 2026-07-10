# M1 SJH::sprite 모듈 신설 + 빌보드 1장 정적 표시 Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** SJH::sprite 코어 모듈 신설 (UniformAtlas + SpriteComponent + ComputeUVRect) + 빌보드 atlas 셰이더 (GLSL 410) + apps/_MyApp_/ 재활성화 후 화면에 `player_atlas.png` 첫 frame 의 빌보드 1장이 정적 표시되는 상태 도달.

**Architecture:** `SJH::sprite` 모듈에 두 가지를 둠 — (1) **`ComputeUVRect`** free function (math 만, GL 무관 — 단위 테스트가 GL fixture 없이 검증 가능) + **`UniformAtlas`** RAII 클래스 (`LoadFromPNG` 가 `GL_TEXTURE_2D` 생성, `GetUVRect` 가 ComputeUVRect 위임), (2) **`SpriteComponent`** (`SJH::Scene::Component` 파생 POD-ish — atlas\*, frameIdx, size, tint, flipX 만 보유, *시간 갱신 책임 없음*). `apps/_MyApp_/main.cpp` 가 `sb7::application` 의 `init()` override 로 GL 4.1 강제, `startup()` 에서 atlas + 셰이더 + VBO 준비, `render()` 에서 단일 `glDrawArrays(GL_TRIANGLES, 0, 6)` 호출. Actor/Playable/Tween 등은 *M2 이후* — M1 은 *직접 GL 호출* 로 최소 작동 확인.

**Tech Stack:** C++17, CMake 3.14+, OpenGL 4.1 Core / GLSL 410, gl3w, vmath (sb7 vendored), stb_image (`SJH::sprite` 가 정의 책임 흡수), sb7::application, Catch2 v3.

---

## File Structure (Locked-in Decomposition)

### 생성 (Create)

| 파일 | 책임 |
|---|---|
| `src/sprite/CMakeLists.txt` | `sjh_sprite` STATIC + `SJH::sprite` ALIAS. PUBLIC `SJH::scene` + `project_deps` + `stb_extra`, PRIVATE `spdlog` |
| `src/sprite/uniform_atlas.h` | `UniformAtlas` 클래스 선언 + `ComputeUVRect` 자유함수 선언 |
| `src/sprite/uniform_atlas.cpp` | `ComputeUVRect` 정의 + `UniformAtlas` 정의 (`SJH::Image::Load` + `SJH::Texture::CreateTexture` 위임). **stb_image 직접 사용 0** (Task 4 fixup 후 정정 — `STB_IMAGE_IMPLEMENTATION` 은 `src/texture/image.cpp` 가 단일 owner) |
| `src/sprite/sprite_component.h` | `SpriteComponent` 클래스 (Component 파생, OnEnter/OnExit/Update 빈 override) |
| `<apps>/_MyApp_/resources/shaders/billboard_atlas.vert` | GLSL 410 vertex shader — 카메라 right/up 벡터로 빌보드 quad + `u_uvRect` atlas UV 매핑 |
| `<apps>/_MyApp_/resources/shaders/billboard_atlas.frag` | GLSL 410 fragment shader — atlas sampling + alpha test discard + tint 곱셈 |
| `apps/_MyApp_/resources/sprites/player_atlas.png` | 256×256 RGBA 4×4 그리드 placeholder atlas (실제 게임 sprite 는 후속 마일스톤에 교체) |
| `<test>/test_uniform_atlas.cpp` | Catch2 단위 테스트 — `ComputeUVRect` math 검증 (GL 불요) |

### 수정 (Modify)

| 파일 | 변경 내용 |
|---|---|
| `src/CMakeLists.txt:1-30` | `add_subdirectory(sprite)` 추가 + `sjhopengl_engine` INTERFACE 의 link 목록에 `SJH::sprite` 1행 추가 |
| `apps/CMakeLists.txt:1` | `# add_subdirectory(_MyApp_)` 주석 해제 (1 → 활성) |
| `apps/_MyApp_/CMakeLists.txt:10` | `target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps game_deps)` 에 `SJH::engine` 추가 |
| `apps/_MyApp_/main.cpp` | 의존성 등록 검증용 임시 코드 *완전 폐기* — sb7::application 상속 + init/startup/render 본문으로 재작성 |
| `test/CMakeLists.txt:?` | `test_uniform_atlas` executable + `catch_discover_tests` 등록 |

### 검증 (Verify, 코드 변경 없음)

| 항목 | 방법 |
|---|---|
| 빌드 성공 | `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_` 0 에러 |
| 단위 테스트 | `cmake --preset ninja -DENABLE_TESTING=ON && ctest --test-dir build_ninja -R test_uniform_atlas -V` 모두 PASS |
| 시각 검증 | `cd build_ninja/apps/_MyApp_ && ./_MyApp_` 실행 → 화면 중앙에 player_atlas frame 0 의 64×64 빌보드 1장 표시 |

---

## Task 1: `SJH::sprite` 모듈 — `ComputeUVRect` 자유함수 + 단위 테스트 (TDD, GL 불요)

**Files:**
- Create: `src/sprite/CMakeLists.txt`
- Create: `src/sprite/uniform_atlas.h`
- Create: `src/sprite/uniform_atlas.cpp`
- Create: `<test>/test_uniform_atlas.cpp`
- Modify: `test/CMakeLists.txt` (test_uniform_atlas 등록)

- [ ] **Step 1: 실패하는 단위 테스트 작성**

`<test>/test_uniform_atlas.cpp` 신규 생성:

```cpp
#include "sprite/uniform_atlas.h"
#include <<catch2>/catch_test_macros.hpp>
#include <<catch2>/catch_approx.hpp>

using SJH::Sprite::ComputeUVRect;
using Catch::Approx;

TEST_CASE("ComputeUVRect — 4x4 grid, 256x256 atlas, 64px tile", "[sprite][uniform_atlas]")
{
    // 4 columns × 4 rows = 16 frames
    SECTION("frame 0 (col=0, row=0) → (0, 0, 0.25, 0.25)") {
        auto uv = ComputeUVRect(0, /*cols=*/4, /*tile=*/64, /*atlasW=*/256, /*atlasH=*/256);
        REQUIRE(uv[0] == Approx(0.0f));
        REQUIRE(uv[1] == Approx(0.0f));
        REQUIRE(uv[2] == Approx(0.25f));
        REQUIRE(uv[3] == Approx(0.25f));
    }
    SECTION("frame 5 (col=1, row=1) → (0.25, 0.25, 0.25, 0.25)") {
        auto uv = ComputeUVRect(5, 4, 64, 256, 256);
        REQUIRE(uv[0] == Approx(0.25f));
        REQUIRE(uv[1] == Approx(0.25f));
        REQUIRE(uv[2] == Approx(0.25f));
        REQUIRE(uv[3] == Approx(0.25f));
    }
    SECTION("frame 15 (col=3, row=3) → (0.75, 0.75, 0.25, 0.25)") {
        auto uv = ComputeUVRect(15, 4, 64, 256, 256);
        REQUIRE(uv[0] == Approx(0.75f));
        REQUIRE(uv[1] == Approx(0.75f));
        REQUIRE(uv[2] == Approx(0.25f));
        REQUIRE(uv[3] == Approx(0.25f));
    }
}

TEST_CASE("ComputeUVRect — 8x4 non-square grid, 512x256 atlas, 64px tile", "[sprite][uniform_atlas]")
{
    // 8 columns × 4 rows = 32 frames. tile 64, so 8*64=512 wide, 4*64=256 tall
    SECTION("frame 0 → (0, 0, 0.125, 0.25)") {
        auto uv = ComputeUVRect(0, 8, 64, 512, 256);
        REQUIRE(uv[0] == Approx(0.0f));
        REQUIRE(uv[1] == Approx(0.0f));
        REQUIRE(uv[2] == Approx(0.125f));
        REQUIRE(uv[3] == Approx(0.25f));
    }
    SECTION("frame 8 (col=0, row=1) → (0, 0.25, 0.125, 0.25)") {
        auto uv = ComputeUVRect(8, 8, 64, 512, 256);
        REQUIRE(uv[0] == Approx(0.0f));
        REQUIRE(uv[1] == Approx(0.25f));
    }
}

TEST_CASE("ComputeUVRect — 잘못된 입력 (zero/negative) → zero rect", "[sprite][uniform_atlas]")
{
    SECTION("cols=0") {
        auto uv = ComputeUVRect(0, 0, 64, 256, 256);
        REQUIRE(uv[0] == Approx(0.0f));
        REQUIRE(uv[2] == Approx(0.0f));
    }
    SECTION("atlasWidth=0") {
        auto uv = ComputeUVRect(0, 4, 64, 0, 256);
        REQUIRE(uv[2] == Approx(0.0f));
    }
}
```

- [ ] **Step 2: `src/sprite/uniform_atlas.h` 작성 (헤더 선언만 — 컴파일 실패 의도)**

```cpp
#ifndef __SJH_SPRITE_UNIFORM_ATLAS_H__
#define __SJH_SPRITE_UNIFORM_ATLAS_H__

#include "GL/gl3w.h"
#include <vmath.h>

namespace SJH::Sprite
{
    /// @brief frameIdx → atlas UV rect (uMin, vMin, uSize, vSize) 0..1 정규화.
    /// @param frameIdx    0-based frame index (row-major: col = idx % cols, row = idx / cols)
    /// @param cols        그리드 column 수 (atlas 가로 = cols × tileSize)
    /// @param tileSize    정사각 tile 한 변 픽셀 수
    /// @param atlasWidth  atlas 전체 가로 픽셀 (= cols × tileSize)
    /// @param atlasHeight atlas 전체 세로 픽셀 (= rows × tileSize)
    /// @return vmath::vec4 UV rect. cols<=0 또는 atlasWidth/Height<=0 이면 zero rect.
    /// @note GL 호출 없음 — 순수 math. 단위 테스트가 GL fixture 없이 검증.
    vmath::vec4 ComputeUVRect(int frameIdx, int cols, int tileSize,
                               int atlasWidth, int atlasHeight);

    /// @brief 등간격 N×M 정사각 그리드 atlas — sprite frame 시퀀스의 1차원 인덱스 → 2D UV rect 변환.
    /// @details
    ///   - PNG 한 장에 동일 tile 크기 sprite N×M 행렬로 배치
    ///   - frameIdx 가 row-major (col = idx % cols, row = idx / cols)
    ///   - GL_TEXTURE_2D 직접 보유 (Release() 로 명시 해제 또는 소멸자 자동)
    class UniformAtlas
    {
    public:
        UniformAtlas() = default;
        ~UniformAtlas();

        UniformAtlas(const UniformAtlas&)            = delete;
        UniformAtlas& operator=(const UniformAtlas&) = delete;
        UniformAtlas(UniformAtlas&&)                 = default;
        UniformAtlas& operator=(UniformAtlas&&)      = default;

        /// @brief PNG 로드 + GL_TEXTURE_2D 생성 + NEAREST/CLAMP_TO_EDGE 셋업 (픽셀아트).
        /// @return 성공 시 true. 실패 시 spdlog::error 출력 후 false (texture 안 만듦).
        bool LoadFromPNG(const char* path, int tilePx);

        /// @brief GL_TEXTURE_2D 명시 해제. 소멸자가 자동 호출하지만 명시 해제 가능.
        void Release();

        /// @brief frameIdx → atlas UV rect 0..1 정규화. ComputeUVRect 위임.
        vmath::vec4 GetUVRect(int frameIdx) const;

        /// @brief 등록된 atlas 의 전체 frame 수 (cols × rows).
        int FrameCount() const { return mCols * mRows; }

        // === Accessors ===
        GLuint TextureId()   const { return mTextureId; }
        int    AtlasWidth()  const { return mAtlasWidth; }
        int    AtlasHeight() const { return mAtlasHeight; }
        int    TileSize()    const { return mTileSize; }
        int    Cols()        const { return mCols; }
        int    Rows()        const { return mRows; }

    private:
        GLuint mTextureId   = 0;
        int    mAtlasWidth  = 0;
        int    mAtlasHeight = 0;
        int    mTileSize    = 64;
        int    mCols        = 0;
        int    mRows        = 0;
    };
}

#endif // __SJH_SPRITE_UNIFORM_ATLAS_H__
```

- [ ] **Step 3: `src/sprite/uniform_atlas.cpp` 작성 — `ComputeUVRect` 정의 + `UniformAtlas` stub**

```cpp
#define STB_IMAGE_IMPLEMENTATION   // ← stb_image 정의 책임 단일 위치 (spec 부록 B.5)
#include "stb_image.h"

#include "uniform_atlas.h"
#include <<spdlog>/spdlog.h>

namespace SJH::Sprite
{
    vmath::vec4 ComputeUVRect(int frameIdx, int cols, int tileSize,
                               int atlasWidth, int atlasHeight)
    {
        if (cols <= 0 || atlasWidth <= 0 || atlasHeight <= 0) {
            return vmath::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        }
        int col = frameIdx % cols;
        int row = frameIdx / cols;
        float u  = static_cast<float>(col * tileSize) / static_cast<float>(atlasWidth);
        float v  = static_cast<float>(row * tileSize) / static_cast<float>(atlasHeight);
        float du = static_cast<float>(tileSize)        / static_cast<float>(atlasWidth);
        float dv = static_cast<float>(tileSize)        / static_cast<float>(atlasHeight);
        return vmath::vec4(u, v, du, dv);
    }

    UniformAtlas::~UniformAtlas()
    {
        Release();
    }

    bool UniformAtlas::LoadFromPNG(const char* /*path*/, int /*tilePx*/)
    {
        // Task 2 에서 구현
        return false;
    }

    void UniformAtlas::Release()
    {
        if (mTextureId) {
            glDeleteTextures(1, &mTextureId);
            mTextureId = 0;
        }
    }

    vmath::vec4 UniformAtlas::GetUVRect(int frameIdx) const
    {
        return ComputeUVRect(frameIdx, mCols, mTileSize, mAtlasWidth, mAtlasHeight);
    }
}
```

- [ ] **Step 4: `src/sprite/CMakeLists.txt` 작성**

```cmake
# SJH::sprite — 등간격 N×M atlas + SpriteComponent (게임 무관 sprite 데이터)
# spec doc/superpowers/specs/2026-05-24-topdown-shooter-design.md §1.4
add_library(sjh_sprite STATIC
    uniform_atlas.cpp
)
add_library(SJH::sprite ALIAS sjh_sprite)

target_include_directories(sjh_sprite PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

# 의존 — PUBLIC: 헤더 노출 (Component 베이스, GL/vmath)
target_link_libraries(sjh_sprite PUBLIC
    SJH::scene           # SJH::Scene::Component 베이스 (sprite_component.h)
    project_deps         # gl3w, vmath, OpenGL, 플랫폼 프레임워크
    # ※ stb_extra 의존 제거됨 (Task 4 fixup C2) — uniform_atlas.cpp 는 stb 직접 사용 0,
    #    SJH::Image (resource_registry) 가 stb_image 흡수. 대신 SJH::resource_registry PUBLIC link.
)

# PRIVATE: cpp 내부 전용
target_link_libraries(sjh_sprite PRIVATE
    spdlog               # uniform_atlas.cpp 의 spdlog::error/info
)
```

- [ ] **Step 5: `test/CMakeLists.txt` 에 `test_uniform_atlas` 등록**

`test/CMakeLists.txt` 의 `test_actor_lifecycle` 블록 (line 222~230) 직후에 다음을 추가:

```cmake
#  Phase M1 — UniformAtlas math (GL 불요)
add_executable(test_uniform_atlas test_uniform_atlas.cpp)
target_link_libraries(test_uniform_atlas PRIVATE
    Catch2::Catch2WithMain
    SJH::sprite               # ComputeUVRect 자유함수
)
target_compile_features(test_uniform_atlas PRIVATE cxx_std_17)
catch_discover_tests(test_uniform_atlas)
```

또한 같은 파일 line 325 부근의 *umbrella `tests` 타겟* 의존 목록을 찾아 `test_uniform_atlas` 추가:

```cmake
# 기존 (예시):
# add_custom_target(tests DEPENDS
#     test_buffer
#     test_keyboard_input     # KeyboardInput<TAction> 디스패치
#     ...
# )
# → test_uniform_atlas 한 줄 추가
```

- [ ] **Step 6: src 모듈 등록 (`src/CMakeLists.txt`) — 우산 합류는 Task 4 에서, 여기는 *서브디렉토리* 만 추가**

`src/CMakeLists.txt` 의 `add_subdirectory(scene)` 다음 줄 (line 8 부근) 또는 알파벳 정렬 위치에 1행 추가:

```cmake
# 기존:
# add_subdirectory(scene)
add_subdirectory(sprite)    # ← 추가 (M1)
# add_subdirectory(shader)
```

- [ ] **Step 7: 빌드 → 테스트 실패 확인 (TDD red)**

```bash
cmake --preset ninja -DENABLE_TESTING=ON
cmake --build --preset ninja --target test_uniform_atlas
```

기대: 빌드 성공. test 실행:

```bash
ctest --test-dir build_ninja -R "test_uniform_atlas" -V
```

기대: `ComputeUVRect` 테스트 모두 **PASS** (Step 3 에서 이미 정의됨). 만약 FAIL 이면 step 3 의 ComputeUVRect 정의 점검.

> 주의: 본 Task 의 TDD red 는 Step 2 `#include "sprite/uniform_atlas.h"` 가 *컴파일* 가능한 것까지가 빨강이었지만 Step 3 에서 바로 정의 추가 — *Step 1 작성 직후 빌드* 하면 *컴파일 에러* (red), Step 3 이후 *PASS* (green) 가 정상 흐름. 위 명령은 *최종 PASS* 확인.

- [ ] **Step 8: 커밋**

```bash
git add src/sprite/CMakeLists.txt src/sprite/uniform_atlas.h src/sprite/uniform_atlas.cpp \
        src/CMakeLists.txt <test>/test_uniform_atlas.cpp test/CMakeLists.txt
git commit -m "feat(sprite): SJH::sprite module skeleton with ComputeUVRect

- Add src/sprite/ — uniform_atlas.{h,cpp} stub (UniformAtlas::LoadFromPNG 미구현)
- Add ComputeUVRect free function — frameIdx → vmath::vec4 UV rect (GL-free math)
- ~~Add STB_IMAGE_IMPLEMENTATION definition (spec §B.5 — single owner)~~ → 2026-05-24 Task 4 fixup C2 에서 *제거*. uniform_atlas.cpp 는 SJH::Image::Load + SJH::Texture::CreateTexture 위임으로 stb 직접 사용 0
- Register test_uniform_atlas (Catch2, math-only, no GL fixture)

M1 Task 1/8. Plan: doc/superpowers/plans/2026-05-24-M1-sprite-module-billboard.md"
```

---

## Task 2: `SJH::sprite` — `UniformAtlas::LoadFromPNG` 구현

**Files:**
- Modify: `src/sprite/uniform_atlas.cpp` (LoadFromPNG stub → 실 구현)

- [ ] **Step 1: 실패하는 통합 테스트는 *생략* — M1 의 시각 검증 (Task 8) 이 LoadFromPNG 의 통합 검증 역할**

> 합리화: LoadFromPNG 의 GL 통합 테스트를 위해서는 (a) 헤드리스 GL fixture 와 (b) fixture PNG 파일이 필요. (a) 는 `SJH::test::GLContextFixture` 가 GL 3.3 만 보장 (`gl_test_fixture.h:6` 참조), (b) 는 별도 fixture asset 관리. M1 의 *최소 범위* 우선 — Task 8 의 실행 검증으로 충분. M2 이후 SpriteComponent + Actor 통합 시점에 LoadFromPNG fixture test 도입 검토.

- [ ] **Step 2: `LoadFromPNG` 실 구현 — `src/sprite/uniform_atlas.cpp` 의 stub 교체**

기존 (Task 1 Step 3):
```cpp
bool UniformAtlas::LoadFromPNG(const char* /*path*/, int /*tilePx*/)
{
    // Task 2 에서 구현
    return false;
}
```

다음으로 교체:
```cpp
bool UniformAtlas::LoadFromPNG(const char* path, int tilePx)
{
    if (!path || tilePx <= 0) {
        spdlog::error("[UniformAtlas] invalid args: path={}, tilePx={}",
                       path ? path : "(null)", tilePx);
        return false;
    }

    stbi_set_flip_vertically_on_load(true);   // OpenGL V축 보정 (spec 부록 B.1)

    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load(path, &w, &h, &channels, 4);
    if (!pixels) {
        spdlog::error("[UniformAtlas] load failed: {} ({})",
                       path, stbi_failure_reason());
        return false;
    }
    if (w % tilePx != 0 || h % tilePx != 0) {
        spdlog::error("[UniformAtlas] atlas size {}x{} not divisible by tile {}",
                       w, h, tilePx);
        stbi_image_free(pixels);
        return false;
    }

    mAtlasWidth  = w;
    mAtlasHeight = h;
    mTileSize    = tilePx;
    mCols        = w / tilePx;
    mRows        = h / tilePx;

    glGenTextures(1, &mTextureId);
    glBindTexture(GL_TEXTURE_2D, mTextureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                  GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);  // 픽셀아트
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);

    spdlog::info("[UniformAtlas] loaded {} ({}x{}, tile={}, {}x{} grid)",
                  path, w, h, tilePx, mCols, mRows);
    return true;
}
```

- [ ] **Step 3: 단위 테스트 재실행 — 기존 `ComputeUVRect` 테스트 영향 없음 확인**

```bash
cmake --build --preset ninja --target test_uniform_atlas
ctest --test-dir build_ninja -R "test_uniform_atlas" -V
```

기대: 모든 SECTION PASS (Task 1 의 단위 테스트는 ComputeUVRect 만 검증 — LoadFromPNG 호출 안 함, 영향 0).

- [ ] **Step 4: 커밋**

```bash
git add src/sprite/uniform_atlas.cpp
git commit -m "feat(sprite): UniformAtlas::LoadFromPNG implementation

- stb_image 로 PNG 디코딩 + glTexImage2D 업로드
- NEAREST + CLAMP_TO_EDGE 픽셀아트 텍스처 매개변수
- atlas 크기가 tile 의 정수 배수 아닐 시 spdlog::error + false
- stbi_set_flip_vertically_on_load(true) — OpenGL V축 보정 (spec §B.1)

M1 Task 2/8."
```

---

## Task 3: `SJH::sprite` — `SpriteComponent` (Component 파생 POD-ish)

**Files:**
- Create: `src/sprite/sprite_component.h`

- [ ] **Step 1: `src/sprite/sprite_component.h` 작성**

```cpp
#ifndef __SJH_SPRITE_SPRITE_COMPONENT_H__
#define __SJH_SPRITE_SPRITE_COMPONENT_H__

#include "scene/actor.h"   // SJH::Scene::Component
#include <vmath.h>

namespace SJH::Sprite
{
    class UniformAtlas;   // forward — 핸들 참조

    /// @brief 게임 무관 sprite 데이터 — atlas 참조 + frameIdx + size + tint + flipX.
    /// @details *시간 축 갱신 책임 없음* — frameIdx 만 보유.
    ///          SpriteSequencePlayable (M3.5 이후) 이 frameIdx 갱신.
    ///          M1 에서는 main.cpp 가 직접 frameIdx=0 설정 후 그리기.
    class SpriteComponent : public SJH::Scene::Component
    {
    public:
        SpriteComponent() = default;

        // === Lifecycle (Component 순수 가상 — 빈 override) ===
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float /*dt*/) override {}   // ← Playable 이 frameIdx 갱신, 본인 무동작

        // === 게임 무관 데이터 (public 멤버 직접 접근 — POD-ish) ===
        UniformAtlas* atlas    = nullptr;
        int           frameIdx = 0;
        vmath::vec2   size     = vmath::vec2(1.0f, 1.0f);   // 월드 단위
        vmath::vec4   tint     = vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        bool          flipX    = false;
    };
}

#endif // __SJH_SPRITE_SPRITE_COMPONENT_H__
```

> 주의: M1 에서는 SpriteComponent 를 *실제 Actor 에 부착 안 함* (main.cpp 가 직접 GL 호출). 단 *컴파일 검증* 을 위해 헤더 작성. M2 이후 PlayerActor 에 부착.

- [ ] **Step 2: 컴파일 검증 — 헤더가 컴파일 가능한지**

`src/sprite/CMakeLists.txt` 의 `target_sources` 또는 `PUBLIC_HEADER` 에 `sprite_component.h` 가 자동 노출 (target_include_directories 통해). 별도 등록 불요.

전체 SJH::sprite 재빌드:
```bash
cmake --build --preset ninja --target sjh_sprite
```

기대: 컴파일 성공 (sprite_component.h 가 다른 cpp 에서 include 안 됐어도 헤더 자체 문법 검증은 안 됨 — 다음 task 의 main.cpp 에서 include 시 검증).

- [ ] **Step 3: 커밋**

```bash
git add src/sprite/sprite_component.h
git commit -m "feat(sprite): SpriteComponent POD-ish (Scene::Component 파생)

- atlas* / frameIdx / size / tint / flipX 보유
- OnEnter/OnExit/Update 빈 override (시간 축 갱신은 Playable 책임)
- M1 에서는 main.cpp 가 직접 GL 호출 — Actor 부착은 M2 부터

M1 Task 3/8."
```

---

## Task 4: `SJH::engine` 우산에 `SJH::sprite` 합류 (+ Task 4 fixup: Image/Texture 위임)

> ⚠️ **Task 4 회고 (2026-05-24)**: 원래 plan 은 *`src/texture/image.cpp` 의 STB_IMAGE_IMPLEMENTATION 을 제거하고 SJH::sprite/uniform_atlas.cpp 가 단일 owner* 인 방향이었으나, code quality review 가 **test_texture link regression** 발견 → 사용자가 *Image/Texture 위임* 으로 재설계 결정. 결과:
> - `STB_IMAGE_IMPLEMENTATION` 단일 owner = **`src/texture/image.cpp`** (원위치 복귀, Task 4 fixup C1)
> - `SJH::sprite/uniform_atlas.cpp` 는 stb 직접 사용 0 — `SJH::Image::Load` + `SJH::Texture::CreateTexture` 위임 (Task 4 fixup C2)
> - SJH::engine 우산에 SJH::sprite 합류는 *유지* (Task 4 Commit B)
>
> spec §B.5 도 *image.cpp 단일 owner* 로 정정됨.

**Files:**
- Modify: `src/CMakeLists.txt` (target_link_libraries 추가 — Task 4 Commit B `8860304`)
- Modify: `src/sprite/uniform_atlas.h` / `.cpp` / `CMakeLists.txt` — Image/Texture 위임 (Task 4 fixup C2 `537e5c3`)
- Modify: `src/texture/image.cpp` — Task 4 Commit A `1c39556` 의 STB_IMAGE_IMPLEMENTATION 제거를 *되돌림* (Task 4 fixup C1 `0db0114`)

> **사전 작업 (Step 0)** — *(History reference)* Task 1 의 code quality review 가 발견한 잠재 multiple-definition 우려 회피 시도였음. 단 *test_texture* 가 SJH::engine 우산 안 거치고 직접 SJH::resource_registry link → image.cpp 가 stb 정의 잃으면 unresolved 발생. Task 4 fixup 으로 *Image/Texture 위임* 재설계가 정본.

- [x] ~~**Step 0a (deprecated)**: `src/texture/image.cpp` 의 STB_IMAGE_IMPLEMENTATION 제거~~

> 2026-05-24 정정 (Task 4 fixup C1 `0db0114`): Step 0a 의 변경을 *되돌림*. image.cpp 가 단일 owner 로 *원위치*. 이유는 본 task 헤더의 회고 박스 + commit C1 메시지 참조.

- [ ] **Step 0b: image.cpp 가 SJH::sprite 와 같은 stbi_load 정의를 *링크 시* 가져오는지 확인**

SJH::resource_registry 와 SJH::sprite 둘 다 stb_image 함수 호출. *정의* 는 SJH::sprite 만. resource_registry 가 SJH::sprite 를 link 하지 않으므로 *link 시 unresolved* 가능성 — 확인 필요.

```bash
cmake --build --preset ninja --target sjh_resource_registry 2>&1 | grep -E "stbi|undefined|error" | head -10
```

만약 unresolved symbol: SJH::resource_registry 의 CMakeLists.txt 에 `SJH::sprite` PRIVATE link 추가 또는 SJH::engine 우산이 *동시에* 둘 다 link 하므로 우산 사용 데모는 OK. 단 *resource_registry 만 link 하는 데모* 에서는 문제 — 확인 후 결정.

대안: `image.cpp` 의 stb_image 함수 호출이 *없다* 면 (단순 header include 만) — 모두 OK. 확인:
```bash
grep -n "stbi_" src/texture/image.cpp
```

- [ ] **Step 0c: image.cpp 변경 commit (Task 4 의 첫 commit)**

```bash
git add src/texture/image.cpp
git commit -m "refactor(resource_registry): remove duplicate STB_IMAGE_IMPLEMENTATION

- spec §B.5: SJH::sprite 가 단일 owner (uniform_atlas.cpp)
- image.cpp 는 stb_image 함수 호출만 — 정의는 SJH::sprite 에 위탁
- Task 4 의 SJH::engine 우산 합류 전 사전 정리 (multiple-definition 폭발 회피)

M1 Task 4/8 pre-step."
```

- [ ] **Step 1: 현 `src/CMakeLists.txt` 확인**

기존 (Task 1 Step 6 후 — `add_subdirectory(sprite)` 이미 추가됨):
```cmake
add_subdirectory(buffer)
add_subdirectory(object)
add_subdirectory(common)
add_subdirectory(diagnostics)
add_subdirectory(layout)
add_subdirectory(program)
add_subdirectory(material)
add_subdirectory(scene)
add_subdirectory(sprite)    # ← Task 1 에서 추가됨
add_subdirectory(shader)
add_subdirectory(render)
add_subdirectory(resource_registry)
add_subdirectory(input)

add_library(sjhopengl_engine INTERFACE)
add_library(SJH::engine ALIAS sjhopengl_engine)

target_link_libraries(sjhopengl_engine INTERFACE
    SJH::common
    SJH::buffer
    SJH::diagnostics
    SJH::input
    SJH::layout
    SJH::material
    SJH::object
    SJH::program
    SJH::render
    SJH::resource_registry
    SJH::scene
    SJH::shader
)
```

- [ ] **Step 2: `target_link_libraries(sjhopengl_engine INTERFACE ...)` 에 `SJH::sprite` 추가**

`SJH::shader` 다음 줄에 `SJH::sprite` 1행:

```cmake
target_link_libraries(sjhopengl_engine INTERFACE
    SJH::common
    SJH::buffer
    SJH::diagnostics
    SJH::input
    SJH::layout
    SJH::material
    SJH::object
    SJH::program
    SJH::render
    SJH::resource_registry
    SJH::scene
    SJH::shader
    SJH::sprite          # M1 — 13 모듈 (sprite 합류)
)
```

- [ ] **Step 3: SJH::engine 우산을 link 하는 *기존 데모* 가 영향 없는지 확인**

활성 데모 (tweeny_demo / box2d_demo / effekseer_demo / migrate_demo) 모두 SJH::engine 우산을 link. 우산에 SJH::sprite 추가해도 *interface only* 라 *링크 실패 없음*. 단 *include path 전파* 가 동작 — `#include "sprite/..."` 가 다른 데모에서도 가능해짐 (의도된 결과).

빌드 확인:
```bash
cmake --build --preset ninja --target tweeny_demo
```

기대: 성공 (변경 없는 데모는 영향 0).

- [ ] **Step 4: 커밋**

```bash
git add src/CMakeLists.txt
git commit -m "feat(engine): SJH::sprite 를 SJH::engine 우산에 합류

- 코어 모듈 12 → 13 (sprite 추가)
- SJH::engine 우산만 link 하면 SJH::sprite 자동 가용
- 기존 데모 (tweeny_demo 등) 영향 없음 — INTERFACE 만 확장

M1 Task 4/8."
```

---

## Task 5: `apps/_MyApp_/` 재활성 — apps/CMakeLists.txt 주석 해제 + apps/_MyApp_/CMakeLists.txt 에 SJH::engine link

**Files:**
- Modify: `apps/CMakeLists.txt:1`
- Modify: `apps/_MyApp_/CMakeLists.txt:10`

- [ ] **Step 1: `apps/CMakeLists.txt` 의 `_MyApp_` 줄 주석 해제**

기존:
```cmake
# add_subdirectory(_MyApp_)
add_subdirectory(migrate_demo)
add_subdirectory(box2d_demo)
add_subdirectory(effekseer_demo)
add_subdirectory(tweeny_demo)
# add_subdirectory(audio_demo)
```

`# add_subdirectory(_MyApp_)` 의 `# ` 제거:
```cmake
add_subdirectory(_MyApp_)
add_subdirectory(migrate_demo)
add_subdirectory(box2d_demo)
add_subdirectory(effekseer_demo)
add_subdirectory(tweeny_demo)
# add_subdirectory(audio_demo)
```

- [ ] **Step 2: `apps/_MyApp_/CMakeLists.txt` 의 link 에 `SJH::engine` 추가**

기존 line 10:
```cmake
target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps game_deps)
```

다음으로 교체:
```cmake
target_link_libraries(${CHAPTER_NAME} PRIVATE
    project_deps
    game_deps
    SJH::engine          # M1 — SJH::sprite 등 코어 모듈 우산 link
)
```

- [ ] **Step 3: 재구성 + `_MyApp_` 빌드 시도**

```bash
cmake --preset ninja
cmake --build --preset ninja --target _MyApp_
```

기대: 빌드 **실패** — main.cpp 가 아직 옛 검증용 코드 (Task 1 작업 *전* 의 `#define STB_RECT_PACK_IMPLEMENTATION` 등) 라 무관한 컴파일 에러 가능. Task 7 에서 main.cpp 재작성으로 해결. 본 step 은 *CMake 단의 setup 성공만* 확인.

확인할 것: CMake configure 단계가 에러 없이 완료 + `Configuring done` 출력.

- [ ] **Step 4: 커밋**

```bash
git add apps/CMakeLists.txt apps/_MyApp_/CMakeLists.txt
git commit -m "build(_MyApp_): activate apps/_MyApp_ + link SJH::engine

- apps/CMakeLists.txt: \`add_subdirectory(_MyApp_)\` 주석 해제
- apps/_MyApp_/CMakeLists.txt: PRIVATE link 에 SJH::engine 추가
- main.cpp 는 아직 옛 검증용 코드 — Task 7 에서 재작성

M1 Task 5/8."
```

---

## Task 6: 빌보드 atlas 셰이더 (GLSL 410)

**Files:**
- Create: `<apps>/_MyApp_/resources/shaders/billboard_atlas.vert`
- Create: `<apps>/_MyApp_/resources/shaders/billboard_atlas.frag`

- [ ] **Step 1: `<apps>/_MyApp_/resources/shaders/billboard_atlas.vert` 작성**

```glsl
#version 410 core

// === vertex attributes ===
layout(location = 0) in vec2 a_quad;   // (-0.5,-0.5) ~ (0.5,0.5) — quad 6 정점 (2 triangles)
layout(location = 1) in vec2 a_uv;     // (0,0) ~ (1,1)            — quad UV

// === uniforms ===
uniform mat4 u_view;
uniform mat4 u_proj;
uniform vec3 u_billboardCenter;        // 빌보드 월드 좌표 (중심)
uniform vec2 u_billboardSize;          // 빌보드 크기 (width, height)
uniform vec4 u_uvRect;                 // (uMin, vMin, uSize, vSize) — atlas 내부 sub-rect
uniform float u_flipX;                 // +1.0 또는 -1.0

// === out ===
out vec2 v_uv;

void main()
{
    // View 행렬에서 카메라 right 벡터 추출 (Y축 고정 cylindrical billboard)
    vec3 cameraRight = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 cameraUp    = vec3(0.0, 1.0, 0.0);    // Y축 고정

    vec3 worldPos = u_billboardCenter
                  + cameraRight * a_quad.x * u_billboardSize.x * u_flipX
                  + cameraUp    * a_quad.y * u_billboardSize.y;

    v_uv = u_uvRect.xy + a_uv * u_uvRect.zw;
    gl_Position = u_proj * u_view * vec4(worldPos, 1.0);
}
```

- [ ] **Step 2: `<apps>/_MyApp_/resources/shaders/billboard_atlas.frag` 작성**

```glsl
#version 410 core

in vec2 v_uv;

uniform sampler2D u_atlas;
uniform vec4 u_tint;

out vec4 fragColor;

void main()
{
    vec4 c = texture(u_atlas, v_uv);
    if (c.a < 0.01) discard;   // alpha-test (sorting 문제 회피, spec §10.2)
    fragColor = c * u_tint;
}
```

- [ ] **Step 3: 셰이더 문법 검증 (glslangValidator — 옵션)**

`glslangValidator` 가 환경에 설치된 경우:
```bash
glslangValidator <apps>/_MyApp_/resources/shaders/billboard_atlas.vert
glslangValidator <apps>/_MyApp_/resources/shaders/billboard_atlas.frag
```

기대: 출력 없음 (= 성공). 에러 출력 시 GLSL 문법 점검.

> 미설치 시 *건너뜀* — Task 7 의 런타임 컴파일 (SJH::Shader::CreateFromFile) 이 문법 검증 자동 수행 (Shader::TryLoadFile 가 컴파일 실패 시 nullptr 반환 + diagnostics 로그).

- [ ] **Step 4: 커밋**

```bash
git add <apps>/_MyApp_/resources/shaders/billboard_atlas.vert \
        <apps>/_MyApp_/resources/shaders/billboard_atlas.frag
git commit -m "feat(_MyApp_): GLSL 410 billboard atlas shaders

- billboard_atlas.vert: Y축 고정 cylindrical billboard + atlas sub-rect 매핑
- billboard_atlas.frag: alpha-test discard + u_tint 곱셈
- spec §2.2/§2.3 정본

M1 Task 6/8."
```

---

## Task 7: `apps/_MyApp_/main.cpp` — sb7 init 4.1 + atlas 로드 + 셰이더 컴파일 + 단일 빌보드 그리기

**Files:**
- Modify: `apps/_MyApp_/main.cpp` (완전 재작성)

- [ ] **Step 1: 기존 `main.cpp` 폐기 + 재작성**

기존 main.cpp 의 의존성 등록 검증용 임시 코드 (Effekseer/Box2D/Assimp/Tweeny include 등) 전부 폐기.

`apps/_MyApp_/main.cpp` 새 내용:

```cpp
/**
 * @file main.cpp
 * @brief M1 — SJH::sprite UniformAtlas 로 player_atlas 한 frame 빌보드 정적 표시.
 *        sb7::application 으로 GL 4.1 강제, SJH::Shader/Program 으로 셰이더 컴파일,
 *        직접 glDrawArrays — Actor/Playable/Tween 통합은 M2 이후.
 */

#include <sb7.h>
#include <GL/gl3w.h>
#include <vmath.h>
#include <<spdlog>/spdlog.h>

#include "sprite/uniform_atlas.h"
#include "shader/shader.h"
#include "program/program.h"
#include "program/program_uniforms.h"

#include <array>
#include <cstring>
#include <memory>
#include <vector>

namespace TopdownShooter
{

class game_application : public sb7::application
{
public:
    void init() override
    {
        sb7::application::init();
        info.majorVersion = 4;
        info.minorVersion = 1;
        info.flags.forwardCompat = 1;   // macOS 필수
        info.flags.debug         = 1;   // KHR_debug 콜백 (windows/linux — macOS 미지원 자동 no-op)
        info.flags.coreProfile   = 1;
        static const char title[] = "M1 — Topdown Shooter (sprite billboard)";
        std::memcpy(info.title, title, sizeof(title));
    }

    void startup() override
    {
        // === 1. atlas 로드 ===
        // 사용자 결정 (2026-05-24): default atlas 는 기존 resources/texture/TestPattern.png 재활용 (512×512, 4×4 grid, 128px tile).
        // POST_BUILD copy 가 apps/_MyApp_/resources/texture/TestPattern.png 을 실행파일 옆으로 복사 (Task 8).
        if (!mAtlas.LoadFromPNG("resources/texture/TestPattern.png", /*tilePx=*/128)) {
            spdlog::error("[M1] atlas load failed — TestPattern.png 가 resources/texture/ 에 있는지 확인");
            return;
        }

        // === 2. 셰이더 컴파일 + 프로그램 link ===
        auto vs = SJH::Shader::CreateFromFile(
            "<resources>/shaders/billboard_atlas.vert", GL_VERTEX_SHADER);
        auto fs = SJH::Shader::CreateFromFile(
            "<resources>/shaders/billboard_atlas.frag", GL_FRAGMENT_SHADER);
        if (!vs || !fs) {
            spdlog::error("[M1] shader compile failed");
            return;
        }
        std::vector<SJH::ShaderPtr> shaders{
            SJH::ShaderPtr(vs.release()),
            SJH::ShaderPtr(fs.release()),
        };
        mProgram = SJH::Program::Create(shaders);
        if (!mProgram) {
            spdlog::error("[M1] program link failed");
            return;
        }

        // === 3. quad VBO/VAO — 6 정점 (2 triangles), interleaved a_quad(vec2) + a_uv(vec2) ===
        const std::array<float, 6 * 4> kQuadData = {
            // x      y      u     v
            -0.5f, -0.5f,  0.0f, 0.0f,
             0.5f, -0.5f,  1.0f, 0.0f,
             0.5f,  0.5f,  1.0f, 1.0f,

            -0.5f, -0.5f,  0.0f, 0.0f,
             0.5f,  0.5f,  1.0f, 1.0f,
            -0.5f,  0.5f,  0.0f, 1.0f,
        };

        glGenVertexArrays(1, &mVao);
        glBindVertexArray(mVao);

        glGenBuffers(1, &mVbo);
        glBindBuffer(GL_ARRAY_BUFFER, mVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadData), kQuadData.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              reinterpret_cast<void*>(2 * sizeof(float)));

        glBindVertexArray(0);

        // === 4. 일회성 GL 상태 ===
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);   // alpha-test 만 사용 (frag discard)
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

        spdlog::info("[M1] startup complete");
    }

    void render(double currentTime) override
    {
        const float fc = static_cast<float>(currentTime);
        (void)fc;   // M1 — 정적 표시라 시간 미사용

        // === clear ===
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (!mProgram || mAtlas.TextureId() == 0) return;   // startup 실패 시 안전 종료

        // === 카메라 (탑다운 — 위에서 내려다보는 시점) ===
        const vmath::mat4 view = vmath::lookat(
            vmath::vec3(0.0f, 5.0f, 5.0f),    // eye
            vmath::vec3(0.0f, 0.0f, 0.0f),    // center
            vmath::vec3(0.0f, 1.0f, 0.0f));   // up
        const vmath::mat4 proj = vmath::perspective(
            45.0f, static_cast<float>(info.windowWidth) / static_cast<float>(info.windowHeight),
            0.1f, 100.0f);

        // === Program 활성화 + uniform 송신 ===
        glUseProgram(mProgram->GetProgramAddr());

        SJH::Uniforms::SetMat4(*mProgram, "u_view", view);
        SJH::Uniforms::SetMat4(*mProgram, "u_proj", proj);
        SJH::Uniforms::SetVec3(*mProgram, "u_billboardCenter", vmath::vec3(0.0f, 0.0f, 0.0f));
        SJH::Uniforms::SetVec2(*mProgram, "u_billboardSize",   vmath::vec2(1.0f, 1.0f));
        SJH::Uniforms::SetFloat(*mProgram, "u_flipX", 1.0f);

        // frame 0 의 UV rect (player_atlas 의 첫 frame)
        const vmath::vec4 uvRect = mAtlas.GetUVRect(/*frameIdx=*/0);
        SJH::Uniforms::SetVec4(*mProgram, "u_uvRect", uvRect);

        SJH::Uniforms::SetVec4(*mProgram, "u_tint", vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));

        // === atlas 텍스처 바인딩 + sampler uniform (texture unit 0) ===
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mAtlas.TextureId());
        SJH::Uniforms::SetInt(*mProgram, "u_atlas", 0);

        // === draw ===
        glBindVertexArray(mVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glUseProgram(0);
    }

    void shutdown() override
    {
        if (mVbo) { glDeleteBuffers(1, &mVbo); mVbo = 0; }
        if (mVao) { glDeleteVertexArrays(1, &mVao); mVao = 0; }
        mProgram.reset();
        mAtlas.Release();
    }

private:
    SJH::Sprite::UniformAtlas       mAtlas;
    SJH::ProgramUPtr                mProgram;
    GLuint                          mVao = 0;
    GLuint                          mVbo = 0;
};

}  // namespace TopdownShooter

DECLARE_MAIN(TopdownShooter::game_application);
```

- [ ] **Step 2: 빌드 시도 (atlas PNG 없어도 컴파일/링크는 성공해야 함)**

```bash
cmake --build --preset ninja --target _MyApp_
```

기대: **빌드 성공** (0 에러). 만약 link 에러 시 `apps/_MyApp_/CMakeLists.txt` 의 SJH::engine link (Task 5) 확인.

만약 컴파일 에러:
- `sprite/uniform_atlas.h` 못 찾음 → SJH::engine 우산이 SJH::sprite 포함하는지 (Task 4)
- `SJH::Uniforms::SetMat4` 못 찾음 → `program/program_uniforms.h` include 확인
- `SJH::ShaderPtr` 못 찾음 → `shader/shader.h` 의 `CLASS_PTR(Shader)` 매크로가 정의했는지

- [ ] **Step 3: 커밋**

```bash
git add apps/_MyApp_/main.cpp
git commit -m "feat(_MyApp_): M1 main.cpp — atlas + 빌보드 1장 정적 표시

- sb7::application::init() override: GL 4.1 core + forward compat + debug
- startup(): UniformAtlas 로드, SJH::Shader/Program 으로 셰이더 컴파일/링크,
  quad VAO/VBO 생성 (6 정점 interleaved)
- render(): SJH::Uniforms 로 view/proj/center/size/flipX/uvRect/tint 송신,
  atlas 텍스처 unit 0 바인딩 후 glDrawArrays
- shutdown(): VBO/VAO/Program/Atlas 명시 해제
- 의존성 등록 검증용 임시 코드 (Effekseer/Box2D/Assimp/Tweeny include) 폐기

M1 Task 7/8. (atlas PNG 는 Task 8 에서 준비)"
```

---

## Task 8: Atlas PNG 준비 (기존 자산 재활용) + 빌드/실행/시각 검증

**Files:**
- Copy: `resources/texture/TestPattern.png` → `apps/_MyApp_/resources/texture/TestPattern.png` (기존 512×512 RGBA, 4×4 grid)

> **사용자 결정 (2026-05-24)**: default atlas 는 기존 `resources/texture/TestPattern.png` 재활용 (4 col × 4 row, 128px tile). placeholder 생성 step 폐기.

- [ ] **Step 1: 기존 TestPattern.png 를 `apps/_MyApp_/resources/texture/` 로 복사**

```bash
mkdir -p apps/_MyApp_/resources/texture
cp resources/texture/TestPattern.png apps/_MyApp_/resources/texture/TestPattern.png
```

> 본 저장소 컨벤션: 각 데모는 *자기 `apps/<demo>/resources/`* 만 보유 (`apps/tweeny_demo/CMakeLists.txt` 의 POST_BUILD copy 패턴 동일). 루트 `resources/` 는 *공유 자산 풀* — 데모가 *필요한 것만 자기 디렉토리로 복사*.

- [ ] **Step 2: PNG 복사 확인**

```bash
ls -la apps/_MyApp_/resources/texture/TestPattern.png
file apps/_MyApp_/resources/texture/TestPattern.png
```

기대 출력:
```
-rw-r--r-- ... TestPattern.png
TestPattern.png: PNG image data, 512 x 512, 8-bit/color RGBA, non-interlaced
```

- [ ] **Step 3: 빌드 + 실행**

```bash
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```

기대:
- 윈도우 타이틀 *"M1 — Topdown Shooter (sprite billboard)"* 의 GL 윈도우 출현
- 화면 중앙에 TestPattern frame 0 (atlas 의 좌상단 128×128 tile) 표시
- 빌보드라 카메라 위치 (0, 5, 5) 에서 내려다보는 시점이지만 quad 가 카메라 평면에 정렬되어 *기울지 않고 정면 보임*
- 콘솔에 `[UniformAtlas] loaded resources/texture/TestPattern.png (512x512, tile=128, 4x4 grid)` + `[M1] startup complete`

종료: 윈도우 닫거나 Esc.

- [ ] **Step 4: 시각 검증 체크리스트**

다음 모두 만족하면 M1 완료:
- [ ] 윈도우 정상 표시 (검은/푸른 배경 + 중앙 sprite quad)
- [ ] sprite 가 TestPattern frame 0 (atlas 의 좌상단 128×128 영역) 으로 표시
- [ ] sprite 가 카메라에 정면 (회전/기울기 없음)
- [ ] *GL_INVALID_OPERATION* 또는 `GL error` 로그 없음
- [ ] Esc 또는 윈도우 X 클릭 시 정상 종료 (segfault 없음)

만약 화면이 *비어 보임*:
- view 행렬의 *카메라 위치/방향* 확인 (현재 eye=(0,5,5), center=(0,0,0))
- atlas frame 0 의 *alpha 값* 확인 (체커보드가 alpha=255 — 100% opaque, discard 안 됨)
- spdlog 로그에서 셰이더 컴파일 실패 또는 atlas 로드 실패 메시지 확인

- [ ] **Step 5: 단위 테스트 회귀 확인**

```bash
ctest --test-dir build_ninja -R "test_uniform_atlas" -V
```

기대: 모든 SECTION PASS (Task 2 의 LoadFromPNG 구현이 ComputeUVRect 영향 없음 확인).

전체 테스트 실행 (회귀 가드):
```bash
ctest --test-dir build_ninja --output-on-failure
```

기대: 기존 활성 테스트 + `test_uniform_atlas` 모두 PASS.

- [ ] **Step 6: 커밋**

```bash
git add apps/_MyApp_/resources/sprites/player_atlas.png
git commit -m "feat(_MyApp_): M1 placeholder atlas + 시각 검증 완료

- 256x256 RGBA 4x4 grid placeholder PNG (각 tile 식별 가능 색)
- player_atlas frame 0 빌보드 1장이 화면 중앙에 정적 표시 확인
- 회귀 가드: test_uniform_atlas + 기존 테스트 모두 PASS

M1 Task 8/8 — M1 완료."
```

---

## Self-Review

**Spec coverage:**
- [x] 부록 D M1 `(a) apps/_MyApp_/ 재활성` → Task 5
- [x] 부록 D M1 `(b) src/sprite/ 신규 모듈 — uniform_atlas + sprite_component` → Task 1+2+3
- [x] `STB_IMAGE_IMPLEMENTATION` 단일 위치 (`src/sprite/uniform_atlas.cpp`) → Task 1 Step 3, 부록 B.5 준수
- [x] `src/CMakeLists.txt 에 add_subdirectory(sprite) + SJH::engine 우산 합류` → Task 1 Step 6 + Task 4
- [x] 부록 D M1 `(c) billboard_atlas.{vert,frag} GLSL 410` → Task 6
- [x] 부록 D M1 `(d) TopdownShooter::startup 에서 atlas 1장 로드 + quad 1개 직접 그리기` → Task 7
- [x] 결정 #5 GL 4.1 강제 (`init()` override) → Task 7 Step 1
- [x] §2.1 sb7::application init() 패턴 → Task 7 Step 1
- [x] §2.2 빌보드 셰이더 GLSL 410 → Task 6
- [x] §B.1 V축 보정 (`stbi_set_flip_vertically_on_load(true)`) → Task 2 Step 2
- [x] §B.2 GLSL 410 강제 → Task 6 (`#version 410 core`)
- [x] §B.5 stb_image 정의 책임 SJH::sprite 흡수 → Task 1 Step 3
- [x] `UniformAtlas::LoadFromPNG + GetUVRect(0) 동작` 산출 검증 → Task 8 Step 4

**Placeholder scan:**
- "TBD" / "implement later" / "TODO" — 없음 ✓
- "Add appropriate error handling" — 없음 (모든 에러 처리는 실제 spdlog::error 코드로 명시) ✓
- "Write tests for the above" — 없음 (Task 1 에 실제 test 코드 인라인) ✓
- "Similar to Task N" — 없음 (각 task 가 자기 코드 완비) ✓

**Type consistency:**
- `UniformAtlas::LoadFromPNG(const char* path, int tilePx)` 시그니처 — Task 1 헤더 / Task 2 정의 일치 ✓
- `ComputeUVRect(int frameIdx, int cols, int tileSize, int atlasWidth, int atlasHeight)` — Task 1 헤더 / 정의 / 테스트 일치 ✓
- `SpriteComponent` — `OnEnter / OnExit / Update` 셋 다 override (Component 순수 가상 강제) ✓
- `SJH::Sprite` namespace (PascalCase) — `actor.h:15` 의 `SJH::Scene` 패턴과 일관 ✓
- `SJH::ShaderPtr` (shared) 변환 — `SJH::ShaderUPtr` (Task 7 Step 1) 에서 `.release()` 후 `ShaderPtr(...)` ✓

**Spec gaps 없음.**

---

## Execution Handoff

**Plan complete and saved to `doc/superpowers/plans/2026-05-24-M1-sprite-module-billboard.md`. Two execution options:**

**1. Subagent-Driven (recommended)** — fresh subagent per task, review between tasks, fast iteration. 본 plan 의 8 task 가 명확히 분리되어 있어 subagent-driven 이 효율적. 각 task 후 빌드/테스트 결과 검토.

**2. Inline Execution** — execute tasks in this session using executing-plans, batch execution with checkpoints. 본 session 의 컨텍스트 (spec 결정 19+8+? = 다수, 본 plan 의 모든 결정) 그대로 사용 가능. 단 8 task 직렬 작업이라 inline 은 session 길어짐.

**Which approach?**

---

## 실행 결과 (Implementation Log, 2026-05-24)

| 단계 | Commit | 상태 |
|---|---|---|
| Task 1 — SJH::sprite skeleton + ComputeUVRect + 단위 테스트 | `a023b27` | spec ✅ quality ✅ |
| Task 2 — UniformAtlas::LoadFromPNG (stb_image 직접) | `6b3bfc2` | spec ✅ quality ✅ |
| Task 3 — SpriteComponent header | `d51ee96` | spec ✅ quality ✅ |
| Task 4 Commit A — image.cpp 의 STB_IMAGE_IMPLEMENTATION 제거 | `1c39556` | **C1 에서 revert** |
| Task 4 Commit B — SJH::engine 우산에 SJH::sprite 합류 | `8860304` | spec ✅ quality ❌ (test_texture link regression) |
| Fixup C1 — image.cpp 복원 (revert Task 4 Commit A) | `0db0114` | ✅ |
| Fixup C2 — UniformAtlas 가 Image+Texture 위임 (stb 직접 호출 제거) | `537e5c3` | ✅ multiple-definition + test_texture link 자동 해결 |
| Task 5+7 통합 — `apps/_MyApp_/` 활성 + main.cpp 초안 (Mesh::CreatePlane + Director 도입) | `b590bb1` | ✅ 시각 검증 (정면 표시) |
| Task 6+7 (D1) — 셰이더 컨벤션 정정 (camelCase + uModel 흡수) + main.cpp uniform 이름 일괄 정정 | `71567be` | ✅ 빌드 PASS |
| Task 5~8 (D2) — main.cpp 전면 재작성: Director + SceneRenderer + Material + MeshRenderer | `16f1426` | ✅ 직접 GL 호출 0, 빌드 PASS |
| Task 4 macOS chdir workaround | `070c6ad` | ✅ |
| Task 8 — TestPattern.png 복사 + 시각 검증 | `419487f` | ✅ 시각 PASS |

### 정착 디자인 (사후)

- **stb_image 단일 owner**: `src/texture/image.cpp` (M1 fixup C2 으로 원위치 — spec §B.5 재정정)
- **UniformAtlas 의존**: `SJH::Image::Load(stb)` + `SJH::Texture::CreateTexture(Image*) (GL)` 위임. UniformAtlas 자기 책임 = grid math + Texture 보유
- **셰이더 GLSL 컨벤션**: `aPos/aNormal/aTexCoord` + `uModel/uView/uProj/uUvRect/uFlipX/uAtlas/uTint` + `vUv` (lighting.vs/.fs 일치)
- **빌보드 center/size**: `uModel` 흡수 — Actor.Transform.Translate=center, Scale=size. 셰이더가 `length(uModel[0].xyz)` 로 sx, `length(uModel[1].xyz)` 로 sy 추출
- **main.cpp 패턴**: Director + SceneRenderer + Material + MeshRenderer (migrate_demo / tweeny_demo 정통). 직접 GL 호출 0. render() 본문 = `Director.Update(dt) + mRenderSys.Render(*mDefaultTarget)` 2-line

### 남은 위험 (Final review 대상)

| # | 항목 | 위치 |
|---|---|---|
| 1 | clangd missing-includes (`PATH_MAX`, `std::move`, `Pass::Kind`, `glClearColor`, `glViewport`) — 정보성, 빌드 통과, 다른 SJH 데모와 동일 컨벤션 | `apps/_MyApp_/main.cpp` |
| 2 | test_texture 의 *pre-existing* compile error (`SJH::ResourceRegistry::Create()` 부재) — M1 시작 전부터 존재, M1 scope 밖 | `<test>/test_texture.cpp` |
| 3 | SJH::diagnostics 통합 (GL error 검출) 미적용 — M1 범위 외, 후속 M2+ 고려 | `apps/_MyApp_/main.cpp` |

### 검증된 산출

- [x] 빌드: `cmake --preset ninja -DENABLE_TESTING=ON && cmake --build --preset ninja --target _MyApp_` PASS
- [x] 단위 테스트: `ctest --test-dir build_ninja -R "ComputeUVRect" -V` 3 cases / 21 assertions PASS
- [x] 시각 검증 (사용자): TestPattern frame 0 빌보드 정면 정적 표시 ("작동은 잘 된다. Billboard 정면" — 2026-05-24)
- [x] 회귀 가드: `tweeny_demo` 빌드 PASS (umbrella 영향 없음)

### M1 완료 선언

본 plan 의 8 task + 후속 정정 (C1/C2/D1/D2) 모두 완료. M1 목표 *"빌드 인프라 + SJH::sprite 모듈 신설 + 빌보드 1장 정적 표시"* 달성 + 추가로 *Image/Texture 위임 + Director 패턴* 까지 정착해 M2 의 기반 마련.
