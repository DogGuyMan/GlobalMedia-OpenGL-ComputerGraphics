# Testing Curriculum — Catch2 Unit Test + Golden Image Snapshot

> 본 문서는 **AI 기반 리팩토링 시 게임 로직 / 렌더링 회귀를 감지**하기 위한 테스트 인프라 구축 계획서다.
>
> **2-Track 전략**:
> - **Track A** (제안 1): `Catch2` 기반 순수 로직 Unit Test -> TDD 워크플로우 확립
> - **Track B** (제안 3): Offscreen FBO 렌더링 + Golden Image diff -> 시각 회귀 안전망
>
> **핸드오프 대상**: 다른 Claude Agent 또는 미래의 본인. 본 계획은 [.claude/architecture.md](../.claude/architecture.md) / [.claude/build-system.md](../.claude/build-system.md) / [doc/migration-plan.md](migration-plan.md) 의 컨벤션을 *전제*로 한다.

## 0. 배경 & 의도

### 왜 이 두 갈래인가
| 회귀 종류 | Track A 로 잡힘 | Track B 로 잡힘 |
|---|:---:|:---:|
| 순수 함수 입출력 변경 (예: 파일 파싱 결과) | ✅ | ❌ |
| 잘못된 행렬 곱셈 / UV 뒤집힘 / 색공간 오류 | ❌ | ✅ |
| Shader uniform 바인딩 누락 | △ | ✅ |
| Buffer offset / stride 실수 | ❌ | ✅ |
| `LoadTextFile` 줄바꿈 처리 | ✅ | ❌ |

OpenGL 코드는 *함수 단위 입출력 검증*만으론 회귀를 못 잡는다. **두 트랙은 서로 다른 회귀를 잡으므로 둘 다 필요**하다.

### TDD vs Characterization 구분
| 상황 | 적합한 모드 |
|---|---|
| **placeholder 모듈을 깨우는 시점** (예: `program/`, `buffer/`) | TDD (Red -> Green -> Refactor) |
| **이미 동작하는 코드를 리팩토링** (예: `shader/`, `common/`) | Characterization (현재 동작을 테스트로 동결 -> 리팩토링) |
| **렌더링 결과 보존** | Golden Image (시각 동결) |

본 커리큘럼은 세 모드를 시점별로 사용한다.

---

## 1. 학습 목표 (Curriculum)

### Track A 학습 항목 (Catch2)
- [ ] Catch2 v3 의존성 추가 (vcpkg) + CTest 통합
- [ ] `TEST_CASE` / `SECTION` / `REQUIRE` / `CHECK` / `REQUIRE_THROWS` 매크로 익히기
- [ ] `catch_discover_tests` 로 개별 케이스가 IDE Test Explorer 에 노출되는지 확인
- [ ] AAA 패턴 (Arrange / Act / Assert) 또는 Given-When-Then 으로 케이스 작성
- [ ] **Characterization 사고법**: "현재 동작이 X 면 그 X 를 테스트로 박는다" 연습
- [ ] 임시 파일 / 임시 디렉토리 처리 패턴 (`std::filesystem::temp_directory_path`)
- [ ] **케이스 도출 방법론** — 부록 A (Boundary Value / CORRECT / 분기 추적 / 적대적 사고)
- [ ] (선택) `GENERATE` / `BENCHMARK` 매크로

### Track B 학습 항목 (Golden Image)
- [ ] Headless GLFW 컨텍스트 (`GLFW_VISIBLE = FALSE`)
- [ ] Framebuffer Object (FBO) + Color Texture Attachment
- [ ] `glReadPixels` -> CPU 메모리로 픽셀 다운로드
- [ ] `stbi_write_png` 로 PNG 저장 (이미 `stb` 의존성 있음 ✅)
- [ ] `stbi_load` 로 골든 PNG 로드 + 픽셀 비교
- [ ] **허용 오차 (tolerance)** 의 필요성 — 드라이버/플랫폼별 1~2 LSB 차이는 정상
- [ ] 골든 갱신 워크플로우 (`--update-golden` 플래그)
- [ ] 플랫폼별 골든 분리 전략 (`golden/macos-arm64/` vs `golden/win-x64/`)

---

## 2. 의존성 그래프

```
Phase A1 Catch2 인프라
    ↓
Phase A2 첫 Unit Test (common::LoadTextFile)
    ↓
Phase A3 추가 모듈 단위 테스트 (glfw_input_utils, diagnostics 일부)
    │
    │   (Track B 는 Track A 와 병렬 진행 가능 — 단, A1 인프라 필요)
    ↓
Phase B1 Headless GL 컨텍스트 헬퍼 (test 전용 fixture)
    ↓
Phase B2 FBO 렌더 + glReadPixels -> PNG 저장
    ↓
Phase B3 첫 골든 시나리오 (clear color)
    ↓
Phase B4 진짜 시나리오 — simple.vs/.fs 로 사각형 한 장
    ↓
(선택) Phase C  CI / GitHub Actions 통합
```

**migration-plan.md 와의 관계**: 본 커리큘럼의 Phase B4 는 migration-plan.md Phase 4 (`context` 모듈) 시점과 맞물려 "이 시점의 렌더 결과" 를 골든으로 박는 데 활용한다.

---

## 3. Phase 계획

각 Phase 의 완료 조건은 **`ctest --preset debug` 통과**.

---

### Phase A1: Catch2 인프라 활성화 ✅ 완료

- [x] [vcpkg.json](../vcpkg.json) 에 `"catch2"` 추가
- [x] [test/CMakeLists.txt](../test/CMakeLists.txt) 주석 해제 + `catch_discover_tests`
- [x] `cmake --preset debug` 재구성 -> `vcpkg install` 이 catch2 다운로드 확인
- [x] 비어있어도 좋으니 빌드만 통과 (다음 Phase 에서 첫 케이스 작성)
- [ ] **CMake 검증 스니펫** ([test/CMakeLists.txt](../test/CMakeLists.txt) 권장 형태):
  ```cmake
  find_package(Catch2 3 CONFIG REQUIRED)

  add_executable(test_common test_common.cpp)
  target_link_libraries(test_common PRIVATE
      Catch2::Catch2WithMain
      SJH::common
  )
  target_compile_features(test_common PRIVATE cxx_std_17)

  include(CTest)
  include(Catch)
  catch_discover_tests(test_common)
  ```

> **결정 사항**: Catch2 **v3** 사용 (vcpkg default). v2 와 v3 는 헤더 경로(`<catch2/catch.hpp>` vs `<catch2/catch_test_macros.hpp>`)가 다르니 주의.

---

### Phase A2: 첫 Unit Test — `common::LoadTextFile` 🟡 진행 중

목적: TDD/Characterization 사이클을 한 번 돌려보고 CTest 흐름 확립.

- [x] `test/test_common.cpp` 작성 — 다음 케이스 최소 3개:
  1. **존재하는 파일** -> `std::optional<std::string>` 이 값 보유, 내용 일치
  2. **존재하지 않는 파일** -> `nullopt`
  3. **빈 파일** -> 빈 문자열 보유 (nullopt 아님 — 현재 동작 확인 필요)
- [x] **임시 파일 처리**: `std::filesystem::temp_directory_path() / "sjh_test_XXXX.txt"` + `RAII` 가드 (테스트 종료 시 자동 삭제)
- [x] `ctest --preset debug --output-on-failure` 통과 (3/3 통과)
- [x] **Characterization 관점**: 케이스 3 의 *현재* 결과를 그대로 테스트에 박는다 (만약 빈 파일이 `nullopt` 라면 그것이 현재 계약)
- [ ] DoD: 일부러 `LoadTextFile` 의 한 줄을 깨뜨려 테스트가 **실패하는지** 확인 후 원복 (테스트가 실제로 회귀를 잡는지 검증)

---

### Phase A3: 단위 테스트 확장 🟡 진행 중

후보 (난이도 낮은 순):

- [x] `test/test_glfw_utils.cpp` — [include/input/glfw_input_utils.h](../include/input/glfw_input_utils.h) 의 `ActionToString`, `ModCtrl/Shift/Alt`
  - [x] constexpr 함수 -> `STATIC_REQUIRE` 사용 (컴파일 타임 검증, 런타임 비용 0)
  - [x] 모든 enum 분기 커버 (PRESS/RELEASE/REPEAT/default + Mod 비트 0/단독/혼합)
  - [x] **함수 간 독립성 케이스** — 적대적 사고에서 도출 (한 함수의 비트 누설 회귀 검출)
  - [ ] `ctest --preset debug` 통과 — 사용자 차례
  - [ ] DoD: 사보타주 (`return "Pressed"` → `return "Press"`) 로 회귀 감지 검증
- [ ] `test/test_diagnostics.cpp` — `diagnostics::GLObjectLog::CheckShaderCompile` 의 *순수 부분만* (예: 로그 메시지 포맷팅 함수가 분리되어 있다면)
  - **GL 호출이 들어가는 함수는 Track B 로 미룬다**
- [ ] **금지 영역**: 진짜 GL 컨텍스트 필요한 코드는 여기서 다루지 않음 -> Phase B 로 이전

---

### Phase B1: Headless GL 컨텍스트 헬퍼 ✅ 완료

목적: 모든 골든 테스트가 공유할 GL fixture 만들기.

- [x] `test/support/gl_test_fixture.h/.cpp` 신설 — `SJH::test::GLContextFixture` 클래스 (forward-decl `GLFWwindow`).
- [x] **macOS 강제 사항** 적용 (CONTEXT_VERSION 3.3 + CORE + FORWARD_COMPAT + VISIBLE=FALSE).
- [x] `gladLoadGLLoader` 호출 + 실패 시 `throw std::runtime_error` (Catch2 가 자동 캐치).
- [x] **테스트 전체에서 GLFW 1회만 init** — `static` 가드 + `std::atexit` 으로 처리.
- [x] [test/test_gl_fixture.cpp](../test/test_gl_fixture.cpp) — fixture 자가검증 통과.

### Phase B1.5: 모듈별 GL 상태 단위 테스트 ✅ 진행 중 (선택 옵션)

Phase 1 모듈 마이그레이션과 *동시 진행* — golden image (B2~B4) 전 단계로, *모듈별 1-2 단언* 만 추가.

- [x] [test/test_gl_debug.cpp](../test/test_gl_debug.cpp) — `GLDebug::CheckGL*` 7개 함수 happy path + negative (BindVAO bad / EnableVAA out-of-range / BufferData unbound).
- [x] [test/test_uniform_diagnostics.cpp](../test/test_uniform_diagnostics.cpp) — `UniformDiagnostics` warn-once 트래커 smoke (GL context 불필요, 가장 빠름).
- [x] [test/test_program_uniforms.cpp](../test/test_program_uniforms.cpp) — 인라인 GLSL 컴파일/링크 후 `ProgramUniforms` location 캐시 + setter 동작.
- [x] **현재 상태**: 21/21 테스트 통과 (`ctest --test-dir build_Darwin --output-on-failure`).

---

### Phase B2: FBO 렌더 + 픽셀 캡처 헬퍼

- [ ] `test/support/fbo_capture.h/.cpp` — 다음 함수:
  ```cpp
  // FBO 생성 -> 콜백 안에서 렌더 -> glReadPixels -> RGBA8 vector 반환
  std::vector<uint8_t> CaptureFrame(int w, int h, std::function<void()> render);

  // 픽셀 vector 를 PNG 로 저장 (디버깅 / 골든 갱신용)
  void WritePng(const std::filesystem::path& out, int w, int h, const std::vector<uint8_t>& pixels);

  // 두 픽셀 vector 비교 — 채널별 절대 차이 ≤ tolerance 인 픽셀 비율
  struct DiffResult { double matchRatio; int maxChannelDiff; };
  DiffResult ComparePixels(const std::vector<uint8_t>& a,
                           const std::vector<uint8_t>& b,
                           int tolerancePerChannel = 2);
  ```
- [ ] **stb 사용**: `#define STB_IMAGE_IMPLEMENTATION`, `#define STB_IMAGE_WRITE_IMPLEMENTATION` 은 *이 .cpp 에만* 한 번 (다중 정의 금지 — image 모듈 신설 시 충돌 주의, migration-plan.md Phase 1-C 와 같은 .cpp 에서 정의 금지)
- [ ] **OpenGL Y축 뒤집힘 처리**: `glReadPixels` 결과는 bottom-left origin -> PNG 저장 시 row 뒤집기 또는 비교 시 일관되게만 처리
- [ ] DoD: 단색 클리어 (`glClearColor(1, 0, 0, 1) -> glClear`) -> 캡처 -> 모든 픽셀 (255, 0, 0, 255)

---

### Phase B3: 첫 골든 시나리오 — Clear Color

가장 단순한 시나리오로 인프라 검증.

- [ ] `test/test_render_clear.cpp`:
  ```cpp
  TEST_CASE("clear color matches golden", "[render][golden]") {
      SJH::test::GLContextFixture ctx(256, 256);
      auto pixels = CaptureFrame(256, 256, []{
          glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
          glClear(GL_COLOR_BUFFER_BIT);
      });
      auto golden = LoadGolden("clear_color_256.png");
      auto diff = ComparePixels(pixels, golden);
      REQUIRE(diff.matchRatio > 0.999);
  }
  ```
- [ ] 골든 PNG 생성 워크플로우:
  - 환경변수 `SJH_UPDATE_GOLDEN=1` 일 때 캡처 결과를 `test/golden/<name>.png` 로 저장하고 테스트 통과
  - 평소엔 비교만
- [ ] `test/golden/.gitignore` 결정: 골든은 **커밋해야 함** (회귀 비교 기준이므로)
- [ ] 플랫폼별 골든: `test/golden/macos-arm64/`, `test/golden/win-x64/` 분리

---

### Phase B4: 실 시나리오 — Simple Shader 사각형

migration-plan.md Phase 4 (`context` 모듈 통합) 직후에 실행하는 게 자연스러움.

- [ ] [resources/shader/simple.vs](../resources/shader/simple.vs), `simple.fs` 로드
- [ ] 작은 quad VAO/VBO/EBO 직접 구성 (또는 `SJH::buffer` + `SJH::layout` 활성화 후 사용)
- [ ] uniform 1개 이상 바인딩 (예: `uTime = 0.5`) -> 결정론적 렌더 보장
- [ ] 캡처 -> `test/golden/<platform>/quad_simple.png` 와 비교
- [ ] DoD: shader.cpp 또는 simple.vs 의 한 줄을 의도적으로 깨뜨리면 테스트 실패 확인

> **이 시점이 진짜 안전망**: 이후 AI 리팩토링이 Buffer / Layout / Program / Context 의 어느 곳을 수정하든 *렌더링 결과가 동일* 해야 통과한다.

---

### Phase C (선택): CI 통합

우선순위 낮음. 로컬 워크플로우 안정화 후.

- [ ] GitHub Actions: macOS-latest, windows-latest 에서 `cmake --preset && ctest --preset`
- [ ] 실패한 테스트의 캡처 PNG / diff PNG 를 artifact 로 업로드 (디버깅 핵심)
- [ ] 플랫폼별 골든 자동 갱신 PR 워크플로우 (`workflow_dispatch` 트리거)

---

## 4. 디렉토리 구조 제안

```
test/
├── CMakeLists.txt                 # 모든 test_*.cpp 를 등록
├── test_common.cpp                # Phase A2
├── test_glfw_utils.cpp            # Phase A3
├── test_render_clear.cpp          # Phase B3
├── test_render_quad.cpp           # Phase B4
├── support/
│   ├── gl_test_fixture.h/.cpp     # Phase B1
│   └── fbo_capture.h/.cpp         # Phase B2
└── golden/
    ├── macos-arm64/
    │   ├── clear_color_256.png
    │   └── quad_simple.png
    └── win-x64/
        ├── clear_color_256.png
        └── quad_simple.png
```

각 `test_*.cpp` 는 **독립 실행 파일** (Catch2WithMain 링크) — 빠른 빌드 + 명확한 격리.

---

## 5. 의존성 추가 명세

### vcpkg.json 변화 (현재)
```json
{
    "name": "opengl-with-cmake",
    "version": "0.1.0",
    "dependencies": [
        "fmt",
        "spdlog",
        "glfw3",
        "glad",
        "stb",
        "catch2"   // ← 추가됨
    ]
}
```

### 추가 의존성 불필요한 항목 (이미 있음)
- 픽셀 비교: `stb_image` (헤더온리, 이미 vcpkg 에서 설치됨)
- PNG 쓰기: `stb_image_write` (`stb` 패키지에 포함)
- GL 컨텍스트: `glfw3`, `glad` (이미 있음)

---

## 6. 함정 & 리스크

| 위험 | 영향 | 완화 |
|---|---|---|
| Catch2 v2/v3 헤더 혼용 | 컴파일 에러 | v3 통일, `Catch2::Catch2WithMain` 타겟 사용 |
| `STB_IMAGE_IMPLEMENTATION` 다중 정의 | 링크 에러 | test 전용 .cpp 1곳에서만 define, image 모듈 .cpp 와 분리 |
| 플랫폼별 GPU 드라이버 픽셀 차이 | False positive 회귀 | tolerance ≥ 1, 플랫폼별 golden 분리 |
| FBO 색공간 (sRGB / linear) | 인간 눈으론 모르나 픽셀 diff 거대 | 명시적으로 `GL_RGBA8` (non-sRGB) 사용 |
| Y축 뒤집힘 (glReadPixels vs PNG) | 골든이 위아래 반전 | 캡처/비교 양쪽에서 *일관된* 처리만 하면 OK |
| GLFW 다중 초기화 / 종료 | 테스트 행 / 크래시 | `glfwInit/Terminate` 는 프로세스 1회 — 정적 RAII 가드 |
| macOS GL 3.3 강제 | 4.3+ 기능 (compute shader, debug callback) 사용 시 | Track B 시나리오는 GL 3.3 호환 기능만 사용 |
| 골든 PNG 가 git LFS 필요할 만큼 커짐 | repo 비대 | 256×256 등 작게 유지, 시나리오당 하나만 |
| AI 리팩토링이 *의도된* 시각 변경 포함 | 테스트가 깨지는데 정상 | `SJH_UPDATE_GOLDEN=1` 워크플로우 + PR 시 골든 변경 리뷰 |

---

## 7. Definition of Done (DoD)

각 Phase 종료 시:

- [ ] `cmake --build build_Darwin` 통과 (warning 없이)
- [ ] `ctest --preset debug --output-on-failure` 통과
- [ ] 새로 추가된 케이스가 의도적으로 코드를 깨뜨릴 때 **실패** 하는지 확인 (테스트 자체의 회귀 감지력 검증)
- [ ] [.claude/MEMORY.md](../.claude/MEMORY.md) 의 테스트 섹션 갱신 (활성/비활성 표시)
- [ ] git commit (Phase 단위로 분리)

---

## 8. 진행 순서 요약

```
Phase A1 Catch2 인프라  ✅ 진행 중
   ↓
Phase A2 LoadTextFile 단위 테스트     ← 첫 TDD 사이클 경험
   ↓
Phase A3 입력 유틸 단위 테스트 확장
   ↓
Phase B1 Headless GL fixture
   ↓
Phase B2 FBO 캡처 헬퍼
   ↓
Phase B3 Clear color 골든 시나리오   ← Track B 인프라 검증
   ↓
Phase B4 Quad shader 골든 시나리오   ← migration-plan Phase 4 와 맞물림
   ↓
Phase C  CI 통합 (선택)
```

---

## 9. 핸드오프 시 즉시 알아야 할 것

1. **Catch2 는 v3** — `<catch2/catch_test_macros.hpp>` / `Catch2::Catch2WithMain` 타겟.
2. **macOS 는 GL 3.3 강제** — Headless 컨텍스트도 3.3 Core + Forward-Compat hint 필수.
3. **골든 PNG 는 커밋** — 비교 기준이므로 .gitignore 에 넣지 말 것.
4. **플랫폼별 골든 분리** — `test/golden/<platform>/` 하위. 한 골든을 모든 플랫폼이 공유하면 false positive 다발.
5. **`STB_*_IMPLEMENTATION`** 매크로는 *프로젝트 전체에서 .cpp 1곳에만* — `image/image.cpp` 와 `test/support/fbo_capture.cpp` 가 동시에 정의하면 링크 에러.
6. **Track A 와 Track B 는 독립** — 어느 한쪽이 막혀도 나머지는 진행 가능.
7. **`migration-plan.md Phase 4` 직후가 Phase B4 의 적기** — context 가 완성되어야 의미있는 골든 시나리오가 나옴.
8. **test_*.cpp 는 각자 독립 실행파일** — `Catch2WithMain` 으로 main 자동 생성. 빌드 시간 vs 격리의 트레이드오프에서 격리를 택함.

---

## 부록 A: 테스트 케이스 발상법 (How to think when writing tests)

> "어떻게 그런 케이스를 떠올리지?" 에 대한 답. **천재의 직감이 아니라 체크리스트 사고**.
> Phase A2 (`LoadTextFile`) 작업 중 도출된 방법론을 정리한 것 — Phase A3 / B3 / B4 의 케이스 도출 시 동일 절차 반복 적용.

### A.1 핵심 원리: 발상이 아니라 절차

좋은 테스트 작성자는 천재라서가 아니라 *빠뜨리지 않는 절차* 를 따른다. 4가지 휴리스틱(A.2~A.5)이 케이스의 80%를 자동 생성하고, 나머지 20%만 *경험으로 카테고리화* (A.8) 된다.

### A.2 휴리스틱 1 — Boundary Value Analysis (경계값 분석)

> "버그는 경계에 산다" — 정상값 한가운데가 아니라 0/1, 끝-1/끝/끝+1, 비어있음/하나/많음 같은 *전환점* 에서.

`LoadTextFile` 적용 예:
| 차원 | 경계값 |
|---|---|
| 파일 존재 | 없음 / 있음 |
| 파일 크기 | 0바이트 / 1바이트 / 큰 파일 |
| 줄 수 | 0줄 / 1줄 / 여러 줄 |
| 인코딩 | ASCII / UTF-8 / BOM |

→ 이 표만 그리면 케이스가 *기계적으로* 나옴. 직감 불필요.

### A.3 휴리스틱 2 — CORRECT 7글자 (Pragmatic Unit Testing)

함수 하나에 대해 7가지 차원으로 자문:

| 글자 | 의미 | 예시 질문 |
|---|---|---|
| **C**onformance | 입력이 형식에 맞나? | 빈 문자열 경로? 절대/상대? |
| **O**rdering | 순서가 중요한가? | (해당 함수에 적용 가능 시) |
| **R**ange | 범위 끝은? | 0 / 매우 큰 값 |
| **R**eference | 외부 의존? | 디스크, 권한, 다른 프로세스 |
| **E**xistence | 존재하는가? | null, 없는 파일, 빈 컨테이너 |
| **C**ardinality | 0 / 1 / N? | 빈 / 단일 / 다수 원소 |
| **T**ime | 타이밍? | 동시성, race, TTL |

→ CORRECT 만 돌려도 핵심 80% 도출.

### A.4 휴리스틱 3 — 구현 분기 추적 (Characterization 특화)

> 코드를 읽고 **모든 분기** (`if`/`else`/`switch` + 암묵적 분기) 를 케이스로 만든다.

`LoadTextFile` 의 분기:
```cpp
std::ifstream fin(filename);
if(!fin.is_open()) return {};   // ← 분기 A: nullopt 경로 (테스트 2)
text << fin.rdbuf();            // ← rdbuf 가 만들 수 있는 값?
return text.str();              // ← 분기 B: "" (테스트 3) 또는 내용 (테스트 1)
```

**규칙**: 코드의 분기 수보다 테스트 케이스가 적으면 빠뜨린 거.

**타입 시그니처도 분기로 셈한다**: `optional<T>` 는 상태 2개, `vector<T>` 는 0/1/N 3개, `Result<T,E>` 는 2개. 각 상태를 *어떤 입력이* 만드는가를 역추적하면 케이스가 나온다.

### A.5 휴리스틱 4 — 적대적 사고 (AI 리팩토링 안전망 특화)

> "내가 AI라면 이 코드를 어떻게 *그럴듯하게* 바꿀까?"

본 프로젝트의 의도(*AI 리팩토링 회귀 감지*)에 가장 직접적인 휴리스틱.

**절차**:
1. 코드 읽기
2. 5가지 "리팩토링" 후보 생성 — 명백히 틀린 게 아니라 *그럴듯해 보이는* 것만
3. 통과시키지 말아야 할 변경 → 그걸 잡는 테스트 필요
4. 통과해도 되는 변경 → 테스트 만들지 말 것 (*cement test* 회피)

`LoadTextFile` 적용 예:
| AI 가 할 만한 "리팩토링" | 통과 OK? | 그럼 테스트는? |
|---|---|---|
| `text.str()` → `std::move(s)` 최적화 | OK (동작 동일) | 불필요 |
| 빈 파일을 `nullopt` 로 매핑 | **NO** (계약 변경) | **테스트 3** ← 이렇게 도출 |
| 한 줄씩 읽어서 trim 추가 | **NO** (줄바꿈 손실) | **테스트 1** (멀티라인 expected) |
| 예외 던지기 | **NO** (계약 변경) | **테스트 2** (REQUIRE_FALSE) |
| 1MB 이상 파일 거부 | 정책 결정 필요 | 결정 후 |

→ 5분 만에 케이스 자동 생성. 발상이 아니라 *목록*.

### A.6 5분 워크플로우 — 매 함수마다

1. **타깃 함수 코드 정독** — 분기/타입/암묵적 상태 식별 (3분)
2. **CORRECT 7글자 자문** — 적용되는 차원만 적기 (1분)
3. **"AI라면?" 5개 생성** — 통과시키지 말아야 할 것만 추리기 (1분)

→ 중복 제거하면 보통 3~6 케이스로 수렴. Phase A2 의 3개도 이 절차의 산물.

### A.7 테스트의 의미 — "옳음" 이 아니라 "계약"

> 테스트는 코드가 *옳다* 를 증명하지 않는다. **현재의 계약(contract)** 을 *동결* 한다.

`LoadTextFile` 의 빈 파일 처리에는 *옳고 그름이 없다*. 두 해석 모두 합리적:
- **해석 A (현재)**: "파일이 열리는가" 가 성공/실패 기준 → `optional("")`
- **해석 B (대안)**: "의미있는 내용이 있는가" → `nullopt`

테스트 3은 *해석 A 가 옳다* 고 주장하지 않는다. **현재 해석 A 라는 사실** 을 박는다. 미래에 의식적으로 B 로 바꾸려면 — 테스트를 *의식적으로* 수정하면서 모든 호출자를 *의식적으로* 검토하게 된다. 이 "의식적 검토의 강제" 가 테스트의 진짜 가치.

#### "그럴듯해 보이는 변경" 이 가장 위험한 이유

명백히 틀린 변경은 코드 리뷰가 잡는다. 하지만 *그럴듯한* 변경은 통과한다. **테스트만이 잡을 수 있다**.

`LoadTextFile` + `if(s.empty()) return {};` 변경 분석:
- 호출자 [src/shader/shader.cpp:25-27](../src/shader/shader.cpp#L25-L27) 는 `has_value()` 만 체크 → 빈 셰이더가 *컴파일러 진단* 으로 가는 경로 차단됨
- 에러 카테고리가 "shader compile error: empty input" → "failed to open file" 로 *조용히* 변형 (파일은 분명히 열렸음에도)
- 미래의 정책 (빈 셰이더 stub 처리, 빈 설정 = 기본값) 가능성 영구 차단

#### 정보 손실 (information loss) 의 비대칭성

`optional<T>` 는 상태 2개 — `nullopt` / `optional(value)`. 입력 공간(파일 시스템)은 3개 — 없음 / 빈 파일 / 내용있음. 3→2 매핑에서 **두 다른 실패를 한 신호로 합치면** 호출자는 영원히 둘을 구별 못함. 이 손실은 *한 줄 변경* 으로 발생하고 *복구 불가능*.

### A.8 경험으로만 배우는 카테고리

휴리스틱으로도 못 잡고 *카테고리화 비용을 지불해야* 하는 영역. 한 번 당하면 평생 안 잊는 패턴들:

| 패턴 | 한번 카테고리화하면 |
|---|---|
| Off-by-one | ±1 경계 항상 검증 |
| Unicode / 한글 / 이모지 | 다국어 케이스 항상 |
| 병행성 race | 멀티스레드 시 항상 |
| 빈 컬렉션 ≠ null ≠ 1개 | Cardinality 항상 |
| GPU 플랫폼 차이 | 골든 tolerance 항상 |
| 시간대 (TZ) | TZ-aware 시간 항상 |
| Floating point 비교 | epsilon 항상 |
| 파일 시스템 권한 / case sensitivity | OS별 케이스 항상 |

카테고리는 유한 (~50개). 한 번 들어가면 평생 자산.

### A.9 추천 자료

- *Pragmatic Unit Testing in C++* (Hunt & Thomas) — CORRECT 휴리스틱 출처
- *Working Effectively with Legacy Code* (Michael Feathers) — Characterization 의 원전
- 본 프로젝트의 `git log` — 과거 버그 = 미래 버그 카테고리
- 회사 / 오픈소스 postmortem 글 — 실제 회귀 사례 카테고리화

---

## 10. 참조

- [doc/migration-plan.md](migration-plan.md) — 모듈 마이그레이션 로드맵 (B4 와 시점 동기화)
- [.claude/architecture.md](../.claude/architecture.md) — 모듈 디자인, namespace, Try* 컨벤션
- [.claude/build-system.md](../.claude/build-system.md) — CMake 빌드 시스템 상세
- [.claude/MEMORY.md](../.claude/MEMORY.md) — 프로젝트 인덱스
- [Catch2 v3 docs](https://github.com/catchorg/Catch2/blob/devel/docs/Readme.md) — 매크로 레퍼런스
- [stb_image_write usage](https://github.com/nothings/stb/blob/master/stb_image_write.h) — PNG 출력 API
