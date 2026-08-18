# TICKET — Verify Assignment #5 in a terminal

> **For the grader.** One command checks every requirement. Nothing here asks you to take
> the README's word for anything: each check re-derives its answer from the files the agent
> just produced, and prints the evidence it used.

**Assignment:** #5 — Goal-Oriented Coding Agent · **Student:** Juan Diego Lugo
**Game:** BREACHPOINT — a 4v4 arena FPS in Unreal Engine 5.8, pure native C++

---

## Requirements

**Python 3.8+. That is the whole list.** No `pip install`, no API key, no network, no game
engine. The agent's target is committed inside the folder and the code imports only the
standard library — `verify.sh` checks both of those claims rather than asserting them.

---

## Run it

```bash
cd BREACHPOINT-goal-oriented-agent      # or assignments/05-goal-oriented-agent in the repo
./verify.sh
```

Takes a few seconds. Exit code is the number of failed checks, so this works:

```bash
./verify.sh && echo "ASSIGNMENT PASSES"
```

### Expected output

```
0. Environment
  PASS  python3 available                          Python 3.11.15
  PASS  no API key set                             the run below uses the recorded responses only

1. The agent runs from a clean state
  PASS  generated code cleared                     deleted before the run, so nothing is pre-baked
  PASS  agent.py exits 0                           42 lines of output

2. The five requirements
  PASS  R1  reads the GDD                          13 features, all traceable verbatim to GDD.md §5.1
  PASS  R2  scans the codebase                     114 source files read, 303 declarations indexed
  PASS  R3  detects gaps, with evidence            11 built / 2 missing, every verdict carries evidence
  PASS  R4  prioritises, ranking is reproducible   winner recomputes to spotter…, margin 90.0
  PASS  R5  generates code for a missing feature   UBRSpotterSubsystem written (16,051 bytes)

3. The two deliverables
  PASS  D1  a complete, runnable agent             agent.py imports stdlib only
  PASS  D2  README answers the three questions     all three answered by heading

4. Claims the README makes that should be checkable
  PASS  reasoning layer makes no model call        --rank reaches a selection with no model reachable
  PASS  refuses to write outside the frozen copy   guard_path() rejects a path outside project/
  PASS  the frozen target records its pin          pinned at 13a3882
  PASS  generated code references only real types  all 10 BREACHPOINT types are defined

ALL CHECKS PASSED — every assignment requirement is satisfied by this run.
```

---

## What each check actually does

The script **deletes the agent's output first**, then re-runs the agent, so every result
below comes from a fresh run rather than from files shipped in the archive.

| Check | How it is verified |
|---|---|
| **R1 — Read your GDD** | Every extracted feature must appear **verbatim** in `project/GDD.md` (markdown emphasis normalised). A feature the agent invented would fail. |
| **R2 — Scan the codebase** | The file count the agent reports is compared against the files actually on disk, excluding the two it wrote itself. |
| **R3 — Detect gaps** | There must be both built and missing verdicts — all-missing means the matcher is broken — and **every** verdict must carry evidence. |
| **R4 — Prioritise** | Each candidate's score is **recomputed from its own stored terms**, and the winner is recomputed by taking the maximum. A selection that did not follow from the printed reasoning fails here. |
| **R5 — Generate code** | The `.h`/`.cpp` must exist and be non-trivial, the agent's own structural verification must have landed clean, and its **re-scan must confirm the gap it selected is now closed**. |
| **D1 — Runnable agent** | Required files present, and `agent.py`'s imports are resolved through `importlib.util.find_spec` and classified by spec origin — a third-party import fails the check. (Origin, not `sys.stdlib_module_names`, so the check itself runs on Python 3.8+ like everything else here.) |
| **D2 — README** | Must answer all three required questions (what it built · why that feature · did it run in your game). |

Section 4 tests the claims the write-up makes that a reader would otherwise have to trust:

- **The reasoning layer uses no model.** `agent.py --rank` is re-run with `PATH=/nonexistent`
  and the API key stripped — no model is reachable by any route — and it must still reach a
  selection.
- **It cannot touch the live game.** `guard_path()` is called with `/tmp/escape.h` and must
  refuse.
- **The target cannot drift.** `project/PROVENANCE.md` must record a full commit hash.
- **The generated C++ names no invented types.** Every `UBR*`/`FBR*`/`ABR*` identifier in the
  generated files is looked up against declarations across the whole codebase.

---

## Inspect it by hand

If you would rather read than run:

```bash
python3 agent.py --rank     # the entire reasoning layer, stops before any model call
```

```
  score  central  inputs  integ breadth  feature
  106.0     40.0    50.0    0.0    16.0  Spotter agent with canned fallback
   16.0     12.0     0.0    0.0     4.0  1 three-level arena map

   SELECTED: Spotter agent with canned fallback
   inputs already on disk: FBRSpotterLineRow (…/BRDataRows.h), DT_SpotterLines.csv
```

| To see | Open |
|---|---|
| The full write-up | `README.md` |
| Every feature, verdict and piece of evidence | `output/perception.json` |
| Every candidate, term, weight and the selection | `output/ranking.json` |
| What was written, the rationale, verification | `output/build_report.json` |
| **The code the agent wrote** | `project/Source/Breachpoint/Telemetry/BRSpotterSubsystem.h` / `.cpp` |
| What the frozen target is, and its pin | `project/PROVENANCE.md` |

---

## What this does NOT prove, stated plainly

`verify.sh` prints these too, so they cannot be missed:

- **The C++ has never been compiled.** There is no Unreal Engine in the environment this was
  built in, so nothing has been through UBT.
- **It has not been run in the game.** The generated code is in the pinned copy of the
  project, not in a running build. Porting it into the live tree is a deliberate manual step.

The README's answer to *"Were you able to run it in your game?"* is **no**, with those
reasons. The verification confirms the agent works; it does not claim the game does.

---

## If a check fails

| Symptom | Cause |
|---|---|
| `python3: command not found` | Python 3.8+ is the one requirement. |
| `no recording.json` | The archive is incomplete — the file must sit beside `agent.py`. |
| `R2 … reported N files but M were scannable` | The `project/` folder was modified after the committed run. `./freeze_project.sh` re-pins it, but only inside the full repo. |
| Everything fails at once | Run from **inside** the folder, not its parent — the agent resolves `project/` relative to `agent.py`. |

The script prints the failing assertion and the values that disagreed, so a failure names the
file to look at.

---

## Optional: watch it call a model for real

Not needed for any check above — the committed run is recorded and replays exactly.

```bash
export ANTHROPIC_API_KEY=...      # or have the `claude` CLI on PATH
python3 agent.py --live           # re-generates the C++ and re-records
```

Stages 1–4 are unchanged by this: they never call a model in either mode. Only the code
generation differs, and `recording.json` is rewritten so the next replay matches.
