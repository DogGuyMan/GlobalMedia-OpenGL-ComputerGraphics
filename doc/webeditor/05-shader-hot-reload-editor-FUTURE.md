# 리서치 #5 — 웹/WGPU 셰이더 핫리로드 에디터 (먼 미래 정찰)

> ⚠️ **현재/차기 스펙 반영 금지.** 핸드오프 §0·§5: 본 보고서는 "Hot Reload Shader Debugger(ShaderToy식)" **먼 미래 아이디어**의 가능성 정찰일 뿐이다. foundation 스펙/플랜/S4에 **반영하지 않는다.**

- **날짜**: 2026-06-19
- **상태**: 배치 B (보류) — 먼 미래 정찰
- **도구**: Context7 MCP (`/websites/rs_naga`, `/microsoft/monaco-editor`) + fallback web(MDN WebGPU·WGSL spec·gfx-rs/wgpu·레퍼런스 구현)

---

## A. 확정 사실 (인용 가능)

### A1. 브라우저 WGSL 런타임 재컴파일 + 파이프라인 핫스왑 — 정석 패턴 [must-have]
출처: web — MDN GPUDevice.createShaderModule(2025-12-12), W3C WGSL spec(2026-05-18), toji.dev best-practices
- `device.createShaderModule({ code })` — WGSL 소스 문자열로 `GPUShaderModule` 생성.
- **에러 분류(WGSL spec 확정)**: ① **shader-creation error**(모듈 생성 시, 소스만으로 탐지) ② **pipeline-creation error**(파이프라인 생성 시, entry point 코드 한정) ③ **dynamic error**(실행 중, 탐지 불가능할 수 있음).
- **핫리로드 흐름(확정 패턴)**: WGSL 텍스트 fetch/편집 → `createShaderModule` → (에러 체크) → **렌더 파이프라인 재생성**. (toji.dev: 파일을 plain text로 로드해 `createShaderModule`에 전달하는 게 일반 패턴.)
- **제약(귀결)**: WebGPU 파이프라인 객체는 불변(immutable) → 셰이더 교체 시 **파이프라인을 재생성**해야 함. 컴파일은 비동기. 편집마다 재컴파일은 비용 → 디바운스 필요(레퍼런스들은 300ms 사용).

### A2. WGSL 검증/에러 리포팅 — 브라우저 네이티브 `compilationInfo()` [must-have]
출처: web — MDN createShaderModule, gfx-rs/wgpu #2130·#2674, gpuweb WGSL 미팅(2025-10-07)
- **브라우저 네이티브 경로**: `GPUShaderModule.compilationInfo()` → 컴파일 메시지 배열(에러/경고) 반환. **CompilationInfo는 API에 제출된 WGSL 내 위치(line/col)를 제공.** 브라우저가 WGSL을 strict 파싱 + static validation 수행(브라우저 내부는 Chrome=Tint, Firefox=naga 등이나 web content에 백엔드 비노출).
- → **브라우저에서는 별도 검증기 없이 WebGPU API 자체가 WGSL 에러를 line/col과 함께 표면화.** 이걸 에디터 마커로 매핑하면 됨.

### A3. naga — Rust 셰이더 검증/번역기 (오프라인/네이티브 경로) [optional]
출처: Context7 `/websites/rs_naga` (docs.rs/naga)
- WGSL 파싱: `naga::front::wgsl::parse_str(src)` → `Module` 또는 `ParseError`. 검증: `naga::valid::Validator::new(flags, caps).validate(&module)` → `ModuleInfo`.
- **에러 리포팅(확정)**: `ParseError`가 `message()`, `labels()`(span+설명), `notes()`, **`location(source) -> Option<SourceLocation>`**, **`emit_to_string(source)`**(codespan_reporting 포맷 진단) 제공 → UI 표면화에 직결.
- WGSL↔GLSL 번역: `wgsl-in`+`glsl-out` feature로 WGSL→GLSL 등.
- **C++ 경계 주의(중요)**: naga는 **Rust 크레이트**. WASM 타겟 컴파일은 가능하나, **C++/ImGui 에디터에서 직접 쓰려면 Rust→WASM 모듈 분리 또는 C-API 래핑 필요**. 브라우저에서는 A2(네이티브 compilationInfo)가 더 단순 → naga 직접 사용은 오프라인 검증/번역이 필요할 때로 한정. (naga 브라우저 직접 사용 성숙도는 미확인.)

### A4. 텍스트 에디터 컴포넌트 — C++ 순정 vs DOM/JS 두 갈래
**(a) ImGuiColorTextEdit — C++/ImGui 내장 [권장 경로]**
출처: web — pongasoft WebGPU Shader Toy, ImGuiColorTextEdit repo 언급
- ImGui 위에서 동작하는 **구문 강조 텍스트 에디터 위젯**. DOM/JS 불필요 → **D8이 기피한 C++/JS 경계가 없음.**
- 실증: pongasoft WebGPU Shader Toy가 ImGuiColorTextEdit 사용("automatically moves the cursor to the error" 등). 단 일부 단축키(Ctrl+A/E) 충돌 side effect 언급.

**(b) Monaco Editor — DOM/JS 컴포넌트 [강력하나 경계 재도입]**
출처: Context7 `/microsoft/monaco-editor`
- `monaco.editor.create(div, {value, language})`로 div에 임베드. 커스텀 언어: `monaco.languages.register({id})` + `setMonarchTokensProvider`(WGSL 구문 강조 가능).
- **에러 스퀴글**: `model.onDidChangeContent` → (디바운스) 검증 → **`editor.setModelMarkers(model, owner, markers)`**(severity+line/col+message)로 표시. `getValue()`로 내용 회수.
- VS Code급 IntelliSense/마커가 강점이나 **DOM 컴포넌트** → C++/ImGui WASM 캔버스와 결합 시 **JS↔WASM 경계(EM_JS/ccall) 필요** = D8이 기피한 복잡성 재도입. (CodeMirror도 동일 성격.)

### A5. 레퍼런스 OSS (확정)
| 프로젝트 | 스택 | 비고 | 출처 |
|---|---|---|---|
| **pongasoft WebGPU Shader Toy** | **C++ + ImGui + ImGuiColorTextEdit + Dawn/WebGPU (브라우저/WASM)** | **#5가 구상한 바로 그 스택의 레퍼런스 구현.** WGSL 편집→즉시 컴파일/피드백, 커서 자동 에러 이동, Dawn v20251002로 갱신, Codeberg 이전 | pongasoft.com/webgpu-shader-toy |
| hideyuki-hori/lab-webgpu-editor | **JS/TS** + CodeMirror 6 + WebGPU + OffscreenCanvas+Worker | ShaderToy식, 300ms 디바운스, 라이브 로그, One Dark. JS 스택 레퍼런스 | github.com/hideyuki-hori/lab-webgpu-editor |
| toji.dev WebGPU best-practices | (가이드) | dynamic shader construction 패턴 | toji.dev/webgpu-best-practices |

---

## B. 권장 (의견 — 확정 아님)

### B1. C++/ImGui/WASM 노선이면 **ImGuiColorTextEdit + 네이티브 compilationInfo가 정석**
- 텍스트 에디터: **ImGuiColorTextEdit**(C++ 순정, DOM 경계 없음). Monaco/CodeMirror는 IntelliSense가 강하나 JS 경계를 다시 끌어옴 → 본 프로젝트 철학(D8)과 충돌.
- 검증/에러: 브라우저 **`compilationInfo()`**(line/col 포함)를 에디터 마커로 매핑. naga 별도 도입 불요(브라우저 네이티브가 처리).
- 핫스왑: 편집 디바운스(~300ms) → `createShaderModule` → compilationInfo 체크 → 파이프라인 재생성.
- **결정적**: pongasoft WebGPU Shader Toy가 이 조합의 **실증 레퍼런스** → 먼 미래에 착수해도 길이 닦여 있음.

### B2. naga는 "오프라인/번역"용으로만
- 브라우저 런타임 검증은 네이티브가 더 단순. naga는 **빌드타임 WGSL 검증**이나 **GLSL→WGSL 번역**이 필요할 때만(C++ 경계 비용 감안).

### B3. 난이도/리스크 (의견)
- 난이도 中: 핫스왑 자체는 정석 패턴 + 레퍼런스 존재. 리스크는 ImGuiColorTextEdit 단축키 충돌, 파이프라인 재생성 비용, WebGPU 브라우저 가용성(아래 C).

---

## C. 불확실 / 후속 검증 (거짓 확신 금지)
1. **WebGPU 브라우저 가용성** — 레퍼런스 기준 Chrome/Edge 113+, Firefox는 Nightly 언급. 본 항목은 먼 미래라 착수 시점에 caniuse 재확인 필수(상황 변동 큼).
2. **ImGuiColorTextEdit / pongasoft 프로젝트 라이선스·유지보수** — LICENSE 직접 미확인. (ImGuiColorTextEdit는 MIT 통설이나 확정 아님. pongasoft는 Codeberg 이전.)
3. **naga 브라우저 직접 사용 성숙도** — Rust→WASM/C-API 래핑 경로의 실증 미확인.
4. **wgpu(gfx-rs) compilationInfo 구현 완성도** — gfx-rs/wgpu #2130·#2674가 "단일 에러 vs spec의 메시지 배열" 정렬을 논의(2021~2022 이슈). 네이티브 wgpu 경로를 쓸 경우 최신 상태 재확인 필요. (브라우저 네이티브 경로는 영향 적음.)
5. **소스맵/transpile 에러 매핑** — gpuweb 논의 진행 중(2025-10). 트랜스파일러를 끼우면 에러 위치 매핑에 추가 작업 필요.
