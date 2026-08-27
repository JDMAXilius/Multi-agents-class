# Assignment #7 — Style Guide Agent

**Game:** BREACHPOINT — 4v4 team arena FPS, Unreal Engine 5.8, pure native C++.
**The loop:** Generator → Evaluator → Refiner → (loop) → Circuit Breaker.
**What it enforces:** the game's actual aesthetic and narrative rules, codified
with citations in [`STYLE-GUIDE.md`](STYLE-GUIDE.md) — the first document in
the project to gather them in one place.

```
                       ┌────────────────────────────────────────────┐
                       │  arena_manifest.json — 7 real landmarks    │
                       │  (name + the level designer's raw notes)   │
                       └───────────────┬────────────────────────────┘
                                       ▼
  ┌────────────┐   card    ┌───────────────────────┐   violations   ┌──────────┐
  │ GENERATOR  │ ────────► │ EVALUATOR             │ ─────────────► │ REFINER  │
  │ (model)    │           │ deterministic Python: │                │ (model)  │
  └────────────┘           │ STYLE-GUIDE.md as     │ ◄───────────── └──────────┘
                           │ executable rules      │   rewritten card
                           └──────────┬────────────┘
                             clean │  │ attempt ≥ 3
                                   ▼  ▼
                            accepted  CIRCUIT BREAKER → escalate to the human
                                      lead with the slot's full history
```

Run it:

```
python3 style_agent.py             # replay the committed run — stdlib only
python3 style_agent.py --rules     # print the executable style guide
python3 style_agent.py --selftest  # every rule fires + the breaker trips, no model
python3 style_agent.py --live      # real model calls (re-records recording.json)
./make_submission.sh               # verified standalone zip
```

## What it generates, and why this content

**Arena intel cards** — the map-screen blurb for each of the seven landmarks
the shipped arena actually has. The arena ships callouts and geometry but zero
player-facing prose. The generator is handed the level designer's *raw
manifest notes* — walls of coordinates and version-history engineering — and
asked for something evocative a player reads in the seconds before a match.
That tension is the deliberate temptation: the model must invent atmosphere
without inventing *canon*, and the Evaluator is what stands between the two.

## The rules — extracted, not invented (full text: STYLE-GUIDE.md)

| Rule | What fails | Source |
|---|---|---|
| 1 CANON_PLACES | any proper noun that isn't one of the 7 landmarks or the sandbox | `arena_manifest.json` `landmarks[]` |
| 2 CANON_ARSENAL | sniper, shotgun, SMG, sword, needler, laser, turret | GDD §2.4 — "Weapons — three" |
| 3 CUT_SYSTEMS | radar, motion tracker, plasma, vehicles, flag modes | GDD §6 cut table, §2.7, §5.1 |
| 4 CANON_NUMBERS | any digit that isn't a real tuning value (a "45 s" rocket is a hallucination; it's 90) | GDD Appendix A + manifest geometry |
| 5 VOICE | "!" or first person — **measured**: 0 of 63 spotter lines and 0 UI LOCTEXTs have either | `DT_SpotterLines.csv`, `UI/*.cpp` |
| 6 FORMAT | > 2 sentences or > 28 words | STYLE-GUIDE.md's own slot spec — the one self-declared rule, labeled as such |
| + NO_FICTION | years, backstory lexicon ("built by", war, ancient, corporation…) | measured: the GDD contains zero fiction |

This is deliberately not generic validation, and rule 4 shows why a *style
guide* is a per-surface document: Assignment #6 banned **all** digits, because
a canned audio line cannot know live state (GDD §3.3). Map copy is the
opposite — the 90-second rocket timer is static canon and *should* be printed;
only a **wrong** number fails. Same game, same canon, different surface,
different rule.

## The committed run (recording.json — real model calls, replayed verbatim)

`--live` was run from this repo's cloud session via the `claude` CLI
(model: `claude-sonnet-5`); every exchange is recorded and the default
invocation replays them deterministically.

| | |
|---|---|
| landmarks | 7 |
| accepted | **7/7** |
| rejected by rule | 5 first-pass cards |
| fixed by the refiner | 5 (all within one refinement) |
| escalated by breaker | 0 in this run — see below |

**Did the Evaluator catch anything real? Five of seven first drafts failed:**

- Every barricade/stack card leaked **`SP1`–`SP4` spawn-point IDs** straight
  from the manifest's engineering prose into player-facing copy —
  CANON_PLACES caught all four (and CANON_NUMBERS independently flagged the
  digits). The refiner replaced them with the landmark's own name each time.
- The Core's card invented **"Core Top"** — a place the manifest does not
  name — exactly the invented-proper-noun failure the rule exists for, on the
  slot pre-declared as the temptation (`TEMPTATIONS` in the script).
- The same card ran 36 words; FORMAT caught it alongside.

The pre-declared **sniper-perch temptation on The Gantry did not land** — the
model wrote "Grapple-only, height 8" and passed first try. Declared honestly:
a temptation is a bet, and this one the generator won.

## The circuit breaker — proven deterministically, not by luck

In this run the model fixed everything within the attempt budget, so the
recording alone never shows the breaker trip. Two things cover it:

1. `--selftest` drives the **real `run_slot` loop** with a scripted refiner
   that never complies, and asserts the breaker trips after exactly 3
   attempts with the full history in the escalation record.
   `make_submission.sh` refuses to zip unless this passes.
2. It *did* trip live during development: the first live run escalated
   **Mezzanine Catwalks** after the refiner trimmed 32 → 31 → 29 words and
   never reached the 28 cap — the classic converging-too-slowly failure the
   breaker exists for. That run's recording was superseded (below), so it is
   reported here as development history, not as a committed artifact.

## The bug the first live run found — in the Evaluator, not the model

The first `--live` run flagged **"the Core's mid-level terrace"** because
`Core's` failed the vocabulary lookup — the possessive of a canon landmark
was read as an invented noun. That is an Evaluator false positive: rejecting
the map's own landmark grammar would sand every card down to nothing. Fixed
(the lookup now uses the stem before the apostrophe), pinned with two
self-test cases (`Core's` clean, `Vanguard's` still fails), and the live run
re-recorded against the fixed evaluator. The committed recording is run 2;
run 1's escalation story above is from its logs. A pipeline whose evaluator
is deterministic can *be wrong deterministically* — which is why the rules
are code with a self-test and not a prompt.

## Honest limits

- **Sentence-start lore slips rule 1.** A capitalized invention that *opens*
  a sentence can't be told from an ordinary word by case alone; NO_FICTION's
  lexicon is the second net, and it is a word list, not comprehension.
- **The digit `1` violation on `SP1` is collateral** — the token was already
  fatal under CANON_PLACES; the number rule flagging its split-off digit is
  noise in the log, not a wrong verdict.
- **VOICE is two measurable proxies** (no "!", no first person) for a house
  voice that has more dimensions than that. The measured claims are exact;
  the coverage claim is not total.
- **Replay is faithful, not re-creative:** the default run replays recorded
  model output; only `--live` demonstrates fresh self-correction.

## Files

```
style_agent.py            the pipeline (stdlib; --live uses ANTHROPIC_API_KEY
                          + anthropic SDK, or the claude CLI)
STYLE-GUIDE.md            the codified rules, every one cited to a project file
PRE-BUILD-DECLARATION.txt what was declared before building
recording.json            the committed live run (model: claude-sonnet-5)
output/ArenaIntelCards.csv   the 7 landed cards
output/run_report.json       full per-slot history, machine-readable
output/run_log.txt           the run, as printed
output/rules.txt             --rules output
make_submission.sh        staged, self-verifying zip build
```
