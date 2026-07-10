# 엔진 의존 사이클 E1~E6 해소 — 구현 계획

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 엔진 17모듈의 양방향 사이클 6개(E1~E6)를 전부 절단해 단방향 의존 그래프 달성 (mutual 0, 7-노드 SCC 해체).

**Architecture:** 슬라이스 ②(Light→scene + RenderTarget→buffer + D6 setter 이주) → ③(render-팩토리 이주) → ①(신규 `src/texture/` 모듈 + D8 SpriteRenderer DI). 정본 spec: `doc/superpowers/specs/2026-06-11-engine-dependency-cycle-refactor-design.md` (D1~D8).

**Tech Stack:** C++17 / CMake preset `ninja` / STATIC 모듈 + `SJH::<module>` ALIAS / `SJH::engine` INTERFACE 우산.

---

## ⚠ 프로젝트 오버라이드 (스킬 기본값 대체 — 반드시 준수)

1. **TDD 없음** — 리팩토링(이동) 작업. 검증 = 슬라이스별 `cmake --build --preset ninja --target _MyApp_` **에러 0**. 신규 테스트 작성 금지 (no_auto_tests).
2. **커밋 금지** — 에이전트는 절대 커밋하지 않는다. 각 슬라이스 끝의 "사용자 커밋 게이트" 스텝에서 *권장 path-scoped 커밋 명령*만 보고. `git add`/`git mv` 도 금지 (사용자 병렬 staging 보호) — 파일 이동은 **plain `mv`** 사용 (rename 검출은 커밋 시 git 이 자동).
3. **주석 한국어 + Doxygen + ASCII/한글만** (특수문자 0). 헤더가드 `__SJH_*_H__` (`#pragma once` 금지). **Tab indent** (.clang-format Microsoft, ColumnLimit=0).
4. **각 슬라이스 시작 전 `git status` 확인** — 본 plan 의 대상 파일이 사용자 편집 중(M/staged)이면 멈추고 보고. **예외(선행 전제로 화이트리스트):** `src/common/constants.h` + `src/program/program_uniforms.cpp` 의 `M` 은 사용자의 `SFX_* → SHADER_PROPERTIE_*` 리네임으로 **기대된 상태** — 중단 사유 아님. 단 이 두 파일의 M 이 *사라져 있으면*(리셋됨) Task 5.2 의 `Const::SHADER_PROPERTIE_*` 가 미정의가 되므로 그때는 중단 후 보고.
5. 이 plan 의 라인 번호는 2026-06-11 HEAD `bf94351` **+ 미커밋 `SFX_*→SHADER_PROPERTIE_*` 리네임이 적용된 working tree** 기준 — drift 시 주변 문맥으로 위치 재확인. (Task 5.2 의 상수명은 working tree 의 `SHADER_PROPERTIE_*` 가 정본.)

## 파일 구조 맵 (최종 상태)

```
src/texture/                       ← 신설 (18번째 모듈, 슬라이스 ①)
  CMakeLists.txt  texture.{h,cpp}  image.{h,cpp}        (resource_registry 에서 mv)
src/scene/
  light.{h,cpp}                    ← 신설 (DirLight/PointLight/SpotLight, 슬라이스 ②)
  compound_actor.{h,cpp}           ← 축소 (Camera/Light 팩토리만 잔존)
  (model_spawner.{h,cpp} 삭제 — render 로 mv)
src/object/
  light.h                          ← 축소 (Light POD + GetAttenuationCoeff 만)
  (light.cpp 삭제 — scene 으로 mv)
src/buffer/
  render_target.{h,cpp}            ← render 에서 mv (슬라이스 ②)
src/render/
  actor_factory.{h,cpp}            ← 신설 (CreateSkyboxActor/CreateScreenCameraActor, 슬라이스 ③)
  model_spawner.{h,cpp}            ← scene 에서 mv
  (render_target.{h,cpp} 삭제 — buffer 로 mv)
src/program/
  program_uniforms.{h,cpp}         ← Set*Light 3종 제거 (D6, → light_uniform_dispatcher.cpp 로컬)
src/resource_registry/
  sprite_resources.{h,cpp}         ← 신설 (D8 — sprite lazy ensure 이주)
  (texture/image 4파일 삭제 — texture 로 mv)
src/sprite/
  sprite_component.{h,cpp}         ← DI 생성자 전환 (D8)
```

---

# 슬라이스 ② — E1 (Light→scene) + E4 (RenderTarget→buffer)

### Task 0: 사전 충돌 확인

- [ ] **Step 0.1: git status 확인**

Run: `git -C /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics status --porcelain`
Expected: 본 슬라이스 대상(`src/object/light.*`, `src/scene/*`, `src/render/*`, `src/buffer/*`, `apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp`, `apps/_MyApp_/main.cpp`, `apps/_MyApp_/src/VFX/ParticleStage.cpp`, `test/test_light.cpp`)에 `M`/staged 없음 — 있으면 **중단 후 사용자 보고**. **예외:** `src/common/constants.h` + `src/program/program_uniforms.cpp` 의 `M` 은 사용자 리네임 선행 전제라 **정상** (오버라이드 #4). 이 두 파일이 clean 이면 working tree 에서 `grep -c SHADER_PROPERTIE_ src/common/constants.h` 가 0 인지 확인 — 0 이면 리네임이 사라진 것이므로 중단 후 보고.

### Task 1: `src/scene/light.h` 신설 (컴포넌트 3종 이주)

**Files:**
- Create: `src/scene/light.h`

- [ ] **Step 1.1: 파일 생성 — 아래 내용 그대로** (클래스 본문 3종은 `src/object/light.h:50-159` 의 `DirLight`/`PointLight`/`SpotLight` 를 **그대로 복사** — Doxygen 포함 무수정. Tab indent 유지)

```cpp
/**
 * @file light.h
 * @brief DirLight / PointLight / SpotLight - Scene::Component 파생 광원 컴포넌트 3종.
 *
 * @details
 *  ### 책임
 *  - 광원 컴포넌트 3종 보유 - Owner Actor 의 Transform 이 위치/방향 제공.
 *  - OnEnter / OnExit 에서 SceneContext 자동 등록 (Cocos cc::Light 정통).
 *
 *  ### 비-책임
 *  - [X] 광원 데이터 POD / 감쇠 계수 - object/light.h 의 Light + GetAttenuationCoeff 잔존.
 *  - [X] 셰이더 uniform 전송 - render 의 LightUniformDispatcher 가 담당.
 *
 * @note 의존 사이클 해소 (2026-06-11, spec D3) - 과거 object/light.h 거주. Scene::Component
 *       상속 + Director(SceneContext) 호출로 몸이 scene 쪽이라 scene 으로 이주 (Camera 와 동일
 *       거주 패턴). 네임스페이스는 SJH 유지 (D7 - 호출처는 include 경로만 변경).
 */
#ifndef __SJH_SCENE_LIGHT_H__
#define __SJH_SCENE_LIGHT_H__
#include "scene/actor.h" // Component base + Actor::GetWorldMatrix (모두 inline -> link 의존 0)
#include <vmath.h>

namespace SJH
{
	// <<< 여기에 src/object/light.h 의 50~159행 (DirLight / PointLight / SpotLight
	//     세 클래스 전체, 각 클래스 위 Doxygen 블록 포함) 를 그대로 붙여넣는다. >>>
}; // namespace SJH
#endif // __SJH_SCENE_LIGHT_H__
```

검증: 붙여넣기 후 파일에 `class DirLight : public Scene::Component` / `class PointLight` / `class SpotLight` 3개가 존재하고, `class Light` 와 `GetAttenuationCoeff` 는 **없어야** 한다.

### Task 2: `src/scene/light.cpp` 신설 + scene CMake 소스 추가

**Files:**
- Create: `src/scene/light.cpp` (`src/scene/light.cpp` 전체 이주)
- Modify: `src/scene/CMakeLists.txt:1-8`

- [ ] **Step 2.1: 파일 이동**

```bash
mv src/scene/light.cpp src/scene/light.cpp
```

- [ ] **Step 2.2: include 경로 수정** — `src/scene/light.cpp:18` 의

```cpp
#include "object/light.h"
```
→
```cpp
#include "scene/light.h"
```

파일 헤더 Doxygen(10~13행)의 "`SJH::object -> SJH::scene` 의존은 이 파일 한정" 문구를 다음으로 교체:
```cpp
 *  - `light.h` 는 데이터 + `Scene::Component` 상속만 보유 - `Actor::GetWorldMatrix()` 정의 호출은
 *    link 의존을 유발하므로 cpp 로 격리. 의존 사이클 해소 (2026-06-11) 로 scene 모듈 거주.
```

- [ ] **Step 2.3: scene CMake 소스 추가** — `src/scene/CMakeLists.txt` 의 `add_library(sjhopengl_scene STATIC` 블록에 한 줄 추가:

```cmake
add_library(sjhopengl_scene STATIC
    actor.cpp
    scene.cpp
    scene_context.cpp      # SP-SceneContext+ProgramRegistry — Cocos2D-x `Scene::_cameras/_lights` 정통 Aggregate
    model_spawner.cpp
    camera.cpp
    compound_actor.cpp
    light.cpp              # 사이클 해소 (2026-06-11) — DirLight/PointLight/SpotLight 컴포넌트 (object 에서 이주)
)
```

### Task 3: `src/object/light.h` 축소 + object CMake 절단

**Files:**
- Modify: `src/object/light.h`
- Modify: `src/object/CMakeLists.txt`

- [ ] **Step 3.1: light.h 에서 컴포넌트 3종 제거** — `src/object/light.h` 에서:
  - 25행 `#include "scene/actor.h" ...` 삭제 (**E1 절단점**)
  - 50~159행 `DirLight`/`PointLight`/`SpotLight` 클래스 3종(각 Doxygen 블록 포함) 삭제
  - 잔존: 파일 헤더 Doxygen + `#include <vmath.h>` + `class Light` (31~48행) + `GetAttenuationCoeff` (Doxygen 161~166행 + 정의 167~181행)
  - 파일 헤더 Doxygen 의 17행 비-책임 항목을 다음으로 교체:
```cpp
 *  - [X] 광원 *컴포넌트* (DirLight/PointLight/SpotLight) - 사이클 해소 (2026-06-11) 로 scene/light.h 거주.
```

- [ ] **Step 3.2: object CMake 에서 scene link + light.cpp 제거** — `src/object/CMakeLists.txt`:

소스 목록에서 `light.cpp` 줄 삭제:
```cmake
add_library(sjhopengl_object STATIC
    mesh.cpp
    model.cpp
    geometry.cpp
)
```

link 블록에서 `SJH::scene` 줄과 그 위 SP5 주석 3줄을 삭제:
```cmake
target_link_libraries(sjhopengl_object
    PUBLIC  SJH::common      # mesh.h: CLASS_PTR(Mesh) 매크로
            SJH::buffer      # mesh.h: Buffer / BufferPtr 필드
            SJH::layout      # mesh.h: VertexLayout / VertexLayoutUPtr 필드
            SJH::material    # model.h: RenderUnit.material / mMaterials / Material
            project_deps     # mesh.h: <<glad>/glad.h>, GLuint
            assimp
)
```

### Task 4: D6 — program_uniforms 에서 Set\*Light 3종 제거

**Files:**
- Modify: `src/program/program_uniforms.h`
- Modify: `src/program/program_uniforms.cpp`

- [ ] **Step 4.1: 헤더에서 선언 제거** — `src/program/program_uniforms.h`:
  - 8행 책임 줄 `*  - 광원 struct -> uniform block 일괄 전송 헬퍼 3종: ...` 을 다음으로 교체:
```cpp
 *  - (광원 헬퍼 3종은 사이클 해소 2026-06-11 로 <render>/light_uniform_dispatcher.cpp 파일-로컬로 이주.)
```
  - 53행 사용 예 `*    Uniforms::SetDirLight(*prog, "dirLight", light, worldDir);` 삭제
  - 68~70행 forward decl 3줄 삭제:
```cpp
    class DirLight;   // forward - 광원 struct 정의는 object/light.h. .cpp 만 include.
    class PointLight;
    class SpotLight;
```
  - **114~138행** (114행 `// --- 광원 struct -> uniform block 일괄 전송 helpers ---` 주석 3줄 + `SetDirLight`/`SetPointLight`/`SetSpotLight` 선언 3종 + 각 Doxygen) 삭제

- [ ] **Step 4.2: cpp 에서 구현 제거** — `src/program/program_uniforms.cpp`:
  - 8행 책임 줄을 헤더와 동일하게 교체
  - 30행 `#include "object/light.h"` 삭제, 31행 `#include "common/constants.h"` 삭제 (헬퍼 제거 후 `Const::` 유일 사용처 소멸 — strict UnusedIncludes), 34행 `#include <cmath>` 삭제
  - 119~157행 (`// === 광원 struct -> uniform block 일괄 전송 helpers ===` 주석부터 `SetSpotLight` 구현 끝까지) 삭제
  - 주의: 이 파일은 사용자 리네임으로 이미 `M` — 삭제 대상 라인의 상수명이 `SHADER_PROPERTIE_*` 로 보여도 정상 (어차피 삭제됨)

### Task 5: light_uniform_dispatcher.cpp 에 파일-로컬 헬퍼 이주

**Files:**
- Modify: `<src>/render/light_uniform_dispatcher.cpp`

- [ ] **Step 5.1: include 갱신** — 22행 `#include "object/light.h"` 를 다음 2줄로 교체 + `<cmath>` 추가:

```cpp
#include "object/light.h"   // GetAttenuationCoeff (광원 데이터/감쇠 자유 함수는 object 잔존)
#include "scene/light.h"    // DirLight/PointLight/SpotLight 컴포넌트 (사이클 해소 - scene 거주)
```
그리고 27행 `#include <string>` 위에:
```cpp
#include <cmath>            // cosf - SpotLight degree->cosine 변환
```

- [ ] **Step 5.2: 익명 네임스페이스 헬퍼 추가** — `namespace SJH` 여는 중괄호(30행) 바로 다음에 삽입. 함수 본문은 `src/program/program_uniforms.cpp:123-157` 의 3개 함수를 **그대로** 이주 (Tab indent 로 재정렬):

```cpp
	namespace
	{
		// === 광원 struct -> uniform block 일괄 전송 helpers ==========================
		// 사이클 해소 D6 (2026-06-11) - 과거 Uniforms::SetDirLight 등 program 모듈 거주.
		// 유일 호출처가 본 TU 라 파일-로컬로 이주 - program 의 광원 타입 의존 제거.
		// 셰이더 struct 멤버 이름과의 *문자열 결합* 만 담당, 실제 GL 호출은
		// Uniforms::SetVec3 / SetFloat 재사용 - 캐시/진단/타입체크 경로 그대로 통과.

		void SetDirLight(const Program &prog, const char *prefix,
		                 const DirLight &light, const vmath::vec3 &worldDir)
		{
			const std::string base = prefix;
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_DIRECTION).c_str(), worldDir);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_AMBIENT).c_str(),   light.Ambient);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_DIFFUSE).c_str(),   light.Diffuse);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_SPECULAR).c_str(),  light.Specular);
		}

		void SetPointLight(const Program &prog, const char *prefix,
		                   const PointLight &light, const vmath::vec3 &worldPos)
		{
			const std::string base = prefix;
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_POSITION).c_str(),    worldPos);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_ATTENUATION).c_str(), GetAttenuationCoeff(light.Distance));
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_AMBIENT).c_str(),     light.Ambient);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_DIFFUSE).c_str(),     light.Diffuse);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_SPECULAR).c_str(),    light.Specular);
		}

		void SetSpotLight(const Program &prog, const char *prefix,
		                  const SpotLight &light, const vmath::vec3 &worldPos, const vmath::vec3 &worldDir)
		{
			const std::string base = prefix;
			Uniforms::SetVec3 (prog, (base + Const::SHADER_PROPERTIE_POSITION).c_str(),     worldPos);
			Uniforms::SetVec3 (prog, (base + Const::SHADER_PROPERTIE_DIRECTION).c_str(),    worldDir);
			// CPU 는 degree, 셰이더는 cosine - 송신 시점에 변환 (struct 정의 시 의도된 분업).
			Uniforms::SetFloat(prog, (base + Const::SHADER_PROPERTIE_CUTOFF).c_str(),       cosf(vmath::radians(light.CutoffAngleDeg)));
			Uniforms::SetFloat(prog, (base + Const::SHADER_PROPERTIE_OUTER_CUTOFF).c_str(), cosf(vmath::radians(light.OuterCutoffAngleDeg)));
			Uniforms::SetVec3 (prog, (base + Const::SHADER_PROPERTIE_ATTENUATION).c_str(),  GetAttenuationCoeff(light.Distance));
			Uniforms::SetVec3 (prog, (base + Const::SHADER_PROPERTIE_AMBIENT).c_str(),      light.Ambient);
			Uniforms::SetVec3 (prog, (base + Const::SHADER_PROPERTIE_DIFFUSE).c_str(),      light.Diffuse);
			Uniforms::SetVec3 (prog, (base + Const::SHADER_PROPERTIE_SPECULAR).c_str(),     light.Specular);
		}
	} // namespace
```

- [ ] **Step 5.3: 호출부 3곳 갱신** — `Uniforms::SetDirLight(` → `SetDirLight(` (63행), `Uniforms::SetPointLight(` → `SetPointLight(` (78행), `Uniforms::SetSpotLight(` → `SetSpotLight(` (94행). 파일 헤더 Doxygen 18행 `@c GetAttenuationCoeff 자유 함수(@c object/light.h) 담당` 문구는 그대로 유효.

### Task 6: E1 include 갱신 (나머지 소비처 5곳)

**Files:**
- Modify: `src/scene/scene_context.cpp:20`, `src/scene/compound_actor.cpp:21`, `<src>/render/scene_renderer.cpp:27`, `apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp:28`, `test/test_light.cpp:21`, `test/CMakeLists.txt` (test_light 블록)

- [ ] **Step 6.1: 치환 4곳** — `scene_context.cpp:20` / `compound_actor.cpp:21` / `scene_renderer.cpp:27` / `WorldSceneBuilder.cpp:28` 에서 `#include "object/light.h"` → `#include "scene/light.h"` (기존 꼬리 주석 보존). `src/scene/scene_context.h` 는 forward decl 만이라 **무수정**. **`test/test_light.cpp` 는 치환 대상 아님 — Step 6.2 의 별도 케이스.**

- [ ] **Step 6.2: test_light — 2-include 공존 (치환 아님)** — test_light.cpp 는 `DirLight/PointLight/SpotLight`(77행 등 — scene 이주분)와 `GetAttenuationCoeff`(128/140/141/153행 — object 잔존분)를 **동시에** 사용 (검증 완료). 단순 치환하면 컴파일 실패. `test/test_light.cpp:21` 을 다음 2줄로 교체:

```cpp
#include "object/light.h" // GetAttenuationCoeff (감쇠 자유 함수 - object 잔존)
#include "scene/light.h"  // DirLight/PointLight/SpotLight (사이클 해소 2026-06-11 - scene 이주)
```

`test/CMakeLists.txt` 의 test_light 블록은 **두 link 모두**:

```cmake
add_executable(test_light test_light.cpp)
target_link_libraries(test_light PRIVATE
    Catch2::Catch2WithMain
    SJH::scene      # "scene/light.h" DirLight/PointLight/SpotLight (사이클 해소 2026-06-11 - object 에서 이주)
    SJH::object     # "object/light.h" GetAttenuationCoeff + vmath
)
```

### Task 7: E4 — render_target → buffer 이동

**Files:**
- Move: `src/buffer/render_target.h` → `src/buffer/render_target.h`
- Move: `src/buffer/render_target.cpp` → `src/buffer/render_target.cpp`
- Modify: `src/buffer/CMakeLists.txt`, `src/render/CMakeLists.txt`

- [ ] **Step 7.1: 파일 이동**

```bash
mv src/buffer/render_target.h src/buffer/render_target.h
mv src/buffer/render_target.cpp src/buffer/render_target.cpp
```

- [ ] **Step 7.2: cpp 자기 include 수정** — `src/buffer/render_target.cpp:13`:

```cpp
#include "buffer/render_target.h"
```
헤더 가드 `__SJH_RENDER_TARGET_H__` 는 유지 (클래스명 기반 — 모듈명 아님).

- [ ] **Step 7.3: buffer CMake** — `src/buffer/CMakeLists.txt` 전체를 다음으로 교체:

```cmake
add_library(sjhopengl_buffer STATIC
    buffer.cpp
    framebuffer.cpp
    render_target.cpp           # vtable home TU — virtual class 의 ODR 보장 (사이클 해소 2026-06-11 — render 에서 이주)
)
add_library(SJH::buffer ALIAS sjhopengl_buffer)

target_include_directories(sjhopengl_buffer
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# 사이클 해소 (2026-06-11) — RenderTarget 인터페이스가 buffer 로 이주, SJH::render 역의존 제거.
# framebuffer.h 가 Texture 헤더 노출 -> SJH::resource_registry PUBLIC (슬라이스 ① 에서 SJH::texture 로 교체 예정).
target_link_libraries(sjhopengl_buffer
    PUBLIC  SJH::common
            SJH::program
            SJH::resource_registry    # framebuffer.h : TexturePtr / Texture
            project_deps
    PRIVATE SJH::diagnostics
)

target_compile_features(sjhopengl_buffer PUBLIC cxx_std_17)
```

- [ ] **Step 7.4: render CMake** — `src/render/CMakeLists.txt`:
  - 소스 목록에서 `render_target.cpp           # vtable home TU ...` 줄 삭제
  - `SJH::buffer` 를 PRIVATE 에서 **PUBLIC 으로 승격** (device_context.h 가 공개 헤더에서 `buffer/render_target.h` include):

```cmake
target_link_libraries(sjhopengl_render
    PUBLIC  SJH::common SJH::program project_deps
            SJH::buffer   # device_context.h : RenderTarget (사이클 해소 2026-06-11 - buffer 거주) + framebuffer.h
    PRIVATE SJH::diagnostics SJH::material SJH::object SJH::resource_registry SJH::scene
)
```

### Task 8: E4 include 갱신 (소비처 6곳)

**Files:**
- Modify: `<src>/buffer/framebuffer.h:26`, `src/render/device_context.h:38`, `<src>/render/screen_quad_stage.cpp:27`, `<src>/render/scene_renderer.cpp:31`, `apps/_MyApp_/main.cpp:51`, `apps/_MyApp_/src/VFX/ParticleStage.cpp:34`

- [ ] **Step 8.0: 소비처 전수 확인** — Run: `grep -rln '"src/buffer/render_target.h"' src/ apps/ test/` — 결과가 위 Files 의 6개 파일과 정확히 일치해야 함. 추가 파일 발견 시 그 파일도 Step 8.1 에 포함.

- [ ] **Step 8.1: 6개 파일에서 `#include "src/buffer/render_target.h"` → `#include "buffer/render_target.h"`** (기존 꼬리 주석 보존). 추가로 클라이언트 주석 갱신: `apps/_MyApp_/src/Entity/Player/PlayerHand.cpp:28` 와 `apps/_MyApp_/src/Playable/SpriteLayerFactory.cpp:13` 의 *주석 속* 경로 문구(`framebuffer.h->render_target.h`)는 기능 무관 — 무수정 허용.

### Task 9: 슬라이스 ② 빌드 검증 + 보고

- [ ] **Step 9.1: configure + build**

Run: `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -20`
Expected: `error` 0건, 링킹 성공. 실패 시 에러 메시지 기준으로 위 Task 들의 누락 include/link 수정 (새 파일 추가는 configure 재실행 필요).

- [ ] **Step 9.2: 절단 검증 grep**

Run: `grep -rn '"object/light.h"' src/ apps/ test/ ; grep -rn '"src/buffer/render_target.h"' src/ apps/ test/`
Expected: 전자는 **정확히 2곳** — `<src>/render/light_uniform_dispatcher.cpp` + `test/test_light.cpp` (둘 다 GetAttenuationCoeff 용, 의도된 잔존). 후자는 0건.

- [ ] **Step 9.3: 사용자 커밋 게이트 — 보고만** (에이전트 커밋 금지). 사용자 권장 명령:

```bash
git add src/object/light.h src/object/CMakeLists.txt src/scene/light.h src/scene/light.cpp \
        src/scene/CMakeLists.txt src/scene/scene_context.cpp src/scene/compound_actor.cpp \
        src/program/program_uniforms.h src/program/program_uniforms.cpp \
        <src>/render/light_uniform_dispatcher.cpp <src>/render/scene_renderer.cpp \
        src/buffer/render_target.h src/buffer/render_target.cpp src/render/CMakeLists.txt \
        src/render/device_context.h <src>/render/screen_quad_stage.cpp \
        src/buffer/render_target.h src/buffer/render_target.cpp src/buffer/CMakeLists.txt <src>/buffer/framebuffer.h \
        apps/_MyApp_/main.cpp apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp apps/_MyApp_/src/VFX/ParticleStage.cpp \
        test/test_light.cpp test/CMakeLists.txt
git commit -m "[refactor] : E1+E4 사이클 절단 - Light 컴포넌트 scene 이주 + RenderTarget buffer 이주 (spec D3/D2/D6)"
```

---

# 슬라이스 ③ — E2 (scene 의 render-팩토리 → render)

### Task 10: `apps/_MyApp_/src/Bootstrap/actor_factory.h` 신설

**Files:**
- Create: `apps/_MyApp_/src/Bootstrap/actor_factory.h`

- [ ] **Step 10.1: 파일 생성** — 선언 2종은 `src/scene/compound_actor.h:67-92` 의 `CreateScreenCameraActor`/`CreateSkyboxActor` (Doxygen 포함) 를 그대로 이주:

```cpp
/**
 * @file actor_factory.h
 * @brief render-결합 PreBuilt Actor 팩토리 - ScreenCamera / Skybox.
 *
 * @details
 *  ### 책임
 *  - MeshRenderer / Framebuffer 등 render 자원을 조립하는 Compound Actor free factory.
 *
 *  ### 비-책임
 *  - [X] Actor 생명주기 - 반환된 unique_ptr 소유권은 호출자 (Director::Root().AddChild).
 *  - [X] render 무관 팩토리 (Camera / Light) - scene/compound_actor.h 잔존.
 *
 * @note 의존 사이클 해소 (2026-06-11, spec D4) - scene/compound_actor.h 에서 이주해
 *       scene -> render 역의존 제거. 네임스페이스는 SJH::Scene 유지
 *       (D7 - MeshRenderer 가 render 파일 + Scene 네임스페이스인 기존 선례).
 */
#ifndef __SJH_RENDER_ACTOR_FACTORY_H__
#define __SJH_RENDER_ACTOR_FACTORY_H__

#include "scene/actor.h"
#include <memory>
#include <string>

namespace SJH
{
	class Framebuffer;
	class Mesh;
	class Material;
}

namespace SJH::Scene
{
	// <<< 여기에 src/scene/compound_actor.h 의 CreateScreenCameraActor (67~78행) 와
	//     CreateSkyboxActor (80~92행) 선언 + Doxygen 을 그대로 붙여넣는다 (Tab indent). >>>
} // namespace SJH::Scene

#endif // __SJH_RENDER_ACTOR_FACTORY_H__
```

### Task 11: `apps/_MyApp_/src/Bootstrap/actor_factory.cpp` 신설

**Files:**
- Create: `apps/_MyApp_/src/Bootstrap/actor_factory.cpp`

- [ ] **Step 11.1: 파일 생성** — 함수 본문 2종은 `src/scene/compound_actor.cpp:92-120` 을 그대로 이주:

```cpp
/**
 * @file actor_factory.cpp
 * @brief render-결합 Compound Actor 팩토리 구현 - ScreenCamera / Skybox.
 *
 * @details
 *  ### 책임
 *  - @c CreateScreenCameraActor - Ortho + NoClear + CullingMask(UI|Screen) + TargetRenderTarget.
 *  - @c CreateSkyboxActor - 큰 scale + MeshRenderer 부착.
 *
 *  ### 비-책임
 *  - [X] 씬 트리 편입 - 반환된 @c unique_ptr 의 @c AddChild 는 호출자 책임.
 *
 * @note 의존 사이클 해소 (2026-06-11, spec D4) - scene/compound_actor.cpp 에서 이주.
 */
#include "apps/_MyApp_/src/Bootstrap/actor_factory.h"
#include "scene/compound_actor.h"  // CreateCameraActor (ScreenCamera 가 내부 재사용)
#include "scene/camera.h"
#include "scene/layer.h"
#include "object/transform.h"
#include "render/mesh_renderer.h"
#include "object/mesh.h"           // SJH::Mesh 완전 타입
#include "material/material.h"     // SJH::Material 완전 타입
#include "<buffer>/framebuffer.h"    // SJH::Framebuffer 완전 타입 (SetTargetRenderTarget 인자)
#include <memory>
#include <string>
#include <utility>
#include <vmath.h>

namespace SJH::Scene
{
	// <<< 여기에 src/scene/compound_actor.cpp 의 CreateScreenCameraActor (92~107행) 와
	//     CreateSkyboxActor (109~120행) 본문을 그대로 붙여넣는다. >>>
} // namespace SJH::Scene
```

### Task 12: compound_actor 축소

**Files:**
- Modify: `src/scene/compound_actor.h`
- Modify: `src/scene/compound_actor.cpp`

- [ ] **Step 12.1: 헤더 축소** — `src/scene/compound_actor.h`:
  - 27~32행 `namespace SJH { class Framebuffer; class Mesh; class Material; }` 블록 삭제
  - 67~92행 `CreateScreenCameraActor` + `CreateSkyboxActor` 선언 (Doxygen 포함) 삭제
  - 파일/네임스페이스 Doxygen 의 책임 줄에서 `ScreenCamera / Skybox Actor` 언급을 제거하고 다음 한 줄 추가:
```cpp
 *  - render-결합 팩토리 (ScreenCamera / Skybox) 는 사이클 해소 (2026-06-11) 로 apps/_MyApp_/src/Bootstrap/actor_factory.h 거주.
```

- [ ] **Step 12.2: cpp 축소** — `src/scene/compound_actor.cpp`:
  - 92~120행 두 함수 본문 삭제
  - include 4줄 삭제: `render/mesh_renderer.h`(23행) / `object/mesh.h`(24행) / `material/material.h`(25행) / `<buffer>/framebuffer.h`(26행)
  - 파일 헤더 Doxygen 의 책임 줄에서 `Skybox / ScreenCamera` 언급 제거

### Task 13: model_spawner → render 이동

**Files:**
- Move: `apps/_MyApp_/src/Bootstrap/model_spawner.h` → `apps/_MyApp_/src/Bootstrap/model_spawner.h`
- Move: `apps/_MyApp_/src/Bootstrap/model_spawner.cpp` → `apps/_MyApp_/src/Bootstrap/model_spawner.cpp`

- [ ] **Step 13.1: 파일 이동**

```bash
mv apps/_MyApp_/src/Bootstrap/model_spawner.h apps/_MyApp_/src/Bootstrap/model_spawner.h
mv apps/_MyApp_/src/Bootstrap/model_spawner.cpp apps/_MyApp_/src/Bootstrap/model_spawner.cpp
```

- [ ] **Step 13.2: 헤더 가드 + cpp include 갱신**
  - `apps/_MyApp_/src/Bootstrap/model_spawner.h:17-18,40` 가드: `__SJH_SCENE_MODEL_SPAWNER_H__` → `__SJH_RENDER_MODEL_SPAWNER_H__` (3곳: ifndef/define/endif 주석)
  - `apps/_MyApp_/src/Bootstrap/model_spawner.cpp:13` `#include "apps/_MyApp_/src/Bootstrap/model_spawner.h"` → `#include "apps/_MyApp_/src/Bootstrap/model_spawner.h"`
  - 양 파일 `@note` 에 한 줄 추가: `사이클 해소 (2026-06-11, spec D4) - scene 에서 render 로 이주 (MeshRenderer 조립 책임). 네임스페이스 SJH::Scene::ModelSpawner 유지.`

### Task 14: CMake + 호출처 갱신

**Files:**
- Modify: `src/scene/CMakeLists.txt`, `src/render/CMakeLists.txt`
- Modify: `apps/_MyApp_/main.cpp:58`, `apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp:35` 부근, `<apps>/_MyApp_/src/Stage/StageBuilder.cpp:38`

- [ ] **Step 14.1: scene CMake 절단** — `src/scene/CMakeLists.txt`:
  - 소스 목록에서 `model_spawner.cpp` 줄 삭제
  - PRIVATE 블록에서 `SJH::render   # compound_actor.cpp: ...` 줄 삭제 (**E2 절단점**):
```cmake
target_link_libraries(sjhopengl_scene
    PUBLIC  SJH::common SJH::object project_deps
    PRIVATE spdlog        # scene_context.cpp 의 warn/limit 송신
)
```

- [ ] **Step 14.2: render CMake** — 소스 목록에 2줄 추가 + scene PUBLIC 승격:
```cmake
    actor_factory.cpp             # 사이클 해소 (2026-06-11) — ScreenCamera/Skybox 팩토리 (scene 에서 이주)
    model_spawner.cpp             # 사이클 해소 (2026-06-11) — Model RenderUnit -> Actor 변환 (scene 에서 이주)
```
link 블록 (Task 7.4 결과에 이어서):
```cmake
target_link_libraries(sjhopengl_render
    PUBLIC  SJH::common SJH::program project_deps
            SJH::buffer   # device_context.h : RenderTarget (사이클 해소 2026-06-11 - buffer 거주) + framebuffer.h
            SJH::scene    # mesh_renderer.h/actor_factory.h : Scene::Component/Actor 공개 헤더 노출 (PUBLIC 승격)
    PRIVATE SJH::diagnostics SJH::material SJH::object SJH::resource_registry
)
```

- [ ] **Step 14.3: 호출처 include 갱신**
  - `apps/_MyApp_/main.cpp:58`: `#include "scene/compound_actor.h"` → `#include "apps/_MyApp_/src/Bootstrap/actor_factory.h"` (main.cpp 는 CreateScreenCameraActor 만 사용 — :131)
  - `apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp`: 35행 `#include "scene/compound_actor.h"` 는 **유지** (CreateCameraActor:56 + CreateDirLightActor:91), 그 다음 줄에 추가: `#include "apps/_MyApp_/src/Bootstrap/actor_factory.h"  // CreateSkyboxActor (사이클 해소 - render 이주)`
  - `<apps>/_MyApp_/src/Stage/StageBuilder.cpp:38`: `#include "apps/_MyApp_/src/Bootstrap/model_spawner.h"` → `#include "apps/_MyApp_/src/Bootstrap/model_spawner.h"`

### Task 15: 슬라이스 ③ 빌드 검증 + 보고

- [ ] **Step 15.1:** Run: `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -20` — Expected: error 0.
- [ ] **Step 15.2:** Run: `grep -rn '"apps/_MyApp_/src/Bootstrap/model_spawner.h"\|CreateSkyboxActor\|CreateScreenCameraActor' src/scene/` — Expected: 0건.
- [ ] **Step 15.3: 사용자 커밋 게이트 — 보고만.** 권장:

```bash
git add src/scene/compound_actor.h src/scene/compound_actor.cpp apps/_MyApp_/src/Bootstrap/model_spawner.h apps/_MyApp_/src/Bootstrap/model_spawner.cpp \
        src/scene/CMakeLists.txt apps/_MyApp_/src/Bootstrap/actor_factory.h apps/_MyApp_/src/Bootstrap/actor_factory.cpp \
        apps/_MyApp_/src/Bootstrap/model_spawner.h apps/_MyApp_/src/Bootstrap/model_spawner.cpp src/render/CMakeLists.txt \
        apps/_MyApp_/main.cpp apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp <apps>/_MyApp_/src/Stage/StageBuilder.cpp
git commit -m "[refactor] : E2 사이클 절단 - render-결합 팩토리를 render/actor_factory 로 이주 (spec D4)"
```

---

# 슬라이스 ① — E3/E5/E6 (texture 모듈 추출 + D8 SpriteRenderer DI)

### Task 16: `src/texture/` 신설 (4파일 mv + CMake)

**Files:**
- Move: `src/resource_registry/{texture.h,texture.cpp,image.h,image.cpp}` → `src/texture/`
- Create: `src/texture/CMakeLists.txt`

- [ ] **Step 16.1: 디렉토리 + 이동**

```bash
mkdir -p src/texture
mv src/texture/texture.h src/texture/texture.cpp \
   src/texture/image.h src/texture/image.cpp src/texture/
```

- [ ] **Step 16.2: texture.cpp 자기 include 수정** — `src/texture/texture.cpp:16`:
```cpp
#include "resource_registry.h"
```
→
```cpp
#include "texture.h"
```
(texture.cpp 는 ResourceRegistry 심볼 미사용 — 검증 완료. `image.cpp:14` 의 `#include "image.h"`, `texture.h:29` 의 `#include "image.h"` 는 동일 디렉토리 이동이라 무수정. **`STB_IMAGE_IMPLEMENTATION` 은 image.cpp 에 그대로 — 단일 owner 불변식 보존.**)

- [ ] **Step 16.3: CMakeLists 생성** — `src/texture/CMakeLists.txt`:

```cmake
# SJH::texture — GL 텍스처(GPU RAII) + 이미지(CPU stb_image RAII) leaf 모듈.
# 의존 사이클 해소 (2026-06-11, spec D1) — 과거 resource_registry 거주.
# leaf 타입과 캐시 파사드의 책임 분리로 E3/E5/E6 (buffer/object/sprite <-> rr) 절단.
# STB_IMAGE_IMPLEMENTATION 은 image.cpp 단 한 곳 (stb_image 단일 owner 불변식).
# stb_image.h 는 include/ 체크인본을 ${CMAKE_SOURCE_DIR}/include 로 해결 — 구 rr CMake 의
# ${Stb_INCLUDE_DIR}(프로젝트 어디에도 미정의인 no-op 변수) PRIVATE 줄은 image.cpp 이동과 함께 의도적 폐기.
add_library(sjhopengl_texture STATIC
    texture.cpp
    image.cpp
)
add_library(SJH::texture ALIAS sjhopengl_texture)

target_include_directories(sjhopengl_texture
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            ${CMAKE_SOURCE_DIR}/include   # vmath.h (image.h) + stb_image.h (image.cpp)
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sjhopengl_texture
    PUBLIC  SJH::common     # texture.h/image.h : CLASS_PTR + common.h
            project_deps    # GL/gl3w.h + OpenGL link
    PRIVATE spdlog          # texture.cpp/image.cpp 로그
)

target_compile_features(sjhopengl_texture PUBLIC cxx_std_17)
```

- [ ] **Step 16.4: src/CMakeLists.txt 우산 합류** — `add_subdirectory(text)` 다음 줄에:
```cmake
add_subdirectory(texture)   # <- 추가 (SJH::texture, 사이클 해소 2026-06-11 — 18 모듈)
```
`target_link_libraries(sjhopengl_engine INTERFACE ...)` 블록의 `SJH::text` 줄 다음에:
```cmake
    SJH::texture            # 18 모듈 — Texture/Image leaf (사이클 해소 2026-06-11)
)
```

### Task 17: resource_registry 정리 (캐시 파사드화)

**Files:**
- Modify: `src/resource_registry/resource_registry.h:35,42`
- Modify: `src/resource_registry/CMakeLists.txt`

- [ ] **Step 17.1: rr 헤더 include 2줄** — `src/resource_registry/resource_registry.h`:
  - 35행 `#include "image.h"` → `#include "texture/image.h"`
  - 42행 `#include "texture.h"` → `#include "texture/texture.h"`

- [ ] **Step 17.2: rr CMake** — `src/resource_registry/CMakeLists.txt` 전체 교체:

```cmake
add_library(sjhopengl_resource_registry STATIC
    resource_registry.cpp
    sound.cpp                # M5 — FMOD::Sound RAII wrap
    effect.cpp               # M5 — Effekseer::EffectRef wrap
    sprite_resources.cpp     # D8 (2026-06-11) — SpriteRenderer 공유 자원 ensure 팩토리 (sprite 에서 이주)
)
add_library(SJH::resource_registry ALIAS sjhopengl_resource_registry)

target_include_directories(sjhopengl_resource_registry
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
            ${CMAKE_SOURCE_DIR}/include   # vmath.h
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# 사이클 해소 (2026-06-11) — Texture/Image leaf 는 SJH::texture 로 분리, rr 은 캐시 파사드 전담.
# buffer/sprite 는 resource_registry.h 가 framebuffer.h/uniform_atlas.h 를 노출하므로 PUBLIC (단방향 명시화).
target_link_libraries(sjhopengl_resource_registry
    PUBLIC  SJH::common
            SJH::texture       # resource_registry.h: Texture/Image 캐시 API 노출
            SJH::buffer        # resource_registry.h: Framebuffer 캐시 (CreateFramebuffer)
            SJH::object        # resource_registry.h: Mesh/Model 노출 (RegisterMesh/CreateModel)
            SJH::material      # resource_registry.h: CreateMaterial/FindMaterial 가 Material* 노출
            SJH::program       # resource_registry.h: CreateProgram/FindProgram 가 Program* 노출
            SJH::sprite        # resource_registry.h: UniformAtlas 캐시 + sprite_resources
            project_deps
            spdlog
            game_deps          # M5 — Sound/Effect 헤더가 FMOD/Effekseer 노출 (spec §6.1) — 불변
    PRIVATE SJH::diagnostics
)

target_compile_features(sjhopengl_resource_registry PUBLIC cxx_std_17)
```
(주의: `sprite_resources.cpp` 는 Task 21 에서 생성 — Task 21 완료 전 configure 하면 실패하므로 **Task 16~23 을 마친 뒤에만 빌드**.)

### Task 18: src 쪽 include + consumer CMake 교체

**Files:**
- Modify: `<src>/buffer/framebuffer.h:27`, `src/buffer/CMakeLists.txt`
- Modify: `src/object/model.h:24`, `src/object/model.cpp:21`, `src/object/CMakeLists.txt`
- Modify: `src/sprite/uniform_atlas.h:26`, `src/sprite/uniform_atlas.cpp:21`, `src/sprite/CMakeLists.txt`
- Modify: `<src>/render/property_block_setter.cpp:35`, `src/render/CMakeLists.txt`

- [ ] **Step 18.1: include 6곳** — `"src/texture/texture.h"` → `"texture/texture.h"` (framebuffer.h:27 / model.h:24 / model.cpp:21 / uniform_atlas.h:26 / property_block_setter.cpp:35), `"src/texture/image.h"` → `"texture/image.h"` (uniform_atlas.cpp:21). 꼬리 주석 보존.

- [ ] **Step 18.2: buffer CMake** — Task 7.3 결과에서 `SJH::resource_registry` 줄을 교체 (**E3 절단점**):
```cmake
            SJH::texture              # framebuffer.h : TexturePtr / Texture (사이클 해소 2026-06-11)
```

- [ ] **Step 18.3: object CMake** — Task 3.2 결과의 PUBLIC 블록에 추가 (**E5 절단점** — 기존 transitive 의존 명시화):
```cmake
            SJH::texture     # model.h: TextureUPtr (사이클 해소 2026-06-11 — rr 분리로 명시 link)
```

- [ ] **Step 18.4: sprite CMake** — `src/sprite/CMakeLists.txt` PUBLIC 블록에서 `SJH::resource_registry # SJH::Image / SJH::Texture / 공유 자원 lookup` 줄을 교체 (**E6 절단점 — D8 과 짝**):
```cmake
    SJH::texture # uniform_atlas.h: SJH::Texture / SJH::Image (사이클 해소 2026-06-11 — rr 의존 제거)
```

- [ ] **Step 18.5: render CMake** — PRIVATE 블록에 `SJH::texture` 추가 (property_block_setter.cpp). `SJH::resource_registry` PRIVATE 는 유지 (render_pipeline.cpp/scene_renderer.cpp 의 캐시 사용 — render→rr 단방향 무해).

### Task 19: apps include 갱신 (12파일, 각 1줄)

**Files (Modify — 전부 include 라인만):**

| 파일 | 행 | 변경 |
|---|---|---|
| `apps/_MyApp_/main.cpp` | 55 | `src/texture/image.h` → `texture/image.h` |
| `apps/_MyApp_/src/UI/PauseButtonLayer.h` | 24 | `src/texture/texture.h` → `texture/texture.h` |
| `apps/_MyApp_/src/UI/UiBootstrap.cpp` | 19 | image → `texture/image.h` |
| `apps/_MyApp_/src/UI/StateOverlayLayer.h` | 25 | texture → `texture/texture.h` |
| `apps/_MyApp_/src/Bootstrap/EnemyBuilder.cpp` | 46 | image → `texture/image.h` |
| `apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp` | 30, 32 | image/texture 2줄 |
| `apps/_MyApp_/src/Bootstrap/PlayerBuilder.cpp` | 53 | image → `texture/image.h` |
| `apps/_MyApp_/src/Stage/Components/GameContextComponent.h` | 24 | texture → `texture/texture.h` |
| `apps/_MyApp_/src/Playable/SpriteFxPlayable.cpp` | 20 | image → `texture/image.h` |
| `<apps>/_MyApp_/src/Stage/StageBuilder.cpp` | 34, 36 | image/texture 2줄 |

- [ ] **Step 19.1: 위 표대로 일괄 치환** (꼬리 주석 보존). 끝나면 검증:

Run: `grep -rn '"src/texture/texture.h"\|"src/texture/image.h"' src/ apps/ test/`
Expected: 0건.

### Task 20: test 갱신

**Files:**
- Modify: `<test>/test_texture.cpp:32` 부근 — `resource_registry/resource_registry.h` include 는 **무수정** (registry 캐시 API 테스트). 파일 내 `"src/texture/texture.h"` 직접 include 가 있으면 `"texture/texture.h"` 로.
- Modify: `test/CMakeLists.txt` — test_texture 의 `SJH::resource_registry` link 유지 (rr 이 texture 를 PUBLIC 전파). 변경 불요 확인만.

- [ ] **Step 20.1:** Run: `grep -rn 'src/texture/texture.h\|src/texture/image.h' test/` — 발견 시 `texture/` 경로로 치환, 없으면 통과.

### Task 21: D8 — `sprite_resources.h` 신설

**Files:**
- Create: `src/resource_registry/sprite_resources.h`

- [ ] **Step 21.1: 파일 생성**

```cpp
/**
 * @file sprite_resources.h
 * @brief SpriteRenderer 공유 자원 ensure/생성 팩토리 - billboard plane Mesh + per-instance Material.
 *
 * @details
 *  ### 책임
 *  - @c "_sprite_plane" Mesh / @c "_sprite_billboard_program" Program /
 *    @c "_sprite_billboard" template SharedMaterial 의 find-or-create.
 *  - @c "_sprite_inst_N" per-instance Material 생성 + 초기 uniform(uUvRect/uFlipX/uTint) 시드.
 *
 *  ### 비-책임
 *  - [X] 매 프레임 uniform 동기화 - @c SpriteRenderer::Update 책임.
 *  - [X] atlas 로드/캐시 - @c ResourceRegistry::CreateUniformAtlas 책임.
 *
 * @note 의존 사이클 해소 D8 (2026-06-11) - 과거 sprite_component.cpp 익명 헬퍼.
 *       sprite -> resource_registry 역의존 제거(DI)를 위해 rr 모듈로 이주.
 *       호출자(클라이언트 빌더)가 본 팩토리로 자원을 해결한 뒤
 *       @c AddComponent<SpriteRenderer>(atlas, mesh, material) 로 주입한다.
 */
#ifndef __SJH_SPRITE_RESOURCES_H__
#define __SJH_SPRITE_RESOURCES_H__

namespace SJH
{
	class Material;
	class Mesh;
	class ResourceRegistry;
} // namespace SJH

namespace SJH::Sprite
{
	class UniformAtlas;
} // namespace SJH::Sprite

namespace SJH::SpriteResources
{
	/// @brief @c "_sprite_plane" 공유 plane Mesh find-or-create. 항상 동일 인스턴스.
	/// @param reg 자원 캐시 (보통 @c ResourceRegistry::Get()).
	/// @return 공유 Mesh (비소유 - owner 는 reg). 생성 실패 시 nullptr.
	Mesh *EnsureSharedPlane(ResourceRegistry &reg);

	/// @brief billboard template find-or-create 후 per-instance Material 생성 (@c "_sprite_inst_N").
	/// @param reg   자원 캐시.
	/// @param atlas 초기 @c uAtlas 텍스처 + @c uUvRect 시드용. @c nullptr 이면 @c nullptr 반환.
	/// @return per-instance Material (비소유 - owner 는 reg). 실패 시 nullptr.
	Material *CreateInstanceMaterial(ResourceRegistry &reg, Sprite::UniformAtlas *atlas);
} // namespace SJH::SpriteResources

#endif // __SJH_SPRITE_RESOURCES_H__
```

### Task 22: D8 — `sprite_resources.cpp` 신설 (헬퍼 이주)

**Files:**
- Create: `src/resource_registry/sprite_resources.cpp`

- [ ] **Step 22.1: 파일 생성** — 로직은 `src/sprite/sprite_component.cpp:33-98` 의 익명 헬퍼 3종을 *reg 파라미터화* 만 하고 그대로 이주:

```cpp
/**
 * @file sprite_resources.cpp
 * @brief SpriteResources 구현 - sprite 공유 자원 find-or-create + per-instance Material 시드.
 *
 * @details
 *  ### ResourceRegistry 키 컨벤션 ('_' 접두 - 사용자 namespace 격리)
 *  - @c "_sprite_plane"             (Mesh)
 *  - @c "_sprite_billboard_program" (Program)
 *  - @c "_sprite_billboard"         (template SharedMaterial)
 *  - @c "_sprite_inst_N"            (per-instance MaterialInstance, N = 단조 증가 counter)
 *
 * @note 사이클 해소 D8 (2026-06-11) - sprite_component.cpp 익명 헬퍼에서 이주.
 *       @c ResourceRegistry::Get() 싱글톤 직접 호출 대신 @p reg 파라미터 주입 (테스트 가능성).
 */
#include "resource_registry/sprite_resources.h"

#include "resource_registry/resource_registry.h"
#include "material/material.h"
#include "material/material_uniforms.h"
#include "material/pass.h"
#include "object/mesh.h"
#include "sprite/uniform_atlas.h"

#include <<spdlog>/spdlog.h>
#include <string>
#include <vmath.h>

namespace SJH::SpriteResources
{
	namespace
	{
		constexpr const char *kPlaneKey = "_sprite_plane";
		constexpr const char *kProgramKey = "_sprite_billboard_program";
		constexpr const char *kTemplateKey = "_sprite_billboard";

		SJH::Material *EnsureTemplateMaterial(ResourceRegistry &reg)
		{
			if (auto *tpl = reg.FindSharedMaterial(kTemplateKey))
				return tpl;

			SJH::Program *prog = reg.FindProgram(kProgramKey);
			if (!prog)
			{
				prog = reg.CreateProgram(
				    kProgramKey,
				    "resources/shaders/billboard_atlas.vs",
				    "resources/shaders/billboard_atlas.fs");
				if (!prog)
				{
					spdlog::error("SpriteResources: billboard_atlas shader 로드 실패");
					return nullptr;
				}
			}

			auto *tpl = reg.CreateSharedMaterial(kTemplateKey);
			if (tpl)
			{
				tpl->SetProgram(prog);
				tpl->SetPass(SJH::Pass::Kind::AlphaTest);
			}
			return tpl;
		}
	} // namespace

	SJH::Mesh *EnsureSharedPlane(ResourceRegistry &reg)
	{
		if (auto *m = reg.FindMesh(kPlaneKey))
			return m;
		return reg.RegisterMesh(kPlaneKey, SJH::Mesh::CreatePlane());
	}

	SJH::Material *CreateInstanceMaterial(ResourceRegistry &reg, Sprite::UniformAtlas *atlas)
	{
		if (!atlas)
			return nullptr;
		auto *tpl = EnsureTemplateMaterial(reg);
		if (!tpl)
			return nullptr;

		// 단조 증가 - 동일 프로세스 안 unique. ResourceRegistry::Clear 이후에도 충돌 없음.
		static int counter = 0;
		const std::string key = std::string("_sprite_inst_") + std::to_string(++counter);
		auto *inst = reg.CreateMaterialInstanceFrom(key, tpl);
		if (!inst)
			return nullptr;

		inst->Properties.Textures["uAtlas"] = {atlas->GetTexture(), /*unit=*/0};
		Uniforms::SetVec4(*inst, "uUvRect", atlas->GetUVRect(/*frameIdx=*/0));
		Uniforms::SetFloat(*inst, "uFlipX", 1.0f);
		Uniforms::SetVec4(*inst, "uTint", vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));
		return inst;
	}
} // namespace SJH::SpriteResources
```

### Task 23: D8 — SpriteRenderer DI 전환

**Files:**
- Modify: `src/sprite/sprite_component.h`
- Modify: `src/sprite/sprite_component.cpp`

- [ ] **Step 23.1: 헤더 — 생성자 시그니처 교체** — `src/sprite/sprite_component.h` 의 기존 생성자 선언(Doxygen 69~71행 + 선언 72행):

```cpp
        /// @brief atlas 주입 생성자 - plane + program + per-instance Material 자동 해결 (ctor 내 1회).
        /// @details @p atlas == @c nullptr 이면 Material 도 @c nullptr - 빈 SpriteRenderer (테스트/지연 셋업).
        /// @param atlas 사용할 @c UniformAtlas 포인터. @c nullptr 허용.
        explicit SpriteRenderer(UniformAtlas* atlas = nullptr);
```
→
```cpp
        /// @brief 자원 주입 생성자 - 공유 plane Mesh + per-instance Material 을 외부에서 주입 (DI).
        /// @details 자원 해결은 @c SJH::SpriteResources (resource_registry/sprite_resources.h) 팩토리 책임.
        ///          사이클 해소 D8 (2026-06-11) 로 본 컴포넌트의 ResourceRegistry 자가해결 폐기 -
        ///          sprite 모듈의 resource_registry 의존 제거.
        ///          모든 인자 @c nullptr 허용 - 빈 SpriteRenderer (테스트/지연 셋업, Update 가 skip).
        /// @param atlas            사용할 @c UniformAtlas 포인터.
        /// @param mesh             공유 billboard plane (@c SpriteResources::EnsureSharedPlane 결과).
        /// @param materialInstance per-instance Material (@c SpriteResources::CreateInstanceMaterial 결과).
        explicit SpriteRenderer(UniformAtlas* atlas = nullptr,
                                SJH::Mesh* mesh = nullptr,
                                SJH::Material* materialInstance = nullptr);
```
파일 헤더 Doxygen 의 "ResourceRegistry 경유 자동 해결" 문구(비-책임 13행 + 클래스 Doxygen "자동 해결되는 공유 자원" 절 + 키 컨벤션 @note)는 다음 요지로 갱신: *공유 자원 해결은 `SpriteResources` 팩토리 + 호출자 주입 (D8). 키 컨벤션 문서는 sprite_resources.h 로 이전.*

- [ ] **Step 23.2: cpp 재작성** — `src/sprite/sprite_component.cpp` 전체를 다음으로 교체 (Update 본문 106~134행은 기존 그대로):

```cpp
/**
 * @file sprite_component.cpp
 * @brief SpriteRenderer 구현 - 주입된 Mesh/Material 보유 + 매 프레임 uniform 동기화.
 *
 * @details
 *  ### 책임
 *  - @c SpriteRenderer::Update : atlas UV / tint / flipX / roll / 피격 / 디졸브 uniform 동기화.
 *
 *  ### 비-책임
 *  - [X] 공유 자원(plane Mesh / billboard Material) 생성 - 사이클 해소 D8 (2026-06-11) 로
 *    @c SJH::SpriteResources (resource_registry/sprite_resources.cpp) 가 담당, 호출자가 주입.
 */
#include "sprite/sprite_component.h"

#include "material/material.h"
#include "material/material_uniforms.h"
#include "sprite/uniform_atlas.h"

namespace SJH::Sprite
{
	SpriteRenderer::SpriteRenderer(UniformAtlas *atlasPtr, SJH::Mesh *mesh, SJH::Material *materialInstance)
	    : MeshRenderer(mesh, materialInstance),
	      atlas(atlasPtr)
	{
	}

	void SpriteRenderer::Update(float dt)
	{
		// <<< 기존 src/sprite/sprite_component.cpp:106-134 의 Update 본문 그대로 >>>
	}
} // namespace SJH::Sprite
```

### Task 24: SpriteRenderer 생성 지점 3곳 DI 호출 갱신 (클라 2 + 엔진 text 1)

**Files:**
- Modify: `apps/_MyApp_/src/Playable/SpriteLayerFactory.cpp`
- Modify: `apps/_MyApp_/src/Entity/Player/PlayerHand.cpp`
- Modify: `src/text/text_renderer.cpp` (검증에서 발견된 **세 번째 생성 지점** — 누락 시 빌드는 GREEN 이지만 월드 텍스트가 통째로 사라지는 무성 회귀)

⚠ **본문 변경 — Entity/ 활성 편집 영역 포함. 직전 `git status` 로 세 파일 미편집 확인, 편집 중이면 중단 후 보고.**

- [ ] **Step 24.1: SpriteLayerFactory.cpp** — 20행 다음에 include 추가:
```cpp
#include "resource_registry/sprite_resources.h" // EnsureSharedPlane/CreateInstanceMaterial (D8 - 자원 주입)
```
44행 교체:
```cpp
		auto *spr        = target.AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
```
→
```cpp
		auto *spr        = target.AddComponent<SJH::Sprite::SpriteRenderer>(
		    atlas,
		    SJH::SpriteResources::EnsureSharedPlane(reg),
		    SJH::SpriteResources::CreateInstanceMaterial(reg, atlas));
```

- [ ] **Step 24.2: PlayerHand.cpp** — 36행(`resource_registry/resource_registry.h`) 다음에 include 추가:
```cpp
#include "resource_registry/sprite_resources.h" // EnsureSharedPlane/CreateInstanceMaterial (D8 - 자원 주입)
```
108행 교체 (`reg` 는 85행 `auto &reg = SJH::ResourceRegistry::Get();` 가 람다 `[&]` 캡처로 가시):
```cpp
				auto *spr = hand->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
```
→
```cpp
				auto *spr = hand->AddComponent<SJH::Sprite::SpriteRenderer>(
				    atlas,
				    SJH::SpriteResources::EnsureSharedPlane(reg),
				    SJH::SpriteResources::CreateInstanceMaterial(reg, atlas));
```

- [ ] **Step 24.3: text_renderer.cpp (엔진 SJH::text — 월드 텍스트 글리프)** — include 블록(20행 `sprite/sprite_component.h` 다음)에 2줄 추가 (이 파일은 현재 rr include 가 없음):
```cpp
#include "resource_registry/resource_registry.h" // ResourceRegistry::Get (D8 - 자원 주입)
#include "resource_registry/sprite_resources.h"  // EnsureSharedPlane/CreateInstanceMaterial
```
글리프 루프 직전(66행 `float penX = ...` 다음)에 공유 자원 1회 해결 추가, 79행 생성을 주입형으로 교체 (이 파일은 space indent — 주변에 맞춤):
```cpp
        auto& reg = SJH::ResourceRegistry::Get();
        SJH::Mesh* plane = SJH::SpriteResources::EnsureSharedPlane(reg);

        for (char c : mText)
        {
            ...
                auto* sr = glyph->AddComponent<SJH::Sprite::SpriteRenderer>(
                    mFont->GetAtlas(), plane,
                    SJH::SpriteResources::CreateInstanceMaterial(reg, mFont->GetAtlas()));
```
(`plane` 은 루프 밖 1회, `CreateInstanceMaterial` 은 글리프마다 — per-glyph `frameIdx`/`uUvRect` 가 다르므로 per-instance Material 필수. CMake 변경 불요 — `src/text/CMakeLists.txt` 가 이미 `SJH::resource_registry` link, text→rr 은 단방향이라 무해.)

- [ ] **Step 24.4: 생성 지점 전수 재확인** — Run: `grep -rn "AddComponent<.*SpriteRenderer>" src/ apps/ test/` — 결과가 위 3곳(+ 주석 제외)뿐인지 확인. 추가 발견 시 동일 패턴으로 갱신.

### Task 25: 슬라이스 ① 빌드 검증 + 테스트 정합 + 보고

- [ ] **Step 25.1:** Run: `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -20` — Expected: error 0. (신규 디렉토리/파일이라 configure 필수.)
- [ ] **Step 25.2: 절단 최종 검증**

Run: `grep -rn '"resource_registry/resource_registry.h"\|"src/texture/texture.h"\|"src/texture/image.h"' src/sprite/ src/buffer/ src/object/`
Expected: 0건 (sprite/buffer/object 의 rr 의존 완전 소멸).

- [ ] **Step 25.3: 테스트 wiring 정합** — Run: `cmake --preset ninja -DENABLE_TESTING=ON 2>&1 | tail -5 && cmake --build --preset ninja --target test_texture test_light 2>&1 | tail -10`
Expected: configure + 두 타겟 컴파일 성공. 끝나면 `cmake --preset ninja` 로 OFF 복원.
- [ ] **Step 25.4: 사용자 커밋 게이트 — 보고만.** 권장:

```bash
git add src/texture/ src/resource_registry/ <src>/buffer/framebuffer.h src/buffer/CMakeLists.txt \
        src/object/model.h src/object/model.cpp src/object/CMakeLists.txt \
        src/sprite/ <src>/render/property_block_setter.cpp src/render/CMakeLists.txt src/CMakeLists.txt \
        src/text/text_renderer.cpp \
        apps/_MyApp_/main.cpp apps/_MyApp_/src/UI/ apps/_MyApp_/src/Bootstrap/ \
        apps/_MyApp_/src/Stage/ apps/_MyApp_/src/Playable/ apps/_MyApp_/src/Entity/Player/PlayerHand.cpp \
        test/
git commit -m "[refactor] : E3/E5/E6 사이클 절단 - SJH::texture 모듈 추출 + SpriteRenderer DI (spec D1/D8)"
```
**+ GUI 육안 검증 요청 (D8 이 유일한 동작-영향 변경):** ① 스프라이트(플레이어/적/손) 표시 ② **월드 텍스트(BitmapFont 데미지 텍스트 등) 표시** — text_renderer 가 세 번째 DI 지점이라 텍스트 누락 = Step 24.3 회귀.

---

# Task 26: 문서 후속 (사용자 확인 후 별도 — 임의 진행 금지)

- [ ] `doxygen/pages/00-mainpage.md` ModuleDeps 그래프 — texture 노드 추가 + mutual 표기 정정 (6→0). **uncommitted 사용자 파일 — 편집 전 조율.**
- [ ] `.claude/CLAUDE.md` + `.claude/architecture.md` — 모듈 17→18, texture 행 추가, resource_registry/sprite/object/scene/render/buffer 서술 갱신.
- [ ] `doc/handoffs/2026-06-11/2026-06-11-dependency-cycle-refactor-handoff.md` Change log — E1~E6 해소 기록 (C1~C4 미착수 잔존 명시).
- [ ] memory `stb_image_owner_resource_registry` — owner 경로가 `src/texture/image.cpp` 로 바뀜을 갱신.

## 검증 요약 (전 슬라이스 공통 기대값)

| 시점 | 명령 | 기대 |
|---|---|---|
| 각 슬라이스 끝 | `cmake --preset ninja && cmake --build --preset ninja --target _MyApp_` | error 0 |
| 슬라이스 ② 후 | `grep -rn '"src/buffer/render_target.h"' src/ apps/` | 0건 |
| 슬라이스 ③ 후 | scene 디렉토리에 render include 0건 | `grep -rn '"render/' src/scene/` → 0건 |
| 슬라이스 ① 후 | sprite/buffer/object 의 rr include 0건 | 위 Step 25.2 |
| 최종 | `cmake --build build_ninja --target doxygen` (선택) | Graphviz 경고 0 유지 |
