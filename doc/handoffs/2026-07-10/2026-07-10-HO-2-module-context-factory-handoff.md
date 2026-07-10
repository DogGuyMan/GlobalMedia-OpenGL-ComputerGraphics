# HO-2 — 모듈 CLAUDE.md 6장 + 결정 스토어 포인터 (핸드오프, 맥락0 자기완결)

> AI-Readiness 70% 플랜의 콘텐츠 패키지 3건 중 1번. 정본 플랜 = [`doc/superpowers/plans/2026-07-10-ai-readiness-70pct-plan.md`](../../superpowers/plans/2026-07-10-ai-readiness-70pct-plan.md) §3 HO-2 행 + §4 "A+B+C" 절. 루브릭 = [`.claude/skills/ai-readiness-cartography/references/scoring-rubric.md`](../../../.claude/skills/ai-readiness-cartography/references/scoring-rubric.md) A/B/C 절. 이 문서만 읽고 실행 가능하도록 아래 ③에 실행 수준 사양을 전부 풀어놓았다 — 플랜/루브릭을 다시 열 필요 없음.

## ① 배경

이 저장소(`GlobalMedia-OpenGL-ComputerGraphics`, C++17/OpenGL 엔진+게임 데모)는 2026-07-10 감사(`doc/report/ai-readiness-score.json`)에서 **18/100 (AI-Hostile)** 등급을 받았다. 카테고리 A(내비게이션)는 **0/15** — 스코어러가 핵심 module 6개(`src`, `apps/_MyApp_`, `test`, `cmake`, `scripts`, `vcpkg-overlay-ports`) 전부에 `CLAUDE.md`가 없다고 판정했다(evidence: `"covered_modules": 0"`). HO-2 는 이 6개 module 루트에 `CLAUDE.md`를 신설해 A 를 15로, 동시에 템플릿에 내장된 요소로 B(9→14 목표 일부)·C(0→14 목표 일부)를 함께 끌어올린다.

**계측기 고정**: 재채점은 반드시 `python3 doc/report/score_cpp.py . --json <출력경로>` (예: `/tmp/rescan.json`)로 실행한다. `doc/report/` 밖의 원본 `score.py`(cartography skill 범용판)는 **사용 금지** — C++/셰이더 확장자(`.h .hpp .cpp .slang .vert .frag` 등)를 인식 못해 이 저장소를 항상 과소평가하고, 이전 결과와 비교도 불가능해진다.

**⚠ 스코어러 핵심 사실 (직접 코드 확인, `doc/report/score_cpp.py:160-165`)**: A 카테고리의 "module에 context 있음" 판정은 정확히 `<module_dir>/CLAUDE.md` (module 디렉토리 바로 밑, 파일명 대소문자까지 정확히 `CLAUDE.md`) 파일 존재만 본다. `.claude/` 하위나 다른 이름은 인식되지 않는다. 즉 아래 6개 파일은 **정확히 이 경로**에 있어야 한다 — `src/CLAUDE.md`, `apps/_MyApp_/CLAUDE.md`, `test/CLAUDE.md`, `cmake/CLAUDE.md`, `scripts/CLAUDE.md`, `vcpkg-overlay-ports/CLAUDE.md`.

## ② 소유 파일 목록

이 핸드오프가 **생성**할 파일은 아래 7개뿐. 다른 경로는 절대 건드리지 않는다(다른 HO 소유 — 병렬 실행 중일 수 있음).

- `src/CLAUDE.md` (신규)
- `apps/_MyApp_/CLAUDE.md` (신규)
- `test/CLAUDE.md` (신규)
- `cmake/CLAUDE.md` (신규)
- `scripts/CLAUDE.md` (신규)
- `vcpkg-overlay-ports/CLAUDE.md` (신규)
- `doc/adr/README.md` (신규 디렉토리 + 파일)

읽기 전용 참조(수정 금지, 인용/링크만): `.claude/architecture.md`, `doc/EngineAPI.md`, `doc/superpowers/specs/*.md`(58건), `src/CMakeLists.txt`, 각 `src/<module>/CMakeLists.txt`. **`.claude/CLAUDE.md`(루트) 는 HO-6 소유 — 읽기만, 수정 금지.**

## ③ 작업 사양

### 표준 템플릿 (오케스트레이터 확정본 — 7개 파일 전부에 그대로 적용)

```markdown
# <모듈 경로> — CLAUDE.md
<1줄: 이 모듈이 무엇인가>

## Purpose (owns / configures)
- <이 모듈이 소유/구성하는 것 1~3 불릿>

## Quick commands
```bash
# <사용 시점 주석> — copy-paste 가능해야 함
<이 모듈 대상 빌드/테스트/실행 명령>
```

## Key files
- <실경로> — <역할>  (3~5개, 전부 실존 검증)

## Gotchas
- 주의: <실패를 유발하는 hidden rule — Why: 형식으로 이유 병기>

## Cross-module deps
- 의존: <PUBLIC/PRIVATE 구분> / 피의존: <이 모듈을 바꾸면 흔들리는 곳>

## See also
- [ARCHITECTURE.md](<상대경로>) · [결정 스토어](<doc/adr/README.md 상대경로>) · <관련 spec 상대링크>
```

각 장 **30~80줄**(스코어러 B1은 10~80줄 구간을 만점 대역으로 계산 — `score_cpp.py:256`, 100줄 초과 시 findings 에 감점 사유로 잡힘). 한국어. 섹션명은 영문 그대로 유지(C 카테고리 자동채점이 `## Purpose`/`## Cross-module`/`Why:`/`Gotcha` 등 정규식 마커로 잡기 때문 — 임의 개명 금지).

### 파일별 실행 사양

- **`src/CLAUDE.md`**: `ls src/`로 하위 모듈을 직접 재확인할 것 — **18개** 디렉토리 존재(`buffer common diagnostics fsm input layout material object playable program render resource_registry scene shader sprite text texture timer`). ⚠ 루트 `.claude/CLAUDE.md`는 "17개 모듈"이라 쓰고 있으나 이는 **stale** — `texture`가 2026-06-11 리팩토링으로 `resource_registry`에서 하위추출된 18번째 모듈(`src/texture/CMakeLists.txt` 주석 "18번째 모듈" 참조). 17이 아니라 18로 쓸 것. Key files 는 모듈 1행 표(17→18개, `texture` 행 추가)로 대체하고 **상세 설명은 `.claude/architecture.md`와 `doc/EngineAPI.md`로 링크 위임 — 중복 서술 금지** (기존 두 문서가 정본).
- **`apps/_MyApp_/CLAUDE.md`**: Quick commands 는 루트 `.claude/CLAUDE.md`의 Build Commands 절(cmake --preset ninja --target _MyApp_ 등)에서 이 앱 전용분만 발췌. Key files = `apps/_MyApp_/main.cpp`, `apps/_MyApp_/CMakeLists.txt`, `apps/_MyApp_/src/GameSystems.h`, `apps/_MyApp_/src/Stage/`(FSM), `apps/_MyApp_/src/Physics/`(Box2D). Gotchas 후보: ESC 키가 sb7 프레임워크 레벨에서 하드와이어드 종료(인게임 액션에 못 씀), 리소스 상대경로라 실행 디렉토리 `cd` 필수, `apps/CMakeLists.txt`의 주석 처리 컨벤션(한 번에 소수만 활성화).
- **`test/CLAUDE.md`**: `test/CMakeLists.txt` 실측 확인(`sjh_add_test` 매크로, `test/smoke`·`test/gpu`·`test/golden`·`test/golden_compare` 4구성, `ENABLE_TESTING` 기본 OFF). Quick commands = `cmake --preset ninja -DENABLE_TESTING=ON` + `ctest --test-dir build_ninja --output-on-failure`. See also 에 `doc/superpowers/specs/2026-06-27-test-expansion-design.md` 링크.
- **`cmake/CLAUDE.md`**: `ls cmake/`로 4개 파일 확인(`CXXStandard.cmake`, `Dependency.cmake`, `Doxygen.cmake`, `Slang.cmake` — 루트 `.claude/CLAUDE.md`는 3개라 쓰지만 `Slang.cmake` 신설로 4개, 이 역시 stale 발견이니 4로 정정).
- **`scripts/CLAUDE.md`**: `ls scripts/`로 실재 스크립트 재확인(현재 12개 .py — `edit_message.py`, `find_abstraction_seams.py`, `find_naming_violations.py`, `find_qualifiers.py`, `find_qualifiers_table.py`, `find_special_chars.py`, `leakloghandler.py`, `migrate_vmath_to_glm.py`, `rename_render_symbols.py`, `replace_special_chars.py`, `slang_compile.py`, `traversal_git.py`). `scripts/audit_docs.py`(HO-1)와 `scripts/rollback_stats.py`(HO-6)는 **이 핸드오프 실행 시점에 아직 없을 수 있다** — 있으면 Key files에 포함, 없으면 언급만 하고 존재를 단정하지 말 것("(계획됨 — HO-1/HO-6)" 표기).
- **`vcpkg-overlay-ports/CLAUDE.md`**: `ls vcpkg-overlay-ports/box2d/`로 `portfile.cmake`+`vcpkg.json` 확인. **⚠ 실측된 미해결 사실 — 이 오버레이가 실제로 vcpkg에 등록되는 배선 지점을 찾지 못했다**: 루트에 `vcpkg-configuration.json` 부재(`ls vcpkg-configuration.json` 실패 확인됨), `CMakePresets.json`에 `overlay` 문자열 없음, `shell/*.sh`·`*.bat`·`*.ps1`에 `VCPKG_OVERLAY_PORTS` 세팅 없음. 이 사실을 Gotchas에 그대로 적을 것 — "배선 경로 미확인(2026-07-10 실측), 로컬 env var 또는 다른 미탐색 경로일 가능성" 식으로. **존재하지 않는 배선을 지어내 쓰지 말 것.**

### `doc/adr/README.md` (결정 스토어 포인터 — 신규 체계 발명 금지)

내용 = "이 레포의 결정 스토어는 이미 존재한다"는 사실만 진술하는 얇은 포인터 문서(30~60줄):
- 정본 = `doc/superpowers/specs/`(58개 파일, 2026-07-10 실측 `ls doc/superpowers/specs | wc -l`) — 날짜-주제 파일명, 본문에 `D-1`/`D-2`... 형식 확정 결정 + "[확정됨]"/"LOCKED" 표기 관행(예: 이 플랜 문서 자체의 §5 "D-1. ... [확정됨 2026-07-10]"). 이것이 이 레포의 **ADR 대체물**임을 명시.
- 보조 = 세션 간 인수인계는 `doc/handoffs/<날짜>/`, 초압축 요약은 사용자 홈 `~/.claude/projects/.../memory/MEMORY.md`(이 레포 안에는 없음 — 프로젝트 로컬 `MEMORY.md`는 **부재**, 착각 금지).
- **신규 ADR 번호 체계·디렉토리 구조를 새로 발명하지 말 것** — 기존 `doc/superpowers/specs/` 관행을 "이게 ADR이다"라고 이름 붙여 노출하는 것이 이 파일의 전부다.
- ⚠ **자동채점 주의사항 (`doc/report/score_cpp.py:327-332` 확인)**: C-Q5(tribal store 자동감지)는 `docs/adr`, `docs/decisions`, `repo/adr`(3경로) 및 `MEMORY.md`, `.claude/memory*` 만 스캔한다 — 이 저장소 컨벤션인 `doc/adr`(단수 `doc`)는 **이 목록에 없어 자동으로는 감지되지 않는다**. 이 파일은 플랜 지시대로 `doc/adr/README.md`에 생성하되(경로 변경 권한 없음 — 구조 결정은 사용자 승인 사안), 이 불일치를 오케스트레이터에게 **보고**할 것(`score_cpp.py`는 다른 HO 소유이므로 이 핸드오프가 직접 고치지 않는다).

## ④ 수신 게이트

착수 전 아래를 직접 실행해 실측하고, 아래 "작성 시점 상태"와 대조. 불일치 시 진행 전 보고.

```bash
git log --oneline -5
git status --short | wc -l
ls scripts/audit_docs.py 2>&1   # HO-1 산출물 — 없으면 "HO-1 미완료"로 오케스트레이터에 보고만 하고 계속 진행(이 HO는 audit_docs.py 없이도 작업 가능)
ls src/CLAUDE.md apps/_MyApp_/CLAUDE.md test/CLAUDE.md cmake/CLAUDE.md scripts/CLAUDE.md vcpkg-overlay-ports/CLAUDE.md doc/adr/README.md 2>&1  # 전부 "No such file" 이어야 함 — 하나라도 존재하면 다른 세션이 먼저 만든 것, 진행 전 보고
```

**작성 시점 상태 (2026-07-10 실측)**
- 브랜치: `refactor/pcb-to-worldscene`
- HEAD: `5590cc9` (`5590cc902d4774964c09d56bfa7ca5df81ced44d`, "[chore] : 주석 수정")
- `git status --short` 476줄(대부분 staged — `doc/` 재편성 + `.claude/` 신설 스캐폴딩), untracked 13건. 매우 dirty한 working tree — 사용자가 병렬 작업 중일 가능성 높음. 커밋은 반드시 파셜.
- 7개 산출물 전부 미존재 확인 완료. `doc/handoffs/2026-07-10/2026-07-10-HO-1-audit-docs-script-handoff.md`는 이미 존재(다른 핸드오프 — 이 HO 소유 아님, 참고만).

## ⑤ green 체크포인트

```bash
python3 doc/report/score_cpp.py . --json /tmp/rescan_ho2.json
python3 -c "import json; d=json.load(open('/tmp/rescan_ho2.json')); print('A =', d['categories']['A']['score'])"
```

**통과 기준**: `A ≥ 10.5`(반올림 정수 점수이므로 실질적으로 `A == 15` 도달을 목표로 한다 — coverage 6/6 = round(1.0×15) = 15). 추가로, `scripts/audit_docs.py`가 이미 존재한다면(HO-1 완료 후):
```bash
python3 scripts/audit_docs.py --scope src apps/_MyApp_ test cmake scripts vcpkg-overlay-ports doc/adr --json /tmp/audit_ho2.json
```
신규 7파일에서 발생하는 broken 참조가 0건이어야 한다(이 HO가 인용한 경로는 전부 위 ③에서 실존 검증됨 — 새로 지어낸 경로 쓰지 말 것).

## ⑥ 금지 사항

- ②의 7개 파일 외 어떤 파일도 수정하지 않는다. 특히 `.claude/CLAUDE.md`(HO-6 소유), `ARCHITECTURE.md`(HO-3 소유), `scripts/audit_docs.py`/`scripts/rollback_stats.py`(각 HO-1/HO-6 소유)는 **불가침**.
- 커밋은 `git commit src/CLAUDE.md apps/_MyApp_/CLAUDE.md ...` 처럼 **소유 경로만 파셜 커밋**. `git add -A`/`git commit -a` 금지.
- 자동 테스트를 임의로 추가하지 않는다(`no_auto_tests` 관례 — 사용자 명시 요청 시에만).
- 확정 기록에는 항상 근거를 남긴다 — 이 문서가 지시하지 않은 사실을 확정형으로 서술할 경우 `[제안됨]` 태그 없이 쓰지 않는다.
- 소유 파일 밖에서 발견한 문제(예: `.claude/CLAUDE.md`의 다른 stale 서술, `score_cpp.py`의 doc/adr 미인식)는 수정하지 말고 오케스트레이터에 보고만 한다.
