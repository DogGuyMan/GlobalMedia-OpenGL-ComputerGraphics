# doc/ — CLAUDE.md

프로젝트 문서 허브 — API 레퍼런스·설계 spec/plan·핸드오프·감사 산출물·회고 기록이 전부 여기 산다 (2026-07-10 `docs/`→`doc/` 통합).

## Purpose (owns / configures)

- 코드가 아닌 모든 지식 소유: API 레퍼런스(정본), 설계 결정(spec/plan), 세션 인수인계(handoffs), AI-readiness 감사 계기판(report), 번복 회고 데이터(번복기록).
- 결정 스토어의 물리적 거처 — [adr/README.md](adr/README.md)가 포인터 정본.

## Quick commands

```bash
# 문서 경로 참조 감사 (CI 게이트와 동일 로직 — 문서 수정 후 실행)
python3 scripts/audit_docs.py --scope doc/superpowers/plans doc/superpowers/specs

# AI-readiness 재채점 (계측기 v2 — 원본 skill score.py 사용 금지)
python3 doc/report/score_cpp.py . --json /tmp/rescore.json

# 번복 통계 (트리거별 빈도 / 평균 왕복 턴)
python3 scripts/rollback_stats.py
```

## Key files

- [EngineAPI.md](EngineAPI.md) — SJH 엔진 코어 API 레퍼런스 (정본)
- [adr/README.md](adr/README.md) — 결정 스토어 포인터 (LOCKED D# 관행은 superpowers/specs/ 에)
- [report/ai-readiness-map.html](report/ai-readiness-map.html) — 감사 대시보드 (+ score.json, 계측기 score_cpp.py)
- [번복기록/rollback-log.jsonl](번복기록/rollback-log.jsonl) — 설계 번복 이벤트 로그 (G 계측 원시 데이터)
- [CLAUDE-extended.md](CLAUDE-extended.md) — 루트 CLAUDE.md 압축 전 원문 보존본 (2026-07-10)

## Gotchas

- 주의: `handoffs/` 는 문서 감사(CI scope) **영구 제외** — 핸드오프가 의도적 예시 broken 경로(스모크용)를 포함. Why: green 화 자체가 부적절한 디렉토리.
- 주의: `superpowers/specs|plans/` 의 날짜 문서는 archival 규약 — 「코드 경로는 작성 당시 기준」 헤더 + `<세그먼트>` placeholder. Why: 역사 문서의 경로를 현행화하면 결정 맥락이 왜곡됨. 본문(LOCKED D#)은 절대 불변.
- 주의: `report/score_cpp.py` 는 **계측기 v2 고정** — cartography skill 의 원본 score.py 로 재채점하면 비교 불가 (C++ 미인식). Why: v2 는 이 레포 적응 패치(C++ 확장자·doc/adr·mermaid arch 스캔) 포함.
- 주의: `html/` 은 Doxygen 빌드 산출물 (`--target doxygen` 후 생성) — 커밋·감사 대상 아님.

## Common modification patterns

- 새 설계 spec/plan 추가: `superpowers/specs|plans/YYYY-MM-DD-<주제>.md` 명명 + 결정에 `[제안됨]`→`[검증됨]` 상태 태그 + 산출 전 정본 D# 정합표.
- 새 핸드오프 작성: `handoffs/YYYY-MM-DD/` 아래 + lossless-handoff 규율 (수신 게이트 절 포함).
- 번복 발생 시: `번복기록/rollback-log.jsonl` 에 1줄 추가 (에이전트 초안 → 사용자 확정).

## Cross-module deps

- 의존: `scripts/`(audit_docs·rollback_stats 실행기), 루트 [ARCHITECTURE.md](../ARCHITECTURE.md)(모듈 지도 정본과 상호 참조).
- 피의존: `.claude/CLAUDE.md`(Key files 가 이곳을 라우팅), `.github/workflows/docs-validation.yml`(superpowers 하위를 CI 게이트로 감사).

## See also

- [../ARCHITECTURE.md](../ARCHITECTURE.md) · [../.claude/CLAUDE.md](../.claude/CLAUDE.md) · [testplan/](testplan/) · [번복기록/](번복기록/)
