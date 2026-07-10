# 게임/엔진 라이브러리 의존성 등록 — 설계 스펙

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> 작성일: 2026-05-19

## 목표

게임/엔진 작업에 쓸 외부 라이브러리 5종(Box2D · Effekseer · EnTT · Tweeny · stb)을
`extern/` 서브모듈 + `lib/`·`include/` 사전 빌드 산출물로 등록하고,
`cmake/Dependency.cmake` 에 `game_deps` 집계 타겟으로 노출한다.
FMOD Core API 는 등록하지 않고 설치 가이드 문서만 작성한다.

## 배경 / 제약

- 기존 패턴: sb7 / glfw3 를 `IMPORTED STATIC` 으로 등록 → `project_deps` INTERFACE 로 집약.
  Debug 시 `_d` 접미사 라이브러리 자동 선택.
- 교수 제출용 — vcpkg 미사용, CMake 단독 완결. `lib/`·`include/` 산출물 체크인으로
  서브모듈 미초기화 상태에서도 빌드 가능해야 한다.
- 서브모듈은 **버전 추적 + 재빌드 소스**용일 뿐, 빌드 자체는 체크인된 산출물에 의존한다.
- macOS(Ninja) + Windows(MSVC) 양쪽 지원. macOS 산출물은 로컬, Windows 산출물은 CI 에서 생성.

## 결정 사항

| 항목 | 결정 |
|---|---|
| 통합 방식 | 서브모듈(`extern/`) + 사전 빌드 산출물 체크인 — sb7/glfw 패턴 |
| 링크 구조 | 신규 `game_deps` INTERFACE 집계 타겟 (`project_deps` 와 대칭, 무변경) |
| FMOD | 계획 제외 — `doc/FMOD_Setup.md` 가이드 문서만 작성 |
| Box2D 버전 | v2.4.1 (C++ API) 태그 고정 |

## 라이브러리 분류

### 헤더 온리 — 헤더만 `include/` 로 복사
- **EnTT** (skypjack/entt @ v3.13.x 최신) → `<include>/entt/entt.hpp` (single-include)
- **Tweeny** (mobius3/tweeny @ v3) → `include/tweeny/` (헤더 묶음 전체)
- **stb** (nothings/stb @ 특정 커밋 — 릴리스 태그 없음) → `<include>/stb_rect_pack.h`
  (`<include>/stb_image.h` 는 이미 존재 → 추가하지 않음)

### 컴파일 필요 — Debug/Release 빌드 후 `_d` 접미사 정리
- **Box2D** (erincatto/box2d @ v2.4.1)
  - 산출물: `lib/macos/libbox2d{,_d}.a`, `lib/windows/box2d{,_d}.lib`
  - 헤더: `include/box2d/`
- **Effekseer** (effekseer/Effekseer @ 1.7x 최신 릴리스 태그)
  - 산출물: `libEffekseer{,_d}` + `libEffekseerRendererGL{,_d}` (macOS `.a` / Windows `.lib`)
  - 헤더: `include/Effekseer/`
  - **최대 리스크** — 다중 산출물, 자체 GL 로딩, 크로스 플랫폼 사전 빌드가 까다로움.
    구현 계획에서 별도 태스크로 분리·검증한다.

## 서브모듈 레이아웃 (`.gitmodules`)

```
extern/sb7code      (기존)
extern/box2d        → https://github.com/erincatto/box2d.git        @ v2.4.1
extern/Effekseer    → https://github.com/effekseer/Effekseer.git     @ 1.7x 최신 태그
extern/entt         → https://github.com/skypjack/entt.git           @ v3.13.x 최신 태그
extern/tweeny       → https://github.com/mobius3/tweeny.git          @ v3
extern/stb          → https://github.com/nothings/stb.git            @ 특정 커밋
```

## `cmake/Dependency.cmake` 설계

기존 `project_deps` 블록은 **무변경**. 신규 라이브러리 블록을 그 아래 추가한다.

- `box2d` — `STATIC IMPORTED`, `IMPORTED_LOCATION` / `IMPORTED_LOCATION_DEBUG` (`_d`)
- `Effekseer`, `EffekseerRendererGL` — 각각 `STATIC IMPORTED` (동일 패턴)
- `entt`, `tweeny`, `stb_extra` — `INTERFACE` 타겟, include 경로만 노출
  (헤더는 이미 `include/` 에 있어 `project_deps` 가 노출하지만, `game_deps` 그룹화·명시성을 위해 INTERFACE 타겟으로 등록)
- **`game_deps`** — 신규 `INTERFACE` 타겟. 위 6개 타겟을 집계:
  `box2d` + `Effekseer` + `EffekseerRendererGL` + `entt` + `tweeny` + `stb_extra`

게임/엔진 챕터의 링크:
```cmake
target_link_libraries(타겟 PRIVATE project_deps game_deps)
```
일반 챕터는 `project_deps` 만 링크 — 물리/파티클 엔진을 불필요하게 링크하지 않는다.

## 빌드 스크립트 / CI

### `shell/BuildExternLibs.{sh,bat}`
기존 glfw3/sb7 빌드 단계 뒤에 추가:
- 헤더 온리 3종: 서브모듈에서 헤더를 `build_extern/output/include/` 로 복사
- Box2D: 서브모듈을 CMake 로 Release/Debug 빌드 → `libbox2d{,_d}` 정리
- Effekseer: 서브모듈을 CMake 로 Release/Debug 빌드 → `Effekseer` + `EffekseerRendererGL` 정리
- 스크립트 마지막 안내문에 신규 산출물 복사 경로 추가

### `.github/workflows/`
- `build-extern-libs.yml` (macOS) — 신규 라이브러리 빌드 단계 추가
- `build-msvc.yml` (Windows) — 신규 라이브러리 빌드 단계 추가

## FMOD 설치 가이드 — `doc/FMOD_Setup.md` (한국어)

빌드 통합은 미포함. 문서 내용:
- FMOD Core API SDK 다운로드 (계정 필요, GitHub 서브모듈 불가 이유)
- 헤더 배치: `include/fmod/` (`fmod.h`, `fmod_common.h`, `fmod_errors.h` 등)
- 정적 vs 동적 라이브러리 차이 설명
- 플랫폼별 배치:
  - macOS: `libfmod.dylib` / `libfmodL.dylib` (로깅 빌드)
  - Windows: `fmod_vc.lib` + `fmod.dll` / `fmodL_vc.lib` + `fmodL.dll`
- `Dependency.cmake` 에 나중에 추가할 `SHARED IMPORTED` 스텁 (주석 형태 예시)
- 동적 라이브러리는 실행 파일 옆으로 POST_BUILD 복사 필요함을 명시

## 문서 갱신 — `.claude/CLAUDE.md`

- `extern/` 서브모듈 목록에 신규 5종 반영
- Dependency layer 섹션에 `game_deps` 집계 타겟 설명 추가
- Reference 섹션에 `doc/FMOD_Setup.md` 추가

## 검증 방법

단위 테스트 대상이 아닌 빌드 인프라 작업이므로, 각 태스크의 검증은
**임시 챕터(`apps/_deptest_/`)에서 해당 라이브러리 헤더 include + 대표 심볼 호출 → 빌드 성공**
으로 한다. 전체 완료 후 임시 챕터는 삭제하거나 비활성화한다.

검증 순서:
1. 헤더 온리 3종 — `#include` + 컴파일 통과
2. Box2D — `b2World` 생성 코드 링크 통과
3. Effekseer — `Effekseer::Manager` + `EffekseerRendererGL` 심볼 링크 통과
4. `game_deps` 한 줄 링크로 위 전부 동작

## 범위 밖 (Non-Goals)

- FMOD 실제 빌드 통합 (가이드 문서만)
- 신규 라이브러리를 실제 사용하는 게임/챕터 구현
- `project_deps` 변경
