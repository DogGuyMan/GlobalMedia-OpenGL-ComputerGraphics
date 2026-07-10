# 2.5D 탑다운 슈터 엔진 구현 지침서 (v2 — 실제 아키텍처 반영)

> Cult of the Lamb 스타일 빌보드 스프라이트 게임. 본 문서는 **실제로 구현된 아키텍처**를 기준으로 작성됨.

| 항목 | 내용 |
|---|---|
| 최초 작성 | 2026-05-04 (초기 리서치 — EnTT/FetchContent/GLM 가정) |
| **전면 재작성** | **2026-05-31 (실제 아키텍처로 동기화)** |
| 대상 환경 | macOS Apple Silicon (Ninja) + Windows MSVC, **OpenGL 4.1 Core / GLSL 410**, C++17 |
| 빌드 시스템 | **CMake 단독** — `lib/`·`include/` 사전 빌드 산출물 체크인 (**vcpkg / FetchContent 미사용**) |
| 활성 데모 | `apps/_MyApp_` (탑다운 슈터 정본). 그 외 `migrate_demo`/`audio_demo`/`box2d_demo`/`effekseer_demo`/`tweeny_demo` 는 `apps/CMakeLists.txt` 에서 옵트인 (현재 `_MyApp_` 만 활성) |
| 정본 문서 | `.claude/CLAUDE.md` (아키텍처/컨벤션), `doc/topdown-shooter-progress.md` (마일스톤), `doc/superpowers/specs/` (설계 스펙 20종) |

> ⚠️ **이 문서의 v1(2026-05-04) 본문은 초기 기획이었고, 프로젝트는 그 후 8개 핵심 스택에서 분기했다.** (EnTT→OOP Actor+Component, FetchContent→prebuilt lib, glad→gl3w, GLM→vmath, Box2D 3.x C API→2.4.1 C++ API, Effekseer 1.80.2→1.7.3.0, GL 3.3→4.1/GLSL 410, Application/MainWindow→sb7::application+Manager). 전체 분기 매핑은 [§0.3](#03-초기-기획--실제-구현-분기-매핑-핵심) 참조. 초기 기획의 *의사결정 근거*(Spine/Rive 배제 등)는 여전히 유효하므로 [§0.4](#04-보존된-의사결정-근거-여전히-유효)에 보존.

---

## 신뢰도 표기 범례

| 표기 | 의미 |
|---|---|
| ✅ **검증** | 실제 소스 헤더/`CLAUDE.md`/spec 으로 직접 확인. |
| 🟡 **부분검증** | spec 기반 — 정확한 시그니처는 헤더 직접 참조 권장 (spec↔코드 드리프트 가능). |
| 🟠 **추정** | 일반 패턴 + 유사 사례. 직접 확인 필요. |
| ⚠️ **정정/주의** | v1(초기 기획) 의 오류 수정 또는 미완 상태 경고. |

---

## 목차

- [제0장 — 개요와 초기 기획 분기](#제0장--개요와-초기-기획-분기)
- [제1장 — 빌드 시스템 (CMake 단독 + prebuilt)](#제1장--빌드-시스템-cmake-단독--prebuilt)
- [제2장 — 렌더 파이프라인 (DeviceContext + SceneRenderer + IRenderStage)](#제2장--렌더-파이프라인-devicecontext--scenerenderer--irenderstage)
- [제3장 — 씬 그래프 (OOP Actor + Component)](#제3장--씬-그래프-oop-actor--component)
- [제4장 — 물리 (Box2D v2.4.1, Client 한정)](#제4장--물리-box2d-v241-client-한정)
- [제5장 — 파티클 (Effekseer)](#제5장--파티클-effekseer)
- [제6장 — FSM + Playable (시간축 추상화)](#제6장--fsm--playable-시간축-추상화)
- [제7장 — Tweeny 트위닝](#제7장--tweeny-트위닝)
- [제8장 — 음향 (FMOD Core + Studio)](#제8장--음향-fmod-core--studio)
- [제9장 — 통합 흐름 (Manager + main.cpp)](#제9장--통합-흐름-manager--maincpp)
- [부록 A — 디렉토리 구조 (실제)](#부록-a--디렉토리-구조-실제)
- [부록 B — 함정 모음 (Caveats)](#부록-b--함정-모음-caveats)
- [부록 C — 미진행/잔여 작업](#부록-c--미진행잔여-작업)
- [부록 D — 참고 자료](#부록-d--참고-자료)

---

## 제0장 — 개요와 초기 기획 분기

### 0.1 게임 컨셉

- **장르**: 2.5D 탑다운 슈터, Cult of the Lamb 스타일 (3D 환경 + 빌보드 2D 스프라이트 캐릭터)
- **카메라**: 고정각 탑다운 (perspective World Camera) + 화면 공간 Orthographic Screen Camera (PostFX 합성용 2-Camera 구조)
- **캐릭터**: 빌보드 quad + frame-by-frame atlas 스프라이트
- **게임 상태**: Title → Combat → Boss → End (Stage FSM — `Stage/State/` stub 단계)

### 0.2 실제 라이브러리 스택 ✅

| 카테고리 | 실제 선정 | 통합 방식 |
|---|---|---|
| 윈도우/입력 | **GLFW** (sb7 vendored 3.0.4) | prebuilt `lib/` 체크인 |
| OpenGL 로더 | **gl3w** (sb7 번들) | `include/GL/gl3w.h` |
| 베이스 앱 | **sb7::application** (SuperBible 7) | prebuilt `sb7` lib |
| 수학 | **vmath.h** (sb7 번들) | `include/vmath.h` (GLM 아님) |
| 이미지 디코딩 | **stb_image** | `extern/stb` 헤더, 단일 owner = `src/resource_registry/image.cpp` |
| 모델 로딩 | **Assimp v5.4.3** | prebuilt STATIC, `game_deps` |
| **씬/게임 로직** | **자체 OOP Actor + Component** (Cocos2D `cc.Node`/`cc.Component` 정통 + Unreal 영감) | `SJH::scene` 모듈 (**EnTT 아님** — [[entt_removed]]) |
| **2D 물리** | **Box2D v2.4.1** (C++ API: `b2World*`/`b2Body*`) | prebuilt STATIC, `game_deps`, *Client 한정* |
| **파티클** | **Effekseer 1.7.3.0** + EffekseerRendererGL | prebuilt STATIC, `game_deps` |
| **트위닝** | **Tweeny** (헤더 온리) | `extern/tweeny` |
| **음향** | **FMOD Core + Studio** (`.bank` 이벤트) | dynamic-only, 조건부 `game_deps` |
| 로깅 | **spdlog v1.17.0** | prebuilt STATIC `game_deps` (코어 모듈은 자체 `log_util.h`) |
| 시간축 추상 | **자체 `SJH::playable`** (IPlayable/Composite) + **`SJH::fsm`** | 코어 모듈 |

### 0.3 초기 기획 → 실제 구현 분기 매핑 (핵심) ⚠️

| 영역 | v1 초기 기획 (2026-05-04) | 실제 구현 | 근거 |
|---|---|---|---|
| 게임 로직 | EnTT v3 데이터 ECS (`entt::registry`) | **OOP Actor+Component 씬 그래프** | [[entt_removed]], spec `2026-05-21-sp3-ecs-render-system` |
| 빌드 | CMake FetchContent (소스 빌드) | **CMake 단독 + prebuilt `lib/`·`include/` 체크인** | `CLAUDE.md`, 교수 제출용 |
| GL 로더 | glad v0.1.34 | **gl3w** (sb7 번들) | `CLAUDE.md` project_deps |
| 수학 | GLM | **vmath.h** (sb7) | `CLAUDE.md` |
| GL 버전 | OpenGL 3.3 Core | **OpenGL 4.1 Core / GLSL 410** | [[glsl_410_project_policy]] |
| 물리 | Box2D 3.1.1 C API (`b2BodyId` 핸들) | **Box2D v2.4.1 C++ API** (`b2World*`/`b2Body*`/`b2ContactListener`) | `CLAUDE.md`, `doc/Box2DAPI.md` |
| 파티클 | Effekseer 1.80.2 | **Effekseer 1.7.3.0** | `CLAUDE.md` |
| 음향 | FMOD Core only | **FMOD Core + Studio** (`.bank`) | `CLAUDE.md`, `apps/audio_demo` |
| 베이스 클래스 | `Application` + `MainWindow` | **`sb7::application`** + Client `Manager` 싱글톤 | `apps/_MyApp_/main.cpp` |
| 렌더 게이트웨이 | (없음) | **`SJH::DeviceContext`** 싱글톤 | `src/render/device_context.h` |
| 렌더 오케스트레이션 | `RenderSystem` | **`SceneRenderer`** (`IRenderStage`) + `MeshPassProcessor` | `src/render/scene_renderer.h` |

> 즉 v1 의 *3장(EnTT)·4장(Box2D 3.x)·6장(EnTT 시스템 순회)* 은 **실제와 다름**. 본 v2 가 해당 장을 실제 구조로 대체.

### 0.4 보존된 의사결정 근거 (여전히 유효)

다음 v1 의사결정은 변함없이 유효하다:

- **Spine 미도입**: 라이선스 \$69~\$379 + Cult of the Lamb 룩은 frame-by-frame 이 정석. (✅ esotericsoftware.com)
- **Rive 미도입**: macOS OpenGL 4.1 < Rive 요구 4.2+ — 하드 차단. (✅ Defold 문서)
- **자체 atlas**: stb_image 외 의존성 최소 + Tweeny(연속)와 frame(이산) 분업 명확. → 실제로 `SJH::sprite::UniformAtlas` 로 구현됨.
- **Tweeny 채택**: 30+ Penner easing 단일 헤더. → 실제로 `TweenPlayable` leaf 로 구현됨. ⚠️ `step(int32 ms)` vs `step(float ratio)` 함정 ([[tweeny_step_overload_trap]]).
- **FMOD 채택**: Free Indie + 크로스플랫폼. → Core + Studio 둘 다 도입.

---

## 제1장 — 빌드 시스템 (CMake 단독 + prebuilt)

### 1.1 정책 ✅

교수 제출용으로 **vcpkg / FetchContent 미사용**. 모든 서드파티는 `lib/{macos,windows}/` 사전 빌드 정적 라이브러리 + `include/` 헤더로 체크인되어, CMake 단독으로 완결된다. (v1 의 FetchContent 가정은 폐기.)

### 1.2 빌드 명령

```bash
# Configure
cmake --preset ninja                                 # Debug (macOS/Linux)
cmake --preset ninja-release                         # Release
cmake --preset msvc-2022                             # Windows VS2022

# 빌드 + 실행 (리소스 상대경로 때문에 cd 필요)
cmake --build --preset ninja --target _MyApp_
cd build_ninja/apps/_MyApp_ && ./_MyApp_

# 개발 CLI (구 shell helper 통합)
python3 scripts/dev.py all debug _MyApp_             # clean + configure + build + run
python3 scripts/dev.py run debug _MyApp_ --leaks     # macOS leaks 체크
```

### 1.3 의존성 계층 (`cmake/Dependency.cmake`) ✅

- **`project_deps`** (INTERFACE) = sb7 + glfw3 + OpenGL + 플랫폼 프레임워크. 모든 데모가 링크.
- **`game_deps`** (INTERFACE) = Box2D + Effekseer + EffekseerRendererGL + assimp + spdlog + Tweeny + stb + (조건부) FMOD Core/Studio. 게임 챕터만 옵트인.
  - FMOD 는 dynamic-only → `game_deps` 사용 챕터는 POST_BUILD 에서 `$<TARGET_FILE:fmod>` (+ fmodstudio) 를 실행 파일 옆으로 `copy_if_different` **필수**.
  - `game_deps` 의 `include/` 는 SYSTEM 인클루드 → 서드파티 헤더가 Debug `-Werror` 에서 제외.

### 1.4 코어 모듈 — `SJH::<module>` 15 STATIC + `SJH::engine` 우산 ✅

`src/` 의 15개 코어 모듈이 `SJH::<module>` ALIAS 로 노출되고, 전체를 묶은 INTERFACE 우산 `SJH::engine` 로 한 줄 링크 가능 (`target_link_libraries(<demo> PRIVATE project_deps SJH::engine)`). Cocos `libcocos2d.a` / Unreal Runtime 통합 바이너리 정통.

| 모듈 | 책임 |
|---|---|
| `common` | 공통 유틸 (`common.h`, `constants.h` — `MAX_POINT_LIGHTS`/`MAX_SPOT_LIGHTS=16` 등) |
| `diagnostics` | GL 호출/셰이더/uniform/상태 진단 |
| `buffer` | VBO/EBO/Framebuffer RAII |
| `shader` / `program` | 셰이더 컴파일 / 프로그램 링킹 + uniform 캐시 (`mUniformCache`) |
| `layout` | Vertex 레이아웃 + VAO |
| `material` | Material 값 클래스 + `Texture*`/`Program*` 보관 |
| `object` | Mesh + Geometry 생성기 + `Light`/`Transform` |
| `scene` | **Actor + Component + Scene(Director) + SceneContext** (씬 그래프) |
| `sprite` | 2D atlas (`UniformAtlas`) + `SpriteRenderer` + `SpriteSequencePlayable` + `SpriteFrameClip` (sprite_sequence 통합) |
| `fsm` | `StateMachine<TState,TOwner>` + `IFsmState<TOwner>` |
| `playable` | `IPlayable` + `PlayableBase` + Composite(Sequence/Parallel) + `IntervalPlayable` |
| `render` | **DeviceContext + SceneRenderer + IRenderStage 계열 + MeshPassProcessor + PassComponent + LightUniformDispatcher** |
| `input` | KeyboardInput/MouseInput 디스패치 |
| `resource_registry` | 자원 캐시 — Texture / Material(shared+instance) / Model / Program / Mesh / Framebuffer / UniformAtlas / **Sound / Effect**. `game_deps` PUBLIC ([[resource_registry_game_deps]]) |

> ✅ 빌드 순서 확정 (`src/CMakeLists.txt`): buffer object common diagnostics layout program material scene sprite fsm shader render resource_registry input playable → `SJH::engine` 우산 합류. leaf Playable (Fmod/Effekseer/Tween) 과 물리(Box2D)·게임 로직은 **Client(`apps/_MyApp_/src/`) 거주** — 코어 `SJH::` 모듈은 게임 도메인 비의존.

### 1.5 챕터 패턴 (A/B) ✅

- **패턴 A**: `entry.h`+`entry.cpp`+`main.cpp` 분리 (sb7::application 상속을 별도 TU). `_MyApp_` 변형 사용.
- **패턴 B**: `main.cpp` 단일 파일 + `DECLARE_MAIN(...)` (신규 데모 기본).

새 데모: `apps/<name>/` 생성 → `main.cpp` + `CMakeLists.txt` → `apps/CMakeLists.txt` 에 `add_subdirectory(<name>)` → 코어 link (`SJH::engine`).

---

## 제2장 — 렌더 파이프라인 (DeviceContext + SceneRenderer + IRenderStage)

### 2.1 계층 구조 ✅

```
Application (sb7::application)
  └─ mStages : vector<unique_ptr<IRenderStage>>      ← 명시 순서 순회
       ├─ CameraStage(worldCam)   → SceneRenderer.RenderWithCamera → sceneFB 3D
       ├─ (ParticleStage)         → Effekseer → sceneFB  ⚠ 미배선 (제5장)
       ├─ CameraStage(screenCam)  → sceneFB PassComponent 체인 (PostFX)
       └─ ScreenQuadStage         → backbuffer 합성
  └─ SceneRenderer (IRenderStage) ─ DeviceContext (GL 게이트웨이) ─ MeshPassProcessor
  └─ SceneContext (Camera/Light Aggregate)
```

### 2.2 `SJH::DeviceContext` — GL 호출 단일 게이트웨이 ✅ (verified `src/render/device_context.h`)

GL context 활성 후 첫 `Get()` 에서 lazy init 되는 싱글톤. 모든 GL **pipeline state** 변경의 단일 facade (v1 의 RenderContext 명칭에서 **DeviceContext 로 확정**). ⚠️ **shader uniform 상태는 본 facade 밖** — `glUniform*` 은 `SJH::Uniforms` 자유 함수가 *의도적으로* facade 우회 (OCP + location 캐시 진단).

```cpp
auto& dc = SJH::DeviceContext::Get();
dc.BeginFrame(dc.GetDefaultTarget());      // BindTarget+Clear+DepthTest+Blend alias
dc.UseProgram(*prog);                       // glUseProgram + bound 추적
SJH::Uniforms::SetMat4(*prog, "uModel", m); // bound program 대상 (facade 밖)
dc.BindVAO(vao);
dc.BindTexture(0, tex);
dc.DrawIndexed(indexCount);
```

primitive: `UseProgram` / `BindVAO` / `BindTexture` / `BindTarget` / `Clear` / `SetDepthTest` / `SetBlend` / `DrawIndexed` / `DrawArrays`. 편의: `BeginFrame(target)` + `GetDefaultTarget()`.

> **순서 계약 (POLA)**: `glUniform*` 는 GL 4.1 에서 *currently bound* program 만 대상 → `UseProgram` 후에만 `Uniforms::Set*`. assertion 없이 doxygen 으로만 명시.

### 2.3 `SJH::SceneRenderer` — 씬 traverse → MeshPassProcessor ✅ (verified `src/render/scene_renderer.h`)

```cpp
class SceneRenderer : public IRenderStage {                  // Unreal FSceneRenderer 정통
    [[deprecated("Use CameraStage + IRenderStage stages 컬렉션")]]
    void Render(RenderTarget& defaultTarget) override;        // SceneContext 자동 순회 (구형)
    void RenderWithCamera(Scene::Camera& cam);               // CameraStage 가 위임
    void SetScreenQuadMesh(Mesh*);  void SetBypassMaterial(Material*);  // PassComponent 처리용
    const Framebuffer* GetLastSceneOutput() const;           // ScreenQuadStage 가 조회
private:
    void CollectFromActor(const Scene::Actor&, const vmath::mat4& view, uint64_t cullingMask);
    MeshPassProcessor      mProcessor;
    LightUniformDispatcher mDispatcher;
    const Framebuffer*     mLastSceneOutput = nullptr;
};
```

- `Render(rt)` 는 `[[deprecated]]` — 단일 카메라 자동 순회 (구형). `_MyApp_` 는 stages 컬렉션 + `RenderWithCamera` 경로.
- `SceneContext` 에서 Camera/Light 컬렉션 직접 조회 (매 프레임 DFS 폐기 — SP-SceneContext). `addCamera` 호출 순서 그대로 렌더 (`std::sort` 폐기, `Camera::Depth` 폐기 — [[camera_depth_postfx_misuse]]).
- Light uniform 송신은 `LightUniformDispatcher` 로 분리. Program 집합은 `ResourceRegistry::GetAllPrograms()`.

### 2.4 `IRenderStage` + stages 컬렉션 ✅ (verified `src/render/{render_stage,camera_stage,screen_quad_stage}.h`)

```cpp
class IRenderStage {
    virtual void Render(RenderTarget& target) = 0;
    virtual void OnResize(int w, int h) {}     // 기본 no-op
};
```

구현체: `SceneRenderer`, `CameraStage`(단일 카메라 wrap → `RenderWithCamera` 위임), `ScreenQuadStage`(`SetSources(vector<const Framebuffer*>)` → backbuffer passthrough blit), `ParticleStage`(Effekseer → sceneFB, ⚠ 미배선). Application: `for (auto& s : mStages) s->Render(*mDefaultTarget);` 단일 패턴.

### 2.5 2-Camera + PassComponent PostFX ✅ (verified `src/render/pass_component.h`, 메모리 [[pass_component_postfx_pattern]])

v1 에 없던 정착 구조. **PostFX 는 `PostFXPass` struct 가 아니라 `PassComponent`(Component)** — "화면 공간 MeshRenderer".

```cpp
class PassComponent : public Component {                       // Camera 자식 Actor 에 부착
    PassComponent(Framebuffer* inputFB, Framebuffer* outputFB, Material* mat);
    Framebuffer* const InputFB;    // readonly — 이전 패스의 OutputFB 와 포인터 공유
    Framebuffer* const OutputFB;   // writable — 다음 패스의 InputFB 와 포인터 공유
    bool Enabled = true;  int QueueOffset = 9000;             // 월드 지오메트리(0~8999) 이후
};
```

- **World Camera** (Perspective) → 3D 씬을 `sceneFB` 에 렌더.
- **Screen Camera** (Orthographic, `IsOrthographic=true`/`NoClear=true`) → `sceneFB` 입력으로 PassComponent 체인 (blur/gamma/invert/sharpening/sobel). `InputFB`/`OutputFB` `*const` 공유로 파이프라인 경계 정의 (`Pass1.OutputFB == Pass2.InputFB`).
- `MeshPassProcessor` DrawCommand 큐(`Kind { WorldMesh, ScreenQuad }`)가 `QueueOffset` 으로 순서 결정 + `activeFB` 동적 전환.
- **폐기**: `PostFXPass` / `PostFXStage` enum / `SceneRenderer::SetPostFXChain`·`RunPostFXChain`.
- ⚠️ 디버깅 기록: `doc/design/GammaStucked.md` — 2-Camera `mSceneFB` 공유 + `*const` 제약에서 비롯된 2종 버그(MeshRenderer 소실 / gamma off 시 화면 동결).

### 2.6 Light + Material + Uniform ✅

- 셰이더 라이팅 schema: `Const::MAX_POINT_LIGHTS = 16` + `Const::MAX_SPOT_LIGHTS = 16` 대칭 ([[shader_max_lights_16]]). `pointLights[]`/`spotLights[]` 배열 + `*Enabled[]` 가드. (구 `NUM_*=2` → 이름·값 정정.)
- `Material` = 순수 데이터. `Material::Apply()` 폐기. Program 은 EagerBuild (Observer 패턴 제거). `CreateSharedMaterial`(Unity sharedMaterial) vs `CreateMaterialInstanceFrom`(Unreal MID) 구분.
- `SceneContext` 자동 등록 — Camera/Light Component 의 `OnEnter` 훅이 자동 등록 ([[scenecontext_auto_register]]). `dir.SetActiveCamera`/`AddLight` 명시 호출 금지.

### 2.7 빌보드 + atlas 셰이더 ✅ (GLSL 410)

v1 의 셰이더 패턴은 유효 (단 `#version 410 core`). 카메라 right/up 으로 quad 정렬 + atlas sub-rect uniform.

```glsl
#version 410 core
// vertex: cameraRight = vec3(uView[0][0],uView[1][0],uView[2][0]); cameraUp = vec3(0,1,0);
// worldPos = center + cameraRight*pos.x*size.x + cameraUp*pos.y*size.y;
// vUV = uvRect.xy + aUV * uvRect.zw;
// fragment: vec4 c = texture(uAtlas, vUV); if (c.a < 0.01) discard; fragColor = c * tint;
```

### 2.8 UniformAtlas (`SJH::sprite`) ✅ (verified `src/sprite/uniform_atlas.h`)

균일 정사각 N×M 그리드. **stb_image/glGenTextures 직접 호출 금지** — `SJH::Image::Load` + `SJH::Texture::CreateTexture` 위임 ([[uniform_atlas_delegates_image_texture]], [[stb_image_owner_resource_registry]]). 픽셀아트 NEAREST + CLAMP.

```cpp
auto* atlas = reg.CreateUniformAtlas("player", "resources/TestPattern.png", /*cols*/4, /*rows*/4);
vmath::vec4 uv = atlas->GetUVRect(frameIdx);   // ComputeUVRect 위임 (row-major: col=idx%cols)
int n = atlas->FrameCount();
// (직접 빌더도 가능: atlas.LoadFromPNG("foo.png").SetGrid(4,4); / .SetTileSize(128);)
```

### 2.9 좌표계 정책 ✅

| 시스템 | 좌표계 |
|---|---|
| 물리 (Box2D) | 2D, +X 오른쪽, +Y 위 |
| 렌더 (OpenGL) | 3D 오른손, +Y 위, -Z 앞 |
| 매핑 | `render = vec3(b2.x, height_offset, -b2.y)` (XY → XZ) |

---

## 제3장 — 씬 그래프 (OOP Actor + Component)

> v1 의 "ECS(EnTT)" 장을 대체. **EnTT 미사용** — Cocos2D `cc.Node`+`cc.Component` 정통 + Unity `MeshRenderer`/Unreal `AActor` 영감. [[entt_removed]]

### 3.1 Actor + Component ✅ (verified `src/scene/actor.h`)

```cpp
namespace SJH::Scene {
class Component {                            // Cocos cc.Component 정통 (pure virtual lifecycle)
    virtual void OnEnter() = 0;             // Actor running 진입 (= Awake/BeginPlay)
    virtual void OnExit()  = 0;             // running 이탈 (= OnDestroy/EndPlay)
    virtual void Update(float dt) = 0;      // 매 프레임 (= Update/Tick)
    bool IsEnabled() const; void SetEnabled(bool);  Actor* GetOwner() const;
};
class Actor {                               // Cocos cc.Node 정통 — 트리 + flat 컴포넌트 + 내장 Transform
    Actor* AddChild(std::unique_ptr<Actor>);                 // entered 부모면 즉시 OnEnter
    template<typename T, typename...A> T* AddComponent(A&&...);  // construct+insert+OnEnter contract
    template<typename T> T* GetComponent() const;            // type_index 정확 매치
    template<typename Fn> void ForEachComponent(Fn) const;   // base 다형 질의용 (FindPhysics 등)
    Transform& GetTransform();  vmath::mat4 GetWorldMatrix() const;  // 부모 체인 곱
    void SetActive(bool);  void SetLayer(uint64_t / Scene::Layer);  // CullingMask 비트
    void OnEnter(); void OnExit(); void Update(float dt);    // 재귀 cascade
};
}
```

- **단일 호출 contract**: `AddComponent`/`AddChild` 가 이미 entered 인 부모에 대해 *construct + insert + 즉시 OnEnter*. 같은 타입 중복은 assert.
- `GetComponent<T>` 는 `type_index` 정확 매치 → base 다형 질의는 `ForEachComponent` + `dynamic_cast` (예: `FindPhysics(Actor*)`).
- `OnExit` 는 destroy 안 함 — RAII(`unique_ptr`) destructor 가 cleanup (ddd Explicit Side Effects).

### 3.2 Director + SceneContext ✅ (verified `src/scene/{scene,scene_context}.h`)

```cpp
SJH::Scene::Director::Get().Root().AddChild(std::move(actor));
SJH::Scene::Director::Get().Enter();          // OnEnter cascade
SJH::Scene::Director::Get().Update(dt);       // 전체 Component::Update
SceneContext& ctx = SJH::Scene::Director::Get().GetContext();  // Camera/Light Aggregate
```

- **`Director`** (engine) = root Actor("WorldRoot") + `SceneContext` 보유 싱글톤 (Cocos `cc::Director` 정통). `mActiveCamera` 슬롯 폐기.
- **`SceneContext`** = `AddCamera/RemoveCamera/GetCameras` + `AddLight/RemoveLight` (DirLight 단일 + PointLight≤16 + SpotLight≤16) 비소유 컬렉션. Component `OnEnter`/`OnExit` 가 자동 push/pop.
- ⚠️ **Client 의 `Manager` 와 혼동 금지** — engine `SJH::Scene::Director` ≠ Client `TopdownShooter::Manager`(구 Director, 제9장).

### 3.3 컴포넌트 카탈로그 (실제) ✅

| 컴포넌트 | 모듈 | 책임 |
|---|---|---|
| `MeshRenderer` | render | Mesh+Material+Visible+QueueOffset (Unity 식 통합) |
| `SpriteRenderer : MeshRenderer` | sprite | atlas/frameIdx/tint/flipX (Unity SpriteRenderer is-a MeshRenderer) |
| `Camera` | scene | view/proj + `IsOrthographic`/`OrthoSize`/`NoClear` + `SetTargetRenderTarget` + `SetCullingMask` |
| `DirLight`/`PointLight`/`SpotLight` | object/scene | 조명 — OnEnter 시 SceneContext 자동 등록 |
| `PassComponent` | render | 화면 공간 PostFX (InputFB/OutputFB/Material/QueueOffset) |
| `Components::Physics`/`BoxBody`/`CircleBody` | Client/Physics | Box2D body (제4장) |
| `PlayerBehavior` / `SimplePursueAI` / `*ContactHandler` | Client/Entity | 게임 도메인 (제6장) |
| `StateMachine<T,O>` / `PlayableBase` 파생 | fsm/playable | 시간축 (제6장) |

### 3.4 프레임 흐름 ✅

```cpp
void render(double t) override {                 // sb7::application hook
    float dt = (float)(t - mLastTime); mLastTime = t;
    Manager::Get().Update(dt);                   // Physics.Step + Audio/VFX.Update (Client)
    Director::Get().Update(dt);                  // 모든 Component::Update 재귀
    // ScreenQuadStage sources 갱신(GetLastSceneOutput ?? sceneFB) → stages 순회
    for (auto& s : mStages) s->Render(*mDefaultTarget);
    Manager::Get().VFX().Draw(view, proj);       // ⚠ ParticleStage 미배선 → 여기서 직접
    mImGuiStack.RenderAll(...); ImGui::Render();
}
```

---

## 제4장 — 물리 (Box2D v2.4.1, Client 한정)

> v1 의 Box2D 3.x C API(`b2BodyId`/`b2World_Step`) 는 **실제와 다름**. 실제는 **v2.4.1 C++ API** (`b2World*`/`b2Body*`/`b2ContactListener`), 그리고 코어가 아닌 **Client (`apps/_MyApp_/src/Physics/`)** 거주.

### 4.1 모듈 구성 ✅ (progress doc M3, `doc/Box2DAPI.md`)

| 파일 | 책임 |
|---|---|
| `PhysicsComponent.h` | `Components::Physics` abstract — b2Body 라이프사이클 + `FindPhysics(Actor*)` |
| `PhysicsComponent.Imp.h` | `BoxBody` / `CircleBody` concrete (shape 태그) |
| `physics_system.{h,cpp}` | `PhysicsSystem` — `b2World` owner + `Init/Step(dt)/Shutdown/SyncToTransform` |
| `physics_movement.{h,cpp}` | `PhysicsMovement : IMovable` — `DoForward` → `SetLinearVelocity` |
| `contact_listener.{h,cpp}` | `b2ContactListener` → `IsSensor()` 분기 → `IContactable` 디스패치 |
| `filter.h` | `enum class PhysicsLayer : uint64_t` + `operator\|/&/~` + `ToBits()` Box2D 어댑터 |

### 4.2 충돌 인터페이스 ✅ (Unity isTrigger 정통)

`IContactable` (4 콜백 default empty): `OnTriggerEnter/Exit` (sensor) + `OnCollisionEnter/Exit` (solid). Component 가 선택적 override. v2.4.1 의 `b2ContactListener::BeginContact/EndContact` 에서 `b2Fixture::IsSensor()` 로 분기.

### 4.3 좌표계 동기화

```cpp
void PhysicsSystem::SyncToTransform() {  // b2Body* → Actor Transform
    b2Vec2 p = body->GetPosition();
    actor->GetTransform().Translate = vmath::vec3(p.x, heightOffset, -p.y);  // XY→XZ
}
```

### 4.4 Stage 형성 — Stage Builder ✅ (spec `2026-05-26-stage-builder-refactor`)

벽/픽업 생성은 `Stage::CreateStageActor(StageConfig)` Builder 로 응집 (factory 는 `Stage/Factories/{wall,pickup}_factory.h` — Physics/ 에서 이동). `StageConfig` PoD (`world`/`registry`/`arenaHalfExtent`/`pickupPositions`) 만 넘기면 plane mesh (`RegisterMesh`) + wall/pickup material 까지 자동 생성. main.cpp 는 한 줄.

---

## 제5장 — 파티클 (Effekseer)

> 버전 정정: v1 의 **1.80.2** → 실제 **1.7.3.0**. ✅ `CLAUDE.md`

### 5.1 VFXSystem (Client) ✅ (verified `apps/_MyApp_/src/VFX/VFXSystem.h`)

Effekseer `Manager` + `EffekseerRendererGL::Renderer` owner. `Init(maxSprites=8000)` / `Update(dt)` (내부 `manager_->Update(dt*60.0f)`) / `Draw(const float* view, const float* proj)` / `Shutdown` + `GetManager()`/`GetRenderer()`.

### 5.2 EffekseerPlayable (leaf) 🟡

`PlayableBase` 파생 leaf — ctor `TrackPolicy { Static, FollowOwner }`. `Static` = OnPlay 시 1회 `SetLocation`, `FollowOwner` = OnUpdate 매 프레임 owner Transform 추적. effect 자원은 `ResourceRegistry::CreateEffect(mgr, key, u"...efk")` (utf-16 path).

### 5.3 ParticleStage ⚠️ 미배선 (verified `apps/_MyApp_/src/VFX/ParticleStage.{h,cpp}`, commit `592e99b`)

`IRenderStage` 구현. WorldCamera 의 `GetTargetRenderTarget()`(=sceneFB) 에 bind 후 Effekseer Draw → PostFX 체인이 파티클까지 처리.

```cpp
void ParticleStage::Render(SJH::RenderTarget&) {
    auto* rt = mWorldCam->GetTargetRenderTarget();         // sceneFB (진실의 원천 단일화)
    SJH::DeviceContext::Get().BindTarget(*rt);             // NoClear
    mVFX->Draw(&view[0][0], &proj[0][0]);                  // Effekseer 자체 GL state
}
```

> ⚠️ **현재 상태**: 클래스는 커밋됐으나 `main.cpp` 의 `mStages` 에 **insert 되지 않음**. `main.cpp:222` 에서 `Manager::Get().VFX().Draw()` 가 stages 순회 *밖* (backbuffer) 에서 직접 호출 → **파티클이 PostFX 미적용**. spec `2026-05-27-particle-stage` §4.5 의 (b) insert + (c) 기존 Draw 삭제가 **다음 작업 1순위**.

---

## 제6장 — FSM + Playable (시간축 추상화)

> v1 의 "애니메이션 + 게임 FSM" 장을 대체. 실제는 **`SJH::fsm`** (상태) + **`SJH::playable`** (시간축 합성) 두 코어 모듈.

### 6.1 `SJH::fsm` — StateMachine ✅ (verified `src/fsm/{state_machine,fsm_state}.h`)

self-transitioning State (전이 그래프가 각 State 객체에 응집).

```cpp
template<typename TState, typename TOwner>
class StateMachine : public SJH::Scene::Component {     // Aggregate Root
    StateMachine(TOwner& owner, TState startup = TState::NONE);
    void RegisterState(std::unique_ptr<IFsmState<TOwner>>);   // id 자가 노출 (GetStateFlag)
    bool TryTransit(TState target);                          // 런타임 가드 (실패 false)
    void ForceTransit(TState target);                        // 계약 위반 시 std::abort()
};
template<typename TOwner> class IFsmState {              // Entity within Aggregate
    virtual uint64_t GetStateFlag()   const = 0;        // 내가 누구
    virtual uint64_t GetTransitFlag() const = 0;        // 갈 수 있는 곳들의 OR
    virtual void OnEnter/OnUpdate/OnExit(TOwner&, [float dt]) = 0;
};
// 전이 판정: (current.GetTransitFlag() & targetBit) == targetBit
```

⚠️ **사용처**: Stage/Enemy 용도 보존. **Player 는 FSM 미사용** — arch-correction D1 로 `PlayerBehavior` flat 메서드가 대체. `Entity/State/`(PlayerStateMachine) 디렉토리는 *의도적 부재*.

### 6.2 `SJH::playable` — IPlayable + Composite ✅ (verified `src/playable/*.h`)

Client 우선 4-method (Play/Pause/Stop/GetIsLoop) + IsFinished. Tweeny `*this` + DOTween Composite 정통.

```cpp
class IPlayable {                          // pure interface
    virtual void Play()/Pause()/Stop() = 0;
    virtual bool GetIsLoop() const = 0;  virtual bool IsFinished() const = 0;
};
class PlayableBase : public IPlayable, public SJH::Scene::Component {  // 다중 상속 abstract
    void SetIsLoop(bool);                  // 인터페이스 외 추가
    void Update(float dt) final;           // → OnUpdate hook (paused_/finished_/elapsed_/isLoop_ 보유)
protected: virtual void OnPlay()/OnStop() {}  virtual void OnUpdate(float)=0;  // OnUpdate 만 필수
};
class SequencePlayable : PlayableBase { SequencePlayable& Append/Insert(...); AppendInterval(float); };
class ParallelPlayable : PlayableBase { ParallelPlayable& Join(...); };
class IntervalPlayable : PlayableBase { explicit IntervalPlayable(float duration); }; // N초 대기→finished
```

### 6.3 SpriteSequencePlayable (multi-clip) ✅ (verified `src/sprite/sprite_sequence_playable.h`)

`SpriteAnimator` 폐기 → `SpriteSequencePlayable` 가 상위 호환. `SpriteFrameClip{startFrame, frameCount, fps}` POD.

```cpp
seq->RegisterClip(clipIdx, &clip);                              // 다중 클립
seq->RegisterOnClipEnter(clipIdx, sideEffectPlayable);         // 클립 진입 슬롯 (IPlayable*)
seq->PlayClip(clipIdx);                                        // 전환 + elapsed 리셋
seq->SetIsLoop(true).Play();                                   // 기본 패턴
```

### 6.4 PlayerBehavior + leaf 조립 🟡 (spec `2026-05-26-m4-player-behavior`)

`PlayerBehavior : Component` (flat Idle/Move/Attack/Hit/Dash/Die). Attack 시 `Parallel(Effekseer ∥ Fmod ∥ Sequence(AppendInterval(0.3) → BulletSpawnPlayable))` 체인 Play. Bullet/Enemy 는 `Entity/Bullet/`·`Entity/Enemy/`(spec 의 Monster 아님) factory + `IContactable` ContactHandler.

---

## 제7장 — Tweeny 트위닝

`TweenPlayable<T>` (leaf, Client `apps/_MyApp_/src/Tween/`) — `tweeny::tween<T>` + onStep 콜백.

```cpp
void OnUpdate(float dt) override {
    int32_t dtMs = static_cast<int32_t>(dt * 1000.0f);   // ⚠ [[tweeny_step_overload_trap]]
    T v = tween_.step(dtMs);                             // step(int32 ms) — float 넘기면 폭주
    if (onStep_) onStep_(v);
    if (tween_.progress() >= 1.0f && !isLoop_) finished_ = true;
}
```

사용: 카메라 shake, 데미지 넘버, UI 슬라이드. easing 은 `tweeny::easing::*`. ⚠️ `vmath::radians(120)` 정수 리터럴은 0 됨 — `120.0f` ([[vmath_radians_int_trap]]).

---

## 제8장 — 음향 (FMOD Core + Studio)

> v1 은 Core only. 실제는 **Core + Studio** (`.bank` 이벤트).

### 8.1 AudioSystem (Client) ✅ (verified `apps/_MyApp_/src/Audio/AudioSystem.h`)

`FMOD::System` + `FMOD::Studio::System` owner. `Init` / `Update(dt)` (`studio_->update()`) / `Shutdown` + `GetSystem()`/`GetStudioSystem()` + bank/event 로드 (EventDescription 캐시).

### 8.2 leaf Playable 🟡

- `FmodPlayable` (Core .wav) — `playSound` → `FMOD::Channel*`. Pause override.
- `FmodStudioPlayable` (Studio event) — `createInstance` → `start()`. Stop = `FMOD_STUDIO_STOP_IMMEDIATE`.
- Sound 자원은 `ResourceRegistry::CreateSound(sys, key, path)` 캐시 ([[resource_registry_game_deps]]).

### 8.3 빌드 의무 ✅

FMOD dynamic-only → POST_BUILD 에서 `$<TARGET_FILE:fmod>` + `$<TARGET_FILE:fmodstudio>` 실행 파일 옆 복사. `.bank` 는 라이선스상 재배포 제약 → `.gitignore`. ([[fmod_game_deps_auto_join]], `doc/FMOD_Setup.md`)

---

## 제9장 — 통합 흐름 (Manager + main.cpp)

### 9.1 `TopdownShooter::Manager` — Client 싱글톤 ✅ (verified `apps/_MyApp_/src/Manager.h`)

> ⚠️ M5 spec 의 `TopdownShooter::Director` 가 commit `2af7efb` 에서 **`Manager` 로 rename** 됨 (engine `SJH::Scene::Director` 와 명칭 충돌 회피). **헤더 가드/주석/spec/메모리/EngineAPI 는 아직 `Director` 로 기재 — 용어 불일치 잔존.**

`Audio()` / `VFX()` / `Physics()` subsystem + `SceneRenderer()` 집계 + `Init`/`Update(dt)`/`Shutdown` 일괄. (engine `SceneRenderer` 인스턴스도 Manager 가 값 보유.)

```cpp
auto& mgr = TopdownShooter::Manager::Get();
mgr.Init();                       // Audio.Init + VFX.Init + Physics.Init
mgr.Update(dt);                   // Audio.Update + VFX.Update + Physics.Step
mgr.SceneRenderer();              // CameraStage 가 위임 대상으로 사용
mgr.Shutdown();                   // 역순 release
```

### 9.2 main.cpp 라이프사이클 (≈616 줄, commit `614b366` 에서 분할) ✅

- `startup()` — 자원(ResourceRegistry 위탁) + Stage(`CreateStageActor`) + Player/Camera Actor + leaf Playable 조립 + `mStages` 등록(CameraStage×2 + ScreenQuadStage) + `Manager::Init`.
- `render(double)` — §3.4 흐름.
- `shutdown()` — ImGui → `Director::Exit()` → `mStages.clear()` → `Manager::Shutdown()` 역순.

---

## 부록 A — 디렉토리 구조 (실제)

```
src/                          # 코어 15 모듈 (SJH::<module>) + SJH::engine 우산
  common diagnostics buffer shader program layout material object input
  scene/    actor.{h,cpp} scene.{h,cpp} scene_context.{h,cpp} components.* camera.* layer.h
  sprite/   uniform_atlas.* sprite_component.h sprite_sequence_playable.* sprite_frame_clip.h
  fsm/      state_machine.h fsm_state.h
  playable/ iplayable.h playable_base.* composite_playable.* interval_playable.*
  render/   device_context.* scene_renderer.* render_stage.* camera_stage.* screen_quad_stage.*
            pass_component.h mesh_pass_processor.* render_target.* light_uniform_dispatcher.*
            mesh_renderer.h pipeline_state_setter.* property_block_setter.*
  resource_registry/  resource_registry.* image.* texture.* material.* model.* sound.h effect.h

apps/_MyApp_/                 # 탑다운 슈터 (정본 데모, 활성)
  main.cpp                    # ≈616줄
  src/
    Manager.{h,cpp}           # Client 싱글톤 (구 Director) — Audio/VFX/Physics/SceneRenderer
    Audio/   AudioSystem.* FmodPlayable.* FmodStudioPlayable.*
    VFX/     VFXSystem.* EffekseerPlayable.* ParticleStage.*   ⚠ 미배선
    Tween/   TweenPlayable.h
    Physics/ PhysicsComponent.* physics_system.* contact_listener.* physics_movement.* filter.h
    Entity/
      Player/  PlayerBehavior.* BulletSpawnPlayable.* PlayerActor.h PlayerEntity.h …
      Bullet/  BulletContactHandler.* BulletLifetime.h bullet_factory.h
      Enemy/   SimplePursueAI.* EnemyContactHandler.* enemy_factory.h
      Components/  LifeComponents.h WeaponComponents.h MovementComponents.h Components.Interfaces.h
    Stage/
      StageBuilder.* StageConfig.h WaveController.* Stage.h
      Components/ StageStateComponent.h PickupTriggerLogger.*
      Factories/  wall_factory.h pickup_factory.h
      State/      StageStateMachine.h StageFSMState.h StageState.Impl.h   (stub)
    UI/        PostFXDebugLayer.h …                            # ImGui Game/Editor 분리
```

---

## 부록 B — 함정 모음 (Caveats)

| # | 함정 | 대처 |
|---|---|---|
| B.1 | Tweeny `step(int32 ms)` vs `step(float ratio)` | dtMs 를 `int32_t` 로. float 로 ms 넘기면 양 끝 깜빡임 폭주 ([[tweeny_step_overload_trap]]) |
| B.2 | `vmath::radians(120)` 정수 → 0 | `120.0f` 사용 ([[vmath_radians_int_trap]]) |
| B.3 | stb_image 다중 정의 | `STB_IMAGE_IMPLEMENTATION` 은 `src/resource_registry/image.cpp` 단 한 곳. `SJH::Image::Load` 위임 ([[stb_image_owner_resource_registry]]) |
| B.4 | VAO/EBO 서드파티 오염 | Effekseer/Box2D init 이 bound VAO 의 EBO 덮어씀 → 매 프레임 `ebo->Bind()` 재핀 ([[vao_ebo_thirdparty_corruption]]) |
| B.5 | macOS GL 기본 3.2 | `init()` override 에서 GL 4.1 명시 ([[glsl_410_project_policy]]) |
| B.6 | FMOD dll 누락 | `game_deps` 챕터 POST_BUILD copy 필수 ([[fmod_game_deps_auto_join]]) |
| B.7 | `Camera.Depth` PostFX misuse | addChild 순서가 자동 렌더 순서. Depth 폐기 ([[camera_depth_postfx_misuse]]) |
| B.8 | `GetComponent<T>` base 질의 불가 | `type_index` 정확 매치 → `ForEachComponent`+`dynamic_cast` (`FindPhysics`) |
| B.9 | 2-Camera `mSceneFB` 공유 버그 | `doc/design/GammaStucked.md` — MeshRenderer 소실 / gamma off 동결. PassComponent `*const` 제약 주의 |
| B.10 ⚠️ | **ParticleStage 미배선** | 파티클이 PostFX 건너뜀. spec `2026-05-27-particle-stage` §4.5 배선 필요 |

---

## 부록 C — 미진행/잔여 작업

> `doc/topdown-shooter-progress.md` 와 동기화 (2026-05-31 기준).

| 우선 | 항목 | 위치/근거 |
|---|---|---|
| **0** | **ParticleStage 배선** — `mStages` insert + 기존 `VFX().Draw()` 삭제 | spec `2026-05-27-particle-stage` §4.5 |
| 1 | M4 시각 검증 — WASD/공격/Dash/피격/적 접촉 | progress doc |
| 2 | **`SP-UniversalRenderTarget` spec 사후 작성** — `eb85809` 가 spec 없이 구현됨 | ⚠ doc/superpowers/specs 에 부재 |
| 3 | **`Director`→`Manager` 용어 정정** — spec/MEMORY/EngineAPI/CLAUDE.md + Manager.h 헤더 가드(`_TOPDOWNSHOOTER_DIRECTOR_H__`) 일괄 | commit `2af7efb` |
| 4 | M6 — 사운드 본격 (Attack 체인에 Effekseer/FMOD 연결) | M6 |
| 5 | M7 — Stage FSM 활성화 (`Stage/State/` stub → WaveController 연결) | M7 |
| — | (정합) `Entity/Monster/`→`Enemy/`, `Entity/State/` 의도적 부재 | spec↔코드 표기 정정 |

---

## 부록 D — 참고 자료

- `.claude/CLAUDE.md` — 아키텍처/빌드/컨벤션 정본.
- `.claude/architecture.md` — `SJH::<module>` STATIC 패턴 + 의존 선언 규칙 + §11.3 자원 보유 컨벤션.
- `doc/topdown-shooter-progress.md` — 마일스톤 진행 (M1~M7) + Render Pipeline 정착.
- `doc/EngineAPI.md` — 엔진 코어 API 레퍼런스 (⚠ 12모듈 시점 — sprite/fsm/playable 미반영, 헤더 직접 참조).
- `doc/Box2DAPI.md` / `doc/FMODAPI.md` / `doc/FMOD_Setup.md` — 라이브러리 실사용 레퍼런스.
- `doc/design/` — 세션 메모: `GammaStucked.md`(2-Camera 버그), `PostFX.md`, `2026-05-27-pass-component-2camera-session.md`, `OptimizeRenderTarget.md`, `2026-05-26-{M4,M5}-handoff.md`.
- `doc/superpowers/specs/` — 설계 스펙 20종 (SP1~SP5 / M1~M5 / FSM / Playable / SceneContext / RenderStage / PassComponent / ParticleStage / Stage Builder).
- `doc/superpowers/plans/` — 구현 플랜 23종.

> **본 v2 의 정확한 시그니처는 해당 헤더를 직접 참조**. spec↔코드 드리프트 사례: `RenderContext`→`DeviceContext`, `RenderSystem`→`SceneRenderer`, `RenderQueue`→`MeshPassProcessor`, Client `Director`→`Manager`, `NUM_*_LIGHTS=2`→`MAX_*_LIGHTS=16`, `Entity/Monster`→`Entity/Enemy`.
