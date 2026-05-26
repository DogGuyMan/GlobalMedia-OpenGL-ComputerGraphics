# 2026-05-26 — Render Refactor Session Context

`SP5` 후속의 렌더 모듈 리팩토링 세션 정리. 다음 세션 진입점.

---

## 세션 범위

브랜치: `game/module/rendertarget` (← `game/module/sprite` ← `game/main` 기준).
대상 모듈: `SJH::render` / `SJH::material` / `SJH::program` / `SJH::scene`.
시각 회귀 대상 데모: **migrate_demo + _MyApp_** (tweeny_demo 제외 — 사용자 결정).

---

## 완료 commit 흐름

| commit | 영역 | 요약 |
|---|---|---|
| `fc4ad0a` | R1 | `Camera::SetTargetFramebuffer → SetTargetRenderTarget` — Framebuffer 직접 의존 제거. 호출처 6곳 일괄 |
| `73ec685` | R2 | `DrawCommand` 의 program/mesh/material/actor 4 필드 → `meshRenderer` 단일 |
| `7b95332` | Phase 1 | `device_context.h` + `program_uniforms.h` 책임 경계 docstring |
| `e95be90` | (사용자) | Observer cascade 제거 — `mDependentMaterials` / `OnProgramReleased` 폐기 |
| `642040b` | 복구 | Material 의 `mProgram` / `SetProgram` / `GetProgram` 복구 + `EagerBuild` 도입 (prune 방향) |
| `6c6c243` | (사용자) | `[dev] material eager delete` — EagerBuild 메서드 자체 삭제. SetProgram 본문 = raw 참조 할당만 |

---

## 채택 결정 (R = 적용됨)

### R1. RenderTarget 의존 역전
- `Camera::mTargetRT` 가 `Framebuffer*` 대신 `RenderTarget*` 보유 (port 추상)
- `Framebuffer` / `DefaultRenderTarget` / (미래) `ShadowMapTarget` 모두 동일 슬롯 수용
- `scene_renderer.cpp` 에서 `buffer/framebuffer.h` include 폐기 + `static_cast` 제거

### R2. DrawCommand SSoT
- `meshRenderer` 단일 참조로 SSoT. program/mesh/material/actor 직접 필드 폐기
- `MeshPassProcessor::SortMultiStage` / `Process` 가 람다 내부에서 meshRenderer 경유 추출
- `SceneRenderer::CollectFromActor` 의 DrawCommand 빌드 6줄 → 3줄

### R3. DeviceContext / Uniforms / Program 책임 경계 명시 (Phase 1)
**의도된 비대칭** — 같은 호출 사이트에서 3 갈래 (`rc.UseProgram` + `Uniforms::Set*` + `prog->GetLocation`) 가 일관성 부재가 아니라 *분리된 책임* 임을 docstring 으로 못박음.

| 의도 | 경로 |
|---|---|
| program 활성화 / VAO·Tex·RT 바인딩 / draw | `DeviceContext` |
| uniform 값 설정 | `SJH::Uniforms` 자유 함수 |
| uniform location 조회 | `Program::GetLocation` 직접 |

**근거 3종**:
1. **OCP** — 새 uniform 타입 추가 시 DeviceContext 헤더 변동 0
2. **진단 가시성** — `glGetUniformLocation` fallback 흐름이 facade 뒤로 가려지지 않음
3. **bound state 분리** — DeviceContext 는 *bound* 만, uniform 값은 *bound program 의 자율*

### R4. Observer 패턴 폐기 (e95be90 + 642040b 묶음)
- `Program::mDependentMaterials` / `RegisterMaterial` / `UnregisterMaterial` / `~Program cascade` 전체 삭제
- `Material::OnProgramReleased` 삭제
- **단**: `Material::mProgram` / `SetProgram` / `GetProgram` 은 *Observer 와 무관한 정상 참조* — 복구 필수 (sort key + light uniform 송신 대상 식별)
- *Lifetime 컨벤션* 으로 안전 보장: Material → Program 순서로 destroy (ResourceRegistry 가 둘 다 owner 시 자동)

### R5. *(취소됨)* — EagerBuild 도입 시도 후 폐기
- `642040b` 에 prune 방향 EagerBuild 도입 (`Properties` 의 미존재 key 를 `stderr` warn + erase)
- **이후 사용자가 메서드 자체 삭제** — 현재 `SetProgram` 본문 = `mProgram = program; return *this;` 만 (검증/동기화 없음)
- `material.h` 의 §EagerBuild docstring 섹션은 *stale* — 정리 필요 (memo: 별도 cleanup)
- 사용자가 *역방향* (cache → Properties 슬롯 채움) 도 검토했으나 **시스템 uniform 충돌 부작용** 으로 별도 취소 (X4)
- 결국 Material 은 Program 직접 참조만 보유. EagerBuild/검증 컨셉은 future SP 단위에서 재논의

### R6. Light 아키텍처 결정 — OOP edge + Data-Driven interior
Context7 4 엔진 리서치 결과:

| 엔진 | Scene/Editor | Renderer/Shader |
|---|---|---|
| Unity (URP) | Light 컴포넌트 4 종 | `GetMainLight()` 통합 struct |
| Unreal | `FLightSceneInfo` 클래스 계층 | `FDeferredLightData` 단일 struct |
| Godot | `Light3D` 클래스 계층 | `RenderingServer.LightType` enum + LightParam 배열 |
| Cocos | Light 클래스들 | forward-add phase per-light |

**4 for 4 수렴 패턴**: "OOP at the edge, Data-Driven at the GPU boundary". 양자택일 false dichotomy.

현재 프로젝트 (`DirLight` / `PointLight` / `SpotLight` 컴포넌트 + 타입별 컬렉션) 이미 정통 부합.

---

## 거부 결정 (X = 명시 거부)

### X1. DeviceContext 비대화 (Uniforms 흡수)
- 거부 근거: `glGetUniformLocation` fallback 의 *진단 가시성* 가려짐 + Uniforms 자유 함수의 OCP 손상
- 대신: R3 (책임 경계 docstring) 으로 *원칙 문서화*

### X2. RenderTarget 의 다중 의존을 *단일 의존* 으로 축소
- 사용자 우려: SceneRenderer + DeviceContext + Camera 가 모두 RT 의존 → 관심사 분리 위반?
- 평가: **정상 hexagonal pattern** — 각자 다른 책임 (선택/실행/선언) 으로 같은 port 의존. 단일화 시도는 *infrastructure 누수* 같은 더 큰 안티패턴 유발
- **MeshPassProcessor 는 RT 의존 0건** — 사용자 관찰의 사실 오류 정정

### X3. Material + Light 공통 `Apply` 인터페이스
- 표면적 공통점: 둘 다 "Program 에 상태 송신"
- 거부 근거 3종:
  - **LSP 부재** — 상호 치환되는 코드 경로 0개. Light 는 program 당 1회 / Material 은 DrawCommand 당
  - **Bounded Context** 분리 — "씬 조명" vs "표면 외형"
  - **4 엔진 정통 4 for 4 분리** — Unity/Unreal/Godot/Cocos 누구도 공통 base 없음
- Sandi Metz: *"Duplication is far cheaper than the wrong abstraction"*
- 각자 도메인 *내* 다형성 추구 — Light 는 가상 `Apply` 도입 검토 (Phase 2.5), Material 은 Visitor (PropertyBlockSetter) 유지

### X4. EagerBuild 양방향 모두 폐기
- **역방향** (cache → Properties 슬롯 채움): UniformCache 순회 → Properties 의 7 typed map 에 사전 슬롯 등록
  - 거부 근거: **시스템 uniform (uModel/uView/uProj/viewPos/dirLight/pointLights/spotLight) 도 Properties 에 들어가** → PropertyBlockSetter 가 `Properties.Mat4s["uView"] = 0` 등으로 *시스템 uniform 을 0 으로 덮어쓰기* → 화면 깨짐
  - 해결책 (`Const::UNI_*` 기반 skip-list) 설계까지 진행했으나 사용자 취소
- **순방향** (prune): `642040b` 에 도입되었으나 사용자가 메서드 삭제 — 검증 없는 상태가 현재
- 향후 schema 검증이 필요해질 때 *명시 schema 도입* (Material 생성 시점 외부 명단 주입) 같은 형태로 재논의

### X5. tweeny_demo 시각 회귀 검증
- 사용자 결정 — *무시*
- migrate_demo + _MyApp_ 만 검증 대상

---

## 미진행 SP / Phase (다음 세션 진입점)

### **Phase 2 — `LightUniformDispatcher` 분리**
- 현 `SceneRenderer::SendLightUniforms` / `CollectLights` / `CollectPrograms` 를 `src/render/light_uniform_dispatcher.{h,cpp}` 로 이주
- SceneRenderer 가 4 책임 → 2 책임 (Camera 수집/정렬 + 위임)
- Phase 1 docstring 의 *후속 코드 실현*

### **Phase 2.5 — `Light` 추상 base + 가상 `Apply(prog, slot)`**
- DirLight / PointLight / SpotLight 가 자기 송신 책임 보유
- `SceneRenderer::CollectLights` 의 타입별 컴포넌트 조회 분기 단순화
- Phase 2 와 같은 commit 묶음 가능

### **Phase 3 — `SP-ProgramRegistry`** *(Observer 후속 정리)*
3 항목 한 묶음:
1. `ResourceRegistry::CreateProgram(key, vs, fs)` + `FindProgram(key)` 추가
2. 데모 3개 — `ProgramUPtr` 멤버 제거, `reg.CreateProgram(...)` 사용, `shutdown()` 의 `reset()` 삭제
3. `Material::~Material` 의 `mProgram` lifetime 가정 *문서화 강화* (Observer 부재 보장 근거)

**왜 필요한가**: 현재 `tweeny_demo:178` 의 `mProgram.reset()` 같은 *명시 해제* 가 lifetime 컨벤션 위반. Material 보다 Program 이 먼저 죽으면 dangling. 컨벤션 정착으로 *원천 봉쇄*.

### **SP-UniversalRenderTarget** *(가장 큰 아키텍처 SP)*
사용자 철학: **"모든 렌더링 대상은 RT 를 가진다. backbuffer 는 ScreenQuadStage 만의 출력"**

4 엔진 정통 부합 (Unity RTHandle / Unreal SceneRenderTarget / Godot Viewport / Cocos 동일).

Phase 분해:
- **A. ScreenQuadStage 신설** — N 개 FBO 입력 → backbuffer 합성
- **B. Camera 강제 non-null + SceneRenderer fallback 제거** — `SetTargetRenderTarget(nullptr)` assert
- **C. Overlay 컨셉 정리** — Camera.Depth = ScreenQuadStage 합성 순서로 격하

---

## 시각 회귀 대기

`6c6c243` (HEAD) 후 다음 2 데모 사용자 검증 필요:
```bash
cd build_ninja/apps/migrate_demo && ./migrate_demo
cd build_ninja/apps/_MyApp_     && ./_MyApp_
```

EagerBuild 가 제거되어 *추가 stderr 출력 없음*. 시각 동일 + 콘솔 warn 0 건 = OK.

---

## 참조

- `clean-ddd-hexagonal` 스킬 (`.claude/skills/clean-ddd-hexagonal/SKILL.md`) — 본 세션의 평가 기준
- Context7 MCP — 4 엔진 Light/Material/RT API 리서치 출처
- `.claude/CLAUDE.md` §자원 보유 컨벤션 — SP-ProgramRegistry 의 정합성 근거
- `src/material/material.h` — EagerBuild + Lifetime 컨벤션 docstring
- `src/render/device_context.h` — pipeline state facade 책임 경계
- `src/program/program_uniforms.h` — shader uniform state 책임 경계
