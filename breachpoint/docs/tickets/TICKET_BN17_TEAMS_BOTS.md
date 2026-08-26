# TICKET — BN17: the team framework's bot half (T4 — measurement, not code)

> STATUS: **BLOCKED on AIB9** (hill reachability — bots bank 0 points for traversal
> reasons, not team reasons; measuring team-hill behavior before movement is honest
> would measure the wrong thing). OPENS the day AIB9 closes. No code in this ticket —
> BN15 already landed the mechanisms; this is the W-VERIFY wave that proves them.
> Order: docs/BN-TEAMS-ROADMAP.md T4; the AIBot roadmap's DEFERRED row-7 measurement.

## Wave plan — W-VERIFY ×2 (one verdict per protocol, no protocol half-run)

Config for BOTH protocols: `bTeamsEnabled=True`, 2v2 bots (Trained+ tier — Novice
Teamwork keeps the board inert and would vacuously pass), the arena map, five-log
baseline discipline (`Tools/aib/80_aib_metrics.py`, the AIB14 rule: no bar without a
spread).

### Protocol A — specs (aib-verifier)

- Module suite green at its expected count (reconcile note: 121 = terminal's 117 + the
  fuse pin + the 3 crowd-contract pins landed 26 Aug; re-count after AIB9's own specs
  land and pin the new number here).
- `AIBot.Sim.Claims` all green POST-AreAllies-flip (the predicate helpers inverted at
  the BN15 review barrier — a stale binary would pass the old polarity silently, so
  build fresh, then run).
- ~~NEW spec to write~~ DONE EARLY (26 Aug, lead — headless work, never blocked by
  AIB9): the `bCrowdKnown` contract is pinned in `AIBConfidenceSpec.cpp`'s "the crowd
  contract" Describe — a real `NearbyAllies` with the flag down changes nothing, the
  selector reads ValueWhenUnknown, and the flag coming up moves exactly the -0.10
  outnumbered term (aib-critic LOW 3 closed).

### Protocol B — live log counts (aib-verifier + the harness)

1. **Row-7 claims (THE deferred measurement)**: two ALLIED bots, one claimable slot
   (weapon pickup), N=10 trials. PASS: `claim GRANTED` ≥ 1 per trial, contested-pickup
   count 0 (never two allied bots converging on one granted slot), `claim DENIED`
   lines name the non-holder. The harness's claim counters are already live.
2. **Cross-team inertness**: same slot, one bot per team. PASS: BOTH bots may hold a
   claim on the same slot simultaneously (each alliance runs its own book); zero
   cross-team DENIED lines.
3. **Corpse window (the BN15 fix's live proof)**: kill the allied claimant, watch the
   surviving ally. PASS: within min(TTL 5s, respawn) the slot is claimable again, and
   a DEAD ENEMY's claim never suppresses (the AreAllies flip — was the collusion bug).
4. **Team hill**: two same-team bots on the hill bank points (no contest lines);
   cross-team occupancy contests. Counted from the hill stanza's own lines.
5. **Teammate targeting**: zero acquisition lines naming a teammate across all logs
   (both bot systems — AIB acquire lines and BN bot lines).
6. **OFF-regression rerun**: one FFA baseline after everything above — team counters
   all `none (FFA?)`, claims inert (AIB12's result still holds), harness class
   unchanged against the stored five-log baseline.

## Done when

- [ ] Both protocols report, verdicts in this Log verbatim
- [ ] The bCrowdKnown spec landed and the spec count re-pinned
- [ ] Quality-bars §7 gains the first team-match baseline row (five logs, spread)

## Log

_(outputs verbatim)_
