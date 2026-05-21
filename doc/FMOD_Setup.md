# FMOD Core + Studio API 설치 가이드

FMOD Engine 은 독점 SDK 라 GitHub 서브모듈로 등록할 수 없다. 아래 절차로 수동 설치한다.

두 가지 API 가 있다:
- **Core** — 저수준 사운드/스트리밍 직접 재생 (PCM, mp3, wav 등)
- **Studio** — `.bank` 파일 기반 이벤트/믹서 시스템 (FMOD Studio 저작 도구 산출물)

둘 다 `if(EXISTS)` 가드로 등록되므로, **필요한 쪽만 설치해도 빌드는 통과**한다.

## 1. SDK 다운로드

- https://www.fmod.com 가입 후 로그인
- Download → FMOD Engine → 플랫폼별 패키지 내려받기
  - macOS: FMOD Engine (macOS)
  - Windows: FMOD Engine (Windows)
- 버전은 코드가 참조하는 API 버전(예: 2.03)에 맞춘다.

## 2. 헤더 배치

### Core (필수)

SDK 의 `api/core/inc/` 안의 헤더를 `include/fmod/` 로 복사 (`.cs` C# 바인딩은 제외).

```
include/fmod/fmod.h
include/fmod/fmod.hpp
include/fmod/fmod_common.h
include/fmod/fmod_errors.h
include/fmod/fmod_codec.h
include/fmod/fmod_dsp.h
include/fmod/fmod_dsp_effects.h
include/fmod/fmod_output.h
```

코드에서는 `#include <fmod/fmod.h>` 로 포함.

### Studio (`.bank` 로딩이 필요할 때만)

SDK 의 `api/studio/inc/` 의 C/C++ 헤더 3개를 같은 `include/fmod/` 에 추가.

```
include/fmod/fmod_studio.h
include/fmod/fmod_studio.hpp
include/fmod/fmod_studio_common.h
```

코드에서는 `#include <fmod/fmod_studio.hpp>` (또는 `.h`) 로 포함.

## 3. 정적 vs 동적 라이브러리

Core / Studio 둘 다 **동적 라이브러리만** 배포된다 (정적 링크 옵션 없음).
- 일반 빌드: `fmod` / `fmodstudio` (최적화)
- 로깅 빌드: `fmodL` / `fmodstudioL` (FMOD_DEBUG 로그 — 개발 중 권장)

CMake 등록은 일반 = Release, 로깅 = Debug 로 자동 매핑한다 (`IMPORTED_LOCATION_DEBUG`).

## 4. 라이브러리 배치

### macOS — SDK 의 `api/core/lib/`, `api/studio/lib/`
```
lib/macos/libfmod.dylib            (Core 일반)
lib/macos/libfmodL.dylib           (Core 로깅)
lib/macos/libfmodstudio.dylib      (Studio 일반)
lib/macos/libfmodstudioL.dylib     (Studio 로깅)
```

### Windows — SDK 의 `api/core/lib/x64/`, `api/studio/lib/x64/`
```
lib/windows/fmod_vc.lib          fmod.dll              (Core 링크 + 런타임)
lib/windows/fmodL_vc.lib         fmodL.dll
lib/windows/fmodstudio_vc.lib    fmodstudio.dll        (Studio 링크 + 런타임)
lib/windows/fmodstudioL_vc.lib   fmodstudioL.dll
```

`arm64` / `x86` 폴더는 미사용 (프로젝트는 x64 만 빌드).

## 5. Dependency.cmake 등록 — 완료 상태

`cmake/Dependency.cmake` 끝부분에 Core / Studio 가 각각 독립 블록으로 등록되어 있다.
헤더 존재 여부로 가드 — 미설치 환경도 configure 통과.

```cmake
# --- Core ---
if(EXISTS "${CMAKE_SOURCE_DIR}/include/fmod/fmod.h")
    add_library(fmod SHARED IMPORTED)
    # ... IMPORTED_LOCATION{,_DEBUG} + (Win) IMPORTED_IMPLIB{,_DEBUG} 설정
    target_link_libraries(game_deps INTERFACE fmod)
endif()

# --- Studio ---
if(EXISTS "${CMAKE_SOURCE_DIR}/include/fmod/fmod_studio.h")
    add_library(fmodstudio SHARED IMPORTED)
    # ... fmodstudio 도 동일 패턴
    if(TARGET fmod)
        target_link_libraries(fmodstudio INTERFACE fmod)   # studio → core 의존
    endif()
    target_link_libraries(game_deps INTERFACE fmodstudio)
endif()
```

**조합 정책:**
- Core 만 설치 → `fmod` 타겟만, `game_deps` 에 `fmod` 만 합류
- Studio 까지 설치 → `fmodstudio` 추가, `game_deps` 에 둘 다 합류, `fmodstudio` 가 `fmod` 를 INTERFACE 의존성으로 잡음 → 링크/로딩 순서 자동
- 둘 다 미설치 → STATUS 메시지만, game 챕터는 link error 로 막힘 (의도)

**런타임 의무**: 두 라이브러리 모두 dynamic → 6절 POST_BUILD copy 가 양쪽 다 처리해야 한다.

## 6. 챕터에서 사용 — POST_BUILD 런타임 복사 (game_deps 사용 챕터 필수)

```cmake
# project_deps + game_deps 만 링크 — fmod/fmodstudio 는 game_deps INTERFACE 자동 전파
target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps game_deps)

# 동적 라이브러리를 실행 파일 옆으로 복사 — 런타임 dt_needed/LC_LOAD_DYLIB 해석용.
# fmod / fmodstudio 각각 독립 가드: 설치된 것만 복사.
if(TARGET fmod)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:fmod> $<TARGET_FILE_DIR:${CHAPTER_NAME}>)
endif()
if(TARGET fmodstudio)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:fmodstudio> $<TARGET_FILE_DIR:${CHAPTER_NAME}>)
endif()
```

`if(TARGET ...)` 가드 덕분에 FMOD 미설치 환경에서도 빌드는 진행되며, 그 챕터는
link 단계에서 미해결 심볼로 막힌다 (의도된 동작).

> **macOS rpath**: FMOD dylib install_name 은 `@rpath/lib*.dylib`. `cmake/Dependency.cmake`
> 가 `CMAKE_BUILD_RPATH` / `CMAKE_INSTALL_RPATH` 에 `@loader_path` 를 전역 추가하므로
> 챕터별 추가 설정 불필요. POST_BUILD copy 로 옆에 옮긴 dylib 가 자동 해결된다.

### `.bank` 파일 배치

`.bank` 는 FMOD Studio 저작 도구에서 export 된 파일. 챕터 리소스로 같이 배포:

```
apps/chapterN/resources/banks/Master.bank
apps/chapterN/resources/banks/Master.strings.bank
```

기존 `resources/` POST_BUILD copy 가 실행 파일 디렉토리로 같이 옮긴다. 코드에서는
실행 위치 기준 상대 경로(`resources/banks/Master.bank`) 로 로드.

## 7. 주의 — 라이선스 / .gitignore

- FMOD 라이브러리 파일(`*.dylib` / `*.dll`)은 프로젝트 `.gitignore` 에서 이미 무시된다.
- FMOD 라이선스상 SDK 재배포에 제약이 있으므로, 산출물을 공개 저장소에 커밋하지 말 것.
- 각 개발 환경에서 본 문서대로 개별 설치한다.
