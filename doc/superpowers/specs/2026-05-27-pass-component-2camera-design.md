# PassComponent + 2-Camera Architecture 설계 스펙

**날짜:** 2026-05-27  
**상태:** 설계 확정 — 구현 플랜 미작성  
**브랜치 참조:** `game/module/well_drawexample`

---

## 1. 동기 (Why)

### 현재 안티패턴

`PostFXPass` 는 POD struct 이고, `SceneRenderer` 가 `SetPostFXChain` / `RunPostFXChain` 으로 체인 로직을 외부에서 소유.

clean-ddd-hexagonal 스킬 기준:
- **Anemic Domain Model**: PostFXPass = data bag, SceneRenderer 가 행동 소유 ❌
- **God Service**: SceneRenderer 가 카메라 렌더 + PostFX 체인 + 수집 모두 담당 ❌

### 핵심 인사이트 (2026-05-27 설계 세션)

> **PostFXPass 는 MeshRenderer 와 본질적으로 같다 — 단지 "화면 공간의 MeshRenderer" 일 뿐이다.**

| | MeshRenderer | PassComponent |
|--|---|---|
| Geometry | Actor에 속한 Mesh | Camera 앞 ScreenQuad |
| Material | 3D 셰이더 | 화면 처리 셰이더 |
| Transform | Actor 월드 매트릭스 | identity (화면 공간) |
| Output | Camera RT | 자신의 OutputFB |
| 실행 순서 | QueueOffset 0~n | QueueOffset = MAX (마지막) |
| 소유 Actor | World Actor | Camera Child Actor |

**차이는 좌표계와 QueueOffset 뿐 — 구조적으로 동일한 Component.**

---

## 2. 확정 결정 (7종)

### 결정 1 — PassComponent: InputFB/OutputFB 포인터 공유 파이프라이닝

```cpp
class PassComponent : public Component {
public:
    PassComponent(Framebuffer* const inputFB,
                  Framebuffer* const outputFB,
                  Material* mat)
        : InputFB(inputFB), OutputFB(outputFB), mMaterial(mat) {}

    Framebuffer* const InputFB;    // readonly — 이전 패스의 OutputFB 와 포인터 공유
    Framebuffer* const OutputFB;   // writable — 다음 패스의 InputFB 와 포인터 공유
    Material*          mMaterial;
    bool               Enabled     = true;
    int                QueueOffset = 9000;  // 월드 지오메트리 이후
};
```

**포인터 공유 = 파이프라인 경계 정의**: `Pass1.OutputFB == Pass2.InputFB` (같은 포인터).
OpenGL 피드백 루프 없음 보장: 읽는 FB ≠ 쓰는 FB 항상.

### 결정 2 — Camera Child Actor 배치

```
CameraActor
  ├── Camera (Component)
  └── PassActor_Blur (Child Actor)
        └── PassComponent(InputFB=SceneFB, OutputFB=PassFB1, blurMat)
  └── PassActor_Gamma (Child Actor)
        └── PassComponent(InputFB=PassFB1, OutputFB=PassFB2, gammaMat)
```

PassComponent 는 Camera Actor 의 Child Actor 에 붙음.
`MeshRenderer` 와 동일한 Actor-Component 패턴으로 트리에 귀속.

### 결정 3 — QueueOffset = 전역 렌더 순서 + activeFB 동적 전환

별도 후처리 타이밍 없음. `MeshPassProcessor` 의 Queue 정렬로 처리.

**activeFB 동적 전환 모델** (2026-05-27 확정):
- `MeshPassProcessor::Process()` 가 `activeFB` 포인터를 추적
- PassComponent 발화(ScreenQuad DrawCommand) → `rc.BeginFrame(*outputFB)` + `activeFB = outputFB`
- 이후 MeshRenderer DrawCommand 는 현재 `activeFB` 에 렌더 (자동으로 올바른 FB)

```
QueueOffset 0~8999:  MeshRenderer → activeFB(초기 Camera RT)     — raw, no-FX
QueueOffset 9000:    PassComponent_Blur 발화 → activeFB = PassFB1
QueueOffset 9500:    MeshRenderer → activeFB(PassFB1)             — blur 위에 합성, gamma 적용
QueueOffset 9001:    PassComponent_Gamma 발화 → activeFB = PassFB2
QueueOffset 9999:    MeshRenderer → activeFB(PassFB2)             — 모든 pass 이후, raw
```

**DrawCommand 확장** — PassComponent 지원:
```cpp
struct DrawCommand {
    // 기존 필드 유지
    enum class Kind { WorldMesh, ScreenQuad };
    Kind         kind     = Kind::WorldMesh;
    Framebuffer* inputFB  = nullptr;  // PassComponent: 읽기 소스 (uScene 바인딩)
    Framebuffer* outputFB = nullptr;  // PassComponent: 쓰기 대상 + activeFB 갱신
};
```

- 기본 HUD QueueOffset = 0 (낮을수록 먼저 = raw layer)
- 개발자가 수동으로 높은 QueueOffset 설정 → 원하는 pass 이후 FB 에 합성
- Fixed 상수 아님 — 런타임 동적 조절 가능

### 결정 4 — SetPostFXChain API 폐기

PassComponent 가 자신의 InputFB/OutputFB 를 관리 → `SceneRenderer::SetPostFXChain` / `RunPostFXChain` / `ClearPostFXChain` / `GetActiveFXOutput` 모두 폐기.

### 결정 5 — 2-Camera 분리 (Perspective World + Orthographic Screen)

```
World Camera (Perspective)       Screen Camera (Orthographic)
─────────────────────────        ──────────────────────────────
3D 월드 렌더 → SceneFB            초기 activeFB = sceneFB (공유)
                                  HUD MeshRenderer → sceneFB (Blur 이전, QueueOffset 0)
                                  Pass 체인 수행 (Blur 등 → passFBs)
                                  출력: lastPassFB (단일 경로)
                                         ↓
                                  ScreenQuadStage → backbuffer
```

**HUD → SceneFB 통합 (2026-05-27 확정)**:
- Screen Camera 의 초기 `activeFB = sceneFB` (World Camera 출력 공유)
- HUD MeshRenderer (QueueOffset 0) → sceneFB 에 직접 합성
- Blur (QueueOffset 9000) 가 sceneFB(HUD 포함) → passFB1 로 블러
- 별도 `screenFB` 없음 — ScreenQuadStage 단일 소스

### 결정 6 — SceneFB 포인터 = 두 Camera 간 계약

```cpp
auto* sceneFB = reg.CreateFramebuffer(w, h);
worldCamera->SetTargetRenderTarget(sceneFB);  // World Camera 가 씀

// Screen Camera 의 첫 Pass 가 sceneFB 를 읽음
screenPassActor->AddComponent<PassComponent>(sceneFB, passFB1, blurMat);
// sceneFB 는 공유 포인터 — World Camera 출력 = Screen Camera 입력
```

Cross-Camera 의존 = ID 가 아닌 FB 포인터 계약.

### 결정 7 — ScreenQuadStage 단일 소스

```cpp
// 최종 출력: PostFX 체인 마지막 FB (HUD 는 이미 sceneFB 에 합성됨)
mScreenQuadStage->SetSources({lastPassFB});
```

HUD 가 sceneFB 에 통합되어 Pass 체인을 함께 통과하므로 `screenFB` 별도 합성 불필요.
`SetSources(vector<Framebuffer*>)` API 는 유지 (다른 용도 재사용 가능).

---

## 3. DDD 평가 요약

| 스킬 기준 | 평가 |
|---|---|
| **Dependency Rule** | PassComponent (Domain Entity) ← SceneRenderer (Application) ← DeviceContext (Infra) ✅ |
| **Entity vs VO** | PassComponent = Entity (Actor 귀속, 정체성 있음) ✅ |
| **Aggregate Boundary** | Camera Aggregate Root + PassComponent/MeshRenderer Entities — 한 프레임 = 하나의 트랜잭션 ✅ |
| **Anemic Domain Model** | 행동을 Component 안으로 이동 (현재 안티패턴 해소) ✅ |
| **God Service** | SceneRenderer 는 수집만 담당, 체인 로직은 Component 소유 ✅ |

**4/4 통과 (현재보다 더 정통에 가까운 모델).**

---

## 4. 폐기 목록

| 폐기 | 대체 |
|---|---|
| `PostFXPass` struct | `PassComponent` (Component) |
| `PostFXStage` enum | QueueOffset 숫자로 흡수 |
| `SceneRenderer::SetPostFXChain` | PassComponent 자체 관리 |
| `SceneRenderer::RunPostFXChain` | MeshPassProcessor DrawCommand 큐 처리 |
| `SceneRenderer::ClearPostFXChain` | PassComponent Actor 제거 |
| `SceneRenderer::GetActiveFXOutput` | ScreenQuadStage 가 LastPassFB 직접 참조 |

---

## 5. 변경 영향 범위 (예비 분석)

### 신규 파일
- `src/render/pass_component.h/.cpp` — PassComponent (InputFB/OutputFB/Material/QueueOffset)
- `src/scene/camera.h` 수정 — Orthographic projection 지원 (`IsOrthographic` flag + `OrthoSize`)

### 수정 파일
- `src/render/mesh_pass_processor.h/.cpp` — DrawCommand 에 `overrideTargetFB` + `inputFB` 필드 추가. PassComponent 처리 분기.
- `src/render/scene_renderer.h/.cpp` — PostFX 관련 API 전체 폐기. PassComponent 수집 추가.
- `apps/_MyApp_/main.cpp` — 2-Camera 셋업 + PassComponent 체인 구성

### 현재 사용 중인 셰이더
- `resources/shader/postprocess/*.fs` — 유지 (Material 에서 동일하게 참조)
- `resources/shaders/passthrough.vs` — 유지 (ScreenQuadStage 용)

---

## 6. 확정 결정 (2026-05-27 질의응답)

1. **ScreenQuad Mesh 소유**: `ResourceRegistry` 공유 — `reg.FindMesh("mesh_screen_quad")`.
   PassComponent 는 Mesh 포인터만 보유, startup() 에 이미 등록된 것 재사용.

2. **DrawCommand overrideTargetFB**: 플랜 작성 시 결정.
   PassComponent DrawCommand 는 QueueOffset=9000+ → 자연 분리. BeginFrame 전환 시점은 MeshPassProcessor 구현에서 확정.

3. **CullingMask**: UI Layer 도입.
   ```
   enum class Layer : uint64_t { World = 1<<0, UI = 1<<1, Screen = 1<<2 };
   WorldCamera.CullingMask  = Layer::World
   ScreenCamera.CullingMask = Layer::UI | Layer::Screen
   ```

4. **Orthographic Size 자동 갱신**: Camera Component 내부에서 `IsOrthographic` 시 `fbW/fbH` 로 자동 세팅.
   Perspective 의 Aspect 갱신 패턴과 동일한 코드 경로.

---

## 7. 다음 단계

`doc/superpowers/plans/2026-05-27-pass-component-impl.md` 작성
— writing-plans 스킬 적용 (Task 단위 분해, 빌드 검증 포함)
