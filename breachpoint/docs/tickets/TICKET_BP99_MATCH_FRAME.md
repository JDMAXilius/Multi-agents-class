# TICKET — The match frame: phases, respawn, kill attribution, scores, killfeed

> STATUS: open — cut 7 Aug 2026. Blocked on BP98 DONE. **Closes the golden triangle:
> shoot → kill → respawn → repeat.**

Founder directive: GameMode is server-only and never replicates. Respawn is not a rebuild —
it re-possesses a fresh pawn and re-points the ASC's avatar, so attributes and granted
abilities survive by construction. The clock is not replicated: the server publishes an end
*timestamp* and every client derives its own countdown.

**Ordering law:** respawn (step 2) lands before scoring (step 3) — a scoring rule that fires
during a broken respawn is untestable.

## Kickoff (machine-checkable)

- requires: engine-installed
- BP98 DONE — a shot kills, `Event.Death` reaches the GameMode seam, rung 4a+4b green
- BP95 DONE — `ABRPlayerState` carries team + K/D/A with `OnRep` delegates
- owner_path: `Source/Breachpoint/Match/`, `Content/Data/`

## Steps (in order)

1. **[netcode-builder]** `Match/BRGameState.h/.cpp`:
   - `EBRMatchPhase` + **`MatchEndServerTime`** (a timestamp, not a remaining-seconds float);
     clients compute the countdown from `GetServerWorldTimeSeconds()`
   - team scores as a small replicated array with `OnRep` → delegate
   - killfeed as a **ring buffer** `TArray<FBRKillFeedEntry>` with `OnRep` — bounded, never
     growing, never a per-event multicast
2. **[netcode-builder]** `Match/BRGameMode.h/.cpp` — respawn first:
   - Phase machine driven by **timers and events**, never Tick
   - `OnDeath(Victim, Killer, DamageTags)` bound to the `Event.Death` seam from `BRAttributeSet`
   - Respawn: pick a start, spawn a fresh `ABRCharacter`, possess, `InitAbilityActorInfo`
     re-points the avatar, **remove `GE_Death`, re-apply `GE_InitStats`**, clear per-life
     tags. Assert in a spec that granted abilities and K/D **survive**; that is the whole
     point of the ASC living on the PlayerState.
   - `DropAll()` on the dead pawn's equipment — GameMode decides when, the component obeys
3. **[netcode-builder]** Kill attribution:
   - killer from the damage effect context's instigator; suicide and environment give no credit
   - **double-KO**: both die in the same frame → both get a kill and both get a death.
     Test this explicitly; it is the case every shooter gets wrong once.
   - team score updates route through GameState; K/D/A through the victim's/killer's PlayerState
4. **[builder]** Bot fill: call the existing `AI/BRBotManagerComponent` across the current
   seam. **Do not modify `AI/`** (D-3 default: out of scope). If the seam does not fit, file a
   `contract_gap` and stop — do not reach into another owner path.
5. **[sim-builder]** `Content/Data/DT_MatchRules.csv` — phase durations, score limit, respawn
   delay. Zero literals in `BRGameMode.cpp`.
6. **[verifier]** Rung 1 (three targets, Server PARTIAL-by-environment). Rung 2:
   `Breachpoint.Sim.Match` — phase transitions on timer expiry, score limit ends the match,
   double-KO credits both, respawn preserves granted abilities and K/D. **Rung 4a**: A kills
   B; assert in threes that the killfeed entry, both scores and B's respawn are consistent
   across server, A and B. Kill B three times and assert no ability leak (ability count on
   B's ASC is identical after each respawn).
7. **[critic REFUTER]** Attack surface: what if the killer disconnects between the damage and
   the death event? Does a respawn during phase-end double-spawn? Does the ring buffer wrap
   correctly with 3 entries and a 16-slot buffer? Can a client observe `MatchEndServerTime`
   drift on a mid-match join?

## Done when

- [ ] `BRGameMode` has no replicated property and no `Tick` — grep clean on both
- [ ] The match clock is a replicated **timestamp**; no per-second RPC and no replicated float countdown
- [ ] Respawn preserves granted abilities and K/D — asserted by count before/after, ×3 respawns
- [ ] Double-KO credits both players — a committed test, not a claim
- [ ] Killfeed is a bounded ring buffer; wrap behaviour asserted
- [ ] Zero gameplay literals in `Match/*.cpp` — all values from `DT_MatchRules.csv`
- [ ] `AI/` is untouched; bot fill goes through the existing seam or a `contract_gap` is filed
- [ ] Rung 1 as above; rung 2 `Breachpoint.Sim.Match` GREEN and PINNED; **rung 4a green,
      asserted in threes**
- [ ] Critic REFUTER pass recorded with findings verbatim
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: netcode-builder owns GameMode + GameState; builder assists on flow; sim-builder owns
  the rules table; critic REFUTERs.
- Binary files this ticket OWNS: `Content/Data/DT_MatchRules.uasset`.
- Out of scope: `AI/` internals, UI scoreboard binding, session lifecycle (`Online/`), the
  remaining abilities (BP100–BP102). **At the end of this ticket the game is playable** —
  that is the gate, and the next thing that happens is a fun test, not more code.

## Log

(append findings here, dated, newest last)
