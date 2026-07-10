# scripts/ — CLAUDE.md
Python 유틸리티 모음이다 — 두 갈래: (1) **개발 작업 통합 CLI `dev.py`** (구 `shell/` 의 sh/bat/ps1 삼중 스크립트를 크로스 플랫폼 단일 진입점으로 통합, 2026-07-10), (2) 명명 규칙 감사·문자 클린업·마이그레이션·문서 감사 등 일회성/유지보수 도구 leaf.

## Purpose (owns / configures)
- **빌드/실행/extern/문서 개발 작업 통합 CLI (`dev.py`)** — 구 `shell/` 전량 대체 (플랫폼 감지: macOS/Linux=Ninja, Windows=MSVC)
- 코드베이스 전수조사 도구 (명명 규칙 위반, 특수문자, 한정자, 추상화 경계선)
- 일회성 마이그레이션 스크립트 (vmath→glm, 심볼 리네임)
- 문서 경로 참조 감사 (`audit_docs.py`, HO-1 산출물)
- Slang→GLSL410 컴파일 post-process SSOT (`slang_compile.py`, `cmake/Slang.cmake` 가 호출)

## Quick commands
```bash
python3 scripts/dev.py all debug _MyApp_             # clean→configure→build→run (구 CMakeALL)
python3 scripts/dev.py run debug _MyApp_ --leaks     # 실행 + macOS 누수검사 (구 CMakeExecute)
python3 scripts/dev.py extern                        # extern 라이브러리 재빌드 (구 BuildExternLibs)
python3 scripts/dev.py --help                        # 전체 서브커맨드 (build/configure/prepare/doxygen/move-shaders/copy-skills/resume-claude/schedule-resume)
python3 scripts/find_naming_violations.py            # 명명 규칙 A/B/C 등급 전수조사
python3 scripts/audit_docs.py --scope <경로> --json <출력>  # 문서 경로 참조 감사
python3 scripts/replace_special_chars.py --apply      # 주석 특수문자 클린업 (적용 전 git diff 필수)
python3 scripts/migrate_vmath_to_glm.py --apply        # vmath→glm 안전 치환
```

## Key files
- `scripts/dev.py` — 개발 작업 통합 CLI (구 `shell/` 11종 통합: all/configure/build/run/prepare/extern/doxygen/move-shaders/copy-skills/resume-claude/schedule-resume)
- `scripts/leakloghandler.py` — macOS `leaks` 산출 `leaklog.txt` 정제 (GLFW vendored 시스템 누수 분리) — `dev.py run --leaks` 가 호출
- `scripts/audit_docs.py` — 문서 경로 참조 감사 (HO-1 완료, 실존)
- `scripts/find_naming_violations.py` — 멤버/지역/타입 명명 규칙 전수조사
- `scripts/slang_compile.py` — Slang→GLSL410 정규화 (varying 이름, sampler 접미사) SSOT
- `scripts/replace_special_chars.py` — 주석 특수문자 클린업, `--apply` 플래그
- `scripts/migrate_vmath_to_glm.py` — vmath→glm 안전 치환, `--apply` 플래그

## Gotchas
- 주의: `--apply` 플래그가 있는 스크립트(`replace_special_chars.py`, `migrate_vmath_to_glm.py`)는 실행 전 반드시 git diff 로 기능 문자열 미변경을 검증해야 한다 — Why: 대량 치환이 실수로 기능 코드를 오손할 수 있다.
- 주의: `find_naming_violations.py` 의 regex 는 `mXX` 류 오타를 못 잡는다 — Why: 정규식 기반 탐지의 사각지대라 수동 보완이 필요하다.
- 주의: 스크립트들은 저장소 루트에서 실행된다고 가정한다 — Why: 상대경로로 스캔 대상을 찾으므로 다른 디렉토리에서 실행하면 조용히 아무것도 못 찾을 수 있다.
- 주의: `scripts/rollback_stats.py` 는 이제 실존한다(HO-6 산출물, 번복 로그 집계기) — Why: 과거 이 문서가 "아직 없음"으로 적었던 게 stale 이 됐다. 새 스크립트 추가 시 이 문서도 같이 갱신할 것.

## Cross-module deps
- 의존: Python3 표준 라이브러리 위주(외부 의존 최소)
- 피의존: `dev.py` 는 개발자 + CI(`.github/workflows/build-extern-libs.yml` 가 `python scripts\dev.py extern` 호출)가 사용, `cmake/Slang.cmake` 는 `slang_compile.py` 를 빌드 시 호출. 나머지는 직접 코드 의존 없는 유지보수 도구 leaf

## Common modification patterns
- `dev.py` 서브커맨드 추가: 기존 서브커맨드 옆에 함수 추가 + `--help` 목록 갱신.
- 일회성 마이그레이션/감사 스크립트 추가: 루트 기준 실행 가정 유지, `--apply` 파괴적 플래그는 실행 전 git diff 검증 관례 준수.
- 감사기(`audit_docs.py`/`find_naming_violations.py`) 범위 확장: `--scope` 인자로 대상 디렉토리 추가.

## See also
- [ARCHITECTURE.md](../ARCHITECTURE.md) (HO-3 생성 중) · [결정 스토어](../doc/adr/README.md)
