# 리서치 #1 — Emscripten + WebGPU + ImGui 툴체인

- **날짜**: 2026-06-19
- **상태**: 배치 A (지금 필수) — 스펙 §15 미해결 확정용
- **도구**: Context7 MCP (`/emscripten-core/emscripten`, `/ocornut/imgui`)
- **주의**: 본 보고서는 Context7가 인덱싱한 공식 저장소 문서(emscripten ChangeLog/settings_reference, ImGui examples/CHANGELOG)에 근거한다. 각 사실에 출처 URL을 명시했다. Context7 응답에 절대 날짜(release date)가 포함되지 않은 항목은 "날짜 미확인"으로 표기했다.

---

## A. 확정 사실 (인용 가능)

### A1. `-sUSE_WEBGPU`는 deprecated 후 제거됨 — `--use-port=emdawnwebgpu`가 현재 방식 **[must-have]**

| 사실 | 버전 | 출처 |
|---|---|---|
| `-sUSE_WEBGPU` 설정이 외부 포트 `Emdawnwebgpu`로 **deprecated** (더 안정적인 `webgpu.h` 구현 제공) | Emscripten **4.0.10** | emscripten `ChangeLog.md` (github.com/emscripten-core/emscripten/blob/main/ChangeLog.md) |
| `-sUSE_WEBGPU` 설정이 **제거(removed)**되고 외부 포트 Emdawnwebgpu로 대체, `--use-port=emdawnwebgpu`로 사용 | Emscripten **4.0.18** | emscripten `ChangeLog.md` |
| `USE_WEBGPU` 설정은 더 이상 지원되지 않으며 `--use-port=emdawnwebgpu`로 대체됨. **newer but incompatible version of webgpu.h** 구현 | (현재 settings ref) | emscripten `settings_reference.md` |

> **핵심 결론**: 학습데이터에 흔한 `-sUSE_WEBGPU` 방식은 죽은 경로다. 현재 정석은 **`--use-port=emdawnwebgpu`** 한 줄이며, 이 포트가 제공하는 `webgpu.h`는 구 내장 헤더와 **호환되지 않는(incompatible) 신버전**이다. 즉 "내장 webgpu.h냐 Dawn이냐"의 답은 → **emdawnwebgpu 포트(= Dawn 계열 webgpu.h)** 로 일원화됐다.

### A2. ImGui WebGPU 백엔드는 emdawnwebgpu와 함께 `IMGUI_IMPL_WEBGPU_BACKEND_DAWN`을 기본값으로 씀 **[must-have]**

| 사실 | 출처 |
|---|---|
| Emscripten **4.0.10+** 사용 시, 백엔드를 명시하지 않으면 WebGPU 백엔드가 `IMGUI_IMPL_WEBGPU_BACKEND_DAWN`으로 **기본 설정**됨 | ImGui `doc/CHANGELOG.txt` (github.com/ocornut/imgui/blob/master/doc/CHANGELOG.txt) |
| 공식 `example_glfw_wgpu` CMake가 `EMSCRIPTEN_VERSION >= 4.0.10`이면 `--use-port=emdawnwebgpu`를 기본으로 설정, 그 미만이면 `FATAL_ERROR("emdawnwebgpu needs EMSCRIPTEN version >= 4.0.10")` | ImGui `examples/example_glfw_wgpu/CMakeLists.txt` |
| 백엔드 매크로는 셋 중 택1: `IMGUI_IMPL_WEBGPU_BACKEND_DAWN` / `..._WGPU` / `..._WGVK`. Emscripten 빌드는 DAWN 매크로를 씀 | ImGui `examples/example_sdl2_wgpu/CMakeLists.txt` |

> **핵심 결론**: emdawnwebgpu = Dawn 계열 헤더이므로, ImGui도 emscripten 빌드에서 `IMGUI_IMPL_WEBGPU_BACKEND_DAWN`을 쓴다(명시 안 하면 자동). `imgui_impl_wgpu.cpp` 한 백엔드 파일이 매크로 분기로 Dawn/wgpu-native/wgvk를 모두 커버한다.

### A3. 공식 예제는 GLFW(contrib 포트)와 SDL2 두 갈래 존재 — 플랫폼 백엔드 선택지 **[must-have 중 택1]**

| 예제 | 플랫폼 백엔드 플래그 | 출처 |
|---|---|---|
| `example_glfw_wgpu` | `--use-port=contrib.glfw3` (Emscripten > 3.1.57 기본). `-sUSE_GLFW=3`로 override 가능 | `examples/example_glfw_wgpu/CMakeLists.txt` |
| `example_sdl2_wgpu` | `-sUSE_SDL=2` | `examples/example_sdl2_wgpu/CMakeLists.txt` |

> **에디터 관련 핵심**: ImGui 공식 GLFW WGPU 예제가 쓰는 GLFW는 **`contrib.glfw3` emscripten 포트**이지, 게임 본체가 핀한 **GLFW 3.0.4 프리빌드가 아니다.** 즉 D8/핸드오프의 전제("에디터는 GLFW 3.0.4 미의존, 별도 ImGui 사본")와 정확히 맞는다 — 에디터는 emscripten이 자체 빌드하는 `contrib.glfw3` 포트를 쓰고, 게임의 네이티브 GLFW와 완전히 분리된다.

### A4. 공식 예제 빌드 플래그 전체 목록 **[must-have]**

**(a) GLFW WGPU 예제 — configure/compile (출처: `example_glfw_wgpu/CMakeLists.txt`, `README.md`)**
```
# configure
emcmake cmake -G Ninja -B build
# (로컬 emdawnwebgpu 포트를 쓰려면)
emcmake cmake -DIMGUI_EMSCRIPTEN_WEBGPU_FLAG="--use-port=path/to/emdawnwebgpu_package/emdawnwebgpu.port.py" -G Ninja -B build

# compile 옵션 (CMake가 add_compile_options로 주입)
--use-port=emdawnwebgpu          # IMGUI_EMSCRIPTEN_WEBGPU_FLAG 기본값
--use-port=contrib.glfw3         # IMGUI_EMSCRIPTEN_GLFW3 기본값
-sDISABLE_EXCEPTION_CATCHING=1
-DIMGUI_DISABLE_FILE_FUNCTIONS=1
# link: LIBRARIES = glfw
```

**(b) SDL2 WGPU 예제 — link 옵션 전체 (출처: `example_sdl2_wgpu/CMakeLists.txt`)**
```
CMAKE_EXECUTABLE_SUFFIX = ".html"
# compile
--use-port=emdawnwebgpu          # IMGUI_EMSCRIPTEN_WEBGPU_FLAG
-sUSE_SDL=2
# link
--use-port=emdawnwebgpu
-sUSE_SDL=2
-sWASM=1
-sASYNCIFY=1
-sALLOW_MEMORY_GROWTH=1
-sNO_EXIT_RUNTIME=0
-sASSERTIONS=1
-sDISABLE_EXCEPTION_CATCHING=1
-sNO_FILESYSTEM=1
--shell-file=.../shell_minimal.html
OUTPUT_NAME = "index"
```

---

## B. 권장/의견 (확정 사실 아님 — 판단 필요)

### B1. 플랫폼 백엔드는 **GLFW(`contrib.glfw3`) 권장** *(의견)*
- 이유: ImGui 공식 `example_glfw_wgpu`가 emdawnwebgpu 기본 경로와 가장 잘 정렬돼 있고, 게임 본체가 이미 GLFW API에 익숙하므로(코드 자체는 공유 안 하지만) 개념적 전이 비용이 낮다. SDL2 경로도 1급 지원되므로, 입력/윈도잉에서 SDL 생태계가 더 필요하면 SDL2도 유효.
- **불확실**: 폼+2D 프리뷰만 하는 에디터에서 GLFW vs SDL2의 실질 차이는 미미. 어느 쪽이든 공식 예제가 있으니 리스크는 낮다.

### B2. ImGui 버전 핀 — **특정 릴리스 번호 미확정 (불확실)**
- Context7가 반환한 근거는 모두 `master` 브랜치 기준 예제/CHANGELOG이고, **권장 ImGui 정확한 release tag(예: v1.9x.x)를 명시한 출처는 확보하지 못했다.**
- 확정된 하한선만 존재: **emdawnwebgpu 백엔드 기본 동작은 ImGui CHANGELOG에 "Emscripten 4.0.10+에서 DAWN 기본"으로 기록된 버전 이후**여야 한다.
- **권장(의견)**: 플랜 단계에서 ImGui를 도킹/마스터 HEAD가 아닌 **최신 안정 릴리스 태그로 핀**하되, 그 태그가 `example_glfw_wgpu`에 emdawnwebgpu 기본 분기(`>= 4.0.10` 가드)를 포함하는지 태그의 `CHANGELOG.txt`에서 1줄 확인 후 확정. (이 확인은 GitHub 릴리스 페이지 직접 조회가 필요 — Context7만으로는 태그별 diff 비교 불가.)

### B3. 에디터에 주의할 플래그 *(의견/플래그)*
- `-sASYNCIFY=1`: WebGPU 어댑터/디바이스 요청이 async라 SDL2 예제가 켰다. WebGPU 초기화를 동기 흐름처럼 쓰면 필요할 수 있음 — 단 ASYNCIFY는 바이너리 크기/성능 비용이 있으니, emscripten의 JSPI 등 대안 여부를 플랜에서 재검토 권장.
- `-sNO_FILESYSTEM=1`: SDL2 예제는 켰지만, **우리 에디터는 fetch GET / IndexedDB / Save를 쓴다.** fetch·IndexedDB는 JS 측 API라 emscripten FS와 무관하게 동작하지만, nlohmann 파싱 등에서 `FILE*`를 안 쓰는지 확인 후에만 이 플래그 채택 가능. (관련: `-DIMGUI_DISABLE_FILE_FUNCTIONS=1`은 ImGui의 ini 파일 저장 비활성 — 에디터에서 ini 영속이 필요하면 끄거나 커스텀 핸들러.)
- `-sALLOW_MEMORY_GROWTH=1`, `-sASSERTIONS=1`(dev), `-sWASM=1`: 표준. 프로덕션 빌드에서 `-sASSERTIONS=0` 권장.

---

## C. 스펙 §15 반영안 (핸드오프 §3 배치 A 처리)

| 스펙 §15 미해결 | 본 리서치 확정값 |
|---|---|
| emscripten WebGPU 정확한 플래그/Dawn 여부 | **`--use-port=emdawnwebgpu`** (구 `-sUSE_WEBGPU`는 4.0.18에서 제거). Dawn 계열 `webgpu.h`. ImGui는 `IMGUI_IMPL_WEBGPU_BACKEND_DAWN` 자동. → 스펙 §7.3 갱신 |
| 에디터 신형 ImGui 버전 + 플랫폼 백엔드 | 백엔드 = **`contrib.glfw3` 포트**(또는 SDL2). 버전 = **release tag 미확정(B2)** — 플랜에서 1줄 검증 후 핀 → 스펙 §7.3·§15 갱신 |
| EMSDK 버전 핀 | **본 #1 범위 밖** → 리서치 #2에서 확정 (단 하한선: emdawnwebgpu·ImGui 예제 모두 **Emscripten ≥ 4.0.10** 요구) |

---

## D. 불확실 / 후속 검증 필요 (거짓 확신 금지)

1. **ImGui 정확한 release tag** — Context7 미반환. GitHub 릴리스/태그 직접 조회 필요 (B2).
2. **emdawnwebgpu 포트의 내부 Dawn 버전/핀** — `--use-port=emdawnwebgpu`가 끌어오는 Dawn 스냅샷의 정확한 버전은 본 조회에서 미확인. emscripten 버전에 종속될 가능성 → #2와 교차.
3. **각 ChangeLog 항목의 절대 날짜** — Context7가 버전 번호(4.0.10/4.0.18)는 줬으나 릴리스 날짜는 미반환. 날짜가 필요하면 emscripten 릴리스 페이지 직접 확인.
4. **JSPI vs ASYNCIFY** 현재 권장 — B3는 SDL2 예제 관찰에 근거한 추론. 최신 권장은 플랜에서 emscripten 문서로 재확인 권장.
