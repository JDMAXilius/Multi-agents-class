# Assignment #5 — Goal-Oriented Coding Agent

**Course:** Multi-Agent AI for Game Development · **Student:** Juan Diego Lugo
**Game:** **BREACHPOINT** — a Halo-inspired 4v4 arena FPS (UE 5.8, pure native C++,
Gameplay Ability System, Steam listen server).

An agent that reads BREACHPOINT's GDD, scans its source, finds what the design promises and
the code does not have, decides which gap to close first — and writes the C++.

```bash
python3 agent.py            # replay the committed run — stdlib only, no API key
python3 agent.py --rank     # just the reasoning: gaps + scores, no model call at all
python3 agent.py --live     # real model call for the code-generation step
```

---

## The one design decision

**It decides deterministically and writes with a model.** Stages 1–4 — read the GDD, scan
source, detect gaps, rank them — are plain Python with **no model call anywhere**. Only
stage 5, writing the C++, calls a model.

The assignment says the objective is *"the reasoning layer; how the agent decides what to
build and in what order."* A reasoning layer you cannot audit is not one you understand, so
every scoring term is printed for every candidate and stored in `output/ranking.json`. The
selection can be recomputed by hand from that file.

`python3 agent.py --rank` reaches the decision and stops before any model is involved.

---

## It does not depend on the live game

The agent reads and writes **`./project/`** — a pinned copy of BREACHPOINT frozen at commit
`13a3882` (`project/PROVENANCE.md`), never the live tree.

That is not tidiness. The game repo changes daily; an agent whose inputs move under it gives
a different answer every run, and every claim in this README would rot within a day of being
written. Freezing makes the run reproducible: `agent.py` on this folder gives the same
perception, the same gaps and the same ranking today as when this was written.

`agent.py` enforces it rather than promising it:

```python
def guard_path(p: Path):
    resolved = Path(p).resolve()
    if PROJECT.resolve() not in resolved.parents and resolved != PROJECT.resolve():
        sys.exit(f"error: refusing to touch {resolved} — outside {PROJECT}")
```

The freeze is one script (`./freeze_project.sh`) and carries the real GDD, all **110 `BR*.h`
headers**, 4 subsystem bodies as style exemplars, and the **7 data tables**. Bodies are
excluded because whether a unit exists is decided by its declaration; the tables are included
because they are how the agent answers *"are this feature's inputs already on disk?"*

---

## The five requirements

### 1. Read the GDD

Features come from the GDD's own **§5.1 Shipped Scope** — the paragraph that ends *"Nothing
else."* It is the project's definitive statement of what the game is, and it is
semicolon-delimited, so it parses **exactly** rather than approximately. Headings would have
been easier and worse: §2.7 *"Information Without Radar"* is a design consequence, not a
feature.

**13 features.**

### 2. Scan the codebase

Every `.h` and `.cpp` in the frozen copy → **114 files, 303 declarations, 7 data tables**.

### 3. Detect gaps

A feature is BUILT when something *does* it, not when something *describes* it. UE's prefixes
make that checkable: `U`/`A`/`I` are runtime classes; `F`/`E` are structs and enums, which in
this project are overwhelmingly DataTable row types — **inputs** to a feature, never the
feature.

**11 built · 2 missing.** Every verdict prints its evidence:

```
built   Grappleshot                        grappleshot -> UBREnvQueryContext_GrapplePerches
built   canned medals + killfeed           killfeed    -> UBRKillfeed (UI/HUD/BRKillfeed.h)
MISSING Spotter agent with canned fallback no U/A/I class matches ['spotter', 'agent', …]
                                           — but its data exists (FBRSpotterLineRow, DT_SpotterLines.csv)
MISSING 1 three-level arena map            no U/A/I class matches ['arena']
```

### 4. Prioritise

Four terms, all printed, weights in the open so the ranking can be argued with:

```
  score  central  inputs  integ breadth  feature
  106.0     40.0    50.0    0.0    16.0  Spotter agent with canned fallback
   16.0     12.0     0.0    0.0     4.0  1 three-level arena map
```

| Term | What it measures | Weight |
|---|---|---:|
| `central` | how often the whole GDD returns to this feature | ×1 |
| `inputs_ready` | data tables and row structs for it already on disk | **×25** |
| `integration` | existing classes it would plug into | ×2 |
| `breadth` | distinct nouns describing it — a system, or a single asset | ×4 |

**`inputs_ready` is weighted hardest on purpose.** The previous attempt at this assignment
(assessed in `breachpoint/docs/ASSIGNMENT-5.md`) selected a unit whose inputs were not on
disk, stalled at the project's owner-path law, and **produced no code at all**. That failure
is now a scored term rather than a lesson.

### 5. Generate code

The agent wrote **`UBRSpotterSubsystem`** — 7,649 + 8,388 characters of C++ — into
`project/Source/Breachpoint/Telemetry/`.

---

## What the agent built, and why it picked it

**It picked the Spotter because it was the only gap that was both real and startable.**

GDD §3.3 specifies a runtime Spotter agent in detail — one coach line per player at match end,
≤ 12 calls/match, 3 s timeout, *"canned-line DataTable fallback shipped in the build"*, and
*"no connectivity ⇒ the game is identical minus flavor."* The GDD returns to it constantly
(`central` 40, the highest of any gap). Its inputs already ship: `FBRSpotterLineRow` in
`BRDataRows.h` and `DT_SpotterLines.csv`, 63 authored lines. **No class reads them.**

The runner-up, *"1 three-level arena map"*, scored 16 — correctly, because it is a level asset
(Tier 4 in the project's authoring matrix), it has no data inputs, and nothing would plug into
it. The gap between 106 and 16 is not close, and the terms show why.

Two details worth naming from the generated code, both visible in
`output/build_report.json`:

- **It reused an existing API instead of inventing one.** Finalized telemetry carries
  anonymised `PlayerKey` integers, not `APlayerState*`. Rather than adding a reverse-lookup,
  it walks the live `PlayerArray` and calls the already-public
  `UBRTelemetrySubsystem::GetPlayerKey`. I verified that method exists and is public —
  `BRTelemetrySubsystem.h:156`, under `public:` at line 145.
- **It refused to invent the parts that do not exist.** There is no HTTP client anywhere in
  this codebase, so `TryRequestModelLine` is an honest stub gated behind
  `bEnableModelSpotter = false`, and the canned path — which is what actually ships — is the
  one every caller exercises. Notable-event classification (spree streaks, rocket multi-kills)
  is left as a documented TODO because the medal evaluator it needs is not built in C++ yet.
  The prompt asks for exactly this: *"an invented call site is worse than an honest gap."*

---

## Were you able to run it in your game?

**No, and the honest answer has three parts.**

**It has not been compiled.** There is no Unreal Engine in this container, so no `.h`/`.cpp`
here has been through UBT. On the project's honesty ladder this is **rung 0** — *compiles ≠
works* is the first rung, and this has not cleared it.

**It has not been placed in the game.** The code is in the frozen copy, which is exactly what
you asked for — the agent must not write to a tree that changes daily. Porting
`BRSpotterSubsystem.h/.cpp` into `breachpoint/Source/Breachpoint/Telemetry/` is a deliberate
manual step under a ticket with an owner-path claim, not something this agent does behind your
back.

**What it *has* cleared** is everything checkable without an engine, and the last check is the
one that matters:

```
6. VERIFY — structural checks (NOT a compile; no engine here)
   pass: pragma once · generated.h · UCLASS · UBRSpotterSubsystem declared ·
         self-include · no Tick · no ConstructorHelpers
   pass: all 10 project types it references are real
         (ABRGameState, EBRSpotterAudience, FBRKillFeedEntry, FBRMatchTelemetryRecord…)

7. RE-SCAN — gap 'Spotter agent with canned fallback' now CLOSED
```

**The undefined-symbol check is the valuable one.** A model writing against an unfamiliar
codebase invents a helper that *should* exist far more readily than it gets a brace wrong, and
an invented type is a compile error every structural check sails straight past. All 10 `BR`
types this file names resolve to real definitions elsewhere in the codebase.

Step 7 closes the loop: the agent re-scans its own output and confirms the gap it selected is
no longer detectable. That is the agent grading itself with the same perception code it
started with.

### That check found two bugs — in the checker, not the code

Worth recording, because both would have read as defects in the generated file:

1. It flagged `FBRSpotterLineReadySignature` as undefined. It is declared by
   `DECLARE_MULTICAST_DELEGATE_ThreeParams` **in the generated header itself** — a checker
   reading only `class`/`struct` never sees a macro-declared name.
2. It then flagged `UBRSpotterSubsystem` — the file's own class. The declaration is
   `class BREACHPOINT_API UBRSpotterSubsystem`, and the regex was capturing the export macro
   instead of the class name.

Both are fixed. The lesson is the one from Assignment #4's §6: a verifier that has only ever
been observed passing is not evidence of enforcement — you learn what a check is really
asserting the first time it fires.

---

## Honest limits

- **Rung 0.** Not compiled, not run, not in the game. See above.
- **Gap detection is name-based.** It matches GDD nouns against class names with a shared-run
  rule. It is right on all 13 features here and I verified that independently, but a feature
  the codebase implements under an unrelated name would read as missing. A stronger version
  would match against the architecture manifest's declared unit list; that document's row
  format is not uniform, and parsing it was most of why the previous attempt's agent ran to
  47 KB.
- **`1 three-level arena map` is a false gap.** It is a level asset, not a class, so no `U`/`A`
  class will ever match it. It ranks last, which is the right outcome, but the agent does not
  *know* it is asset-tier — it just scores it low for having no inputs and no integration.
- **One choice is delegated to the model:** which folder a new unit belongs in. The agent
  constrains it to folders that already exist and validates the answer, and records the
  reasoning. *Which* feature to build — the thing the assignment is actually about — is
  decided before the model is called.
- **Cost:** one call, 61,845 output tokens, **$2.49**.

---

## Files

```
agent.py            the agent — read GDD · scan · gap · rank · generate · verify   (652 lines)
freeze_project.sh   builds the pinned target and writes its provenance             (103 lines)
recording.json      the committed live run, so replay needs no API key
project/            THE FROZEN TARGET — pinned at 13a3882, never the live tree
  GDD.md              the real vertical-slice GDD
  PROVENANCE.md       what was copied, from which commit, and why
  Source/             110 BR*.h + 4 subsystem bodies
  Content/Data/       7 data tables
  Source/Breachpoint/Telemetry/BRSpotterSubsystem.h    ← WRITTEN BY THE AGENT
  Source/Breachpoint/Telemetry/BRSpotterSubsystem.cpp  ← WRITTEN BY THE AGENT
output/
  perception.json     13 features, 303 declarations, every built/missing verdict + evidence
  ranking.json        every candidate, every scoring term, the weights, the selection
  build_report.json   what it wrote, the model's rationale, verify results, re-scan result
README.md           this file
```

**Replay is not a printout.** Reading, scanning, gap detection, ranking, path guarding,
verification and the re-scan all execute for real on `python3 agent.py`; only the generated
C++ comes from `recording.json`. Delete the two `BRSpotterSubsystem` files and re-run — they
come back, and the gap closes again.
