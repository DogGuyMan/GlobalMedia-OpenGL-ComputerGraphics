---
name: lossless-handoff
description: >-
  Use when delegating a unit of work to a separate Claude Code agent/session, or preserving
  conversation context so a fresh session can resume with zero loss. Produces either (A) a
  paste-ready, self-contained agent prompt for one bounded task, or (B) a lossless resume/context
  handoff document for a multi-step effort. Trigger this whenever the user mentions 핸드오프/handoff,
  "다른 (Claude) 에이전트에게 넘겨/맡겨", "프롬프트 만들어/추출", "무손실", "컨텍스트 저장/이주", "재개",
  "병렬로 빼줘/나눠", delegating to a parallel agent, preserving session context before a long task,
  or migrating work to another session — even if they never say the word "handoff". Especially
  important in multi-agent / parallel-branch setups where scope boundaries and conflict-avoidance matter.
---

# Lossless Handoff

## Why this exists

The receiving agent (or the fresh resume session) has **none of this conversation's context**. It
did not watch you investigate the code, weigh the options, or absorb the user's corrections. So a
handoff fails the moment it assumes shared memory — a dangling reference to "the approach we
discussed", a file path that drifted, an unstated guardrail. A good handoff is **self-sufficient**:
the receiver needs nothing but the handoff to do the work correctly and safely.

This skill captures two artifacts that recur whenever work crosses an agent/session boundary:

- **A — Self-contained agent prompt**: a paste-ready block another Claude Code agent runs to do **one
  bounded task**. Optimized for "copy → new session → go".
- **B — Resume / context handoff doc**: a tracked document that lets a **fresh session pick up a
  multi-step effort** (or that you write *before* a long task so nothing is lost if the session ends).

You often produce both: B preserves the whole effort; A delegates a slice of it.

## Step 0 — Discover the target project's conventions (do this first, every time)

A handoff is only safe if it encodes the rules the receiver must obey. These are **project-specific**
and you cannot guess them — find them, then bake them into the handoff. Spend a moment to establish:

- **Build / run / test commands** — the exact verification commands (e.g. `cmake --build --preset …`,
  `npm test`). The receiver verifies with these; vague "make sure it builds" is not enough.
- **Commit policy** — does the user gate commits? Is there a commit-message format? A trailer
  convention (some projects deliberately omit `Co-Authored-By`)? Branch rules? When unsure, default to
  "implement + report, do NOT commit" — unauthorized commits are hard to reverse.
- **Test policy** — TDD expected, or tests-only-on-request? (Encode it; don't impose TDD on a project
  that rejects it.)
- **Code conventions** — comment language, header-guard style, naming, banned constructs.
- **The parallel-agent landscape** — *is the receiver alone on this branch?* In multi-agent setups,
  find who else is editing what (recent commits, `git status`, other handoff docs). This drives the
  conflict matrix (below). Skipping this is the #1 cause of handoff collisions.

Read `CLAUDE.md` / `AGENTS.md`, recent `git log`, and any sibling handoff docs to source these. If a
convention is genuinely unknown and the user is reachable, ask rather than invent.

## The five principles (these are what make a handoff "lossless")

1. **Self-containment.** Inline the facts the receiver needs; don't make them read three other docs to
   start. Referenced specs are *optional depth*, not prerequisites. (If a fact lives in a file the
   receiver can open, a pointer is fine — but the critical path must be in the handoff itself.)

2. **Grounding — verify, don't trust.** Every file path, signature, line number, and "current state"
   claim must be re-checked against the live code **at handoff-writing time**, not recalled from
   memory or copied from an earlier handoff. Repos drift (renames, parallel commits). State drift = the
   receiver builds on a false premise. Re-measure `git log`/`git status` right before writing; cite
   `file:line`. Tell the receiver to re-verify too ("the repo is being renamed — confirm actual
   filenames before include").

3. **Scope boundaries + conflict matrix.** Say explicitly what the receiver **owns** and what they must
   **not touch** (and why — "that file is owned by parallel task X"). In multi-agent settings, a small
   ownership table (file × who-edits-it) prevents collisions far better than prose. Prefer path-scoped
   commits over `git add -A` so the receiver never sweeps another agent's work.

4. **Guardrails.** Carry the dangerous-by-default rules forward: no unauthorized commits, project
   conventions, "don't modify the frozen dependency", contested files to keep surgical. Frame them with
   the *why* so the receiver applies judgment, not just rote compliance.

5. **Hygiene on the originating side.** When a decision supersedes an earlier handoff, mark the old one
   (a "🔴 superseded → see X" banner) instead of leaving two contradictory docs. Migrate any memory /
   index pointers to the new entry point. Mind gitignored paths (a local-only spec won't travel to
   another machine — note it, or put durable handoffs in a tracked dir).

## Choosing A vs B (or both)

```
Is this delegating ONE bounded task to run now?            → A (agent prompt)
Is this preserving a multi-step effort / session context?  → B (resume handoff)
Multi-step effort AND you're slicing a piece off to delegate? → B for the whole + A for the slice
A decision/answer another agent asked for?                 → a short decision-response doc (A-style: ruling + rationale + boundary)
```

## Artifact A — Self-contained agent prompt

The receiver pastes one fenced block into a new session. Structure it so a stranger could execute it.
Full annotated skeleton: **`references/agent-prompt-template.md`** — read it when writing one. The shape:

- `[ROLE]` — one or two lines: who the agent is, the project/path/branch, the single goal. State up
  front if the task ships intentionally incomplete (e.g. "unwired — correct by construction").
- `[Hard rules]` — commit/test policy + code conventions (from Step 0).
- `[Verified facts]` — the grounded `file:line` facts the receiver needs (signatures,
  existing patterns to mirror, current wiring). "Don't trust this report — re-verify" where drift risk.
- `[STEP n]` — exact files + **complete code** (not "add appropriate X"). Show the real code.
- `[Boundaries]` — owns-these-files / never-touch-these (+ why). Parallel-agent notes.
- `[Verify]` — exact build/run command + expected output; what "done & correct" looks like.
- `[Self-review]` — a short checklist the agent runs before reporting.
- `[Report]` — ask for a **structured status**: DONE / DONE_WITH_CONCERNS / BLOCKED + changed
  files + verification result + anything deferred. (Structured reports let you trust-but-verify on
  return.) Reiterate "do not commit" if commits are gated.

Wrap the whole thing in one ``` fence so the user copies it cleanly. Put orchestrator-facing notes
(coordination, recommended commit message) *outside* the fence under a "Notes" heading.

## Artifact B — Resume / context handoff doc

A tracked markdown doc that is the **single entry point** for resuming. Full annotated skeleton:
**`references/resume-handoff-template.md`**. The shape:

- **TL;DR + next action** — where we are in one glance, and the very next concrete step.
- **State of the world, re-measured** — branch, `git log` (recent commits, what each did), what's
  committed vs uncommitted. (Re-run these as you write — don't paste stale state.)
- **Task / step status table** — done / in-progress / pending, with commit SHAs.
- **Locked decisions** — what is settled and must not be re-litigated (and any correction to a prior
  locked claim, with the reason).
- **Parallel-track conflict matrix** — who owns which file; what's safe to do in parallel; sequencing.
- **Guardrails & conventions** — carried from Step 0.
- **Pointers** — spec/plan locations (note gitignored ones), and which docs this one supersedes.
- **(If loss is likely) verbatim recovery** — paste the critical uncommitted code so a reset can't
  destroy it.
- **Change log** — append-only, so the next reader sees how state evolved.

When you write B, also do the **hygiene** (principle 5): banner the superseded handoff, migrate memory
pointers to this doc.

## Anti-patterns to avoid

- **Assumed context** — "continue the approach we discussed". The receiver wasn't there. Restate it.
- **Stale grounding** — reusing line numbers/paths/state from an earlier handoff without re-checking.
  Parallel commits and renames silently invalidate them.
- **Vague verification** — "make sure it works". Give the command and the expected output.
- **No boundaries in a shared repo** — the fastest way to make two agents clobber each other.
- **`git add -A` in a handoff** — sweeps unrelated/parallel work into the commit. Path-scope.
- **Orphaned superseded docs** — two handoffs disagreeing, with nothing saying which wins.
- **Over-stuffing the prompt with optional reading** — the receiver shouldn't *need* the linked specs;
  keep the critical path inline and the rest as "open if you want depth".
- **Forcing the receiver to read the plan file** — extract the slice into the prompt (self-containment).

## Where to put handoffs

Durable handoffs (B, and any prompt you want to survive) go in a **git-tracked** dir (e.g.
`doc/handoff/`), not a gitignored one — otherwise they won't reach another machine/session. Date-stamp
filenames so the latest is obvious. Reference templates for both artifacts live in `references/`.
