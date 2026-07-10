# WebEditor 리서치 — 종합 인덱스 (5건)

- **날짜**: 2026-06-19
- **방식**: 핸드오프 §4 프롬프트 5건을 **Context7 MCP 직접 호출**(문자 그대로) + Context7 미보유/웹플랫폼 호환성 영역은 핸드오프 규율의 **명시적 fallback 조항**(MDN/공식/GitHub)으로 보완. 각 사실에 출처+버전/날짜, "확정 사실 ↔ 권장/의견" 분리, 불확실 명시.
- **연관 스펙**: `2026-06-19-webeditor-master-data-foundation-design.md`

## 보고서 목록
| # | 파일 | 배치 | 한 줄 결론 |
|---|---|---|---|
| 1 | `01-emscripten-webgpu-imgui-toolchain.md` | A (지금) | `-sUSE_WEBGPU` 제거됨 → **`--use-port=emdawnwebgpu`**, ImGui는 `BACKEND_DAWN` 자동, GLFW=`contrib.glfw3` |
| 2 | `02-emsdk-cmake-browser-io.md` | A (지금) | emsdk `install/activate` + **`if(EMSCRIPTEN)` 가드**, `file://` 불가(CORS)→Python 서버, PUT 1차/IndexedDB 베이스라인 |
| 3 | `03-cpp-wasm-webgpu-ui-landscape.md` | A (지금) | **D8(ImGui) 유지** — 유일한 C++ + emscripten-WebGPU 1급 경로. 변경 트리거 없음 |
| 4 | `04-timeline-sequencer-widgets-FUTURE.md` | B (보류) | ⚠️현재 스펙 반영 금지. ImSequencer+ImCurveEdit 재사용 후보 |
| 5 | `05-shader-hot-reload-editor-FUTURE.md` | B (보류) | ⚠️현재 스펙 반영 금지. ImGuiColorTextEdit+compilationInfo, 실증 레퍼런스 존재 |

---

## 핸드오프 §3 "결과 처리" — 배치 A 반영안

### 스펙 §15 미해결 3종 → 확정값
| 스펙 §15 | 확정값 (근거) |
|---|---|
| emscripten WebGPU 플래그/Dawn 여부 | **`--use-port=emdawnwebgpu`** (구 `-sUSE_WEBGPU` 4.0.18 제거). Dawn 계열 webgpu.h. ImGui `IMGUI_IMPL_WEBGPU_BACKEND_DAWN` 자동. **요구: Emscripten ≥ 4.0.10** [#1] |
| 신형 ImGui 버전 + 플랫폼 백엔드 | 백엔드 = **`contrib.glfw3` 포트**(또는 SDL2 `-sUSE_SDL=2`). 버전 = **release tag 미확정** → 플랜에서 1줄 검증 후 핀 [#1 B2] |
| EMSDK 버전 핀 | `emsdk install <X.Y.Z>`(≥4.0.10). 정확 번호는 `install latest`+`emcc --version`로 확정 [#2 A2/E1] |

### 스펙 갱신 타겟
- **§7.2**: EMSDK 설치/검증 절차(#2 A1) + `if(EMSCRIPTEN)` 가드 정석(#2 A3) 반영.
- **§7.3**: WGPU=emdawnwebgpu(#1 A1), ImGui 백엔드=DAWN+contrib.glfw3(#1 A2/A3), 빌드 플래그(#1 A4) 반영.
- **§7.4 / D10**: `python3 -m http.server`(GET) + PUT 커스텀 핸들러 필요(#2 A4). `file://` 불가 사유=CORS.
- **§8**: PUT 1차 + IndexedDB 드래프트 결정이 호환성상 타당함 확인(#2 B1/B2).
- **D8**: 변경 없음. ImGui 유지 — 사용자 보고 완료, 재논의 트리거 미발생(#3 C1).

### 다음 단계
- 위 §15 확정값을 스펙에 반영 → `/writing-plans`로 구현 플랜 착수.
- 플랜 착수 전 잔여 1줄 검증: **ImGui release tag**(#1 B2), **EMSDK 정확 핀**(#2 E1) — 각 GitHub/실행으로 확정.

## 배치 B 처리
- #4·#5는 **보관만.** S4(타임라인) 착수 시 #4, 먼 미래 셰이더 툴 착수 시 #5를 꺼내 씀. 현재 foundation 스펙/플랜에 **반영 금지**(핸드오프 §5 가드레일).

---

## 도구 사용 메모 (투명성)
- **Context7 직접 사용**: emscripten/emsdk, ImGui/imgui_bundle, RmlUi, Slint, naga, Monaco, ImGuizmo.
- **fallback(web)**: File System Access API·IndexedDB(브라우저 호환성), Qt for WASM·NoesisGUI(상용/이슈트래커), imnodes·ImNodeFlow·ImGuiColorTextEdit·레퍼런스 OSS(Context7 미보유). 핸드오프 "부족할 때만 WebSearch/WebFetch(MDN/공식)" 조항에 부합.
- **미확보(정직 표기)**: NoesisGUI WebGPU/라이선스, ImGuiNeoSequencer 전반, 다수 라이브러리의 LICENSE 원문, 일부 release 절대 날짜. → 추측으로 채우지 않음.
