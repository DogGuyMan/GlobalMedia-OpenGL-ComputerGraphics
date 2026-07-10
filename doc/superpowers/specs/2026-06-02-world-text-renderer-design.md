# World Text — BitmapFont + TextRenderer (`SJH::text`) + SpawnWorldText 설계

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **상태**: 작성 완료 — 사용자 리뷰 대기 (섹션별 의사결정 질의 기반 — `spec_phase_by_phase_inquiry` 준수)
> **날짜**: 2026-06-02
> **대상 데모**: `apps/_MyApp_` (탑다운 슈터)
> **선행 리서치**: init / EngineAPI.md / Context7 ImGui / `SJH::render` / `TweenPlayable` / Spawns(VfxInstance·AutoDespawnOnFinish·OneShotSweeper) / minogram BMFont
> **정통 패턴 출처**: `Spawns/VfxInstance` (Actor 스폰 + Playable + AutoDespawnOnFinish + Play) 와 구조 동일

---

## 1. 개요 · 범위

### 1.1 목적
World 공간에 **떠오르며 사라지는 텍스트**(전투 데미지 피드백 — `10`, `+5` 등)를 스폰하는 프리미티브를 제공한다.
`Actor + TextRenderer + TweenPlayable + AutoDespawnOnFinish` 조립이며, 픽셀아트 빌보드로 렌더된다.

### 1.2 확정 결정 (D1~D6)

| # | 결정 | 근거 |
|---|---|---|
| **D1** | **용도 = 데미지/전투 피드백** (월드 위치 spawn → 상승+fade → 자동 despawn) | 사용자 선택 |
| **D2** | **렌더 = World 빌보드 메시 + 비트맵 폰트** (스크린 오버레이/ImGui 아님) | 픽셀아트 게임 미적·아키텍처 일관 |
| **D3** | **글리프 조립 = α** (글자마다 child Actor + 기존 `SpriteRenderer` 재사용, 새 셰이더 0) | 최저 리스크·최대 재사용. 공개 API는 백엔드 무관 → 후일 β 승격 가능 |
| **D4** | **모듈 거주 = Core `SJH::text`** (17번째 모듈). `BitmapFont` + `TextRenderer` 거주. 스포너 `SpawnWorldText`는 **Client**(`apps/_MyApp_/src/Spawns/`) | 텍스트 렌더링은 재사용 기술 → Core. 데미지 스폰은 게임별 → Client |
| **D5** | **폰트 = `minogram_6x10`** BMFont(PNG+XML). 균일 13×7 그리드라 `UniformAtlas` 재사용 성립. XML은 의존성 0 최소 파서로 읽음 | 사용자 제공 에셋. assimp 외 standalone XML 라이브러리 부재 |
| **D6** | **범위 = 프리미티브 + 데모 트리거**. 전투 배선(CombatSequences) 제외 | 사용자 선택 (깔끔한 스코프) |

### 1.3 공개 식별자 (확정)

```
Core  src/text/  →  SJH::text
  ├─ SJH::Text::BitmapFont        // BMFont(PNG+XML) 로더 + codepoint→frame/advance
  └─ SJH::Text::TextRenderer      // Component — glyph child Actor 조립 (α)

Client apps/_MyApp_/src/Text/   →  MyApp::Text   (Manager 보유)
  └─ TopdownShooter::Text::WorldTextSystem        // BitmapFont 1개 보유 (VFX/Audio 시스템 평행)
Client apps/_MyApp_/src/Spawns/  (MyApp::Spawns)
  ├─ TopdownShooter::Spawns::WorldTextStyle       // {color,charHeight,riseHeight,durationSec,fadeStart}
  └─ TopdownShooter::Spawns::SpawnWorldText(...)   // VfxInstance 패턴 — 애니메이션+자동 despawn
```

### 1.4 범위 밖 (YAGNI)
비례폰트/커닝(minogram은 monospace), 워드랩·멀티라인, 유니코드(BMP 밖), 인라인 리치텍스트(색상 토큰), 오브젝트 풀링(필요 시 추후), β 단일메시 백엔드, HUD/스크린 고정 텍스트, 지속(persistent) 라벨, 전투 시스템 배선.

### 1.5 비기능 제약 (코드베이스 검증됨)
- **Actor 타입당 컴포넌트 1개** (`unordered_map<type_index>` + duplicate assert) → 글리프는 child Actor, rise+fade는 **단일** `TweenPlayable<float>`.
- **iterator 안전** — child Add/Remove는 Update 트리 순회 밖에서만 (스폰 1회 / 기존 `OneShotSweeper` 규율 동일).
- **크로스플랫폼** — `long` 금지(고정폭 정수), 경로 슬래시, 파일 I/O 바이너리, `windows.h` 가드. (CLAUDE.md 규칙)
- **tweeny `step(int32_t ms)` 강제** — `TweenPlayable`이 이미 준수 (`tweeny_step_overload_trap`).

---

## 2. `BitmapFont` (Core `SJH::Text::BitmapFont`)

### 2.1 책임
BMFont(AngelCode) PNG+XML 폰트를 로드해 **`codepoint → {frameIndex, xadvance}` 매핑 + 공유 `UniformAtlas` 참조**를 제공한다. 글리프 렌더는 TextRenderer가 `SpriteRenderer`로 수행하므로 **BitmapFont는 데이터만 보유(렌더 안 함)**.

### 2.2 소유·생성 (사이클 회피 — 강제 결정)
- BitmapFont는 **caller 소유** (ResourceRegistry 캐시 ❌). 이유: `resource_registry → text → sprite → resource_registry` **순환** 회피. 실제 보유자 = **`WorldTextSystem`** (Client, §4).
- 내부 `UniformAtlas`는 **레지스트리 캐시** — `LoadFromBMFont(reg, key, png, xml)`가 XML에서 그리드(cols = scaleW/cellW, rows = scaleH/cellH) 도출 후 `reg.CreateUniformAtlas(key, png, cols, rows)` 호출 (Find 우선, 중복키 공유).
- 필터 = 픽셀아트 **NEAREST** (UniformAtlas 기본 — memory `uniform_atlas_delegates_image_texture`).
- 의존 방향: `SJH::text → SJH::resource_registry` (직접). resource_registry는 text 비의존 → 무순환.

### 2.3 BMFont XML 최소 파서 (의존성 0)
- standalone XML 라이브러리 부재(pugixml은 assimp 내부 전용) → `bitmap_font.cpp`에 **최소 BMFont 리더** 내장(~40줄): `<char .../>` 라인별 속성 스캔.
- 추출: `<common scaleW scaleH lineHeight/>`, 각 `<char id x y width height xadvance/>`.
- 파생: `frameIndex = (y / cellH) * cols + (x / cellW)` — **균일 그리드 전제**(모든 글리프 동일 w/h 검증, 위반 시 warn). `cellW/cellH = 첫 char width/height`.
- 파일 I/O **바이너리 모드**, 경로 슬래시 (크로스플랫폼). `long` 미사용.
- ⚠ **범용 XML 파서 아님** — frostyfreeze/BMFont 균일그리드 서브셋 전용 (헤더 주석 명시).

### 2.4 공개 API (초안)
```cpp
namespace SJH::Text {
  struct Glyph { int frameIndex; int xadvance; };       // xadvance = px

  class BitmapFont {
  public:
    // reg 로 내부 UniformAtlas 생성/캐시 + XML 파싱. 실패 시 빈 폰트(경고 후 '?'/space 부재).
    static BitmapFont LoadFromBMFont(SJH::ResourceRegistry& reg, const std::string& key,
                                     const std::string& pngPath, const std::string& xmlPath);
    const Glyph* Find(uint32_t cp) const;                // 없으면 nullptr
    int   FrameOf(uint32_t cp) const;                    // 없으면 '?'→space fallback (§2.5)
    int   AdvanceOf(uint32_t cp) const;                  // px (공백=4, 숫자=6)
    Sprite::UniformAtlas* GetAtlas() const;
    int   LineHeight() const;
    int   CellW() const;  int CellH() const;             // px (TextRenderer aspect 도출용)
  private:
    Sprite::UniformAtlas* mAtlas = nullptr;              // 비소유 (registry 보유)
    std::unordered_map<uint32_t, Glyph> mGlyphs;
    int mLineHeight = 0, mCellW = 0, mCellH = 0;
  };
}
```

### 2.5 미존재 글리프 폴백 (D-2.3 = '?')
`FrameOf(cp)`: cp 없으면 → `'?'`(id 63, **존재 검증됨**) → 없으면 `space`(32) → 없으면 `-1`(skip). 최초 1회 spdlog warn-once.

### 2.6 `minogram_6x10` 사실 (검증 완료)
- PNG 78×70, glyph 6×10 균일 → 13열 × 7행 = 91칸 = 91 글자. lineHeight 12.
- 데미지 문자 **전부 존재**: `0-9`(48–57), `+`(43), `-`(45), ` `(32, **advance=4**), `?`(63).
- → `AdvanceOf`는 **per-glyph** 저장 필수(공백 4 ≠ 숫자 6). 단일 상수 spacing 금지.

---

## 3. `TextRenderer` (Core `SJH::Text::TextRenderer : SJH::Scene::Component`)

### 3.1 책임 (α — 글리프 = child Actor)
문자열을 **글자마다 child Actor**(각자 `SpriteRenderer` 1개)로 펼쳐 owner Actor 밑에 부착한다. 각 글리프 쿼드는 **기존 `billboard_atlas` 셰이더로 GPU 빌보드**(카메라 정면). TextRenderer 자신은 렌더 컴포넌트가 아니라 **글리프 child 오케스트레이터**.

> Actor 타입당 컴포넌트 1개 제약 → 한 Actor에 SpriteRenderer 여럿 불가 → 글리프마다 child Actor (각 SpriteRenderer 1개). §1.5 검증.

### 3.2 결정 (D-3.x)
| # | 결정 |
|---|---|
| D-3.1 | **Center 정렬 고정** (enum 없음 — YAGNI). |
| D-3.2 | **앵커 = 스폰점 하단중앙** — 텍스트 바닥 변이 owner 원점(y=0), 위(+Y)로 확장. |
| D-3.3 | **가로 = 월드 X축 고정 배치 (방식 A)**, **상승 = 월드 +Y**. SetText 시 1회 배치(per-frame 재배치 없음). |
| D-3.4 | **글리프 aspect 6:10 보존** — `glyphW = charHeight × cellW/cellH`. |
| D-3.5 | char 크기 = `charHeight`(월드 단위), 기본 0.5(튜너블), `SpawnWorldText` style이 주입. |

### 3.3 공개 API (초안)
```cpp
namespace SJH::Text {
  class TextRenderer : public SJH::Scene::Component {
  public:
    explicit TextRenderer(const BitmapFont* font);
    void SetText(const std::string& s);          // 글리프 child 재구성 (Rebuild)
    void SetColor(const vmath::vec4& rgba);       // 모든 글리프 SpriteRenderer.tint
    void SetAlpha(float a);                        // fade — 모든 글리프 tint.a (TweenPlayable이 구동)
    void SetCharHeight(float worldH);             // 폭 = worldH × cellW/cellH
    void OnEnter() override;  void OnExit() override;
  private:
    void Rebuild();                               // 기존 child 제거 후 재빌드 (스폰 시 1회)
    const BitmapFont* mFont = nullptr;
    std::string mText;  float mCharHeight = 0.5f;  vmath::vec4 mColor{1,1,1,1};
    std::vector<SJH::Scene::Actor*> mGlyphs;      // 비소유 (owner children 이 소유)
  };
}
```
> **TweenPlayable 비의존** — SetAlpha/SetColor만 노출, 구동은 Client. Core `SJH::text`는 tweeny 링크 안 함.

### 3.4 배치 알고리즘 (center, 하단중앙 앵커, 월드 X)
```
worldPerPx = charHeight / cellH
glyphW     = cellW * worldPerPx
totalW     = Σ AdvanceOf(c_i) * worldPerPx
penX       = -totalW / 2                          // center
for c_i in text:
    frame = font.FrameOf(c_i)                     // '?'→space fallback, -1=skip
    if frame >= 0:
        child = owner.AddChild(Actor "glyph")
        child.Transform.Translate = (penX + glyphW/2, charHeight/2, 0)  // 하단중앙: 바닥 y=0 (실제 필드명 Translate)
        child.Transform.Scale    = (glyphW, charHeight, 1)             // 빌보드 sx/sy
        child.Layer              = Layer::Default                       // 카메라 cullingMask 포함
        sr = child.AddComponent<SpriteRenderer>(font.GetAtlas())
        sr.frameIdx = frame;  sr.tint = mColor
        mGlyphs.push_back(child)
    penX += AdvanceOf(c_i) * worldPerPx
```
- `SpriteRenderer(atlas)`가 `_sprite_*` 공유자원(plane/billboard program/AlphaTest material) 자동 lazy 해결 (EngineAPI §3.13) — 수동 Material 불필요.
- 글리프 child EulerRot.z=0 → 빌보드 roll 0(똑바른 글자).
- **Rebuild는 스폰 시 1회** (Update 트리 순회 밖 — iterator 안전, §1.5).
- **호출 순서** — `SetCharHeight`는 글리프 크기를 child Scale에 baked하므로 **`SetText` 전** 설정. `SetColor`/`SetAlpha`는 기존 글리프에 broadcast라 **언제든**(SetText 후 포함) 가능. (`SpawnWorldText`는 CharHeight → Color → Text 순 — §4.4.)

### 3.5 알려진 한계 (D-3.3 방식 A)
- 카메라 **yaw(궤도 회전)** 시 텍스트 행이 월드 X와 함께 회전 → 큰 yaw에서 글자 겹침. 데미지 텍스트 단명(≈0.8s)이라 체감 적음. **yaw-robust가 필요해지면 β(단일메시+per-vertex offset 셰이더)로 승격** (D3 메모와 동일 경로). 상승(+Y)은 yaw 무관 정상.

---

## 4. Client — `WorldTextStyle` + `WorldTextSystem` + `SpawnWorldText`

### 4.1 거주 (Client — Core `SJH::text` 와 분리)
- `WorldTextSystem` (font 보유) → `apps/_MyApp_/src/Text/WorldTextSystem.{h,cpp}` (VFX/VFXSystem·Audio 시스템 선례 평행). `Manager` 가 보유(`Manager::Get().VFX()` 패턴 동일).
- `WorldTextStyle` + `SpawnWorldText` → `apps/_MyApp_/src/Spawns/WorldTextInstance.{h,cpp}` (`VfxInstance`/`AudioInstance` 와 동일 위치·패턴).
- namespace `TopdownShooter::{Text, Spawns}`.

### 4.2 `WorldTextStyle` (D-4.2 = 구조체 파라미터)
```cpp
struct WorldTextStyle {
    vmath::vec4 color       = {1,1,1,1};
    float       charHeight  = 0.5f;    // 월드 단위 (글리프 높이)
    float       riseHeight  = 0.7f;    // 월드 +Y 상승량
    float       durationSec = 0.9f;    // 수명
    float       fadeStart    = 0.45f;  // 진행률 0..1 — 이 지점부터 alpha 1→0
};
```

### 4.3 `WorldTextSystem` (얇은 — font 1개 보유)
```cpp
class WorldTextSystem {
  public:
    void Init();                              // minogram BMFont 1회 로드 (내부 ResourceRegistry::Get())
    SJH::Text::BitmapFont* GetFont();         // nullptr if !Init
  private:
    SJH::Text::BitmapFont mFont;              // 값 보유 (caller 소유 — §2.2)
    bool mLoaded = false;
};
// Init(): auto& reg = SJH::ResourceRegistry::Get();
//         mFont = BitmapFont::LoadFromBMFont(reg, "minogram_6x10",
//             "resources/font/minogram_6x10.png", "resources/font/minogram_6x10.xml");
```

> **구현 정정 (Init 무인자)**: 원안은 `Init(ResourceRegistry&)` 였으나 구현 중 GL 헤더 충돌 발견 — `Manager.cpp`(VFX→Effekseer 가 macOS `gl3.h` 포함)에 `resource_registry.h`(gl3w.h)를 추가하면 `gl.h + gl3.h` 동시 포함으로 `PFNGLGETPOINTERVPROC` 미정의 컴파일 에러. 해법: `Init()` 을 **무인자**로 바꿔 `ResourceRegistry::Get()` 호출을 `WorldTextSystem.cpp`(Effekseer 비의존) 안으로 격리 → `Manager.cpp` 가 gl3w.h 를 안 끌어오게 함. Manager 는 `mWorldText.Init()` 만 호출.

### 4.4 `SpawnWorldText` (VfxInstance 패턴 — 애니메이션 + 자동 despawn)
```cpp
void SpawnWorldText(SJH::Scene::Actor& fxParent, SJH::Text::BitmapFont* font,
                    const vmath::vec3& worldPos, const std::string& text,
                    const WorldTextStyle& style);
```
구현:
```cpp
if (!font) return;                                            // 폰트 미존재 — no-op (VfxInstance 동일)
auto* a  = fxParent.AddChild(std::make_unique<Actor>("WorldText"));
a->GetTransform().Translate = worldPos;                       // 앵커 = 하단중앙
auto* tr = a->AddComponent<SJH::Text::TextRenderer>(font);
tr->SetCharHeight(style.charHeight);
tr->SetColor(style.color);
tr->SetText(text);                                            // 글리프 child 빌드

// 단일 progress 트윈 0→1 — 상승+페이드 동시 (one-shot)
auto tween = tweeny::from(0.0f).to(1.0f).during(static_cast<int32_t>(style.durationSec * 1000.0f));
const float baseY = worldPos[1];
auto* tw = a->AddComponent<Tween::TweenPlayable<float>>(
    std::move(tween),
    [a, tr, baseY, style](float t) {
        const float e = 1.0f - (1.0f - t) * (1.0f - t);       // easeOutQuad (팝→감속)
        a->GetTransform().Translate[1] = baseY + style.riseHeight * e;
        const float alpha = (t < style.fadeStart) ? 1.0f
                          : 1.0f - (t - style.fadeStart) / (1.0f - style.fadeStart);
        tr->SetAlpha(alpha);                                  // 후반 페이드
    });
tw->SetIsLoop(false);                                         // t≥1 → finished_
a->AddComponent<Spawns::AutoDespawnOnFinish>(tw);             // 종료 감지
tw->Play();
```
- 캡처 `a`/`tr` = `a` 가 소유하는 컴포넌트가 `a`(+그 자신)를 참조 — `a` 가 despawn까지 생존하므로 안전(VfxInstance 동일 수명 규율).
- `tweeny.step(int32_t ms)` 강제 → `TweenPlayable` 내부 준수 (`during(ms)` int). **float step 금지** (`tweeny_step_overload_trap`).
- easing은 onStep 람다에서 적용(트윈은 선형 유지) — 가독성.

### 4.5 자동 despawn (기존 sweeper 재사용 — 신규 0)
- `AutoDespawnOnFinish(tw)` 가 `tw.finished_` → `mDone`. **이미 매 프레임 도는** `SweepFinishedChildren(*mFxRoot)` (main.cpp:354) 가 done child를 `RemoveChild` → Actor + 글리프 일괄 소멸. **새 sweeper/parent 불필요**.
- `fxParent = mFxRoot` (VFX/Audio one-shot 과 동일 부모).

### 4.6 데모 트리거 (D-4.3 = 마우스 클릭 위치)
- main.cpp 의 **기존 마우스 클릭 → 피킹 worldPos `p` → `mFxRoot` 에 VFX 스폰** 핸들러(main.cpp:313 부근)에 한 줄 추가:
```cpp
TopdownShooter::Spawns::SpawnWorldText(*mFxRoot, Manager::Get().WorldText().GetFont(),
                                       p, demoDamageString(), WorldTextStyle{});
```
- `demoDamageString()` = 데모용 임시 값(예: 카운터/난수 기반 `"-10"`, `"+5"`). 포맷은 **caller 책임**(프리미티브는 `std::string` 그대로 렌더 — 범용).
- 전투 배선(CombatSequences) 은 §1.4 범위 밖.

---

## 5. CMake · 모듈 배선

### 5.1 Core 신규 모듈 `SJH::text` (16 → 17)
```cmake
# src/text/CMakeLists.txt (신규)
add_library(sjh_text STATIC bitmap_font.cpp text_renderer.cpp)
add_library(SJH::text ALIAS sjh_text)
target_include_directories(sjh_text PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(sjh_text
    PUBLIC  SJH::scene SJH::sprite SJH::resource_registry project_deps
    PRIVATE spdlog)            # bitmap_font.cpp warn-once
```
- `src/CMakeLists.txt`: `add_subdirectory(text)` + 우산 `sjhopengl_engine` INTERFACE 에 `SJH::text` 추가.
- **무순환 검증**: `text → {scene, sprite, resource_registry}`. 이들 CMake 미변경 → 누구도 text 역의존 X.

### 5.2 Client
- `apps/_MyApp_/src/Text/CMakeLists.txt` (신규): `MyApp::Text` STATIC(`WorldTextSystem.cpp`) → PUBLIC `SJH::text` `SJH::resource_registry`.
- `apps/_MyApp_/src/CMakeLists.txt`: `add_subdirectory(Text)` (**Manager 앞** — Manager가 의존), `MyApp::Manager` + `MyApp::Client` 우산에 `MyApp::Text` 링크.
- `apps/_MyApp_/src/Spawns/CMakeLists.txt`: `WorldTextInstance.cpp` 추가 + `SJH::text` PUBLIC (BitmapFont* 헤더 노출). `MyApp::Tween` 이미 링크됨.
- `Manager.{h,cpp}`: `mWorldText` 멤버 + `WorldText()` 접근자 + `Init()` 에서 `mWorldText.Init(reg)`.
- `main.cpp`: 데모 트리거 1줄(§4.6).

### 5.3 리소스 배포 (작업 0)
`resources/font/minogram_6x10.{png,xml}` 이미 존재 → POST_BUILD `copy_directory resources`(apps/_MyApp_/CMakeLists.txt:61–64)가 **자동 배포**. 추가 작업 없음.

---

## 6. 데이터 흐름 · 의존 그래프

### 6.1 spawn → despawn (end-to-end)
```
[데모] 마우스 클릭 → 피킹 worldPos p
   └→ SpawnWorldText(*mFxRoot, Manager.WorldText().GetFont(), p, "-10", style)
        1. Actor("WorldText") = mFxRoot.AddChild ; Transform.Translate = p   (하단중앙 앵커)
        2. TextRenderer(font) 부착 → SetCharHeight/SetColor/SetText
              └→ 글자마다 child Actor + SpriteRenderer(atlas, frameIdx) 빌드 (월드 X, center)
        3. TweenPlayable<float>(0→1, durationSec) 부착, onStep: +Y 상승(easeOut) + 후반 alpha fade
        4. AutoDespawnOnFinish(tween) 부착 → Play
   [매 프레임] PlayableBase.Update → 상승+fade ; t≥1 → finished_ → AutoDespawn.mDone
   [매 프레임] SweepFinishedChildren(*mFxRoot) → done child RemoveChild → Actor+글리프 소멸
```

### 6.2 모듈 의존 그래프 (델타)
```
SJH::text (신규)
  ├── SJH::scene             (Component/Actor — TextRenderer 베이스)
  ├── SJH::sprite            (SpriteRenderer/UniformAtlas — 글리프 렌더)
  └── SJH::resource_registry (UniformAtlas 캐시 — BitmapFont)
        ↑ (역의존 없음 — 무순환)

MyApp::Text (신규, Client) ── SJH::text, SJH::resource_registry
MyApp::Spawns (+WorldTextInstance) ── + SJH::text
MyApp::Manager ── + MyApp::Text
```

---

## 7. 검증 (수동 GUI — `no_auto_tests` 준수)
- **빌드**: `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_` (+ MSVC 교차 빌드 확인 — `long` 미사용/슬래시/바이너리 I/O).
- **실행**: `cd build_ninja/apps/_MyApp_ && ./_MyApp_`.
- **시나리오**: 월드 클릭 → 픽킹 지점에 텍스트 spawn → 위로 떠오름 + 후반 페이드 → ≈0.9s 후 despawn.
- **누수**: `sh <shell>/CMakeExecute.sh debug _MyApp_ leaks` — despawn 후 글리프 Actor 회수 확인.
- **포맷**: `.clang-format` (Microsoft, Tab=4, ColumnLimit=0). GL 골든/Catch2 단위테스트 **없음**.

---

## 8. 제약 · 회귀 방지 · 후속

### 8.1 회귀 0 표면 (순수 가산)
신규 파일/모듈만 추가. 기존 코드 편집은 가산뿐: `src/CMakeLists`(+1 모듈), `Spawns/CMake`(+1 src,+1 link), `apps src CMake`(+1 subdir,+2 link), `Manager`(+1 system), `main`(+1 트리거 줄). **기존 동작 무변경**.

### 8.2 가드레일 (memory `next_work_playable`)
`main.cpp` Fog / `enemy_factory`(Task5) / Timer 코어 / **셰이더 미접근** — 본 작업은 main 에 트리거 1줄(기존 픽킹 핸들러 내), `billboard_atlas` 셰이더 **재사용**(무변경), `enemy_factory` 무관. path-scoped 추가. Co-Authored-By 미사용.

### 8.3 후속 (범위 밖)
- β 승격 — yaw-robust 단일메시 + per-vertex offset 셰이더 (1 draw call, 통합 tint fade).
- 전투 배선 — `CombatSequences::OnEnemyHit/Death` → `SpawnWorldText`.
- 프리셋 스타일(Damage/Heal/Crit), 오브젝트 풀링, 멀티라인/리치텍스트/유니코드.

---

## 9. 구현 Task 분해 (→ writing-plans 입력)

| Task | 내용 | 의존 |
|---|---|---|
| **T1** | Core `SJH::text` 스캐폴드 — `src/text/CMakeLists.txt` + `bitmap_font.{h,cpp}`(BMFont 최소 파서 + UniformAtlas 그리드 + codepoint→frame/advance + '?' fallback) | — |
| **T2** | `text_renderer.{h,cpp}` — 글리프 child 조립(§3.4) + Set{Text,Color,Alpha,CharHeight} | T1 |
| **T3** | `src/CMakeLists.txt` 우산 배선(16→17) + 빌드 그린(consumer 0이라도 컴파일) | T2 |
| **T4** | Client `MyApp::Text` `WorldTextSystem` + Manager 배선(Init 폰트 로드) | T3 |
| **T5** | `Spawns/WorldTextInstance.{h,cpp}` — `WorldTextStyle` + `SpawnWorldText`(트윈+AutoDespawnOnFinish) | T2·T4 |
| **T6** | main 데모 트리거(픽킹 핸들러) + 수동 GUI 검증 | T5 |

> 빌드 그린 게이트: **T3 직후 Core 단독 컴파일 확인**(consumer 없어도), **T6 직후 GUI 검증**.

---

## 10. 미해결 / 가정
- **가정**: minogram 글리프가 균일 6×10 그리드(검증됨). 비균일 BMFont는 §2.3 파서가 warn(범위 밖).
- **가정**: 카메라 yaw가 데미지 텍스트 수명(≈0.9s) 동안 과도하지 않음(방식 A, §3.5). 위반 시 β 승격.
- **열림**: `charHeight`/`riseHeight` 기본값은 게임 월드 스케일에 맞춰 데모에서 1차 튜닝 필요(구현 T6에서 확정).

