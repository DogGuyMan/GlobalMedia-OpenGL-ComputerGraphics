# RenderTarget 최적화 — 세션 논의 정리

> ⚠ 시점 문서 (archival) — 코드 경로는 작성 당시 기준. 소멸/이동 경로는 `<세그먼트>` placeholder 표기.

> **작성일**: 2026-05-24
> **시작점**: [<apps>/migrate_demo/main.cpp:281](../../<apps>/migrate_demo/main.cpp#L281) 의 `mDefaultTarget = std::make_unique<DefaultRenderTarget>(w, h)` 재생성 패턴 검토
> **진행 결과**: `apps/tweeny_demo` 의 SceneRenderer 마이그레이션 (Phase 0~4) 완료
> **미해결**: 3 개 Future SP 후보 — SP-RenderStage / SP-FramebufferResize / SP-PerRendererProperties

```
1. N×M 조합 폭발 회피 (가장 큰 동기)
현재 추상:

RenderTarget — 어디에 그리나 (Default backbuffer / FBO / ...)
??? — 무엇을 그리나 (Scene / ImGui / Skybox / DebugDraw / PostFX)
만약 "무엇을" 책임을 RenderTarget 에 박으면:

DefaultSceneRT, DefaultUiRT, FboSceneRT, FboUiRT, FboSkyboxRT, ... → N × M 조합 마다 클래스 폭발
직교 축으로 분리해야 N + M 으로 끝남:


RenderTarget × IRenderStage  =  자유 조합 (덧셈)
```

---

## 0. 컨텍스트

본 문서는 *DefaultRenderTarget 의 매 resize 마다 재생성* 이라는 미세 의문에서 출발해, **RenderTarget 추상의 한계 + ImGui/UI 통합 + Camera-less 렌더링 + MaterialPropertyBlock 확장 + Tweeny 시퀀스 표현** 까지 가지를 친 세션의 *리팩토링 필요성 제기* 를 *논의 순서대로* 보존한다.

전체 항목 중 **3 가지는 즉시 정착** ([apps/tweeny_demo](../../apps/tweeny_demo) 마이그레이션 + 엔진 PropertyBlock Vec2 확장), **5 가지는 결정만 + 후속 SP 분리** 가 정통 경로. *마이그레이션 완료 후 다음 작업 진입 시 체크리스트* 역할.

---

## 1. 논의 순서별 항목 (10 종)

### ① DefaultRenderTarget 재생성 vs 재활용

**제기**: [<apps>/migrate_demo/main.cpp:281](../../<apps>/migrate_demo/main.cpp#L281) — 매 resize 마다 `unique_ptr` 교체.

**관찰**:
- `DefaultRenderTarget` 은 `int mWidth, mHeight` 2 개 + FBO 0 wrapper 뿐. 실제 GL 자원 0 → 재생성 비용 0.
- 하지만 *raw pointer 보유자* (예: [src/scene/camera.h:82-86](../../src/scene/camera.h#L82-L86) 의 `Framebuffer*`) 입장에선 rewire 코드가 강제됨 ([migrate_demo:294-304](../../<apps>/migrate_demo/main.cpp#L294-L304)).
- `glTexStorage2D` immutable storage 사용 시 재생성이 강제.

**결정**:
- `DefaultRenderTarget` — 어느 쪽이든 무방 (보유자 0).
- `Framebuffer` — **`Resize(w, h)` 추가 권장**. Unity `RTHandle` / DX11 `ResizeBuffers` 정통. Camera `Framebuffer*` raw 보유자가 영원히 안전.

**상태**: 🟡 **결정만 — 미구현**. Future **SP-FramebufferResize**.

---

### ② 재생성/재활용 정책 — 팩토리별 분리

**제기**: 사용자 — "타입마다 다른 정책 (RT 는 재생성, FB 는 재활용)" 을 *팩토리에 박기*.

**3 형태 비교**:

| 형태 | 호출자 균일성 | 정책 가시성 | LSP 위험 |
|---|---|---|---|
| **A**: 팩토리에 정책 (`Create + Resize 부재 vs Create + Resize`) | ❌ 비대칭 | ✅ 명시 | ✅ 안전 |
| **B**: `EnsureSize` free function 오버로드 | ✅ 균일 | ⚠️ 함수 안 | ✅ 안전 |
| **C**: virtual `RenderTarget::Resize(w,h)` | ✅ 균일 | ❌ 디스패치 뒤 | 🚨 **위험** |

**함정 — C 의 조용한 LSP 위반**: 미래 `MSAAFramebuffer` 가 `glTexStorage2DMultisample` (immutable) 쓰면 텍스처 핸들 바뀜 → 호출자가 *구체 모름* + Material `uScene` 캐시 깨짐 → 디버깅 지옥.

**결정**: **A + B 하이브리드**. "멤버 함수 존재 자체가 정책 선언" (DefaultRT 에 `Resize` *없음*, FB 에 `Resize` *있음*) + `EnsureSize` free function 으로 호출자 균일화.

**상태**: 🟡 **결정만 — 미구현**. ① 과 같은 SP.

---

### ③ UI/ImGui 통합 — 추상 레이어 redirect

**제기**: 사용자 — "Camera 없이 FullRect 렌더링 되는 *UI 용 RenderTarget*" 으로 ImGui 통합.

**REDIRECT**: RenderTarget 이 아니라 **`RenderPass`/`Renderer`** 가 정답 자리.

| 추상 | 책임 | "무엇/어디" |
|---|---|---|
| `RenderTarget` | `glBindFramebuffer` + `glViewport` | **어디에** 그리나 |
| `IRenderPass::Render(RenderTarget&)` | 카메라/씬/메시 → 드로우 콜 | **무엇을** 그리나 |

RT 와 Pass 는 *직교 축*. RT 에 ImGui 책임 박으면 N×M 조합 폭발 (Default+UI, Default+Scene, FBO+UI, FBO+Scene, ...).

**3 옵션 비교**: ① `IRenderPass` + `ImGuiPass` / ② ImGui backend 자체 Material/Mesh 재작성 / ③ Overlay Camera + UI Layer.

**결정**: **`IRenderPass` 패턴** — `SceneRenderer`/`ImGuiPass`/`DebugDrawPass`/`SkyboxPass`/`TonemapPass` 통일. Unity `ScriptableRendererFeature` / Unreal `FSceneRenderer` 정통.

**상태**: 🟡 **결정만 — 미구현**. Future **SP-RenderStage** (이름은 ④ 참조).

---

### ④ 명명 충돌 — `Pass` 단어 점유 → `Stage`

**제기**: ③ 의 `IRenderPass` 가 기존 `Material::Pass::Kind` (Opaque/Transparent/Skybox Queue Layer — [src/material/pass.h](../../src/material/pass.h)) 와 *같은 단어 점유*.

기존 의미:
- `Pass::Kind` — 한 SceneRenderer *내부* 의 Queue Layer 분류 (Opaque 2000 / Transparent 3000 등)
- `MeshPassProcessor` — Kind 별 sort + draw

새로 추가하려는 의미:
- `IRenderPass` — *최상위 렌더 단계* (SceneRenderer + ImGuiPass + ...)

→ **같은 단어 두 레이어** = 미래 본인 혼동.

**결정**: **`IRenderStage`** 로 재명명. Unreal `FSceneRenderer` *stage* 어휘 정통. 기존 `Pass::Kind` 는 그대로 유지 (Filament/Unreal Pass 정통).

**상태**: 🟡 **결정만 — 미구현**. ③ 과 동일 SP 흡수.

---

### ⑤ Camera-less SceneRenderer 사용 — tweeny_demo 마이그레이션 진입점

**제기**: tweeny_demo 의 manual GL draw 6 줄을 SceneRenderer 위임으로 옮기고 싶음.

**2 질문**:
- Q1: Camera 없이 `mDefaultTarget` 사용 가능? → **YES** (DefaultRT 는 Camera 무관).
- Q2: SceneRenderer 가 RenderTarget 강한 의존? → 시그니처상 ✅, 하지만 **Camera 우회 overload 존재** ([<src>/render/scene_renderer.h:36-38](../../<src>/render/scene_renderer.h#L36-L38)):
  ```cpp
  /// @brief 명시 view/proj — 단위 테스트 + 디버그용 (CameraComponent 우회).
  void Render(RenderTarget& defaultTarget,
              const glm::mat4& viewMat, const glm::mat4& projMat);
  ```

**2 옵션**:
- **A (임시)**: 우회 overload + identity view/proj — 변경 최소. doxygen 이 "단위 테스트/디버그용" 으로 limit.
- **B (정통)**: Overlay Camera Actor — `migrate_demo` PostFX 패턴과 통일.

**결정**: **임시로 A 채택** (tweeny_demo Phase 4 의 TODO 주석으로 B 승격 표식).

**상태**: ✅ **A 구현 완료** (Phase 0~4 — §3 참조). B 승격은 SP-RenderStage 의 일.

---

### ⑥ `MaterialPropertyBlock` Vec2 미지원 → 사용자가 엔진 확장

**제기**: tweeny_demo 셰이더의 `uniform vec2 uOffset/uScale` 을 PropertyBlock 에 못 담음. 기존 5 종만 지원 (Vec3/Vec4/Mat4/Float/Int/Texture).

**최초 제안**: 셰이더 vec2 → vec4 packing 으로 우회.

**사용자가 더 좋은 해결**: **엔진 자체 확장** — 3 레이어 일관 추가:
- [src/material/material_property_block.h:49](../../src/material/material_property_block.h#L49) — `std::unordered_map<std::string, glm::vec2> Vec2s;`
- [src/material/material_uniforms.cpp:20-23](../../src/material/material_uniforms.cpp#L20-L23) — `SetVec2(Material&, ...)` store-only setter
- [<src>/render/property_block_setter.cpp:45-50](../../<src>/render/property_block_setter.cpp#L45-L50) — `GL_FLOAT_VEC2` dispatch (Cache outer + Block inner)

**결과**: tweeny_demo 셰이더 그대로 유지 가능. *Unity 정통 typed map dispatch 패턴* 의 빈 칸 메움.

**상태**: ✅ **해결됨 — 엔진 자체에 정착**.

---

### ⑦ `MeshRenderer` per-instance PropertyBlock 부재

**제기**: 사용자 — "1 SharedMaterial + N PropertyBlock" (Unity `Renderer.SetPropertyBlock` 정통).

**발견**: 현재 `MeshRenderer` 는 `Material*` 만 보유, *per-renderer PropertyBlock 슬롯 없음* ([src/render/mesh_renderer.h:59-62](../../src/render/mesh_renderer.h#L59-L62)) — *의도된 부재*. PropertyBlockSetter 도 *Material 의 block* 만 송신.

```cpp
// 현재 MeshRenderer
SJH::Mesh *const Mesh = nullptr;
SJH::Material *const Material = nullptr;
bool Visible = true;
int QueueOffset = 0;
// ← per-instance PropertyBlock 슬롯 없음
```

**엔진 정통 우회**: **`CreateMaterialInstanceFrom(key, template)`** — template + Clone N 개. Unreal `UMaterialInstanceDynamic` 정통. 사용자 의도와 *기능 동등* (메모리만 N 인스턴스).

**결정**: tweeny_demo 는 Clone 패턴 사용. 진짜 "1 Material + N PropertyBlock" 모델이 필요해지면 **별도 SP** (엔진 3-3 모듈 확장: `MeshRenderer.Properties` 추가 + `PropertyBlockSetter::Set` 가 *Material block 다음에 MeshRenderer block* 송신 (덮어쓰기 우선)).

**상태**: ✅ **우회로 해결** (Clone). Future **SP-PerRendererProperties** 후보 (선택).

---

### ⑧ Tweeny 외부 ping-pong state → tween 내부 sequence

**제기**: 사용자 — "`forward` bool + `step(±dtMs)` 분기 + `progress()` 양 끝 가드 4 줄을 tween 내부로 흡수".

**해결**: **Tweeny multi-stage chain** —
```cpp
row.tween = tweeny::from(kTweenFromX)
                .to(kTweenToX).during(kTweenDurationMs).via(easing)   // 출발
                .to(kTweenFromX).during(kTweenDurationMs).via(easing); // 복귀
```

render() 에서는 단일 `step(dtMs)` + `if (progress() >= 1.0f) seek(0)` 로 자동 loop.

**부수 효과**:
- `EasingRow::forward` 필드 *완전 제거* — 외부 state 흡수.
- ping-pong 의미가 *데이터 (tween 구조)* 로 표현.
- 대칭 easing (`*InOut`) 한정으로 시각 동등. Asymmetric easing 은 미세 차이 가능.

**상태**: ✅ **구현 완료** ([<apps>/tweeny_demo/main.cpp:90-96](../../<apps>/tweeny_demo/main.cpp#L90-L96)).

---

### ⑨ lerp 수식 표기 정리 (사소)

**제기**: 사용자 — `row.y = 0.9f + (-0.9f - 0.9f) * t;` 가 *뺄셈처럼 읽힘*. 수식은 정답이지만 `(start, -start)` 부호 혼동.

**해결**: `(1 - t) * a + t * b` 정통 lerp 형태:
```cpp
row.y = (1.0f - t) * kRowTopY + t * kRowBottomY;
```

**상태**: ✅ **적용 완료** ([<apps>/tweeny_demo/main.cpp:127](../../<apps>/tweeny_demo/main.cpp#L127)).

---

### ⑩ Phase 2 Instance key 충돌 버그

**제기**: 검토 중 발견 — `CreateMaterialInstanceFrom("tweeny_row", tsm)` 가 *고정 키* 사용 → 11 row 동일 키 충돌.

**증상 예측** (수정 전): 11 row 모두 같은 색 OR nullptr deref segfault. ResourceRegistry 의 `CreateMaterialInstanceFrom` 가 *중복 키 거부* 동작 가정.

**해결**: `key` 변수 (`"tweeny_row_" + name`) 로 교체 + template 키도 `"tweeny_template"` 으로 prefix 분리.

**상태**: ✅ **수정 완료** ([<apps>/tweeny_demo/main.cpp:130](../../<apps>/tweeny_demo/main.cpp#L130)).

---

## 2. 분류별 정리표

| # | 주제 | 분류 | 정착 위치 / 후속 |
|---|---|---|---|
| ① | DefaultRT 재생성/재활용 | 🟡 결정만 | SP-FramebufferResize |
| ② | 정책별 팩토리 분리 | 🟡 결정만 | SP-FramebufferResize (① 과 묶음) |
| ③ | UI/ImGui 통합 추상 | 🟡 결정만 | SP-RenderStage |
| ④ | `Pass` → `Stage` 명명 | 🟡 결정만 | SP-RenderStage (③ 과 묶음) |
| ⑤ | Camera-less SceneRenderer | ✅ 임시 (옵션 A) | tweeny_demo Phase 0~4 / B 승격은 SP-RenderStage |
| ⑥ | PropertyBlock Vec2 | ✅ 엔진 정착 | 3 레이어 확장 완료 |
| ⑦ | per-instance PropertyBlock | ✅ Clone 우회 | (선택) SP-PerRendererProperties |
| ⑧ | Tweeny multi-stage | ✅ 정착 | tweeny_demo |
| ⑨ | lerp 표기 | ✅ 정착 | tweeny_demo |
| ⑩ | Instance key 충돌 | ✅ 수정 | tweeny_demo |

---

## 3. tweeny_demo 마이그레이션 결과 (Phase 0~4)

옵션 ⑤ A 의 실증 구현. *수동 GL → SceneRenderer 위임* 의 stepping stone.

### Phase 0 — 깨진 빌드 복구
- `mCameraComponenetRPtr` (미선언 멤버) / `SetTargetFramebuffer()` (인자 누락) 두 컴파일 에러 제거.
- 미사용 `mSceneFB` 멤버 + 관련 include 3 종 (`scene/actor.h`, `scene/scene.h`, `<buffer>/framebuffer.h`) 정리.
- `onResize` 를 *최소형* 으로 (Retina HiDPI 변환 + `mDefaultTarget` 갱신만).
- **검증**: 빌드 OK + 시각 기존 동일.

### Phase 2 — Material + Actor + MeshRenderer 셋업 (dormant scaffolding)
- include 5 종 추가 (`material/material_uniforms.h`, `render/mesh_renderer.h`, `resource_registry/resource_registry.h`, `scene/actor.h`, `scene/scene.h`).
- `EasingRow::material` 필드 추가 (`SJH::Material*` 비소유 관찰자).
- Template Material `"tweeny_template"` 1 개 + 11 row 각각 `CreateMaterialInstanceFrom(key, tsm)` Clone.
- 불변값 (`uScale`, `baseColor`) 을 Material PropertyBlock 에 startup 1 회 저장.
- Actor + MeshRenderer 컴포넌트 → 씬 트리.
- `Director::Enter()` 호출.
- **이 phase 만으론 시각 변화 0** — render() 는 여전히 manual GL.
- **검증**: 빌드 OK + 시각 동일.

### Phase 3 — render() body 교체 (manual GL → SceneRenderer)
- `glClearColor`/`glClear`/`glUseProgram`/`glBindVertexArray`/`glDrawElements` *모두 삭제*.
- 가변값 `uOffset` 만 매 프레임 `Uniforms::SetVec2(*row.material, "uOffset", ...)` (Material 변종 오버로드).
- 1 줄 `mRenderSys.Render(*mDefaultTarget, I, I)` — Camera 우회 overload + identity view/proj.
- **검증**: 빌드 OK + 시각 동일 + spdlog warn 허용 (`model`/`view`/`proj` 누락 — 셰이더 미사용, warn-once).

### Phase 3 보너스 — Tweeny multi-stage (⑧)
- `makeRow` 람다에 `.to(kTweenFromX).during().via()` 복귀 stage 추가.
- `EasingRow::forward` 필드 제거.
- render 에서 `step + seek(0)` 자동 loop.

### Phase 4 — 인계 주석
- `mRenderSys.Render(...)` 호출 위에 `// TODO(옵션 B): Overlay Camera 승격 표식` 주석.
- 미래 자기 자신에게 "이 데모는 임시 형태" 명시.

---

## 4. Future SP 후보 — 3 개

각 SP 는 [.claude/architecture-design-agent.md §3](../../.claude/architecture-design-agent.md) 의 *4-phase chain (brainstorming → spec → plan → impl)* 을 따라 진행.

| SP 가칭 | 흡수 항목 | 우선순위 | Trigger |
|---|---|---|---|
| **SP-RenderStage** | ③ ④ ⑤(B 부분) | 🔥 **높음** | ImGui/PostFX/Skybox/Debug 의 미래 통일에 *seam* 역할. `migrate_demo` 가 이미 PostFX chain 보유 → 정통 기반 존재. tweeny_demo 의 옵션 A → B 승격이 이 SP 의 가시적 산출. |
| **SP-FramebufferResize** | ① ② | 🟡 중간 | *현재 migrate_demo 의 rewire 통증* 이 명백한 trigger. Camera `Framebuffer*` raw 보유자가 영원히 안전해짐. `migrate_demo:294-304` rewire 루프 *삭제 가능*. |
| **SP-PerRendererProperties** | ⑦ | 🟢 낮음 (선택) | Clone 패턴이 기능 동등 우회. *Unity 1:N 패턴이 진짜 필요* 해질 때 진입. 데이터 중복(인스턴스 N 개)이 메모리 압박 되는 시점이 trigger. |

### SP-RenderStage 의 예상 산출

```cpp
// <src>/render/render_stage.h (가칭)
namespace SJH
{
    class IRenderStage {
    public:
        virtual ~IRenderStage() = default;
        virtual void Render(RenderTarget& target) = 0;
    };
}

// SceneRenderer 가 자연스럽게 상속
class SceneRenderer : public IRenderStage { ... };

// 새 stages
class ImGuiStage    : public IRenderStage { ... };
class SkyboxStage   : public IRenderStage { ... };
class DebugDrawStage : public IRenderStage { ... };

// App 사용
std::vector<std::unique_ptr<IRenderStage>> mStages;
for (auto& s : mStages) s->Render(*mDefaultTarget);
```

### SP-FramebufferResize 의 예상 산출

```cpp
// src/<buffer>/framebuffer.h 확장
class Framebuffer : public RenderTarget {
public:
    bool Resize(int w, int h);   // FBO 핸들 + Texture shared_ptr 동일성 유지
};

// DefaultRenderTarget 은 의도적으로 Resize 없음 — 정책 선언

// 호출자 (free function)
namespace SJH::RT {
    void EnsureSize(DefaultRenderTargetUPtr& s, int w, int h);  // 재생성
    void EnsureSize(FramebufferUPtr& s, int w, int h);          // 재활용
}

// migrate_demo:281-313 의 30 줄이 3 줄로:
SJH::RT::EnsureSize(mDefaultTarget, w, h);
SJH::RT::EnsureSize(mSceneFB, w, h);
for (auto& p : mPostFX) SJH::RT::EnsureSize(p.OutputFB, w, h);
// rewire 루프 삭제 — Framebuffer 포인터 안정
```

### SP-PerRendererProperties 의 예상 산출

```cpp
// src/render/mesh_renderer.h 확장
class MeshRenderer : public Component {
public:
    // ... 기존 멤버 ...
    MaterialPropertyBlock Properties;   // ← per-renderer override
};

// PropertyBlockSetter::Set 호출 순서:
//   1. Material 의 block 송신 (공통값)
//   2. MeshRenderer 의 block 송신 (override — 덮어쓰기)
```

---

## 5. 참고

### 관련 파일
- 시작점: [<apps>/migrate_demo/main.cpp](../../<apps>/migrate_demo/main.cpp) — RenderTarget 재생성 패턴 (rewire 루프 포함)
- 실증 구현: [<apps>/tweeny_demo/main.cpp](../../<apps>/tweeny_demo/main.cpp) — 옵션 A 마이그레이션 완료
- 엔진 확장: [src/material/material_property_block.h](../../src/material/material_property_block.h), [src/material/material_uniforms.cpp](../../src/material/material_uniforms.cpp), [<src>/render/property_block_setter.cpp](../../<src>/render/property_block_setter.cpp) — Vec2 추가
- 추상 위치: [<src>/render/scene_renderer.h](../../<src>/render/scene_renderer.h), [<src>/render/render_target.h](../../<src>/render/render_target.h), [src/render/mesh_renderer.h](../../src/render/mesh_renderer.h)

### 관련 결정 문서
- [.claude/architecture.md §11.2](../../.claude/architecture.md) — lifetime ownership 분리 (Material 비소유 관찰자)
- [.claude/architecture.md §11.3](../../.claude/architecture.md) — 자원 보유 컨벤션 (ResourceRegistry 위탁)
- [.claude/architecture.md §11.5](../../.claude/architecture.md) — 진실의 원천 단일화 (Pass.Kind)
- [.claude/architecture.md §11.6](../../.claude/architecture.md) — Retina HiDPI 패턴 (sb7 우회)
- [.claude/architecture-design-agent.md §3](../../.claude/architecture-design-agent.md) — 4-phase chain (SP 진입 시 따를 워크플로우)
- [doc/api/EngineAPI.md](../api/EngineAPI.md) — 현재 코어 API 레퍼런스
- [doc/design/EngineDesign.md](../../doc/design/EngineDesign.md) — SP4 (RenderTarget/PostFX) 미착수 상태

### 정통 매핑 (외부 엔진)

| 우리 결정 | Unity | Unreal | Cocos / Filament |
|---|---|---|---|
| `IRenderStage` (③④) | `ScriptableRendererFeature` | `FSceneRenderer` | (N/A) / `Renderer` |
| `Framebuffer::Resize` (①) | `RTHandle` | `FRHITexture::Resize` | DX11 `ResizeBuffers` |
| `EnsureSize` 정책 분리 (②) | `RTHandleSystem` | RHI dirty 추적 | (N/A) |
| `CreateMaterialInstanceFrom` (⑦) | `Material` (auto-Clone) | `UMaterialInstanceDynamic` | (N/A) |
| Tweeny multi-stage (⑧) | `Timeline.Append` | `UTimelineComponent` | (N/A) |

---

**갱신**: 2026-05-24 — tweeny_demo Phase 0~4 완료 + 3 SP 후보 식별 시점.
