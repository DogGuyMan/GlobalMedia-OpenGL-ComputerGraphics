# CLAUDE.md / `.claude/` 지침 문서 통합 실행 플랜

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

작성일: 2026-06-18 (갱신: 2026-06-18 — 범용 Skill 추출 반영)
목적:
1. `.claude/` 아래 중복/stale 지침 문서를 **카테고리별 단일 정본**으로 통합.
2. 프로젝트 독립 *가치관/방법론* 은 **전역 Skill** 로 분리 추출 → 원본 문서는 프로젝트 종속 잔여만 남기거나 삭제.

---

## 0. 핵심 전제 (조사로 확정)

- **`.claude/` 전체가 gitignore 대상** (`git check-ignore` 전부 매칭, `git ls-files .claude` = 0).
  → 문서 정리 + 복사된 Skill 모두 **로컬 전용**. git 히스토리/커밋/PR 부담 없음. 병렬 git 작업과 무관.
  → `doc/` 로 **이동(move)** 하는 파일만 추적 대상이 된다 (의도된 경우).
- **inbound 참조 수 (worktree 제외, 상대경로 링크)**:

  | 파일 | inbound | 처리 제약 |
  |---|---|---|
  | architecture.md | **13** | ⚠ 절대 이동/개명 금지 (doc/ 다수가 `../.claude/architecture.md` 참조) |
  | Graphics-Testing-Prompt.md | **5** | ⚠ 위치 유지 (commands 3 + CLAUDE.md + PostFX) |
  | architecture-design-agent.md | 3 | 삭제/이동 시 3곳 링크 갱신 |
  | Anti-Hallucination…md | 1 | CLAUDE.md 링크만 |
  | window_11_env_handoff.md | 1 | CLAUDE.md 링크만 |
  | EngineDesign.md | 1 | OptimizeRenderTarget.md 링크만 |
  | collision_detection_tutor_prompt.md | 1 | curriculum_overview.md만 |
  | **project-context-for-claude-agents.md** | **0** | 🔴 자유 삭제 가능 |
  | **curriculum_overview.md** | **0** | 자유 이동 가능 |

---

## 0.5. 완료된 작업 — 범용 Skill 추출 (2026-06-18) ✅

프로젝트 독립 가치관 10종을 `~/.claude/skills/` 에 작성 후, [<shell>/CopyGlobalSkills.sh](../shell/CopyGlobalSkills.sh) 로 프로젝트 `.claude/skills/` 에 복사 완료. (전역 + 프로젝트 양쪽 보유 — 단 프로젝트 `.claude/` 는 gitignore 라 로컬 사본)

**문서 → Skill 대응(supersession) 표** — 이 매핑이 이번 갱신의 핵심. "범용 내용"이 어디로 빠졌는지 = 원본에서 무엇을 지울 수 있는지.

| Skill | 추출 출처 문서 | 출처에 남는 *프로젝트 종속* 잔여 |
|---|---|---|
| **design-decision-discipline** ⭐ | architecture.md §3 (변동성≠다형성/팩토리/소유권/에러철학) | 구체 클래스(Context/Shader…) 사례, GL 자원 수명 |
| **modular-build-discipline** | architecture.md §1·§2·§4, CLAUDE.md 크로스플랫폼 | `SJH::<module>` 인벤토리, sb7/GL 헤더 순서, 실제 CMake 명령 |
| **code-design-review-lenses** | EngineDesign.md §3 (5렌즈) | 실제 SJH 모듈 평가 결과(artifact) |
| **benchmark-research-method** | EngineDesign.md §4, design-agent §4 | Unity/Godot/Cocos *실제* 비교 결과 |
| **architecture-design-workflow** | architecture-design-agent.md (전반) | (거의 전량 범용 — 잔여 적음) |
| **agent-orchestration-anti-gaming** | Graphics-Testing-Prompt.md §C·§D | §A GL규칙·§B 골든 FLIP 임계값·§E 비용 |
| **response-quality-calibration** | Anti-Hallucination…md (전반) | (전량 범용 — 잔여 없음) |
| **confidence-and-sourcing** | Anti-Hallucination + 지문(다수) | (범용) |
| **socratic-tutor** | collision_tutor + curriculum (방법론) | 충돌검출 *고유 커리큘럼* (위상정렬/SDF·SAT·GJK·EPA Phase) |
| **personal-naming-conventions** | CLAUDE.md Conventions + memory | 헤더가드 `__CHAPTER_N_ENTRY_H__` 등 프로젝트 표기 |

**결론**: 행동 규약 문서(Anti-Hallucination / design-agent)는 거의 **전량 Skill로 이관 → 삭제 후보**. Graphics-Testing / architecture / EngineDesign / tutor 는 **부분 이관 → 범용 절 제거 + 프로젝트 잔여 유지 + Skill 참조 링크**.

---

## 1. 통합 후 목표 구조 (Skill 반영 개정)

### 세 축 분리 원칙
- **① 프로젝트 사실(Facts)** → CLAUDE.md / architecture.md 에 응집.
- **② 범용 가치관/방법론** → **전역 Skill** (`.claude/skills/`, `~/.claude/skills/`). 문서에 중복 서술하지 않는다.
- **③ 도메인 학습 자료** → `doc/` 로 분리.

### `.claude/` 최종 레이아웃 (목표)

```
.claude/
├── CLAUDE.md                  # [정본 인덱스] 카테고리 요약 + 상세 링크 + Skill 포인터
├── architecture.md            # [정본] 엔진 아키텍처 (프로젝트 적용). 범용 설계철학은 Skill 참조
├── Graphics-Testing-Prompt.md # [정본] §A GL규칙·§B 골든 임계값만 (§C/§D 제거→Skill 참조). 위치 유지
├── skills/                    # 범용 Skill 10종 (복사본) + 기존(ddd/doxygen/handoff…)
├── agents/  commands/  hooks/ # (불변)
└── 2026-06-18-claude-md-consolidation-plan.md  # (본 문서)

# 삭제 (Skill로 전량 이관):
- Anti-HallucinationBehavioralCalibrationSystem.md  → 🔴 삭제 (response-quality-calibration + confidence-and-sourcing)
- architecture-design-agent.md                      → 🔴 삭제 (architecture-design-workflow)
- project-context-for-claude-agents.md              → 🔴 삭제 (stale, 0 inbound)

# 제자리 보존 + 역사 배너 (결정 2026-06-18 — ⚠ src/engine/ 폐기 구조 평가본임이 드러남):
- EngineDesign.md  → 흡수/삭제 안 함. 상단 🛑 역사 배너만 추가 (경로 dead 명시). architecture.md 오염 방지. inbound 1(OptimizeRenderTarget) 그대로 유지

# 폐기 (결정 2026-06-18 — 방법론은 socratic-tutor Skill로 충분):
- curriculum_overview.md + collision_detection_tutor_prompt.md  → 🔴 삭제 (이동 아님)

# 이동:
- window_11_env_handoff.md  → doc/handoff/ (휘발성 인수인계)
```

---

## 2. 카테고리 → 정본 매핑 (Skill 열 추가)

| # | 카테고리 | 프로젝트 정본 | 범용 Skill |
|---|---|---|---|
| 1 | 빌드 & 개발환경 | CLAUDE.md §Build | modular-build-discipline |
| 2 | 엔진 아키텍처 | architecture.md | (없음 — 프로젝트 종속) |
| 3 | 클래스/설계 컨벤션 | architecture.md §3 (사례) | **design-decision-discipline** |
| 4 | Client 컨텐츠 개발 | CLAUDE.md §자원보유 | (없음) |
| 5 | 테스트 & 품질 | CLAUDE.md §테스트 + Graphics §B | agent-orchestration-anti-gaming |
| 6 | 명명 규칙 | CLAUDE.md §Conventions (1줄+링크) | personal-naming-conventions |
| 7 | 코드 리뷰 방법 | — | code-design-review-lenses |
| 8 | 설계 워크플로우 | — | architecture-design-workflow |
| 9 | 벤치마크 리서치 | — | benchmark-research-method |
| 10 | 응답 품질 | — | response-quality-calibration + confidence-and-sourcing |
| 11 | 학습 튜터링 | 🔴 폐기 (문서 삭제) | socratic-tutor |

---

## 3. 중복 제거 맵 (file-by-file)

### M1 — 크로스플랫폼 규칙 (3중 → CLAUDE.md 1 + Skill)
- 정본: CLAUDE.md §크로스 플랫폼 코딩 규칙 (구체 항목 유지). *왜* 는 modular-build-discipline Skill.
- project-context…md §핵심 규칙 → 삭제(파일째). window_11…md 의 규칙 절 → 삭제(이동 시 본문 제거).

### M2 — 빌드 명령/env (정본 1 + stale 2)
- 정본: CLAUDE.md §Build Commands. project-context…md 의 mingw/Wine/Superbuild = 🔴 현행과 모순 → 삭제. window_11…md MSVC 부분 = 이미 반영, 잔여는 handoff로.

### M3 — 모듈 인벤토리 (숫자 불일치)
- 정본: CLAUDE.md §Src 모듈 레이아웃 (17모듈) + architecture.md §5. EngineDesign §1.1(11모듈) stale → 삭제.

### M4 — diagnostics (2중)
- 정본: architecture.md §6. CLAUDE.md §Diagnostics 세부는 요약 + 링크.

### M5 — 팩토리/소유권/자원보유 (3중)
- *원칙* 은 design-decision-discipline Skill. architecture.md 는 GL 자원 수명 *사례* 유지. CLAUDE.md §자원 보유 컨벤션은 Client 요약 유지.

### M6 *(신규)* — 문서 ↔ Skill 중복 제거
- 삭제 대상 문서(Anti-Hallucination/design-agent/project-context)는 Skill이 대체 → 본문 보존 불필요.
- 부분 문서(Graphics §C·§D, architecture §3·§4 산문, EngineDesign §3·§4, tutor 행동규칙)는 **범용 산문을 1~2줄 Skill 참조로 축약**하고 프로젝트 사례만 남긴다.
- CLAUDE.md §Reference 의 Anti-Hallucination / Graphics-Testing 항목 설명을 *"→ Skill 로 이관됨"* 로 갱신.

---

## 4. 실행 순서 (Skill 반영 개정)

> 각 단계 로컬 편집, 단계마다 diff 확인. Skill은 이미 추출/복사 완료(§0.5).

1. **[안전망] 스냅샷**: `.claude/*.md` → `.claude/_backup_2026-06-18/` 복사.
2. **EngineDesign.md 흡수 (D1=보존)**: evergreen 의존 그래프 + 실제 SJH 평가결과 + **§5 로드맵(P0/P1/P2)** 을 architecture.md 부록으로 *보존 흡수*. (5렌즈/벤치마크 *방법*은 이미 Skill — 중복 서술 금지, 단 실제 결과·로드맵은 프로젝트 자산이라 보존) → EngineDesign.md 삭제 + OptimizeRenderTarget.md 링크 갱신.
3. **Anti-Hallucination…md 삭제** + CLAUDE.md §Reference 항목을 `response-quality-calibration` / `confidence-and-sourcing` Skill 포인터로 교체.
4. **architecture-design-agent.md 삭제** + 참조 3곳(EngineDesign[삭제됨]/work_history/OptimizeRenderTarget)을 `architecture-design-workflow` Skill 로 갱신.
5. **Graphics-Testing-Prompt.md 슬림화**: §C 5에이전트 + §D 안티게이밍 본문 제거 → `agent-orchestration-anti-gaming` Skill 1줄 참조. §A·§B·§E 유지 (위치/파일명 불변 — 5 inbound 보호).
6. **architecture.md 슬림화 (보수적 — 결정 2026-06-18)**: §3·§4 의 *범용 산문* 앞에 "범용 원칙은 design-decision-discipline / modular-build-discipline Skill 참조" **헤더 한 줄만 추가**. 산문 본문은 *유지* (들어내지 않음). ⚠ 13 inbound — 섹션 앵커/파일명 변경 금지.
7. **project-context-for-claude-agents.md 하드 삭제** (0 inbound, stale).
8. **window_11_env_handoff.md 이동** → `<doc>/handoff/2026-06-18-win11-arm64-msvc-handoff.md`. 중복 규칙 절 제거. CLAUDE.md 링크 갱신.
9. **충돌검출 2종 하드 삭제 (결정 2026-06-18 — 폐기)**: curriculum_overview.md + collision_detection_tutor_prompt.md 삭제. 방법론은 socratic-tutor Skill 이 대체. (inbound: curriculum→tutor 내부 링크뿐, 외부 0 → 안전)
10. **CLAUDE.md 정리**: §Conventions 명명 규칙을 1줄 + personal-naming-conventions Skill 링크로 축약. §Reference 를 카테고리 인덱스 + Skill 포인터 구조로 재배치.
11. **링크 + Skill 정합성 전수 검증** (§5).

---

## 5. 검증 체크리스트

- [ ] 깨진 상대링크 0: `grep -rn '\](\.\./\?\.claude/' .claude doc | grep -E 'EngineDesign|project-context|Anti-Hallucination|architecture-design-agent|window_11|curriculum'` 결과 모두 갱신/제거됨
- [ ] architecture.md inbound 13곳 무손상 (위치/파일명/주요 앵커 불변)
- [ ] Graphics-Testing-Prompt.md inbound 5곳 무손상 (위치 불변, §C/§D 제거해도 §A/§B 앵커 유지)
- [ ] 삭제된 문서를 CLAUDE.md/commands/agents 가 더 이상 참조 안 함
- [ ] 범용 내용이 문서와 Skill 양쪽에 중복 서술되지 않음 (문서는 Skill 참조만)
- [ ] 크로스플랫폼 규칙 구체 항목이 정확히 1곳(CLAUDE.md)에만 존재
- [ ] mingw/Wine/Superbuild 모순 서술 제거됨
- [ ] Skill 10종이 `.claude/skills/` 에 존재 (CopyGlobalSkills.sh 재실행으로 동기화 가능)

---

## 6. 결정 현황 (2026-06-18 확정 반영)

### ✅ 확정
- **Q1/원문 처리** → **하드 삭제**: Anti-Hallucination…md / architecture-design-agent.md / project-context…md 전량 삭제 (아카이브 없음). Skill이 대체하되 학술 인용·4논문 근거·비용 수치는 손실 감수.
- **D4/git 추적** → **의도적 로컬 전용 유지**: `.claude/` gitignore 그대로. CLAUDE.md/skills 추적 승격·`.gitignore` 예외 *안 함*. (시스템 컨텍스트의 "checked into" 표기는 무시 — 실측 gitignore 가 정답)
- **<D5>/architecture.md** → **보수적**: 참조 헤더 한 줄만 추가, 산문 유지. 분리 파일 안 함.
- **D3/충돌검출** → **폐기**: curriculum_overview + collision_tutor 하드 삭제 (이동 아님).

- **D1/EngineDesign §5 로드맵** → **보존**: architecture.md 부록으로 흡수 (실제 평가결과 + 로드맵은 프로젝트 자산).
- **D6/Skill SSOT** → **전역 SSOT + 단방향 복사**: `~/.claude/skills/` 가 정본, 프로젝트 사본은 CopyGlobalSkills.sh 로 받기만. 프로젝트 사본 직접 편집 금지(다음 복사 시 덮어쓰임). Skill 수정은 전역에서 → 재복사.

- **D2/window_11 핸드오프** → **이동 안 함, `.claude/` 로컬 유지** (D4 로컬전용 우선 + 개인 프로필/VM 정보 git 노출 회피). 내용 미편집(프리즈된 핸드오프).

### ⬜ 남은 결정
- **D7** description 트리거 최적화(skill-creator run_loop) 실행 여부 — 10종 오발/미발 정확도 검증 (선택).

---

## 7. 실행 결과 (2026-06-18 완료)

- 스냅샷 `.claude/_backup_2026-06-18/` (11파일) 생성.
- **하드 삭제 3**: Anti-Hallucination / project-context / 충돌검출 2종(curriculum_overview·collision_tutor) → 실제로는 4파일.
- **슬림화 2**: architecture-design-agent.md(범용→Skill, 프로젝트 §2·§7~§14 보존), Graphics-Testing-Prompt.md(§C/§D 원칙→Skill, render-* 매트릭스·hooks 강제·§A/§B/§E 유지).
- **배너 1**: EngineDesign.md (🛑 역사 스냅샷 — src/engine 폐기 경로). 흡수/삭제 안 함.
- **참조 헤더 추가**: architecture.md §3·§4, CLAUDE.md(크로스플랫폼·명명·Reference 전역 Skill 인덱스).
- **검증**: 삭제 문서로의 잔존 링크 0 ✅.
- ⚠ **사고+복구**: 병렬 git 작업이 `.claude/skills/` 의 untracked 사본 10종을 wipe → `CopyGlobalSkills.sh` 재실행으로 복원. **D6(전역 SSOT) 설계가 정확히 동작함을 실증** — 프로젝트 사본 분실은 재복사로 무손실 복구.
