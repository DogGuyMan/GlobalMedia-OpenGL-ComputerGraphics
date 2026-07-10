# HO-1 — `scripts/audit_docs.py` 신설 (핸드오프, 맥락0 자기완결)

> AI-Readiness 70% 플랜의 인프라 패키지 3건 중 1번. 정본 플랜 = [`doc/superpowers/plans/2026-07-10-ai-readiness-70pct-plan.md`](../../superpowers/plans/2026-07-10-ai-readiness-70pct-plan.md) §3 HO-1 행 + §4 "F — 신선도" 절. 원 사양 = [`doc/번복기록/2026-07-10-쟁점별-비교분석-및-파이프라인개선안.md`](../../번복기록/2026-07-10-쟁점별-비교분석-및-파이프라인개선안.md) B부 P9(경로검증) — 이 문서를 먼저 열어 P9 원문을 대조할 필요는 없다. 아래 ③이 실행 수준으로 이미 풀어놓았다.

## ① 배경

이 저장소(`GlobalMedia-OpenGL-ComputerGraphics`, C++17/OpenGL 엔진+게임 데모)는 2026-07-10 AI-Readiness 감사(`doc/report/ai-readiness-score.json`)에서 **18/100 (AI-Hostile)** 등급을 받았다. 카테고리 E(검증 게이트, 5/15)의 최대 감점 항목은 **hallucinated path 14건** — 컨텍스트 문서(`.claude/architecture.md`, `.claude/CLAUDE.md`, `doc/superpowers/plans/2026-05-24-M2-prompts/README.md`)가 존재하지 않는 파일 경로를 인용하고 있다. 카테고리 F(신선도, 2/10)는 이런 부패를 잡아내는 자동 장치(CI/훅)가 전무하다. HO-1 은 이 두 카테고리(E·F)의 **엔진**이 될 `scripts/audit_docs.py` 를 신설한다 — 스크립트 자체는 이번 감사에서 채점 대상이 아니지만(스코어러가 `scripts/`를 컨텍스트 카테고리로 채점하지 않음), HO-4(E1 실제 정화)와 HO-5(pre-commit/CI 게이트)가 이 스크립트에 의존한다.

**계측기 고정**: 이 저장소의 재채점은 반드시 `python3 doc/report/score_cpp.py . --json <출력경로>` 로 실행한다. `doc/report/` 밖의 원본 `score.py`(범용 cartography skill 것)는 **사용 금지** — C++/셰이더 확장자(`.h .hpp .cpp .cc .slang .vert .frag` 등)를 인식하지 못해 이 저장소를 항상 과소평가하며, 이전 결과와 비교 불가능해진다.

## ② 소유 파일 목록

이 핸드오프가 **생성**할 파일은 아래뿐이다. 다른 경로는 절대 건드리지 않는다 (다른 HO 소유 — 병렬 실행 중일 수 있음).

- `scripts/audit_docs.py` (신규)

검증만 하고 수정하지 않는 대상(읽기 전용 참조):
- `doc/report/score_cpp.py` — `RE_PATH_REF`/`IGNORE_DIRS` 등 기존 검증 로직 재사용 대상 (import 하지 말고 **동일 패턴을 audit_docs.py 안에 자체 상수로 재정의** — `scripts/`와 `doc/report/`는 서로 다른 배포 단위이므로 cross-import 금지)
- `scripts/find_naming_violations.py` — docstring/CLI 관례 참고용 (Korean docstring, `ROOT = Path(__file__).parent.parent`, 신뢰도 등급 표기 스타일)

## ③ 작업 사양

`scripts/audit_docs.py` — pure stdlib(외부 의존 0), `#!/usr/bin/env python3`, 한국어 docstring. 기존 `scripts/find_*.py` 관례(신뢰도 등급 표기 + "코드를 수정하지 않는다" 선언)를 따른다.

**상수**
- `IGNORE_DIRS` — `doc/report/score_cpp.py:31-38` 의 집합을 그대로 복제(node_modules/.git/build_ninja/extern/lib/include/... 등).
- `RE_PATH_REF` — `doc/report/score_cpp.py:45-48` 의 정규식을 **그대로 복제** (C++/셰이더 확장자 `h|hpp|cpp|cc|inl|cmake|slang|vert|frag|glsl|geom|comp|dot` 이미 포함 — 재설계 불필요, 원본 P9 사양이 "재정의 필요"라 적은 항목은 이미 score_cpp.py 단계에서 해결돼 있었다. 그대로 가져다 쓸 것).
- `RE_COMPOUND_REF = re.compile(r"\.\w+/\.\w+$")` — `Foo.h/.cpp` 류 복합 표기(예: 이번 감사에서 실제 검출된 `apps/_MyApp_/src/InputHandler/CameraController.h/.cpp`) 탐지용. 단일 파일로 존재 검증이 불가능한 저자 표기 관용구.
- `RE_SUPERSEDED_MARK = re.compile(r"SUPERSEDED|폐기|deprecated", re.IGNORECASE)` — 문서가 이미 스스로 "낡음" 표시를 했는지 런타임 감지(하드코딩 리스트 금지).

**함수**
- `discover_markdown_files(scope: list[str], repo: Path) -> list[Path]` — `scope`의 각 항목이 디렉토리면 `IGNORE_DIRS` 를 건너뛰며 재귀적으로 `*.md` 전부 수집, 파일이면 그대로 포함. (주의: `doc/handoffs/*.md`는 CLAUDE.md 같은 고정 파일명이 아니라 날짜 기반 임의 파일명이므로 `score_cpp.py`의 `CONTEXT_FILES` 필터를 쓰지 말 것 — 모든 `.md`를 대상으로 한다.)
- `extract_refs(text: str) -> set[str]` — `RE_PATH_REF.findall(text)` 후 `set()` (동일 경로 중복 참조는 1건으로 카운트, `score_cpp.py` 동일 관례).
- `classify_ref(ref: str) -> "compound" | "normal"` — `RE_COMPOUND_REF` 매치 시 `"compound"`.
- `resolve_ref(ref: str, doc_path: Path, repo: Path) -> bool` — 후보 4개: `repo/ref`, `doc_path.parent/ref`, `repo/"include"/ref`, `repo/"src"/ref` (2-base OR + include/·src/ 베이스, `score_cpp.py:454` 와 동일 규약). 하나라도 존재하면 True.
- `compute_drift(doc_path: Path, valid_refs: list[str], repo: Path) -> dict | None` — `doc_path`가 이미 `RE_SUPERSEDED_MARK` 매치면 `None`(스킵). 아니면 `valid_refs`가 가리키는 실제 파일들의 mtime 중 최댓값과 `doc_path` mtime 비교, `doc_mtime + 30*86400 < code_max_mtime` 이면 SUPERSEDED **후보**(`{"reason": ..., "code_file": ..., "confidence": "Heuristic"}`) 반환 — **자동 마킹 금지, 후보 제시만**. 참조 없는 문서는 drift 판정 대상에서 제외(중립).
- `audit_document(doc_path: Path, repo: Path) -> dict` — 위 함수들을 조합해 `{path, refs_total, refs_broken: [...], refs_skipped_compound: [...], drift_candidate: dict|None}` 반환. `refs_total == 0` 이면 중립(broken 0, 경고 없음 — `score_cpp.py` E1 의 "참조 0건은 중립" 원칙 계승).
- `run_audit(scope: list[str], repo: Path) -> dict` — 전체 집계: `{documents: [...], summary: {total_docs, total_refs, total_broken, total_skipped_compound, drift_candidates}}`.
- 출력 3종 (요구사항 — 반드시 분리):
  - `write_json(report: dict, path: str)` — 구조화 전체 결과.
  - `print_console_summary(report: dict)` — 사람이 읽는 요약(문서 수·broken 수·skip 수·drift 후보 수, 신뢰도 태그 `[Auto]`/`[Heuristic]` 병기).
  - `print_action_list(report: dict)` — broken 항목만 뽑아 `<file>: <ref>` 액션 리스트(수정 유도용).
- `main()` — `argparse`: `--scope`(nargs="+", default=`["doc/handoffs", "doc/superpowers/specs"]`), `--json`(출력 경로), `--quiet`(콘솔 요약 억제), 위치 인자 없음(레포 루트는 항상 `Path(__file__).parent.parent`로 고정 — 다른 스크립트 관례와 통일).

**종료 코드 (HO-5가 게이트로 재사용 — 반드시 구현)**: broken(compound 제외 순수 존재-실패) 건수가 1 이상이면 `sys.exit(1)`, 아니면 `sys.exit(0)`. drift 후보는 종료 코드에 영향 없음(후보 제시일 뿐 실패 아님).

**신뢰도 태그**: 모든 finding 항목에 `"confidence": "Auto"`(존재 검증 — 결정론적) 또는 `"confidence": "Heuristic"`(drift SUPERSEDED 후보 — 판단 필요) 필드를 붙인다.

## ④ 수신 게이트

작업 착수 전 아래를 **직접 실행**해 실측하고, 아래 "작성 시점 상태"와 대조한다. 불일치 시 — 특히 `scripts/audit_docs.py` 가 이미 존재한다면 — 작업 진행 전에 보고부터 할 것 (다른 세션이 먼저 만들었을 수 있다).

```bash
git log --oneline -5
git status --short | wc -l
ls scripts/audit_docs.py 2>&1   # 존재하면 안 됨 — 존재 시 보고
```

**작성 시점 상태 (2026-07-10 실측)**
- 브랜치: `refactor/pcb-to-worldscene`
- HEAD: `5590cc9` (`5590cc902d4774964c09d56bfa7ca5df81ced44d`, "[chore] : 주석 수정")
- `git status --short` 474줄(대부분 staged — `doc/` 재편성 + `.claude/` 신설 스캐폴딩), untracked 11건. **매우 dirty한 working tree** — 사용자가 병렬로 다른 작업을 진행 중일 가능성이 높다(memory: `user-parallel-git-and-builds`). 커밋은 반드시 파셜(⑥ 참조).
- `scripts/audit_docs.py` 미존재 확인 완료(2026-07-10 시점).

## ⑤ green 체크포인트

```bash
python3 scripts/audit_docs.py --scope .claude doc/superpowers/plans/2026-05-24-M2-prompts --json /tmp/audit_report.json
```

**통과 기준**: `summary.total_broken == 14` (compound 스킵 2건은 `refs_skipped_compound` 로 별도 집계되어 `total_broken` 에서 제외됨에 유의 — 순수 broken 12 + compound-skip 2 = 14 인지, 혹은 compound 도 broken 에 포함해 14 인지는 구현 시 `resolve_ref`가 compound 를 별도 트랙으로 완전히 빼는지에 따라 달라진다. **핵심은 "이 14개 경로가 하나도 빠짐없이 보고서 어딘가(broken 또는 skipped_compound)에 잡히는가"** — `doc/report/ai-readiness-score.json` 의 `categories.E.evidence.ref_broken: 14` 와 대조해 개수 재현을 확인할 것. 목록: `module/file.h`, `app/main.cpp`, `app/config.h`, `doc/design/2026-05-19-input-module-design.md`, `vendor/header.h`, `vendor/file.h`, `src/renderer/renderer.h`, `spdlog/spdlog.h`, `glad/glad.h`, `cmake/Config.cmake`, `src/context/game_action.h` (이상 `.claude/architecture.md` 11건) + `doc/html/index.html`(`.claude/CLAUDE.md`) + `test/test_uniform_atlas.cpp`, `apps/_MyApp_/src/InputHandler/CameraController.h/.cpp`(이상 archival README 2건, 후자가 compound 케이스).

추가로 `python3 scripts/audit_docs.py --scope doc/handoffs` 를 인자 없이도(기본 scope) 에러 없이 실행되는지 확인(빈 결과라도 크래시 금지).

## ⑥ 금지 사항

- 이 스크립트 밖의 어떤 파일도 수정하지 않는다 — 특히 `.claude/architecture.md`/`.claude/CLAUDE.md`/`doc/superpowers/plans/...` 는 **HO-4 소유**. HO-1은 검증 스크립트만 만든다.
- `doc/report/score_cpp.py` 를 import 하거나 수정하지 않는다 (읽기 전용 참조만).
- 커밋은 `git commit scripts/audit_docs.py` 처럼 **소유 경로만 파셜 커밋**한다. `git add -A`/`git commit -a` 금지 — working tree 에 474줄의 다른 변경(사용자 작업 포함)이 이미 떠 있다.
- drift 후보를 자동으로 문서에 "[SUPERSEDED]" 라고 써넣지 않는다 — 후보 제시(JSON/콘솔)까지만, 문서 수정은 사람 몫.
- 하드코딩된 "SUPERSEDED 문서 개수" 같은 매직넘버 금지 — 항상 실행 시점 동적 감지.
- 이 스크립트에 대한 단위 테스트를 임의로 새로 만들지 않는다(사용자가 명시 요청 시에만 — `no_auto_tests` 관례).
- 소유 파일 밖에서 발견한 문제(예: 다른 문서의 오류)는 수정하지 말고 보고만 남긴다.
