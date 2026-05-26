# PostFX — Ordered Pass List 설계 지침

> **상태**: 디자인 노트 (구현 전).
> **작성일**: 2026-05-27.
> **대상 독자**: 본 프로젝트 PostFX 리팩토링을 진행할 AI 에이전트 / 개발자.
> **선행 메모리**: [`camera_depth_postfx_misuse`](../../.claude/projects/...) — Camera.Depth 의 PostFX 재활용이 잘못된 이유.
> **관련 문서**: [`doc/EngineAPI.md`](../EngineAPI.md) §3.9 (Camera) / §3.10 (SceneRenderer) / §4.4 (Camera 모드).

---

## 1. 배경 — 현재 PostFX 표현의 문제

[apps/migrate_demo/main.cpp](../../apps/migrate_demo/main.cpp) 의 PostFX 5-pass 체인이 다음과 같이 표현되어 있다:

```
PostFX 한 단계 = Camera 1 + Layer 1 + Quad Actor 1 + Material 1 + Framebuffer 1
```

5 단계를 직렬화하려고 [src/scene/camera.h:41](../../src/scene/camera.h#L41) `Camera::Depth` 를 *체인 단계 인덱스* (1, 2, 3, 4, 5) 로 재활용. [src/render/scene_renderer.cpp:34](../../src/render/scene_renderer.cpp#L34) 가 `a->Depth < b->Depth` 로 정렬해 순차 렌더.

### 문제점

| 항목 | 증상 |
|---|---|
| **Camera.Depth 의미 혼합** | (a) 진짜 view N 개 우선순위 (Unity 정통) + (b) PostFX 체인 단계 — 두 의미가 같은 정수에 섞임. *읽는 사람이 의도를 못 가려냄.* |
| **카메라 속성 낭비** | PostFX quad 에는 `FovYDeg / Aspect / NearZ / FarZ / CullingMask` 가 의미 없음. full-screen blit 인데도 카메라 metadata 를 강제로 가짐. |
| **자원 비대화** | 단계 N 개 = Actor N + Layer N (LAYER bit 소진) + Camera N + FBO N. PostFX 5 개로도 layer bit 5 개를 소비. |
| **확장 불능** | "vignette 다음에 bloom 추가" 같은 *체인 동적 변경* 시 layer/camera/actor 까지 모두 재정렬해야 함. |

---

## 2. 4 엔진 정통 패턴 (조사 결과)

조사 출처: Context7 — Unity Graphics SRP, Unreal 5.7, Godot 4.5, Cocos 3.8.

| 엔진 | PostFX 단계 표현 | 순서 결정 메커니즘 | 카메라 N? |
|---|---|---|---|
| **Unity URP** | `ScriptableRenderPass` | `RenderPassEvent` enum (BeforeRenderingPostProcessing / AfterRenderingPostProcessing / ...) + `renderer.EnqueuePass()` 호출 순서 | ❌ (카메라 1) |
| **Unity HDRP** | `CustomPostProcessVolumeComponent` | `CustomPostProcessInjectionPoint` enum (BeforePostProcess / AfterPostProcess / BeforeTransparent) + Volume Stack priority | ❌ |
| **Unreal** | `UPostProcessMaterial` Blendable | `APostProcessVolume.Priority` → `UMaterial.BlendableLocation` (BL_BeforeTonemapping/BL_AfterTonemapping/...) → `BlendablePriority` (정수) — 3단 분기 | ❌ |
| **Godot 4** | `CompositorEffect` resource | `effect_callback_type` enum (POST_OPAQUE/POST_SKY/POST_TRANSPARENT/...) + `RenderingServer.compositor_set_compositor_effects(rid, Array[RID])` *배열 인덱스* | ❌ |
| **Cocos 3.x** | Custom Render Pipeline Pass | 파이프라인 builder `addRenderPass(...)` *명시 호출 순서* + Effect YAML `phase:` 라벨 | ❌ |

### 4 엔진이 공유하는 3 원칙

1. **PostFX 는 *카메라가 아니라 pass node*** — `RenderPass` / `Blendable` / `CompositorEffect` / `RenderPipelinePass`. 씬 카메라는 *1 개*.
2. **순서 = stage 라벨 + 그 안의 인덱스** — 2축 분리. stage enum (Before/AfterTonemap 등) 가 큰 분류, 정수/배열 인덱스가 미세.
3. **Camera 의 rendering order 는 *진짜 다중 view* 용** — 미니맵, 분할 화면, UI overlay. PostFX 와 *직교*.

---

## 3. 채택 결정

### 3.1 PostFX = 비-카메라 *ordered pass list*

PostFX 체인을 다음 구조로 표현:

```cpp
namespace SJH {

/// @brief PostFX 한 단계 — full-screen quad blit 의 모든 입력.
/// @details Camera 는 *부착되지 않음*. Aspect/Fov/Near/Far 무의미.
///          입출력 FB 와 Material 만 보유 — Godot CompositorEffect 정통.
struct PostFXPass {
    std::string         Name;           ///< 디버그/UI 용 식별자 ("blur" / "bloom" 등)
    Material*           Material;       ///< full-screen shader (PropertyBlock 으로 파라미터 전달)
    Framebuffer*        InputFB;        ///< 이 단계가 읽을 color attachment 의 owner FB (이전 단계 OutputFB 또는 SceneFB)
    Framebuffer*        OutputFB;       ///< 이 단계가 쓸 FB (nullptr = 백버퍼 — 체인의 *마지막* 단계만)
    bool                Enabled = true; ///< 토글 (UI 에서 비활성 시 InputFB→OutputFB 단순 blit 또는 skip-rewire)
    int                 Stage = 0;      ///< Stage enum 의 정수 값 — 같은 stage 안 미세 순서용 sortkey (※ Future, §6.2)
};

/// @brief Stage enum — Unreal BlendableLocation / Godot effect_callback_type 정통.
/// @note  현 단계에서는 LinearChain 만 구현. Stage 분기는 §6.2 future.
enum class PostFXStage : int {
    BeforeTonemap = 0,   ///< HDR 공간 (bloom, exposure)
    AfterTonemap  = 100, ///< LDR 공간 (vignette, color grade, FXAA)
    AfterUI       = 200, ///< 화면 캡쳐 등 — UI 위에 덮는 효과
};

} // namespace SJH
```

### 3.2 SceneRenderer 통합 — 메인 카메라 렌더 *직후* 순차 실행

```cpp
class SceneRenderer {
public:
    void Render(RenderTarget& defaultTarget);
    void SetPostFXChain(std::vector<PostFXPass> chain);   ///< Builder 패턴 — 한 번에 교체
    void ClearPostFXChain();                              ///< 비우면 SceneFB → backbuffer 직접 blit

private:
    void RunPostFXChain(RenderTarget& finalTarget);       ///< Camera loop 종료 후 호출
    std::vector<PostFXPass> mPostFXChain;                 ///< index 순서가 곧 실행 순서 — Godot Array[RID] 정통
};
```

**렌더 흐름** (단일 SceneCamera + PostFX N 단계):

```
1. SceneCamera.GetTargetRenderTarget() == mSceneFB
   → 모든 MeshRenderer 를 mSceneFB 에 렌더 (기존 SceneRenderer 동작)

2. for (i = 0; i < mPostFXChain.size(); ++i):
       PostFXPass& p = mPostFXChain[i];
       if (!p.Enabled) continue;
       DeviceContext::Get().BindTarget(*p.OutputFB or finalTarget);
       DeviceContext::Get().UseProgram(*p.Material->GetProgram());
       PropertyBlockSetter::Set(rc, p.Material->Properties, *p.Material->GetProgram());
       Uniforms::SetTexture(*p.Material->GetProgram(), "uSrcColor",
                            p.InputFB->GetColorAttachment().get(), 0);
       fullScreenQuad.Draw();

3. (마지막 단계의 OutputFB == nullptr) 인 경우 finalTarget 으로 blit 완료
```

**핵심**:
- *체인 인덱스 = 실행 순서* (Godot `Array[RID]` 정통).
- Camera 는 1 개 (SceneCamera), Layer/CullingMask 도 1 개로 충분.
- PostFXPass 자체는 Actor 트리에 없음 — Director traversal 과 무관.

### 3.3 Camera.Depth 의 회복

`Camera::Depth` 는 *진짜 다중 view* 의 우선순위 용도로 복귀 (Unity 원래 의미).

```cpp
sceneCam->Depth   = 0;   // 메인 씬
minimapCam->Depth = 10;  // 미니맵 — 메인 위에
uiCam->Depth     = 100;  // UI overlay — 마지막
```

**금기**:
- PostFX 단계 인덱스를 Camera.Depth 에 인코딩하지 말 것.
- PostFX 단계마다 Camera 를 생성하지 말 것 (Actor + Layer + FBO 비대화).

---

## 4. 마이그레이션 계획 (migrate_demo 기준)

### 4.1 변경 범위

| 파일 | 변경 |
|---|---|
| [src/render/scene_renderer.h](../../src/render/scene_renderer.h) | `SetPostFXChain` / `ClearPostFXChain` / `mPostFXChain` 추가. `Render()` 내부 카메라 loop 종료 후 `RunPostFXChain()` 호출 |
| `src/render/postfx_pass.h` (신규) | `PostFXPass` struct + `PostFXStage` enum |
| [apps/migrate_demo/main.cpp](../../apps/migrate_demo/main.cpp) | `BuildPostFXChain` 이 *Camera Actor 5 개* 대신 *`std::vector<PostFXPass>` 1 개* 빌드. `sceneCam->Depth = 0` 만 남기고 PostFX 카메라 5 개 + LAYER_POSTFX_* 5 개 + quad Actor 5 개 *전부 삭제* |
| `apps/migrate_demo/scene/postfx_chain.h` | 헬퍼 (선택) — 기본 5 단계 체인 빌더 free function |

### 4.2 단계별 작업

1. **Stage A — 자료구조 도입**
   - `src/render/postfx_pass.h` 추가, `SJH::PostFXPass` 정의.
   - `SceneRenderer` 에 `mPostFXChain` 멤버 + setter/clear 만 추가 (`RunPostFXChain` 은 stub).
   - 기존 PostFX-as-Camera 경로는 *유지* — 빌드 깨지지 않음.

2. **Stage B — SceneRenderer 통합**
   - `RunPostFXChain` 본구현. 단일 카메라 + 후속 직렬 PostFX 흐름 검증.
   - SceneFB → PostFX[0] → ... → backbuffer 가 *기존과 동일 골든 이미지* 산출하는지 확인.

3. **Stage C — migrate_demo 전환**
   - `BuildPostFXChain` 를 새 API 로 재작성.
   - PostFX 카메라 5 + 전용 Layer 5 + quad Actor 5 → 삭제.
   - `sceneCam->Depth = 0` 만 남김.
   - `LAYER_POSTFX_BLUR` 등 layer 매크로 제거.

4. **Stage D — Camera 정리**
   - `Camera::Depth` 주석 갱신 — "*진짜 다중 view* 의 우선순위, PostFX 단계 아님" 명시.
   - `Camera::SetTargetRenderTarget` 사용처 점검 — SceneCamera 1 개만 PostFX 입력 FB 를 가리켜야 함.

5. **Stage E — 회귀 검증**
   - `apps/_MyApp_` 골든 이미지 비교 (PostFX 미사용 → 영향 0 이어야).
   - `apps/migrate_demo` 골든 이미지 — toggle 시나리오 4 종 (전부 on / 1개씩 off).
   - 단위 테스트 — `test/` 에 `postfx_chain_order_test` 추가 (체인 등록 순서 == 실행 순서 검증).

---

## 5. UI / 디버깅 컨벤션

| ImGui 패널 | 표시 |
|---|---|
| `PostFX Chain` 섹션 | `mPostFXChain` 의 *배열 인덱스 순서* 로 행 나열 (= 실행 순서) |
| 각 행 | `[v] 0: blur  [Material: blur_mat]  [InputFB: scene]  [OutputFB: tmp0]` |
| 토글 | `Enabled` 체크박스 — 비활성 시 입력 FB 를 다음 단계 입력으로 *bypass-rewire* |
| 재정렬 | drag-and-drop 으로 vector 순서 swap (= 실행 순서 즉시 반영) |

**금기** — UI 에서 "PostFX Camera Depth" 같은 라벨 노출 (Camera.Depth 와 PostFX 순서 분리 결정 위반).

---

## 6. 미해결 / Future Work

### 6.1 토글 시 FB rewire

현재 migrate_demo `ApplyUIState` 는 비활성 단계의 input/output FB 를 매 프레임 재배선. 새 구조에서도 동일 — `RunPostFXChain` 에서 비활성 단계 만나면 *해당 단계 skip + 이전 단계의 출력을 다음 단계 입력으로 위임*.

### 6.2 Stage 분기 (Unreal BlendableLocation 정통)

§3.1 의 `PostFXStage` enum 은 *현재 LinearChain* 에서는 sortkey 로만 쓰임. HDR/LDR 경계가 분명해질 때 (e.g., 톤매핑 도입 후 bloom 은 HDR / vignette 은 LDR) Stage 별 sub-chain 분기 도입.

**현 단계**: enum 만 정의, 모든 PostFX 가 `AfterTonemap` 으로 통일. Stage 분기 알고리즘은 *bloom 도입 시점에 spec 추가*.

### 6.3 SceneCamera 외 진짜 다중 view

분할 화면 / 미니맵 도입 시:
- 진짜 카메라 N 개 각각이 *자기 SceneFB* 를 가짐.
- 각 카메라의 SceneFB → *공유 PostFX 체인* 통과 → 각자 화면 영역에 blit.
- Camera.Depth 의 진짜 의미가 비로소 발휘됨.

### 6.4 골든 이미지 임계값

PostFX 결과는 GPU vendor 별 미세 차이 (특히 blur kernel) 가능. [.claude/Graphics-Testing-Prompt.md](../../.claude/Graphics-Testing-Prompt.md) 의 FLIP threshold 기준 적용. 첫 도입 시 PostFX off 케이스부터 골든 캡쳐.

---

## 7. 참조

- **조사 출처** (Context7 2026-05-27):
  - Unity Graphics: `/unity-technologies/graphics` — URP `ScriptableRendererFeature` / HDRP `CustomPostProcessInjectionPoint`.
  - Unreal 5.7: `/websites/dev_epicgames_unreal-engine` — `APostProcessVolume.Priority` / `UMaterial.BlendableLocation` / `BlendablePriority`.
  - Godot: `/godotengine/godot-docs` — `CompositorEffect` / `compositor_set_compositor_effects(rid, Array[RID])`.
  - Cocos: `/cocos/cocos-engine` — Custom Render Pipeline + Effect YAML `phase`.
- **메모리**:
  - `camera_depth_postfx_misuse` — Camera.Depth 의 PostFX 재활용 금기.
  - `vao_ebo_thirdparty_corruption` — ScreenQuadStage 매 프레임 ebo->Bind() 재핀 (PostFX quad 에도 동일 적용).
- **선행 결정 문서**:
  - [`doc/design/2026-05-26-render-refactor-session.md`](2026-05-26-render-refactor-session.md) — render refactor 세션 메모.
  - [`doc/EngineAPI.md`](../EngineAPI.md) §3.10 — 현 SceneRenderer 동작.

---

**문서 끝**. 구현 착수 시 본 문서를 spec 으로 삼고, 결정 변경 시 본 문서를 *먼저* 갱신할 것.
