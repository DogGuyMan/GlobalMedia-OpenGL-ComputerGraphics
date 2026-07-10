# doc/adr/ — 결정 스토어 포인터

이 디렉토리는 새로운 ADR 체계가 아니다. 이 레포의 ADR(Architecture Decision Record) 대체물이 **이미 어디 있는지**를 알려주는 얇은 포인터 문서다.

## 정본 — doc/superpowers/specs/

이 레포의 확정 설계 결정은 `doc/superpowers/specs/` 에 쌓인다 (58개 파일, 2026-07-10 실측). 파일명은 `YYYY-MM-DD-주제.md` 형식이고(예: `2026-05-24-topdown-shooter-design.md`), 본문 안에서 `D-1`/`D-2`... 형식의 확정 결정 목록 + `[확정됨]`/`LOCKED` 표기 관행을 따른다. 새 챕터/서브시스템을 설계할 때도 같은 패턴으로 spec 파일을 추가하는 것이 이 레포의 컨벤션이다.

**신규 ADR 번호 체계나 새 디렉토리 구조를 여기서 발명하지 마라.** 이 README 는 기존 `doc/superpowers/specs/` 관행을 "이게 이 레포의 ADR 이다"라고 이름 붙여 드러내는 역할만 한다.

## 보조 자료

- 세션 간 인수인계 = `doc/handoffs/<날짜>/` (해당 세션의 자기완결 재개 문서)
- 초압축 요약 = 사용자 홈의 `~/.claude/projects/.../memory/MEMORY.md` — **이 레포 안에는 없다.** 프로젝트 로컬 `MEMORY.md` 는 존재하지 않으므로 착각하지 말 것.

## 자동채점 주의

`doc/report/score_cpp.py` 의 tribal-store 자동 감지 로직은 `docs/adr` · `docs/decisions` · `repo/adr` 및 `MEMORY.md` · `.claude/memory*` 경로만 스캔한다. 이 레포의 실제 컨벤션인 `doc/adr`(단수 `doc`)는 그 목록에 없어 자동 감지되지 않는다. 이 구조 결정(`doc/` 단수 유지)은 사용자 승인 사안이라 이 문서 작성 시점에 경로를 바꾸지 않았다 — 담담한 사실 기록일 뿐, 시정 조치는 아니다.

## See also

- [../superpowers/specs/](../superpowers/specs/) — 정본 설계 결정 스토어
- [../handoffs/](../handoffs/) — 세션 인수인계
- [../../.claude/architecture.md](../../.claude/architecture.md)
