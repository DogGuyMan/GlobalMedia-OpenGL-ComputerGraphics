---
name: design-decision-discipline
description: Apply when deciding whether to introduce inheritance/polymorphism, an abstraction, an interface, or a class hierarchy — and when reasoning about ownership, data placement, or error handling. Use whenever the user asks "should this be a base class?", "should I make this polymorphic/an interface?", "is this over-engineered?", "where should this data/state live?", weighs composition vs inheritance, debates abstraction timing, designs object ownership/lifetime, or is proposing a new interface/module/layer. Pushes back on speculative abstraction and inheritance-for-reuse; separates edit-time change from runtime change; demands grep-measured evidence before new structure is declared. Also trigger when a proposal risks bundling several orthogonal design decisions (placement, interface depth, naming, lifecycle, …) into one converging recommendation. Trigger even when the user doesn't name a pattern but is clearly making a structural design call.
---

# Design Decision Discipline

A language- and project-agnostic set of design values for deciding code structure. Two ideas are central: **"separate change along the time axis"** and **"judge by cost asymmetry."**

## 1. Volatility ≠ Polymorphism (most important)

A common mistake: *"This class changes often, so I should give it polymorphic behavior via an inheritance hierarchy."*
This reasoning reaches the wrong conclusion **whenever it fails to distinguish the time axis of the change.**

| Kind of change | Meaning | Right tool |
|---|---|---|
| **Runtime change** | At runtime an object is swapped for a *different concrete type* (menu scene ↔ game scene) | ✅ Polymorphism (virtual + base interface) |
| **Edit-time change** | The code itself changes as the project progresses (a rectangle today, a cube tomorrow) | ❌ Polymorphism is useless — you just *rewrite the code* |

When you hear "this class changes a lot, so it needs to be flexible," ask first: **does it change at runtime, or does it change the next time I edit the code?** If the latter, polymorphism adds +0 capability and +1 complexity.

## 2. The 3 conditions that justify polymorphism (all must hold to adopt it)

1. **A common abstract interface is meaningful** — every variant can reasonably implement the same signature.
2. **The caller dispatches through a base pointer/reference** — calling without knowing the concrete type, e.g. `current->Render()`.
3. **A runtime-swap scenario actually exists** — menu↔game transition, backend selection, etc.

If even one is missing, keep it a leaf type, and if you want to nail down the intent, block accidental inheritance with `final` (or the language equivalent: Java `final`, Kotlin's default-final, C# `sealed`). When the time to introduce it arrives, extract the base then — don't build it ahead of time.

Justified triggers: same logic with a different backend (Vulkan/Metal/SW), runtime scene swap, pipeline stages dispatched through one interface. Not justified: "the next variant's code is different" (= edit-time change).

## 2.5 New-structure declaration — measure before you propose

When you propose a new interface/module/layer/class, disclose 4 items **in the same message as the proposal**:

1. **Implementer count now/expected** — from grep, not estimation.
2. **Consumer count now/expected** — from grep.
3. **Deletion test** — if this structure is removed, in how many places does its complexity reappear?
4. **Relation to the user's plan documents** — in-plan (cite the D#) or out-of-plan. Out-of-plan requires an irreplaceability argument; "mature engines do it" alone is NOT sufficient grounds.

If you did not actually measure (1) or (2), say so explicitly and demote the proposal to a hypothesis.

This is the *measured* counterpart of the 3 conditions in §2: condition ② and ③ asserted there in prose must survive contact with grep here.

## 2.6 Three blind-spot rules (they bind the human AND the agent)

1. **Layer direction**: before asking "isn't X really the role of Y?", check who takes whom as an argument — the call/parameter arrow decides the layer, and it is grep-measurable (a pool is policy ABOVE a backend because the pool's create function receives the command list). SRP-smell detection and layer attribution are separate judgments; being right on the first does not validate the second.
2. **Conceptual separation (SRP) ≠ physical file separation**: after agreeing a responsibility split, explicitly ask "new interface FILE, or extension of an existing class?" — an interface with 1 implementer and 0 mocks fails polymorphism condition ③ (§2) → extend the class instead.
3. **Principle-name definition**: when citing a named principle (LSP/SRP/OCP/ISP) to argue a point, append its one-line definition — a wrong name with a right intuition (and the reverse) must be separable; correct the name, then judge the intuition on its own merit.

## 2.7 Multi-axis separation — one axis per turn

A design proposal often bundles several ORTHOGONAL decision axes (placement, interface depth, naming, lifecycle, …) and converges them all at once — the decision space collapses before the user has examined any single axis.

Before proposing, **enumerate the axes explicitly** in an axis-declaration line (e.g. "this proposal contains 3 axes: placement / interface depth / naming"); commit a recommendation on **ONE axis per utterance**; mark every other axis explicitly deferred (e.g. "axis 2·3: deferred — undecided"). A recommendation on axis 1 must not silently decide axis 2.

Per axis, present at least 2 options with trade-offs, never a single pre-converged answer — and §1.5's confidence-to-action binding applies per axis (its option-table obligation binds any axis that is not confidently grounded).

The axis-declaration line is itself an inspectable artifact — its absence in a multi-decision proposal is the smell.

## 3. Inheritance for code reuse = anti-pattern

Creating `class FooDemo : public BaseDemo` just to share common setup violates "Favor Composition Over Inheritance." The typical decay path:
- helpers like `protected void SetupX()` accumulate in the base
- a child calls only *part* of the parent (a template-method variant)
- the parent swells from being an *abstraction* into the *union of all children*

**Alternative**: put common code in a free function, or hold a helper object as a member (composition). Inheritance is justified only when *"this object really is a kind of the base" (is-a)*.

As a natural evolution path, prefer **absorbing variation as data rather than inheritance** — instead of a subclass per variant, use one class + a `Config` struct (`Create(const Config&)`) so the differences live in data.

## 4. YAGNI — abstract only after the repetition appears twice

Build an abstraction/module/interface only after the *same pattern has actually appeared in two or more places*. Building it ahead of time "because I'll probably need it later" is debt. For every adoption decision, state explicitly **"now, or when the trigger arrives"** — the habit of writing down "not adopting now; adopt when X is needed" is what prevents over-design.

## 5. Judge by cost asymmetry (dependencies / explicitness)

Compare design trade-offs by the *temporal shape of their cost*:

> Cost of being explicit = (number of targets × once) — **one-time / linear**
> Cost of omission = (number of changes × debugging time) — **cumulative / permanent, and it blows up far from the root cause**

So a module declares the dependencies it actually uses directly, rather than relying on transitive leakage, and stays self-contained. (For the build-system application of this, see `modular-build-discipline`.)

## 6. Ownership model — single owner + raw references

- Every resource has a **single owner** (`unique_ptr` or an equivalent container); everywhere else sees it **only through a raw pointer/reference**. Avoid overusing `shared_ptr` and avoid raw `new`.
- A wrapper that solely owns a handle should **delete its copy/assignment** (`= delete`). If you don't block it, a copy causes a double-free that the compiler won't catch.
- A parent-child relationship is changed only through *one gate* (a single entry point that synchronizes both sides on add/remove) — touching it directly from multiple paths breaks consistency.

### 6.1 Data placement — prior owner first

When deciding WHICH class a piece of state lives in, ask in order:

1. **Who already owns the singular version of this state?** Generalization (single → container) happens in place — e.g. a `techniquePtr` generalizing to `passes` stays in Material, it does not migrate to Model. Search the codebase for the existing owner BEFORE proposing a location.
2. **What does this state describe?** Co-locate with the cohesion unit it describes (appearance → Material, shape → Geometry, scene composition → Renderer).
3. **The location of the code that iterates/uses the data is NOT a placement argument** — the user site can reach it by reference.
4. Still ambiguous → draw the ownership graph (see `graphviz-class-diagram`) and compare arrow directions per candidate.

Warning sign: patching a new requirement into "the class currently under discussion" instead of re-mapping ownership is the canonical incremental-patch bias (locality bias).

## 7. Error-handling philosophy — domains where exceptions aren't natural

In callback/error-code-based APIs (graphics, system calls, etc.), don't force exceptions. "Error handling" = **state polling + logging + returning bool/optional**.

| Situation | Return |
|---|---|
| Load/parse (can fail, value needed) | `optional<T>` |
| Resource factory (invalid on failure) | smart pointer (`nullptr` on failure) |
| Validation / status | `bool` (log the reason for failure) |

## How to apply

If the proposal bundles more than one orthogonal decision, run the §2.7 axis-declaration before anything else. When you get a structural decision: ① first determine whether the change is runtime or edit-time → ② if polymorphism is on the table, check the 3 conditions → ③ if adoption can be deferred, keep it a leaf + `final` and write down the trigger → ④ explain trade-offs via cost asymmetry. If you're proposing a new interface/module/layer/class, run the §2.5 declaration before the proposal leaves your mouth. If the question is where a piece of state should live, run the §6.1 order before naming a class. State conclusions firmly, but always attach the *why*.
