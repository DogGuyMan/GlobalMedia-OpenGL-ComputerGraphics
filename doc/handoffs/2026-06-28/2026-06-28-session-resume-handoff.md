# 세션 재개 핸드오프 — 테스트 확장(완료) + 골든게이트 세분화(plan) + Metal 백엔드 전조사

> ⚠️ **2026-07-26 정정 (원문 보존)** — 본 문서에 나오는 `SJH_GOLDEN_CAPTURE=1 ./_MyApp_` 실행과 `ctest --test-dir build_ninja -R golden` 은 *당시* 절차이며 현재는 **폐기**됐다. 골든 캡처가 런타임 환경 변수 -> 컴파일 정의로 바뀌어 프리셋 `ninja-golden`(빌드 디렉토리 `build_ninja-golden`) 전유가 됐고, 게임 빌드의 `_MyApp_` 는 골든을 캡처하지 않는다(실행해도 창만 뜨는 조용한 실패). 현행 절차 = `test/CLAUDE.md`. 아래 본문은 당시 기록으로 그대로 둔다.

> **작성** 2026-06-28 · **재측정** HEAD `770755f`(detached) · **맥락0 자기완결** 단일 진입점.
> ⚠ `doc/` = .gitignore 로컬(same-machine 연속성용). 이 문서만 읽으면 재개 가능.

## TL;DR + 다음 액션

이 세션은 3개 arc: **①엔진 테스트 확장 A+B+C (완료·커밋·병합)** → **②latent 버그 2건 수정** → **③골든게이트 세분화 spec+plan 작성(미실행) + Metal 백엔드 의존 전조사(분석만)**.

**가장 최근 활성 = 골든게이트 세분화.** spec+plan 완성, **실행 대기**. 다음 액션 택1:
1. **골든게이트 plan 실행** — `doc/superpowers/plans/2026-06-28-golden-gate-per-pass.md` 를 subagent-driven 으로. **첫 리스크 = GG-0 Task 0.3 Step2**(diagnostics↔render STATIC 순환 링크 실빌드 검증).
2. **Metal 백엔드 마이그레이션 착수** — 전조사 완료(아래 §Metal). RHI 추상화 = 큰 다단계, spec 먼저 권장.

## State of the world (재측정 2026-06-28)

- **HEAD = `770755f` (detached from 755faa7)**. 브랜치: `game/main`, `game/remove-unused`(내 테스트 커밋 보유), `feature/engine-extraction`, `game/golden-315269a` 등.
- **git status**: working tree clean except 사용자 미커밋 2 = `M .clangd`, `M apps/_MyApp_/shaders_slang/matrix_skybox.slang`. (내 작업 아님 — 건드리지 말 것.)
- ⚠ **사용자 병렬 git**: 세션 중 `game/remove-unused → game/main` 병합(`1326783`) + "Rollback Phase 2"(`755faa7`) + billboard/texture_atlas 셰이더 refactor 6커밋(`770755f`~`c070f18`) 추가. **테스트 확장 커밋은 병합으로 game/main 에 이미 합류**(working tree 에 전부 존재).

### 커밋된 것 (game/main 병합 완료, working tree 존재 확인됨)
| SHA | 내용 |
|---|---|
| `0be67b5` | [test] 1차 골든 이미지 + capture 모드 |
| `9be83eb` | [test] CPU 단위 테스트(Track A+ 6종: scene/playable/input/multiple_timer/material/text) |
| `1b45ce6` | [feat] diagnostics 갱신(A3: DiagResult/DiffStates/ClassifyInfoLog/LinkReport + 셰이더 fail-fast) |
| `4c59695` | [test] 골든 비교 게이트 OpenCV(absdiff 5%) |
| `aa82ac1` | [test] OpenGL 단위테스트(Phase C: gpu_tests 셰이더링크/상태캐시/roundtrip/누수/text) — game/remove-unused tip |

**ctest = 122** (28 기존 + Track A+ + gpu_tests). `test/gpu/`(8파일)·`test/test_*.cpp`(6)·`test/golden_compare/`·`test/third_party/`(pixelmatch 삭제됨, opencv 전환)·`test/golden/`(3 골든) 전부 working tree 존재.

### ⚠ 커밋 상태 불확실 (재개 시 확인)
- **latent 버그 2건 수정**: `src/material/material.h`(GetRootOriginal 자기참조 가드) + `src/diagnostics/gl_state_fields.cpp`(CaptureGLState VAO=0 가드) + `test/test_material.cpp`(재호출 lock) + `test/gpu/test_state_leak.cpp`(ScopedVAO 제거). **커밋 명령 제시했으나 사용자 실행 여부 미확인.** working tree 가 clean 이므로 *커밋됐거나* 병합에 흡수됨 — `git log --oneline | grep -i "latent\|GetRootOriginal\|VAO"` 로 확인.

### 미커밋/미실행
- **골든게이트 세분화** = `doc/` 의 spec+plan 만(gitignore 로컬). 코드 0. `src/diagnostics/frame_capture` 부재 확인.
- **Metal 백엔드** = 분석만.

## 진행 arc 상세

### ① 엔진 테스트 확장 A+B+C — ✅ 완료
정본 spec = `doc/superpowers/specs/2026-06-27-test-expansion-design.md`. 메모리 = [[test-expansion-effort]].
- **A (CPU)**: scene·playable·input·MultipleTimer·material 6종 characterization + **diagnostics 구조화반환 additive**(CheckIndicesDetailed→DiagResult·DiffStates·ClassifyInfoLog·CheckProgramLinkReport→LinkReport, 기존 wrapper 보존) + **셰이더 fail-fast**(Shader/Program 실패 시 Debug abort/Release throw; 호출처=ResourceRegistry 1곳). text 대부분 GL결합이라 Phase C 이연.
- **B (골든)**: `_MyApp_` capture 모드(env `SJH_GOLDEN_CAPTURE=1`, 고정-dt 180프레임, srand42) → 3 PNG → **opencv4(default-features:false+png) absdiff+countNonZero 5% 예산** 비교(pixelmatch→opencv 사용자 교체). ctest fixture(capture setup→compare).
- **C (GL)**: `test/gpu/` 신규 `gpu_tests`(Catch2 글로벌리스너 숨김GLFW+GL4.1) — C-1 셰이더링크검증(slang 독립재컴파일→raw GL+LinkReport, fail-fast 우회)·C-2 상태캐시·C-3 roundtrip·C-4 상태누수(CaptureGLState+DiffStates)·C-5 text BitmapFont.
- **검증**: subagent-driven(구현↔테스트작성↔리뷰 분리, anti-gaming). 골든/링크 게이트는 **실제 깨뜨려 adequacy** 확인. ⚠ GPU 리뷰 Explore 에이전트 스톨 전례 → GL adequacy 는 오케스트레이터 직접 검증 안전.

### ② latent 버그 2건 — 수정(커밋 확인 요)
1. `Material::GetRootOriginal` 진짜 root 자기참조→재호출 무한루프 → `if(p!=this)` 가드(반환값 불변).
2. `CaptureGLState` GL4.1 Core VAO=0 GL_INVALID_OPERATION → `if(f.vao!=0)` attribute query skip.
둘 다 characterization lock 테스트 동반. **Phase D(Mull 뮤테이션/퍼징)=🔴보류(HOLD).**

### ③-A 골든게이트 세분화 — spec+plan(미실행) ★현 활성
- **정본 spec** = `doc/superpowers/specs/2026-06-28-golden-gate-per-pass-design.md`
- **plan** = `doc/superpowers/plans/2026-06-28-golden-gate-per-pass.md` (4유닛 15태스크, 완전코드)
- **목표**: 골든 3장 → 패스/상태별 세분(회귀 국소화 + Metal divergence 검출). GG-0(캡처 엔진모듈화 diagnostics) → GG-A(PostFX 누적 8) → GG-B(World RenderQueue별 raw-FBO ~4) → GG-C(golden_compare glob).
- **LOCKED 결정**: GG-1(World=TitleState 한정) · GG-2(PostFX=cumulative) · D1(raw World FBO, PostFX 전) · D2(전역 5% 유지) · **D3(캡처 전부 `src/diagnostics` 이관, cycle-exempt)** · D4(2560×1440).
- ⚠ **최대 리스크**: diagnostics 가 render(PassIterator)+buffer(RenderTarget) 상향링크 → `render→diagnostics` 와 **STATIC 순환**. **diagnostics=예외 모듈로 감수**(사용자 결정, `.claude/CLAUDE.md`「예외 모듈 지위」마크업). CMake static 순환 link-line 반복 해소 — **GG-0 Task 0.3 Step2 에서 실빌드 검증이 첫 관문.**
- 캡처 flip(상하반전 방지) 주석 = plan L119 근처(사용자 요청 반영됨).

### ③-B Metal 백엔드 전조사 — 분석만
사용자 목표 = OpenGL→Metal 백엔드 교체(XCode Frame Debugger). **가설("GL 의존은 device_context 만")은 틀림** 판정. 실측:
- **GL 호출 src 11모듈**: buffer/layout/program/shader/texture/object(자원생성) + render/device_context(상태·draw) + diagnostics(glGet*). GL-free 7: fsm/input/playable/resource_registry/scene/text/timer.
- **공개 헤더 22개 GL 타입/인라인 누수**(GLuint/GLenum) — device_context.h 포함.
- **apps 실제 GL 호출 = 2파일 5호출**(main.cpp glClearColor/glViewport, golden_capture glReadPixels 등). 나머지 apps ~15파일은 gl3w include-만(정리대상).
- **sb7 결합**: main.cpp(sb7::application/DECLARE_MAIN) + **shader.cpp(sb7::shader::load = 엔진 셰이더로더!)** + GLFW 입력 5파일.
- **변경 범위** = RHI 추상화(자원계층 최대) + sb7→platform 모듈 + 셰이더는 Slang 중간언어라 MSL 백엔드로 절감. **테스트안전망(122 ctest+골든)이 회귀가드.**
- 권장 순서: ①헤더 GL타입 제거 ②RHI 인터페이스(GL 첫 백엔드) ③sb7→platform ④Slang→MSL ⑤Metal. 메모리 [[backend-abstraction-direction]].

## Locked 결정 (재논의 금지)
- **diagnostics = cycle-exempt 예외 모듈** — 진단/캡처는 어느 계층이든 관측하는 게 본질. render/buffer 상향의존+순환 허용. **diagnostics 에만** — 타 모듈은 무순환 유지. (`.claude/CLAUDE.md`「Diagnostics 세부 > 예외 모듈 지위」.) 사이클 규율 출처 = `modular-build-discipline` 스킬 + 2026-06-11 엔진 의존사이클 리팩토링([[dependency_cycles_survey]]).
- 골든 비교 = **opencv absdiff 5% 예산**(pixelmatch 폐기). SSIM 아님.
- 테스트 = characterization(현동작 잠금, red-first 아님, [[no_auto_tests]]).
- **deep-research 남용 사고**(96에이전트·490만토큰·monthly spend limit 도달) → `.claude/CLAUDE.md` 「도구 사용 가드레일」신설. **무거운 Workflow/deep-research 는 착수 전 비용고지+승인, 표준지식/단일lookup 은 직접 답.**

## Guardrails
- 커밋 **path-scoped**(`git commit <경로>` / `git add <명시경로>` — **`git add -A` 금지**, 사용자 병렬 staging 휩쓺 [[user-parallel-git-and-builds]]). **Co-Authored-By 미사용.** 주석 한글.
- 빌드/커밋 사용자 게이트. 구현 에이전트는 `--target tests`/`gpu_tests`/`_MyApp_` 자율 빌드+ctest 만.
- sb7(extern/sb7code) 무수정. 게임 로직/셰이더 무변경(테스트 확장 한정). 사용자 미커밋 `.clangd`/`matrix_skybox.slang` 건드리지 말 것.
- clang 진단(gl3w.h not found 등)은 알려진 clangd 거짓에러([[clangd-imgui-cascade-false-errors]]) — 빌드/ctest 가 진실.

## Pointers
- specs: `doc/superpowers/specs/2026-06-27-test-expansion-design.md`(A/B/C 완료) · `2026-06-28-golden-gate-per-pass-design.md`(현활성)
- plan: `doc/superpowers/plans/2026-06-28-golden-gate-per-pass.md`
- 메모리: [[test-expansion-effort]] · [[golden-image-capture-effort]] · [[engine-test-harness-effort]] · [[backend-abstraction-direction]] · [[dependency_cycles_survey]]
- ⚠ doc/ = gitignore 로컬 → 다른 머신 미전파. 필요시 tracked 경로 이주.

## Change log
- 2026-06-28: 테스트확장 A+B+C 완료·병합(game/main). latent 2건 수정. 골든게이트 세분화 spec+plan 작성(미실행). Metal 백엔드 전조사. diagnostics=cycle-exempt 결정+CLAUDE.md 마크업. deep-research 가드레일 신설. 이 핸드오프 생성.
