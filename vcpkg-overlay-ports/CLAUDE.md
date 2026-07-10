# vcpkg-overlay-ports/ — CLAUDE.md
box2d 의 arm64 테스트 빌드 버그를 교정하는 로컬 vcpkg overlay-port. 표준 vcpkg 레지스트리의 box2d 포트를 이 로컬 레시피가 대체한다.

## Purpose (owns / configures)
- box2d 포트의 arm64 테스트 빌드 버그 교정 (upstream 포트 레시피 오버라이드)
- box2d 2.4.1 버전 고정 메타데이터 (`vcpkg.json` 의 override 와 짝)

## Quick commands
```bash
# 이 디렉토리는 직접 실행 대상이 아니다 — vcpkg 가 manifest 빌드 시 자동 소비한다.
# 소비 여부 확인:
cmake --preset ninja --fresh   # 재구성 시 vcpkg 가 overlay-ports 를 다시 읽음
```

## Key files
- `vcpkg-overlay-ports/box2d/portfile.cmake` — box2d 빌드 절차 (arm64 테스트 교정)
- `vcpkg-overlay-ports/box2d/vcpkg.json` — 포트 메타데이터 / 버전 (2.4.1)

## Gotchas
- 주의: 이 overlay 는 별도 `vcpkg-configuration.json` 파일이 아니라 **루트 `vcpkg.json` 내부의 인라인 `"vcpkg-configuration"` 키**(`overlay-ports: ["./vcpkg-overlay-ports"]`)로 등록된다 — Why: `vcpkg-configuration.json` 파일 자체는 이 레포에 없어서(2026-07-10 실측) 별도 파일을 찾으면 "배선 없음"으로 오판하기 쉽다. vcpkg 는 manifest(`vcpkg.json`) 안에 이 키를 직접 인라인할 수 있는 대안 문법을 지원한다.
- 주의: box2d 는 2.4.1 로 override 핀 고정 — Why: v3(3.x)는 API 가 대폭 바뀌어(36파일 규모) 게임 물리 코드(`apps/_MyApp_/src/Physics/`)와 불일치한다.
- 주의: overlay 포트 레시피(`portfile.cmake`)를 수정하면 vcpkg 바이너리 캐시가 stale 포트를 재사용할 수 있다 — Why: 캐시 무효화(`--fresh` 재구성 또는 vcpkg 캐시 삭제) 없이는 변경이 반영 안 될 수 있다.

## Cross-module deps
- 의존: 루트 `vcpkg.json` 의 `overrides`(box2d 2.4.1) + `vcpkg-configuration.overlay-ports` 필드 + box2d upstream 소스
- 피의존: `apps/_MyApp_/src/Physics/`(box2d::box2d 링크) + `box2d_demo`(현재 비활성)

## Common modification patterns
- box2d 버전 override 변경: 루트 `vcpkg.json` 의 `overrides` 항목 + 이 디렉토리 `vcpkg.json` 버전 필드 동시 갱신(불일치 시 빌드 오류).
- `portfile.cmake` 패치: vcpkg 바이너리 캐시 무효화(`--fresh` 재구성) 필요 여부 확인.
- 새 overlay port 추가: 루트 `vcpkg.json` 의 `vcpkg-configuration.overlay-ports` 배열에 경로 추가.

## See also
- [ARCHITECTURE.md](../ARCHITECTURE.md) (HO-3 생성 중) · [결정 스토어](../doc/adr/README.md) · [vcpkg.json](../vcpkg.json)
