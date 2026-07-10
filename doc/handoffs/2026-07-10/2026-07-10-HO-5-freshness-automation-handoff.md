# HO-5 — 신선도 자동화: pre-commit 훅 + CI docs-validation (핸드오프, 맥락0 자기완결)

> AI-Readiness 70% 플랜의 인프라 패키지 3건 중 3번. 정본 플랜 = [`doc/superpowers/plans/2026-07-10-ai-readiness-70pct-plan.md`](../../superpowers/plans/2026-07-10-ai-readiness-70pct-plan.md) §3 HO-5 행 + §4 "F — 신선도" 절. **선행 = HO-1 완료 필수** (`scripts/audit_docs.py` 없이는 이 HO 자체가 성립하지 않는다).

## ① 배경

이 저장소는 2026-07-10 AI-Readiness 감사(`doc/report/ai-readiness-score.json`)에서 18/100(AI-Hostile)을 받았다. 카테고리 **F(신선도)는 2/10** — CI에 문서 검증 워크플로가 없고(`ctx_validation_workflow` 우연히 true지만 실질 검증 없음), pre-commit/pre-push 훅도 없다(`hook_validates_paths: false`). HO-5는 HO-1이 만든 `scripts/audit_docs.py`를 **실제 게이트**로 배선한다 — 커밋 시점(로컬)과 PR 시점(CI) 양쪽에서 hallucinated path 재발을 차단.

**계측기 고정**: 재채점은 반드시 `python3 doc/report/score_cpp.py . --json <출력경로>` 로 실행한다. cartography skill 의 원본 `score.py` 사용 금지(C++ 확장자 미인식).

## ② 소유 파일 목록

- `.husky/pre-commit` (신규 — 실제 훅 스크립트)
- `.github/workflows/docs-validation.yml` (신규 — CI 게이트)

다른 경로는 절대 건드리지 않는다. `scripts/audit_docs.py`(HO-1 소유, 읽기 전용 참조만), `.claude/architecture.md` 등 문서 본문(HO-4 소유), `ARCHITECTURE.md`(HO-3 소유) 불가침. 기존 `.github/workflows/docs.yml`/`build-msvc.yml`/`build-extern-libs.yml` 은 **수정하지 않고** 새 워크플로 파일을 추가한다(공존 — 다른 워크플로 소유자 아님).

## ③ 작업 사양

### ⚠ 계측기 함정 (착수 전 필독)

`doc/report/score_cpp.py` 의 F 채점 함수(`score_f`, 대략 560행 부근)는 `hook_validates_paths` 를 **정확히 `(repo/".husky"/"pre-commit").exists() or (repo/".husky"/"pre-push").exists()`** 로만 판정한다 — `.git/hooks/`나 `.githooks/` 같은 다른 이름은 인식하지 않는다(Node/husky 생태계 전제로 작성된 채점 로직이 C++ 프로젝트에도 그대로 남아있는 미세한 계측기 결함 — 수정 대상 아님, `score_cpp.py`는 고정 계측기이므로 있는 그대로 만족시킨다). 동시에 `.git/hooks/`는 **git이 추적하지 않는 디렉터리**라 커밋해도 팀원/CI에 전파되지 않는다.

**해결**: `.husky/` 를 순수 디렉터리 이름으로만 재사용한다(Node/npm/husky 패키지 설치 불필요) — `git config core.hooksPath .husky` 로 로컬 git 이 이 디렉터리에서 훅을 찾도록 지정하면, `.husky/pre-commit` 는 이름만 husky 스타일일 뿐 **실제로 이 저장소가 실행하는 진짜 훅**이 된다(가짜 스텁 아님 — 채점 로직의 파일명 요구사항과 실제 동작 메커니즘이 우연히 일치).

### 3-1. `.husky/pre-commit` (실행 가능 셸 스크립트, `chmod +x` 필수)

```bash
#!/usr/bin/env bash
# pre-commit — 스테이징된 마크다운 문서의 경로 참조를 scripts/audit_docs.py 로 검증.
# 활성화: `git config core.hooksPath .husky` (1회, 로컬 git config — 리포별 설정이며 팀원 각자 실행 필요.
#          README나 CONTRIBUTING에 안내 문구 추가를 검토할 것, 단 이 HO 소유 파일 밖이므로 발견만 하고 실제 추가는 별도 판단).
set -uo pipefail

staged_md=$(git diff --cached --name-only --diff-filter=ACM -- '*.md')
if [ -z "$staged_md" ]; then
  exit 0
fi

# shellcheck disable=SC2086
python3 scripts/audit_docs.py --scope $staged_md
status=$?

if [ "$status" -ne 0 ]; then
  echo "" >&2
  echo "BLOCKED: 스테이징된 문서에 존재하지 않는 경로 참조가 있다 (scripts/audit_docs.py)." >&2
  echo "위 액션 리스트를 확인해 경로를 고치거나, 예시/placeholder 표기(<>)로 전환한 뒤 다시 커밋할 것." >&2
  exit 1
fi
exit 0
```

`--scope $staged_md` 는 HO-1 의 `discover_markdown_files` 가 "파일이면 그대로 포함" 하도록 설계됐으므로 개별 파일 나열이 그대로 동작해야 한다(HO-1 스펙과의 계약 — 만약 HO-1 산출물이 디렉터리만 받게 구현돼 있다면 이 훅에서 실패할 것이니, 착수 전 `python3 scripts/audit_docs.py --scope .claude/CLAUDE.md` 처럼 단일 파일 인자로 먼저 스모크 테스트할 것).

### 3-2. `.github/workflows/docs-validation.yml`

```yaml
name: Docs Validation

on:
  pull_request:
  push:
    branches: ['**']

jobs:
  audit-docs:
    name: audit_docs.py broken-path gate
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Run audit_docs.py (hard gate — non-zero exit fails the job)
        run: python3 scripts/audit_docs.py --scope .claude doc/superpowers/plans doc/superpowers/specs doc/handoffs

      - name: Run score_cpp.py (informational — full category snapshot)
        if: always()
        continue-on-error: true
        run: python3 doc/report/score_cpp.py . --markdown
```

첫 스텝이 하드 게이트(HO-1 스펙상 broken>0 이면 `sys.exit(1)` → job 실패 → PR 머지 차단 가능). 두번째 스텝은 정보성이라 `continue-on-error: true` — 이 워크플로의 목적이 "카테고리 총점 게이트"가 아니라 "hallucinated path 재발 방지"이므로 실패해도 job 자체를 막지 않는다.

## ④ 수신 게이트

착수 전 아래를 직접 실행해 실측하고 "작성 시점 상태"와 대조. **특히 `scripts/audit_docs.py` 실존 확인은 필수 선행 조건** — 없으면 HO-1이 아직 안 끝난 것이므로 이 HO는 착수하지 말고 대기/보고할 것.

```bash
git log --oneline -5
git status --short | wc -l
ls -la scripts/audit_docs.py                    # 필수 — 없으면 HO-1 미완료, 착수 중단하고 보고
python3 scripts/audit_docs.py --scope .claude/CLAUDE.md   # 단일 파일 --scope 스모크 테스트 (3-1 참고)
ls .github/workflows/
git config --get core.hooksPath                 # 비어있어야 정상(아직 미설정) — 이미 다른 값이면 보고
```

**작성 시점 상태 (2026-07-10 실측)**
- 브랜치: `refactor/pcb-to-worldscene`, HEAD: `5590cc9` (`5590cc902d4774964c09d56bfa7ca5df81ced44d`, "[chore] : 주석 수정")
- `git status --short` 474줄(대부분 staged), untracked 11건 — 매우 dirty, 사용자 병렬 작업 가능성(memory: `user-parallel-git-and-builds`). 파셜 커밋 필수.
- `scripts/audit_docs.py` **2026-07-10 시점 미존재** (이 문서 작성 시점 = HO-1 착수 전) — **이 HO의 실행자는 착수 전에 반드시 재확인**할 것. 존재하면 그 스크립트의 실제 CLI 계약(특히 개별 파일 `--scope` 지원 여부와 exit code 규약)을 먼저 읽고 3-1/3-2 의 호출부와 어긋나면 조정할 것(단 `scripts/audit_docs.py` 파일 자체는 수정하지 않는다 — HO-1 소유).
- `.github/workflows/` 에는 `build-extern-libs.yml`/`build-msvc.yml`/`docs.yml` 3종만 존재, `docs-validation.yml` 없음.
- `core.hooksPath` 미설정(기본값), `.husky/` 디렉터리 없음.

## ⑤ green 체크포인트

**훅 실동작 검증**:
```bash
git config core.hooksPath .husky
chmod +x .husky/pre-commit
# 고의로 깨진 경로를 doc/handoffs 아래 임시 문서에 넣고 스테이징
printf '# temp\n존재하지 않는 참조: nope/does-not-exist.h\n' > doc/handoffs/2026-07-10/_ho5_smoke_test.md
git add doc/handoffs/2026-07-10/_ho5_smoke_test.md
git commit -m "smoke test — should be BLOCKED"
```
**통과 기준**: 위 `git commit` 이 훅에 의해 **차단**(exit 1, "BLOCKED" 메시지 출력)돼야 한다. 확인 후 `git reset HEAD doc/handoffs/2026-07-10/_ho5_smoke_test.md && rm doc/handoffs/2026-07-10/_ho5_smoke_test.md` 로 스모크 테스트 산출물을 반드시 정리할 것(실제 커밋에 남기지 않는다).

**재채점**:
```bash
python3 doc/report/score_cpp.py . --json /tmp/score_after_ho5.json
python3 -c "import json; d=json.load(open('/tmp/score_after_ho5.json')); print(d['categories']['F'])"
```

⚠ **정직 고지 — F≥6 도달 조건**: `score_f` 는 `hook_validates_paths`(+2, 이 HO 완료 시 달성) + `ctx_validation_workflow`(+2, 이미 true) 외에, **최대 +6점을 "모듈 context 파일 drift 없음"에 배정**하는데 이 항은 `measurable_modules > 0` 일 때만(즉 HO-2 의 모듈 CLAUDE.md 6장이 이미 존재해야) 활성화된다. HO-2가 아직 완료 전이면 이 HO 단독으로는 F=4 까지만 오르고 6에 못 미친다 — **이것은 이 HO의 실패가 아니라 플랜 §2 DAG상 Wave2(F)가 Wave1(A/B/C, HO-2)보다 뒤에 실행되는 것을 전제한 설계**다. 재채점 시 `evidence.measurable_modules` 를 확인해 0이면 "F=4 도달, 6은 HO-2 완료 후 자동 상승 예정"이라고 **있는 그대로 보고**할 것 — 억지로 6을 만들려고 가짜 모듈 파일을 만들지 않는다(HO-2 소유 침범 금지, ⑥).

## ⑥ 금지 사항

- 소유 파일 2개(`.husky/pre-commit`, `.github/workflows/docs-validation.yml`) 외 어떤 파일도 수정하지 않는다. 특히 `scripts/audit_docs.py`(HO-1), `.claude/architecture.md`/`.claude/CLAUDE.md`/archival README(HO-4), 모듈 CLAUDE.md 6장(HO-2), `ARCHITECTURE.md`(HO-3), 기존 `.github/workflows/docs.yml` 등 불가침.
- F≥6 을 억지로 맞추려고 HO-2 소유인 모듈 CLAUDE.md 를 대신 만들지 않는다 — 위 ⑤의 정직 고지대로 도달 가능한 지점까지만 보고.
- 스모크 테스트로 만든 임시 커밋/파일(`_ho5_smoke_test.md` 등)을 실제 커밋 이력에 남기지 않는다 — 검증 후 즉시 정리.
- 커밋은 `git commit .husky/pre-commit .github/workflows/docs-validation.yml` 처럼 **소유 경로만 파셜 커밋**. `git add -A` 금지 — working tree 에 474줄의 다른 변경(사용자 작업 포함)이 이미 떠 있다.
- `core.hooksPath` 설정은 로컬 git 설정(리포지토리 범위)이며 global(`--global`)로 바꾸지 않는다.
- 테스트를 임의로 새로 추가하지 않는다(`no_auto_tests`).
- 확정 수치("F=6 달성" 등)를 실측 없이 기록하지 않는다 — 반드시 ⑤의 명령을 실행한 실제 출력을 인용.
