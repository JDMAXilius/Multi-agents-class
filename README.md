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
| **`breachpoint/`** | **The active project.** [Vertical-slice GDD](breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md) (the six-week build) · [full-concept GDD](breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md) (Phase-2 target) · [Architecture](breachpoint/BREACHPOINT-ARCHITECTURE.md) (44 class-units, file-by-file) · [Authoring Matrix](breachpoint/BREACHPOINT-AUTHORING-MATRIX.md) (**R18: zero Blueprint classes** — the four tiers and who authors each: crew C++/text · generated script · you in the editor) · [Bot AI](breachpoint/BREACHPOINT-AI-BOTS.md) (**GOAP ambitions → StateTree spine → GAS hand** — the Halo-researched three-layer brain) · [GameLift Plan](breachpoint/BREACHPOINT-GAMELIFT-PLAN.md) (Phase-2 dedicated servers: the seam gaps fixed in-slice, five cost-gated rungs, Steam-derived identity) · [Roadmap](breachpoint/BREACHPOINT-ROADMAP.md) (six pods, six milestones) · [Quality Bars](breachpoint/BREACHPOINT-QUALITY-BARS.md) (budgets, DoD, playtest protocol, ship checklist) |
| **`crew/`** | **The drop-in agent studio, v2** — copy its contents to the game repo root. 12 agents (7 builders, 2 reviewers, 3 curators — each with ROUTING/I-O/KICKOFF sections), **law-enforcement hooks** (owner-path + banned-API blocks at tool-call time), the [rulings ledger](crew/docs/DESIGN-RULINGS.md), the [crew map](crew/docs/CREW_MAP.md) (diagrams + invocation matrix), 7 skills (game-lead · tickets · **gas-purity** · **ue-editor** · **ue5-ui-architecture** · **gauntlet-testing** · **cmc-prediction** — the last three are unverified drafts, corrected by the packet that first uses them), 6 contracts, and the executable board: [tickets BP00–BP14](crew/docs/tickets/), including [BP13](crew/docs/tickets/TICKET_BP13_DATA_CREW.md) — the data crew that **has run** — and [BP08](crew/docs/tickets/TICKET_BP08_BOTS.md) recut for the three-layer bot brain. Start here: [`crew/README.md`](crew/README.md) |
| **`assignments/`** | Course submissions of record — [Assignment #1](assignments/01-gdd-first-draft/) (*Slash Roller: Arena* first-draft GDD) · [#2](assignments/02-gdd-final-draft/) (**Breachpoint** final GDD, Markdown + styled PDF) · [#3](assignments/03-agent-crew/) (**the runnable agent crew** — 4 agents producing Breachpoint's `DT_Weapons.csv` + `arena_manifest.json`, with replay mode and Mermaid architecture) |
| **`docs/course/`** | Course-wide references — [the GDD writing standard](docs/course/GDD-FORMAT-GUIDE.md) · [Game Developers Conference overview](docs/course/GDC-overview.md) |
| **`docs/method/`** | The crew methodology — [engineering disciplines D1–D8](docs/method/ENGINEERING-DISCIPLINES.md) · [crew operations plan](docs/method/CREW-OPERATIONS.md) (topology, model economics, metrics — v2-synced) · [researched best-practice validation](docs/method/ARCHITECTURE-VALIDATION.md) · [roster design history](docs/method/CREW-ROSTER.md) (superseded by [`crew/docs/CREW_MAP.md`](crew/docs/CREW_MAP.md)) |
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

**Coursework: complete.** Assignments [#1](assignments/01-gdd-first-draft/),
[#2](assignments/02-gdd-final-draft/), and [#3](assignments/03-agent-crew/) are all
submitted. **From here this repo is a Breachpoint project, not a course project** — the
crew, the board, and the docs serve the build. The board is
[`crew/docs/tickets/`](crew/docs/tickets/) (BP00–BP14); `assignments/` is a record, not a
workstream.

**M0 — pre-production complete.** ✅
The data crew has **run**: `DT_Weapons.csv` and `arena_manifest.json` are authored, reviewed,
and verifier-proven ([BP13 Log](crew/docs/tickets/TICKET_BP13_DATA_CREW.md#log)).

Next: create the game repo, copy `crew/` contents to its root, then in a Claude terminal
(everything below needs UE 5.8 installed — it cannot run in a cloud container):

```
/tickets TICKET_BP01_SKELETON_INPUT   # the first code pickup
/tickets TICKET_BP13_DATA_CREW        # step 6 only: import the landed data
/tickets TICKET_BP14_ENGINE_BRIDGE    # wire the crew to the real ladder
```

The milestone ladder from there: M1 Foundry → **M2 Golden Triangle (fun gate)** → M3
Playground → **M4 First Match (go/no-go)** → M5 Two Boxes → **M6 Steam Demo Live**.
