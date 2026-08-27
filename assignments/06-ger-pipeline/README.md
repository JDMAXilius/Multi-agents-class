# Assignment #6 — GER Pipeline

**Course:** Multi-Agent AI for Game Development · **Student:** Juan Diego Lugo
**Game:** **BREACHPOINT** — a Halo-inspired 4v4 arena FPS (UE 5.8, pure native C++,
Gameplay Ability System, Steam listen server).

A Generator → Evaluator → Refiner loop with a Circuit Breaker, writing announcer lines for
team events and enforcing one rule from BREACHPOINT's GDD.

```bash
python3 ger.py            # replay the committed run — stdlib only, no API key
python3 ger.py --rules    # the evaluator's rules, no model call at all
python3 ger.py --live     # real model calls, re-records
```

---

## Pre-Build Declaration

Written before any pipeline code, as the assignment requires. Full text in
**[`PRE-BUILD-DECLARATION.txt`](PRE-BUILD-DECLARATION.txt)** (127 words).

**1. What content does my game generate manually, inconsistently, or not at all?**
Canned announcer lines for team events. BREACHPOINT became a team game this week — the
founder ruled TEAMS the default — but `DT_SpotterLines` has 23 triggers and not one is about
a teammate. **A teammate dying is silent.**

**2. What specific rule from my GDD must every piece satisfy?**
GDD §3.3: the canned table is *"shipped in the build"* and *"No connectivity ⇒ the game is
identical minus flavor."* Every line must therefore **stand alone** — no player name, no
score, no count, nothing the offline path cannot know. The same section caps an event line at
18 words.

**3. What does a failure look like, concretely?**
The announcer says *"Reyes is down"* or *"Two teammates left"* from a table that knows
neither. Offline it renders empty or stale, and the player hears the match misreported — in a
game that cut radar, so audio **is** the awareness system.

---

## The pipeline

```
                    ┌──────────────────────────────────────────┐
                    │  GENERATOR                               │
   GDD §3.3 ───────▶│  3 variants per trigger, shown the       │
   63 shipped lines │  house voice from DT_SpotterLines        │
   (the exemplars)  └────────────────┬─────────────────────────┘
                                     │
                                     ▼
                    ┌──────────────────────────────────────────┐
                    │  EVALUATOR  — deterministic, 5 rules,    │
                    │  every one traceable to the GDD or the   │
                    │  shipped table. No model involved.       │
                    └────────┬──────────────────────┬──────────┘
                        pass │                      │ fail
                             ▼                      ▼
                        ACCEPTED         ┌─────────────────────┐
                                         │  REFINER            │
                                         │  given the exact    │
                                         │  violations, rewrite│
                                         └──────────┬──────────┘
                                                    │
                              ┌─────────────────────┴─────────┐
                              │  attempt < 3 ──▶ back to the  │
                              │                  EVALUATOR    │
                              │  attempt = 3 ──▶ CIRCUIT      │
                              │                  BREAKER      │
                              └───────────────────────────────┘
                                                    │
                                                    ▼
                                    ESCALATED to the human lead,
                                    with the full attempt history
```

**The Evaluator is deterministic and makes no model call.** `python3 ger.py --rules` prints
all five rules and exits. A rule that needs a model to decide is a rule the pipeline cannot
be held to.

| # | Rule | Source |
|---|---|---|
| **1** | **STANDS_ALONE** — no `{placeholders}`, no digits, no spelled-out counts, no player names | **GDD §3.3** |
| 2 | LENGTH — ≤ 18 words | GDD §3.3 (`spotter_line (≤ 18 words)`) |
| 3 | CANON — no radar, motion tracker, plasma, vehicles, flag modes | GDD §6 cut list, §5.1 |
| 4 | VOICE — no exclamation marks | measured: 0 of the 63 shipped lines have one |
| 5 | DISTINCT — no variant duplicating one already accepted for that trigger | the shipped table's 3-variants-per-trigger pattern |

Rule 1 is the one the assignment asks for, and it is **deliberately not a validity check**.
`"Reyes is down."` is perfectly good text — right register, right length, correctly spelled —
and wrong for this game, for a reason the GDD states in one sentence.

### Detecting a player name without a dictionary

Two approaches failed before one worked, and both failures are recorded in the code:

- Flagging capitalised words after position 0 **misses the commonest form of all** — *"Reyes
  is down."* puts the name first. My own unit test caught this: the headline example from my
  declaration passed.
- Flagging capitalised words absent from the GDD's vocabulary flags **"Teammate", "Squad",
  "Regroup"** — legitimate announcer words that simply never appear in a design document.

What survives is **grammatical shape**: subject-of-a-verb, agent-after-*"by"*, possessive —
each exempted when the word is BREACHPOINT's own vocabulary. `"Teammate down."` is silent;
`"Reyes is down."` is not. **8 of 8 planted failures caught, 7 of 8 good lines clean.**

---

## The committed run

```
accepted             13
rejected by rule      6
fixed by the refiner  0
escalated by breaker  2
```

Landed: **[`output/DT_SpotterLines_TeamEvents.csv`](output/DT_SpotterLines_TeamEvents.csv)** —
13 lines across 5 team triggers. Verified after landing: no duplicate variants, max 4 words
against a cap of 18, and **zero rows fail the evaluator that produced them**.

Two of the five slots were chosen **because they tempt the rule** — a line about the last
survivor wants a count, a line about being avenged wants a name. An evaluator that only ever
sees safe input demonstrates nothing.

### The Circuit Breaker earned its place

`Team.LastAlive` did not fail once. It **cycled**:

```
attempt 1 (generate)  'Last one standing.'
attempt 2 (refine)    'Last one alive.'
attempt 3 (refine)    'Last one standing.'    ← back to where it started
BREAKER tripped — escalating to the human lead
```

Attempt 3 returned attempt 1's line. The refiner could not escape the word *"one"* because
the **concept** of being last invites it. Without a breaker that loop runs until the budget
does. Both escalations carry their full history into
[`output/run_report.json`](output/run_report.json), so the human inherits the attempts rather
than a bare failure.

---

## Did the pipeline catch something I would have missed?

**Yes — and not the escalation, which I planted.** This one:

In the previous run the refiner fixed `'All four down.'` (a count violation) by writing
`'Team wipe.'` — which was **already accepted for that same trigger**. The table landed with
three variants for `Team.Wipe.Enemy` and only **two distinct**. Every line passed every rule.
Watching the run told me nothing; I found it by checking the landed CSV afterwards.

The shipped table gives every trigger three variants for one reason: a player who earns the
same event twice should not hear the identical clip. A third of that was silently gone.

So rule 5, **DISTINCT**, exists — and the comment in `ger.py` records why:

> *A refiner's cheapest escape from any rule is to collapse onto the safest line it has
> already seen, so the loop has to forbid that explicitly or it silently trades variety for
> compliance.*

Re-running with the rule changed the outcome. `Team.Wipe.Enemy` now goes: `'All four down.'`
rejected → refiner writes `'Team wipe.'` → **rejected as a duplicate** → refiner returns to
`'All four down.'` → breaker trips. **A silent defect became a visible escalation.** That is
the pipeline catching something I would have shipped.

### Two robustness failures the loop had to survive

Both hit live runs before either reached the tempting slots:

1. The refiner returned **two JSON objects back to back**. Parsing first-brace-to-last-brace
   gave `Extra data` and killed the run. `raw_decode` now takes the first complete value.
2. The generator returned `{"lines": ["Revenge kill.", "Payback.", "Avenged."}` — a missing
   bracket, genuinely invalid. The parser was right to refuse it; **the pipeline was wrong to
   die.** The engine now feeds the parse error back and asks once more, and a second failure
   raises `ParseFailure`, which the slot loop escalates through the **circuit breaker**.

That second fix closed a gap in how I had read the rubric. *"Escalates when the loop can't
self-correct"* is not only about rule violations — a slot returning garbage twice cannot
self-correct either, and it should cost that slot rather than the run. The malformed reply
**recurs at the same slot on every run**, so it is reproducible behaviour, not a fluke, and
the recovery is visible in the committed log.

The recording also now saves after **every** call. The first crash threw away two slots of
paid calls because it was written only on success.

---

## Honest limits

- **`'Last one standing.'` is arguably a false positive.** "One" there is idiom, not a live
  count. I kept the rule strict on purpose: a false positive costs one refine cycle, a false
  negative ships a line that misreports the match. The consequence is that the breaker
  escalated the one case where my own rule is debatable — which, on reflection, is exactly
  what escalation is for. A human decides whether that idiom counts.
- **The name detector is grammatical, not semantic.** It catches the realistic forms. A name
  used as a bare noun phrase — `"Reyes."` alone — would pass.
- **Nothing here has been imported into the game.** These are CSV rows on disk. Importing
  them into `Content/Data/DT_SpotterLines.csv` is a separate step with an asset lock, and the
  new `Team.*` triggers need code to raise them. Rung 0 on the project's honesty ladder.
- **Cost:** the committed run is 4 slots' worth of calls plus retries; earlier runs were lost
  to the two crashes above before the incremental save existed.

---

## Files

```
PRE-BUILD-DECLARATION.txt   the three answers, written before any code   ← deliverable
ger.py                      Generator · Evaluator · Refiner · Circuit Breaker
recording.json              the committed live run, so replay needs no API key
output/
  DT_SpotterLines_TeamEvents.csv   13 accepted lines               ← deliverable
  run_report.json                  every attempt, every violation, both escalations
  rules.txt                        the evaluator's rules, as printed by --rules
README.md                   this file
```

**Replay is not a printout.** The Evaluator, the loop, the breaker and the CSV assembly all
execute for real on `python3 ger.py`; only the model replies come from `recording.json`.
Delete `output/` and re-run — it comes back, escalations included.
