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

**Pipeline Run Video:**
<!-- FILL: screen recording of `python3 run_pipeline.py` then `land_in_engine.py`, then the
     packaged build's scoreboard showing the generated callsigns -->
`PENDING`

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
land_in_engine.py    the landing step: pipeline output → the playable build
verify.sh            proves the connection from the files; needs no engine
```
