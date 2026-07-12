# Template B — Resume / context handoff doc

A git-tracked markdown doc that is the **single entry point** for resuming a multi-step effort in a
fresh session — or that you write *before* a long task so nothing is lost if the session ends. Unlike
the agent prompt (one task, paste-and-go), this preserves the *whole effort's* state.

Put it somewhere tracked (e.g. `doc/handoff/YYYY-MM-DD-<topic>-resume-handoff.md`) so it survives across
machines/sessions. Date-stamp so the latest is obvious.

---

## Skeleton (fill the `<…>`; delete bracketed guidance)

````markdown
# Handoff — <topic> resume context (<date>, lossless)

> **This document = the single entry point for the next session.** <one line: what effort, current position>.
> **Written**: <date>. Branch `<branch>`. HEAD `<sha>`.
> **Supersedes**: <prior handoff this supersedes, if any — and banner that one>.
> ⚠ <if this repo is mid-parallel-commits>: at start, re-measure with `git log --oneline -N` + `git status --short`.

## 0. At a glance (TL;DR)
- **What we're doing**: <one-paragraph what + why>.
- **Progress**: <Task A·B committed / C not started> (verify against HEAD).
- **Next action**: <the very next concrete step>.
- **Canonical docs**: <spec/plan paths — note which are gitignored/local-only>.
- **Cadence/method**: <how the work is executed — e.g. subagent-driven, per-task commit gate>.

## 1. State (at writing time — must re-measure)   ← re-run these as you write; don't paste stale
```
HEAD: <sha + subject>
Branch: <branch>
Recent commits (new→old): <list, with one-line of what each did + which are yours vs parallel agents>
Uncommitted: <your relevant uncommitted files; note parallel-agent dirty separately = "not mine">
```

## 2. <Task/Step> status table
| # | Content | Status | Commit/verification |
|---|---|---|---|
| <0> | <…> | ✅ committed `<sha>` / 🟡 implemented·uncommitted / ⬜ not started | <evidence / verification> |

## 3. Canonical document index
| Document | Path | git | Role |
|---|---|---|---|
| <spec> | <path> | <tracked / gitignore(local)> | <…> |
> Gitignored canonical docs may not exist on another machine — make this doc enough to reconstruct from.

## 4. Locked decisions (do not re-litigate) + corrections found
- <decision 1 — settled, do not re-litigate>.
- ⚠ <correction to a prior locked claim, with the reason it was wrong + the fix>.

## 5. Remaining work — implementation pointers
- **<Task C>**: <files + exact pointers + caution (account for parallel work)>. <build-only / GUI-verify>.

## 6. Parallel-track conflict matrix   ← multi-agent only, but critical there
| Track | Status | vs. my work |
|---|---|---|
| <other agent's track> | <committed/wip> | <why 0 conflict / shared edit point / sequencing> |

| File | TrackX | MyTask | TrackY |  ← file × owner, for genuinely shared files
|---|---|---|---|
| <shared file> | <edits> | <edits> | <edits> | → <sequencing/coordination>

## 7. Guardrails / conventions
- <commit policy / path-scoped / no_auto_tests / files-not-to-touch / code conventions / trailer policy>.

## 8. (If loss is likely) critical code verbatim   ← so a reset can't destroy uncommitted work
```<lang>
<paste the critical uncommitted code, file by file>
```

## 9. Known issues (unrelated to my work, to avoid confusion)
- <parallel issue that affects runtime but isn't yours — so the resumer doesn't chase it>.

## 10. Change log (append-only)
| Date | Change |
|---|---|
| <date> | <what this handoff captured> |
````

---

## Notes on the parts

- **Single entry point** — the resumer should open *this* and need nothing else to orient. If five docs
  exist, this one says which is which and which wins.
- **Re-measured state (§1)** — the most common rot. Parallel agents commit while you write; renames
  happen. Run `git log`/`git status` *as you write the handoff*, not from memory. Separate "my
  uncommitted work" from "parallel agent's dirty files (not mine)".
- **Status table (§2)** — verify each "done" against HEAD (grep the symbol, check the SHA). A handoff
  that says "Task 3 done" when it was reverted sends the resumer down a false path.
- **Locked decisions (§4)** — protects settled ground from being re-opened. If implementation revealed a
  prior locked claim was *wrong*, record the correction + why (don't silently contradict it).
- **Conflict matrix (§6)** — in multi-agent work this is the highest-value section. A file×owner table
  prevents two agents clobbering the same file. State sequencing for genuinely shared files.
- **Verbatim recovery (§8)** — only when meaningful uncommitted work exists and reset/fold is a real
  risk (e.g. another agent or the user squashes the working tree). Paste it so it can be re-applied.
- **Change log (§10)** — append-only history; lets the next reader see how state evolved across handoffs.

## Originating-side hygiene (do this when you write B)

- **Banner the superseded handoff**: open the old doc and add `> 🔴 superseded (<date>) → see <new doc>`
  at the top, so two docs never silently disagree.
- **Migrate pointers**: update any memory/index entry (e.g. `MEMORY.md`, a project memory file) that
  pointed at the old entry point to point at this one.
- **Mind gitignore**: if your spec/plan live in a gitignored dir, the durable handoff (this doc) must be
  in a *tracked* dir and self-sufficient enough to reconstruct from.
