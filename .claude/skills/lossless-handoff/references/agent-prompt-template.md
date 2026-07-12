# Template A — Self-contained agent prompt

A paste-ready prompt for delegating **one bounded task** to a separate Claude Code agent. The receiver
copies the fenced block into a fresh session and executes it with no other context.

Two layers: the **fenced block** (what the receiver pastes) and the **Notes** outside it (for the
orchestrator/user — coordination, commit message). Keep them separate so the copy is clean.

---

## Skeleton (fill the `<…>`; delete bracketed guidance)

````markdown
# Prompt — <task name> (for another Claude Code Agent)

> Paste the ``` code block below into a new session as-is. Self-contained.
> Canonical reference (optional, you don't need to read it): <spec/plan path>

```
[ROLE]
You are the implementation agent for a <language/stack> project (<absolute repo path>, branch <branch>).
Goal: <one sentence — what you are building>. <If intentionally incomplete, say so: "X ships unwired — the trigger is a follow-up">.

[HARD RULES]   ← put the Step 0 conventions here
- No commits / git add (only after user approval). Implement + verify with <build cmd>, then report only.   ← or the project's actual commit policy
- Tests: <TDD or not — e.g. do NOT auto-add unit tests>. Verification = build + <visual/log>.
- <Code conventions: comment language / header guards / naming / banned constructs / commit-trailer policy>.
- This branch has many parallel agents — path-scoped commits, don't touch unrelated dirty files.   ← if multi-agent

[IMPORTANT — BOUNDARIES (conflict avoidance)]   ← when overlapping with multi-agent / in-progress work
- **Do not modify <file/area>** — <reason: owned by another task/agent>.
- Your working files = <exactly N paths>. Do not stray outside this scope.
- <contested file (e.g. main.cpp)>: <allowed range — e.g. "one line only, don't touch line X, surgical">.

[VERIFIED FACTS — current infrastructure]   ← grounding: actually-measured file:line facts. The foundation the receiving agent trusts.
- <existing pattern to mirror>: the <signature/behavior> at <file:line>.
- <already exists — do not redefine>: <symbol> at <file:line>.
- <drift risk>: "this repo is mid-<rename/parallel commits> — confirm actual filenames/paths before include."

========================================================================
[STEP 1] <file path> (new/modify)
========================================================================
<complete code — no "add appropriately"; show the real code>
```cpp
// ... full code ...
```
- <one line of caution/rationale>.

========================================================================
[STEP 2] <next file> ...
========================================================================
<...>

[VERIFY]
- Build: `<exact build command>` → exit 0, `<expected last line>`.
- Run (if applicable): `<run command>` → <expected observation: what should be seen/heard, or "0 behavior change = correct">.

[Self-review]
- <check 1: signature/pattern consistent?>
- <check 2: only the designated files changed? boundaries respected?>
- <check 3: build exit 0?>
- <check 4: no redefinition/duplication?>

[REPORT]
DONE / DONE_WITH_CONCERNS / BLOCKED + per-file change summary + last build line + (if applicable) run observation
+ explicitly list anything unhandled / needing coordination + `git status --short` (your files). Do not commit.
```

---

## Notes (for the orchestrator/user)   ← *outside* the code block
- <nature of this task — e.g. ships unwired, build verification only>.
- <contention points — e.g. "the foundation also touches PlayerActor.cpp → if run concurrently, one side rebases">.
- When you get the result (DONE + diff), the <orchestrator> verifies against the spec, then commits.
- Commit message (recommended): `<type> : <summary>` <(project trailer policy)>.
````

---

## Notes on the parts

- **`[ROLE]`** — short. The receiver must instantly know identity + goal + repo. If the task ships
  intentionally incomplete, say so here so the agent doesn't "helpfully" wire it up.
- **`[HARD RULES]` / `[BOUNDARIES]`** — these are the *safety* of the handoff. Default commit policy to
  "report, don't commit" unless you know the user wants autonomous commits. In a shared repo, the
  ownership boundary is not optional.
- **`[VERIFIED FACTS]`** — the heart of self-containment. Inline the signatures/patterns the receiver will
  mirror, with `file:line`. This is what lets them work without reading the spec. Flag drift risk.
- **`[STEP n]`** — **complete code**, not descriptions. "Add validation" / "similar to step 1" are
  failures — the receiver may read steps out of order and has no other context.
- **`[VERIFY]`** — exact command + expected output. For intentionally-no-op tasks, say "0 behavior change = correct"
  so the agent doesn't think it failed.
- **`[REPORT]`** — a structured status is what lets the orchestrator trust-but-verify. Always restate the
  commit gate here too (it's the most dangerous default to get wrong).
- **Decision-response variant**: when another agent asked a design question, the same A-style structure
  works as a *ruling*: state the decision, the rationale (tie to locked constraints), the resulting
  boundary, and answer each question. Banner any prior doc it supersedes.
