# 골든 이미지 캡처 설계 — Track B Phase 2 (샘플 생성)

> **날짜** 2026-06-25 · **base** `7949a65` (game/main HEAD = oracle, 안정) · **상태** 설계 확정 → /writing-plans 대기
> **범위** 결정적 골든 **샘플 3장 생성**만. 비교(FLIP)·ctest 게이트·CI 는 후속 별도.
> ⚠ `doc/` = .gitignore 로컬 (커밋 안 됨). Track A(테스트 하네스)와 별개 effort.

## 0. 목적

안정 브랜치(7949a65)의 렌더 출력을 **결정적 골든 PNG 3장**으로 캡처 — 향후 렌더 리팩토링의 시각 회귀 기준(Track B). gate-0(전 비결정 고정)을 충족해 *재현 가능*해야 한다.

## 1. 결정 로그 (LOCKED)

| ID | 결정 | 근거 |
|---|---|---|
| **GB1** | 1차 범위 = **3 골든 PNG 생성**(비교/FLIP/ctest 는 후속) | 사용자 — "샘플 만드는 작업" |
| **GB2** | 캡처 형식 = **A: _MyApp_ 내 capture 모드** | 실제 파이프라인 100% 재사용(충실도) + 시드 고정을 실제 set-site 에 surgical 주입 + 중복 0 |
| **GB3** | 3 이미지 = **G1 풀**(Skybox+Scene+ImGui) / **G2**(Skybox+Scene, ImGui off) / **G3**(Skybox only, ImGui+Scene off). **3장 모두 PostFX 거쳐 backbuffer** — 차이는 *내용 패스*만(일관성) | 레이어 peel; PostFX 일관 = "실제 보이는 것"과 동형 |
| **GB4** | 타이밍 = **고정-dt 1/60 × 180프레임 = 정확히 3.0s** (벽시계 아님) | 벽시계 3초는 매 실행 상태 달라 재현 불가 → 골든 무용 |
| **GB5** | 상태 = **TitleState** | Director 프리즈(scene actor update 0) + WaveController/physics 미동작 → 비결정 최소 |
| **GB6** | 결정성 = `srand(고정)` + **u_time/dt 를 고정 클럭으로 구동**(render() capture 분기) | gate-0 인벤토리(아래 §3) |
| **GB7** *(default)* | 해상도 = 캡처 시 framebuffer 실제 크기 + 메타 기록. Retina 2× 안정 위해 **`GLFW_COCOA_RETINA_FRAMEBUFFER=FALSE` 로 1280×720 고정 검토** | cross-machine 안정 |
| **GB8** *(default)* | 저장 = `test/golden/golden_{full,no_imgui,skybox}.png` + 메타(해상도·커밋·시드) | Graphics-Testing-Prompt 컨벤션 |
| **GB9** | 브랜치 = **`game/golden-capture`**(7949a65 기반, Track A `game/test-harness` 와 분리) | _MyApp_ 수정 동반(테스트 신규파일과 성격 다름) |

## 2. 아키텍처

### 2.1 capture 모드 트리거 + 루프 (sb7 무수정)

sb7 `run()` 가 루프를 소유하고 `render(glfwGetTime())` 를 호출한다(sb7 immutable, 메모리 `sb7code_immutable`). 따라서 **capture 모드는 sb7 루프를 그대로 쓰되 `render()` 내부에서 분기**:

- 트리거: CLI arg `--capture-golden` (또는 env `SJH_GOLDEN_CAPTURE=1`). `main()`/startup 에서 감지.
- capture 모드 시 `render(currentTime)` 는 **인자 currentTime(벽시계) 무시** → 내부 프레임 카운터 `mCaptureFrame` 로 합성:
  - `dt = 1.0f/60.0f` 고정.
  - `effectiveTime = mCaptureFrame / 60.0f` → u_time 등 모든 시간 입력에 이걸 사용.
  - `mCaptureFrame++`.
- `mCaptureFrame == 180` 도달 시 → **3 변형 캡처**(§2.2) → `glfwSetWindowShouldClose(true)` 로 종료.
- startup 에서 `srand(고정값)` (예 42). TitleState 로 시작(기본).

### 2.2 3 변형 캡처 (프레임 180, 동일 sim 상태)

같은 sim 상태에서 pass 집합만 바꿔 3회 렌더+readback (같은 순간 3장):

| 골든 | 켜는 content pass | 끄는 것 | readback |
|---|---|---|---|
| G1 | Skybox+World+Particle+PostFX+ScreenQuad+ImGui | — | backbuffer |
| G2 | Skybox+World+Particle+PostFX+ScreenQuad | ImGuiPass | backbuffer |
| G3 | Skybox+PostFX+ScreenQuad | WorldPass+ParticlePass+ImGuiPass | backbuffer |

- 토글 = `PassComponent::Enabled` per pass (PostFX 가 이미 사용; SkyboxPass/WorldPass/ParticlePass/ImGuiPass 가 모두 Enabled 를 honor 하는지 **구현 시 검증** — 미honor 면 capture 모드가 per-variant 로 PassIterator 재구성).
- 각 변형: pass enable 설정 → `mPassIterator.Execute` → `glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE)` → **수직 flip**(GL 원점 좌하단) → `stbi_write_png`.

### 2.3 readback 헬퍼 (신규)

- 신규 `apps/_MyApp_/src/Capture/golden_capture.{h,cpp}`: `CaptureBackbufferToPng(path, w, h)` = glReadPixels + vflip + stbi_write_png.
- `STB_IMAGE_WRITE_IMPLEMENTATION` 정의(이 .cpp 단 한 곳; `STB_IMAGE_IMPLEMENTATION`(image.cpp)과 다른 매크로라 충돌 없음 — 메모리 `stb_image_owner_resource_registry` 와 별개). stb_image_write.h 는 vcpkg Stb include 에 포함.

## 3. 결정성 고정 (gate-0 인벤토리 → 적용)

| 원천 | 위치 | 적용 |
|---|---|---|
| u_time (skybox) | `main.cpp:373` | capture 분기에서 `= effectiveTime`(=3.0 @ frame180) |
| rand() (WaveController + **Effekseer 공유 전역**) | `WaveController.cpp:94` / Effekseer | startup `srand(42)` |
| dt 구동 전반 (Box2D/VFX/Sprite/wall uTime) | 다수 | TitleState(Director 프리즈)로 대부분 미동작 + 고정 dt |
| 카메라 view (ActorFollower ease) | `ActorFolower.cpp:157` | TitleState 초기 고정(0,3,6 pitch-30) |
| Effekseer rand | Manager | TitleState 파티클 없으면 N/A; 있으면 `SetRandFunc` 고정 |
| 잔차(GPU float) | — | 본 단계는 캡처만 → noise floor 는 §4 GU2 에서 측정 |

## 4. ★ HANDOFF-UNIT 지도 (메모리 `plan-handoff-boundary-markup`)

| unit | 내용 | green | ctx |
|---|---|---|---|
| **GU0** | capture 모드 scaffold: 트리거 + 고정-dt 180프레임 루프 + srand + golden_capture 헬퍼 → **G1 1장** PNG 생성(육안 OK) | PNG 1장 출력 + 비검정 | 中 |
| **GU1** | 3 변형(PassComponent::Enabled 토글) → **3 PNG** | 3장 출력, 레이어 차이 육안 확인 | 中 |
| **GU2** | 결정성 카나리아: capture **2회** 실행 → 3 PNG **bit-identical**(또는 diff 픽셀 수 = noise floor 측정·기록) | 재현성 확인 | 小 |
| **★ HANDOFF #2** | Phase 2 샘플 완료 → `lossless-handoff` Artifact B(`doc/handoffs/`). **다음 = 비교 게이트(FLIP+ctest, 별도 plan)** | | |

**커밋:** path-scoped(`apps/_MyApp_/...` capture + `test/golden/`), `add -A` 금지, Co-Authored-By 미사용. 빌드 자율(VCPKG_ROOT). worktree 는 lib/macos 심볼릭 링크 필요(메모리).

## 5. 사각지대 / 후속 (별도 plan)

- **비교 게이트(FLIP + ctest)** — 본 단계는 골든 *생성*만. 캡처한 PNG vs 재렌더 비교는 후속.
- **CI / cross-machine** — Retina·드라이버 차이 → noise floor 기반 임계.
- **렌더 리팩토링 검증 실사용** — 골든이 있어야 5-에이전트 render 파이프라인이 시각 회귀를 잡음.
- **G3 PostFX 일관성 재검토** — 현 설계는 3장 모두 PostFX 통과(backbuffer). raw sceneFB(PostFX 전) 골든이 더 유용하면 변경.
