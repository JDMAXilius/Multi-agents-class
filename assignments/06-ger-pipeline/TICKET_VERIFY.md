# TICKET — Verify Assignment #6 in a terminal

> **For a terminal session (or the grader).** One command checks every rubric criterion
> against a fresh replay. Nothing here asks anyone to take the README's word for anything:
> each check re-derives its answer from the files the pipeline just produced, and prints
> the evidence it used.

**Assignment:** #6 — GER Pipeline (mandatory) · **Student:** Juan Diego Lugo
**Game:** BREACHPOINT — a 4v4 arena FPS in Unreal Engine 5.8, pure native C++

---

## Requirements

**Python 3.8+. That is the whole list.** No `pip install`, no API key, no network, no game
engine. In the repo the pipeline reads the live GDD and shipped table from `breachpoint/`;
in the zip it falls back to the copies in `game/`. Either layout works.

## Run it

```bash
cd assignments/06-ger-pipeline        # or the unzipped BREACHPOINT-ger-pipeline/
./verify.sh
```

Takes seconds. Exit code is the number of failed checks:

```bash
./verify.sh && echo "ASSIGNMENT PASSES"
```

## What each check verifies, mapped to the rubric

The script **deletes `output/` first**, then replays the committed run, so every result
comes from a fresh execution — not from files shipped in the archive.

| Rubric criterion | Checked by |
|---|---|
| **Working Pipeline /3.0** | Four separate checks, one per stage: the Generator produced accepted content; the Evaluator rejected real lines **with citations**; a Refiner rewrite **passed and landed** (the before → after pair is printed); the Circuit Breaker escalated after ≥ 3 attempts **with its history attached**. |
| **Evaluator Quality /3.0** | The three quoted GDD phrases are grepped out of the real GDD; three *valid-but-wrong* lines (`"Reyes is down."`, `"Two teammates left."`, `"Down 3 to 5."`) must be rejected **with a GDD citation** while three legitimate lines pass — proving it is not a validity check; and `ger.py --rules` must print the rules with `PATH=/nonexistent` and no API key, proving the evaluator never calls a model. |
| **Game Connection /2.0** | The README must name BREACHPOINT, the content type, and the catch; the landed CSV must have a schema **identical to the shipped `DT_SpotterLines.csv`**, use only triggers the table does not already have, and contain zero rows that fail the evaluator that produced them. |
| **ReadMe /2.0** | The declaration must contain all three answers in under 150 words, and all three answers must **also** appear inside the README, as the rubric requires. |
| **Code runs (the 0-gate)** | The replay itself, run with the API key stripped from the environment. |

Section 5 of the output prints what the verification does **not** prove — the CSV is not
imported, and the `Team.*` triggers need game code before any line plays — so a passing run
cannot be mistaken for shipped content.

## Inspect by hand instead

```bash
python3 ger.py --rules      # the five rules and their GDD citations, no model call
python3 ger.py              # full replay: rejections, refinements, escalations
```

| To see | Open |
|---|---|
| The write-up | `README.md` |
| The three declaration answers | `PRE-BUILD-DECLARATION.txt` |
| Every attempt, violation, and escalation with history | `output/run_report.json` |
| The landed lines | `output/DT_SpotterLines_TeamEvents.csv` |

## If a check fails

| Symptom | Cause |
|---|---|
| `python3: command not found` | Python 3.8+ is the one requirement |
| `no recording.json` | incomplete copy — it must sit beside `ger.py` |
| GDD checks fail in the repo | run from `assignments/06-ger-pipeline/`, not the repo root |
| everything fails in a zip | the `game/` folder is missing — use `make_submission.sh`'s zip, never a hand-zip |

## Optional: watch it call a model for real

```bash
python3 ger.py --live       # re-generates, re-records; needs ANTHROPIC_API_KEY or the claude CLI
```

The evaluator's behaviour is identical in both modes — it never calls a model in either.
