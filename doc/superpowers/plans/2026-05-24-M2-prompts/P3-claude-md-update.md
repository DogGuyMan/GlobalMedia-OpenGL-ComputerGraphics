<!-- # M2 P3 Agent Prompt — `.claude/CLAUDE.md` Active Target Management 갱신

> **사용법**: 이 파일의 `## Prompt` 섹션 아래 *전체 내용* 을 복사하여 새 Claude Code 세션 (또는 Agent tool 의 `prompt` 인자) 에 그대로 붙여넣으세요. self-contained 라 다른 컨텍스트 불요.
>
> **작업 디렉토리**: `/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics`
> **브랜치**: `game/module/sprite`
> **예상 시간**: 3~5분 (1 파일 edit + commit)

--- -->

## Prompt

You are implementing **M2 P3** of the topdown-shooter milestone — `.claude/CLAUDE.md` 의 "Active Target Management" 섹션을 M1 완료 상태에 맞춰 갱신.

`.claude/CLAUDE.md` 의 "Active Target Management — CRITICAL" 섹션 (보통 line 60-90 부근) 의 *활성/비활성 데모 목록* 을 현 실제 상태와 일치하도록 갱신.

### 현 실제 상태 (`apps/CMakeLists.txt` line 1-6)

```cmake
add_subdirectory(_MyApp_)        # ← M1 완료 후 활성화 (Director + SceneRenderer + 빌보드)
add_subdirectory(migrate_demo)
add_subdirectory(box2d_demo)
add_subdirectory(effekseer_demo)
add_subdirectory(tweeny_demo)
# add_subdirectory(audio_demo)
```

### Step 1: `.claude/CLAUDE.md` 의 Active Target Management 섹션 정정

Edit tool 사용 — 정확한 old_string / new_string 매칭. `_MyApp_` 줄을 *활성* 의 맨 위로 이동 + M1 완료 설명 추가:

기존 (Edit old_string):
```
- **활성 (`apps/CMakeLists.txt` 주석 해제됨):**
  - `migrate_demo` — 진행 중 마이그레이션 워크스페이스 (working tree 다수 변경)
  - `box2d_demo` — Box2D v2.4.1 물리 데모. 내부 `common` STATIC + `demo1`/`demo2`/`demo3` 서브타겟 3종
  - `effekseer_demo` — Effekseer 1.7.3.0 파티클 데모. 현재 `demo1` 1종
  - `tweeny_demo` — Tweeny 헤더 온리 트위닝 데모. `step(int32_t ms)` vs `step(float ratio)` 오버로드 함정 주의
- **임시 비활성 (주석 처리):**
  - `_MyApp_` — 자유 작업 / `game_deps` 링크 점검용 임시
  - `audio_demo` — FMOD Studio + ImGui 파라미터 데모. ImGui 의존으로 임시 비활성
```

다음으로 교체 (Edit new_string):
```
- **활성 (`apps/CMakeLists.txt` 주석 해제됨):**
  - `_MyApp_` — 탑다운 슈터 게임 (M1 완료 2026-05-24). SJH::sprite atlas + Director + SceneRenderer + Material 패턴 + Player WASD + Camera follow (M2 진행 중). 정통 데모 — 다른 SJH::engine 사용 데모 작성 시 main.cpp 참조 우선순위
  - `migrate_demo` — 진행 중 마이그레이션 워크스페이스 (working tree 다수 변경)
  - `box2d_demo` — Box2D v2.4.1 물리 데모. 내부 `common` STATIC + `demo1`/`demo2`/`demo3` 서브타겟 3종
  - `effekseer_demo` — Effekseer 1.7.3.0 파티클 데모. 현재 `demo1` 1종
  - `tweeny_demo` — Tweeny 헤더 온리 트위닝 데모. `step(int32_t ms)` vs `step(float ratio)` 오버로드 함정 주의
- **임시 비활성 (주석 처리):**
  - `audio_demo` — FMOD Studio + ImGui 파라미터 데모. ImGui 의존으로 임시 비활성
```

### Step 2: Single commit

```bash
git add .claude/CLAUDE.md
git commit -m "docs(CLAUDE): _MyApp_ 활성 데모로 승격 (M1 완료)

- M1 commit chain (a023b27..16f1426) 으로 _MyApp_ 가 정통 데모로 등극
- SJH::sprite + Director + SceneRenderer + Material 패턴 정착
- M2 진행 중 (Player WASD + Camera follow)
- 활성 목록의 최우선 — SJH::engine 사용 데모 작성 시 main.cpp 참조 권장

M2 P3."
```

## Project Conventions

- 한국어 톤 유지
- `.claude/CLAUDE.md` 가 cwd 기반 자동 로드됨

## DO NOT touch

- `src/`, `apps/`, `test/`, `doc/` — 별도 subagent (P1, P2) 가 담당하거나 변경 불필요
- 본 task 는 `.claude/CLAUDE.md` *단일 파일* 만 수정

## Self-Review

- `.claude/CLAUDE.md` 한 파일만 수정?
- _MyApp_ 가 *활성* 목록의 첫 번째 (최근 정통 데모이자 가장 활발)?
- audio_demo 는 *비활성* 그대로 (단독으로)?
- 한국어 톤 유지?
- 단일 commit, 메시지 형식 정확?

## Report

Status: DONE
+ File changed (1 file)
+ Git commit SHA

Work from: `/Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics`
