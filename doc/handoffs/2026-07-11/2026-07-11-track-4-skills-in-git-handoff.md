# HO: 설계 규율 스킬 4종 프로젝트 사본 git 추적화 (맥락0 자기완결)

> 대상 에이전트: 이 문서만 읽고 실행 가능해야 한다. 예상 소요 Effort S (<30분).

## ① 배경 (맥락0)

이 레포의 설계 규율 전역 Skill들은 `~/.claude/skills/`(SSOT)에 살고, 프로젝트 사본은 `python3 scripts/dev.py copy-skills`로 `.claude/skills/`에 단방향 복사된다. 2026-07-10~11에 아래 4종이 대폭 보강되었는데(신규 구조물 신고 §2.5 / 확신도-행동 바인딩 §1.5 / Self-review gate / 결정 상태 태그+Lock 정합표), 현재 `.gitignore:6`의 `.claude/skills/` 규칙 때문에 사본이 **git 미추적**이라 클론/CI/다른 머신에서 이 규율들이 보이지 않는다. 사용자 결정(2026-07-11): **아래 4종만 예외로 git 추적**한다.

- `design-decision-discipline` / `confidence-and-sourcing` / `code-design-review-lenses` / `architecture-design-workflow`

## ② 소유 파일 (이 HO만 수정 가능)

- `.gitignore` (6행 부근의 Claude Code 블록만)
- `.claude/skills/{design-decision-discipline,confidence-and-sourcing,code-design-review-lenses,architecture-design-workflow}/` (복사 대상)
- 그 외 파일 불가침. 특히 다른 15개 스킬 디렉토리·`scripts/dev.py`·전역 `~/.claude/skills/` 원본은 수정 금지.

## ③ 작업 사양

1. **최신 사본 확보**: `python3 scripts/dev.py copy-skills` 실행 (4종 포함 11종을 전역→프로젝트로 복사. 이미 최신일 수 있으나 재실행이 안전).
2. **`.gitignore` 수정 — ⚠ 핵심 함정**: git은 **ignore된 디렉토리 내부를 negation으로 재포함할 수 없다** ("It is not possible to re-include a file if a parent directory of that file is excluded"). 따라서 `!.claude/skills/<name>/`만 추가하면 **작동하지 않는다**. 부모 규칙을 `*` 형으로 바꿔야 한다 — 이 레포의 `lib/` 패턴(`.gitignore` "C/C++" 절: `lib/*` + `!lib/windows/` + `!lib/windows/**`)이 동일 기법의 사내 선례다:

```gitignore
########### Claude Code ###########
.claude/settings.local.json
# 스킬 사본은 기본 미추적 (SSOT = ~/.claude/skills, 동기화 = scripts/dev.py copy-skills)
.claude/skills/*
# 설계 규율 4종만 추적 예외 (2026-07-11 사용자 결정 — 클론/CI에서도 규율 가시화)
!.claude/skills/design-decision-discipline/
!.claude/skills/design-decision-discipline/**
!.claude/skills/confidence-and-sourcing/
!.claude/skills/confidence-and-sourcing/**
!.claude/skills/code-design-review-lenses/
!.claude/skills/code-design-review-lenses/**
!.claude/skills/architecture-design-workflow/
!.claude/skills/architecture-design-workflow/**
```

3. **추적 개시**: `git add .claude/skills/design-decision-discipline .claude/skills/confidence-and-sourcing .claude/skills/code-design-review-lenses .claude/skills/architecture-design-workflow .gitignore`
4. **커밋** (소유 경로 파셜만 — `git add -A` 금지, 사용자가 같은 트리에서 병렬 작업함): `git commit -m "[chore] 설계 규율 스킬 4종 git 추적화 (.gitignore 예외 + 사본 추가)" -- .gitignore .claude/skills/design-decision-discipline .claude/skills/confidence-and-sourcing .claude/skills/code-design-review-lenses .claude/skills/architecture-design-workflow`

## ④ 수신 게이트 (착수 전 실측 — P10 프로토콜)

1. `git log --oneline -3` + `git status --short` 실측 → 아래 '작성 시점 상태'와 대조. 불일치(특히 `.gitignore`가 이미 수정돼 있거나 스킬이 이미 추적 중)면 **작업 전 보고**.
2. `git check-ignore -v .claude/skills/design-decision-discipline/SKILL.md` → `.gitignore:6:.claude/skills/` 매치가 나오는지 확인 (안 나오면 이미 처리된 것 — 중단·보고).
3. 4개 스킬 디렉토리에 SKILL.md가 실존하는지 `ls` 확인.

**작성 시점 상태 (2026-07-11 실측)**: 브랜치 `game/main`, HEAD `6df8f2f`, working tree 1줄(사용자 병렬분 — 건드리지 말 것). ignore 매치 = `.gitignore:6:.claude/skills/`. 4종 사본 모두 실존·최신 (design-decision-discipline 115줄 / confidence-and-sourcing 47줄 / code-design-review-lenses 70줄 / architecture-design-workflow 96줄).

## ⑤ green 체크포인트

- `git check-ignore .claude/skills/design-decision-discipline/SKILL.md` → **exit 1** (미무시). 나머지 3종 동일.
- 대조군: `git check-ignore .claude/skills/socratic-tutor/SKILL.md` → **exit 0** (다른 15종은 여전히 무시 — 예외가 4종에만 한정됐다는 증명).
- `git status --short .claude/skills/` 에 4종만 `A ` 로 표시.
- 커밋이 pre-commit 훅(문서 경로 감사)을 통과. ※ 스킬 파일 내 예시 경로가 훅에 걸리면 — 스킬은 교육 자료라 감사 제외가 레포 정책 — 해당 파일이 훅 대상에 포함된 것 자체를 보고하고 사용자 판단을 받을 것 (임의로 `--no-verify` 금지).

## ⑥ 금지

- 다른 스킬 15종을 추적에 포함 금지 (사용자 결정은 4종 한정).
- `~/.claude/skills/` 전역 원본 수정 금지 (이 작업은 사본 추적화일 뿐).
- `git add -A` / bare `git commit` 금지 — 반드시 위 파셜 경로.
- 확정 기록은 [제안됨] 규율 유지. 발견한 소유 외 문제는 수정 말고 보고만.

## 운용 노트 (커밋 메시지·완료 보고에 남길 것)

추적화 이후에는 전역 스킬을 수정할 때마다 `copy-skills` → **4종 사본의 diff가 git에 노출**되므로, 스킬 갱신 커밋에 사본 갱신을 함께 담는 운용이 된다 (SSOT는 여전히 전역 — 사본 직접 수정 금지, 수정은 전역에서 하고 복사).
