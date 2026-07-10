# PCB 장식 모델 → WorldSceneBuilder 이관 Implementation Plan

> ⚠ 2026-07 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> 🟢 **완료 + 초과 진화 (2026-07-09).** 본 plan(PCB 이관 Task1~4)은 **커밋 `0b85943`으로 완료**(골든 14/14 bit-동일).
> 이후 사용자 결정으로 작업이 크게 확장됨 → **StageBuilder 전체 흡수(`4ec8804`) + Bootstrap↔Stage 사이클 절단(미커밋)**.
> **재개 단일 진입점 = [`doc/handoffs/2026-07-09/2026-07-09-stagebuilder-merge-cycle-break-resume-handoff.md`](../../handoffs/2026-07-09/2026-07-09-stagebuilder-merge-cycle-break-resume-handoff.md)** (loose end 3개 + 다음 액션).
> 아래 원본 plan은 *이력 참고용* — 실제 코드는 이 plan을 넘어섬(StageBuilder는 이제 삭제됨, 물리/렌더 경계 기준도 사용자가 기각).

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** PCB 장식 3D 모델을 `StageBuilder`(물리 아레나) 에서 `WorldSceneBuilder`(환경/렌더-only) 로 이관해, 모듈 경계를 "물리 결합 vs 렌더-only" 로 재획정한다. Orbit VFX 는 gameplay 시스템(GameSystems/Effekseer) 결합이라 StageBuilder 에 잔류한다.

**Architecture:** `WorldSceneBuilder::BuildWorldScene` 이 카메라·광·스카이박스에 더해 PCB 장식 모델을 조립(`BuildPcbModel` 신규 free 함수). `StageBuilder::CreateStageActor` 는 PCB 를 제거하고 물리 벽 4개 + StageState + Orbit VFX 만 남긴다. **렌더 출력은 불변**이어야 하며, 이미 커밋된 골든 14장(`test/golden/`)이 bit-동일 유지로 무회귀를 자동 검증한다(characterization refactoring — red-first 아님).

**Tech Stack:** C++17, CMake/Ninja, OpenGL 4.1, macOS. 골든 게이트 = Catch2 + OpenCV(`golden_compare`), env `SJH_GOLDEN_CAPTURE`.

---

## 배경 — 반드시 먼저 읽기 (제로컨텍스트 대비)

- 이 프로젝트는 탑다운 슈터(`apps/_MyApp_`). 씬은 `SJH::Scene::Director::Get().Root()` 아래 Actor 트리. 렌더는 `MeshRenderer`(IRenderable) 를 `RenderQueue` 로 정렬해 발행.
- **두 빌더가 boot 시 나란히 호출**된다 ([apps/_MyApp_/main.cpp:218](../../../apps/_MyApp_/main.cpp#L218) `BuildWorldScene`, [:225](../../../apps/_MyApp_/main.cpp#L225) `CreateStageActor`). **main.cpp 는 이 plan 에서 수정하지 않는다**(호출 시그니처 불변).
- **PCB 모델** = 장식용 3D 모델(`resources/model/pcb.fbx`), Phong 라이팅(Opaque 큐 2000), 물리 무관. 현재 `StageBuilder` 가 소유.
- **골든 게이트가 안전망**: `test/golden/`(14장, 커밋 `401b4a7`)이 REF. 캡처(`SJH_GOLDEN_CAPTURE=1` 로 `_MyApp_` 실행 → `build_ninja/apps/_MyApp_/test/golden/` 에 PNG) 후 `golden_compare` 가 REF 와 비교(5% 예산, 실질 bit-exact). **PCB 를 옮겨도 렌더가 같으면 골든 bit-동일 → 무회귀.**
- ⚠ **원자적 변경**: PCB 를 WorldSceneBuilder 에 추가하고 StageBuilder 에서 제거하는 건 **한 세트**다. 추가만 하고 제거 안 하면 PCB 가 **이중 렌더**(두 PcbActor)된다. 그래서 Task 1~3 을 모두 끝낸 뒤 Task 4 에서 골든 검증 + 단일 커밋한다. **중간(이중-PCB) 상태로 캡처/커밋 금지.**

**공통 규칙**: 주석 한국어 · 커밋 **path-scoped**(`git add <명시경로>`, **`git add -A` 금지** — 사용자가 같은 트리에서 병렬 작업) · **Co-Authored-By 미사용** · 사용자 미커밋 `.clangd` 미접촉 · 빌드 `cmake --build --preset ninja --target _MyApp_` · 캡처 `cd build_ninja/apps/_MyApp_ && SJH_GOLDEN_CAPTURE=1 ./_MyApp_`.

---

## File Structure

| 파일 | 변경 | 책임 |
|---|---|---|
| `apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp` | 수정 | `BuildPcbModel()` free 함수 추가 + `BuildWorldScene` 에서 호출 + include 3종 추가 |
| `apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.h` | 수정 | 파일 doc 에 PCB 장식 책임 명시 |
| `<apps>/_MyApp_/src/Stage/StageBuilder.cpp` | 수정 | PCB 상수/Ensure 헬퍼/조립 코드 제거 + 불필요 include 제거 |
| `<apps>/_MyApp_/src/Stage/StageBuilder.h` | 수정 | 파일 doc 에서 PCB 언급 제거 |
| `test/golden/*.png` | **무변경** | 검증 REF (bit-동일 유지가 성공 기준) |

---

## Task 1: WorldSceneBuilder 에 BuildPcbModel 추가

**Files:**
- Modify: `apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp`

- [ ] **Step 1: include 3종 + `<memory>` 추가**

`WorldSceneBuilder.cpp` 의 `#include "render/mesh_renderer.h"` 줄 **바로 아래**에 추가:
```cpp
#include "object/model.h"            // SJH::Model (BuildPcbModel: GetMaterialCount/GetMaterial)
#include "program/program.h"         // SJH::Program (PCB phong 프로그램)
#include "apps/_MyApp_/src/Bootstrap/model_spawner.h" // SJH::Scene::ModelSpawner::SpawnEntities (PCB RenderUnit 펼침)
```
그리고 파일 맨 아래 `#include <<glm>/glm.hpp>` 위에 `#include <memory>` 가 없으면 추가(`std::make_unique` 용):
```cpp
#include <memory>
#include <<glm>/glm.hpp>
```
> 확인: `grep -n '#include <memory>' apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp` 결과가 없으면 추가, 있으면 skip.

- [ ] **Step 2: `BuildPcbModel()` free 함수 추가**

`WorldSceneBuilder.cpp` 의 익명 네임스페이스 안, `BuildSkybox()` 함수의 닫는 `}` **다음 줄**(익명 namespace 를 닫는 `} // namespace` **바로 위**)에 삽입:
```cpp
		// -- PCB 장식 3D 모델 (Phong lit, 물리 무관) - StageBuilder 물리아레나에서 이관 --------
		/// @brief PCB 3D 모델을 Phong 알베도 셰이더로 조립해 @c Director::Root() 에 추가한다.
		/// @details 자원(모델/Phong program)은 idempotent(find-or-create) 등록. model.cpp 가 저장한
		///          material.albedo(Vec3) 를 MaterialBlock.baseColor(Vec4) 로 승격 - slang phong UBO 입력.
		///          @c ModelSpawner 로 모델의 RenderUnit 을 자식 Actor 로 펼친다. 물리 무관이라 환경 요소.
		void BuildPcbModel()
		{
			auto &reg = SJH::ResourceRegistry::Get();
			auto &dir = SJH::Scene::Director::Get();

			// 자원 등록 (idempotent - 이미 있으면 Find 재사용). 키는 continuity 위해 "stage_" 유지.
			SJH::Model *pcbModel = reg.FindModel("stage_pcb");
			if (!pcbModel)
				pcbModel = reg.CreateModel("stage_pcb", "resources/model/pcb.fbx");

			SJH::Program *pcbProg = reg.FindProgram("stage_phong_albedo");
			if (!pcbProg)
				pcbProg = reg.CreateProgram("stage_phong_albedo",
				                            "resources/shaders/phong.vs",
				                            "resources/shaders/phong.fs");

			// slang phong UBO 셰이더 주입 + albedo(Vec3) -> baseColor(Vec4) 승격 (idempotent).
			for (int i = 0; i < pcbModel->GetMaterialCount(); ++i)
			{
				if (SJH::Material *mat = pcbModel->GetMaterial(i))
				{
					if (mat->GetProgram() == nullptr)
						mat->SetProgram(pcbProg);
					const auto albedoIt = mat->Properties.Vec3s.find("material.albedo");
					const glm::vec3 albedo = (albedoIt != mat->Properties.Vec3s.end())
					                             ? albedoIt->second
					                             : glm::vec3(0.8f, 0.8f, 0.8f);
					mat->Properties.Vec4s["baseColor"] = glm::vec4(albedo[0], albedo[1], albedo[2], 1.0f);
				}
			}

			// PCB Actor - Transform + ModelSpawner 로 RenderUnit 자식 펼침. Root 직속(환경 요소).
			auto pcbActor = std::make_unique<SJH::Scene::Actor>("PcbActor");
			pcbActor->GetTransform().SetTransformWithVectors(
			    glm::vec3(0.0, -1.75, 0.0),
			    glm::vec3(90.0, 0.0, 0),
			    glm::vec3(0.75, 0.75, 0.75));
			SJH::Scene::ModelSpawner::SpawnEntities(*pcbActor, *pcbModel);
			dir.Root().AddChild(std::move(pcbActor));
		}
```
> 이 코드는 `StageBuilder.cpp` 의 기존 PCB 로직(EnsurePcbModel + EnsurePhongAlbedoProgram + albedo 승격 루프 + pcbActor 조립)을 **동작 동일하게** 옮긴 것이다. 리소스 키/모델 경로/Transform 값/albedo 기본값 모두 원본과 일치해야 골든이 bit-동일 유지된다 — **값을 바꾸지 말 것.**

- [ ] **Step 3: `BuildWorldScene` 에서 호출 + 사보타지 잔재 정리**

`BuildWorldScene` 함수를 아래 old→new 로 교체(사보타지 테스트 주석 + 불필요 `{ }` 스코프 제거 겸):

old:
```cpp
	WorldSceneResult BuildWorldScene(const WorldSceneDeps &deps)
	{
		WorldSceneResult result;
		result.WorldCamera = BuildWorldCamera(deps);
		BuildLighting();

		// !! 사보타지 테스팅 (BuildWorldScene)
		{
			auto *skyboxRenderer  = BuildSkybox();
			result.SkyboxRenderer = skyboxRenderer;
			result.SkyboxMat      = skyboxRenderer ? skyboxRenderer->Material : nullptr;
		}

		return result;
	}
```
new:
```cpp
	WorldSceneResult BuildWorldScene(const WorldSceneDeps &deps)
	{
		WorldSceneResult result;
		result.WorldCamera = BuildWorldCamera(deps);
		BuildLighting();

		auto *skyboxRenderer  = BuildSkybox();
		result.SkyboxRenderer = skyboxRenderer;
		result.SkyboxMat      = skyboxRenderer ? skyboxRenderer->Material : nullptr;

		BuildPcbModel(); // PCB 장식 모델 (물리 무관 - StageBuilder 에서 이관).

		return result;
	}
```
> ⚠ old 블록의 `{ }` 스코프/주석이 사용자 편집으로 달라졌을 수 있다 — `grep -n "사보타지 테스팅" apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp` 로 실제 현재 형태를 확인하고 맞춰라. 핵심은 (1) skybox 세팅 유지 (2) 그 뒤 `BuildPcbModel();` 한 줄 추가.

- [ ] **Step 4: 컴파일 확인 (⚠ 아직 이중-PCB 상태 — 캡처 금지)**

Run: `cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -5`
Expected: 링크 성공(`Linking CXX executable apps/_MyApp_/_MyApp_`). sb7 `gl.h and gl3.h` 경고 1개는 정상(무시). **이 시점엔 PCB 가 StageBuilder+WorldSceneBuilder 양쪽에서 이중 조립되므로 캡처/커밋하지 말 것** — Task 2 에서 StageBuilder 쪽을 제거한다.

---

## Task 2: StageBuilder 에서 PCB 제거

**Files:**
- Modify: `<apps>/_MyApp_/src/Stage/StageBuilder.cpp`

- [ ] **Step 1: PCB 상수 5개 제거**

`StageBuilder.cpp` 익명 네임스페이스에서 아래 블록 전체 삭제(각 `// ! 이 내용들의 Constant...` 주석 포함):
```cpp
			// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
			constexpr const char *kPcbKey = "stage_pcb";                ///< PCB 모델 key.
			// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
			constexpr const char *kPcbModelPath = "resources/model/pcb.fbx"; ///< PCB 모델 경로.
			// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
			constexpr const char *kPhongAlbedoProgKey = "stage_phong_albedo"; ///< PCB 용 Phong+알베도 Program key.
			// Phase 2.5 (S7) - slang phong UBO 셰이더로 전환 (구 phong_tex.vs / phong_albedo.fs loose 판 대체).
			//   phong.slang -> phong.{vs,fs} (LightBlock UBO + MaterialBlock.baseColor.rgb=albedo). 값은 UBO 경로(loose 없음).
			// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
			constexpr const char *kPhongAlbedoVS = "resources/shaders/phong.vs"; ///< slang phong VS (LightBlock UBO).
			// ! 이 내용들의 Constant는 적절한 위치로 Static 접근이 가능하게 하는게 좋지 않나?
			constexpr const char *kPhongAlbedoFS = "resources/shaders/phong.fs"; ///< slang phong FS (albedo 기반).
```
> `kPlaneKey`/`kWallMatKey`/`kTransparent*`/`kWallTex*` 는 **남긴다**(벽 자원). PCB 관련(`kPcb*`, `kPhongAlbedo*`)만 제거.

- [ ] **Step 2: `EnsurePhongAlbedoProgram` 헬퍼 제거**

아래 함수 전체 삭제:
```cpp
		/// @brief PCB 모델용 Phong 알베도 Program 을 idempotent 하게 등록/조회.
		/// @param reg 자원 레지스트리.
		/// @return 등록된(또는 기존) @c SJH::Program 포인터.
		SJH::Program *EnsurePhongAlbedoProgram(SJH::ResourceRegistry &reg)
		{
			if (auto *existing = reg.FindProgram(kPhongAlbedoProgKey))
				return existing;
			return reg.CreateProgram(kPhongAlbedoProgKey, kPhongAlbedoVS, kPhongAlbedoFS);
		}
```

- [ ] **Step 3: `EnsurePcbModel` 헬퍼 제거**

아래 함수 전체 삭제:
```cpp
		/// @brief PCB 3D 모델을 idempotent 하게 등록/조회.
		/// @param reg 자원 레지스트리.
		/// @return 등록된(또는 기존) @c SJH::Model 포인터.
		SJH::Model *EnsurePcbModel(SJH::ResourceRegistry &reg)
		{
			if (auto *existing = reg.FindModel(kPcbKey))
				return existing;
			return reg.CreateModel(kPcbKey, kPcbModelPath);
		}
```

- [ ] **Step 4: `CreateStageActor` 안 PCB 등록/셋업 블록 제거**

`CreateStageActor` 에서 아래 블록 전체 삭제(주석 `// PCB 모델 자원 등록 및 캐싱` 부터 albedo 승격 루프 끝까지):
```cpp
			// PCB 모델 자원 등록 및 캐싱
			SJH::Model *pcbModel = EnsurePcbModel(reg);

			// PCB 머티리얼에 slang phong UBO 셰이더 주입 (Phase 2.5 S7).
			//   model.cpp 가 저장한 material.albedo(Vec3) 를 MaterialBlock.baseColor(Vec4) 로 승격 -
			//   phong.slang 이 baseColor.rgb 를 albedo 로 사용 (mesh_pass useUbo 분기가 UpdateUniformBlock).
			SJH::Program *pcbProg = EnsurePhongAlbedoProgram(reg);
			for (int i = 0; i < pcbModel->GetMaterialCount(); ++i)
			{
				if (SJH::Material *mat = pcbModel->GetMaterial(i))
				{
					if (mat->GetProgram() == nullptr)
						mat->SetProgram(pcbProg);
					// albedo(Vec3) -> baseColor(Vec4) 승격 (idempotent). UBO MaterialBlock 의 입력.
					const auto albedoIt = mat->Properties.Vec3s.find("material.albedo");
					const glm::vec3 albedo = (albedoIt != mat->Properties.Vec3s.end())
					                               ? albedoIt->second
					                               : glm::vec3(0.8f, 0.8f, 0.8f);
					mat->Properties.Vec4s["baseColor"] = glm::vec4(albedo[0], albedo[1], albedo[2], 1.0f);
				}
			}
```

- [ ] **Step 5: `CreateStageActor` 안 PcbActor 조립 블록 제거**

아래 블록 전체 삭제:
```cpp
		// 4) PCB 모델 Actor 추가
		auto pcbActor = std::make_unique<SJH::Scene::Actor>("PcbActor");
		pcbActor->GetTransform().SetTransformWithVectors(
		    glm::vec3(0.0, -1.75, 0.0),
		    glm::vec3(90.0, 0.0, 0),
		    glm::vec3(0.75, 0.75, 0.75));

		// ModelSpawner 유틸리티를 사용해 모델의 모든 RenderUnit을 자식 Actor로 펼침
		SJH::Scene::ModelSpawner::SpawnEntities(*pcbActor, *pcbModel);
		stage->AddChild(std::move(pcbActor));
```
> 벽 4개(`spawnWall`) 와 Orbit VFX(`// 5) Orbit 배경 VFX`) 블록은 **남긴다**. `// 5)` 주석 번호는 그대로 둬도 무방(또는 `// 4)` 로 조정 — 기능 무관).

- [ ] **Step 6: 불필요해진 include 제거 (grep 가드)**

PCB 제거로 `apps/_MyApp_/src/Bootstrap/model_spawner.h` 와 `<<assimp>/defs.h>` 가 StageBuilder 에서 안 쓰일 수 있다. 먼저 확인:
```bash
grep -nE "ModelSpawner|SJH::Model\b|assimp|aiReal|ai_real" <apps>/_MyApp_/src/Stage/StageBuilder.cpp
```
Expected: 결과 없음(PCB 제거 후 잔여 사용 0). 결과가 없으면 아래 두 include 삭제:
```cpp
#include "apps/_MyApp_/src/Bootstrap/model_spawner.h"
```
```cpp
#include <<assimp>/defs.h>
```
> `#include "program/program.h"` 는 **남긴다**(`EnsureTransparentProgram` 이 `SJH::Program` 사용). `#include "render/mesh_renderer.h"`, `material.h`, `material_uniforms.h`, `object/mesh.h` 등도 벽/VFX 가 계속 사용하므로 유지. 만약 grep 에 잔여가 있으면 그 include 는 남길 것.

- [ ] **Step 7: 컴파일 확인**

Run: `cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -5`
Expected: 링크 성공. 미해결 심볼(예: `EnsurePcbModel` 잔여 참조)이 나오면 삭제 누락 — 해당 참조를 찾아 제거.

---

## Task 3: 두 빌더 헤더 doc 갱신

**Files:**
- Modify: `apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.h`
- Modify: `<apps>/_MyApp_/src/Stage/StageBuilder.h`

- [ ] **Step 1: WorldSceneBuilder.h — PCB 책임 추가**

`WorldSceneBuilder.h` 파일 상단 doc 의 `### 책임` 목록에서, Skybox 줄 다음에 한 줄 추가:
old:
```
 *  - Matrix Skybox Actor 생성 (프로그램/텍스처/머티리얼/메시 조립) + @c SkyboxMat 반환.
 *  - 세 Actor 모두 @c Director::Root().AddChild 까지 수행 (Pure factory - caller 추가 wiring 불필요).
```
new:
```
 *  - Matrix Skybox Actor 생성 (프로그램/텍스처/머티리얼/메시 조립) + @c SkyboxMat 반환.
 *  - PCB 장식 3D 모델(Phong lit, 물리 무관) 조립 - @c BuildPcbModel (StageBuilder 물리아레나에서 이관).
 *  - 모든 Actor 를 @c Director::Root().AddChild 까지 수행 (Pure factory - caller 추가 wiring 불필요).
```

- [ ] **Step 2: StageBuilder.h — PCB 언급 제거**

`StageBuilder.h` 파일 상단 doc 에서 PCB 언급 2곳 수정:
old (책임):
```
 *  - @c StageConfig 를 받아 물리 벽 4개 + PCB 모델 + Orbit VFX + @c StageState Component 를
 *    자식/컴포넌트로 조립한 Stage @c Actor 를 생성/반환.
```
new:
```
 *  - @c StageConfig 를 받아 물리 벽 4개 + Orbit VFX + @c StageState Component 를
 *    자식/컴포넌트로 조립한 Stage @c Actor 를 생성/반환 (PCB 장식은 WorldSceneBuilder 로 이관).
```
그리고 함수 doc 의 key 나열 줄:
old:
```
	///   - plane mesh / wallMat / pickupMat / simple.vs/fs Program / pcb model 은 cfg.registry 에 자동 등록
	///     (key: "stage_plane" / "stage_wall" / "stage_pickup" / "stage_solid_plane" / "stage_pcb").
```
new:
```
	///   - plane mesh / wallMat / transparent Program 은 cfg.registry 에 자동 등록
	///     (key: "stage_plane" / "stage_wall" / "stage_transparent"). PCB 자원은 WorldSceneBuilder 소관.
```
> `StageBuilder.h` 의 다른 doc 문구(`### 비-책임`, `### 정통 매핑`)는 그대로.

- [ ] **Step 2: 컴파일 확인**

Run: `cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -3`
Expected: 링크 성공(헤더 주석만 바뀌었으므로 재컴파일 후 GREEN).

---

## Task 4: 골든 게이트 무회귀 검증 + 원자적 커밋

**Files:**
- 검증 대상: `test/golden/*.png` (REF, 무변경)

- [ ] **Step 1: 전체 테스트 타겟 빌드**

Run: `cmake --build --preset ninja --target _MyApp_ golden_compare tests gpu_tests 2>&1 | tail -4`
Expected: 전 타겟 링크 성공. `ld: warning: ignoring duplicate libraries` 는 기존 무해 경고(무시).

- [ ] **Step 2: 재캡처 + 골든 bit-동일 검증 (★핵심 무회귀)**

Run:
```bash
cd build_ninja/apps/_MyApp_ && SJH_GOLDEN_CAPTURE=1 ./_MyApp_ >/dev/null 2>&1 ; cd -
bad=0
for f in test/golden/*.png; do
  b="build_ninja/apps/_MyApp_/test/golden/$(basename "$f")"
  cmp -s "$f" "$b" || { echo "DIFF: $(basename "$f")"; bad=1; }
done
[ $bad -eq 0 ] && echo "전 14골든 bit-동일 = PCB 이관 무회귀" || echo "차이 발생 - 아래 판단 참조"
```
Expected: `전 14골든 bit-동일 = PCB 이관 무회귀`.

**만약 DIFF 가 나오면** (특히 `golden_world_opaque` / `golden_full` / `golden_no_imgui`): PCB 는 Opaque 큐라 씬그래프 부모/순서 변경(MainStage 자식 → Root 직속)이 렌더 순서를 바꿨을 수 있다. `MainStage` 는 identity Transform(위치 무변)이라 위치는 안 바뀌지만, **동일 큐에 다른 Opaque 가 있으면 stable_sort 순서가 달라질 수 있다.** 이 경우 **임계값을 조작하지 말고**(게이트 무력화 금지), diff 아티팩트(`build_ninja/test/golden_artifacts/diff_*.png`)를 열어 (a) 순수 순서 차이로 시각 동일한지 (b) 실제 렌더 손실인지 판단 후 보고. 시각 동일하면 새 골든 승인(재캡처본을 REF 로 복사) 여부를 사용자에게 확인.

- [ ] **Step 3: 전체 ctest 회귀 0**

Run: `ctest --test-dir build_ninja --output-on-failure 2>&1 | tail -4`
Expected: `100% tests passed`.

- [ ] **Step 4: 원자적 커밋 (path-scoped)**

> ⚠ 커밋은 사용자 게이트일 수 있다. 프로젝트 관례상 **path-scoped** 로만 커밋하고 `.clangd` 등 무관 파일은 절대 포함하지 말 것. 커밋 전 `git status --short` 로 범위 확인.
```bash
git add apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.cpp apps/_MyApp_/src/Bootstrap/WorldSceneBuilder.h \
        <apps>/_MyApp_/src/Stage/StageBuilder.cpp <apps>/_MyApp_/src/Stage/StageBuilder.h
git commit -m "[refactor] : PCB 장식 모델 StageBuilder -> WorldSceneBuilder 이관 (물리/렌더 경계 재획정)

- WorldSceneBuilder::BuildPcbModel 신설 (Phong lit, 물리 무관 = 환경 요소)
- StageBuilder 는 물리 벽 4개 + StageState + Orbit VFX 만 (물리 결합만 잔류)
- 골든 14장 bit-동일 = 렌더 무회귀 검증"
```
> `.clangd`, `test/golden/*` 는 add 하지 말 것(골든은 무변경, .clangd 는 사용자 것).

---

## Self-Review

**Spec coverage:**
- PCB → WorldSceneBuilder 이관 → Task 1 ✓ (BuildPcbModel + 호출 + include).
- StageBuilder PCB 제거 → Task 2 ✓ (상수/헬퍼2/CreateStageActor 2블록/include).
- Orbit VFX Stage 잔류 → Task 2 가 VFX 미접촉으로 보장 ✓.
- 헤더 doc 정합 → Task 3 ✓.
- 무회귀 검증 → Task 4 골든 bit-동일 + ctest ✓.

**Placeholder scan:** 없음. 값(리소스 키 "stage_pcb"/"stage_phong_albedo", 모델 경로, Transform (0,-1.75,0)/(90,0,0)/(0.75), albedo 기본 0.8) 은 원본과 동일 명시.

**Type consistency:** `BuildPcbModel()`(Task1) 는 반환 void·인자 없음(BuildSkybox 와 달리 per-frame 갱신 불요 → WorldSceneResult 확장 없음). `reg.FindModel/CreateModel`→`SJH::Model*`, `reg.FindProgram/CreateProgram`→`SJH::Program*`, `pcbModel->GetMaterialCount()/GetMaterial(int)`→`SJH::Material*` ([src/object/model.h](../../../src/object/model.h)), `ModelSpawner::SpawnEntities(Actor&, Model&)` — 원본 StageBuilder 시그니처와 일치.

**리스크:**
1. **씬그래프 순서 변경** → 골든 DIFF 가능(Task 4 Step 2 에 판단 지침). PCB 가 Title 유일 Opaque 면 무영향 예상.
2. **리소스 키 유지**("stage_pcb") — WorldSceneBuilder 에서 "stage_" prefix 는 의미상 어색하나 외부 참조 안전 위해 유지. 리네임은 별도 cleanup(그 전 `grep -rn 'stage_pcb\|stage_phong_albedo' apps/ src/` 로 참조 0 확인 필요).
3. **원자성** — Task 1~3 완료 전 캡처/커밋 금지(이중-PCB). Task 4 가 유일 검증+커밋 지점.

## 가드레일 (수신 에이전트)
- 커밋 path-scoped(위 4파일만), `git add -A` 금지, Co-Authored-By 미사용, 주석 한국어.
- 사용자 미커밋 `.clangd` 미접촉. `main.cpp` 미수정(호출 시그니처 불변). `test/golden/*` 무변경(REF).
- clang IDE 의 missing/unused-include 진단은 알려진 거짓(사용자 `.clangd` 의 Strict 정책) — **빌드/ctest 가 진실**, 소스에 include 대량 추가 금지.
- ⚠ `doc/` 는 gitignore 로컬 — 이 plan 은 same-machine 전용. 다른 머신이면 내용을 붙여 전달.
