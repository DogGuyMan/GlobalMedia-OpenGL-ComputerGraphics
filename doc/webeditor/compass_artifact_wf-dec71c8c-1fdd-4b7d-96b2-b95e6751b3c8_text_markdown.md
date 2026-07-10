# C++ REST/HTTP 서버·클라이언트 vs Python 표준 dev 서버 — 학습 프로젝트용 트레이드오프 리포트

## TL;DR
- **결론(권장):** "전부 C++로 제작" 목표라면, dev 서버는 **header-only 단일 파일 라이브러리 cpp-httplib (yhirose/cpp-httplib, v0.47.0 — 2026-06-10 릴리스, MIT)** 로 교체하는 것이 sweet spot이다. 프로젝트의 header-only vendoring 스타일과 완벽히 맞고, GET 정적 + PUT JSON 같은 작은 dev 서버에는 충분하며, 의존성/빌드 부담이 사실상 0이다.
- **libcurl은 서버가 아니라 HTTP 클라이언트 라이브러리다.** 서버 교체용이 아니라, (a) native CLI 동반 도구, (b) 통합 테스트, (c) 게임/툴의 outbound 요청 같은 진짜 클라이언트 역할에서만 의미가 있다. C API의 거친 부분을 감싸려면 **cpr (libcpr/cpr, 1.12.x, MIT)** 가 CMake FetchContent로 가장 쉽게 통합된다.
- **무거운 프레임워크(Drogon/Beast)는 학습 자체가 목적이 아닌 한 피하라.** localhost 전용 dev 서버는 plain HTTP로 충분하므로 TLS 의존성 부담(OpenSSL/SChannel/Secure Transport)을 대부분 제거할 수 있어, C++ 전환 비용이 크게 낮아진다.

## Key Findings (확정 사실 vs 권장 분리)

### 확정 사실 (출처 있음)
- **libcurl은 transfer/client 라이브러리이며 서버 기능이 없다.** 최신 stable 버전은 **8.20.0 (2026-04-29 릴리스)**, 라이선스는 MIT-like. curl.se 공식 표현으로 "the Internet transfer engine for countless software applications in **over twenty billion installations**"이며, 제작자 Daniel Stenberg는 2026-05-26 블로그(daniel.haxx.se "The pressure")에서 "Thirty billion installations world-wide"로 상향 언급했다. (curl.se, github.com/curl/curl)
- **cpp-httplib**: header-only 단일 파일, 버전 **0.47.0 (2026-06-10 릴리스, Copyright (c) 2026)**, **MIT 라이선스**, blocking I/O, HTTP/1.1만 지원, 32-bit 미지원. README는 "Windows 8 or lower, Visual Studio 2015 or lower, and Cygwin and MSYS2 including MinGW are neither supported nor tested"라고 명시(=Win10+·최신 VS만 공식 지원). (github.com/yhirose/cpp-httplib)
- **Crow**: 최신 **v1.3.2 (2026-03-29)**, **BSD-3-Clause**, 필수 의존성은 Asio(standalone 또는 Boost.Asio) 하나. (github.com/CrowCpp/Crow/releases)
- **Drogon**: 최신 **v1.9.13 (2026)**, **MIT**, 필수 의존성 jsoncpp(≥1.7)·libuuid·zlib·Trantor(서브모듈), C++17 필요. (github.com/drogonframework/drogon)
- **Boost.Beast**: Boost의 일부, **Boost Software License 1.0**, header-only이지만 Boost.Asio에 의존하는 저수준 라이브러리. (github.com/boostorg/beast)
- **cpr**: 최신 **1.12.x (MIT)** — README에 "The main reason for an earlier 1.12.0 release of cpr is supporting curl >= 8.13" 명시(Windows NuGet 최신은 libcpr 1.14.2). C++17 필요, FetchContent로 libcurl을 자동으로 가져와 빌드("There's no need to handle libcurl yourself. All dependencies are taken care of for you."). (github.com/libcpr/cpr, nuget.org/packages/libcpr)
- **Python http.server**: Python 3.14.6 공식 문서 verbatim 경고 — "Warning: http.server is not recommended for production. It only implements basic security checks. Availability: not WASI." 즉 production 비권장이면서 WASM에서도 사용 불가다. (docs.python.org/3/library/http.server.html)

### 권장/의견
- 학습 동기 + header-only 선호 + no vcpkg + localhost dev 서버라는 제약 조합에서는 cpp-httplib가 명백한 1순위다.
- libcurl/cpr은 서버가 아니라 별도의 native 클라이언트 도구·테스트에서 도입할 때 학습 가치가 크다.

## Details

### 1. libcurl — HTTP 클라이언트 (서버 아님)

libcurl은 URL 문법으로 데이터를 전송하는 **클라이언트** 라이브러리다. HTTP, HTTPS, FTP 등 다수 프로토콜을 지원하지만 **들어오는 연결을 listen하는 서버 기능은 없다.** 따라서 dev 서버 교체 후보가 될 수 없다. (curl 프로젝트는 Daniel Stenberg가 1998년 창시 — 본인 표현 "I founded the curl project back in 1998" —, 2026년 약 30주년에 이르는 극도로 성숙한 프로젝트이며, Stenberg는 전체 약 24,000 커밋의 절반 이상을 작성했다.)

- **easy interface vs multi interface:** easy는 동기(블로킹) 단일 전송용(`curl_easy_init` → `curl_easy_setopt` → `curl_easy_perform`)이고, multi는 여러 전송을 한 스레드에서 비동기로 다루는 인터페이스다. C API이므로 C++에서는 RAII 래퍼를 직접 두거나 cpr 같은 래퍼를 쓴다.
- **버전/라이선스/성숙도:** 최신 stable 8.20.0 (2026-04-29). MIT-like 라이선스. 20+ billion ~ 30 billion installations로 추정되는 사실상 표준 클라이언트.
- **빌드/링크:** header-only가 **아니다.** C 라이브러리를 빌드/링크해야 한다 — 프로젝트의 header-only vendoring 선호와 대비되는 결정적 지점이다. 시스템 패키지(brew), prebuilt, 또는 소스 빌드(curl/curl CMake) 가능. static/dynamic 모두 가능.
- **의존성(TLS 백엔드):** macOS는 Secure Transport(또는 8.17.0+의 SecTrust)·OpenSSL, Windows는 Schannel·OpenSSL 등(curl 공식: "On Windows this is Schannel, on macOS Secure Transport, and OpenSSL (or equivalent) on all other platforms"). zlib, libpsl 등도 옵션. plain HTTP만 쓰면 `--without-ssl`로 TLS 의존성 제거 가능.
- **C++ 래퍼:** 아래 표 참조.

#### libcurl 래퍼 비교

| 래퍼 | 버전/상태 | 라이선스 | header-only? | libcurl 처리 | CMake 통합 | 평가 |
|---|---|---|---|---|---|---|
| **cpr** (libcpr/cpr) | 1.12.x(NuGet 1.14.2), 활발 | MIT | 아니오(빌드 필요) | FetchContent가 자동으로 가져와 빌드(별도 처리 불필요) | `FetchContent_Declare(cpr ...)` → `cpr::cpr` | Python Requests 스타일. 가장 권장 |
| **curlcpp** (JosephP91) | 활발, curl≥7.86 | MIT | 아니오 | 직접 `-lcurl` 링크 필요 | submodule + `add_subdirectory` | easy/multi/share 전부 래핑 |
| **curlpp** (jpbarrette) | 사실상 유지보수 정체 | MIT | 아니오 | 직접 링크 | CMake | 오래된 wrapper, 신규 도입 비권장 |

cpr은 OpenSSL/WinSSL(Schannel)로 HTTPS를 지원하며, 시스템 curl을 쓰려면 `CPR_USE_SYSTEM_CURL=ON`(curl ≥ 7.71.0 필요)으로 설정한다.

### 2. C++ HTTP 서버 라이브러리 비교 (Python dev 서버 대체 후보)

| 라이브러리 | 버전 | 라이선스 | header-only? | 의존성 footprint | macOS+Windows | GET정적+PUT 구현 노력 | 학습 가치 |
|---|---|---|---|---|---|---|---|
| **cpp-httplib** | 0.47.0 (2026-06-10) | MIT | ✅ 단일 파일 | 없음(TLS만 선택적 OpenSSL) | ✅ (VS 최신, Win10+) | 매우 낮음(수십 줄) | 중 (HTTP 기초) |
| **Crow** | v1.3.2 (2026-03-29) | BSD-3-Clause | △ (single-header 생성 가능, but Asio 필요) | Asio(standalone/Boost) | ✅ | 낮음(Flask형 라우팅) | 중상 (라우팅/asio) |
| **Drogon** | v1.9.13 (2026) | MIT | ❌ | jsoncpp·libuuid·zlib·Trantor(+OpenSSL 옵션) | ✅ (빌드 무거움) | 높음(프레임워크 학습 필요) | 상 (async/coroutine) |
| **Boost.Beast** | Boost 1.8x | BSL-1.0 | ✅ (단 Boost 전체 필요) | Boost.Asio | ✅ | 매우 높음(HTTP 로직 직접) | 매우 상 (저수준) |
| **Pistache** | - | Apache-2.0 | ❌ | 주로 Linux 중심 | △ (Windows 약함, 미확인) | 중 | 중 |

- **cpp-httplib**: `httplib.h` 하나만 include하면 끝. `svr.Get(...)`, `svr.Put(...)`, `set_mount_point`로 정적 파일 서빙. blocking 멀티스레드 모델, async 아님. localhost dev 서버에 최적. CVE 이력이 있으나(예: CVE-2025-46728 — 0.20.1 이전 chunked/Content-Length 미지정 시 무제한 메모리 할당, GHSA-px83-72rx-v57c; CVE-2025-53629 — 0.23.0에서 수정; CVE-2026-45352 — 0.43.4 이전 음수 chunk-size '-2'가 strtoul wrap-around로 unbounded allocation·SIGABRT 유발, CVSS 5.3, 2026-05-29 공개, GHSA-h6wq-j5mv-f3q8) 모두 0.x 후반에서 지속 수정 중이며, localhost 전용이면 위험이 작다. (출처: github.com/yhirose/cpp-httplib Security Advisories, tracker.debian.org/pkg/cpp-httplib)
- **Crow**: Flask풍 라우팅, 빌드된 JSON 지원. Asio 필요해 완전 header-only는 아님. dev 서버에는 다소 과하지만 라우팅 학습엔 좋다. (참고: v1.3.0은 2025-10-13, v1.3.1은 2026-02-11, v1.3.2는 2026-03-29 — header injection 보안 수정.)
- **Drogon**: full async 프레임워크. ORM·WebSocket·coroutine 등. dev 서버 용도로는 명백한 overkill이며 jsoncpp 등 외부 의존성·빌드 부담이 크다.
- **Boost.Beast**: HTTP 메시지 vocabulary만 제공하고 서버 루프는 직접 작성. Boost.Asio 숙련자용. 학습 가치는 최고지만 GET정적+PUT 한 개 만드는 데 드는 노력이 가장 크다.

### 3. 실제 트레이드오프: 현행 Python stdlib dev 서버 대비

**Python http.server가 공짜로 주는 것:** 의존성 0, ~30-50줄, 즉시 실행, Python만 있으면 크로스플랫폼. 단점: 공식적으로 "not recommended for production. It only implements basic security checks", 성능 낮음, 별도 Python 런타임 의존, C++ 코드베이스와 언어 불일치.

**C++ 전환의 비용:** 빌드/의존성 부담(특히 TLS는 macOS Secure Transport vs Windows Schannel/OpenSSL 차이로 마찰), 초기 구현 시간.

**C++ 전환의 이득:** "전부 C++" 목표 달성, 코드베이스 일관성, 단일 toolchain(CMake) 빌드, Python 런타임 제거, HTTP 동작 원리 학습.

**핵심:** dev 서버는 **로컬 전용 개발 편의 도구**이지 production이 아니다. 따라서 localhost plain HTTP로 충분하고, TLS 의존성 부담을 대부분 제거할 수 있다. 이는 C++ 전환 비용을 결정적으로 낮춘다 — cpp-httplib를 OpenSSL 없이 vendoring하면 의존성이 사실상 0이 된다.

## Recommendations (단계별)

분류: **must-have / common / optional**

1. **(must-have) dev 서버 = cpp-httplib (header-only, plain HTTP)**. `httplib.h`를 `include/`에 vendoring(이미 nlohmann/json·tweeny·stb와 동일 방식). OpenSSL 미연결 → 의존성 0. GET 정적 + PUT JSON을 수십 줄로 구현. 기존 Python 서버와 1:1 대체. (Windows에서 `localhost` 대신 `127.0.0.1` 사용 권장 — IPv6 DNS 지연 회피.)
2. **(common) native 클라이언트 도구가 필요해지면 cpr 도입**. CMake FetchContent로 `cpr::cpr` 링크. dev 서버에 GET/PUT으로 데이터 push/pull하는 CLI, 또는 서버를 두드리는 통합 테스트에 적합. 이때만 libcurl(+TLS) 빌드 비용을 감수.
3. **(optional) 학습 자체가 목적이면 Boost.Beast 또는 Crow로 한 번 더 구현**. 저수준 HTTP/asio를 배우고 싶으면 Beast, 라우팅 프레임워크 감을 익히려면 Crow.
4. **(피할 것) Drogon은 이 용도에 overkill**. ORM/async 프레임워크 학습이 별도 목표일 때만.

**판단 기준(thresholds):**
- 동시 접속·성능이 dev 환경에서 문제되면 → Crow/Drogon의 async 고려.
- HTTPS가 실제로 필요해지면(예: WASM 클라이언트가 secure context 요구) → cpp-httplib + OpenSSL 또는 cpr 쪽 TLS 백엔드 비용 재평가.
- 의존성 0·즉시성이 최우선이고 학습 동기가 약하면 → Python 유지도 합리적.

## Caveats
- **WebEditor(WASM 클라이언트)는 libcurl/native 서버 라이브러리를 쓸 수 없다.** 브라우저 샌드박스에서는 fetch API만 가능. 이는 이미 확정된 아키텍처 제약이며, 서버 언어 선택과 무관하게 fetch↔HTTP로 통신한다. (참고로 Python http.server 자체도 공식 문서상 "not WASI" — WASM 비가용이다.)
- cpp-httplib의 CVE 이력(위 CVE-2025-46728 / CVE-2025-53629 / CVE-2026-45352 등)은 공개 노출 서버에서 중요하나, localhost 전용 dev 서버에서는 위험이 낮다. 그래도 최신 버전 유지 권장.
- 버전·날짜는 2026년 6월 기준 확인값이며, 빠르게 갱신될 수 있다.
- Pistache의 Windows 지원 수준은 불확실(미확인)로 표기한다.