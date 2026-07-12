---
name: graphviz-class-diagram
description: Create publication-quality UML-style class & dependency diagrams of a codebase in Graphviz (`.dot` → SVG/PNG). Use this whenever the user wants to visualize code structure or dependencies — "draw a class diagram", "diagram how module X depends on Y", "show the dependency graph", "Graphviz/dot diagram of these classes", "visualize this architecture", "map the relationships between these files" — even if they don't say "Graphviz" or "UML". Also reach for it during review/refactoring when a structural picture would expose coupling, layering, or cycles. Encodes a grounding-first method (read headers + .cpp bodies + build files), 5 distinguished UML edge kinds, module-box clustering, and a bottom-up dependency-layer layout.
---

# Graphviz Class / Dependency Diagram

Operational method for drawing a UML-style class/dependency diagram of code — one module ↔ its consumer, a subsystem, or a layered architecture — as a Graphviz `.dot`, rendered to SVG + PNG.

A good diagram is **an argument about structure**, not a dump of every class. Its value is in what it *omits* and in how clearly it shows *who depends on whom, and how*. The method is six principles applied over four phases.

## The six principles (the identity of this style)

| P | Principle | What it means |
|---|---|---|
| **P1** | **Module-box clustering** | `subgraph cluster` per source dir / layer, so a dependency that crosses a layer is visible at a glance |
| **P2** | **Explicit dependency edges** | 5 relationship kinds (inherit / realize / compose / aggregate / depend) distinguished by arrowhead + color + label — "what depends on what, *how*" |
| **P3** | **Members + methods (UML 3-compartment)** | each core node shows fields + methods, so a reader infers *what the class does* from the node alone |
| **P4** | **Dependency-layer layout (`rankdir=BT`)** | most-depended-upon (foundation) at top, most-depending (orchestrator) at bottom |
| **P5** | **Straight-line routing (`splines=line`)** | orthogonal/straight edges; kills the spline-spaghetti that obscures a busy graph |
| **P6** | **Architecture-boundary visual gate** | "allowed leak vs violation" marked with bold-red edges — red anywhere else reads instantly as a breach |

**Prerequisite A1 — `constraint=true/false` split (this is what makes P4 work).** P4 collapses without it: with BT + clusters + many dependency edges, *every* edge pulls on the rank assignment and the layer axis scatters (clusters spread, edges cross). So split them: structural edges (inherit / compose / aggregate) get `constraint=true` — they form the layout skeleton; dependency edges get `constraint=false` — they cross-cut without distorting the layers. **Wanting P4 means A1 is mandatory, not optional.**

The four phases below are also a checklist. Why grounding comes first: dependency facts live in three different files, and reading only one leaves edges missing or wrong.

## Phase 1 — Grounding (before any drawing)

**The three-layer source rule** — the single most important habit:

- **Headers (`.h`)** → *member edges* (composition / aggregation). A `unique_ptr<T>` or by-value member = compose; a `T*` / `T&` / `vector<T*>` (non-owning) = aggregate.
- **Consumer bodies (`.cpp`)** → *usage edges* (dependency) + *node literals*. `creates` / `Get()` / `Execute()` calls and return literals (e.g. a `GetKey()` that returns `"World"`) appear only in constructor calls and method bodies — **never in the header.** Read the `.cpp` that *originates* the edges: for a consumer-focused diagram that's the consuming `main.cpp` / `*.impls.cpp`. **For a library / foundation module (no single consumer), read the module's *own* `.cpp` — it is the consumer of *its* dependencies, and the usage edges toward those modules live there.**
- **Build files (`CMakeLists.txt` `target_link_libraries`, or `BUILD`, `package.json`, etc.)** → *module dependency direction*, authoritatively. A forward-decl in a header can lie about direction; the link graph cannot. This is how you assert "X depends on Y but Y never depends on X".

Checklist:
- [ ] State goal + scope in one line ("module X ↔ consumer Y"); put it in the file header comment.
- [ ] Scout the output dir (e.g. `doc/diagrams`) for the date-naming convention and sibling diagrams to match style and cross-check.
- [ ] Read core class **headers** → members, public methods, inheritance (`class X : public A, B`).
- [ ] Read consumer **`.cpp` bodies** → usage edges + node literals the headers don't show.
- [ ] Classify each member's **ownership** (the rule above) — this decides compose vs aggregate vs depend, and is the main driver of diagram accuracy.
- [ ] Determine module boundaries **and direction** from the build file's link graph.
- [ ] Scan for bidirectionality / cycles (`A→B` and `B→A`; `A→B→C→A`). Found → mark red `dir=both` or redefine layers. Not found → that absence is itself a result worth asserting in a note node.
- [ ] Record scope provenance + exclusion reasons (which spec/plan; why a node is omitted) in the header comment.

## Phase 2 — Nodes (render vs omit)

Detail is graded by role — too much kills readability, too little hides structure.

| Kind | Render level | Notation |
|---|---|---|
| **Core class** (in-scope) | UML 3-compartment: name + stereotype / members + ownership notes / methods | HTML `<table>` |
| **External / forward** (other module) | box + name only | `shape=box`, gray |
| **struct / enum (data)** | node yes, fields collapsed to "field1, field2…" | 1–2 row HTML |
| **public free function / namespaced API** (e.g. a free `ComputeX`, not a private helper) | node with `«free fn»` stereotype, signatures only — it originates real edges, don't omit it | 1-row HTML |
| **static / macro / anon-ns / private helper** | omit entirely | — |
| **transitional (being removed)** | keep node, mark the edge `style=dashed` + "transitional" | dashed |

- Even on core classes, drop trivial getters/setters; keep methods that convey responsibility. Collapse several onto one line: `+ A() / B() / C()`.
- **Double-encode ownership**: an arrowhead (diamond) *and* a colored member-text note (`(owns)` / `(observes)` / `(shared)`). In a large graph one channel often can't be traced across the page; the other recovers it.
- `«interface»` stereotype on pure-virtual types; multiplicity labels on collections (`0..*`).
- **Inheritance depth**: show only the *immediate* base of an in-scope class; render external ancestors as boxes and stop — don't transitively expand them.
- **Fluent Builder** methods that `return *this` → show as a method signature (`: Self&`); do **not** draw a self-loop edge (pure noise).
- An **external** type is box+name only — but may still carry an `«interface»` stereotype and a realize/inherit edge to it **when that edge explains an in-scope class's base** (e.g. an in-scope base class realizing an external interface). An edge between two external boxes belongs only if it clarifies an in-scope node.

## Phase 3 — Edges (5 UML kinds)

Group each kind under an `edge[...]` default block so the style is applied uniformly and the `.dot` stays readable.

| Relationship | Style | Color | Source signal |
|---|---|---|---|
| **Inherit** (is-a) | `arrowhead=onormal, style=solid` | navy `#1f4e79` | `class D : public Base` |
| **Realize** (implements) | `arrowhead=onormal, style=dashed` | black `#333` | `class C : public IFace` |
| **Compose** (owns) | `dir=both, arrowtail=diamond, arrowhead=vee` | purple `#7030a0` | `unique_ptr` / by-value member |
| **Aggregate** (refs) | `dir=both, arrowtail=odiamond, arrowhead=vee` | blue `#2e75b6` | `T*` / `T&` / `vector<T*>` non-owning |
| **Depend** (uses) | `style=dashed, arrowhead=vee` | gray `#777` | method arg / return / `creates` |

- Edge label = member name + multiplicity (`mPasses 0..*`) or action (`creates`, `Get()/Execute`).
- **P6 boundary gate**: an "allowed leak" dependency (e.g. `DeviceContext → GraphicsAPI` calling `gl*` directly) gets **bold red** (`color="#D50000", penwidth=2.6`). The rule the reader internalizes: *red anywhere unexpected = architecture violation.* Use it sparingly so it stays a signal.
- **Bidirectional**: only a true mutual `A↔B` earns red `dir=both`. If there is none, add a **note node asserting "single direction / no cycle"** — documenting the *absence* of a cycle answers the reader's first structural question up front.

## Phase 4 — Layout (a decision rule, not taste)

- **Dependency / layering diagram** → `rankdir=BT` + `splines=line` + the `constraint=true/false` split (the three travel together — see A1). BT puts foundation on top, orchestrator on the bottom; the constraint split is what keeps a busy graph from scattering.
- **Process / control-flow diagram** → `rankdir=TB` is correct — but that is a *different diagram type*, not this one.
- `ranksep`: larger (1.7–2.5) when emphasizing layers; ~0.9 when compact.
- `compound=true` **plus actual `lhead=cluster_X`** to bundle edges entering a cluster (fewer crossings). Declaring `compound=true` without using `lhead` does nothing. **Trade-off**: skip `lhead` when edges must reach *specific* nodes inside the cluster (e.g. a per-type ownership diagram where "*which* external type" is the whole point) — bundling to the cluster head erases that. Use it only when the cluster, not the node, is the meaningful endpoint.
- **Many-cluster / foundation targets sprawl horizontally** — but the fix is *not* to flatten externals into one gray bucket; that sacrifices P1, and an external module still deserves its own bordered box. Reconcile with **nested clusters**: per-module external sub-clusters (each labeled + bordered) wrapped inside one outer `external` band cluster, held at the top rank. Bound the width by stacking with **invisible `constraint=true` edges** — *intra-module* (a module's types stacked vertically) and, if still wide, *inter-module* (chain modules into 2–3 short columns) — rather than `rank=same/source/sink`, which is unreliable inside clusters under `rankdir=BT`. This keeps per-module boxes (P1) *and* bounds the width.

## Legend + header comment

- **Legend = rendered example edges**, not a text table. Inside `cluster_legend`, draw real `L_a -> L_b [arrowhead=onormal, ...]` for each of the 5 kinds, so the legend renders with the *exact* styles the graph uses (self-verifying, and more teachable than prose). On a many-cluster graph the legend's unconnected rows stretch to canvas width — stack them vertically with invisible `constraint=true` edges between rows.
- **Header comment** (top of the `.dot`): ① title + date ② goal / scope ③ one-line summary of the 5 edge kinds ④ scope provenance (spec/plan) + exclusion reasons ⑤ the render command.
- Comments / labels: follow the project convention (this project: Korean prose + ASCII/English signatures).

## Render

```bash
dot -Tsvg NAME.dot -o NAME.svg
dot -Tpng -Gdpi=140 NAME.dot -o NAME.png
```

Always render **both** and eyeball the SVG — `dot` exiting 0 ≠ a readable diagram. If clusters scatter or edges cross heavily, the usual cause is skipping the Phase-4/A1 discipline (a `TB + splines=spline + no-constraint` graph is the classic failure mode).

## Decision-gate mode — candidate ownership mini-graphs

- **Trigger**: when a placement or layering decision arises (the new-structure declaration template's `종류` field flags this), draw BEFORE arguing in prose.
- **Format**: one mini `.dot` **per candidate placement** — 3–7 nodes only (the classes involved); edges = ownership (diamond) + calls (arrow). Place candidates side by side and compare arrow directions; a candidate where "the abstract knows the concrete" (reverse arrow) is eliminated.
- **Storage**: save as `<decision-name>-ownership.dot/.svg` in the SAME directory as the spec/handoff that records the decision, plus a relative link from that doc (the audit gate then watches link liveness).
- This is the default tool for the 'Uncertain' tier of `confidence-and-sourcing` §1.5.

## Worked example & rationale (bundled)

Pick the worked example whose *shape* matches your target:

- **`references/worked-example.dot`** — **consumer-shape**: a render module ↔ its application consumer (6 clusters, a bidirectional/cycle check, UML HTML nodes, the 5 `edge[]` blocks, a rendered-example legend, a direction-assertion note). The template for *"module X ↔ its consumer"*.
- **`references/worked-example-foundation.dot`** — **foundation/library-shape**: a leaf module (`SJH::sprite`) and its dependency fan. Demonstrates the patterns the consumer example lacks — a `«free fn»` node, **nested per-module external sub-clusters** (each external module bordered) inside one band + a vertically-stacked legend (anti-sprawl, reconciling P1 with bounded width), immediate-base-only external boxes, and the library-module *".cpp originates the edges"* reading. The template for *"diagram a module's own dependencies"*.
- **`references/methodology-rationale.md`** — *why* this style beats a naive auto-layout dump: an adversarial comparison against 8 peer diagrams, plus the honest limits (where the baseline was actually better). Read it when you want the reasoning behind a principle, or when adapting the method to an unusual graph.
