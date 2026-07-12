---
name: spec-review-dashboard
description: Use when a Plan/Spec draft is ready and the user needs to make an acceptance call on it — spec review requests, "spec 검토 대시보드", "플랜 HTML로 보여줘", or as the natural companion artifact whenever architecture-design-workflow finishes a Decision Log. Also trigger whenever a design document needs to become a scannable HTML so a human can approve/hold/reverse (승인/보류/번복) it instead of reading prose end-to-end. Produces one self-contained HTML file next to the spec — never a narrative summary.
---

# Spec Review Dashboard

Plan/Spec을 **사용자가 수용 판정을 내리기 좋은 계기판**으로 압축한다. 산문 대신 표·배지·다이어그램. 판정 자체는 항상 사용자 몫.

## When to use

- "spec 검토 대시보드 만들어줘", "플랜 HTML로 보여줘"
- architecture-design-workflow가 Decision Log를 산출한 직후 (동반 산출)
- 사용자가 승인/보류/번복 판정을 내려야 하는 국면

## What to produce

단일 파일 HTML **하나**. spec 옆에 `<spec-name>-review.html` 로 저장 (예: `2026-07-12-foo-design.md` → `2026-07-12-foo-design-review.html`).

블록 구성 — **이 순서 그대로**:

| 블록 | 내용 |
|---|---|
| (a) 헤더 | spec명 · 날짜 · 브랜치 · 전체 상태 배지(proposed/accepted) |
| (b) 결정 요약 표 | D# · 결정 한 줄 · 확신도 앵커(🔵92/🟡65/💭40) · Status · 옵션 수 |
| (c) 옵션 비교표 | 핵심 결정의 옵션별 트레이드오프 열 + 추천 행 하이라이트 |
| (d) Before/After 패널 | 두 SVG 나란히 인라인 임베드 + added/removed/moved 범례 |
| (e) Lock 정합표 | 정본 D# 대조 — 일치 green · 무관 gray · 상충 red(+번복 제안 링크) |
| (f) 신규 구조물 신고 | 있을 때만 — 4항(종류/구현자수/소비자수/삭제테스트) + grep 결과 |
| (g) 수용 판정 푸터 | 승인/보류/번복 + 한 줄 사유 — **사용자가 채우는 자리** |

## Workflow

1. spec의 Decision Log 표에서 D# · Status · Confidence(있으면)를 파싱한다.
2. 아키텍처 변경 spec이면 Before/After `.svg`(예: `doc/diagrams/*.svg`)를 찾아 `<svg>` 태그 내용을 그대로 인라인 임베드한다(`<img src>` 아님 — 오프라인 자기완결).
3. `assets/template.html` 을 **복사**해서 `{{PLACEHOLDER}}` 를 치환한다 — 처음부터 쓰지 않는다.
4. spec 파일에 `review.html` 로의 상대 링크 한 줄을 추가한다(선택, 사용자 승인 후).

## Style rules (non-negotiable)

- **산문 문단 금지** — 설명은 spec 본문에 이미 있다. 대시보드는 계기판이지 축약본이 아니다.
- **전체 1~2 스크린** — 스크롤 몇 번으로 끝나야 판정 가능.
- **template 복사 후 치환** — 절대 처음부터 쓰지 않는다.
- **인라인 SVG + CSS만** — 차트/JS 라이브러리 금지.
- **라이트 단일 테마** — 다크모드 만들지 않는다.
- **이모지는 확신도 앵커 3종(🔵🟡💭)만** — 장식 이모지 금지.
- **외부 리소스 0건** — 폰트/CDN 로드 금지, 시스템 폰트 스택만(오프라인에서 열려야 함).

## Common pitfalls

- **산문 유입** — "왜 이렇게 결정했는지" 설명을 대시보드에 다시 씀. spec 본문 링크로 대체.
- **다이어그램 없이 생성** — 아키텍처 변경 spec인데 Before/After가 없으면 **생성을 거부하고 사용자에게 보고**(다이어그램 먼저 만들라고).
- **판정 푸터를 AI가 채움** — 절대 금지. 체크박스/사유 칸은 항상 비워서 산출한다.
- **확신도 앵커 색 혼동** — 🔵=green(실측) · 🟡=amber(개연) · 💭=red(판단/미검증). 숫자와 색이 둘 다 있어야 한다. 등급·앵커 경계의 정본 = `confidence-and-sourcing` §1.5 (🔵≥90 / 🟡60–89 / 💭<60) — 여기서 재정의하지 말 것.
- **Lock 정합표 생략** — 정본 spec이 있는데 대조표 없이 산출 금지(architecture-design-workflow의 lock-conformance table을 그대로 옮겨온다).

## Files

- `assets/template.html` — 복사 후 채울 원본 대시보드 (약 250줄, `{{PLACEHOLDER}}` 치환점 + 블록별 치환 지침 주석)
