# 테스트 확장 계획 — 추가 CPU + GPU 테스트 (2026-06-27)

> ⚠️ **2026-07-26 정정 (원문 보존)** — 본 문서에 나오는 `SJH_GOLDEN_CAPTURE=1 ./_MyApp_` 실행과 `ctest --test-dir build_ninja -R golden` 은 *당시* 절차이며 현재는 **폐기**됐다. 골든 캡처가 런타임 환경 변수 -> 컴파일 정의로 바뀌어 프리셋 `ninja-golden`(빌드 디렉토리 `build_ninja-golden`) 전유가 됐고, 게임 빌드의 `_MyApp_` 는 골든을 캡처하지 않는다(실행해도 창만 뜨는 조용한 실패). 현행 절차 = `test/CLAUDE.md`. 아래 본문은 당시 기록으로 그대로 둔다.

> **근거 문서**: `ENV_SETUP_PLAN_v3.md`(골든 회귀 PoC Phase 0~3), `catch2_pipeline_korean.md`(Catch2+Mull+골든+퍼징 5단계).
> **조사 방법**: 18개 코어 모듈(`src/<module>/`) 공개 헤더 전수 + 기존 5 테스트 대조 (Explore 읽기전용 조사).
> **상태**: 계획 — 아래 §6 결정 잠금 후 spec/plan 정형화. `doc/` = .gitignore 로컬.

---

## 0. 현 위치 (재측정)

- **Track A (CPU 안전망)**: 28 ctest / 5모듈 — `common`(smoke)·`timer`·`object`(Transform·Light)·`sprite`(ComputeUVRect)·`fsm`. game/remove-unused 통합 GREEN (2026-06-27, 28/28).
- **Track B (골든)**: 3 PNG(`test/golden/golden_{full,no_imgui,skybox}.png`, 2560×1440), noise floor=0(비트결정적, 동일머신). **비교 게이트 미구현** — 캡처만 됨.
- **미테스트**: 18모듈 중 13.

근거 문서 핵심:
- **v3**: 시드 전수 고정 → noise floor 측정 → 헤드리스 GL → diff 게이트. (Track B 가 noise floor=0 으로 Phase 0 충족.)
- **catch2_pipeline**: 권고 순위 ① Catch2 코어(완료) ② 골든 하네스 ③ Mull `gitDiffRef` 게이트 ④ 커스텀 스킬 ⑤ RapidCheck/libFuzzer. 골든 비교 라이브러리 = pixelmatch-cpp17 임베드 권장. GPU 의존 골든은 **뮤테이션 제외**(Mull 은 결정적 CPU 로직에 집중).

---

## 1. 커버리지 갭 — 모듈별 분류 (순수-CPU vs GPU 결합)

조사 결과, 미테스트 13모듈을 GL 컨텍스트 필요 여부로 분류:

| 모듈 | 순수-CPU 테스트 가능 표면 | GPU(GL) 필요 표면 |
|---|---|---|
| **scene** | ✅ **거의 100%** — Actor/Component 트리·부모체인 행렬·컴포넌트 dup가드·Update 순회 | OnEnter/OnExit hook(자체는 순수, concrete 책임) |
| **playable** | ✅ **100%** — IPlayable 상태머신·Composite 순서·Loop/Finished | — |
| **input** | ✅ **거의 100%** — Dispatch·HandleMove·dragging (GLFW 상수만, GL 무관) | PollHeld(glfwGetKey — 창 필요, GL 아님) |
| **material** | ✅ **거의 100%** — Pass/QueueLayer·Clone 메타·Properties·GetRootOriginal | SetProgram 도 GL 호출 없음 |
| **text** | ✅ BitmapFont Find/Frame/Advance fallback·XML 파싱·V-flip / TextRenderer 레이아웃 수학 | LoadFromBMFont→Texture 생성, SpriteRenderer 렌더 |
| **timer**(부분) | ✅ **MultipleTimer 다중트랙**(기존 test_timer 갭) | — |
| **object**(부분) | ✅ Transform **부모체인/월드행렬**(기존 테스트 갭) | — |
| **diagnostics** | ✅ `GLValidate::CheckIndices`(OOB/degenerate/중복) | CheckVSAttributeLayout·CheckProgramLink·CheckGL*(glGet*) |
| **texture** | ✅ Image: Create/SetCheck/SetWhite/SetSingleColor(malloc 픽셀채움)·Load(stbi I/O) | Texture: glGenTextures/glTexImage2D/Bind/SetFilter |
| **resource_registry** | ✅ Find 계열·GetAllPrograms·RegisterMesh·Clear(map 조작) | Create 계열(Texture/Program/Framebuffer/Atlas 생성) |
| **buffer** | ❌ Stride/Count 저장값만(trivial) | CreateWithData→glGenBuffers/glBufferData, Bind |
| **layout** | ❌ 없음 | Create/TrySetAttrib/Bind 전부 VAO GL |
| **program** | ❌ 없음 | Create(링킹)/GetLocation/BindUniformBlocks 전부 GL |
| **shader** | ❌ 없음 | CreateFromFile/Source = glCompileShader |
| **render** | ✅ ApplyRenderStateBlock dirty-check 비교 로직만 | 나머지 전부 glUseProgram/Bind*/Draw* |

**요지**: scene·playable·input·material·timer(MultipleTimer)·text(BitmapFont)·diagnostics(CheckIndices) = 순수-CPU 안전망 확장 가능. buffer·layout·program·shader·texture(Texture)·render = GL 컨텍스트 필요 → GPU 트랙.

---

## 2. Track A+ — 추가 CPU 단위 테스트 (순수, GL 불필요, contract-anchoring)

| # | 모듈 | 테스트 핵심 | 우선 |
|---|---|---|---|
| 1 | **scene::Actor/Component** | AddChild/Detach/RemoveChild(null·중복·recursive)·FindChild/FindChildIf(DFS/predicate)·**GetWorldMatrix 부모체인 곱**·AddComponent 중복가드·GetComponent fast(typeid)/slow(dynamic_cast)·Update 순회순서 | ★1 |
| 2 | **playable::Sequence/Parallel** | Append/Insert/Join 순서·Cursor 진행(child 종료감지→다음)·IsFinished(Seq 전부끝 vs Par 전부끝)·**Loop=true 면 IsFinished 영영 false**·Play/Pause/Stop 리셋 | ★1 |
| 3 | **input::Keyboard/Mouse** | Dispatch 바인딩조회→핸들러(PRESS/RELEASE/REPEAT)·HandleMove delta(dx,dy)·HandleButton dragging 전이·IsDragging/CancelDrag | ★1 |
| 4 | **timer::MultipleTimer** | 다중트랙 Register/tick(개별 진행)/Unregister·핸들 무효화 (기존 test_timer 단일 Timer 만 커버) | ★1 |
| 5 | text::BitmapFont | Find/FrameOf/AdvanceOf fallback('?'→' '→-1/cellW)·XML 파싱 정상/malformed·V-flip(rows-1-pngRow) | 2 |
| 6 | material::Material | SetPass→GetQueueLayer(enum→int)·Clone 후 IsInstance/OriginalMaterial·GetRootOriginal 체인·Properties typed map(Float/Int/Vec3/Color) | 2 |
| 7 | ~~diagnostics::GLValidate::CheckIndices~~ | ⛔ **제외** (TX-1 사용자 결정) | — |
| 8 | ~~texture::Image~~ | ⛔ 범위 밖 (3순위) | — |
| 9 | ~~resource_registry~~ | ⛔ 범위 밖 (3순위) | — |

**✅ 확정 범위 (TX-1=b − CheckIndices)**: **6 테스트 파일** = scene·playable·input·MultipleTimer(1순위) + text(BitmapFont)·material(2순위). CheckIndices 및 3순위(Image·rr) 제외.

**배선**: `test/test_<x>.cpp` + `test/CMakeLists.txt` 에 `sjh_add_test(test_<x> SJH::<module>)` + `tests` 커스텀타깃 `DEPENDS` 추가. 테스트는 characterization(현 동작 잠금, red-first 아님). **★ 1순위 4종 green = ★HANDOFF #A1**, 2순위 2종 green = ★HANDOFF #A2.

---

## 3. Track B — 골든 비교 게이트 (deferred Track B 완성, GPU)

이미 골든 3장 + 캡처모드 + noise floor=0 확보. 남은 = **비교 게이트**.

- **B1 비교기 (TX-3=b, TQ-1)**: **pixelmatch-cpp17 단일헤더 vendoring + 차이픽셀 비율 5% 예산**(`maxPixelFraction=0.05`, per-pixel 임계는 pixelmatch 기본 0.1). stb 로 두 PNG 로드 → pixelmatch diff → 차이픽셀/전체 > 5% 면 FAIL + diff PNG 아티팩트 출력. (noise floor=0 이라 동일머신은 사실상 비트일치지만 처음부터 허용오차로 크로스머신 대비.)
- **B2 verify**: ctest `add_test` 가 `SJH_GOLDEN_CAPTURE=1 _MyApp_` 실행 → 3 PNG 생성(`build_ninja/apps/_MyApp_/test/golden/`) → repo `test/golden/` 와 비교 → exit 0/1. (단일 `verify` 경로, v3 1.5.)
- **B3 갱신 게이트**(v3 1.3, 비협상): 에이전트 자동 덮어쓰기 **금지**, `/golden-update` 사람 승인. (CITYWALK §5.4 컴파일러-출력 오라클 문제 회피.)
- **B4 사각지대 명시**(v3 1.6): 골든은 GL 상태누수·성능·비가시 동작파손 못잡음 → Track C 보완.
- ⚠ `<scripts>/crossver_verify.sh`(Track A 차분)는 현 트리에 없음 — game/test-harness 브랜치 로컬, 통합 시 동반. (부재가 확정적이라 archival placeholder 표기 — `doc/CLAUDE.md` Gotchas.)

**★HANDOFF #B**: B green = CPU→GPU 성격전환 → 필수 분리(메모리 `plan-handoff-boundary-markup`).

---

## 4. Track C — GPU 단위 테스트 (GL 컨텍스트 픽스처) ★프로젝트 최대가치

- **C0 GL 픽스처**: Catch2 커스텀 main(`Catch2::Catch2`, WithMain 아님) 또는 글로벌 픽스처 — 숨김 GLFW 창 + GL4.1 Core 1회 init. (프로젝트는 EGL 없음, GLFW 숨김창 관행. catch2_pipeline §3.) **전 GPU 테스트 enabler.**
- **C1 셰이더 링크 검증** ★★: 전 `.slang`→GLSL410 프로그램(또는 엔진 program set) 링크 후 `program != 0` + 링크에러 0 단언. **macOS Slang 함정 직격** — 메모리 3건(`slang-varying-name-mismatch`·`slang-sampler-name-suffix`·`slang-glsl410-traps`): glslang 통과·GL 런타임만 거부 → 은밀한 투명. **이 프로젝트 단일 최고가치 GPU 테스트.**
- **C2 DeviceContext 상태캐시**: blend func 캐시 기본값 불일치(메모리 `blend-func-cache-default-mismatch`)·ApplyRenderStateBlock dirty-check·InvalidateStateCache 규율.
- **C3 roundtrip**: buffer CreateWithData→read back·Texture CreateTexture→glGetTexImage·program GetLocation(알려진 uniform).
- **C4 GL 상태누수**(v3 1.6): 패스 렌더 후 GL 상태 스냅샷 비교(다음 패스로 새는 바인딩 탐지).

우선: C0(enabler) → **C1(최고)** → C2 → C3/C4. **★HANDOFF #C**: C green.

---

## 5. Track D — 적대적/뮤테이션 (⛔ DEFERRED, 지금 안 함)

> 사용자 결정 "적대적 검증 보류" 유지. 별도 effort + 무거운 툴체인.

- **Mull `gitDiffRef` 뮤테이션 게이트**: CPU 테스트가 실제 mutant 죽이는지(테스트 품질) 검증. Clang+Mull(`-fpass-plugin=mull-ir-frontend-N`) + `mull.yml`(excludePaths=Catch2/third_party, GPU 골든 제외). PR 변경라인만 뮤테이션(실험적). catch2_pipeline §2.
- **RapidCheck**(`<rapidcheck>/catch.h` — 미도입 외부 라이브러리 헤더라 placeholder) 프로퍼티 + **libFuzzer**: BMFont XML 파서가 1순위 퍼징 후보 → 크래시→`[regression]` Catch2 케이스.
- **LLM 보조 테스트 초안**(Qwen-Coder), Mull 점수로 게이트 — 최후, 사람 감독.

---

## 6. ★ 결정 — LOCKED (2026-06-27 사용자)

| ID | 결정 | **확정값** |
|---|---|---|
| **TX-1** | Track A+ CPU 범위 | ✅ **(b) 1+2순위 − CheckIndices** = 6종(scene·playable·input·MultipleTimer·text·material) |
| **TX-2** | GPU 트랙 우선 | ✅ **(c) 둘 다** — 골든 게이트(B) + GL 단위테스트(C) 모두 |
| **TX-3** | 골든 비교 방식 | ✅ **(b) pixelmatch 허용오차 5%** (의미는 TQ-1 확정) |
| **TX-4** | Track D 시점 | ✅ **spec 후속단계로만 명시 마크업** (지금 구현 안 함) |

**꼬리질문 LOCKED (TQ):**
- **TQ-1** 골든 5% = **차이픽셀 비율 예산**(maxPixelFraction=0.05).
- **TQ-2** 작업 = **현 game/remove-unused 직접**(worktree 아님, path-scoped 공존).
- **TQ-3** 실행 = **subagent-driven**(구현 에이전트 + spec리뷰 + 품질리뷰, anti-gaming — Track A 패턴).

---

## 7. ★ HANDOFF-UNIT 지도 (메모리 `plan-handoff-boundary-markup`)

| unit | green 체크포인트 | 성격 |
|---|---|---|
| #A1 | Track A+ 1순위 4종 ctest GREEN | 순수 CPU |
| #A2 | Track A+ 2순위 GREEN | 순수 CPU |
| **★HANDOFF (CPU→GPU)** | A 완료 → 핸드오프 (성격전환 필수분리) | |
| #B | 골든 비교 게이트 exit0/1 동작 + 의도적 1px 변경이 FAIL | GPU/골든 |
| #C | GL 픽스처 + 셰이더 링크검증 GREEN | GPU/GL |

**가드레일**: 커밋 path-scoped(`git add -A` 금지)·Co-Authored-By 미사용·주석 한국어·sb7 무수정·main.cpp 무침투(「// ! 모듈화 대상」 캡처 모듈화는 별도 DEFERRED effort)·테스트 임의추가 금지(이 계획 범위 내만).
