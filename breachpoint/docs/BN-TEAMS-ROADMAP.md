# BN TEAMS — the Team Deathmatch framework roadmap

> Founder-ordered 26 Aug 2026, from `docs/BN-TEAMS-PACKET.md` (the OnSight
> reverse-engineering + the BN integration audit, merged). The packet is the DESIGN and
> stays authoritative on every mechanism; this file is the ORDER, the proofs, and the
> tickets — the AIBOT-ROADMAP's shape, applied to teams. The honesty rungs apply
> unchanged: written → compiled → specs → PIE → listen+client; multiplayer claims come
> in threes (server, acting client, observing client) — and TEAMS ARE THE FIRST FEATURE
> SINCE THE SCORES WHOSE WHOLE POINT IS REPLICATED STATE, so rung 5 is not optional
> here the way a server-only bot brain let it be.

## Prime decisions (settled in the packet; re-litigating them is a finding)

1. **Native or nothing**: `FGenericTeamId` on `ABNPlayerState` is the ONE team datum —
   a replicated byte, authority-set, `NoTeam`=255 the only sentinel, pawn and bot
   controllers DELEGATE. No team subsystem, no team DataAsset, no int accessor.
2. **One choke point**: `BNTeams::AreActorsFriendly` with the NoTeam guard (the engine's
   default solver calls NoTeam-vs-NoTeam FRIENDLY — unguarded, FFA blocks all damage).
   The helper is the only caller of `GetAttitude` in game code — grep-enforced.
3. **One damage door**: friendly fire is refused at the top of `BNDamage::ApplyDamage`,
   before any GE spec exists. GAS purity untouched; cues never fire on refused hits.
4. **`bTeamsEnabled=false` ships FFA byte-for-byte** — every phase below proves the OFF
   case before the ON case.
5. **Bots through the doors**: the AIBot module learns teams through
   `IAIBWorldQuery::AreEnemies` + `CountNearbyAllies` and ONE pre-sanctioned attitude
   edit. Nothing else inside `Plugins/AIBot/` changes for teams, ever.
6. **No new RPCs.** BN ships zero Server RPCs today; the team layer keeps it that way.

## Phases

| Phase | Deliverable | Proof |
|---|---|---|
| T0 | **Identity + assignment**: `TeamId` on PlayerState (OnRep, delegate, `CopyProperties`), interface on PlayerState/pawn/BN-bot-controller, `BNTeams` helper, balanced assignment in `GenericPlayerInitialization`, Config `bTeamsEnabled`/`bFriendlyFire` | rung 1 all targets; `Breachpoint.Sim`-style spec for the helper's attitude table incl. the NoTeam guard; OFF-regression (one FFA match, zero behavioural diffs); ON: a 4v4 lobby logs eight assignments landing 4/4 by population |
| T1 | **Combat honesty**: FF gate in the damage door; kill-credit re-check (no team-kill credit); BN bot targeting via attitude; AIB doors (`AreEnemies` team compare, real `CountNearbyAllies`) + the one module attitude edit | FF refused count > 0 in an ON match while self-grenade still damages; acquisition lines never name a teammate (both bot systems); claims board: FIRST-ever live `claim GRANTED`/`DENIED` lines (teams make slots claimable between allies) and AIB12's FFA-inert result still holds OFF |
| T2 | **TDM the mode**: team scores + `WinningTeamId` on GameState (OnReps); kill → team point; buzzer + score-limit team win through `FinishMatch`'s team sibling | a full 4v4 TDM match ends on team score with the winner announced from the replicated ints; leaver's points survive (the record is the ints, never a PlayerArray sum) |
| T3 | **Team spawns**: `ChoosePlayerStart` filtering `PlayerStartTag` Team0/Team1 with Super fallback; committed tagging script for the arena's 8 starts | spawn-side table from one match: zero cross-tag spawns; NoTeam (late-join frame) falls back to engine choice, never crashes |
| T4 | **Team objective**: the hill's team stanza — same-team occupants do not contest, sole TEAM scores, team objective points; depends on AIB9's reachability work for the bot half | two same-team bodies on the hill bank points; cross-team contests; the AIBot roadmap's DEFERRED row-7 measurement runs AT LAST: two allied bots, one claimable slot, contested-pickup count 0 over N trials |
| T5 | **Team UI**: transcribe the BR-module prior art — relative friendly/enemy color off `OnTeamChanged` (deferred subscription for the replication race), two-block scoreboard, killfeed tints, match band | eyes-on protocol + the OFF case renders today's FFA HUD untouched |
| T6 | **Rung 5 — multiplayer in threes**: listen server + 2 clients over the whole stack | per netcode law: team seen correctly by server/acting/observing; the degenerate cheat test (a client-side TeamId write never replicates up); late-join reads NoTeam honestly for a frame, then the real id |

Dependency notes: T1 needs T0. T2/T3 need only T0. T4 needs T2 and is BLOCKED on the
hill reachability (AIB9 — bots currently bank 0 points for traversal reasons, not team
reasons). T5 needs T2. T6 runs once T0–T3 are green and is the framework's DONE bar.

## Tickets

- **BN15 — T0+T1 in one packet** (the packet's own recommendation: identity without
  combat honesty is untestable, and the 8-edit list is one feature). Carries the
  netcode obligations: new replicated property = netcode packet + critic REFUTER.
- **BN16 — T2+T3** (the mode + its spawns; both read only `TeamId`).
- **BN17 — T4** (team hill + the row-7 claims measurement; opens when AIB9 closes).
- **BN18 — T5** (UI transcription from BR prior art).
- **BN19 — T6** (the rung-5 threes protocol; the framework's acceptance ticket).

Tickets are cut when their phase starts, not before — the packet holds the design so
nothing is lost in the meantime.

## The metrics harness rides along

`Tools/aib/80_aib_metrics.py` gains, with BN15: an assignment-line counter (per-team
population), an FF-refused counter, and a team-kill-credit-denied counter — countable
events for every proof above, so no phase's claim rests on an impression. The bars land
in `BREACHPOINT-QUALITY-BARS.md` §7's table when the first five-match team baseline
exists, not before (the AIB14 discipline: no bar without a spread).

## Founder items already open (packet §Open, unchanged)

FF default (rec: off) · team count (rec: hard 2 now) · the module attitude edit
(rec: take it in BN15) · scope (rec: T0+T1 as one packet — reflected in BN15 above).
A "go" on this roadmap closes all four as recommended unless overridden line by line.
