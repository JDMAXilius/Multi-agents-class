# Multi-Agents-Class

**Course:** Multi-Agent AI for Game Development (ELVTR) · **Author:** Juan Diego Lugo
**Capstone:** **BREACHPOINT** — a Halo-inspired 4v4 arena FPS, built in UE 5.8 / pure
native C++ / GAS by one principal engineer directing a 12-agent AI crew, shipping a Steam
demo in six weeks.

This repo is the course work **and** the capstone's complete pre-production studio: design
documents, architecture, contracts, the agent crew, and the executable ticket board. The
game's code lives in its own repo; this is where it was planned, and where the crew kit
ships from.

---

## Repo Map

| Path | What it is |
|---|---|
| **`breachpoint/`** | **The active project.** [Vertical-slice GDD](breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md) (the six-week build) · [full-concept GDD](breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md) (Phase-2 target) · [Architecture](breachpoint/BREACHPOINT-ARCHITECTURE.md) (44 class-units, file-by-file) · [Authoring Matrix](breachpoint/BREACHPOINT-AUTHORING-MATRIX.md) (**R18: zero Blueprint classes** — the four tiers and who authors each: crew C++/text · generated script · you in the editor) · [Bot AI](breachpoint/BREACHPOINT-AI-BOTS.md) (**GOAP ambitions → StateTree spine → GAS hand** — the Halo-researched three-layer brain) · [GameLift Plan](breachpoint/BREACHPOINT-GAMELIFT-PLAN.md) (Phase-2 dedicated servers: the seam gaps fixed in-slice, five cost-gated rungs, Steam-derived identity) · [Roadmap](breachpoint/BREACHPOINT-ROADMAP.md) (six pods, six milestones) · [Quality Bars](breachpoint/docs/BREACHPOINT-QUALITY-BARS.md) (budgets, DoD, playtest protocol, ship checklist) |
| **`breachpoint/` (cont.)** | **The crew studio now lives INSIDE the game repo** — `breachpoint/.claude/` (12 agents, 7 skills, law-enforcement hooks) and `breachpoint/docs/` ([rulings ledger](breachpoint/docs/DESIGN-RULINGS.md) · [crew map](breachpoint/docs/CREW_MAP.md) · [playbook](breachpoint/docs/CREW_PLAYBOOK.md) · contracts · [the board](breachpoint/docs/tickets/), BP00–BP16). `crew/` at the root is an empty leftover of the move. Start here: [`breachpoint/README.md`](breachpoint/README.md) |
| **`assignments/`** | Course submissions of record. **Start at [`assignments/README.md`](assignments/README.md)** — the index, run commands, and submission guide. [Assignment #1](assignments/01-gdd-first-draft/) (*Slash Roller: Arena* first-draft GDD) · [#2](assignments/02-gdd-final-draft/) (**Breachpoint** final GDD, Markdown + styled PDF) · [#3](assignments/03-agent-crew/) (**the runnable agent crew** — 4 agents producing Breachpoint's `DT_Weapons.csv` + `arena_manifest.json`, with replay mode and Mermaid architecture) · [#4](assignments/04-content-pipeline/) (**the dynamic content pipeline** — BM25 RAG over the real GDD + shipped DataTables, an adversarial critic, and three landed content tables filling gaps the pipeline *proves* the game has) · [#5](assignments/05-goal-oriented-agent/) (**the goal-oriented coding agent** — reads the GDD, scans a pinned copy of the codebase, ranks the gaps deterministically, and writes the missing `UBRSpotterSubsystem`) · [#6](assignments/06-ger-pipeline/) (**the GER pipeline** — Generator → Evaluator → Refiner with a circuit breaker, enforcing GDD §3.3's stand-alone rule on canned announcer lines) |
| **`docs/course/`** | Course-wide references — [the GDD writing standard](docs/course/GDD-FORMAT-GUIDE.md) · [Game Developers Conference overview](docs/course/GDC-overview.md) |
| **`docs/method/`** | The crew methodology — [engineering disciplines D1–D8](docs/method/ENGINEERING-DISCIPLINES.md) · [crew operations plan](docs/method/CREW-OPERATIONS.md) (topology, model economics, metrics — v2-synced) · [researched best-practice validation](docs/method/ARCHITECTURE-VALIDATION.md) · [roster design history](docs/method/CREW-ROSTER.md) (superseded by [`breachpoint/docs/CREW_MAP.md`](breachpoint/docs/CREW_MAP.md)) |
| **`docs/decisions/`** | Decision records — [the A/B scope comparison](docs/decisions/SCOPE-COMPARISON.md) that chose Breachpoint's vertical slice (Option C) |
| **`archive/`** | Superseded work, kept honestly — the [Slash Roller: Arena tech spec](archive/slashroller/) (pre-pivot capstone) · the [Gauntlet single-player draft](archive/concept-gauntlet/) (its wave/AI-Director design feeds Phase-2 Firefight) |

## How the project got here

1. **Assignments** — the capstone GDD was drafted and revised as *Slash Roller: Arena*
   (third-person melee deathmatch on the studio's shipped GAS core), stress-tested by an
   agent review crew per the Class-03 method.
2. **The crew was formulated** — disciplines D1–D8 were drawn as real ownership
   boundaries, validated against industry practice, and minted into the agent kit with
   binding contracts.
3. **The scope comparison** — a second concept (Halo-inspired FPS) was designed and both
   were costed line-by-line. Verdict: full Concept B doesn't fit six weeks; **Option C —
   the Breachpoint vertical slice — does**, and it was chosen as the build.
4. **Pre-production completed (M0)** — GDD, architecture (with GAS-purity law, soft-ref
   modularity, the owned input layer), roadmap (six parallel pods, Halo-Studios-style),
   quality bars, and all 15 tickets.

## Status & next action

**Coursework: all six built.** Assignments [#1](assignments/01-gdd-first-draft/),
[#2](assignments/02-gdd-final-draft/), [#3](assignments/03-agent-crew/) and
[#4](assignments/04-content-pipeline/) are submitted;
[**#5**](assignments/05-goal-oriented-agent/) (due 13 Aug) and
[**#6**](assignments/06-ger-pipeline/) (due 18 Aug) are built and pushed but **not yet
submitted** — both need a short late note. The index,
run commands and submission steps are in
[`assignments/README.md`](assignments/README.md).
**From here this repo is a Breachpoint project, not a course project** — the
crew, the board, and the docs serve the build. The board is
[`breachpoint/docs/tickets/`](breachpoint/docs/tickets/) (BP00–BP15); `assignments/` is a record, not a
workstream. Assignment #5 is **self-contained in `assignments/05-goal-oriented-agent/`** and reads a
pinned copy of the game rather than the live tree, so the daily churn in `breachpoint/` cannot
invalidate it.

**M0 — pre-production complete.** ✅
The data crew has **run**: `DT_Weapons.csv` and `arena_manifest.json` are authored, reviewed,
and verifier-proven ([BP13 Log](breachpoint/docs/tickets/TICKET_BP13_DATA_CREW.md#log)).

The game repo **exists** — `breachpoint/` is its root, the module compiles, and PIE runs our
GameMode. Read [`breachpoint/docs/tickets/HANDOFF.md`](breachpoint/docs/tickets/HANDOFF.md)
first; it records where every ticket actually stands. Everything on the board needs UE 5.8
installed — it cannot run in a cloud container.

**The board is `ls breachpoint/docs/tickets/`** — that directory is the authority, not a list
here. This README has been wrong about the board twice; it now points at it instead of
restating it.

**On the Architect.** `Tools/architect/` — the deterministic planning layer cut from
[Class 07](docs/course/CLASS-07-AUTONOMOUS-AGENCY.md) — was built, ran, and was **deleted on
4 Aug 2026** (`6996f86`) along with the rest of the tooling nothing invoked. Its assessment
survives at [`breachpoint/docs/ASSIGNMENT-5.md`](breachpoint/docs/ASSIGNMENT-5.md), kept
deliberately un-rewritten, and the code remains readable at `ac84b76`. That run **produced no
gameplay code** — it stalled at the owner-path law on a unit whose inputs were not on disk.
The course deliverable was rebuilt from scratch instead, in
[`assignments/05-goal-oriented-agent/`](assignments/05-goal-oriented-agent/), where that
failure is a scored ranking term rather than a lesson.

The milestone ladder from there: M1 Foundry → **M2 Golden Triangle (fun gate)** → M3
Playground → **M4 First Match (go/no-go)** → M5 Two Boxes → **M6 Steam Demo Live**.
