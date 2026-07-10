# 리서치 #2 — EMSDK 설치/검증 + CMake 프리셋 + 브라우저 I/O

- **날짜**: 2026-06-19
- **상태**: 배치 A (지금 필수) — 빌드/배포 절차 확정용
- **도구**: Context7 MCP (`/emscripten-core/emsdk`, `/emscripten-core/emscripten`) + 핸드오프 fallback 조항에 따른 MDN/web(File System Access API·IndexedDB는 브라우저 호환성 영역이라 Context7 라이브러리 인덱스 범위 밖)
- **출처 구분**: §1·§2·§3 = Context7. §4 = MDN/caniuse/web.dev (fallback, 각 사실에 URL+수정일 명시).

---

## A. 확정 사실 (인용 가능)

### A1. EMSDK 설치/활성화/검증 — macOS·Windows **[must-have]**
출처: Context7 `/emscripten-core/emsdk` (`emsdk/llms.txt`, `README.md`)

**공통 절차**
```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest      # 최신 안정 설치
./emsdk activate latest     # 활성화
```
**환경 변수 설정 (OS별)**
- macOS/Linux: `source ./emsdk_env.sh`
- Windows: `emsdk_env.bat` (cmd) / `.\emsdk_env.ps1` (PowerShell)
- 영속화(macOS): `echo 'source "/path/to/emsdk/emsdk_env.sh"' >> ~/.zprofile` (zsh) 또는 `~/.bash_profile`
- 영속화(Windows): `./emsdk activate latest --permanent` (또는 `--system` 시스템 전역)

**검증**
```bash
emcc --version
# 예: emcc (... GNU ld) 5.0.6 (...)   ← Context7 예제 출력에 등장한 버전
echo 'int main(){return 0;}' > test.c
emcc test.c -o test.js
node test.js
```

### A2. 버전 핀(고정) 방법 **[must-have]**
출처: Context7 `/emscripten-core/emsdk`
- 특정 버전 설치/활성화: `./emsdk install 5.0.6` → `./emsdk activate 5.0.6`
- 구버전 탐색: `emsdk list --old`
- 기타: `sdk-main-64bit`(소스빌드, `--build=Release -j4`/`--shallow`), `tot`(tip-of-tree)
- **버전 핀 권장 형식**: 팀 전체가 `emsdk install <X.Y.Z>` + `emsdk activate <X.Y.Z>`로 동일 버전 고정. CI/문서에 `<X.Y.Z>` 명시.

> **버전 주의(불확실)**: Context7 예제 출력에 **emcc 5.0.6**이 등장하나, 이것이 "오늘 시점 latest"라는 보장은 Context7 응답만으로는 없다(예제 스냅샷일 수 있음). **#1에서 확정된 하한선 = Emscripten ≥ 4.0.10** (emdawnwebgpu·ImGui WGPU 예제 요구). 따라서 핀 후보는 **4.0.10 이상의 검증된 안정 버전**이며, 정확한 핀 번호는 `emsdk install latest` 후 `emcc --version` 출력으로 확정 권장.

### A3. CMake 통합 + `if(EMSCRIPTEN)` 가드 **[must-have]**
출처: Context7 `/emscripten-core/emscripten` (`llms.txt`, `test/cmake/target_js/CMakeLists.txt`)
- **빌드 래퍼**: `emcmake cmake .` — `emcmake`가 cmake를 감싸 `emcc`를 컴파일러로 강제. (toolchain file을 자동 주입)
- **크로스컴파일 시 정의되는 CMake 변수(확정)**: 다음이 emscripten 빌드에서 참이어야 함 —
  - `EMSCRIPTEN` = 정의됨 → **`if(EMSCRIPTEN)` 가드가 정석** (스펙 §7.1·§7.2 패턴과 일치)
  - `CMAKE_EXECUTABLE_SUFFIX` = `.js` (기본)
  - `WIN32` / `APPLE` = **정의 안 됨** (크로스컴파일이므로) — 즉 호스트 OS 분기와 무관
  - `CMAKE_C_SIZEOF_DATA_PTR` = 정의됨
- 이 사실은 ImGui 예제(#1 A1~A4)가 `if(EMSCRIPTEN) ... else()` 분기를 쓰는 것과 정확히 호응 → **기존 네이티브 CMakeLists에 `if(EMSCRIPTEN)` 블록만 추가하고, 네이티브 의존(FMOD/Box2D/GLFW prebuilt)은 `else()`에 격리**하면 안 깨짐.

### A4. WASM은 `file://` 불가 — 로컬 HTTP 서버 필수 **[must-have]**
출처: Context7 `/emscripten-core/emscripten` (`Running-html-files-with-emrun.md`, `Deploying-Pages.md`, `process.md`)
- **이유(확정)**: 브라우저 **기본 CORS 규칙** 때문에 생성된 `.html`/`.wasm`/`.js`를 `file://`로 열면 안 된다. 로컬 웹 서버로 서빙해야 함.
- **Python 표준 라이브러리 서버(확정)**: `python3 -m http.server 8000 -d <dir>` — 의존 0, 정적 서빙. (emscripten 문서가 자사 사이트 서빙에 동일 명령 사용)
- 대안: emscripten 트리의 `emrun`/`emrun.py` (gzip 서빙 + CLI 자동화 사전구성).

> **스펙 D10 정합**: 스펙은 "Python 표준 라이브러리 정적 서버 + PUT 핸들러"를 정했는데, `http.server`는 GET만 기본 제공 → **PUT 핸들러는 `BaseHTTPRequestHandler` 서브클래싱으로 직접 추가 필요** (구현은 플랜에서). 본 리서치는 "GET 서빙은 표준 라이브러리로 충분, PUT은 커스텀 핸들러 필요"까지 확정.

---

## B. 브라우저 로컬 파일 I/O — 현재 상태 (출처: MDN/caniuse/web.dev, fallback)

### B1. File System Access API — **로컬 디스크 쓰기, 단 호환성 제한** [common, fallback 필수]
| 사실 | 출처 (수정일) |
|---|---|
| `showOpenFilePicker()` / `showSaveFilePicker()` / `showDirectoryPicker()` = 로컬 디스크 핸들 팩토리 | MDN showSaveFilePicker (2026-01-25) |
| 지원: **Chrome 86+, Edge 86+, Opera 72+** (데스크톱) | TestMu/LambdaTest 학습허브 (2026-05-05), caniuse |
| **Firefox·Safari = 로컬 디스크 피커 미지원**, OPFS(Origin Private File System)만 제공. Mozilla는 로컬 디스크 피커를 standards position에서 부정적 평가 | TestMu (2026-05-05), WICG spec (2025-10-10) |
| 모바일(Chrome for Android/Firefox for Android): 로컬 디스크 피커 **미노출** (OPFS는 일부 가능) | TestMu (2026-05-05) |
| **secure context(HTTPS 또는 localhost) 필수**, **user gesture(클릭/키) 필수**, cross-origin iframe에서 차단 | web.dev FS Access, fsjs.dev (2026) |
| 흔한 함정: 데이터 가공을 picker 호출 *전*에 하면 `SecurityError(Must be handling a user gesture)`. 핸들 먼저 획득 후 가공 | developer.chrome.com FS Access |

**Fallback (확정)**: 
- **읽기**: `<input type="file">` 또는 drag-and-drop → `File` 객체.
- **쓰기(저장)**: `Blob` + `URL.createObjectURL(blob)` + `<a download>` 클릭 → 다운로드(덮어쓰기는 불가, 새 파일 다운로드만).
- 폰필: `browser-fs-access`(Google), Firefox 확장 `ext-file`(헬퍼 앱 필요).

> **스펙 §8 SaveAdapter 정합**: 스펙은 "dev 서버 PUT (실패 시 FSAccess/download 폴백)"으로 정했다. 본 리서치 결론 → **dev 서버 PUT을 1차 경로로 두는 스펙 결정이 옳다.** FS Access API는 Chrome/Edge에서만 동작하고 user gesture 제약이 있어, 저장 1차 경로로 삼으면 Firefox/Safari에서 깨진다. localhost dev 서버 PUT은 모든 브라우저에서 동작 → 1차로 적절. FS Access는 "dev 서버 없이 쓰고 싶을 때"의 보조, `<a download>`는 최후 폴백.

### B2. IndexedDB — **드래프트 저장소, 전 브라우저 베이스라인** [must-have]
| 사실 | 출처 (수정일) |
|---|---|
| 구조적 데이터(파일/blob 포함) 대용량 클라이언트 저장용 저수준 트랜잭션 API. 비동기(request/event) | MDN IndexedDB API (2025-04-03) |
| **same-origin 정책**. **Web Worker에서도 사용 가능** | MDN IndexedDB API |
| 최소 API: `window.indexedDB.open(name, version)` → `IDBRequest`; `onupgradeneeded`에서 `db.createObjectStore(name, {keyPath})`; `transaction([store], "readwrite")` → `objectStore.put(value)` / `.get(key)` | MDN Using IndexedDB (2026-02-08), createObjectStore (2025-06-23) |
| 저장 쿼터/eviction은 브라우저마다 다름 | MDN IndexedDB API |
| 경량 래퍼(선택): `idb`(API 미러), `idb-keyval`(~600B promise key-value), `localForage`, `Dexie.js` | MDN Using IndexedDB |

> **호환성 핵심**: IndexedDB는 FS Access와 달리 **Chrome/Firefox/Safari/Edge 전부 지원하는 베이스라인**이다. 따라서 스펙 §8의 `IndexedDbDraftAdapter`(작업 중 드래프트, 새로고침 생존)는 **모든 타겟 브라우저에서 안전하게 동작** — 스펙 결정 타당.

---

## C. 권장/의견

- **C1.** dev 서버는 `http.server` 서브클래스(+PUT)로 충분하되, 단일 파일 `tools/webeditor-server/serve.py`로 두고 `--directory apps/_MyApp_/resources/data`를 루트로. (스펙 D10/§7.4와 일치, 구현은 플랜)
- **C2.** 저장 경로 우선순위(의견, B1 근거): **① localhost dev 서버 PUT(전 브라우저) → ② FS Access `showSaveFilePicker`(Chrome/Edge, user gesture) → ③ `<a download>` blob(최후)**. 스펙 §8 어댑터 순서와 일치.
- **C3.** 개발 브라우저(의견): 로컬 디스크 직접 저장 실험까지 하려면 **Chrome/Edge 권장**(FS Access 지원). Firefox/Safari는 dev 서버 PUT 경로로만 검증.
- **C4.** IndexedDB 직접 vs 래퍼(의견): 드래프트 1종 저장이면 `idb-keyval`(~600B)로 충분. 다중 스토어/마이그레이션이 필요하면 `idb` 또는 `Dexie.js`. (벤더링 정책은 헤더온리 C++가 아니라 JS glue라 별도 판단 — 플랜에서)

---

## D. 스펙 반영안 (핸드오프 §3 배치 A 처리)

| 스펙 항목 | 본 리서치 확정 |
|---|---|
| §7.2 EMSDK 설치/검증 | `git clone emsdk` → `install/activate latest` → `source emsdk_env.sh`(mac)/`emsdk_env.bat`(win) → `emcc --version` 검증. 핀: `install <X.Y.Z>` (≥4.0.10) |
| §7.2 emscripten 프리셋/가드 | `emcmake cmake` 래퍼 + **`if(EMSCRIPTEN)` 가드 정석**(EMSCRIPTEN 정의됨, WIN32/APPLE 미정의 확정). 네이티브 의존은 `else()`에 격리 |
| §7.4 / D10 dev 서버 | `python3 -m http.server`(GET) + **PUT은 BaseHTTPRequestHandler 커스텀**. `file://` 불가 사유 = 브라우저 CORS |
| §8 SaveAdapter / IndexedDbDraftAdapter | **PUT 1차 결정 타당**(FS Access는 Chrome/Edge·user gesture 제약). IndexedDB는 전 브라우저 베이스라인 → 드래프트 어댑터 안전 |

---

## E. 불확실 / 후속 검증 (거짓 확신 금지)

1. **정확한 EMSDK 핀 번호** — Context7 예제의 5.0.6은 스냅샷. `emsdk install latest` 실행 후 실제 출력으로 확정 필요. 하한선만 확정(≥4.0.10).
2. **`CMAKE_TOOLCHAIN_FILE` 절대 경로 문자열** — 스펙 §7.2가 적은 `$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`는 통상 경로이나, Context7 이번 조회가 그 문자열을 직접 반환하진 않음(emcmake가 자동 주입하므로 명시 불요일 수 있음). CMakePresets에 toolchainFile을 손으로 박을지/emcmake로 갈지는 플랜에서 택1.
3. **FS Access API 세부 호환(특정 버전별 quirks)** — caniuse 실시간 표 직접 확인 권장. 본 보고서는 "Chrome/Edge 86+ 지원, FF/Safari 미지원(OPFS만)"의 큰 그림까지만 확정.
4. **브라우저 저장 쿼터 구체 수치** — 브라우저별 상이, 본 범위 밖.
