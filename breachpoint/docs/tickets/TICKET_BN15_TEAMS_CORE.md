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
