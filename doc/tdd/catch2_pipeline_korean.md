# OpenGL 렌더러를 위한 Catch2 중심 TDD·적대적 검증 파이프라인: 빌드 가능한 리서치 보고서

## 핵심 요약 (TL;DR)
- **Catch2 v3 + CTest 코어를 구축하고, 그 위에 Mull(LLVM IR 뮤테이션 테스팅)과 커스텀 골든이미지 회귀 하네스를 얹은 뒤, 커스텀 Claude Code 스킬로 오케스트레이션하라** — 단위 테스트, 뮤테이션, 퍼징 계층은 실제로 성숙하고 Catch2와 기성 호환되지만, **골든이미지 회귀 계층과 모든 Claude Code 스킬은 직접 구축해야 한다.** 시장에 C++/OpenGL/Catch2를 인지하는 것이 하나도 없기 때문이다.
- **Mull이 올바른 뮤테이션 엔진이다**: 모든 Catch2 테스트 실행 파일을 `mull-runner`를 통해 불투명 바이너리로 실행하고, 증분 `gitDiffRef` 브랜치-diff 뮤테이션을 지원하며, 활발히 유지보수된다(v0.27.1, LLVM 13.0~22.0 전부 지원). Catch2 v3는 Ninja에서 FetchContent + `catch_discover_tests()`로 마찰 없이 연결된다.
- **C++/Catch2 테스트 생성을 목적으로 만들어진 Hugging Face 모델은 없다** — Catch2용으로 프롬프팅한 범용 코드 LLM(Qwen2.5/3-Coder, StarCoder2)을 쓰고, LLM이 작성한 테스트는 라인 커버리지가 아니라 Mull의 뮤테이션 점수로 검증하라. LLM 테스트 생성은 자율 컴포넌트가 아니라 보조 가속기로 취급할 것.

## 주요 발견

1. **Catch2 v3 / CMake / Ninja 배선은 이미 해결됐고 마찰이 없다.** 고정 태그로 FetchContent, `Catch2::Catch2WithMain` 링크, `extras` 디렉터리를 모듈 경로에 추가, 그 후 `catch_discover_tests()`가 모든 `TEST_CASE`를 CTest에 등록. Ninja와 CTest 병렬화(`ctest -j`)는 기본으로 동작. 유일한 실제 함정: FetchContent 사용 시 `${catch2_SOURCE_DIR}/extras`를 `CMAKE_MODULE_PATH`에 추가하지 않으면 `include(Catch)`가 실패.

2. **Mull은 Catch2와 깔끔하게 동작한다.** 프레임워크가 아니라 컴파일된 테스트 바이너리 위에서 동작하기 때문이다. 테스트 실행 파일을 `-fpass-plugin=mull-ir-frontend-<v>`로 컴파일한 뒤 `mull-runner-<v> ./tests` 실행. 생존한 뮤턴트 = Catch2 단언이 놓친 의미적 공백. 이것이 사용자가 원하는 바로 그 적대적 신호다.

3. **골든이미지 회귀는 커스텀이어야 한다.** OpenGL 프레임을 Catch2 `TEST_CASE` 안에서 렌더링하고, 프레임버퍼를 읽어 들이고, 커밋된 PNG와 허용 오차를 두고 비교하는 기성 도구는 없다. 그러나 모든 *컴포넌트*는 존재한다: 결정론적 헤드리스 렌더링을 위한 EGL surfaceless / llvmpipe, PNG I/O를 위한 `stb_image`/`stb_image_write`, 그리고 커스텀 Catch2 매처에 임베드할 여러 성숙한 C++ 픽셀-diff 라이브러리(pixelmatch-cpp17, perceptualdiff).

4. **퍼징은 Catch2와 사소하게 공존한다.** 별도 CMake 타깃을 통해서다. libFuzzer 하네스(`LLVMFuzzerTestOneInput`)는 자체 실행 파일에 존재하고, 크래시 재현물은 곧장 Catch2 회귀 `TEST_CASE`로 전환된다. RapidCheck는 프로퍼티 기반 테스팅을 위한 1급 Catch2 통합 헤더(`rapidcheck/catch.h`)를 제공하여 Catch2 자체의 `GENERATE()`를 보완한다.

5. **TDD/뮤테이션용 Claude Code 스킬은 존재하지만 C++ 인지 능력은 없다.** obra/superpowers, nizos/tdd-guard, citypaul/.dotfiles는 성숙하고 인기 있으나, 이들의 뮤테이션 테스팅 스킬은 Mull/Catch2가 아니라 JS/TS(Stryker)를 대상으로 한다. 커스텀 스킬을 직접 작성해야 한다.

## 세부 내용

### 1. Catch2 v3 + CMake/Ninja 배선 (현행 모범 사례)

**버전과 라이선스.** Catch2 v3가 현행 라인이다 — **Catch2 v3.15.0이 최신 태그 릴리스(2026년 5월 12일), v3.14.0이 2026년 4월 5일** (프로젝트가 자주 태깅하므로 고정 전 라이브 확인). Catch2는 **Boost Software License 1.0**(관대, 바이너리 내 출처 표기 불필요) 라이선스. 결정적으로, **Catch2 v3는 더 이상 헤더 전용이 아니다**: 별도로 컴파일되는 정적 라이브러리이며 **C++14 이상**을 요구. 헤더 전용 모델은 v2.x 브랜치에 동결됨. 이것이 가장 큰 마이그레이션 함정 — v3는 헤더를 모듈식 인클루드(`<catch2/catch_test_macros.hpp>`, `<catch2/matchers/catch_matchers.hpp>` 등)로 분할하며 실제 라이브러리를 링크.

**FetchContent vs find_package.** 둘 다 1급:
- `find_package(Catch2 3 REQUIRED)` — Catch2가 시스템 전역(vcpkg, Conan, 배포판, 또는 `make install`)에 설치된 경우 사용. `Catch.cmake` 모듈(따라서 `catch_discover_tests`)을 자동으로 사용 가능하게 함.
- `FetchContent` — 의존성을 인트리에 고정하는 밀폐·재현 가능 빌드용. **함정:** 모듈 경로를 수동으로 확장하지 않으면 `include(Catch)`가 실패.

표준 FetchContent 배선(Ninja 호환, 제너레이터에 변경 불필요):

```cmake
cmake_minimum_required(VERSION 3.24)
project(renderer_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # Mull/Dextool이 나중에 요구

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.15.0            # 현행 릴리스에 고정; 의도적으로만 올림
)
FetchContent_MakeAvailable(Catch2)

add_executable(unit_tests test_math.cpp test_pipeline.cpp)
target_link_libraries(unit_tests PRIVATE Catch2::Catch2WithMain renderer_lib)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)  # include(Catch)에 필수
include(CTest)
include(Catch)
catch_discover_tests(unit_tests
  TEST_PREFIX "unit."
  REPORTER junit                 # CI 수집용 JUnit XML
  OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)
```

**타깃.** `Catch2::Catch2WithMain`(라이브러리 + 기본 `main`)이 기본 선택; 자체 `main()`을 제공할 때만 `Catch2::Catch2` 사용(어떤 테스트보다 먼저 EGL 컨텍스트를 초기화해야 하는 골든이미지 하네스에 관련 — §3 참조).

**`catch_discover_tests()` 메커니즘.** 빌드/테스트 시점에 `--list-tests`로 빌드된 실행 파일을 실행하고 출력을 파싱하여, 각 케이스를 CTest에 개별 등록. 이로써 `ctest -R`, 라벨, 병렬화에 대한 테스트별 세분성 확보. 전체 시그니처는 `TEST_SPEC`, `EXTRA_ARGS`, `PROPERTIES`, `TEST_LIST`, `REPORTER`, `OUTPUT_DIR` 지원.

**태그, 라벨, 서브셋, 병렬화.** Catch2 태그(`TEST_CASE("renders triangle", "[golden][gpu]")`)는 테스트 스펙 필터링을 통해 CTest에 매핑. 서브셋 실행은 `ctest -R unit.` 또는 실행 파일을 통한 태그(`./unit_tests "[golden]"`). 병렬 실행: `ctest -j$(nproc)`(프로세스 수준, GPU/상태 무거운 테스트에 견고한 선택) — Catch2 자체의 인프로세스 병렬화는 제한적이므로, 각자 GL 컨텍스트를 쥐는 골든이미지 테스트에는 CTest 수준 샤딩을 선호.

**뮤테이션/계측을 위한 테스트 타깃 구조화.** 프로덕션 코드를 테스트가 링크하는 라이브러리 타깃(`renderer_lib`)에 둘 것. 이것이 필수다: Mull과 커버리지 계측은 *라이브러리* 컴파일(해당 타깃의 컴파일러 플래그 또는 전용 빌드 디렉터리 통해)에 적용되고, Catch2 실행 파일은 테스트 드라이버로 남는다. 일반 개발 빌드가 빠르게 유지되도록 뮤테이션 빌드용 별도 CMake 구성(또는 `option()`으로 게이트된 플래그 세트)을 정의 — §6 참조.

### 2. Catch2 스위트에 Mull 뮤테이션 테스팅

**Mull이 무엇이고 현재 상태.** Mull은 C/C++용 LLVM-IR 기반 뮤테이션 테스팅 도구. **현행 문서 라인은 Mull 0.27.1; 직전 태그 릴리스는 0.27.0(2025년 10월 20일, LLVM 19 지원 추가), 0.26.1은 2025년 4월.** Alex Denisov와 Stanislav Pankevich가 활발히 유지보수(저작권 2016–2026). **최신 "How Mull works" 문서(v0.27.1)에 따르면 Mull은 LLVM 13.0~22.0의 모든 현대 버전에 걸쳐 macOS와 다양한 Linux 시스템을 훌륭히 지원한다.** 도구 바이너리는 버전 접미사가 붙는다(예: `mull-runner-19`, `mull-ir-frontend-19`). IR 프론트엔드가 특정 LLVM/Clang에 묶인 Clang 플러그인이기 때문. 레포 LICENSE에서 정확한 SPDX 라이선스 태그를 확인할 것.

**동작 방식.** Mull은 모든 뮤턴트를 LLVM-IR 수준에서 단일 바이너리에 주입하고, 각 뮤턴트는 런타임 조건 뒤에 숨김. 이후 `mull-runner`가 뮤턴트당 한 번씩 테스트 바이너리를 (자식 서브프로세스로) 실행하고 사살을 기록. Clang AST 정보("junk detection")로 소스에 깔끔히 매핑되지 않는 IR 뮤턴트를 폐기. 2021년 1월 이후 LLVM JIT를 더 이상 사용하지 않음.

**왜 Catch2가 그냥 동작하는가.** Mull은 *임의의 테스트 실행 파일*을 실행하고 프로세스 종료 코드만 신경 씀: 0 = 테스트 통과(뮤턴트 생존), 0이 아님 = 테스트 실패(뮤턴트 사살). `Catch2WithMain` 바이너리는 단언이 하나라도 실패하면 0이 아닌 값을 반환하므로, Mull은 추가 설정 없이 사살을 정확히 읽음. 이는 Mull 자체 문서의 GoogleTest와 fmtlib 예제에서 동작하는 것과 동일한 메커니즘.

**구체적 CMake + Ninja 레시피.** 검증 대상 코드(와 테스트 바이너리)를 Clang과 Mull 플러그인으로 컴파일한 뒤 러너 실행:

```bash
# Clang + Mull IR 플러그인으로 전용 뮤테이션 빌드 디렉터리 구성
cmake -G Ninja -S . -B build-mull \
  -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_CXX_FLAGS="-O0 -g -grecord-command-line -fpass-plugin=/usr/lib/mull-ir-frontend-19"
ninja -C build-mull unit_tests
mull-runner-19 ./build-mull/unit_tests
```

핵심 플래그: `-fpass-plugin=`은 뮤테이터 로드; `-g -grecord-command-line`은 junk detection/소스 매핑에 필요; `-O0`은 뮤턴트가 최적화로 제거되는 것을 방지. (참고: `-grecord-command-line`은 `clang a.cpp b.cpp`처럼 한 호출에서 여러 파일을 컴파일하면 깨짐 — CMake의 파일별 컴파일이 이를 회피.)

**mull.yml 설정.** 프로젝트 루트에 `mull.yml` 배치. 관련 키(Mull 설정 레퍼런스 기준): `mutators`/`ignoreMutators`(예: `cxx_add_to_sub`, `cxx_logical`), `includePaths`/`excludePaths`(정규식; **프레임워크 뮤테이션을 피하기 위해 `Catch2`, `extras`, 서드파티 디렉터리 제외**), `compilationDatabasePath`(`CMAKE_EXPORT_COMPILE_COMMANDS` 빌드의 `compile_commands.json`을 가리킴), `gitDiffRef` + `gitProjectRoot`(증분), `parallelization.workers`/`executionWorkers`.

**증분 / 브랜치-diff 뮤테이션(`gitDiffRef`).** 이것이 리팩토링 워크플로의 핵심축. `gitDiffRef: origin/main`과 `gitProjectRoot: <레포 루트>`를 설정(또는 CLI에 `-git-diff-ref=origin/master -git-project-root=...` 전달)하여 Mull이 리팩토링 브랜치의 베이스라인 대비 diff에서 변경된 라인에만 뮤턴트를 생성하게 함. 이로써 수 시간짜리 전체 프로젝트 뮤테이션 실행이 빠른 PR 게이팅 검사로 전환됨. **주의:** 문서는 증분 뮤테이션 테스팅을 **실험적**으로 표기하며, Mull은 현재 트리 위에서 동작(ref를 `git checkout` 하지 않음)하므로 임의의 옛 커밋이 아니라 브랜치/`origin/main` ref를 사용할 것.

**그래픽스/C++용 성능 스코핑.** 전체 스위트 뮤테이션은 비싸다(Mull 자체 OpenSSL 튜토리얼은 한 모듈에 약 150초가 걸리고 전체 LLVM 실행은 약 9시간이 걸릴 수 있다고 명시). 렌더러의 경우: (a) PR 게이트에 `gitDiffRef` 사용; (b) `excludePaths`로 셰이더-에셋과 서드파티 코드 건너뛰기; (c) 커버리지(`-fprofile-instr-generate -fcoverage-mapping`)로 컴파일하여 Mull이 미커버 코드의 뮤턴트를 건너뛰게 함; (d) 로컬 TDD 루프에는 파일별 스코핑; (e) GPU 의존 골든이미지 테스트 자체는 **뮤테이션하지 말 것**(느리고 비결정적) — Mull을 결정론적 CPU측 로직(수학, 씬그래프, 컬링, 상태 관리)에 집중.

**Dextool mutate(소스 수준 대안).** Dextool의 `mutate` 플러그인은 C/C++용 Clang-AST/소스 수준 뮤테이션 테스터 — Mull의 IR 접근과 근본적으로 다른 설계. `compile_commands.json`이 필요하고, 사용자 제공 `test_cmd`/`build_cmd`를 실행하며, 테스트 명령이 0 아닌 값으로 종료하면 뮤턴트를 사살로 표시. **Catch2와 잘 페어링됨** — 범용 `test_cmd`/`test_cmd_dir` 메커니즘(테스트 실행 파일을 위해 디렉터리를 스캔)을 통해서. 다만 *내장* 테스트케이스 수준 분석기는 Catch2가 아니라 GoogleTest와 CTest용 — 따라서 Catch2에서는 뮤턴트 사살/생존은 얻지만, 커스텀 `--test-case-analyze-cmd`를 작성하지 않으면 테스트케이스별 자동 귀속은 못 얻음. Dextool은 유지보수됨(LLVM 19, 20에 대해 최근 릴리스 검증), 변경 기반 뮤테이션 테스팅, SQLite 결과 영속성, 풍부한 HTML 리포트(중복/무용 테스트케이스 탐지 포함) 지원. **권고:** 속도와 IR 수준 단순성을 위해 Mull 선호; 더 풍부한 테스트 스위트 품질 리포트가 필요하거나 LLVM-플러그인 툴체인이 환경에서 까다로우면 Dextool 고려.

### 3. Catch2 테스트 케이스로서의 골든이미지 회귀

이 계층은 **전적으로 커스텀**이지만 성숙한 부품들로 조립됨.

**CI에서 헤드리스 결정론적 렌더링.** 두 가지 실행 가능 접근:
- **소프트웨어 래스터라이저(결정성을 위해 권장):** Mesa의 **llvmpipe**(Gallium LLVM 기반 소프트웨어 OpenGL) 또는 **OSMesa**. `LIBGL_ALWAYS_SOFTWARE=1` 및/또는 surfaceless EGL 플랫폼(`EGL_PLATFORM=surfaceless`)으로 강제. 이로써 GPU/드라이버 비결정성 제거 — 동일 코드가 CI 러너 전반에서 비트 안정(또는 거의 안정) 출력을 생성. 트레이드오프: llvmpipe는 느리고 하드웨어 대비 사소한 래스터화 quirk이 있을 수 있음.
- **EGL surfaceless 컨텍스트:** `eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, ...)`(또는 GPU용 `/dev/dri/renderD128` 통한 GBM)로 오프스크린 컨텍스트 생성, `eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)`, FBO로 렌더링, 그 후 `glReadPixels`. X 서버 불필요 — 컨테이너/CI에 이상적. (EGL surfaceless가 없으면 Xvfb + GLX가 구형 폴백.)

컨텍스트는 커스텀 `main()`(`...WithMain`이 아니라 `Catch2::Catch2` 링크) 또는 Catch2 글로벌 픽스처/이벤트 리스너에서 **한 번** 초기화. GL 컨텍스트 생성은 비싸고 스레드 안전하지 않기 때문.

**프레임버퍼 읽기 + PNG I/O.** FBO로 렌더링 후 `glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,buf)`, 수직 뒤집기(GL 원점은 좌하단, PNG는 좌상단), **stb_image_write**(`stbi_write_png`)로 쓰기; 골든은 **stb_image**(`stbi_load`)로 로드. stb는 단일 헤더, 퍼블릭도메인/MIT, 무의존성 — 직접 임베드.

**이미지 비교 라이브러리(인프로세스 임베드 vs CLI):**

| 라이브러리 | 언어/통합 | 알고리즘 | 권고 |
|---|---|---|---|
| **pixelmatch-cpp17** (jwmcglynn) | C++17, 약 300줄, 무의존, RGBA 버퍼 | YIQ 지각적 픽셀별 + 안티앨리어스 탐지; 불일치 픽셀 수 반환 | **인프로세스 임베드** — Catch2 매처의 최선 기본 |
| **mapbox/pixelmatch-cpp** | C++11 헤더 전용, CMake(`mapbox::pixelmatch`) | 픽셀별 임계값, 불일치 수 반환 | 임베드; cpp17 포트보다 단순/구형 |
| **perceptualdiff (pdiff)** | C++/CMake, 라이브러리 API(`pdiff::yee_compare`) + CLI | HVS/Yee 지각 메트릭, FOV/휘도 인식 | 지각적 허용 오차에 임베드; 또는 아티팩트용 CLI |
| **SSIM(커스텀 또는 dssim)** | C++로 계산; dssim은 Rust(C API) | 구조적 유사도 | 픽셀별이 너무 깨지기 쉬울 때 사용 |
| **ODiff** | CLI(OCaml) | 빠른 지각적 diff | diff-이미지 출력을 원할 때만 CLI로 호출 |

**권고: 핵심 단언에는 pixelmatch-cpp17을 인프로세스 임베드**(빠름, 서브프로세스 없음, 예산에 직접 매핑되는 정확한 불일치 픽셀 수 반환), 선택적으로 지각적 재확인이나 diff 아티팩트 출력을 위해 perceptualdiff로 셸 아웃.

**"이미지가 허용 오차 내에서 골든과 일치"하는 커스텀 Catch2 매처.** `Catch::Matchers::MatcherBase<Image>`에서 파생:

```cpp
#include <catch2/matchers/catch_matchers.hpp>

class MatchesGolden : public Catch::Matchers::MatcherBase<Image> {
  Image golden_;
  double maxPixelFraction_;   // 허용 픽셀 수 예산
  float perPixelThreshold_;   // pixelmatch 민감도 0..1
public:
  MatchesGolden(Image g, double frac, float t)
    : golden_(std::move(g)), maxPixelFraction_(frac), perPixelThreshold_(t) {}

  bool match(Image const& actual) const override {
    if (actual.w != golden_.w || actual.h != golden_.h) return false;
    std::vector<uint8_t> diff(actual.rgba.size());
    pixelmatch::Options o; o.threshold = perPixelThreshold_;
    int bad = pixelmatch::pixelmatch(actual.rgba, golden_.rgba, diff,
                                     actual.w, actual.h, actual.w*4, o);
    if (double(bad) / (actual.w*actual.h) > maxPixelFraction_) {
      write_png("artifacts/diff_" + name_ + ".png", diff, actual.w, actual.h);
      write_png("artifacts/actual_" + name_ + ".png", actual.rgba, ...);
      return false;
    }
    return true;
  }
  std::string describe() const override {
    return "골든과 " + std::to_string(maxPixelFraction_*100) + "% 픽셀 예산 내 일치";
  }
};
// 사용: REQUIRE_THAT(renderFrame(scene), MatchesGolden(loadPng("golden/triangle.png"), 0.001, 0.1f));
```

실패 시 매처는 diff + actual PNG를 아티팩트 디렉터리에 쓰고, CI가 이를 사람 검토용으로 업로드(GitHub Actions `actions/upload-artifact`).

**비결정성 처리.** 계층화된 방어: (1) 비트 안정성을 위해 CI에 소프트웨어 래스터라이저; (2) 픽셀별 임계값 + **허용 픽셀 수 예산**(FP/래스터화 지터로 인한 N개의 떠도는 픽셀 허용); (3) **영역 마스킹**(HUD/텍스트/타임스탬프 영역 제외); (4) **모든 입력 고정** — 고정 카메라, 뷰포트, 뷰포트 크기, RNG 시드, 애니메이션 시각 `t=const`, vsync/디더링 비활성화. 사소한 지터가 불가피하면 정확 일치보다 지각 메트릭(pdiff/SSIM) 선호.

**브랜치-diff 골든 워크플로.** Mull `gitDiffRef` 게이트를 그대로 반영:
1. **베이스라인 브랜치**에서 골든 PNG를 렌더링하고 커밋(또는 CI에서 생성·캐시).
2. **리팩토링 브랜치**에서 동일하게 고정된 입력으로 *actual* 렌더링.
3. actual을 커밋된 골든과 허용 오차로 diff; 어떤 테스트든 예산 초과 시 **PR 실패**; diff 아티팩트 업로드.
4. 정당하게 변경된 골든은 재생성되어 검토 가능한 diff로 재커밋.

**골든 저장/버전 관리.** PNG는 바이너리이고 Git 히스토리를 부풀림. **Git LFS 사용**(`*.png filter=lfs`), 또는 해시로 키된 콘텐츠 주소 외부 저장소. 인레포(비LFS)는 작고 거의 변하지 않는 세트에만 허용. 골든에 그것을 생성한 렌더러/Mesa 버전을 태깅 — Mesa 업그레이드가 출력을 바꿀 수 있으므로.

### 4. Catch2와 호환되는 적대적 / 퍼징 테스트 생성

**libFuzzer + AFL++ 공존.** 퍼즈 하네스는 자체 `int LLVMFuzzerTestOneInput(const uint8_t*, size_t)` 엔트리 포인트를 가진 *별도 실행 파일*로, `-fsanitize=fuzzer,address`로 컴파일. 전용 CMake 타깃에 존재하며 Catch2 단위 타깃과 완전히 독립 — 퍼저가 자체 `main`을 제공하므로 충돌 없음. **상태 참고:** libFuzzer의 원저자들은 활발한 기능 작업을 중단(유지보수만)하고 **Centipede**로 이동; 본격적 멀티코어 캠페인에는 **AFL++** 또는 **LibAFL** 권장하나, libFuzzer는 가장 마찰 적은 인프로세스 옵션으로 남아 동일 하네스 시그니처 공유.

```cmake
option(BUILD_FUZZERS "Build libFuzzer targets" OFF)
if(BUILD_FUZZERS)
  add_executable(fuzz_parser fuzz/parse_scene.cpp)
  target_link_libraries(fuzz_parser PRIVATE renderer_lib)
  target_compile_options(fuzz_parser PRIVATE -fsanitize=fuzzer,address)
  target_link_options(fuzz_parser PRIVATE -fsanitize=fuzzer,address)
endif()
```

**크래시를 Catch2 회귀로 전환.** libFuzzer가 크래시를 발견하면 재현물 파일(`crash-<sha1>`)을 씀. `./fuzz_parser crash-<sha1>`로 재실행하여 확인. 그 후 그 바이트 시퀀스를 픽스처로 커밋하고, 동일 코드 경로로 그것을 흘려보내 크래시 없음/올바른 처리를 단언하는 Catch2 케이스 작성:

```cpp
TEST_CASE("regression: 퍼저 crash-a9993e의 잘못된 씬", "[regression][fuzz]") {
  auto data = readFile("fuzz/corpus/crash-a9993e36");
  REQUIRE_NOTHROW(parseScene(data.data(), data.size()));
}
```

이로써 루프가 닫힘: 퍼저가 발견 → 영구 결정론적 Catch2 가드가 됨.

**Catch2 네이티브 데이터 제너레이터.** `GENERATE()`는 값-파라미터화 케이스(`auto i = GENERATE(0, 1, 7, INT_MAX);`), `GENERATE(range(...))`, `GENERATE(take(100, random(...)))`, `GENERATE_REF` 제공. `SECTION`과 결합하면 외부 의존성 없이 경량 조합 엣지케이스 커버리지 확보. 참고: `GENERATE`는 인자를 decay함(v3 동작 변경).

**RapidCheck 프로퍼티 기반 테스팅.** RapidCheck(emil-e/rapidcheck)는 QuickCheck 스타일 PBT 라이브러리로 **1급 Catch2 통합** 보유: `rapidcheck/catch.h`를 인클루드하고 `TEST_CASE` 안이나 그 자체로 `rc::prop("description", [](T arg){ ... });` 사용; 입력을 자동 생성하고 반례를 축소(shrink). 통합 헤더는 Catch2 v3를 명시적으로 지원. 스위트에 네이티브로 꽂히는 가장 강력한 적대적 입력 생성기. (대안 pcc / "Property Checking for Catch2"가 존재하나 훨씬 덜 성숙.)

**LLM 기반 엣지케이스 생성.** 코드 LLM(예: Qwen2.5-Coder)으로 엣지케이스용 Catch2 `TEST_CASE`를 초안하고, **그 강도를 커버리지가 아니라 Mull의 뮤테이션 점수로 검증** — 커버리지는 라인이 실행됐음을 증명하나, 사살된 뮤턴트는 단언이 실제로 동작을 검사함을 증명. 기계 생성 테스트의 가장 중요한 품질 게이트. 2026년 기준 C++에 대해 이를 자율적·신뢰성 있게 하는 모델은 없음; §5 참조.

### 5. Claude Code 스킬과 Hugging Face 모델 (2026년 중반 현행성)

**Claude Code 스킬/플러그인(성숙, 인기, 그러나 C++/Catch2 인지 없음):**
- **obra/superpowers** — 단연 가장 많은 스타를 받은 Claude Code 스킬 프레임워크: **2026년 5월 3일 기준 177,000 스타 돌파(포크 15,700, 워처 731), MIT 라이선스, 2026년 1월 15일 Anthropic 공식 Claude Code 플러그인 마켓플레이스 채택**(최신 안정 릴리스 v5.0.7, 2026년 3월 31일). 자동 트리거되는 조합형 스킬 번들: 브레인스토밍, **test-driven-development**(red-green-refactor를 하드 게이트로 강제 — 실패 테스트 전에 작성된 코드는 삭제), systematic-debugging, 코드 리뷰가 포함된 서브에이전트 주도 개발, git-worktrees, writing-skills(새 스킬 작성용 메타 스킬). 공식 마켓플레이스로 설치: `/plugin install superpowers@claude-plugins-official`. 플랫폼 불문(Claude Code, Codex, Cursor, Gemini CLI 등)이나 Claude Code에서 가장 깊음.
- **nizos/tdd-guard** — 비TDD 파일 쓰기를 차단하는 훅을 통한 자동 TDD *강제*; 빠른 on/off 명령(`tdd-guard off`); 활발히 유지보수(최근 릴리스가 Windows/VS Code 이슈 수정, Minitest 리포터 추가). 강제 계층은 언어 불문이나 리포터가 JS/Python/Ruby 중심 — **C++/Catch2 리포터 없음**, 따라서 편집을 게이트할 수는 있으나 커스텀 작업 없이 Catch2 결과를 네이티브로 파싱하지 못함.
- **citypaul/.dotfiles** — 명시적 **RED-GREEN-MUTATE-뮤턴트 죽이기-REFACTOR** 워크플로와 전용 **mutation-testing** 스킬을 갖춘 포괄적 개인 스킬 세트 — 그러나 **TypeScript/Stryker 지향**(functional, typescript-strict, react-testing). `skills.sh`(`npx skills ...`) 또는 레포의 `install-claude.sh`로 설치.
- **Anthropic 공식 플러그인 마켓플레이스** — Superpowers와 관련 플러그인 호스팅; 표준 설치 채널.

**솔직한 공백:** 이들 모두 *방법론*(TDD 규율, 검증으로서의 뮤테이션)을 인코딩하지만 **C++, CMake/Ninja, Catch2, Mull, OpenGL, 골든이미지를 기본으로 이해하는 것은 하나도 없다.** 이들의 뮤테이션 스킬은 Stryker(JS/TS)를 가정. 커스텀 스킬을 작성해야 함 — `.claude/skills/golden-image-regression/SKILL.md`, `catch2-tdd` 스킬, `mull-mutation` 스킬 — 셸 스크립트를 감싸고 프로젝트별 명령을 Claude에게 가르치는 것. 좋은 소식: Superpowers의 `writing-skills` 스킬과 SKILL.md 형식이 정확히 이 확장을 위해 설계됨.

**Hugging Face 모델(목적 제작 C++/Catch2 테스트 모델 없음):**
- **Qwen2.5-Coder** — 6개 크기(0.5B, 1.5B, 3B, 7B, 14B, 32B), Base + Instruct, 5.5T 코드 토큰, 128K 컨텍스트, 92개 언어. **3B를 제외한 전 크기 Apache-2.0**(3B는 Qwen-Research 라이선스). (32B-Instruct는 GPT-4o급 코딩과 견줌.) 레포 패턴: `Qwen/Qwen2.5-Coder-<size>-Instruct`. 기술 보고서: arXiv 2409.12186. (참고: "3B와 72B" 비Apache 예외는 *일반* Qwen2.5 LLM 라인에 적용; Qwen2.5-Coder는 72B 변형이 없으므로 비Apache 예외는 3B뿐.)
- **Qwen3-Coder** — 가장 에이전트적인 Qwen 코드 라인: `Qwen/Qwen3-Coder-480B-A35B-Instruct`(MoE, 총 480B / 활성 35B, 262K→약 1M 컨텍스트)와 `Qwen/Qwen3-Coder-30B-A3B-Instruct`(MoE, 30B/3.3B 활성, "Flash"), 그리고 신규 `Qwen3-Coder-Next`(Qwen3-Next-80B-A3B-Base 기반). **모두 Apache-2.0**(HF 레포의 LICENSE 파일 확인). arXiv 2505.09388.
- **StarCoder2**(BigCode/ServiceNow/NVIDIA) — 3개 크기: `bigcode/starcoder2-3b`, `bigcode/starcoder2-7b`, `bigcode/starcoder2-15b`; 16K 컨텍스트, FIM 학습, **The Stack v2**(`bigcode/the-stack-v2-train`). **BigCode OpenRAIL-M v1** 라이선스(HF 태그 `bigcode-openrail-m`) — 책임 있는 AI 라이선스로 **사용 제한 있음, Apache-2.0처럼 관대하지 않음**. 논문: "StarCoder 2 and The Stack v2: The Next Generation"(2024년 2월). 다운로드 수는 시점별; 라이브 확인.
- **결함/취약점 분류기(C/C++용으로 실제 존재):** `mrm8488/codebert-base-finetuned-detect-insecure-code`(CodeXGLUE/Devign의 CodeBERT, C 함수 — 고전적 베이스라인), `rootxhacker/CodeAstra-7B`(Mistral-7B 파인튜닝, C/C++ 포함 다언어, 자체 보고 약 83% 정확도 — 벤더 주장, 확인 필요), 그리고 학술 VulBERTa / LineVul / AIBugHunter. 데이터셋: `google/code_x_glue_cc_defect_detection`, `mcanoglu/defect-detection`. *자문* 신호로 유용, 게이트로는 아님.

**C++ LLM 테스트 생성의 현재 상태(2026), 솔직하게.** 작지만 실재하는 틈새가 존재. **CPP-UT-Bench**(arXiv 2412.02735, 2024년 12월 3일 제출; Nutanix의 Bhargava, Ghosh, Dutta)는 **"9개의 다양한 도메인에 걸친 14개의 서로 다른 오픈소스 C++ 코드베이스에서 추출한 2,653개의 {코드, 단위 테스트} 쌍"** 벤치마크로, GoogleTest 스타일 정답을 사용; 그들의 파인튜닝 모델이 10개 중 9개 실험에서 베이스 모델을 능가(HF 데이터셋 `Nutanix/CPP-UNITTEST-BENCH`). **CITYWALK**(arXiv 2501.16155, ACM TOSEM 게재)은 GPT-4o 기반 프레임워크로 프로젝트 의존성 + C++ 특화 지식(포인터, 템플릿, 가상 함수, 모킹)을 추가하며 GoogleTest를 기본값으로. **그러나: 널리 쓰이는 어떤 HF 모델도 C++ — 하물며 Catch2 — 테스트 생성용으로 브랜딩/특화되지 않았다.** 기존 작업은 GoogleTest를 대상; Catch2 측면은 사실상 미해결. 유일한 목적 제작 C++ 테스트 아티팩트는 Nutanix의 작은 실험적 파인튜닝(예: TinyLlama-1.1B)으로, 연구 수준이지 프로덕션 아님. Meta의 TestGen-LLM(arXiv 2402.09171)은 Kotlin/Java. **결론: Catch2용으로 프롬프팅한 범용 코드 LLM을 쓰고, Mull로 출력을 게이트하며, 사람을 루프에 유지하라.**

### 6. 조립 / 파일 매니페스트

생성할 구체적 컴포넌트, 솔직한 "존재 vs 구축" 라벨링과 함께.

**CMake(대부분 기성 배선):**
- `CMakeLists.txt` — Catch2 FetchContent + `catch_discover_tests()`(§1). **기성 패턴.**
- `tests/CMakeLists.txt` — `Catch2::Catch2WithMain` + `renderer_lib`를 링크하는 `unit_tests` 타깃. **기성.**
- 골든이미지 타깃 — `Catch2::Catch2`(EGL 초기화용 커스텀 `main`), stb, pixelmatch-cpp17를 링크하는 `golden_tests`. **커스텀.**
- Mull 빌드 구성 — `-fpass-plugin=...`을 주입하는 별도 `build-mull/` 디렉터리 또는 `option(ENABLE_MULL)`, Clang 전용(§2). **기성 플래그, 커스텀 글루.**
- 퍼즈 타깃 — `-fsanitize=fuzzer,address`를 가진 `option(BUILD_FUZZERS)`(§4). **기성.**

**뮤테이션 설정:**
- `mull.yml` — `mutators`, `excludePaths`(Catch2/extras/third_party), `compilationDatabasePath`, `gitDiffRef: origin/main`, `gitProjectRoot`, `parallelization`. **기성 형식, 커스텀 내용.**

**셸 스크립트(모두 커스텀):**
- `scripts/golden_branch_diff.sh` — 베이스라인 체크아웃 → 골든 렌더링·저장 → 리팩토링 체크아웃 → actual 렌더링 → 허용 오차로 diff → 게이트. **커스텀.**
- `scripts/mull_gitdiff.sh` — `build-mull` 구성, `unit_tests` 빌드, `gitDiffRef=origin/main`으로 `mull-runner-19` 실행, 뮤테이션 점수 파싱, 임계값 게이트. **커스텀.**
- `scripts/fuzz_to_catch2.sh` — 크래시 재현물을 받아 `[regression][fuzz]` `TEST_CASE` 스캐폴드. **커스텀.**

**Claude Code 스킬(모두 커스텀 — 존재하지 않음):**
- `.claude/skills/golden-image-regression/SKILL.md` — 범위: 헤드리스 렌더, 골든과 비교, diff 해석, 의도적 변경 시 골든 재생성, 통과시키려 허용 오차를 절대 약화시키지 않기. **커스텀.**
- `.claude/skills/catch2-tdd/SKILL.md` — Catch2 + CMake/Ninja + CTest 명령에 특화된 red-green-refactor. **커스텀.**
- `.claude/skills/mull-mutation/SKILL.md` — Mull 실행, 생존자 읽기, 사살 단언 작성, 리팩토링 게이트. **커스텀.** (citypaul의 mutation-testing 스킬의 *구조*를 차용할 수 있으나 모든 명령을 Mull/Catch2로 변경해야 함.)

**CI(GitHub Actions, 표준 액션의 커스텀 배선):**
- `.github/workflows/ci.yml` — 잡: (1) 빌드 + `ctest -j` 단위 테스트, JUnit 업로드; (2) Mesa/llvmpipe 컨테이너의 골든이미지 잡, diff 아티팩트 업로드; (3) PR의 Mull `gitDiffRef` 뮤테이션 게이트; (4) 선택적 야간 퍼즈 잡. **커스텀 배선, 표준 액션.**

**순위별 권고(먼저 구축할 것):**
1. **Catch2 + CTest 코어**(최고 가치, 최저 노력, 완전 기성).
2. **골든이미지 하네스**(커스텀이나 사용자의 핵심 요구; 소프트웨어 래스터라이저 결정성 우선).
3. **Mull `gitDiffRef` 게이트**(적대적 검증 키스톤; 기성 엔진).
4. 위 스크립트를 감싸는 **커스텀 Claude Code 스킬.**
5. **RapidCheck + libFuzzer** 적대적 입력.
6. **LLM 보조 테스트 초안**(Qwen-Coder), Mull로 게이트 — 마지막, 사람 감독 하에.

## 권고

**1단계 — 기반(1주차).** Catch2 v3를 FetchContent로 배선(v3.15.0 같은 현행 릴리스에 고정), 프로덕션 코드를 `renderer_lib`에, `catch_discover_tests()`로 테스트 등록, JUnit 아티팩트와 함께 CI에서 `ctest -j` green 달성. *진행 임계점:* 기존 동작이 통과 테스트로 특성화됨.

**2단계 — 골든이미지 회귀(2–3주차).** CI 컨테이너에서 llvmpipe/EGL-surfaceless로 헤드리스 렌더링 구축, stb + pixelmatch-cpp17 임베드, 픽셀별 임계값 + 픽셀 수 예산을 가진 `MatchesGolden` 매처 작성, 골든을 Git LFS에 저장, `golden_branch_diff.sh` 구현. *임계점:* 의도적 1픽셀 셰이더 변경이 게이트를 실패시키고 diff를 업로드; no-op 리팩토링은 통과.

**3단계 — 뮤테이션 게이트(4주차).** Clang+Mull 빌드 구성과 `mull.yml` 추가, 전체 스위트를 한 번 실행하여 뮤테이션 점수 베이스라인, 그 후 PR을 `gitDiffRef: origin/main`으로 전환. *조정할 임계점:* 변경 라인에 뮤테이션 점수 하한 설정(약 70–80%에서 시작, 시간이 지나며 상향); 실행이 CI 예산을 초과하면 커버리지 계측 추가하고 `excludePaths` 강화.

**4단계 — 적대적 입력(5주차).** 순수 로직용 RapidCheck(`rapidcheck/catch.h`) 프로퍼티와 각 파서/역직렬화기용 libFuzzer 타깃 하나 추가; `fuzz_to_catch2.sh` 배선. *임계점:* 퍼저가 발견한 모든 크래시는 수정이 머지되기 전에 커밋된 `[regression]` 케이스가 됨.

**5단계 — 오케스트레이션 + LLM 보조(6주차+).** TDD 방법론 중추로 obra/superpowers 설치, 그 후 세 커스텀 SKILL.md 파일 작성. Mull 게이팅이 신뢰된 후에만 Qwen2.5/3-Coder를 도입하여 엣지케이스 테스트 초안 — **이전에 생존한 뮤턴트를 사살하는 경우에만 생성 테스트를 수용하라.** *LLM 사용 확장 임계점:* 생성 테스트가 거짓 확신 없이 뮤테이션 점수를 측정 가능하게 높임; 그렇지 않으면 자문 수준으로 유지.

**이 권고를 바꿀 요인:** GPU 하드웨어 특화 렌더링을 (소프트웨어 래스터화 로직뿐 아니라) 테스트해야 하면, 골든이미지가 본질적으로 불안정해지므로 더 느슨한 예산의 지각 메트릭(pdiff/SSIM)으로 전환하고 수동 검토를 수용하라. LLVM/Clang 플러그인 툴체인을 빌드 환경에서 쓸 수 없거나 까다로우면, 뮤테이션 엔진을 Mull에서 Dextool(소스 수준, IR 플러그인 불필요)로 전환. `gitDiffRef`로도 뮤테이션 런타임이 CI를 지배하면, 핵심 로직 파일의 큐레이션 세트로 뮤테이션 제한.

## 유의사항 (Caveats)
- **버전/인기 수치는 시점별(2026년 6월)이며 라이브로 검증해야 함:** Catch2 최신 태그(v3.15.0, 2026년 5월), Mull 버전(0.27.1 문서 라인 / 0.27.0 태그, 2025년 10월)과 LLVM 버전 접미사 바이너리, GitHub 스타 수(Superpowers 2026년 5월 기준 177K+), 모든 Hugging Face 다운로드 수(StarCoder2 수치는 2024년 스냅샷으로 오래됨).
- **Mull의 증분(`gitDiffRef`) 모드는 공식적으로 실험적**이고 ref를 체크아웃하지 않고 현재 트리에서 동작 — 임의의 과거 커밋이 아니라 라이브 브랜치/`origin/main` 비교를 사용할 것.
- **골든이미지 계층과 모든 Claude Code 스킬은 기성으로 존재하지 않으며** 커스텀 엔지니어링의 대부분을 차지. 턴키 C++/OpenGL/Catch2 골든이미지 Claude 스킬을 주장하는 자가 있으면 회의적으로 볼 것.
- **C++/Catch2 테스트 또는 뮤턴트 생성을 목적으로 만들어진 Hugging Face 모델은 없다.** C++ 테스트 생성 연구(CPP-UT-Bench, CITYWALK)는 GoogleTest를 대상; 모든 LLM 생성 테스트를 뮤테이션 점수로 검증할 초안으로 취급.
- **소프트웨어 래스터라이저 결정성은 강하나 절대적이지 않음** — Mesa 버전 업그레이드가 출력을 바꿀 수 있음; 골든에 생성 툴체인 버전을 태깅하고 주기적 재생성을 예상.
- **라이선스 실사:** Catch2(BSL-1.0)와 Qwen-Coder(Apache-2.0, Qwen2.5-Coder-3B 제외)는 관대; **StarCoder2는 사용 제한이 있는 BigCode OpenRAIL-M** — 그 출력에서 파생된 것을 상용 제품에 출하하기 전 수용 가능성 확인.
