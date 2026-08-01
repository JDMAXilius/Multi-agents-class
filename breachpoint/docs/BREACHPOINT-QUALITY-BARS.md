# BREACHPOINT — Quality Bars
## The numbers behind "good": budgets, definitions of done, playtest protocol, ship checklist

**Companion to:** `BREACHPOINT-ROADMAP.md` (when) · `BREACHPOINT-ARCHITECTURE.md` (where) · crew contracts (law).
This doc exists so no gate ever argues about what "fast enough," "tested," or "shippable"
means — the bar is written before the work, and the verifier reads THIS doc for rung 5.

---

## 1. Performance budgets (rung 5 — measured, not vibed)

**Target hardware:** mid-range 2020 PC (GTX 1660 / Ryzen 5 class) — the Steam-demo floor.

| Metric | Budget | Measured by |
|---|---|---|
| Client frame time | **≤ 16.6 ms (60 fps)** at 1080p, 8 fighters + rockets on screen | `stat unit` capture in the standard soak scene |
| GT / RT / GPU split | GT ≤ 8 ms · RT ≤ 8 ms · GPU ≤ 14 ms | `stat unit` |
| Server tick (listen host) | **30 Hz stable** — tick ≤ 33 ms with 8 fighters | `stat unit` on host during 4v4 |
| Hitch ceiling | no hitch > 100 ms after map load (soft-ref async loads amortized) | `stat unitgraph` + insights trace |
| Load: menu → match | ≤ 15 s on SATA SSD | stopwatch, packaged build |
| Memory (client) | ≤ 6 GB working set | `stat memory` packaged |
| Draw calls | ≤ 2,500 in worst view (mid-level, all fighters visible) | `stat rhi` |

## 2. Network budgets (fills netcode.md's "track as classes land")

| Item | Budget |
|---|---|
| Per-connection bandwidth | **≤ 20 KB/s** sustained, ≤ 35 KB/s burst (rocket volley) |
| `ABRCharacter` + CMC | dominant share; NetUpdateFrequency 60, adaptive min 30 |
| `BRPlayerState` (ASC host) | NetUpdateFrequency 10 (raised from engine default 1) |
| `BRGameState` | 5 Hz effective (ring buffer + one clock float — no per-tick payloads) |
| Projectiles | replicated spawn + dormancy at rest; ≤ 8 live simultaneously |
| Fire path | 1 batched RPC per shot (`ServerAbilityRPCBatch` proven in BP03) |
| Playable floor | all rung-4 scenarios pass at **120 ms RTT + 5% loss**; playtest-fun at 80 ms |

## 3. Definition of Done — the card every packet is judged by

A packet is DONE when ALL of these hold (the verifier's checklist, quoted in every ticket):

1. **Compiles clean on all three targets** from scratch (live-coding state discarded).
2. **Its specs exist and were seen red then green** — a test that never failed proves nothing.
3. **The rungs it names are green**, including emulation variants for netcode packets;
   BLOCKED rungs are reported, never skipped.
4. **Zero grep-gate hits** (engine damage API, direct attribute writes, hard asset refs,
   `ConstructorHelpers`, NativeTick in widgets, loose gameplay tags outside cosmetics).
   Three of these — the damage API, `ConstructorHelpers`, unseeded `RandRange` — are now
   blocked live by `guard_laws.py` at tool-call time; the sweep still runs, because a hook
   only guards writes an agent makes *through it* (hand edits, merges, and imports bypass it).
5. **Cancel/rollback hygiene proven** for anything predicted (zero state residue).
6. **The ticket Log is written** — findings, decisions, rejected paths, seeds used.
7. **Multiplayer claims come in threes** (server, acting client, observing client) and name
   their rung.
8. **Dangerous domains carry a critic verdict** — REFUTER findings addressed or explicitly
   waived with rationale in the Log. **Only `high` severity blocks a landing** (R13);
   medium/low travel with the artifact in its risk register and are inherited by the lead,
   never silently dropped.
9. **Determinism claims name their seed** — any packet whose behavior is seeded (bots,
   spread, jitter) proves *same seed + same data row ⇒ identical trace*, and reports the
   seed list so the verifier can reproduce it. For bots this is all three layers:
   ambition, plan, and action traces (`Breachpoint.Bots.*`). A "it behaved fine in my run"
   claim without a seed is an opinion.

## 4. Playtest protocol (the human side of the gates — Class-03 method)

Agents find logic gaps; humans find fun gaps. Every gate playtest runs the same protocol so
results compare across weeks:

- **Cadence:** M2, M3, M4 = internal (TD + 1 invited player). M5 = 4 external players
  (2 sessions × 2). M6 = 2 strangers, zero coaching, screen-recorded.
- **Session shape:** 3 matches minimum; no instructions beyond "it's a Halo-style shooter"
  (M6: not even that — the game must teach itself).
- **The three questions** (asked AFTER, never during):
  1. *"What were you trying to do when you died?"* (legibility — did the game read?)
  2. *"Did anything feel unfair?"* (netcode/balance perception — compare vs telemetry)
  3. *"Would you play one more?"* — then WATCH whether they actually do (the run-it-back
     metric is behavioral, not verbal).
- **Telemetry pairing:** every session's `FBRMatchTelemetry` is archived with the notes;
  human claims get checked against data (a "sniper OP" claim meets the actual TTK table).
- **Grapple gate metric (M3+):** ≥ 1 offensive grapple (weapon-yank or reel) per match per
  player by M5, unprompted.
- **First-time-user metric (M6):** install → first kill **≤ 2 minutes**, unaided, 2/2
  strangers.

## 5. Steam ship checklist (M6 — gold means this list, done)

- [ ] App ID + demo depot configured; build uploaded via `app_build.vdf`; branch set live
- [ ] Packaged build passes the FULL ladder (editor ≠ packaged — rung run ON the package)
- [ ] Store page: 5 screenshots (one per level + grapple kill), 30 s GIF, short description
      written from the GDD elevator pitch
- [ ] Steam Input: gamepad glyphs verified; Steam Overlay + invite flow tested on 2 machines
- [ ] Crash-free soak: 20 overnight matches on the LIVE depot build, zero crashes
- [ ] No API keys, no `env.local`, no editor-only cvars in the package (grep the pak list)
- [ ] Version stamped; the shipped SHA tagged `demo-1.0`; rollback depot ready
- [ ] EULA/privacy: Spotter's outbound telemetry disclosed in the store description
      (one honest sentence), disabled cleanly when offline

## 6. Presentation bar (the second deliverable)

The capstone presents two artifacts: the game and the studio. The presentation is DONE when
it shows: the live demo (with the fallback capture ready), the six-pod structure and one
real ticket Log end-to-end (claim → packets → REFUTER finding → fix → green ladder), the
caught-defects list (every critic finding that never reached a player), and the honest
ledger — what was cut, what the cut order saved, what Phase 2 restores.
