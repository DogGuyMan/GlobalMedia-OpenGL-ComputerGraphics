# 재개 핸드오프 — 골든 이미지 캡처 (Track B Phase 2) 완료

> ⚠️ **2026-07-26 정정 (원문 보존)** — 본 문서에 나오는 `SJH_GOLDEN_CAPTURE=1 ./_MyApp_` 실행과 `ctest --test-dir build_ninja -R golden` 은 *당시* 절차이며 현재는 **폐기**됐다. 골든 캡처가 런타임 환경 변수 -> 컴파일 정의로 바뀌어 프리셋 `ninja-golden`(빌드 디렉토리 `build_ninja-golden`) 전유가 됐고, 게임 빌드의 `_MyApp_` 는 골든을 캡처하지 않는다(실행해도 창만 뜨는 조용한 실패). 현행 절차 = `test/CLAUDE.md`. 아래 본문은 당시 기록으로 그대로 둔다.

> **작성** 2026-06-25 · **재측정** branch `game/golden-capture` @ `34f1f55`.
> ⚠ `doc/` = .gitignore 로컬 (same-machine 세션 연속성용). 단일 진입점.

## TL;DR + 다음 액션

**Phase 2 Track B (골든 이미지 샘플 생성) 완료.** `_MyApp_` 에 capture 모드를 넣어 **고정-dt 180프레임(=3.0s) 결정적 시뮬 후 3 레이어 변형을 backbuffer readback → 3 골든 PNG** 생성. **bit-결정적**(2회 실행 MD5 동일) + **사용자 육안 승인 완료**.

**다음 중 택1:**
1. **브랜치 통합** — `game/golden-capture` → `game/main` (사용자 직접). worktree=`.claude/worktrees/golden-capture`.
2. **비교 게이트(FLIP+ctest, 별도 plan)** — 골든 vs 재렌더 자동 비교 + ctest 통합. ⭐ noise floor=0 이라 same-machine 은 bit-exact(memcmp) 가능, cross-machine 만 FLIP.

## State (재측정)

- branch `game/golden-capture` (base 7949a65=oracle), tip `34f1f55`. 4 커밋(전부 path-scoped, `[capture]`, Co-Authored-By 없음):
  | SHA | 내용 |
  |---|---|
  | `81b7177` | GU0 capture 모드 scaffold (고정-dt 180프레임 + readback, G1) |
  | `feb6b09` | GU1 3 레이어 변형 (G1/G2/G3) |
  | `15fe086` | GU1-fix G1 에서 PostFXDebugLayer 제외 |
  | `34f1f55` | 골든 PNG 3장 (`test/golden/golden_{full,no_imgui,skybox}.png`, 2560×1440) |

## 산출물 (3 골든 — 육안 승인 + bit-결정적)

| 파일 | 레이어 | MD5 |
|---|---|---|
| `test/golden/golden_full.png` | Skybox+Scene+ImGui(PostFXDebug 제외) | `0a37e866…` |
| `test/golden/golden_no_imgui.png` | Skybox+Scene | `63886a2d…` |
| `test/golden/golden_skybox.png` | Skybox only (matrix rain) | `c50987b5…` |

## 구현 (capture 모드)

- **트리거**: env `SJH_GOLDEN_CAPTURE=1` (sb7 argv 비의존). → `srand(42)` + capture 모드.
- **고정-dt**: `render()` 가 벽시계 currentTime 무시, `mCaptureFrame/60` 기반(dt=1/60). u_time=effectiveTime(skybox 결정화). 프레임 180에서 캡처 후 `glfwSetWindowShouldClose`.
- **3 변형**(프레임 180 같은 상태): G1=전체 Execute / G2=`DebugPassIndex`로 ImGuiPass 스킵(ScreenQuad 까지 실행→backbuffer) / G3=임시 PassIterator(Skybox+PostFX+ScreenQuad). 3장 모두 PostFX 거쳐 backbuffer readback.
- **PostFXDebug 제외**(G1): `GameUiDeps.outPostFXDebugLayer` out-ptr → G1 캡처 직전 `Enabled=false`→재RenderAll→캡처→복원. (다른 ImGui 유지.)
- **readback**: `apps/_MyApp_/src/Capture/golden_capture.{h,cpp}` — glReadPixels+수직flip+stbi_write_png(`STB_IMAGE_WRITE_IMPLEMENTATION` 단일). 런타임 출력=`build_ninja/apps/_MyApp_/test/golden/`.
- 변경 파일: `main.cpp`(capture 분기), `src/Capture/*`(신규), `src/UI/UiBootstrap.{h,cpp}`(out-ptr), `CMakeLists.txt`.

## 결정 로그 (spec GB1~GB9)

GB1 범위=3 골든 생성 / GB2 capture 모드 in _MyApp_ / GB3 3 레이어 peel(전부 PostFX→backbuffer) / GB4 고정-dt 180프레임=3.0s / GB5 TitleState / GB6 srand+고정클럭 / GB7 해상도=실제 framebuffer(2560×1440 Retina) / GB8 저장 test/golden/ / GB9 별도 브랜치.
정본 spec: `doc/superpowers/specs/2026-06-25-golden-image-capture-design.md` (gitignore 로컬).

## 발견

1. **bit-결정성 달성** — 고정-dt + srand + u_time 핀 + TitleState 로 noise floor=0(same-machine). 비교를 bit-exact 로 할 수 있음.
2. GUI _MyApp_ 가 서브에이전트/bash 에서 실행 가능(헤드리스 우려 없었음, 보이는 창 잠깐 뜨고 자동 종료).
3. `GLFW_TRUE` → `1` 대체(sb7 vendored GLFW 3.0.4 호환).

## Guardrails

- **sb7 무수정**(메모리 sb7code_immutable) — capture 는 render() 분기로만. 게임 로직 무변경.
- 커밋 path-scoped, Co-Authored-By 미사용, 주석 한국어.
- worktree 는 gitignored `lib/macos` 심볼릭 링크 필요(이미 박힘).
- ⚠ 골든 PNG raw 커밋(~7.6MB) — 스케일 시 Git LFS 검토(`*.png filter=lfs`).

## 범위 밖 (별도 plan)

- **비교 게이트(FLIP+ctest)** · **CI/cross-machine 정규화** · **5-에이전트 render 파이프라인 실사용**(시각 회귀 검증) · G3 PostFX 일관성 재검토(현재 3장 모두 PostFX 통과).

## Change log

- 2026-06-25: GU0~GU2 + 골든 커밋 완료. G2/G3 1차 육안 OK, G1 은 PostFXDebug 제외 후(GU1-fix) 육안 승인. GU2 결정성 카나리아 = 3장 bit-identical.
