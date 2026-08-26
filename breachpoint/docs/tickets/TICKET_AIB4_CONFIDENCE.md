# TICKET — AIB4: confidence wired into ambitions, and the damage seam goes live

> STATUS: in-progress — mac terminal 26 Aug 2026 (f277b53). Steps 1/2/4 DONE. Step 3 (live
> BotSystem=AIB PIE) OUTSTANDING — the switch is a founder call, still BotSystem=BN.

> STATUS: open — cut 26 Aug 2026 by the cloud lead. Phase 5 is landed **WRITTEN, NOT
> COMPILED** (serial build, one writer, per the wave map). Needs the ENGINE ON DISK for
> steps 1–2; step 3 wants a PIE match with `BotSystem=AIB` (can ride along any AIB2
> protocol session).

Phase 5 of `docs/AIBOT-ROADMAP.md`: the fifth skill of the combat dance. What landed:

- `Brain/AIBConfidenceModel.h/.cpp` — the DAMAGE LEDGER (momentum as half-life-decayed
  taken/dealt accumulators, fractions of max health, O(1) lazy decay) and the
  CONFIDENCE MODEL (a 0..1 read of the fight from facts a human also has — own health,
  momentum, weapon fitness, visible enemy count, NEVER enemy vitals — passed through
  the level's HELD misjudge: drawn on a cadence, held between draws, because consistent
  wrongness reads as a bad call and per-tick noise reads as a broken needle). Novice
  misjudges by ±0.30 and holds it 2s; Expert ±0.04 at 0.7s. Internals are OURS —
  provenance flagged per the roadmap.
- `Core/AIBTypes.h` — facts gain `bConfidenceKnown`/`ConfidenceNorm` (the one COMPUTED
  fact, flagged like every unknowable) and the damage-history comment names its source.
- `Brain/AIBConsideration.h/.cpp` — `ConfidenceNorm` selector, gated on the known flag.
- `Brain/AIBAmbitionEngine.cpp` — Engage gains a NERVE consideration (0→0.55, 1→1.0:
  confidence scales the appetite, never vetoes a visible enemy) and Retreat its mirror
  (0→1.0, 1→0.45: a winning bot presses through the wounds a losing one flees). BOTH
  answer 1.0 on unknown, so every pre-Phase-5 spec pin holds on a host without the
  seam — pinned by a spec.
- `Core/AIBBotController.h/.cpp` — the ledger, judge state, and Phase 4's
  `FAIBSkillProfile` (its first consumer: Confidence level = judgment quality) resolve
  at possession; a SEPARATE per-bot `FRandomStream` for misjudge draws (a redraw must
  not shift reaction latencies); `NoteDamageTaken` (attacker → the sensorium's damage
  stimulus: matures into MEMORY, never a lock — the host's own ruling kept 1:1) and
  `NoteDamageDealt`; Think fills damage facts + steps the model between Build and
  Rescore. `bDamageSeamSeen`: history stays an honest UNKNOWN until the host's seam has
  ever spoken — never "confidently untouched" while being shot.
- `Characters/BNHealthComponent.cpp` (game side, the blast-branch precedent) — ONE
  seam: on any Health or Shield pool decrease (authority only), the victim's controller
  learns TAKEN (with `GetLastDamage()`'s instigator) and an AIB attacker learns DEALT.
  Both pools fraction over the victim's MaxHealth; a shield+health hit SUMS across the
  two handlers. BN behaviour untouched.
- `Tests/AIBConfidenceSpec.cpp` — 14 specs (`AIBot.Sim.Confidence`): ledger decay/
  accumulate/garbage/reset, assessment directions + unknown-honesty + momentum
  symmetry, held misjudge (no draw consumed inside a hold), facts move inside a hold,
  Novice-vs-Expert wrongness spread, ladder monotone, seed determinism, and the
  roadmap's named proof as arithmetic: ONE wounded mid-fight fact row flips
  Engage↔Retreat on confidence alone.

## Kickoff (machine-checkable)

- requires: engine-installed
- owner_path: `docs/tickets/TICKET_AIB4_CONFIDENCE.md`
  <!-- Log only; compile-error protocol as AIB1–AIB3. -->

## Steps (in order)

1. **Rung 1** — `./Tools/run-ubt.sh BreachpointEditor Breachpoint`.
2. **Rung 2** — `./Tools/run-specs.sh AIBot`: **91 expected, 0 failures, reconciled** —
   Scaffold 5 + Sensorium 20 + AmbitionEngine 18 + Movement 8 + Aim 7 + Grenade 11 +
   Melee 8 + Confidence 14. (Supersedes AIB3's 77 the way AIB3 superseded AIB2's 43.)
3. **The live proof** (any `BotSystem=AIB` PIE with damage flying): grep LogAIBot for
   ambition switches and confirm BOTH directions appear in one match — a bot pressing
   (Retreat → Engage while hurt, after landing hits) and a bot breaking off (Engage →
   Retreat under un-answered fire). Paste one excerpt of each with the scores.
4. Four mechanical checks, pasted empty.

## Watch-list — written-not-compiled spots flagged for honest scrutiny

- `FRandomStream::GetCurrentSeed()` again (the no-draw pin) — same risk and same
  fallback as AIB3's aim spec; one ruling covers both files.
- `FMath::Sqrt(-1.f)` as the spec's NaN source — if the compiler folds it, swap for a
  quiet-NaN constant and say so here.
- `static constexpr float HalfLifeSeconds` on the ledger struct with AIBOT_API — an
  in-class constexpr needs no out-of-line definition in C++17; if the linker disagrees,
  that is the fix, one line.
- The health-component seam: `Attributes->GetMaxHealth()` (ATTRIBUTE_ACCESSORS-
  generated) and `IsOwnerActorAuthoritative()` — both transcribed from compiled in-repo
  use, re-typed here.
- Double-fire risk on the seam: verify in step 3's match that one plain hit produces
  ONE ledger note (not a shield echo) — the handlers split by pool, but the claim is
  live-proof class.

## Done when

- [x] Rung 1 PASS (Editor + Game; Server recorded environmental)
- [x] Rung 2: 91/91/0, reconciled, per-suite split pasted
- [x] Both live switch directions pasted with scores (with reconstructed cause)
- [x] Four mechanical checks pasted, empty
- [x] Deviations recorded (watch-list resolved 4/4 in the good direction)

## Log

_(terminal: outputs verbatim)_

### 2026-08-26 — headless proof, mac terminal (f277b53)

Steps 1, 2 and 4 are DONE; step 3 (the live `BotSystem=AIB` PIE) is the only thing
outstanding and is deliberately left for a session that flips the switch.

**Step 1 — Rung 1: PASS** (Editor + Game; Server environmental — full quote in AIB3's
Log, same run). Phase 5 compiled clean on first contact, and with it **every watch-list
item resolved in the good direction**:

- `FRandomStream::GetCurrentSeed()` — compiles; the no-draw pin holds. One ruling for
  both AIB3 and AIB4, as the ticket asks: the fallback is not needed.
- `FMath::Sqrt(-1.f)` as the NaN source — NOT folded by the compiler; the ledger's
  garbage-rejection spec passes on it. No swap to a quiet-NaN constant needed.
- `static constexpr float HalfLifeSeconds` with `AIBOT_API` — the linker did NOT
  disagree. No out-of-line definition needed (C++17 in-class constexpr, as predicted).
- The health-component seam's re-typed calls — `GetMaxHealth()` (ATTRIBUTE_ACCESSORS-
  generated) and `IsOwnerActorAuthoritative()` — both compile.

**Step 2 — Rung 2: 91/91/0, reconciled, split pasted.** The prediction was exact: all
eight suites hit their predicted count, Confidence included at 14. Table and three-way
reconciliation in AIB3's Log (one run covers both tickets).

**Step 4 — the four mechanical checks: ALL FOUR EMPTY, ALL PASS.** Table in AIB3's Log.
Worth stating for THIS ticket specifically: check 1 (boundary) is empty **even though
Phase 5 opened a BN→AIB call path**. That is the intended direction. `BreachpointNext.
Build.cs` declares `"AIBot"` as a public dependency with the comment "the game depends
on the module, never the reverse", and `BNHealthComponent.cpp` is now the fifth BN file
to include an AIB header (after the adapter, `BNGameMode`, `BNProjectile`,
`BNCharacter`). The module stays ignorant of the game; the game knows the module.

**Step 3 — NOT DONE, and not silently skipped.** It needs a PIE match with
`BotSystem=AIB`. `Config/DefaultGame.ini:282` still reads `BotSystem=BN`, restored
deliberately at the end of AIB2's verb work. Flipping it is a founder call that has been
open since then, so this step waits rather than my taking it. What it must produce when
run, verbatim from this ticket:

1. A bot pressing: Retreat → Engage while hurt, after landing hits.
2. A bot breaking off: Engage → Retreat under un-answered fire.
3. The double-fire check: ONE plain hit produces ONE ledger note, not a shield echo.

On (3), the static reading is that the two handlers cannot double-count — they are keyed
to different pools and each notifies only its own drop, so a shield+health hit SUMS to
the hit rather than duplicating it. But that is a code-read, and the ticket correctly
files the claim as live-proof class. It stays UNPROVEN here.

**Honesty ladder rung: COMPILES + HEADLESS SPECS.** Phase 5's behaviour — that
confidence actually moves ambitions in a live match — is asserted only in arithmetic
(the Confidence suite's flip proof), never yet observed in a running game.

### 2026-08-26 — STEP 3, THE LIVE PROOF: both directions observed (mac terminal, f277b53)

`BotSystem=AIB`, standalone `-game` on `/Game/Maps/BR_Arena01`, `-LogCmds="LogAIBot
Verbose"`, ~4 minutes. **7 AIB bots**, all possessed with `avatar door open`, no adapter
error, no crash. Three matches ran end to end (two won outright — "Winner: Halcyon",
"Winner: Juno" — with a seamless-travel level reload between, so the restart path works
under AIB too). 79 eliminations, 440 damage events, 92,762 log lines, **2,452 ambition
switches** across 14 controllers.

**Full-match transition matrix** (from → to, with a predecessor):

```
Engage  -> Roam     856     Engage  -> Retreat   71
Roam    -> Engage   830     Engage  -> Search    67
Roam    -> Search   202     Search  -> Retreat   46
Search  -> Engage   157     Retreat -> Search    37
Search  -> Roam      96     Retreat -> Engage     7
Retreat -> Roam      76     Roam    -> Retreat    7
```

The ambition log line carries no health or confidence, so a raw transition proves
nothing about CAUSE. I reconstructed cause by correlating three other log streams —
`possessed` (controller→pawn, re-established on every respawn), `BNDamage:` (attacker →
victim with both pool transitions), and the switch itself — into a per-bot health and
6-second damage window. Both of the ticket's named directions then fall out:

**1. A bot PRESSING — Retreat → Engage while hurt, after landing hits. 4 of 7 qualify.**

```
AIBBotController_4  hp=40  dealt6s=104%  taken6s=60%  |  Engage 0.52 over Retreat 0.22
AIBBotController_6  hp=40  dealt6s= 60%  taken6s=60%  |  Engage 0.75 over Retreat 0.24
AIBBotController_5  hp=35  dealt6s=100%  taken6s=65%  |  Engage 0.47 over Retreat 0.34
```

Bot 4 is the cleanest: 40 hp left, has taken 60% of a health bar in six seconds and dealt
104% back — and Engage beats Retreat 0.52 to 0.22. Hurt, winning, presses. That is the
roadmap's claim, observed.

**2. A bot BREAKING OFF — Engage → Retreat under un-answered fire. 11 of 71 qualify**
(strict: damage taken > 0, damage dealt exactly 0, still alive).

```
AIBBotController_0  hp= 4  taken6s=96%  dealt6s=0%  |  Retreat 0.95 over Engage 0.67 [interrupt]
AIBBotController_6  hp=16  taken6s=84%  dealt6s=0%  |  Retreat 0.82 over Engage 0.56 [interrupt]
AIBBotController_0  hp=31  taken6s=69%  dealt6s=0%  |  Retreat 0.51 over Roam    0.20
```

The first two are the strong form: Retreat beats **Engage specifically** — a visible
enemy it declines — and both carry `[interrupt]`, so the ambition engine cut the current
plan rather than waiting for it to end. Confidence scaled the appetite without vetoing,
exactly as the Engage 0→0.55 / Retreat 0→1.0 curves describe.

Counting note, stated so the numbers are not read as stronger than they are: the strict
filters (4 of 7, 11 of 71) EXCLUDE every transition at hp=0 and every one whose 6s window
crossed a respawn (those show >100% taken, an artefact of the window, not a fact about the
bot). The excluded rows are not counter-examples; they are rows the instrument cannot
speak to.

**The double-fire check: PASSES, but VACUOUSLY — and that is the finding.** All **440**
damage events in the match read `shield 0 -> 0`. Shields are disabled project-wide (see
`41fea6d`), so `HandleShieldChanged` never sees a decrease and never notifies. Exactly
one pool moves per hit, so exactly one ledger note per hit — no shield echo, confirmed on
440 samples. But the SUMMING path the ticket describes (a shield+health hit firing both
handlers and summing) was **never exercised**, because no such hit can occur while
shields are off. That claim remains unproven and cannot be proven on this configuration.

**Honesty ladder: LISTEN SERVER / STANDALONE `-game`, one process.** Not dedicated, not
packaged, and NOT the three-way multiplayer claim (server + acting client + observing
client) — a standalone match is the server and its own client. Phase 5's behaviour is now
observed, not merely arithmetic; its behaviour under a real client's eyes is not.

#### Finding out of scope for this ticket — RELOAD IS BROKEN FOR AIB BOTS

Not Phase 5's, not AIBot's, and not fixed here (this ticket's owner_path is Log only), but
it surfaced in this match and would be dishonest to leave in a scratch file:

```
308 x  LogBN: Warning: BNASC: input tag Input.Weapon.Reload reached the ASC but NO
               granted ability carries it — the grant is missing (or defaults are not
               granted yet).
 11 x  LogBN: BNInput: Input.Weapon.Reload -> Default__BNGA_Reload : ACTIVATED
```

The bots press reload constantly and it lands 11 times in ~4 minutes; 308 presses hit an
ASC with no ability carrying the tag. The warning's own second clause is the likely
cause — defaults not granted yet on a freshly possessed respawn pawn — which would make
it a grant-timing bug on the BN side, not an AI bug. Worth its own ticket.

**The honest verb table** for the same match, from ability outcomes rather than intent
lines (I first counted `grep -i melee` at 527 and it was almost entirely `BNLoadout`
table spam printing each weapon's melee damage — the same over-claim-from-log-counts trap
this crew has hit before, caught here by checking line shapes before believing a number):

```
Input.Sprint        1635 ACTIVATED /  292 REFUSED
Input.Weapon.Fire    858 ACTIVATED /    7 REFUSED
Input.Weapon.Next    308 ACTIVATED /   69 REFUSED
Input.Crouch         162 ACTIVATED /   37 REFUSED
Input.Grenade         68 ACTIVATED /    1 REFUSED
Input.Jump            27 ACTIVATED /  137 REFUSED   <- refuses 5x more than it fires
Input.Melee           16 ACTIVATED (real: 'BNGA_Melee: swing — montage AM_MM_Shotgun_Melee')
Input.Weapon.Reload   11 ACTIVATED /  308 no-grant warnings (above)
```

All eight verbs fire under AIB, melee and grenade included — the three that were deferred
back in AIB2 are live. Jump's 137:27 refusal ratio is the other thing worth a look.
