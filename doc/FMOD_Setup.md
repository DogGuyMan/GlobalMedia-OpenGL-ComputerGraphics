# FMOD Core API 설치 가이드

FMOD Core API 는 독점 SDK 라 GitHub 서브모듈로 등록할 수 없다. 아래 절차로 수동 설치한다.
이 문서는 설치 방법만 다루며, 빌드 통합(`Dependency.cmake` 등록)은 별도 작업이다.

## 1. SDK 다운로드

- https://www.fmod.com 가입 후 로그인
- Download → FMOD Engine → 플랫폼별 패키지 내려받기
  - macOS: FMOD Engine (macOS)
  - Windows: FMOD Engine (Windows)
- 버전은 코드가 참조하는 API 버전(예: 2.03)에 맞춘다.

## 2. 헤더 배치

SDK 의 `api/core/inc/` 안의 헤더를 `include/fmod/` 로 복사한다.

```
include/fmod/fmod.h
include/fmod/fmod_common.h
include/fmod/fmod_errors.h
include/fmod/fmod_codec.h
include/fmod/fmod_dsp.h
include/fmod/fmod_output.h
```

코드에서는 `#include <fmod/fmod.h>` 로 포함한다.

## 3. 정적 vs 동적 라이브러리

FMOD Core 는 **동적 라이브러리만** 배포된다 (정적 링크 라이브러리는 제공되지 않음).
- 일반 빌드: `fmod` (최적화)
- 로깅 빌드: `fmodL` (FMOD_DEBUG 로그 — 개발 중 권장)

## 4. 라이브러리 배치

### macOS — SDK 의 `api/core/lib/`
```
lib/macos/libfmod.dylib
lib/macos/libfmodL.dylib
```

### Windows — SDK 의 `api/core/lib/x64/`
```
lib/windows/fmod_vc.lib    fmod.dll      (링크용 import lib + 런타임 DLL)
lib/windows/fmodL_vc.lib   fmodL.dll
```

## 5. Dependency.cmake 등록 (향후 작업 — 참고용 스텁)

```cmake
# ====== FMOD Core API (수동 설치 — doc/FMOD_Setup.md 참조) ======
if(EXISTS "${CMAKE_SOURCE_DIR}/include/fmod/fmod.h")
    add_library(fmod SHARED IMPORTED)
    if(WIN32)
        set_target_properties(fmod PROPERTIES
            IMPORTED_IMPLIB   ${LIB_DIR}/fmod_vc.lib
            IMPORTED_LOCATION ${LIB_DIR}/fmod.dll)
    else()
        set_target_properties(fmod PROPERTIES
            IMPORTED_LOCATION ${LIB_DIR}/libfmod.dylib)
    endif()
    target_link_libraries(game_deps INTERFACE fmod)
else()
    message(STATUS "FMOD 미설치 — 오디오 비활성. doc/FMOD_Setup.md 참조")
endif()
```

## 6. 동적 라이브러리 런타임 배치

`.dylib`/`.dll` 은 실행 파일과 같은 디렉토리에 있어야 한다.
챕터 `CMakeLists.txt` 의 POST_BUILD 단계에서 실행 파일 옆으로 복사한다.

```cmake
add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:fmod> $<TARGET_FILE_DIR:${CHAPTER_NAME}>)
```

## 7. 주의 — 라이선스 / .gitignore

- FMOD 라이브러리 파일(`*.dylib` / `*.dll`)은 프로젝트 `.gitignore` 에서 이미 무시된다.
- FMOD 라이선스상 SDK 재배포에 제약이 있으므로, 산출물을 공개 저장소에 커밋하지 말 것.
- 각 개발 환경에서 본 문서대로 개별 설치한다.
