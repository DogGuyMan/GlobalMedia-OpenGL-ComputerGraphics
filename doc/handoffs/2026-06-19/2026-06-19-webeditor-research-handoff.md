# WebEditor — 리서치 핸드오프 (다른 Claude Agent 위탁용)

- **날짜**: 2026-06-19
- **작성 목적**: WebEditor(WASM+WebGPU+Emscripten) 설계를 *구현 플랜*으로 구체화하기 전에 필요한 **외부 리서치 5건**을, 컨텍스트 없는 다른 Claude Agent가 단독 실행할 수 있도록 패키징.
- **연관 스펙**: [doc/superpowers/specs/2026-06-19-webeditor-master-data-foundation-design.md](../../doc/superpowers/specs/2026-06-19-webeditor-master-data-foundation-design.md)
- **이 문서가 하는 일**: §4의 프롬프트 5개를 각각 복붙해 에이전트에 던지면 됨. §2는 모든 에이전트가 공유할 레퍼런스, §3은 실행 순서/결과 처리 플랜.

---

## 0. 사용법 (한눈에)

1. **지금 실행 권장 = #1·#2·#3** (서로 독립 → 병렬). foundation 스펙을 확정한다.
2. **#4·#5는 미래(S4+) 정찰** — 결과를 *현재 스펙에 반영 금지* (프롬프트에 가드 박음). 급하지 않으면 보류.
3. 각 프롬프트는 자립적이라 **그대로 한 블록 복사** → 새 세션에 붙여넣기 → 실행.
4. 결과 회수 후 처리 = §3 참조.

---

## 1. 배경 (자립 컨텍스트 — 리서처가 알아야 할 전부)

**무엇을 만드나**: 기존 네이티브 C++17 OpenGL 게임(`_MyApp_`)의 튜닝 상수(`Constants.h`의 `constexpr`)를 **JSON 마스터 데이터로 외부화**하고, 그걸 편집하는 **브라우저 데이터 에디터**를 만든다. 에디터는 게임과 **독립된 빌드 타겟**(`WebEditor`)이며 스택은 **Emscripten + WebGPU + Dear ImGui**.

**확정된 결정 (재논의 대상 아님)**:
- 게임은 JSON을 **런타임 로딩**(재컴파일 없는 밸런스 패치). 파서는 nlohmann/json(헤더온리).
- 스키마 SSOT = `*.schema.json`(JSON Schema 부분집합) → 에디터가 스키마 구동 폼 자동 생성.
- 런타임 데이터 거주 = `SJH::ResourceRegistry`에 `DataTable` 자원으로.
- 첫 스펙 범위 = 밸런스 4종(Entity/Stage/Physics/Audio), Stage만 사용처 완전 전환.
- 파일 I/O = `IDataStore` 포트 + 어댑터(정적서버 fetch GET / IndexedDB 드래프트 / Save PUT·FSAccess). 추후 CRUD 서버는 어댑터 교체.
- 에디터 UI = Dear ImGui (단, **게임의 ImGui v1.53 핀과 무관한 독립 사본** — 에디터는 GLFW 3.0.4 미의존).
- WGPU 1차 용도 = ImGui 렌더 백엔드 + 샘플 스프라이트 1장. **라이브 게임 프리뷰/타임라인 sim은 S4(미래)**.

**리서치가 필요한 이유**: 위 스택의 *정확한 버전/플래그/백엔드 조합*은 빠르게 바뀌는 영역(특히 emscripten의 WebGPU 지원이 내장 `webgpu.h`→`emdawnwebgpu` 포트로 이동 중일 가능성)이라, 학습데이터 추정으로 플랜을 짜면 위험. 그래서 *검증된 사실*만 외부에서 수집한다.

---

## 2. 필요 레퍼런스 (모든 리서처 공통)

**프로젝트 사실**:
- 언어/빌드: C++17, CMake 멀티프리셋(`ninja`/`msvc`). vcpkg 미사용 — 사전 빌드 lib + 헤더온리 벤더링(`include/`).
- 게임 본체는 네이티브 전용 의존(FMOD/Box2D/Effekseer/sb7/GLFW 프리빌드)을 가짐 → **이것들은 WASM 빌드 불가**. 에디터는 절대 이 의존을 끌어오면 안 됨(별도 타겟).
- 게임 ImGui는 **v1.53 핀**(GLFW 3.0.4 호환 마지막 태그). 에디터는 이 제약과 무관(별도 사본 허용).
- 대상 OS: macOS + Windows 양쪽 개발.

**도구 우선순위 (엄수)**:
- 라이브러리/프레임워크/표준 API 문서 = **Context7 MCP 우선** (`resolve-library-id`로 `/org/project` 찾기 → `query-docs`에 질문 전체 전달). 부족할 때만 WebSearch/WebFetch(공식 문서/릴리스 노트/MDN/GitHub README).
- 절대 학습데이터 단독 추정으로 빈칸을 메우지 말 것.

**공통 산출물 규칙 (모든 프롬프트가 요구함)**:
- 마크다운 보고서. 각 항목에 **버전/날짜 + 출처 URL** 명시.
- **"확정 사실"(인용 가능) 과 "권장/의견" 을 시각적으로 분리**.
- 기능은 **must-have / common / optional** 로 분류(여러 레퍼런스가 공유할수록 must-have).
- 상충·불확실 지점은 솔직히 "불확실"로 표기(거짓 확신 금지).
- **코드 작성 금지** (리서치 산출물은 사실 보고서이지 구현이 아님).

**선택적 깊이(있으면 좋음, 없어도 실행 가능)**: 위 연관 스펙 파일, 그리고 `apps/_MyApp_/src/*/Constants.h` 8종(외부화 대상 원본).

---

## 3. 리서치 실행 플랜

| 배치 | 프롬프트 | 목적 | 시점 | 의존 |
|---|---|---|---|---|
| **A (병렬)** | #1 Emscripten+WebGPU+ImGui 툴체인 | 스펙 §15 미해결 확정 (가장 위험) | 지금 | 없음 |
| **A (병렬)** | #2 EMSDK 설치+CMake 프리셋+브라우저 I/O | 빌드/배포 절차 확정 | 지금 | 없음 |
| **A (병렬)** | #3 Emscripten+WebGPU C++ UI 프레임워크 지형도 | ImGui 확정 재검증 / 재사용 결정 | 지금 | 없음 |
| **B (보류)** | #4 타임라인/시퀀서 재사용 위젯 | S4 Playable 에디터 정찰 | 미래 | S4 착수 전 |
| **B (보류)** | #5 웹/WGPU 셰이더 핫리로드 | 먼 미래 정찰 | 먼 미래 | — |

**배치 A 결과 처리**:
1. #1·#2 결과 → 스펙 §15의 3개 미해결 항목(emscripten WGPU 플래그 / 신형 ImGui 버전·백엔드 / EMSDK 핀)을 *확정값*으로 치환. 스펙 §7.3·§7.2 갱신.
2. #3 결과 → ImGui 결정(D8)이 여전히 최선인지 재확인. 더 나은 후보가 나오면 사용자에게 보고 후 D8 재논의(임의 변경 금지).
3. 위 반영 끝 → `/writing-plans`로 구현 플랜 작성 착수.

**배치 B 결과 처리**: 보관만. S4 스펙 착수 시점에 꺼내 쓴다. 현재 foundation 스펙/플랜에 **반영 금지**.

---

## 4. 프롬프트 (각각 단독 복붙 실행 — 자립형)

> 각 블록은 컨텍스트 없는 새 세션에서 그대로 실행 가능하도록 핵심 컨텍스트를 인라인했다.

### 프롬프트 #1 — Emscripten + WebGPU + ImGui 툴체인 (지금 필수)

```
역할: C++/WASM 빌드 환경 리서처. 코드 작성 금지, 검증된 사실만 수집·보고.

컨텍스트: 기존 네이티브 CMake C++17 프로젝트에 독립 "WebEditor" 타겟을 추가한다.
스택 = Emscripten + WebGPU + Dear ImGui. 게임 본체(FMOD/Box2D/Effekseer 등 네이티브
의존)는 건드리지 않고, 에디터는 그 의존을 일절 링크하지 않는다. 에디터 ImGui는 게임의
v1.53 핀과 무관한 별도 사본을 써도 된다(에디터는 GLFW 3.0.4 미의존).

2025년 현재 기준으로 다음을 확정하라(각 항목에 버전/날짜 명시):
1. Emscripten에서 WebGPU를 쓰는 "현재 권장" 방식. 구 -sUSE_WEBGPU(내장 webgpu.h)가
   deprecated 되고 emdawnwebgpu 포트로 이동했는지? 정확한 빌드 플래그/링크 옵션은?
2. Dear ImGui의 WebGPU 백엔드(imgui_impl_wgpu)가 기대하는 WebGPU 헤더가 emscripten
   내장인지 Dawn인지. emdawnwebgpu와의 호환 상태.
3. ImGui 공식 example_glfw_wgpu / emscripten 예제가 쓰는 플랫폼 백엔드 조합(GLFW
   emscripten 포트 vs SDL2/SDL3)과 권장 ImGui 버전.
4. 위 조합으로 빌드할 때 필요한 CMake / emcc 플래그 전체 목록.

도구: Context7 MCP 우선(resolve-library-id로 emscripten / dear-imgui / dawn → query-docs).
부족하면 WebSearch + WebFetch로 공식 문서/릴리스 노트 확인.

산출물(마크다운): 항목별 "확정 사실(출처 URL+버전/날짜)"과 "권장안(의견)" 분리.
기능을 must-have / optional 분류. 상충·불확실 지점은 "불확실"로 명시.
금지: 코드 작성, 추정으로 빈칸 메우기, 출처 없는 단언.
```

### 프롬프트 #2 — EMSDK 설치/검증 + CMake 프리셋 + 브라우저 I/O (지금 필수)

```
역할: 빌드 환경 리서처. 코드 작성 금지, 검증된 사실만.

컨텍스트: macOS/Windows 양쪽 개발. 기존 멀티프리셋 CMake 프로젝트(ninja/msvc)에
emscripten 프리셋을 추가하려 한다. 에디터는 브라우저(WASM)에서 로컬 JSON 파일을
읽고/쓴다.

확정할 것(버전/날짜 명시):
1. EMSDK 설치 + 활성화 절차(macOS/Windows), 설치 검증 커맨드, 최근 안정 버전 핀.
2. 기존 네이티브 CMakeLists를 안 깨고 emscripten 타겟만 추가하는 정석: CMakePresets.json에
   emscripten 프리셋(toolchain file 경로) 추가 방식, if(EMSCRIPTEN) 가드 패턴.
3. WASM 산출물을 로컬에서 띄우는 최소 정적 HTTP 서버(Python 표준 라이브러리) 방법,
   그리고 file:// 로 못 여는 정확한 이유/제약.
4. 브라우저 로컬 파일 I/O 현재 상태: File System Access API 브라우저 지원/제약,
   IndexedDB 최소 사용 API. 각각의 fallback.

도구: 라이브러리/표준 API는 Context7 우선, 나머지는 WebSearch/WebFetch(MDN/공식).

산출물(마크다운): 항목별 확정 사실(출처)+권장안 분리, must-have/optional 분류,
불확실 지점 명시. 금지: 코드 작성, 출처 없는 단언.
```

### 프롬프트 #3 — Emscripten + WebGPU C++ UI 프레임워크 지형도 (지금 유용)

```
역할: C++/WASM UI 프레임워크 리서처. 코드 작성 금지, 검증된 사실만.

컨텍스트: Emscripten + WebGPU로 브라우저 데이터 에디터(폼 + 2D 프리뷰)를 C++로 만든다.
"직접 구현 대신 재사용 가능한" UI 프레임워크/라이브러리 지형도를 만들어라.

조사 대상(각각 Emscripten+WebGPU 빌드 가능 여부가 핵심 판정 기준):
- Dear ImGui (+ 생태계 add-on: implot, imnodes, ImGuizmo 등)
- RmlUi, Slint(C++ 바인딩), Qt for WebAssembly, NoesisGUI, 기타 C++ WASM UI
각 후보: WebGPU 백엔드 지원, 성숙도/유지보수, 라이선스, 이 용도(폼+프리뷰)에의 적합성,
WASM 바이너리 부담.

도구: Context7 우선(resolve-library-id→query-docs), 부족하면 WebSearch/WebFetch.

산출물(마크다운): 비교표 + "확정 사실(출처/버전)"과 "권장(의견)" 분리.
기능을 must-have(데이터 에디터 사실상 필수) / common / optional 분류.
최종 추천 1~2개 + 이유. 불확실 지점 명시. 금지: 코드 작성, 출처 없는 단언.
```

### 프롬프트 #4 — 타임라인/시퀀서 재사용 위젯 (S4 미래, 정찰만)

```
역할: UI 위젯 리서처. 코드 작성 금지, 검증된 사실만.
주의: 이건 미래(S4) 조사다. 현재 진행 중인 foundation 스펙/플랜에는 절대 반영하지 않는다 —
사전 정찰용 보고서일 뿐이다.

컨텍스트: After Effects식 "Playable Sequence Timeline Editor"를 ImGui(C++/WASM/WebGPU)
위에서 만들 계획. 바닥부터 짜기 전에, 재사용 가능한 오픈소스 타임라인/시퀀서/노드 위젯이
있는지 조사하라.

조사 대상:
- ImGuizmo의 ImSequencer / GraphEditor, ImGuiNeoSequencer, 기타 ImGui 타임라인 위젯
- 노드 에디터: imnodes, ImNodeFlow 등
각각: 제공 기능(트랙/키프레임/이징 커브/드래그/스냅), 성숙도, 라이선스, ImGui 버전 호환,
WASM 빌드 가능성, 커스터마이즈 용이성.

도구: Context7 우선, 그 다음 WebSearch/WebFetch(GitHub repo/README/이슈).

산출물(마크다운): 비교표, 확정 사실(출처)+의견 분리, 기능을 must-have(타임라인 핵심)/
common/optional 분류, "재사용 vs 직접 구현" 권장 경계. 불확실 명시.
금지: 코드 작성, 현재 스펙에의 반영 제안.
```

### 프롬프트 #5 — 웹/WGPU 셰이더 핫리로드 에디터 (먼 미래, 정찰만)

```
역할: 그래픽스 툴링 리서처. 코드 작성 금지, 검증된 사실만.
주의: 이건 먼 미래 아이디어다. 현재/차기 스펙에 절대 반영하지 않는다 — 가능성 정찰만.

컨텍스트: 브라우저(WebGPU/WASM)에서 셰이더를 편집하면 바로 결과가 보이는
"Hot Reload Shader Debugger"(ShaderToy식)를 언젠가 추가하고 싶다.

조사할 것:
1. 브라우저에서 WGSL(또는 GLSL→WGSL) 셰이더를 런타임에 재컴파일하고 WebGPU 파이프라인을
   핫스왑하는 정석 패턴. 비용/제약.
2. WGSL 검증/에러 보고 도구(naga 등)와 에러를 UI에 표면화하는 방법.
3. 텍스트 에디터 컴포넌트(Monaco, CodeMirror) 웹 통합 방식과 C++/WASM(ImGui) 환경에서의
   결합 가능성/대안(ImGui 내장 텍스트 에디터 등).
4. 참고할 기존 오픈소스(ShaderToy 클론, wgpu live-reload 예제 등).

도구: Context7 우선, 그 다음 WebSearch/WebFetch.

산출물(마크다운): 접근법별 확정 사실(출처)+의견 분리, must-have/common/optional 기능 분류,
난이도/리스크 평가. 불확실 명시. 금지: 코드 작성, 현재 스펙 반영 제안.
```

---

## 5. 가드레일 (리서처/오케스트레이터 공통)

- 리서치는 **읽기 전용**: 코드 수정·파일 생성·커밋 금지. 산출물은 마크다운 보고서뿐.
- **출처·버전 규율**: 모든 사실에 URL + 버전/날짜. 거짓 확신보다 "불확실" 명시.
- **#4·#5 결과는 현재 foundation 스펙/플랜에 반영 금지** (미래 정찰 격리).
- **D8(ImGui 채택) 변경은 임의 금지**: #3에서 더 나은 후보가 나와도 사용자 보고 후 재논의.
- 배치 A 완료 전에는 `/writing-plans` 착수 보류(플랜이 정확한 emcc 플래그/버전에 의존).
