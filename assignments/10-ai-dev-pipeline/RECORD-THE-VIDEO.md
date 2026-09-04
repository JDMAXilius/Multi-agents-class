# Deliverable 2 — the Pipeline Run Video: a shot list

**Why this file exists.** The rubric gives **3.0 points** — the largest single block — to
*Pipeline-to-Game Connection*, and says it is "verified via video/output". The video is not
decoration; it is the evidence for the biggest criterion on the sheet. This is the shot
list so the recording takes one take, not five.

**Total runtime: about 3 minutes.** Record with OBS (already installed). 1280x720 or
higher. Narration optional — every claim below is legible on screen without it.

---

## Before you hit record (2 minutes of setup, saves a re-take)

```bash
cd assignments/04-content-pipeline
python run_pipeline.py            # confirm it ends "= $3.6197" and exit 0
cd ../10-ai-dev-pipeline
bash verify.sh                    # confirm ALL 9 CHECKABLE CRITERIA PASS
```
Both were passing as of 4 Sept. Make the terminal font large — a grader is watching a
compressed video, and the callsigns are the whole point.

---

## Shot 1 — the pipeline runs, and it costs what the audit says (~60s)

```bash
cd assignments/04-content-pipeline
python run_pipeline.py
```

Let it run to the end without cutting. What the grader needs to see land on screen:

| moment | what it proves |
|---|---|
| `pool: 30 candidates across 3 slots` | divergent generation, not one-shot |
| `judged — kept 15 of 30` | a critic actually discards work |
| `Recruit-08 won — Dulledge lifts the GDD's own descriptor` | the name is *derived*, not vibed |
| `refuted: FINDINGS — 2 finding(s)` | a REFUTER pass runs over survivors |
| `LANDED output/DT_BotCallsigns.csv (15 rows)` | the artifact exists |
| `spend (recorded run): … = $3.6197` | **the number AUDIT.md quotes** |

That last line is why the cost math was fixed on 4 Sept — before, the run printed $4.1760
while the audit said $3.6197, and this shot would have contradicted the write-up on camera.

## Shot 2 — the output is real data (~20s)

```bash
head -6 output/DT_BotCallsigns.csv
```
Show that each row carries the tuning number that justifies it. Say the one sentence that
makes the anti-slop rule moot: *"Slowdraw exists because reaction_ms=500 is the slowest
profile — the name is derived from the game's own tuning table."*

## Shot 3 — it lands in the engine, one command (~30s)

```bash
cd ../10-ai-dev-pipeline
python land_in_engine.py --check
```
Expected: `OK: all 15 bot names in the shipped config came from the pipeline`.

Then show the door it writes through:
```bash
grep -A4 'BotNames' ../../breachpoint/Config/DefaultGame.ini | head -12
```
The point to state out loud: **no reformatting, no import, no recompile.** `BotNames` is
`UPROPERTY(Config)`, so data reaches the engine through config and is read at bot-fill time.

## Shot 4 — the same names, in the running game (~45s) — THE MONEY SHOT

Launch the packaged build and get to a match:
```
Export/Win64/Breachpoint.exe /Game/Maps/BR_Spillway
```
(The Development build honours a map on the command line; the Shipping one does not.)

**Hold on the scoreboard — press Tab — long enough to read the names.** Dulledge, Softaim,
Slowdraw, Evenkeel, Wideshot, Shakygrip, Midpace. That single frame IS the
Pipeline-to-Game Connection criterion: the names generated in Shot 1, on screen in the
artifact from Deliverable 1.

This frame is also the still owed to `verify.sh`'s one remaining PEND
("pipeline callsigns visible in the build"), so grab a screenshot while you are here.

## Shot 5 — the audit is checkable, not asserted (~20s)

```bash
bash verify.sh
```
Ends on `ALL 9 CHECKABLE CRITERIA PASS — 0 failures`. It re-derives the cost from the
recordings and fails loudly if AUDIT.md's figures drift from them, so the cost claim is
enforced rather than typed.

---

## Where to put it

Upload unlisted to YouTube (or Drive with link-sharing on) and paste the URL into
`README.md` Deliverable 2, replacing `PENDING`. **Check the link in a private window** —
the same trap as the itch.io page: a permissions-locked video reads to a grader exactly
like a broken one.

## What NOT to claim on camera

The honesty rule this project runs on applies to the narration too:

- It is a **Windows client, single player against bots.** Not a browser build, not a
  multiplayer claim. UE5 has no browser target — HTML5 was removed in 4.24.
- **Keyboard and mouse are proven; the gamepad is not.** The layout ships audited 77/77 but
  has never been held.
- The twelve spotter lines the same run produced are **deliberately not in the build**, and
  saying so on camera is worth more than the twelve rows would have been — it is the
  anti-slop rule being obeyed out loud. AUDIT.md §1b explains why.
