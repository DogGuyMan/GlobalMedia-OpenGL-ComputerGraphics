# GrayScale + Vignetting PostFX 패스 — Design Spec

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **대상**: `apps/_MyApp_` PostFX 체인. 브랜치 `game/module/ingame/temp`.
> **작성**: 2026-06-01. brainstorming 2 결정 확정.
> **선행**: depth-based fog SP(2026-05-31) + PostFX 순서 재배열(2026-06-01).

---

## 0. 목표
- 사용자 작성 `grayscale_vignetting.fs`(grayscale + vignetting 결합)를 PostFX 패스로 정식 합류.
- ImGui 에 **"Health"[0,1] 슬라이더 → grayscale 강도** + **Vignette Color(필수) + amount** 독립 컨트롤.

## 1. 확정 결정 (brainstorming)
| # | 결정 | 비고 |
|---|---|---|
| D1 | **Health = ImGui debug 슬라이더** [0,1] → `uGrayscaleAmount` 직접 구동 (1=원본색, 0=무채색). | 실제 Player HP 미연결 — 요청이 ImGui 중심. 추후 HP 연결은 확장 여지. |
| D2 | 체인 위치 = **fog 뒤 (idx 4, invert 앞)**. | 최종 순서: gamma→sharpening→bloom→fog→**grayscale_vignetting**→invert→blur→sobel. |
| D3 | **Vignette 는 grayscale 과 독립** — 자체 Color(`uVignetteColor`, ColorEdit3) + amount(`uVignetteAmount`, slider). Health 무관. | 셰이더가 단일 패스로 둘 다 수행하되 uniform 독립. |

## 2. 변경 분해

### T1 — 셰이더 컨벤션 정리
파일: `apps/_MyApp_/resources/shaders/postprocess/grayscale_vignetting.fs` (이미 존재 — 로직 유지).
- `uniform sampler2D uScene;` 에 `// SP4 컨벤션` 주석 (다른 PostFX 와 통일).
- uniform 주석 스타일을 fog/bloom 과 정렬. 로직(`uGrayscaleAmount` mix + `uVignetteColor` edge mix) 불변.
- 셰이더는 "Health" 를 모름 — `uGrayscaleAmount` 만 받음(관심사 분리).

### T2 — 패스 등록 + startup 초기화
파일: `apps/_MyApp_/main.cpp`.
- `POSTFX_PROGRAM_CONFIGS` idx 4(fog 뒤)에 삽입:
  ```cpp
  {"grayscale_vignetting", "...postprocess.vs", "...grayscale_vignetting.fs",
   {{"uGrayscaleAmount", 1.0f}, {"uVignetteAmount", 0.5f}}},
  ```
- `uVignetteColor`(vec3)는 InitFloats(float-only) 밖 → **startup 에서 명시 set** (fog `uFogColor` 패턴). 기본 `vec3(0,0,0)`(검정 비네팅). `props.Vec3s["uVignetteColor"]` 에 set → "반드시 특정 Color 를 받아" 보장.
- **DRY**: `FindFogMaterial()` → `FindPassMaterial(const char* name)` 일반화. `FindFogMaterial()` = `FindPassMaterial("fog")` 얇은 래퍼. startup 에서 fog init + grayscale_vignetting init 둘 다 일반 헬퍼 사용. (render() 의 uInverseProjection·RebindFogUniforms 는 `FindFogMaterial()` 그대로 — 회귀 0.)

### T3 — ImGui 컨트롤
파일: `<apps>/_MyApp_/src/UI/PostFXDebugLayer.cpp` `OnBuildUI()` — `else if (entry.Name == "grayscale_vignetting")`:
```cpp
ImGui::SliderFloat("Health##gv",    &props.Floats["uGrayscaleAmount"], 0.0f, 1.0f); // Health 1=색, 0=무채색
ImGui::SliderFloat("vignette##gv",  &props.Floats["uVignetteAmount"],  0.0f, 1.0f);
ImGui::ColorEdit3 ("vig color##gv", &props.Vec3s["uVignetteColor"][0]);             // 별도 Color (Health 무관)
```
- `props.Floats[...]` operator[] 는 InitFloats 가 채워둔 값 사용(uGrayscaleAmount=1, uVignetteAmount=0.5). uVignetteColor 는 startup init 값 사용.

### T4 — 빌드 + 시각 검증
- Health↓ → 화면 탈색. vig color 변경 → 테두리 색 변화. vig amount↑ → 비네팅 강화. fog 와 공존.

## 3. 데이터 흐름
```
fog 출력 → grayscale_vignetting.uScene
ImGui Health[0,1]  → Floats[uGrayscaleAmount] ─┐
ImGui vig amount   → Floats[uVignetteAmount]  ─┼→ PropertyBlockSetter 자동 송신
ImGui vig color    → Vec3s [uVignetteColor]   ─┘
→ invert → blur → sobel → backbuffer
```

## 4. 비목표
- 실제 Player HP 연결 (D1 — 별도). emissive bloom (Phase 2 — 무관).
- 단위 테스트 (프로젝트 정책 — 시각 검증이 수용 기준).

## 5. 검증
| # | 조작 | 기대 |
|---|---|---|
| G1 | F1 → PostFXDebug → grayscale_vignetting Health 슬라이더 1→0 | 화면 점진적 탈색 |
| G2 | vig color ColorEdit3 변경 | 화면 테두리에 해당 색 비네팅 |
| G3 | vig amount 0→1 | 비네팅 영역 확대/강화 |
| G4 | fog ON 동시 | fog→grayscale_vignetting 순차 적용, 깨짐 없음 |
| G5 | 패스 OFF 토글 | 해당 효과만 bypass |

**spec 끝.**
