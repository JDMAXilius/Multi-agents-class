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
| **`breachpoint/`** | **The active project.** [Vertical-slice GDD](breachpoint/BREACHPOINT-GDD-VERTICAL-SLICE.md) (the six-week build) · [full-concept GDD](breachpoint/BREACHPOINT-GDD-FULL-CONCEPT.md) (Phase-2 target) · [Architecture](breachpoint/BREACHPOINT-ARCHITECTURE.md) (42 class-units, file-by-file) · [Roadmap](breachpoint/BREACHPOINT-ROADMAP.md) (six pods, six milestones) · [Quality Bars](breachpoint/BREACHPOINT-QUALITY-BARS.md) (budgets, DoD, playtest protocol, ship checklist) |
| **`crew/`** | **The drop-in agent studio** — copy its contents to the game repo root. 11 agents (7 builders, 2 reviewers, 2 curators), 2 skills (`game-lead`, `tickets`), 6 contracts (netcode, data-and-assets, testing, animation, online-services, **gas-purity**), and the executable board: [tickets BP00–BP12](crew/docs/tickets/) covering Week 1 → ship. Start here: [`crew/README.md`](crew/README.md) |
| **`assignments/`** | Course submissions of record — [Assignment #1](assignments/01-gdd-first-draft/) and [#2](assignments/02-gdd-final-draft/) (*Slash Roller: Arena* GDDs, Markdown + styled PDFs) |
| **`docs/course/`** | Course-wide references — [the GDD writing standard](docs/course/GDD-FORMAT-GUIDE.md) · [Game Developers Conference overview](docs/course/GDC-overview.md) |
| **`docs/method/`** | The crew methodology — [engineering disciplines D1–D8](docs/method/ENGINEERING-DISCIPLINES.md) · [crew roster & agent kinds](docs/method/CREW-ROSTER.md) · [researched best-practice validation](docs/method/ARCHITECTURE-VALIDATION.md) · [crew operations plan](docs/method/CREW-OPERATIONS.md) |
| **`docs/decisions/`** | Decision records — [the A/B scope comparison](docs/decisions/SCOPE-COMPARISON.md) that chose Breachpoint's vertical slice (Option C) |
| **`archive/`** | Superseded work, kept honestly — the [Slash Roller: Arena tech spec](archive/slashroller/) (pre-pivot capstone) · the [Gauntlet single-player draft](archive/concept-gauntlet/) (its wave/AI-Director design feeds Phase-2 Firefight) |

## How the project got here

1. **Assignments** — the capstone GDD was drafted and revised as *Slash Roller: Arena*
   (third-person melee deathmatch on the studio's shipped GAS core), stress-tested by an
   agent review crew per the Class-03 method.
2. **The crew was formulated** — disciplines D1–D8 were drawn as real ownership
   boundaries, validated against industry practice, and minted into the 12-agent kit with
   binding contracts.
3. **The scope comparison** — a second concept (Halo-inspired FPS) was designed and both
   were costed line-by-line. Verdict: full Concept B doesn't fit six weeks; **Option C —
   the Breachpoint vertical slice — does**, and it was chosen as the build.
4. **Pre-production completed (M0)** — GDD, architecture (with GAS-purity law, soft-ref
   modularity, the owned input layer), roadmap (six parallel pods, Halo-Studios-style),
   quality bars, and all 13 tickets.

## Status & next action

**M0 — pre-production complete.** ✅
Next: create the game repo, copy `crew/` contents to its root, then in a Claude terminal:

```
/tickets TICKET_BP01_SKELETON_INPUT
```

The milestone ladder from there: M1 Foundry → **M2 Golden Triangle (fun gate)** → M3
Playground → **M4 First Match (go/no-go)** → M5 Two Boxes → **M6 Steam Demo Live**.
