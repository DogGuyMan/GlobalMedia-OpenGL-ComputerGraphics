# 리서치 #3 — Emscripten + WebGPU C++ UI 프레임워크 지형도 (D8 재검증)

- **날짜**: 2026-06-19
- **상태**: 배치 A (지금 유용) — ImGui 결정(D8) 재확인 / 재사용 결정
- **도구**: Context7 MCP (`/pthom/imgui_bundle`, `/mikke89/rmlui`, `/websites/slint_dev`) + fallback web(Qt 공식 문서·QTBUG, NoesisGUI는 1차 출처 미확보)
- **핵심 판정 기준**: ① Emscripten+WebGPU 빌드 가능 여부(1순위) ② C++ 1급 지원 ③ 라이선스 ④ 폼+2D 프리뷰 적합성 ⑤ WASM 바이너리 부담
- **가드레일 준수**: 핸드오프 §5 — "더 나은 후보가 나와도 사용자 보고 후 D8 재논의, 임의 변경 금지". 본 보고서는 보고일 뿐 변경 아님.

---

## A. 후보별 확정 사실 (인용 가능)

### A1. Dear ImGui (raw `imgui_impl_wgpu` + emdawnwebgpu) — **WebGPU 1급 지원** [must-have 충족]
- WebGPU 백엔드 사실은 **리서치 #1에서 확정**: emscripten 4.0.10+에서 `--use-port=emdawnwebgpu` + `IMGUI_IMPL_WEBGPU_BACKEND_DAWN` 자동. 공식 `example_glfw_wgpu`/`example_sdl2_wgpu` 존재.
- 언어: C++ 네이티브. 라이선스: **MIT** (ImGui 본체).
- 적합성: immediate-mode가 스키마 구동 폼(InputInt/SliderFloat/Checkbox/InputText)에 직결 — 스펙 §9 위젯 매핑과 1:1.
- 출처: ImGui examples/CHANGELOG (리서치 #1 A1~A4 참조).

### A2. Dear ImGui Bundle — 풍부한 add-on, 단 **웹 렌더는 WebGL** [common]
출처: Context7 `/pthom/imgui_bundle` (`llms.txt`, `key_features.md`, `CMakeLists.txt`)
- 크로스플랫폼(Windows/Linux/macOS/iOS/Android/**WebAssembly**), C++(Emscripten)/Python(Pyodide).
- **결정적 뉘앙스**: 웹에서 "native-speed rendering via **WebGL**"이라 명시 — 번들의 기본 웹 경로는 **WebGL이지 WebGPU가 아니다.**
- 번들 add-on(`IMGUI_BUNDLE_WITH_*`): Hello ImGui, ImmApp, ImmVision, **ImPlot/ImPlot3D**, **ImGuiNodeEditor**, ImGuiMd, ImFileDialog, ImGuiTexInspect, NanoVG, **ImGuizmo(ImGuiZmo)**, MicroTex (전부 기본 ON).
- 라이선스: 구성요소별 상이(본체 ImGui는 MIT). **번들 전체 라이선스는 본 조회 미반환 → 확인 필요(불확실).**

### A3. RmlUi — HTML/CSS 기반, **공식 WebGPU 백엔드 없음** [optional]
출처: Context7 `/mikke89/rmlui` (`readme.md`, `Backends/CMakeLists.txt`, `Tests/CMakeLists.txt`)
- 제공 렌더러(확정): **OpenGL 2, OpenGL 3, Vulkan, SDL GPU, SDL renderer, DirectX 12**. → **WebGPU 전용 백엔드는 목록에 없음.**
- Emscripten 경로: `SDL_GL3` 백엔드가 emscripten에서 `OpenGL::GL` 링크 → 웹에서 **GL3/WebGL**로 렌더. (`--preload-file`로 데이터 프리로드)
- 모델: `Rml::RenderInterface`로 vertices/indices/draw command를 앱이 직접 렌더 → **커스텀 WebGPU RenderInterface 작성은 이론상 가능**하나 공식 제공 없음.
- HTML/CSS(RML/RCSS) + data binding(`CreateDataModel`/`Bind`) — 폼 UI에 강점.
- 라이선스: **MIT(통설)** — 단 본 조회 미반환(불확실).

### A4. Slint — WebGPU 백엔드 존재하나 **unstable + Rust 우선** [optional]
출처: Context7 `/websites/slint_dev` (docs.slint.dev wgpu_27/28, slint_interpreter)
- WebGPU 지원: `Backends::BROWSER_WEBGPU` 존재, `new_instance_with_webgpu_detection`로 **WebGPU 미지원 시 WebGL 자동 폴백**. 단 crate feature가 **`unstable-wgpu-27`/`wgpu-28`** = 불안정 표식.
- 렌더러: `renderer-femtovg`(기본), `renderer-femtovg-wgpu`, `renderer-skia`(OpenGL/Vulkan), `renderer-software`.
- **결정적 뉘앙스**: 확보된 WGPU 통합 예제·API(`slint::wgpu_27`, `require_wgpu_27`, `set_rendering_notifier`, `GraphicsAPI::WGPU27`)가 전부 **Rust**다. Slint의 1급 언어는 Rust이며 C++ 바인딩은 존재하나, **C++에서의 WGPU 렌더링 통합 성숙도는 본 조회로 미확인.**
- 라이선스: **본 조회 미반환.** (일반적으로 Slint는 GPL/로열티프리/상용 다중 라이선스로 알려져 있어 — **MIT 아님, 채택 전 라이선스 정밀 확인 필수**.)

### A5. Qt for WebAssembly — **WebGL 고정, WebGPU 미지원** [optional, 부적합]
출처: web fallback — Qt 6.11 공식 문서(doc.qt.io/qt-6/wasm.html), Qt Wiki, QTBUG-75040
- **고정 WebGL 요구**: 앱이 GL을 안 써도 Qt WASM은 WebGL 필수. 최고 가용 WebGL(보통 WebGL 2) 사용.
- **WebGPU 미지원**: "Web Assembly RHI backend for WebGPU/GPUWeb"는 **미해결 버그(QTBUG-75040)**. 현재 WASM 경로에 WebGPU 없음.
- 바이너리: Qt WASM 모듈은 **무겁다**(단순 UI도). Safari가 Qt 크기 wasm 모듈을 못 다룬 이력.
- qmake 타겟명: `emscripten { }`.
- 라이선스: Qt는 **GPL/LGPL/상용 다중** — 상용·정적링크 시 라이선스 부담 큼.

### A6. NoesisGUI — **확인 불가(거짓 확신 금지)** [판정 보류]
- NoesisGUI는 XAML 기반 상용 UI 라이브러리로 알려져 있으나, **Emscripten+WebGPU 지원 여부 및 현재 라이선스 조건의 1차 출처를 본 조회에서 확보하지 못함.**
- 판정: **"미확인"** — 진지 검토가 필요하면 NoesisGUI 공식 문서/라이선스 페이지 직접 확인 필요. 상용 라이선스 비용이 전제이므로 OSS 우선 정책이면 후순위.

---

## B. 비교표

| 프레임워크 | 웹 렌더 | **WebGPU(emscripten)** | C++ 1급 | 라이선스 | WASM 부담 | 폼+프리뷰 적합 |
|---|---|---|---|---|---|---|
| **Dear ImGui (raw wgpu)** | **WebGPU** | ✅ **1급**(#1 확정) | ✅ | **MIT** | **작음** | ★★★ |
| ImGui Bundle | WebGL | △ 번들 경로는 WebGL, raw WGPU는 DIY | ✅ | 혼합(본체 MIT) | 중 | ★★ (add-on 생태계 ◎) |
| RmlUi | GL3/WebGL | ✗ 공식 없음(커스텀 가능) | ✅ (HTML/CSS) | MIT(미확인) | 소~중 | ★★ (HTML/CSS 폼) |
| Slint | FemtoVG/Skia | △ 있으나 **unstable**+Rust우선 | △ 바인딩 | **다중(GPL/상용)** | 중 | ★★ |
| Qt for WASM | WebGL2 고정 | ✗ (QTBUG-75040) | ✅ | **GPL/상용** | **큼** | ★ |
| NoesisGUI | XAML | ? 미확인 | ✅ | **상용** | 중 | ? |

---

## C. 권장 (의견 — 확정 사실 아님)

### C1. **최종 추천: Dear ImGui (raw `imgui_impl_wgpu` + emdawnwebgpu) 유지 → D8 변경 없음**
근거:
1. **유일하게 C++ + emscripten WebGPU를 1급으로 지원**하는 깔끔한 경로(#1 확정). 나머지 후보는 WebGPU가 없거나(RmlUi/Qt), unstable+Rust우선(Slint), 또는 WebGL 경로(ImGui Bundle).
2. **MIT** — 라이선스 리스크 0. (Slint/Qt는 다중·상용 라이선스로 정밀 검토 부담, NoesisGUI는 상용.)
3. **immediate-mode가 스키마 구동 폼에 직결** — 스펙 §9 위젯 매핑이 ImGui API 그대로.
4. **WASM 바이너리 가장 가벼움** — Qt와 정반대.
5. WGPU 1차 용도(ImGui 렌더 + 샘플 스프라이트 1장)에 정확히 부합.

→ **핸드오프 §5 가드레일 충족: 더 나은 후보 없음. D8 재논의 트리거 발생하지 않음.** (사용자 보고만 하고 결정은 유지.)

### C2. 차순위/미래 옵션 (의견)
- **add-on이 필요해지면(S4 등): ImGui Bundle 생태계를 raw WGPU 백엔드 위에 부분 차용** 고려. 특히 **ImGuizmo의 ImSequencer / ImGuiNodeEditor**가 S4 타임라인(리서치 #4)과 직결. 단 번들의 기본 웹 경로는 WebGL이므로, 번들을 통째로 쓰기보다 **add-on 소스만 raw WGPU 빌드에 합치는 방식**이 WebGPU 요구와 정합.
- **RmlUi**는 "HTML/CSS로 폼을 선언적으로" 짜고 싶을 때의 대안이나, WebGPU RenderInterface를 직접 써야 하고 D8이 이미 HTML/DOM 오버레이를 기각한 맥락과 충돌 → 후순위.

### C3. 명백한 부적합 (의견)
- **Qt for WebAssembly**: WebGPU 없음 + 무거운 바이너리 + 라이선스 부담. 본 용도에 부적합.
- **Slint(C++)**: WGPU가 unstable+Rust우선 + 다중 라이선스. C++/ImGui 친숙 팀엔 과한 베팅.

---

## D. 스펙 반영안 (핸드오프 §3 배치 A 처리 — #3)

| 항목 | 결론 |
|---|---|
| D8 (ImGui 채택) 재확인 | **유지(변경 없음).** ImGui = 유일한 C++ + emscripten-WebGPU 1급 경로. 사용자 보고만, 재논의 트리거 없음 |
| 재사용 vs 직접 구현 | UI 프레임워크 자체는 **ImGui 재사용**. 폼 렌더러는 스키마 구동으로 자작(§9). add-on은 필요 시점에 차용(C2) |
| S4 연계 메모 | ImGuizmo ImSequencer / ImGuiNodeEditor가 S4 타임라인 후보 → 리서치 #4에서 정밀화 |

---

## E. 불확실 / 후속 검증 (거짓 확신 금지)

1. **ImGui Bundle / RmlUi / Slint 정확한 라이선스 문구** — Context7 미반환. 채택 후보로 격상 시 각 LICENSE 직접 확인. **특히 Slint는 MIT 아님(다중 라이선스) — 반드시 확인.**
2. **NoesisGUI WebGPU/WASM 지원·라이선스** — 1차 출처 미확보. "미확인"으로 보류.
3. **Slint C++ 바인딩에서의 WGPU 통합 성숙도** — 확보 예제가 전부 Rust. C++ 경로 실증 필요.
4. **RmlUi `SDL GPU` 백엔드의 브라우저 WebGPU 매핑 여부** — SDL_GPU 추상화가 emscripten에서 WebGPU로 가는지 미확인. 현재로선 "공식 WebGPU 백엔드 없음"으로 판정.
5. Qt QTBUG-75040 최신 상태 — 본 출처(2024 이슈 참조) 이후 진척 가능성 있으나, 현재까지 "WASM WebGPU 미지원"으로 판정. 필요 시 버그트래커 직접 확인.
