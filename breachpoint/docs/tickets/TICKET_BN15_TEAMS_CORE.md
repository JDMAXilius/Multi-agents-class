# TICKET — BN15: the team framework core (T0 identity+assignment, T1 combat honesty)

> STATUS: in-progress — cloud lead + crew wave, 26 Aug 2026. Design authority:
> docs/BN-TEAMS-PACKET.md; order: docs/BN-TEAMS-ROADMAP.md (this is T0+T1 as one
> packet, per the design's own recommendation). Contracts in force: netcode.md
> (new replicated property = this packet + critic REFUTER), gas-purity.md (the FF
> gate runs BEFORE any GE spec exists).

## Wave plan (AIBOT-WAVES doctrine: shared headers SERIAL, then disjoint writers)

SERIAL FOUNDATION (lead, landed with this ticket): `Match/BNTeams.h` (the one query,
NoTeam guard), header deltas to `BNPlayerState.h` (TeamId + IGenericTeamAgentInterface
+ OnTeamChanged), `BNGameState.h` (team scores + WinningTeamId + delegate),
`BNGameMode.h` (ChoosePlayerStart override, AssignTeamIfNeeded/GetLowestPopulationTeam/
FinishTeamMatch, Config bTeamsEnabled/bFriendlyFire).

W-BUILD ×3, disjoint file lists:

| Writer | Files (exact, nothing else) |
|---|---|
| netcode-builder | `Match/BNPlayerState.cpp` · `Match/BNGameState.cpp` · `Tests/BNTeamsSpec.cpp` (new) |
| bn-builder (mode) | `Match/BNGameMode.cpp` · `AbilitySystem/Effects/BNDamage.cpp` · `Config/DefaultGame.ini` · `Tools/bn/tag_team_starts.py` (new) |
| bn-builder (bots) | `AIBotAdapter/BNAIBWorldQuery.h/.cpp` · `AI/BNBotController.cpp` · `Plugins/AIBot/Source/AIBot/Core/AIBBotController.cpp` (the ONE pre-sanctioned module edit) |

Barrier: lead merges, runs the module gates + the new grep (BNTeams is the only
GetAttitude caller), single commit. Then W-REVIEW: bn-critic (netcode/GAS dimension)
+ aib-critic (fairness on the bot half), read-only, highs block.

## Done when (terminal proof, after the review barrier)

- [ ] Rung 1 all targets; specs (module 118 + BNTeamsSpec)
- [ ] OFF-regression: one FFA match, zero behavioural diffs (harness output identical class)
- [ ] ON: 4v4 assigns 4/4 by population (assignment log lines)
- [ ] FF refused count > 0 while self-grenade still damages
- [ ] Neither bot system ever targets a teammate (acquisition lines)
- [ ] Claims: first live GRANTED/DENIED between allied AIB bots; AIB12 FFA-inert holds OFF
- [ ] The degenerate cheat test green — asserted as ABSENCE OF SERVER EFFECT in threes,
      not as client self-correction (bn-critic: a memory-poked client byte is never
      re-sent, so "the client got corrected" is not the provable claim; "the server
      never changed and no observer's OnRep fired" is): server — cheater's TeamId,
      FF outcomes, team ledger all unchanged; acting client — the setter no-ops even
      locally; observing client — no OnRep_TeamId, scoreboard team unchanged

## Log

**26 Aug — W-REVIEW barrier (bn-critic netcode/GAS ∥ aib-critic fairness). No highs.
Two MEDIUMs, one root, fixed at the barrier; LOWs to the register below.**

- **The root (both critics, independently): `!AreEnemies` is not "ally".** The adapter's
  `AreEnemies` folds liveness into hostility (a corpse is nobody's enemy), so the two
  consumers that read its negation as alliance both broke:
  1. *Attitude consult* (`AIBBotController::GetTeamAttitudeTowards`): corpses and
     ASC-less spawn-window pawns read **Friendly** where the pre-BN15 constant read
     Hostile — refuting the OFF-regression "FFA byte-identical" gate, and (aib-critic's
     trace) eating a dead target's perception LOSS events at the Note-boundary filter,
     so `bSightCurrent` stayed true and the sensorium live-sampled a corpse through
     walls until actor destruction at respawn.
  2. *Claims board binding* (`FAIBClaimsBoard` predicate): a dead ENEMY flipped to
     not-enemies for everyone, so its live claim bound ACROSS teams for the corpse
     window — the charter's own collusion case.
- **The fix (one interface answer, three consumers):** `IAIBWorldQuery::AreAllies` —
  alliance WITHOUT liveness, defaulted `false` (a host that wires nothing is FFA).
  Adapter implements it as the pure team compare (`BNTeams::AreFriendly`, NoTeam
  guard). Attitude: Friendly iff `AreAllies`, else Hostile — FFA is again
  byte-identical to the old constant, dead enemies are Hostile, loss events flow.
  Board: predicate flipped to `AreAllies`, binding stated positively; coordinator
  resolver returns false with no query (inert board, same direction as before);
  `AIBClaimsSpec`'s `Allies`/`Enemies` helpers now answer their own names. Gates
  re-run clean; BNTeams.h still the only GetAttitude caller.
- **Considered and declined:** aib-critic's loss-before-filter ordering exemption in
  `OnPerceptionUpdated` — unnecessary once corpses read Hostile again (a target can
  only stop being Hostile by team reassignment, which BN never does mid-life), and
  less-is-more says no second mechanism for a closed hole. Re-open if a mode ever
  reassigns teams mid-match. bn-critic's Neutral-fallthrough variant — declined
  because Neutral still diverges from the pre-BN15 Hostile answer in FFA.
- **Register (LOWs, dated, no code change):**
  - `OnRep_TeamId`'s body becomes load-bearing on the authority the moment the first
    gameplay subscriber to `OnTeamChanged` lands — BN16's reviewer must re-check law
    3's deletion test then (bn-critic 2).
  - Cheat-test wording tightened above (bn-critic 3).
  - `bCrowdKnown` contract ("true only when BOTH counts are honest") must be
    spec-pinned before any enemy count lands; `NearbyAllies` is real but consumerless
    — the `TActorIterator` cost buys nothing until then (aib-critic 3). Candidate
    simplification for BN17, not now. → PINNED 26 Aug (AIBConfidenceSpec "the crowd
    contract", 3 Its; expected spec count now 121 — see BN17). The iterator-cost half
    stays open.
  - Dated acceptances: FF-on teammate damage informs the ledger but never turns the
    bot (less information is always fair); teammate grenades still dodged (attitude
    filter would ADD information); a dead killer's bearing memory persists until the
    F-2.2 lifetime door clears it (aib-critic 4). All PASS as designed.
- **Explicit passes worth keeping:** assignment covers all three arrival paths before
  first spawn (server-side readers never see the NoTeam window); 255 honored at every
  seam; zero Server RPCs in the diff; FF gate structurally server-only behind
  `HasAuthority`; `WinningTeamId` needs no OnRep (uint8 rides the same bunch as
  MatchState, applied before RepNotifies — the Winner pointer's GUID-resolution bug
  class cannot apply); seamless travel carries TeamId and honestly resets the ledger;
  `CountNearbyAllies` at 10000uu is HUD-grade under the interface ruling.

### 2026-08-26 — rung 1 and specs, mac terminal

**Rung 1 PASS both targets** after one fix. `BNGameMode.cpp` included
`GameFramework/PlayerStartPIE.h`; `APlayerStartPIE` lives in `Engine/PlayerStartPIE.h`.
Its `APlayerStart` BASE is the one under `GameFramework/`, which is exactly why the
transposition is easy to make and hard to spot — the compiler only says "file not found".

```
BreachpointEditor   Result: Succeeded
Breachpoint         Result: Succeeded
```

**Specs:**

```
AIBot                        119/119/0  reconciled
BreachpointNext.Sim.Teams      5/5      PASS
Breachpoint.Sim.*              3 FAILURES — see below
```

The three failures are **pre-existing and out of this wave's scope**, in the legacy
`Breachpoint` (BR-prefix) module:

- `Breachpoint.Sim.Combat.UBRAttributeSet clamps` — clamps against an uninitialised capacity
- `Breachpoint.Sim.Combat.UBRDamageExecCalc` — R22 Damage.* flatness
- `Breachpoint.Sim.Shields` — refuses to mark an uninitialised fighter as broken

`git diff --stat` over the whole 15-commit teams wave shows **zero changes** under
`Source/Breachpoint/`; those files last moved in the BP91-era commits. Recorded rather than
folded into this ticket's count: BN15 must not be judged on them, and they must not be
allowed to disappear either.

Remaining Done-when boxes are all live-proof and unstarted: OFF-regression, 4v4 assignment,
friendly-fire refusal, no-teammate-targeting, live claims, and the degenerate cheat test.

### 2026-08-26 — crew wave: verifier + REFUTER critic. Two BLOCKING findings.

#### What is genuinely proven

**4v4 assignment by population — PASS, three matches, unambiguous log lines:**

```
BNGameMode: juans-MacBook-Pro.lo assigned to team 0.    BNGameMode: Marcus  assigned to team 1.
BNGameMode: Vale assigned to team 0.                    BNGameMode: Ossian  assigned to team 1.
BNGameMode: Rook assigned to team 0.                    BNGameMode: Halcyon assigned to team 1.
BNGameMode: Juno assigned to team 0.                    BNGameMode: Piper   assigned to team 1.
```

Identical 4/4 split across all three ON matches.

#### Two verifier PASSES REJECTED — wrong instrument, and an unchecked assertion

The verifier reported "Teams framework core PROVEN live". I am not accepting two of its
boxes, and the reasons are specific:

1. **"Neither bot system targets a teammate" was proven from ELIMINATION lines.** The box
   says *acquisition lines*, and 49 acquisitions exist in its own match-1 log. Absence of
   teammate KILLS is equally explained by the FF gate blocking damage while bots keep
   aiming at allies — the two hypotheses are indistinguishable from kill data, which is
   exactly why the box names acquisitions.

2. **"FF refused: explicit logging doesn't occur because the gate runs pre-damage" is
   FALSE.** `BNDamage.cpp:159` is
   `UE_LOG(LogBN, Verbose, TEXT("BNDamage: friendly fire refused — %s -> %s (%.1f)."))`.
   The gate logs. The verifier ran without `LogBN Verbose`, so the line could not appear,
   and then explained its absence with a property of the code it had not read. The box
   asks for a POSITIVE refusal count and is provable at the right verbosity.

**Its matches were also not comparable.** Durations 130s / 41s / 21s / 17s against a 220s
baseline, at 182 / 281 / 7 / 52 ambition switches against a healthy 1613–2551. It called
these "normal range for partial match"; match 3 (7 switches, 0 kills) is the deadlock
signature this ticket's own bots showed under the hill. Re-running at correct verbosity.

#### REFUTER findings — TWO BLOCKING

**B1 — deterministic 5v3 with two or more humans** (`BNGameMode.cpp:674`, `:262`, `:328`).
Host joins → {0,0} tie → Team 0; fill alternates 7 bots to 4v4 with the `SpawnedBots` tail
on Team 1. Second human joins in warmup: `AssignTeamIfNeeded` runs FIRST, sees {4,4}, ties
to the lower id → Team 0 ({5,4}); then `OnPostLogin` → `EnsureBotFill` computes
`BotsNeeded = -1` and pops the **tail, which is Team 1** → **5v3**. Repeats at 4 humans.
Invisible in PIE because one player never reaches the pop path.

**B2 — teams spawn selection skips the encroachment partition** (`BNGameMode.cpp:386`).
The teams branch returns `Matches[RandRange]` and never runs the engine's
`EncroachingBlockingGeometry` pass, while `SpawnDefaultPawnAtTransform` uses
`AlwaysSpawn`. A round restart rebodies all 8 in one loop and four teammates draw from one
tagged pool **with replacement** — two draw the same start, two capsules interpenetrate.
Also lets a live respawn drop a player inside a standing teammate. FFA is unaffected (it
still goes through `Super`), so this is teams-only and ≥2-bodies-only.

**N1 — the FF gate has a 3-second hole, and the two numbers are equal today**
(`BNDamage.cpp:155`). The gate resolves both sides only via `APawn::GetPlayerState`, and
`APawn::UnPossessed` nulls it. `RespawnDelay = 3.0` (`BNGameMode.h:190`) **exactly equals**
grenade `FuseTime = 3.0` (`DefaultGame.ini:450`) — both confirmed by reading. A player
killed in the frame he throws has `RespawnPlayer`'s `UnPossess(); Destroy()` racing the
blast: the instigator resolves NoTeam, `AreActorsFriendly` answers false, and **the grenade
damages and kills teammates with friendly fire OFF**. Same hole via `DespawnBot`. A
knife-edge today; routine the moment `RespawnDelay` drops below `FuseTime`. Reading the
instigator's Controller's PlayerState closes it.

**N2 — `CopyProperties` carries TeamId but not `ObjectivePoints`**
(`BNPlayerState.cpp:112`), and `GetScore() = Kills + ObjectivePoints`. A seamless travel
with the hill on silently zeroes every objective point while kills survive. Dormant (BN
restarts in place) — but the diff edited exactly this function and left the sibling int out.

**N3 — killfeed relations are baked at push time** (`BNViewModels.cpp:434`) and
`PushKillfeedEntry` drops anything with `InSequence <= LastKillfeedSequence`, so
`HandleAnyTeamChanged`'s re-push cannot recolour. A client whose killfeed ring lands before
its own TeamId renders every pre-join line grey None forever. Cosmetic.

**Verified-handled, so nobody re-checks them:** TeamId ships in the PlayerState's initial
bunch (assignment at `GenericPlayerInitialization` precedes `RestartPlayer`); the client
cheat-write is dead on `HasAuthority()` with no local residue; `CopyProperties` does carry
TeamId; `GetAuthGameMode` is read only behind `BNDamage`'s authority refusal; and all four
`!AreEnemies` sites are now `AreAllies`.

### 2026-08-26 — cloud lead: all four REFUTER findings FIXED (WRITTEN, NOT COMPILED)

- **B1** — the yield is team-aware: when teams are on, `EnsureBotFill`'s shrink pass pops
  the NEWEST bot on the MOST-populated team (populations counted the
  `GetLowestPopulationTeam` way, inverted; tie keeps the lower id; newest preserves the
  named veterans, the tail-pop's own reasoning). FFA keeps the plain tail pop. The
  deterministic 5v3 dies: the joiner crowds a side, the yield uncrowds the same side.
- **B2** — the teams spawn branch runs the engine partition it skipped: candidates a
  default pawn does NOT encroach at are preferred, all-blocked falls back to the full
  pool (spawning clumped beats not spawning). WATCH-LIST for the terminal's compile:
  `UWorld::EncroachingBlockingGeometry(APawn* CDO, Location, Rotation)` — the REFUTER
  named the pass from engine source; my call-shape transcription (arg constness, CDO as
  TestActor) is unverified from this repo.
- **N1** — closed at BOTH ends: `ABNProjectile` captures the thrower's PlayerState at
  BeginPlay (the one moment the pawn link is guaranteed alive; weak, so a leaver
  degrades to the old world-damage answer) and passes it as the damage instigator when
  the pawn route reads null at detonation; the FF gate's resolution is now a LADDER
  that accepts a PlayerState directly, then falls to the pawn. Bonus the fix buys: BN's
  ASC lives on the PlayerState, so spec attribution and the killfeed also survive the
  thrower's death — the "killed by a dead man's grenade" line now names the dead man.
- **N2** — `CopyProperties` carries `ObjectivePoints` beside Kills/Deaths/TeamId
  (GetScore() = Kills + ObjectivePoints; the diff had edited exactly this function and
  left the sibling int out).
- **N3 — ACCEPTED, dated, no code change**: killfeed relations bake at push and the
  sequence dedupe means a pre-join ring line keeps None tints. Aligned with BN16's
  barrier ruling ("feed lines are immutable once pushed; the roadmap's re-tint claim
  names rows"). Cosmetic by the REFUTER's own rating; re-open only if a founder eyes-on
  reads grey pre-join lines as a bug worth a recolour path.
- The two REJECTED verifier passes stand rejected — the re-run protocol is the
  terminal's own (acquisition lines as the instrument, `LogBN Verbose` on so the FF
  refusal line CAN appear; the harness's `ff_refused` counter is already waiting).
