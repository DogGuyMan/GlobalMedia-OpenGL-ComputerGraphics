---
name: code-design-review-lenses
description: Apply when reviewing or evaluating the design quality of a class, module, or codebase — not line-level bugs, but structure, ownership, coupling, header hygiene, SOLID, and consistency. Use whenever the user asks to "review this design", "evaluate this architecture/module", "is this well-structured?", "what's wrong with how this is organized?", or wants a design critique of existing code — including reviewing your OWN structural proposal before sending it. Produces a structured multi-lens assessment that separates objective facts (citable) from subjective opinion. Trigger even when the user just pastes a class and asks "thoughts?".
---

# Code Design Review Lenses

A methodology for evaluating the *design quality* of code (structure, not bugs). Look through 5 lenses, and **always separate objective fact from subjective opinion.**

## Core discipline: objective / 💭 subjective separation

Tag each observation as one of two kinds:
- **Objective**: a fact citable by file and line ("`material.h:57` deletes the copy constructor").
- **💭 Subjective**: a value judgment or recommendation ("this would be cleaner extracted into a Renderer class").

Separating them lets the reader instantly tell *what is fact and what is my opinion*, and makes the points of agreement/disagreement clear. Unsupported assertions ("this is bad code") are forbidden — back them with a fact or mark them with 💭.

Also keep the assessment **balanced**: note 💚 good / 💛 weakness / 🚨 bug·typo / 💭 open question together. Explain *why* a good decision is good, too.

## The 5 lenses

### Lens ① Ownership model
Who owns what? Single owner + raw references, or is there overuse of `shared_ptr` / raw `new` / unclear ownership? Does a handle-owning class block copy/assignment (`= delete`)? If not, point out the double-free risk.

### Lens ② Coupling
Is the dependency one-way or two-way? A knows B, but does B also need to know A? Where can a forward declaration break a cycle? Is there a class that has come to know types outside its responsibility (e.g. a container type)? Does the dependency direction match the abstraction level (be suspicious when the abstract knows the concrete)?

### Lens ③ ODR / header hygiene
Header-guard consistency (+ endif-comment typos). A non-`inline` global defined in a header (especially a heavy object like a `const std::vector`) is duplicated per translation unit → costs compile time and binary size. Recommend `inline constexpr` vs moving to a `.cpp`. Whether header-only `inline` functions are appropriate (fine if short and there's no reason to split link).

### Lens ④ SOLID / separation of responsibility
Evaluate each principle as 💚/💛 with evidence:
- **S**: Does one class/module have a single responsibility? (e.g. 💛 if geometry-builder free functions and a model-object class are mixed in one namespace)
- **O**: Are there extension points? Distinguish whether the absence of `virtual` is intentional or an omission.
- **L**: Does a derived type avoid breaking the base contract?
- **I**: Is the interface so bloated that the caller has to know everything?
- **D**: Is mocking/testing hard because of a direct dependency on a concrete class? (For a learning/small-scale project, note this is a trade-off.)

### Lens ⑤ Consistency (naming / conventions / patterns)
Is naming unified into one style? Has the same design pattern (e.g. "base + specific") recurred enough that the intent is clear? Are cross-cutting concerns like diagnostics/logging applied consistently?

## Output structure

```markdown
## Class design assessment

### Lens ① Ownership model
**Objective**: <citable fact + file:line>
**Assessment**: 💚 ... / 💛 ... / 🚨 ... / 💭 ...

### Lens ② Coupling
...

### Summary
> 💭 <overall impression — what's done well, and what's needed as the next step>
```

## Self-review gate — apply the lenses to your OWN proposal

These lenses exist to review code — and the recorded failure mode is applying them to others' code while exempting your own structural proposals. Verification asymmetry: your own proposal rides generation inertia (a minimal edit of your previous proposal), so confirmation bias passes unchecked unless you deliberately re-review it as a stranger's PR.

Before sending any structural-change proposal (new placement, new interface, new layer): re-read it as if it were someone else's PR and run at minimum **Lens ① (ownership — who owned this data before? does my placement match the prior owner?)** and **Lens ④ (SRP — did I just teach a class a responsibility it shouldn't know?)**. Emit a 1-2 line self-review verdict WITH the proposal ("self-review: lens ①/④ pass because …" — or the smell you found).

Falsification questions that have caught real errors: "What happens to the variant that does NOT use this?" / "Who owned this data originally?" / "Should this class know this name?"

This gate binds any author — human or agent; for cross-agent enforcement see `agent-orchestration-anti-gaming` (complementary, not overlapping).

## How to apply

When you get code, sweep the 5 lenses in order, but dig deep only into the lenses that matter for that code. Attach a file:line basis to every criticism, and never mix fact with opinion. Close the final summary with *how the extraction/structure looks + what needs to be added next*. Before sending your own structural proposal, pass it through the Self-review gate above.
