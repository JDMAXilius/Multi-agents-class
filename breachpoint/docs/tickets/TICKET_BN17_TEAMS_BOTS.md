# TICKET — BN17: the team framework's bot half (T4 — measurement, not code)

> STATUS: **OPEN — the AIB9 block is CLEARED (28 Aug 2026).** ~~BLOCKED on AIB9~~ (hill
> reachability — bots bank 0 points for traversal reasons, not team reasons; measuring
> team-hill behavior before movement is honest would measure the wrong thing). The
> movement got honest: BN21 replaced the solid stair volumes with 26 walkable treads and
> bots climb them (mid-flight pawns 1 → 16 across 90 PIE samples, footprint hits 5 → 53),
> AIB9's drop half fell 9.54 → 0.42 refusals per switch, and AIB19 put the grapple in bot
> hands for the Gantry. **This ticket can be picked up. Nothing in it has been run.**
> One caveat for protocol B row 4 below: `bHillEnabled` is still False and AIB11's hill
> re-run is itself unrun — turn it on, or that row measures nothing. No code in this
> ticket —
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

### 2026-08-28 — board-hygiene pass: unblocked, unrun

Header corrected only; no protocol was run and no verdict below is answered.

The block is gone (see the STATUS line). Three notes for whoever opens it, all of them
corrections to premises this ticket wrote down while it was waiting:

- **The spec count reconciliation is settled by supersession.** Protocol A asks for
  "121 = terminal's 117 + the fuse pin + the 3 crowd-contract pins" and to re-count after
  AIB9's specs land. AIB9 landed no specs; the suite reads **119/119/0**. Pin 119 and stop
  chasing the arithmetic.
- **`AIBot.Sim.Claims` green POST-AreAllies-flip is satisfied** by that run — it is a
  fresh build, which is the condition this ticket correctly insisted on (a stale binary
  would pass the old polarity silently).
- **Protocol B row 6's OFF-regression leans on AIB12's FFA inertness result, which was
  never measured** — see AIB12's own 28 Aug entry. The claims board has never been
  observed inert in a real FFA match. Either run that grep as part of row 6 or stop
  citing it as a baseline.
