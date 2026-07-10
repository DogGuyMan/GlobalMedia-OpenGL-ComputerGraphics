# 골든 게이트 세분화 — WorldPass RenderStateBlock별 + PostFX 패스별 (spec)

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **날짜** 2026-06-28 · **base** game/remove-unused · **상태** 작성 — 아래 §7 OPEN 결정 확정 후 /writing-plans
> **목적** 현 골든 3장(full/no_imgui/skybox)을 **패스/상태별로 세분** → 회귀를 특정 RenderStateBlock/PostFX 로 국소화 + **백엔드(Metal) 교체 시 fixed-function state·shader별 divergence 정밀 검출**.
> **선행** [[test-expansion-effort]] Phase B(opencv 골든게이트, 커밋됨). ⚠ `doc/` = gitignore 로컬.

## 0. 배경 (실측)

- 현 capture(`main.cpp` frame-180, TitleState) = `PassIterator` + `DebugPassIndex`(멈출 패스) + `PassComponent::Enabled` 토글 + `Find(name)` 로 **3 변형**(full/no_imgui/skybox) → opencv absdiff 5% 게이트.
- **_MyApp_ 사용 RenderQueue**: Skybox·Transparent(다수)·AlphaTest·Opaque(불릿)·TransparentDepthWrite(학습). **Outline/Stencil = 미사용(콘텐츠 0).**
- **PostFX** = `POSTFX_PROGRAM_CONFIGS` 8스테이지(gamma·sharpening·bloom·fog·grayscale_vignetting·invert·blurring·sobel; depth_debug=Phase1 게이트). 기본 `Enabled=false`.

## 1. LOCKED 결정 (2026-06-28 사용자)

| ID | 결정 | 값 |
|---|---|---|
| **GG-1** | World 골든 캡처 씬 | **현 TitleState 한정** — 존재하는 큐만(Opaque/Outline 등 빈 큐는 skip) |
| **GG-2** | PostFX 추출 방식 | **Cumulative** — 체인 스테이지 N까지 누적 적용 후 캡처(gamma→+sharpening→…) |
| **GG-3** | 진행 | **spec 먼저 정형화** 후 subagent-driven 착수 |
| **D1** | World 캡처 대상 | **raw World FBO(PostFX 전)** — 상태 순수출력 |
| **D2** | 비교 임계 | **전역 5% 유지** (per-category 아님) |
| **D3** | 캡처 코드 위치 | ✅ **전부 `src/diagnostics`** — readback `frame_capture` + orchestration `pass_capture`. render→diagnostics 순환은 **diagnostics 예외 모듈로 감수**(CLAUDE.md 「예외 모듈 지위」). |

## 2·0. Phase GG-0 — 캡처 facility 엔진 모듈화 (D3, 기반 · 먼저)

> D3: 클라 캡처 로직 일반화 → **전부 `src/diagnostics`**(사용자 결정 2026-06-28). ⚠ `render → diagnostics` 가 이미 존재 → orchestration(PassIterator=render)을 diagnostics 에 넣으면 `diagnostics→render→diagnostics` **순환** 발생. **→ diagnostics 는 예외 모듈(cycle-exempt)로 감수**(진단/캡처는 어느 계층이든 관측하는 게 본질 — CLAUDE.md「예외 모듈 지위」마크업). CMake 는 STATIC lib 순환을 link-line 반복으로 해소(구현 시 검증; `SJH::engine` 우산이 최종 링크). 사이클 규율 출처=`modular-build-discipline`+2026-06-11 엔진 사이클 리팩토링 — diagnostics 만 예외, 타 모듈은 무순환 유지.

### GG-0.1 readback primitive → `src/diagnostics/frame_capture.{h,cpp}`
- `CaptureBackbufferToPng(path,w,h)` — `<apps>/_MyApp_/src/Capture/golden_capture.cpp` 에서 **엔진 이관**(glReadPixels FBO0 + vflip + stb_write).
- `CaptureFramebufferToPng(GLuint fbo, path,w,h)` — **신규**(raw World FBO readback, D1). raw `GLuint` 인자라 buffer/RenderTarget 의존 0 → 사이클 없음.
- `STB_IMAGE_WRITE_IMPLEMENTATION` = 이 TU 단일소유(image.cpp 의 STB_IMAGE 매크로와 별개). deps = project_deps(GL)+stb+spdlog.
- **백엔드 무관 시그니처**(경로/크기), 내부만 GL → 후일 Metal readback 교체점. `apps/.../Capture/` 삭제.

### GG-0.2 generic pass-capture runner → `src/diagnostics/pass_capture.{h,cpp}` (예외 모듈)
- `struct CaptureVariant { std::string outName; std::vector<std::string> disablePassKeys; int stopAtPass=-1; /*optional*/ int worldQueueMin=-1, worldQueueMax=-1; enum Target{Backbuffer, WorldFbo} target; };`
- `void RunCaptureVariants(PassIterator&, RenderTarget* worldFbo, const std::vector<CaptureVariant>&, const std::string& outDir, int w, int h);` — 각 변형: 패스 토글/`DebugPassIndex`/queue-range 설정 → `Execute` → GG-0.1 readback → 원복.
- ⚠ diagnostics 가 `SJH::render`(PassIterator)+`SJH::buffer`(RenderTarget) **상향 링크** → `render→diagnostics` 와 순환. **diagnostics 예외 모듈로 감수**(CLAUDE.md). CMake static 순환은 link-line 반복 해소(구현 시 빌드 검증). 앱은 *변형 LIST(자기 패스명)* 만 제공.

### GG-0.3 client 잔여 (thin)
- env `SJH_GOLDEN_CAPTURE` 트리거·`srand(42)`·고정-dt drive(`effectiveTime`)·frame==180 게이트 = render 루프 wiring(앱 고유, 유지).
- 3+8+~4 변형 LIST 구성 → `RunCaptureVariants` 1콜. **main.cpp 캡처블록 대폭 축소**(「// ! 모듈화 대상」 상당수 해소).

★HANDOFF #GG-0: frame_capture(diagnostics) + pass_capture(render) + 기존 3골든이 새 경로로 **동일 재생성**(회귀 0, bit-동일) + 빌드 GREEN.

## 2. Phase GG-A — PostFX 누적 골든 (쉬움, GG-0 위)

PostFX 스테이지는 PassComponent(`Enabled`) + `Find(name)`. 기본 비활성이므로 **누적 enable** 로 캡처:
- for N in 1..8: `POSTFX_PROGRAM_CONFIGS[0..N)` 를 `Enabled=true`, 나머지 `false` → 전체 파이프라인 실행 → **backbuffer** 캡처 → `golden_postfx_<N>_<name>.png`.
  - N=1 → gamma / N=2 → gamma+sharpening / … / N=8 → +sobel. **누적 = 순서 의존 효과 관찰.**
- 신규 인프라 0 — 기존 `Enabled` 토글 + `Find`. depth_debug 는 Phase1 게이트라 제외.
- ⚠ 캡처 후 원복(전 postfx `Enabled=false` 기본 복원) 필수.
- **8 골든.** ★HANDOFF #GG-A: 8 postfx 누적 골든 생성 + 육안.

## 3. Phase GG-B — World RenderStateBlock별 골든 (신규 인프라)

WorldPass 는 전 MeshRenderer 를 큐 정렬해 한 번에 그림 → **단일 RenderQueue 격리 훅이 없다.**

### GG-B.1 신규 인프라: queue-range 필터 (render-side)
- `SceneRenderer`/WorldPass 에 **디버그 queue-range 필터** `[minQueue, maxQueue)` 추가 — 이 범위의 draw 만 발행. 데이터주도(RenderQueue enum). (per-object `Visible` 토글 대비 render 개념상 깔끔 + 백엔드 이식 친화.)
  - 평시 default = 전범위(무영향). capture 만 좁힌다.
- ⚠ **비침투 원칙**: 게임 로직 무변경, `SceneRenderer` 에 옵셔널 필드 추가(default 무해).

### GG-B.2 캡처
- TitleState 에 **콘텐츠가 있는 큐만**(impl 이 판정 — 대략 Skybox/Transparent/AlphaTest; Opaque=불릿·Title엔 없음, Outline=미사용 → skip): queue-filter 를 단일 큐로 좁혀 WorldPass 렌더 → 캡처 → `golden_world_<queue>.png`.
- ✅ **D1 확정 = raw World FBO(PostFX 전)**: GG-0.1 `CaptureFramebufferToPng(worldFbo, ...)` 로 그 큐의 순수 출력 캡처(PostFX 미적용). 백엔드 fixed-function state divergence 가 PostFX 에 안 섞임.
- 빈 큐는 골든 안 만듦(skip + 로그).
- **≈3~4 골든**(TitleState 실 콘텐츠 의존). ★HANDOFF #GG-B: World 큐별 골든 + 육안.

## 4. Phase GG-C — 비교 게이트 데이터주도 확장

현 `golden_compare.cpp` 는 3 이름 하드코딩. → **ref dir 의 `golden_*.png` glob** 해서 캡처본 vs ref 를 전수 비교(추가 골든 = 비교코드 무변경). ctest fixture 는 그대로(capture setup → compare).
- ✅ **D2 확정 = 전역 5% 유지**. per-queue/postfx 희소 이미지도 동일 5%(noise floor=0 이라 same-machine 실질 bit-exact → 5% 는 안전마진). golden_compare 비교로직 무변경 — glob 만 추가.
- ★HANDOFF #GG-C: glob 비교 + 전 골든(3+8+~4≈15) PASS + 의도주입 FAIL.

## 5. 캡처 코드 위치 → **§Phase GG-0 에서 해결** (D3)

readback=`src/diagnostics/frame_capture`, orchestration=`src/render/pass_capture`, client=thin(변형 LIST + 고정-dt drive). main.cpp 캡처블록 대폭 축소 = 「// ! 모듈화 대상」 상당수 해소.

## 6. ★ HANDOFF-UNIT 지도

| unit | green | 신규인프라 |
|---|---|---|
| **#GG-0** | frame_capture+pass_capture(**둘 다 diagnostics**), 기존 3골든 새경로로 bit-동일 재생성 + 빌드GREEN(순환 링크 통과) | **엔진 캡처모듈**(diagnostics, cycle-exempt) |
| #GG-A | PostFX 누적 8 골든 + 육안 | 없음(변형 LIST 확장) |
| #GG-B | World 큐별 ~4 골든(raw FBO) + 육안 | **queue-range 필터**(SceneRenderer) |
| #GG-C | glob 비교 게이트 + 전 골든(~15) PASS + 의도주입 FAIL | golden_compare glob화 |

착수 순서: **GG-0(엔진모듈 기반) → GG-A(쉬움) → GG-B(인프라) → GG-C(게이트)**. GG-0 이 서면 A/B/C 는 변형 LIST 추가 + queue-filter 만.

## 7. 결정 — LOCKED (2026-06-28) + 확인 1건

| ID | 결정 | 확정값 |
|---|---|---|
| **D1** | World 캡처 대상 | ✅ **raw World FBO(PostFX 전)** — GG-0.1 `CaptureFramebufferToPng` |
| **D2** | 비교 임계 | ✅ **전역 5% 유지** — golden_compare 무변경(glob만) |
| **D3** | 캡처 코드 위치 | ✅ **전부 `src/diagnostics`**(GG-0) — render→diagnostics 순환은 **diagnostics 예외 모듈로 감수**(사용자 결정 + CLAUDE.md 마크업). CMake static 순환 link-line 반복 해소 |
| **D4** *(default)* | 해상도/저장 | 2560×1440 유지(~15×2.5MB≈37MB); 스케일 시 Git LFS |

## 8. 가드레일
- 비침투: 게임 로직/셰이더 무변경. queue-filter 는 SceneRenderer 옵셔널(default 무해). capture 는 render 분기.
- 커밋 path-scoped·Co-Authored-By 미사용·주석 한글·characterization(현출력 잠금). subagent-driven(구현↔검증 분리) — 골든 게이트는 **의도주입 FAIL** 로 adequacy 필수. 골든 raw PNG 커밋 증가 → 필요시 Git LFS.
