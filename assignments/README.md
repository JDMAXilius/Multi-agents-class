# Assignments — index and submission guide

**Course:** Multi-Agent AI for Game Development (ELVTR) · **Student:** Juan Diego Lugo
**Capstone:** **BREACHPOINT** — a Halo-inspired 4v4 arena FPS (UE 5.8, pure native C++,
Gameplay Ability System, Steam listen server).

Every assignment targets the same capstone. Nothing here uses placeholder lore or a toy
project: assignments #3 through #6 all read BREACHPOINT's real design documents and its real
source, and the artifacts they produce are the game's own data and code.

**Repo:** `https://github.com/JDMAXilius/Multi-agents-class`
**This folder:** `assignments/` at the repo root.

---

## Where everything is

| # | Assignment | Folder | Entry point |
|---|---|---|---|
| 1 | GDD — first draft | [`01-gdd-first-draft/`](01-gdd-first-draft/) | [`GDD.md`](01-gdd-first-draft/GDD.md) · [PDF](01-gdd-first-draft/Juan_Diego_Lugo_GDD_First_Draft.pdf) |
| 2 | GDD — final draft | [`02-gdd-final-draft/`](02-gdd-final-draft/) | [`BREACHPOINT-GDD.md`](02-gdd-final-draft/BREACHPOINT-GDD.md) · [PDF](02-gdd-final-draft/BREACHPOINT_Final_GDD_Juan_Diego_Lugo.pdf) |
| 3 | Build an Agent Crew | [`03-agent-crew/`](03-agent-crew/) | [`README.md`](03-agent-crew/README.md) → `run_crew.py` |
| 4 | Dynamic Content Pipeline | [`04-content-pipeline/`](04-content-pipeline/) | [`README.md`](04-content-pipeline/README.md) → `run_pipeline.py` |
| 5 | Goal-Oriented Coding Agent | [`05-goal-oriented-agent/`](05-goal-oriented-agent/) | [`README.md`](05-goal-oriented-agent/README.md) → `agent.py` |
| 6 | GER Pipeline | [`06-ger-pipeline/`](06-ger-pipeline/) | [`README.md`](06-ger-pipeline/README.md) → `ger.py` |

**Read the folder's own `README.md` first** — each one is the graded write-up for that
assignment. This file is only the map.

---

## Running the four that are code

All four are **Python 3, standard library only**. No `pip install`, no API key, no network.
Each ships a recorded run so the pipeline executes end-to-end on any machine:

```bash
cd assignments/03-agent-crew     && python3 run_crew.py
cd assignments/04-content-pipeline && python3 run_pipeline.py
cd assignments/05-goal-oriented-agent && python3 agent.py
cd assignments/06-ger-pipeline       && python3 ger.py
```

**Replay is not a printout.** In all four, everything except the model responses executes for
real — retrieval, validation gates, scoring, file writing. Only the LLM calls are served from
`recording.json`. Delete the outputs and re-run; they come back.

To make real model calls instead, add `--live` (needs `ANTHROPIC_API_KEY` + `pip install
anthropic`, or the `claude` CLI on PATH).

---

## What each one does, and what to look at

### #3 — Build an Agent Crew · [`03-agent-crew/`](03-agent-crew/)

Four agents (curator → critic → builder → verifier) with deterministic gates between them,
authoring BREACHPOINT's weapon table and arena blockout spec.

| Look at | For |
|---|---|
| [`README.md`](03-agent-crew/README.md) | the crew, the Mermaid architecture diagram, what the critic caught |
| `output/DT_Weapons.csv` · `output/arena_manifest.json` | the game-ready artifacts |
| `recording.json` | 18 recorded agent exchanges, prompts included |

### #4 — Dynamic Content Pipeline · [`04-content-pipeline/`](04-content-pipeline/)

BM25 RAG over the real GDD, shipped DataTables **and shipped C++ headers**; generates a pool
of candidates, ranks them with a critic in JUDGE mode, then attacks the survivors in REFUTER
mode. Drives the project's real crew definitions from `breachpoint/.claude/agents/`.

| Look at | For |
|---|---|
| [`README.md`](04-content-pipeline/README.md) | the full write-up — §6 is the defect the pipeline found in itself |
| `output/gap_report.md` | the four content gaps, **proven from the repo**, not asserted |
| `output/rag_trace.md` | query · retrieved chunk · output, side by side |
| `output/judge_log.md` | the candidate pool, the ranking, what got discarded |
| `output/critic_log.md` | every finding with a computed before/after diff |
| `output/DT_*.csv` | the three landed content tables |

### #5 — Goal-Oriented Coding Agent · [`05-goal-oriented-agent/`](05-goal-oriented-agent/)

Reads the GDD, scans the codebase, detects gaps, ranks them **deterministically**, and writes
the C++ for the top one.

**Grading this one takes one command:**

```bash
cd assignments/05-goal-oriented-agent && ./verify.sh
```

It deletes the agent's output, re-runs it, and checks every assignment requirement against
that fresh run — including recomputing the ranking from its own stored terms to confirm the
selection followed from the printed reasoning. Exit 0 means all checks passed;
[`TICKET_VERIFY.md`](05-goal-oriented-agent/TICKET_VERIFY.md) explains each one and states
plainly what it does *not* prove.

| Look at | For |
|---|---|
| [`TICKET_VERIFY.md`](05-goal-oriented-agent/TICKET_VERIFY.md) | the terminal ticket — what to run and what each check verifies |
| [`README.md`](05-goal-oriented-agent/README.md) | the write-up, incl. the honest "did it run in your game?" |
| `output/ranking.json` | every candidate, every scoring term, the weights, the selection |
| `output/perception.json` | 13 features, 303 declarations, every built/missing verdict |
| `project/Source/Breachpoint/Telemetry/BRSpotterSubsystem.h`/`.cpp` | **the code the agent wrote** |
| `project/PROVENANCE.md` | what the frozen target is and which commit it is pinned to |

`python3 agent.py --rank` shows the whole reasoning layer and **stops before any model call**:

```
  score  central  inputs  integ breadth  feature
  106.0     40.0    50.0    0.0    16.0  Spotter agent with canned fallback
   16.0     12.0     0.0    0.0     4.0  1 three-level arena map
```

### #6 — GER Pipeline · [`06-ger-pipeline/`](06-ger-pipeline/)

Generator → Evaluator → Refiner with a Circuit Breaker, writing announcer lines for team
events and enforcing one rule from GDD §3.3: a canned line must **stand alone**, because the
table ships in the build and plays with no connectivity.

| Look at | For |
|---|---|
| [`PRE-BUILD-DECLARATION.txt`](06-ger-pipeline/PRE-BUILD-DECLARATION.txt) | the three answers, written before any code |
| [`README.md`](06-ger-pipeline/README.md) | the write-up — what the pipeline caught that I would have missed |
| `output/run_report.json` | every attempt, every violation, both escalations with their history |
| `output/DT_SpotterLines_TeamEvents.csv` | the 13 accepted lines |
| `python3 ger.py --rules` | the five rules and their GDD citations, no model call |

The circuit breaker did not fire on a failure — it fired on a **cycle**: the refiner returned
attempt 1's line as attempt 3, because the concept of being the last survivor invites the word
"one" and the rule forbids counts.

---

## One structural difference worth knowing

**#3 and #4 read the live game tree. #5 reads a frozen copy.**

`breachpoint/` changes daily. That is fine for #3 and #4, whose claims are about content and
are re-derived on every run — #4's `gaps.py` re-proves its gaps against whatever is on disk
right now, and re-cites line numbers if code has moved.

It is *not* fine for #5, whose whole output is a judgement about what a codebase does and does
not contain. So `05-goal-oriented-agent/project/` is a **pinned copy** of the game — the real
GDD, 110 `BR*.h` headers, 4 subsystem bodies, 7 data tables — frozen at a commit recorded in
`project/PROVENANCE.md`. `agent.py` refuses at runtime to write anywhere else:

```python
def guard_path(p: Path):
    resolved = Path(p).resolve()
    if PROJECT.resolve() not in resolved.parents and resolved != PROJECT.resolve():
        sys.exit(f"error: refusing to touch {resolved} — outside {PROJECT}")
```

Re-pin with `./freeze_project.sh`, which rewrites `PROVENANCE.md` so the pin is always a fact
on disk rather than something remembered.

---

## Submitting

**Prefer the repo link over a zip.** GitHub renders the Mermaid diagrams and the `<details>`
blocks in `rag_trace.md`, and the grader can browse `output/` without downloading anything.
Link straight to the assignment folder, not the repo root:

```
https://github.com/JDMAXilius/Multi-agents-class/tree/main/assignments/05-goal-oriented-agent
```

**If the form demands a file upload**, all four code assignments build self-testing zips:

```bash
cd assignments/03-agent-crew          && ./make_submission.sh
cd assignments/04-content-pipeline    && ./make_submission.sh
cd assignments/05-goal-oriented-agent && ./make_submission.sh
cd assignments/06-ger-pipeline        && ./make_submission.sh
```

Do not hand-zip any of them. Both scripts bundle things the code needs at runtime (agent
definitions, the knowledge-base mirror) and **refuse to build a package that fails its own
replay**. A hand-zip omits them and the code exits immediately, which is the whole submission.

#5 has no zip script — it is already self-contained, because its target is committed inside
the folder.

Per-assignment submission checklists, where they exist:
[`03-agent-crew/TICKET_SUBMIT.md`](03-agent-crew/TICKET_SUBMIT.md) ·
[`04-content-pipeline/TICKET_SUBMIT.md`](04-content-pipeline/TICKET_SUBMIT.md)

---

## Status

| # | Built | Submitted | Note |
|---|---|---|---|
| 1 | ✅ | ✅ | |
| 2 | ✅ | ✅ | |
| 3 | ✅ | ✅ | landed a day past the deadline, with a note |
| 4 | ✅ | ✅ | |
| 5 | ✅ | **not yet** | due 13 Aug — **needs a short late note on submission** |
| 6 | ✅ | **not yet** | due 18 Aug — **needs a short late note on submission** |

**Honesty note that applies to all of these:** none of the generated C++ or CSV has been
compiled or imported into a running build of the game. The container these ran in has no
Unreal Engine. Where an assignment's README claims something works, it names which rung of the
project's honesty ladder it reached — *compiles ≠ works · PIE ≠ multiplayer · listen ≠
dedicated · editor ≠ packaged* — and #5's answer to "were you able to run it in your game?" is
plainly **no**, with the reasons given.
