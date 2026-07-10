# WebEditor 마스터 데이터 기반 (Foundation) 설계

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

- **날짜**: 2026-06-19
- **상태**: 설계 확정 (구현 플랜 대기)
- **대상**: `_MyApp_` 게임 본체 + 신규 `WebEditor` (WASM) 타겟
- **범위**: 첫 스펙 = "워킹 스켈레톤" 수직 슬라이스. 밸런스 마스터 데이터 4종을 JSON 리소스화 + 런타임 로딩 + 브라우저 에디터로 한 바퀴 관통.
- **리서치 반영 (2026-06-19)**: [doc/webeditor/](../../../doc/webeditor/) 5건 리서치로 §15 미해결 확정 + 신규 결정 D12~D15 추가. emscripten WebGPU = `--use-port=emdawnwebgpu`(구 `-sUSE_WEBGPU` 제거), dev 서버 = cpp-httplib, 에디터 프리뷰 = Slang→WGSL 공유 + reflection 구동 bind group + 아틀라스 서브렉트.

---

## 0. Plan 분할 (2026-06-19 재우선순위 — 데이터 먼저, 에디터 나중)

WebEditor를 짓기 전에 **게임 측 JSON 데이터 시스템을 먼저 세워 스키마가 데이터를 올바르게 담는지 검증**한다. 두 플랜으로 분리:

- **Plan 1 (지금) — JSON DoD 시스템 (게임 측, 에디터 없음)**: 마스터데이터를 schema+JSON 화 + 로더 + ResourceRegistry DataTable + **게임 런타임 cutover**. 검증 = 게임이 JSON 읽고 *동일 동작* + schema 검증 통과 + 로더 round-trip. (S1 확장)
- **Plan 2 (나중) — WebEditor (S2+S3)**: Plan 1 동작 보장 후에만 착수. D1~D4·D8~D15(emdawnwebgpu/cpp-httplib/Slang→WGSL/IDataStore 등)는 Plan 2 에서 유효.

### Plan 1 커버리지 = Tier A + B 전부, Tier C 제외 (사용자 결정 2026-06-19)
| Tier | 성격 | 대상 | Plan 1 |
|---|---|---|---|
| **A 순수 마스터데이터** | 스칼라/색/문자열 | Entity·Physics·Bootstrap·Stage(웨이브/아레나)·HUD(glm 색)·Audio(event/bus 경로) | ✅ JSON + cutover |
| **B 구조화 데이터** | 중첩배열/특수타입 | Playable 스프라이트 테이블(`vector<EntityTextureConfig>` + `glm::vec2` 방향임계 + 연출 스칼라), VFX `EffectAsset`(`char16_t`) | ✅ JSON + cutover (스키마 모델 진짜 시험대) |
| **C 코드 배선** | 식별자/엔진결합 | 루트 actor·`UNI_*`·UI 이름, Stage/Audio actor 이름, **Playable PostFX 파이프라인(`POSTFX_PROGRAM_CONFIGS`·`PASS_*`)** | ❌ constexpr 유지 (튜닝값 아님 — JSON화 이득 없음) |

→ **D5(밸런스 4종 우선)·D7(Stage만 cutover)은 Plan 1 에서 superseded** — Plan 1 은 Tier A+B *전부* schema+JSON+cutover. 단계화는 플랜 내부 Phase 로(Tier A → Tier B). WGPU/에디터 관련(§5~§9 의 S2/S3 부분, D1·D12~D15)은 **Plan 2 로 이연**.

---

## 1. 동기 (Why)

현재 게임 튜닝 값은 8개 `Constants.h`에 `constexpr`로 박혀 있어 **수정할 때마다 재컴파일**이 필요하다. 게임업계의 마스터 데이터(데이터 테이블/시트) / Unity ScriptableObject처럼, 이 값들을 **JSON 리소스로 외부화**하고 **편집용 브라우저 에디터**를 만들어 *재컴파일 없는 밸런스 패치*를 가능하게 한다.

에디터는 사용자 결정에 따라 **WASM + WGPU + Emscripten** 스택으로, 게임과 **독립된 빌드 타겟**(`WebEditor`)으로 만든다.

## 2. 큰 그림 — 하위 프로젝트 분해

이 비전은 단일 스펙으로 너무 크다. 4개 하위 프로젝트로 분해하고, **본 스펙은 S1+S2+S3를 밸런스 데이터에 적용한 첫 조각**만 다룬다.

| # | 하위 프로젝트 | 본 스펙 포함 |
|---|---|---|
| **S1** | 런타임 데이터 주도 기반 (스키마 SSOT, 게임측 JSON 로더, constexpr→런타임) | ✅ (Stage 완전 전환) |
| **S2** | WebEditor 빌드 타겟 (Emscripten/WGPU/ImGui 스캐폴드, dev 서버, `IDataStore` 포트) | ✅ |
| **S3** | 스키마 구동 테이블 에디터 (밸런스 4종 폼) | ✅ |
| **S4** | Playable 타임라인 에디터 (After Effects식 시퀀스 트랙 + WGPU 라이브 스프라이트 sim) | ❌ (다음 스펙) |

## 3. 결정 로그 (Decision Log)

| # | 결정 | 선택 | 근거 / 기각안 |
|---|---|---|---|
| D1 | WGPU 역할 | 2D 스프라이트/아틀라스 프리뷰 (장기). **첫 스펙은 ImGui WGPU 백엔드 + 아틀라스 서브렉트 스프라이트**(D13~D15) | 엔진 통째 WASM 포팅 회피. 라이브 게임 프리뷰(거대)는 기각 |
| D2 | 게임의 JSON 소비 | 런타임 로딩 (진짜 데이터 주도) | 재컴파일 없는 밸런스 패치가 핵심 이득. 코드젠(오프라인)/하이브리드 기각 |
| D3 | 스키마 SSOT | `*.schema.json` (JSON Schema 부분집합) → 범용 에디터 | 새 필드가 코드 수정 없이 추가됨. C++ struct SSOT / 코드젠 기각 |
| D4 | 파일 입출력 | `IDataStore` 포트 + Phase1 어댑터(정적서버 fetch GET / IndexedDB 드래프트 / Save) | WASM은 어차피 HTTP 서빙 필수 → fetch GET 공짜. Phase2 REST는 어댑터 교체 |
| D5 | 1차 데이터 범위 | ~~밸런스 4종~~ → **Plan 1 = Tier A+B 전부** (§0, 2026-06-19 superseded) | 스키마 검증이 목적이라 구조화(B)까지 포함해야 모델 de-risk |
| D6 | 런타임 데이터 거주 | **`SJH::ResourceRegistry`에 DataTable 자원으로** | 프로젝트 컨벤션(자원=레지스트리 위탁) + ScriptableObject 비유 일치 |
| D7 | 마이그레이션 경계 | ~~Stage만 전환~~ → **Plan 1 = Tier A+B 전부 풀 cutover** (§0, 2026-06-19 superseded) | "동작 보장"이 검증 기준 — 게임이 실제로 전 데이터를 JSON 으로 소비해야 스키마 정합 증명 |
| D8 | 에디터 폼 UI | Dear ImGui (게임 v1.53과 **독립 사본**, 신형 + `imgui_impl_wgpu`) | 에디터는 GLFW 3.0.4 미의존 → 신형 ImGui 가능. HTML/DOM 오버레이 기각(C++/JS 경계 복잡) |
| D9 | 에디터 위치 | 신규 최상위 `editor/` | emscripten 전용, `project_deps`(네이티브 prebuilt) 링크 금지 → `apps/`와 격리 |
| D10 | dev 서버 | **cpp-httplib** (header-only C++, `include/`에 벤더링, MIT) — 네이티브 타겟 | "전부 C++" 일관성 + 헤더온리 벤더링 정합 + 추후 C++ CRUD 서버로 성장. Python 기각(런타임/언어 불일치). 근거 [doc/webeditor/02·compass] |
| D11 | 게임측 JSON 파서 | nlohmann/json — **이미 vcpkg `find_package(nlohmann_json CONFIG)` + `game_deps` 합류**(`cmake/Dependency.cmake:131,163`). 벤더링 불필요, `#include <<nlohmann>/json.hpp>`. **에디터(WASM)·dev서버도 동일** | vcpkg 전환 후 tweeny/stb와 동일 패턴. `resource_registry`가 game_deps PUBLIC → SJH::engine consumer 자동 전파 |
| D12 | emscripten WebGPU | **`--use-port=emdawnwebgpu`** + ImGui `IMGUI_IMPL_WEBGPU_BACKEND_DAWN` 자동 + 플랫폼 백엔드 `contrib.glfw3` 포트. Emscripten ≥ 4.0.10 | 구 `-sUSE_WEBGPU`는 4.0.18에서 제거됨. `contrib.glfw3`는 게임 GLFW 3.0.4와 완전 분리 → D8 전제 부합. 근거 [doc/webeditor/01] |
| D13 | 에디터 프리뷰 셰이더 | **게임 `.slang` → WGSL 공유** (`simple_texture.slang`에 WGSL 산출 추가, 에디터가 fetch) | 손-WGSL 드리프트 0, 게임과 단일 소스, S4/셰이더 핫리로드 경로 de-risk. 근거 [scripts/slang_compile.py] |
| D14 | WGPU bind group | **Slang reflection JSON(`.refl.json`) 구동** 데이터 주도 구성 | 프로젝트 reflection 투자 활용, 셰이더 다수화(S4) 대비 일반화. 하드코딩 기각 |
| D15 | 프리뷰 증명 범위 | **아틀라스 서브렉트** — `SJH::sprite::ComputeUVRect`(순수 함수) WASM 포팅 | WGPU 역할 #1(아틀라스 프리뷰)의 실제 경로를 foundation에서 증명. 풀-텍스처 쿼드 기각 |

## 4. 산출물 (Definition of Done)

1. `stage.json`만 수정하고 **재컴파일 없이** 게임 실행 시 웨이브 난이도/아레나 크기가 바뀐다 (Stage 완전 전환).
2. Entity/Physics/Audio도 schema+json이 존재하고, 게임 시작 시 파싱·검증되어 `ResourceRegistry`에 적재된다 (사용처 전환은 미적용).
3. `WebEditor`가 Emscripten으로 빌드되어 브라우저에서 뜨고, ImGui가 WGPU 백엔드로 렌더되며 샘플 스프라이트 1장이 보인다.
4. WebEditor가 dev 서버에서 4종 schema+json을 GET → 스키마 구동 폼으로 편집 → Save로 `resources/data/`에 되돌린다.

## 5. 아키텍처

```
[ apps/_MyApp_/resources/data/ ]   ← 단일 소스 (스키마 + 데이터 한 벌)
  ├─ schema/
  │   ├─ stage.schema.json     (SSOT: 필드/타입/기본값/min·max/description)
  │   ├─ entity.schema.json
  │   ├─ physics.schema.json
  │   └─ audio.schema.json
  ├─ stage.json   entity.json   physics.json   audio.json
        │ fetch(GET)                              │ 게임 시작 시 파싱
        ▼                                         ▼
[ editor/  (WASM/Emscripten) ]              [ _MyApp_ (네이티브) ]
  ImGui(WGPU 백엔드) 스키마 구동 폼            DataTable 로더 (nlohmann/json)
  + 샘플 스프라이트 WGPU draw                   → ResourceRegistry 에 DataTable 적재
  + IDataStore 포트                            → Stage 소비처가 StageData 참조
        │ Save (PUT / FSAccess)
        ▼
  tools/webeditor-server/ (Python 정적+PUT) → resources/data/ 갱신
```

### 5.1 데이터/스키마 레이아웃
- 정본 위치: `apps/_MyApp_/resources/data/` — **한 벌만** 유지.
  - 게임: POST_BUILD가 `resources/`를 실행 디렉토리로 복사 → 복사본에서 읽음 (기존 패턴).
  - 에디터: dev 서버가 `apps/_MyApp_/resources/data/`를 루트로 서빙 → 같은 파일 GET/PUT.
- 스키마 포맷: **JSON Schema draft-07의 작은 부분집합**.
  - 사용 키워드: `type` (`integer`/`number`/`string`/`boolean`만), `properties`, `default`, `minimum`, `maximum`, `description`.
  - 풀 스펙 미사용 — 폼 렌더러가 다루는 만큼만.
- 기존 Doxygen 주석의 튜닝 설명 → schema `description`으로 이주 → 에디터 폼 툴팁.

### 5.2 스키마 예시 (`stage.schema.json`)
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "Stage",
  "type": "object",
  "properties": {
    "waveSpawnInterval": { "type": "number",  "default": 3.0, "minimum": 0.1, "description": "적 스폰 간격(초)" },
    "waveMaxEnemies":    { "type": "integer", "default": 5,   "minimum": 1,   "description": "동시 생존 최대 수" },
    "waveHpBase":        { "type": "integer", "default": 20,  "minimum": 1,   "description": "웨이브1 기본 HP" },
    "waveHpPerWave":     { "type": "integer", "default": 5,   "minimum": 0,   "description": "웨이브당 HP 증가" },
    "waveSpeedBase":     { "type": "number",  "default": 1.5, "minimum": 0,   "description": "웨이브1 기본 속도" },
    "waveSpeedPerWave":  { "type": "number",  "default": 0.3, "minimum": 0,   "description": "웨이브당 속도 증가" },
    "waveContactDamage": { "type": "integer", "default": 10,  "minimum": 0,   "description": "접촉 데미지" },
    "arenaHalfExtent":   { "type": "number",  "default": 10.0,"minimum": 1.0, "description": "아레나 벽 안쪽 절반" },
    "wallThickness":     { "type": "number",  "default": 0.5, "minimum": 0.1, "description": "벽 두께(반-크기)" }
  }
}
```
대응 `stage.json`은 `default`만 추린 평평한 값 객체.

## 6. S1 — 게임측 런타임 로더

### 6.1 nlohmann/json 벤더링
- 단일 헤더 `<include>/nlohmann/json.hpp` 체크인.
- `cmake/Dependency.cmake`에 `INTERFACE` 표식 타겟(tweeny/stb 패턴) → `game_deps`에 합류.

### 6.2 DataTable 로더 + ResourceRegistry 적재 (D6)
- `ResourceRegistry`에 `DataTable` 자원 종류 추가 (기존 9종 → 10종).
- 로더 책임: `Load(key, path)` → 파일 읽기 → nlohmann 파싱 → 타입드 struct(`StageData` 등) 채움 → 레지스트리에 보관. 조회: `registry->GetDataTable<StageData>("stage")`.
- **관대한 파서**: 필드별 기본값 폴백. 누락/오타/타입오류 필드는 코드 기본값 유지 + spdlog warn. JSON이 깨져도 게임은 죽지 않는다.
- **런타임 스키마 검증은 게임에 넣지 않는다** (에디터 책임).

### 6.3 Stage 완전 전환 (D7)
- 신규 `StageData` struct (스키마와 1:1 평평한 필드).
- 전환 대상 사용처:
  - `WaveController` ctor에 `const StageData&` 주입 → `mSpawnTimer(data.waveSpawnInterval)`, 웨이브 곡선(`hp/speed/damage`), `LiveCount() < data.waveMaxEnemies`.
  - `StageConfig`의 `arenaHalfExtent`/`wallThickness` 기본값을 로드된 `StageData`에서.
  - `main.cpp`의 `Stage::ARENA_HALF_EXTENT` 전달 지점.
- **Before/After** (WaveController):
  ```cpp
  // Before
  : mSpawnTimer(WAVE_SPAWN_INTERVAL)
  d.hp = WAVE_HP_BASE + mWave * WAVE_HP_PER_WAVE;
  // After
  : mData(data), mSpawnTimer(data.waveSpawnInterval)
  d.hp = mData.waveHpBase + mWave * mData.waveHpPerWave;
  ```
- Entity/Physics/Audio: `*Data` struct + 로드까지만. 사용처는 constexpr 유지(다음 스펙).

## 7. S2 — WebEditor 빌드 타겟

### 7.1 위치/격리 (D9)
- 신규 최상위 `editor/` + 자체 `CMakeLists.txt`.
- **`project_deps` / `game_deps` 링크 금지** (네이티브 prebuilt lib는 WASM 링크 불가).
- emscripten 프리셋으로 configure할 때만 빌드(루트 CMake에서 `if(EMSCRIPTEN)` 가드).

### 7.2 툴체인/프리셋 (D12, 리서치 #2 확정)
- 빌드 래퍼 = **`emcmake cmake`** (emcc toolchain 자동 주입). CMakePresets에 toolchainFile 직접 박기 vs emcmake 래퍼는 플랜에서 택1.
- 크로스컴파일 시 **`EMSCRIPTEN` 정의됨 / `WIN32`·`APPLE` 미정의** → `if(EMSCRIPTEN)` 가드가 정석. 네이티브 의존(FMOD/Box2D/GLFW prebuilt)은 `else()`에 격리.
- EMSDK: `git clone emsdk` → `./emsdk install latest` → `activate latest` → `source emsdk_env.sh`(mac)/`emsdk_env.bat`(win) → `emcc --version` 검증. **버전 핀 ≥ 4.0.10** (emdawnwebgpu 요구), 정확 번호는 `emcc --version` 출력으로 확정. → **플랜에 설치+검증 단계 포함**.

### 7.3 렌더/UI 스택 (D8, D12~D15, 리서치 #1·#3 확정)
- WGPU: **`--use-port=emdawnwebgpu`** (Dawn 계열 webgpu.h, 구 `-sUSE_WEBGPU` 제거). Emscripten ≥ 4.0.10.
- ImGui: 게임 v1.53과 **독립 사본** + `imgui_impl_wgpu`(매크로 `IMGUI_IMPL_WEBGPU_BACKEND_DAWN` 자동) + **`contrib.glfw3` 플랫폼 포트**(게임 GLFW 3.0.4와 분리). 정확 release tag는 플랜에서 1줄 검증 후 핀.
  - 빌드 플래그(참고, 리서치 #1 A4): `--use-port=emdawnwebgpu` `--use-port=contrib.glfw3` `-sDISABLE_EXCEPTION_CATCHING=1` `-sALLOW_MEMORY_GROWTH=1` (+ dev `-sASSERTIONS=1`). `-sNO_FILESYSTEM`/ASYNCIFY vs JSPI는 플랜에서 재검토.
- 첫 스펙 WGPU 산출 (D13~D15): `simple_texture.slang`을 **WGSL로도 산출**(slang_compile.py `--target wgsl`) → 에디터가 `.wgsl` + `.refl.json` + PNG를 fetch → **reflection 구동 bind group** + **`ComputeUVRect`(WASM 포팅) 아틀라스 서브렉트** 렌더. S4 타임라인 sim / #5 셰이더 핫리로드의 씨앗.

### 7.4 dev 서버 (D10, 리서치 #2·compass 확정)
- **cpp-httplib 네이티브 C++ 타겟** — `<include>/httplib.h` 벤더링(OpenSSL 미연결 → 의존 0), `tools/webeditor-server/`에 작은 실행 타겟. **ninja(네이티브) 프리셋으로 빌드** (emscripten 아님).
- `GET` 정적 서빙 + `PUT` JSON 핸들러를 수십 줄로. 루트 = `apps/_MyApp_/resources/data/`. `127.0.0.1` 권장(Windows IPv6 지연 회피).
- `file://` 불가 사유 = 브라우저 CORS → 로컬 HTTP 서버 필수(이 서버가 그 역할 겸함).
- 보안: localhost 전용이라 cpp-httplib CVE 노출 낮음, 단 최신 버전 유지.

## 8. S2 — IDataStore 포트 (D4)

에디터 로직은 `IDataStore`만 의존. Phase 1 어댑터:
| 어댑터 | 역할 |
|---|---|
| `FetchReadAdapter` | `GET ./data/*.json`, `GET ./data/schema/*.schema.json` (시드/스키마 읽기) |
| `IndexedDbDraftAdapter` | 작업 중 드래프트(로컬 DB, 새로고침 생존) |
| `SaveAdapter` | dev 서버 `PUT` (실패 시 File System Access API / download 폴백) |

Phase 2(CRUD 서버)는 이 포트의 어댑터 교체로 끝 — 에디터 로직 무변경.

## 9. S3 — 스키마 구동 ImGui 폼

스키마 `properties`를 순회하며 타입별 위젯 매핑:
| schema type | 위젯 |
|---|---|
| `integer` | `InputInt` (min·max 있으면 `SliderInt`) |
| `number`  | `InputFloat` (min·max 있으면 `SliderFloat`) |
| `string`  | `InputText` |
| `boolean` | `Checkbox` |

- `description` → 위젯 툴팁.
- 저장 전 검증: 타입 일치, `minimum`/`maximum` 범위. 위반 시 폼 경고 + 저장 차단.
- 4종 파일은 탭/리스트로 전환.

## 10. 데이터 흐름 (end-to-end)

1. **편집**: 에디터 부팅 → `FetchReadAdapter`로 schema+json GET → 폼 생성/채움 → 사용자 편집(드래프트는 IndexedDB 자동 저장) → Save → `SaveAdapter` PUT → `resources/data/*.json` 갱신.
2. **소비**: 게임 부팅 → DataTable 로더가 `resources/data/*.json` 파싱 → `ResourceRegistry`에 적재 → (Stage) `WaveController`/`StageConfig`가 `StageData` 참조 → 재컴파일 없이 반영.

## 11. 에러 처리

- 게임측: 관대한 파서(필드 폴백 + warn). 파일 부재 시 전부 코드 기본값 + warn.
- 에디터측: schema↔data 불일치/범위 위반 → 폼 경고, 저장 차단. dev 서버 PUT 실패 → FSAccess/download 폴백.

## 12. 테스트 (권장 — 프로젝트 규칙상 단위테스트는 요청 시에만)

- `DataTable` 로더 Catch2: 정상/누락필드/타입오류/깨진JSON/파일부재 5케이스.
- 에디터 검증 로직: 범위 위반/타입 불일치 차단 확인.

## 13. 스펙 내부 빌드 순서

1. **Stage 워킹 스켈레톤** — S1(nlohmann 벤더링 + DataTable 자원 + StageData + Stage 완전 전환) + S2(editor emscripten 타겟 + `emdawnwebgpu`/ImGui(`contrib.glfw3`) 부팅 + cpp-httplib dev 서버 네이티브 타겟 + IDataStore) + S3(stage 폼) 전부 관통.
2. **밸런스 3종 추가** — Entity/Physics/Audio schema·json·`*Data` 로드 + 에디터 폼 (사용처 전환 없음).
3. **WGPU 아틀라스 프리뷰** — `simple_texture.slang` WGSL 산출 추가 → `ComputeUVRect` WASM 포팅 → reflection JSON 구동 bind group → 아틀라스 서브렉트 1장 렌더.

## 14. 명시적 범위 밖 (Out of Scope)

- **S4 Playable 타임라인 에디터** (AE식 시퀀스 트랙 + WGPU 라이브 sim) — 다음 스펙.
- **프레젠테이션/엔진 배선 데이터** (Playable/VFX/HUD/Bootstrap — vector/vmath/char16_t 구조적 스키마) — 다음 스펙.
- **CRUD 웹서버** (Phase 2) — `IDataStore` 어댑터 교체로 추후.
- Entity/Physics/Audio **사용처 전환** — 본 스펙은 로드까지만.

## 15. 미해결 / 플랜에서 확정할 항목

**리서치(2026-06-19)로 해소됨** — emscripten WebGPU 플래그(→D12 `--use-port=emdawnwebgpu`), 플랫폼 백엔드(→`contrib.glfw3`), dev 서버(→D10 cpp-httplib), 프리뷰 셰이더/바인딩(→D13~D15). 상세 [doc/webeditor/](../../../doc/webeditor/).

**잔여 — 플랜 단계 1줄 검증 (거짓 확신 금지)**:
- **ImGui 정확 release tag** — `example_glfw_wgpu`에 emdawnwebgpu 기본 분기(≥4.0.10 가드) 포함하는 최신 안정 태그를 GitHub에서 확인 후 핀 [리서치 #1 B2].
- **EMSDK 정확 핀 번호** — `emsdk install latest` 후 `emcc --version` 출력으로 확정(하한 4.0.10) [리서치 #2 E1].
- **`-sNO_FILESYSTEM` / ASYNCIFY vs JSPI** — nlohmann가 `FILE*` 미사용인지 확인 후 채택, async 초기화 방식 재검토 [리서치 #1 B3].
- **emdawnwebgpu 포트의 내부 Dawn 스냅샷 버전** — emscripten 버전 종속 가능, 빌드 시 확인 [리서치 #1 D2].

**S4/미래 정찰 (현 스펙 무반영)**: 타임라인 위젯(ImSequencer/ImCurveEdit) [doc/webeditor/04], 셰이더 핫리로드(ImGuiColorTextEdit+compilationInfo) [doc/webeditor/05].
