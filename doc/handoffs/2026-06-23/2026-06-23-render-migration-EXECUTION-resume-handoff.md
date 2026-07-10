# Resume Handoff — 렌더 마이그레이션 *구현(EXECUTION)* 진행 중: Phase 1~2 완료 → Phase 3 직전

> 작성 2026-06-23. 이 문서 하나로 다른 Claude 가 *맥락 0* 에서 구현을 이어받게 자기완결.
> **설계 정본 = [`doc/마이그레이팅계획안.md`](../마이그레이팅계획안.md) (v5, D1~D13)**.
> **구현 plan 정본 = [`doc/superpowers/plans/2026-06-23-render-migration-implementation-v5-reviewed.md`](../superpowers/plans/2026-06-23-render-migration-implementation-v5-reviewed.md)** (Task 단위 + Execution Status 배너).
> (설계 단계 핸드오프 [`2026-06-23-rendering-migration-design-resume-handoff.md`] 는 *설계* 재개용 — 본 문서는 그 다음 단계인 *구현* 재개용.)

---

## 0. ⚠️ 먼저 알아야 할 4가지

1. **`doc/` 전체 gitignored** (`.gitignore:10`). 설계/plan/핸드오프 문서는 *로컬 전용*(git 전달 안 됨). 코드 `src/`/`apps/` 는 정상 추적.
2. **빌드/커밋 = 사용자.** 에이전트는 구현 + 보고만. 커밋 **path-scoped**(`git commit <경로>`), `git add -A` ❌, `Co-Authored-By` ❌.
3. **subagent-driven 실행 모델**: 오케스트레이션(로직·무결성·spec/품질 검증) = Opus(메인). 구현 스크립팅 = **Sonnet 서브에이전트**(Task 마다 fresh, full-text 프롬프트, plan 안 읽힘). 각 Task: Sonnet 구현 → Opus 가 disk/ git diff 로 검증 → **사용자 빌드+육안+커밋 게이트** → 다음.
4. **clangd 거짓에러 만연**: rename-stale 인덱스 + 헤더 compile-DB 부재로 IDE 가 `file not found`/`Pass::Kind`/`PFNGLGETPOINTERVPROC`/`std::is_void no member` 류 *거짓에러* 를 대량 표시. **disk 내용 + 실제 ninja 빌드만 진실.** 검증은 항상 disk Read + `git diff` 로(보고/ clangd 믿지 말 것).

---

## 1. TL;DR + 다음 액션

- **상태**: Phase 1(Material ROP) + Phase 2(IRenderable + Flat RenderableProcessor + A-dedup) **구현 완료, 전체 빌드 GREEN + GUI 회귀 0**. Task 2.4(실제 draw 경로 전환)까지 동작 검증됨.
- **다음 액션**: **Phase 3.0 — 원자 인터페이스 교체** (`IRenderPassable`→`IPassable`: `Draw(DeviceContext&, const Texture*)`/`GetPassResult()->const Texture*`/`BeforeIndex`). 인터페이스 + 전 구현체(SceneRenderer/ScreenQuadStage/CameraStage + ParticleStage) + main 을 **ONE 커밋**으로 동시 교체해야 빌드 GREEN(plan Task 3.0). 그 후 3.1~3.5 한 Pass씩 게이트.
- **착수 전 필수**: §2 git 재측정(사용자 병렬 작업으로 HEAD drift 가능) + §5 미커밋분 커밋 여부 확인.

---

## 2. 세계 상태 (재측정 2026-06-23, 2차 — Phase 2 전량 커밋됨)

- **branch**: `game/slang-phase2-ubo` · **HEAD**: `c4d11f9` · **working tree CLEAN**
- **최근 커밋**:
  - `c4d11f9 [refactor] MeshPassProcessor -> RenderableProcessor (IRenderable flat 경로 전환)` ← **Task 2.4 + gl3w fix + 사용자 병렬작업 묶음 커밋(사용자 직접)**
  - `107f728 [refactor] IRenderable 인터페이스 추가 & DeviceContext UseProgram A-dedup 가드` ← Task 2.1+2.2+2.3 묶음
  - `0dca57d [refactor] 렌더링 시스템 최종 다이어그램`
  - `7853888 [refactor] Material RenderStateBlock 보유 + IRenderStateProvider 구현 (D7)` ← Task 1.2
  - `d9a2ee4 [refactor] IRenderStateProvider ROP Facade 인터페이스 추가 (D7)` ← Task 1.1
  - `68506a8 [refactor] 렌더링 모듈 Rename` (Phase 0 기준선)
- **미커밋: 없음** (Phase 1~2 전량 커밋 완료). gl3w 격리 fix(`i_render_state_provider.h` 전방선언)는 `c4d11f9` 에 포함.
- **ℹ️ 사용자 병렬 작업 처리됨**: `light_ubo_uploader.cpp`(M) + `resources/shader/*`(D 삭제)를 사용자가 `c4d11f9` 에 **본인 판단으로 함께 커밋**(Slang 이행). 더 이상 분리 대상 아님.

---

## 3. Task 상태표

| Task | 내용 | 상태 | 커밋 |
|---|---|---|---|
| 1.1 | IRenderStateProvider 헤더 | ✅ | d9a2ee4 (+gl3w fix 미커밋) |
| 1.2 | Material RenderStateBlock 보유 + Facade | ✅ | 7853888 |
| 2.1 | IRenderable 인터페이스 | ✅ | 107f728 |
| 2.2 | DeviceContext UseProgram dedup (void+가드) | ✅ | 107f728 |
| 2.3 | draw_ops.h + MeshRenderer::Render 잎 | ✅ | 107f728 묶음 |
| 2.4 | MeshPassProcessor→RenderableProcessor (실제 경로 전환) | ✅ 빌드GREEN | c4d11f9 |
| 3.0 | IRenderPassable→IPassable 원자 교체 | ⬜ 다음 | — |
| 3.1~3.5 | WorldPass/SkyboxPass/ParticlePass/ImGuiPass/PostFxPass | ⬜ | — |
| 4.1~4.2 | PassIterator + main 배선 | ⬜ | — |
| 5.x | 정리/네이밍/문서(파일 git mv 포함) | ⬜ | — |

---

## 4. 실행 중 발견/해결한 함정 (재발 방지)

1. **clangd 거짓에러 ≠ 빌드 에러** (§0-4). 매 Task 마다 IDE 가 대량 거짓에러를 띄웠으나 disk/빌드는 정상. *실제 ninja `error:`* 만 대응. 검증은 disk Read + `git diff HEAD -- <file>`.
2. **인터페이스 헤더 GL 격리 (빌드로 확인된 실수)**: `i_render_state_provider.h` 가 `#include "material/pass.h"` 했더니 pass.h→`GL/gl3w.h` 가 `i_renderable.h → mesh_pass_processor.h → render_passable.impls.h → GameSystems.h` 체인을 타고 **Effekseer 클라이언트(GameSystems.cpp)** 까지 전파 → Apple `gl3.h` 와 충돌(`PFNGLGETPOINTERVPROC` 미정의). **해결: `Pass::RenderStateBlock` 전방선언**(반환 const& 라 충분). → **앞으로 `i_renderable.h`/`i_passable.h`/`pass_iterator.h` 도 GL-free 유지**(GL 끌어오는 헤더 include 금지). plan Task 1.1 에 노트 반영됨.
3. **A-dedup 형태**: `UseProgram` 은 **void 유지 + 내부 dedup 가드**(`if (mStateInitialized && mBoundProgram==&prog) return;`). bool 반환 아님(초안의 bool 안 + 잎 조건 FrameBlock 은 폐기). 잎(MeshRenderer::Render)은 FrameBlock per-draw 업로드, ROP 미적용(Processor 루프가 ApplyRenderStateBlock).
4. **ScreenQuad 전이 부채**: RenderableProcessor 에 `mScreen`/`SubmitScreenQuad`/`mScreenQuadMesh`/`mBypassMat`/`mLastOutputFB` 가 *이질 의존*으로 남아있음 — **`[TRANSITIONAL-3.5]` grep 태그로 마킹**. Phase 3.5 PostFxPass 가 흡수하며 `grep -rn "TRANSITIONAL-3.5" src/render/` 로 일괄 제거. 제거 후 RenderableProcessor = `mWorld` 단일 큐.
5. **파일 개명 연기**: 플랜 원안의 `git mv mesh_pass_processor.* -> renderable_processor.*` 는 *구현자 git 금지* + CMake churn 회피로 **Phase 5(사용자 git mv)로 연기**. 현재 클래스명만 `RenderableProcessor`(파일명/가드 `__SJH_MESH_PASS_PROCESSOR_H__` 유지).

---

## 5. 즉시 할 일 (핸드오프 받은 직후)

1. `git log --oneline -5 && git status --short` 재측정 (사용자 병렬로 HEAD/working tree drift 가능).
2. ~~미커밋분 커밋~~ **완료** — Phase 1~2 전량 `c4d11f9` 까지 커밋됨, tree clean.
3. Phase 3.0 착수 (아래 §6).

---

## 6. Phase 3 진입 (plan Task 3.0~3.5)

- **Task 3.0 (원자, ONE 커밋)**: `render_passable.h` 의 `IRenderPassable`→`IPassable` 클래스 in-place 개명(파일 유지, 신규 i_passable.h 안 만듦) + 시그니처 `Draw(DeviceContext&, const Texture* before)`/`GetPassResult()->const Texture*`/`BeforeIndex`. **전 구현체(SceneRenderer/ScreenQuadStage/CameraStage + apps ParticleStage + main.cpp `for(s:mStages) s->Render` → `s->Draw(DeviceContext::Get(), nullptr)`)를 동시 교체**(각 Draw 본문=기존 Render 그대로 임시 이식, before 미사용, GetPassResult=자기 FB attachment 또는 nullptr). 빌드 GREEN + 육안(동작 동일) + 한 커밋.
  - ⚠️ `i_passable.h`(=render_passable.h)는 **GL-free 유지**: `class Texture; class DeviceContext;` 전방선언만(§4-2 교훈).
- **3.1~3.5 (한 Pass씩 게이트)**: WorldPass(←SceneRenderer+CameraStage) / SkyboxPass / ParticlePass(←ParticleStage) / ImGuiPass(←main ImGui::Render) / PostFxPass(←PassComponent+ScreenQuadStage, 이때 `[TRANSITIONAL-3.5]` screen 경로 제거). 각 Pass 전환마다 빌드+육안+커밋. 스켈레톤 코드는 plan Task 3.1~3.5.
- **Task-time DECISION** (plan 표): 3.2 SkyboxPass 독립(추천) / 3.x A-dedup 깊이(FrameBlock 만 UseProgram, sampler/UBO dedup 은 프로파일 후).

---

## 7. 가드레일 (위반 금지)

- 빌드·커밋 = 사용자. path-scoped, `git add -A` ❌, `Co-Authored-By` ❌. **사용자 병렬 작업(light_ubo_uploader.cpp/shader) 분리.**
- 🔒 GL 격리: glXxx 는 DeviceContext(+자원 RAII) 안에만. **인터페이스 헤더(i_renderable/i_passable/i_render_state_provider/pass_iterator)는 GL 헤더 include 금지**(전방선언). 외부(efk/ImGui)는 ParticlePass/ImGuiPass + InvalidateStateCache.
- 주석 한국어+ASCII only. sb7code 수정 금지. 신규 헤더가드 `__SJH_<FILE>_H__`.
- Phase 3 한 Pass씩(3.0 원자 제외) 빌드+육안. 검증은 disk/`git diff`(clangd 무시).
- 무회귀 기준: 픽셀/순서 동일. 회귀 핫스팟 = 반투명(healthbar/shadow), Outline stencil, Skybox depth, AlphaTest sprite cull, efk/ImGui foreign-GL.

---

## 8. 포인터
- 설계 정본: `doc/마이그레이팅계획안.md` (v5).
- 구현 plan 정본: `doc/superpowers/plans/2026-06-23-render-migration-implementation-v5-reviewed.md` (Execution Status 배너 = 진행 현황).
- 설계 핸드오프: `doc/handoffs/2026-06-23-rendering-migration-design-resume-handoff.md`.
- 다이어그램: `doc/diagrams/render-migration-plan-v5.{svg,png}` (plan 클래스) · `engine-migration-class.{svg,png}` (v5 설계).
- 연구: `doc/RenderingSystemRefactorReserch.md`.
- (doc/ 전부 gitignored 로컬 — §0-1)

## 9. Change log
- 2026-06-23 작성. Phase 1~2 구현 완료(빌드 GREEN), Task 2.4+gl3w fix 미커밋, 다음=Phase 3.0. subagent-driven(Opus orchestrate/Sonnet implement) 실행 중.
