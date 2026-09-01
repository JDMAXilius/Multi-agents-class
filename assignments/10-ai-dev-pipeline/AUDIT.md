# Pipeline Audit & Cost Analysis

**Student:** Juan Diego Lugo · **Capstone:** BREACHPOINT · **Engine:** Unreal Engine 5.8 (native C++)

**Game concept.** A Halo-inspired 4v4 arena FPS: shields that break and regenerate, a
grappleshot, a rocket that spawns on a timer the whole lobby is fighting over, and bots
that hold the line when a human seat is empty. Server-authoritative, Gameplay Ability
System, zero Blueprint gameplay classes.

---

## 1. Pipeline Production & Functionality

### a. What did the pipeline produce that is in the playable build?

**Fifteen bot callsigns, and they are the names of every bot in the game.** The content
pipeline (assignment #4) reads the game's own tuning table and derives a callsign from a
real number in it — not from a vibe. `Slowdraw` exists because `reaction_ms=500` is the
slowest of the three profiles. `Deadeye` exists because `accuracy_pct=0.65` is the highest.
`Coverlock` because `cover_preference=0.85`. Each row carries the number that justifies it.

They reach the build through `land_in_engine.py`, which writes them into
`ABNGameMode::BotNames`. A player sees them on the scoreboard, in the killfeed, and in
every kill line — eight of the fifteen are drawn per match at the current fill.

**Before this assignment, they were not in the game.** The pipeline's output sat in
`assignments/04-content-pipeline/output/` and the shipped config used seven hand-typed
names (`Marcus`, `Vale`, `Ossian`, `Rook`, `Halcyon`, `Juno`, `Piper`). Finding that gap
is the single most useful thing this audit did — I had been calling #4 a content pipeline
for two weeks while its content had never once reached the executable.

### b. What manual steps remain?

**One, and it is honest to call it a real one: packaging and uploading the build.** The
data lands in the engine with no manual reformatting, but turning the project into a
downloadable executable is `RunUAT BuildCookRun` plus an upload, run by hand.

**And one deliberate non-landing, which matters more than the friction.** The same #4 run
produced twelve spotter (announcer) lines. They are **not** in the build, and I did not
land them, because:

- The live `BreachpointNext` module **has no spotter system at all** — the announcer lives
  in the older `Breachpoint` module, which the playable game does not run.
- All four TriggerIds those lines are keyed to — `Kill.Rocket.Multi`, `Kill.First`,
  `Match.SuddenDeath.Win`, `Kill.SpreeEnder` — **appear nowhere in the source or config.**

Landing them would have added twelve rows to a table nothing reads, keyed to events nothing
fires, and let me claim a bigger integration. That is precisely the slop the rubric warns
about, so the lander refuses to touch them and says why in its own docstring.

### c. What would it take to eliminate them?

- **The packaging step:** a committed `Tools/package.sh` wrapping `RunUAT BuildCookRun`
  with the target, config and output path fixed, plus `butler push` for the upload. Roughly
  half a day, mostly spent on signing so the artifact runs without a security prompt. It is
  automatable; it simply is not automated yet.
- **The spotter lines:** not a pipeline problem, a **game** problem. The lines cannot land
  until `BreachpointNext` has an announcer subsystem and fires those four events. The fix is
  to build the consumer, then rerun the pipeline against the trigger list the code actually
  emits — the same discipline that already governs the callsigns, where every generated row
  points at a number that exists.

---

## 2. Architectural Reflection

### a. The decision I would now change

**I let the pipeline generate content for a schema instead of for a consumer.** #4 was
pointed at the design document's trigger list and produced well-formed rows for
`Kill.First` and `Match.SuddenDeath.Win` — trigger IDs that exist in the GDD and in no
line of code. The generated rows were valid CSV, correctly shaped, plausibly worded, and
completely unusable. The pipeline had no way to know, because nothing in it ever asked the
codebase whether the thing it was writing for was real.

That is the same failure this project has now hit three separate times: #4's coach lines
keyed to telemetry the game never records, and here again with the spotter triggers. The
common shape is **generating against an aspiration rather than against an implementation.**

### b. The specific alternative I would implement

**A consumer manifest as a hard precondition on generation.** Before any model call, the
pipeline greps the live source for the identifiers it is about to write content for, and
emits `consumers.json`: for each target key, the file and line that reads it. Any key with
zero consumers is **refused, not warned about** — generation for that key never runs, and
the run log records the refusal.

This is not hypothetical; it is the shape of the check I already run by hand and that
`land_in_engine.py` now encodes for callsigns. The change is moving it *upstream*, from
"discovered at landing time" to "enforced before a single token is spent." It would have
saved the money quantified below, and it converts a silent quality failure into a loud,
cheap, pre-flight one.

---

## 3. Cost Analysis

All figures are computed from the committed recordings of the **actual runs** (every
pipeline records its live API responses so the run can be replayed). Prices are Claude
Sonnet rates: **$3.00/M input · $15.00/M output · $3.75/M cache write · $0.30/M cache read.**

| Pipeline run | Calls | Input | Cache write | Cache read | Output | Cost |
|---|---:|---:|---:|---:|---:|---:|
| #4 content pipeline (divergent) | 11 | 80 | 211,096 | 2,129,120 | 145,942 | **$3.6197** |
| #5 goal-oriented agent | 1 | 2,192 | 0 | 0 | 61,845 | **$0.9343** |
| #4 naive baseline (comparison) | 4 | 10 | 37,679 | 172,866 | 13,981 | **$0.4029** |
| #7 style guide agent | 12 | 26 | 0 | 0 | 1,228 | **$0.0185** |
| #6 GER pipeline | 12 | 0 | 0 | 0 | 291 | **$0.0044** |

### a. Total actual run cost: **$4.98**

### b. Most expensive step

**The #4 content pipeline's divergent curation stage — $3.62, or 73% of everything spent.**
Not the model, not the prompt: the *strategy*. That stage generates ~10 candidates per slot,
has a critic rank them in JUDGE mode, keeps 3, then runs a REFUTER pass over the survivors.
Every one of those passes re-sends the retrieved design context, which is why cache reads
(2.13M tokens) dominate the bill. The expensive thing was deciding to throw most of the
output away — which is also why the output is good, and I would keep it.

### c. Is this sustainable for a solo developer or small team?

**Yes, comfortably — and the reason is architectural, not frugal.** $4.98 covers every
content-generating run across five assignments. The load-bearing decision is that **every
pipeline records its live responses and replays deterministically**, so re-running is free.
The pipelines have been executed dozens of times during verification since; those runs cost
**$0.00**, because only the model calls were ever the expensive part and they come from disk.

The honest caveat: this is one seat generating text and small data. It does not extend to
generated art or audio, and a team of ten running live would multiply the *first* number
while the replay number stays flat. For a solo founder shipping a vertical slice, the cost
of the AI is not the constraint — my time reviewing its output is.

---

## 4. Mid-Project Cost-Reduction Change

### a. Strategy / prompting approach

**Before:** each pipeline stage sent the retrieved design context as a fresh prompt. With
divergent curation re-reading the same GDD sections for ten candidates plus a judging pass
plus a refuter pass, the same corpus was paid for at full input price on every call.

**After:** the retrieved context became a **cached prefix**, written once per run and read
back by every subsequent call, and the replay recording made every re-run free thereafter.

### b. Token / API cost

Measured on the #4 pipeline, the most expensive step:

| | Tokens | Rate | Cost |
|---|---:|---:|---:|
| **Before** — same context at full input price | 2,129,120 | $3.00/M | **$6.39** |
| **After** — served as cache reads | 2,129,120 | $0.30/M | **$0.64** |
| Cache writes to enable it | 211,096 | $3.75/M | $0.79 |

**Net saving on that one step: $4.96** — the pipeline's context cost fell from $6.39 to
$1.43, a **78% reduction**, and it is why the whole project's bill is $4.98 rather than
roughly $10. The second-order saving is larger and harder to put a number on: because runs
replay from disk for free, the verification scripts across five assignments re-execute the
full pipelines on every check without spending anything.

---

## What this audit does not claim

The honesty rule this project runs on — *compiles ≠ works · PIE ≠ multiplayer · listen ≠
dedicated · editor ≠ packaged* — applies here too. The callsigns are verified in the config
by `land_in_engine.py --check` and were authored by a recorded pipeline run. The claim that
a player *sees* them on the scoreboard rests on the packaged build linked as Deliverable 1;
where that build's own limits are known, the submission states them rather than letting the
link imply more than it is.
