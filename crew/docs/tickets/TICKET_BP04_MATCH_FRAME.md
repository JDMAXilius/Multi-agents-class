# TICKET — BP04: Match frame — mode, state, scoring, respawn

> STATUS: open — cut by lead session, 29 Jul 2026. Gated by BP02 (death events exist);
> runs in parallel with BP03 (different owner paths).

Founder directive: the server-only match spine. Phase machine on timers and events (no Tick),
kill attribution with the edge cases DECIDED (double-KO both credit, no-instigator = −1 self),
one replicated end-time (clients render the clock locally), scored respawns.

**Ordering law:** Steps 1→2→3. Step 4 after 2.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- Ticket BP02 is DONE and the `Event.Death` path fires from
  `PostGameplayEffectExecute` (this ticket consumes it)
- BP00 rung 4 (`BRGauntlet.SmokeTS2C`) can run — match flow claims are networked claims
- owner_path: `Source/Breachpoint/Match/`

## Steps (in order)

1. `BRGameState`: `EBRMatchPhase` (Warmup→Live→SuddenDeath→PostMatch) RepNotify,
   `MatchEndServerTime` (one float — no ticking replication), team scores, killfeed ring
   buffer (`FBRKillFeedEntry` RepNotify → delegate). `BRPlayerState`: TeamID, K/D/A
   RepNotify. Owner: **netcode-builder**.
2. `BRGameMode` (server-only): phase timers; kill handling from `Event.Death`
   (attribution: last damaging instigator ≤ 5 s, else −1 self; simultaneous killing blows
   credit both), team score → win checks (25 / 8:00 / sudden-death 60 s cap + damage
   tiebreak per `DT_MatchRules`); **scored respawn** (farthest-from-threat over manifest
   spawn points, 5 s timer). Owner: **netcode-builder** (authority) + **builder** (flow).
3. `BRPlayerController`: death cam (view target = killer, 5 s), respawn request path,
   UI intent boundary stubs. Owner: **builder**.
4. Verify + refute: spec `Breachpoint.Sim.Match` (attribution table: normal, steal,
   double-KO, self, environment; tiebreak ladder) added to rung 2; rung 4 extends the smoke
   (kill A→B updates all three views' scoreboards). **Critic REFUTER:** kill-trade races,
   score during Warmup, death during phase transition, respawn into sudden death.
   Owner: **verifier**, **critic**.

## Done when

- [ ] Full match completes vs. nothing (empty arena, timer path) AND via kill path to 25
- [ ] Attribution table spec-proven (incl. double-KO both credit)
- [ ] Clock renders identically on server + 2 clients from one replicated float (rung 4)
- [ ] Sudden death caps at 60 s with damage tiebreak (spec-proven)
- [ ] Critic findings addressed or waived in the Log

## Notes

- Crew: netcode-builder leads · builder assists · verifier proves · critic refutes
- Contracts: `netcode.md` (authority gate, RepNotify-as-cosmetic, one replicated clock float) · `gas-purity.md` (match meta is a NAMED exception — server-only, never read by combat sim) · `testing.md` (rungs 2 + 4)
- Binary files owned: none (BR_Arena01 blockout belongs to the arena ticket)
- Out of scope: bots (own ticket), teams UI, medals, Spotter

## Log

(append findings here, dated, newest last)
