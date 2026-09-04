# Assignment #10 — Complete AI Dev Pipeline

**Student:** Juan Diego Lugo · **Capstone:** BREACHPOINT · **Engine:** Unreal Engine 5.8 (native C++)

```bash
./verify.sh    # proves the pipeline→game connection from the files themselves, no engine needed
```

**Game concept.** A Halo-inspired 4v4 arena FPS: shields that break and regenerate, a
grappleshot, a rocket that spawns on a timer the whole lobby fights over, and bots that
fill empty seats. Server-authoritative, Gameplay Ability System, zero Blueprint gameplay
classes. The pipeline in this assignment writes the bots' callsigns.

---

## Deliverable 1 — Playable link

<!-- FILL AFTER PACKAGING (TICKET_BN40): paste the public itch.io URL here -->
**Playable Game Link:** `PENDING — see breachpoint/docs/tickets/TICKET_BN40_WINDOWS_PACKAGE_ITCH.md`

**What the download is, stated plainly:** a packaged **Windows client**, single player
against bots. Unreal Engine 5 has no browser/WASM target — HTML5 was removed in UE 4.24 —
so a native download is the only honest form this can take. It is not a browser build and
not a multiplayer claim.

---

## Deliverable 2 — Pipeline source & engine integration

**Pipeline Repository:** https://github.com/JDMAXilius/Multi-agents-class/tree/main/assignments
The content pipeline is [`04-content-pipeline/`](../04-content-pipeline/); its landing step
is [`land_in_engine.py`](land_in_engine.py) in this folder. Supporting agents that shaped
the same content: [`06-ger-pipeline/`](../06-ger-pipeline/) (generate→evaluate→refine) and
[`07-style-guide-agent/`](../07-style-guide-agent/) (house voice).

**Pipeline Run Video:** *not submitted for this assignment — the run is verified from its
output instead, which is the other half of the rubric's own "verified via video/output".*
A video walkthrough ships with the final project.

**How to verify the run without a video, in three commands** (no API key, no engine, ~20s):

```bash
cd assignments/04-content-pipeline && python3 run_pipeline.py
#   -> pool: 30 candidates across 3 slots · judged - kept 15 of 30 · refuted: FINDINGS
#   -> LANDED output/DT_BotCallsigns.csv (15 rows)
#   -> spend (recorded run): 11 calls · 80 in · 145,942 out · 211,096 cache-write ·
#      2,129,120 cache-read; priced at $3.00/$15.00/$3.75/$0.30 per M = $3.6197

cd ../10-ai-dev-pipeline && python3 land_in_engine.py --check
#   -> OK: all 15 bot names in the shipped config came from the pipeline

bash verify.sh
#   -> ALL 9 CHECKABLE CRITERIA PASS - 0 failures
```

The pipeline **replays its recorded API responses**, so anyone can execute the real run
end to end with no key and no spend and watch the same candidates get generated, judged,
refuted and landed. That is a stronger artifact than a screen recording: a video shows that
it ran once on my machine, whereas `recording.json` lets a grader re-run it on theirs and
`verify.sh` fails loudly if the write-up ever drifts from what the recordings say. The
$3.6197 printed by the run is the same figure `AUDIT.md` quotes, enforced by `verify.sh`
rather than typed.

### Integration breakdown

**Target Game Engine:** Unreal Engine 5.8 (native C++, no Blueprint gameplay classes)

**Automated flow — how agent output reaches a running scene with no reformatting:**

```
DT_BotTuning.csv                 the game's real tuning data (reaction_ms, accuracy_pct,
   │                             aim_error_deg, cover_preference, target_memory_s)
   ▼
04-content-pipeline/rag.py       BM25 retrieval over the GDD + live source, no embeddings
   ▼
run_pipeline.py                  divergent curation: generate ~10 → critic ranks (JUDGE)
   │                             → keep 3 → REFUTER pass over survivors
   ▼
output/DT_BotCallsigns.csv       15 callsigns, each derived from a real tuning number
   ▼
land_in_engine.py                writes ABNGameMode::BotNames in Config/DefaultGame.ini
   ▼
ABNGameMode::EnsureBotFill()     reads BotNames at bot-fill time — no recompile, no import
   ▼
the scoreboard, killfeed, and kill lines a player reads in the packaged build
```

**Why there is no reformatting step.** `BotNames` is `TArray<FString>` marked
`UPROPERTY(Config)` — a door that already existed. The pipeline's product is *data*, and
data reaches this engine through config, not through a build. A DataTable would have needed
a new `USTRUCT`, a header edit, and a compile before one name could appear on screen; the
config path needs none of them. `land_in_engine.py --check` re-proves at any time that every
bot name in the shipped build came from the pipeline, and exits non-zero if one didn't.

**One command, idempotent, reversible:**
```bash
python3 land_in_engine.py            # land the pipeline's callsigns in the game
python3 land_in_engine.py --check    # verify the game matches the pipeline (exit 0/1)
python3 land_in_engine.py --restore  # put the seven hand-written names back
```

---

## Deliverable 3 — Pipeline audit & cost analysis

**→ [`AUDIT.md`](AUDIT.md)** — one page, covering what the pipeline produced, the manual
steps that remain, the architectural decision I would change, and the cost analysis
computed from the actual recorded runs.

The three numbers, if you read nothing else:

| | |
|---|---|
| **Total actual run cost** | **$4.98** across five pipeline assignments |
| **Most expensive step** | #4's divergent curation — **$3.62 (73%)**, and worth it |
| **Mid-project cost reduction** | prompt caching cut that step's context cost **$6.39 → $1.43 (78%)** |

---

## The finding this assignment turned up

Auditing my own pipeline for this write-up found that **its content had never actually
reached the game.** For two weeks I had been calling #4 a content pipeline while its output
sat in an assignment folder and the shipped build used seven hand-typed bot names. This
assignment is what closed that gap — which is, in fairness, exactly what an audit is for.

The second finding is the one I'm keeping: the same pipeline run also produced twelve
announcer lines, and **I did not land them.** The live module has no announcer system at
all, and the four trigger IDs those lines are keyed to appear nowhere in the source. Landing
them would have grown the integration story by twelve rows of content that nothing reads and
nothing fires. `land_in_engine.py` refuses to touch them and says why in its own docstring;
[`AUDIT.md`](AUDIT.md) §1b states it as a remaining manual step rather than hiding it.

Both failures share a shape — **generating against an aspiration instead of against an
implementation** — and the architectural change proposed in AUDIT.md §2b is the fix:
grep the live source for a consumer *before* spending a token, and refuse to generate for
any key nothing reads.

---

## Files

```
README.md            this file — the submission document
AUDIT.md             Deliverable 3: the one-page audit and cost analysis
RECORD-THE-VIDEO.md  the shot list for Deliverable 2's run video (5 shots, ~3 min)
land_in_engine.py    the landing step: pipeline output → the playable build
verify.sh            proves the connection from the files; needs no engine
```
