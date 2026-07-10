---
name: architecture-design-workflow
description: Apply when undertaking a multi-step architecture or refactor effort that deserves a real design process — not a one-line fix. Use whenever the user wants to redesign a subsystem, migrate an architecture (monolith→modular, OOP→ECS/DOD), benchmark reference implementations before adopting a pattern, incrementally migrate a legacy module, or asks to "design this properly", "write a spec/plan", or "think through the architecture first". Drives a 4-phase chain (brainstorm → evaluate → plan → dispatch), an explicit Decision Log with status tags (proposed/accepted/superseded) and a lock-conformance table against canonical decisions, and option-table-with-recommendation decisions. Trigger when a change is big enough that traceability of decisions matters, or when a new proposal must be checked against existing locked decisions.
---

# Architecture Design Workflow

A workflow for running multi-step design/refactoring work *traceably*. The core is **writing decisions down** — what was chosen, why, and instead of which alternative, all get referenced again later.

## When to use / when not to

**Fits**: multi-step redesign (monolith→modular/ECS), adopting a pattern after benchmarking best practices, incremental legacy migration, any work where decision traceability matters.
**Doesn't fit**: simple bug fixes, one-file one-off changes, exploratory prototypes (the cost of going all the way to a formal spec > the value). Just fix it in those cases.

## The 4-Phase chain

Real design work doesn't dump a big design all at once. It proceeds in phases, agreeing with the human along the way.

1. **Brainstorming** — Before writing code, explore intent/requirements/constraints in a 1:1 dialogue and reach agreement. When writing a spec/plan, split it into phases and **ask one question per section**.
2. **Evaluate (optional)** — Check candidate designs against design principles (SOLID / coupling / ownership, etc. — use `code-design-review-lenses`).
3. **Write the Plan** — Decompose the agreed design into file-by-file change Tasks (template below).
4. **Dispatch / implement** — Run independent Tasks sequentially or in parallel. Use a worktree when isolation is needed.

## Decision Framework — options + recommendation

At a design fork, **present the options as a table and recommend one**. Don't just lay out a survey.

```markdown
| Option | Shape | Output | Side effects | Pros | Cons |
|---|---|---|---|---|---|
| **A1** ... | | | | | |
| **A2** ... | | | | | |

**Recommend: A2** — reasons (1)..., (2)..., (3).... Alternative: A1 is also valid under constraint X.
```

Ask the user only the *real* decisions (ones you can't resolve from the code or sensible defaults). 2–4 options, put the recommendation first marked `(recommended)`, keep labels short.

## Decision Log — the heart of traceability

A design document always carries a decision-log table. This is the core of traceability for the whole session and what the next step references most often.

```markdown
| ID | Decision | Chosen option | Rationale | Status |
|----|----------|---------------|-----------|--------|
| D-1 | ... | A1 | ... | proposed |
| D-2 | ... | B2 | ... | accepted |
```

`Status` is one of `proposed / accepted / superseded(by D-#)`. A decision is recorded as `proposed` the moment it is made, and promoted to `accepted` only after user confirmation AND verification (build / visual check / golden test). Early recording is good — *statusless* early recording is the bug. In As-Built blocks the Korean tags `[제안됨]` → `[검증됨 <commit-hash>]` serve the same two states (a date suffix on `[제안됨]` is optional — the repo's existing bare-tag convention is the canonical form); an untagged block is read as `proposed`.

## Lock-conformance table

Before emitting any new proposal or plan in a session that has canonical decision documents, produce a conformance table FIRST:

| canonical D# | this proposal | verdict (consistent / unrelated / conflicting) |
|---|---|---|

Conflicting rows go into a separate section explicitly labeled as a reversal proposal (번복 제안), with the NEW evidence attached — never silently re-decide a locked decision.

## Document authority ordering

At session start, identify (or ask the user for) the authority ranking of the design documents (e.g. original plan > research > migration plan) and weigh every proposal against the highest-authority document.

## Document templates

### Spec (`specs/YYYY-MM-DD-<topic>-design.md`)
```
# <Topic> design
0. Higher-level context (if a decomposed sub-project, note its position)
1. Motivation (why now, problem diagnosis)
2. Core decisions (D-1, D-2 ...) — table
3. Before/After diagram (mermaid)
4. Changes — file-by-file
5. Design-principle consistency assessment
6. Verification method (build/test/visual regression)
7. Explicit out-of-scope
8. Seam the next step can build on without modification
9. Decision log (summary table)
10. Follow-up work
```

### Plan (`plans/YYYY-MM-DD-<topic>.md`)
```
# <Topic> Implementation Plan
## Summary
## Task N: <name>
  - changed files / acceptance criteria / verification
```

### Commit message
One-line summary + what/why. Referencing a decision ID in the body links it back to the spec.

## How to apply

When you get a big change request, don't start coding — ① first filter out whether it's an unfit case → ② narrow the intent with Phase 1 brainstorming (section-by-section questions) → ③ at each fork, option table + recommendation → ④ write it down as a spec/plan while keeping a decision log → ⑤ decompose into Tasks, then implement. Attaching the *why* to every decision is what this whole workflow is about. If canonical decision documents already exist for this session, run the lock-conformance table before ④ and give every logged decision a status tag from the moment it's written.
