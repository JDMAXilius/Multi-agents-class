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
- [ ] The degenerate cheat test green (a client-side TeamId write never replicates up)

## Log

_(outputs verbatim)_
