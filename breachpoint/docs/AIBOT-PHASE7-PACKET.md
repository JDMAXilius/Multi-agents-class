# PHASE 7 — W-AUDIT barrier record + rulings + build (the claims board)

> 26 Aug 2026. Two auditors (architecture+containment / fairness+utility), read-only,
> against the frozen contract in `Source/AIBot/Team/AIBTeamCoordinator.h`. Both scopes
> returned constraints, no contradictions after one merge (below). Build landed the same
> day — WRITTEN, NOT COMPILED. Open items were CLOSED on the audits' recommendations
> under the founder's roadmap-completion directive (26 Aug, "continue until we finish
> the roadmap completely"); any of them can be reopened by ruling.

## The design, as built

- **`Team/AIBClaimsBoard`** — the HEADLESS core (the Perception/ pattern): claims array,
  time as a parameter, hostility as an injected predicate. Key = backing actor, else
  (Kind, 100uu cell). Claim = key + opaque claimant + weak claimant pawn + expiry.
- **`Team/UAIBTeamCoordinator`** — the thin world-subsystem shell: supplies time and the
  hostility predicate (from the manager's world query; NO query registered ⇒ all pairs
  enemies ⇒ nothing ever binds — inert, never colluding), refuses client worlds and
  PAWN-BACKED targets (agents are not slots — a shared enemy-assignment board is
  F-3.7's coordinated omniscience). Grants and denials log; renewals stay quiet.
- **Negative-only comms**: the read surface is one boolean per target
  (`IsClaimedByOtherTeammate`). No claimant position, roster, or intent crosses. A claim
  can only SUPPRESS a target the reader independently knows — never teach one.
- **Slot vs zone**: `FAIBPointOfInterest::bClaimableSlot` (provider-declared, default
  false). Zones — BN's hill — are refused at the board, structurally: zeroing a zone's
  want is how a team mode gets a one-defender hill. A zone that wants N bodies exposes
  N slot-POIs.
- **The one scoring seam**: `FAIBObjectiveFact::bClaimedElsewhere` (PRESENT-zero, set by
  the facts builder only when matching POIs exist and every one is an other-claimed
  slot) → new selector `ObjectiveClaimedElsewhere` → a falling consideration attached at
  the same translation site that keeps mode wants honest (`BuildModeAmbitionSpec`).
  Claimed ⇒ the product is EXACTLY 0 ⇒ the engine's zero-score veto releases a
  committed loser in one rescore. Task-side mirror: `FAIBMoveToObjectiveTask` skips
  other-claimed slots at the pick, or a bot honours the claim at scoring and walks to
  the claimed slot anyway.
- **Filing**: in `Think()`, synchronously after `Rescore` — think timers serialize on
  the game thread, so the same-frame lobby cluster resolves in strict order with no
  data race; filing at task-enter would open a multi-frame window where the whole
  cluster commits to one slot. The claim renews (TTL 5s, `AIB::ClaimTtlSeconds`) while
  the want keeps winning.
- **Lifecycle belts**: TTL lapse (= the drift release: non-renewal, never an instant
  release a flapping claimant could weaponize against teammates' arbitration);
  stale-pawn-reads-expired (death); `ReleaseAll` on OnUnPossess AND EndPlay (ordered
  paths); lazy pruning — no tick (the module's one per-frame surface stays the
  executor's).
- **Tier gate, SYMMETRIC**: Teamwork ≥ Trained to file AND to honour. A Novice neither
  calls out nor listens — novices packing onto one rocket is honest novice play, and it
  is what makes Teamwork tiers observable in Phase 8. Default profile ships
  Teamwork=Novice ⇒ the whole board is dormant until a tier raises it.

## The one merge between the two audit scopes

Architecture's seam zeroes the mode WANT via the objective fact; fairness forbids
zeroing a ZONE objective's want. Reconciled by `bClaimableSlot`: the flag can only go
true when the kind's matching POIs are all claimable slots and all spoken for — one
zone POI in the set keeps the want alive whatever the slot book says.

Second merge: architecture wanted `ReleaseAll` on ambition drift; fairness wanted a
minimum hold ≥ commit windows (board transitions slower than the engine's anti-dither
machinery). Reconciled: death/unpossess release immediately; drift releases by
NON-renewal at the 5s TTL, which IS the minimum hold.

## Rulings closed 26 Aug 2026 (on audit recommendations, under the directive)

1. Contract's "~0" → **exactly 0, present-zero** (the tilde decided whether the veto
   works; the stub comment is amended in place).
2. Tie-break = **first-come at the board**, a dated acceptance (server think order makes
   it stable; revisit only if "the same bot always gets the rocket" becomes observable).
3. Renewal by **route-persistence**, not re-perception (strict re-perception re-couples
   the board to the sensorium for marginal gain; expiry catches the rest).
4. Mixed-lobby packing by low tiers is **intended**, not residual (the roadmap's row-7
   promise is scoped to Teamwork-competent bots).
5. Tier gate is **symmetric** (file AND honour).
6. TTL = **5s**.
7. Claimables this phase: **board generic over POIs, wired through the Mode path only**;
   Seek/pickup wiring waits for a host with pickups (and the recorded Seek-POI
   divergence: `FAIBMoveToPOITask`'s header promises POI consumption its code does not
   do — whoever wires it inherits the claim filter obligation).
8. Actor-less key quantization = **100uu**.

## The honest phase proof (roadmap row 7, re-scoped)

BN today has no teams (all-hostile world query) and no pickups (hills are zones). "Two
bots never contest one pickup — measured" is therefore NOT claimable live in this host.
What Phase 7 proves now:

- **Headless**: `AIBot.Sim.Claims` — 11 specs (grant/deny arrival order; renewal +
  self-pass; zone refusal; enemies-never-bound = FFA inertness; TTL lapse; death
  release; key stability; exactly-zero + one-rescore veto release + absence-vs-flag;
  default-Novice dormancy). Module spec total: **108**.
- **Live (negative instrument)**: an FFA PIE match logs ZERO claim grants; with a
  raised-Teamwork test row and no team mode, still zero (nothing claimable). Countable
  absences, in AIB12.
- **Deferred**: the two-allied-bots-one-pickup measurement waits for a host mode where
  `AreEnemies` answers false for some pair AND ≥2 claimable slots exist — realistically
  Phase 9's harness or a team-mode adapter. The roadmap row carries the annotation.

## Audit findings not built, registered

- The Seek-POI divergence (arch finding 10) — recorded above under ruling 7.
- Boundary hygiene: module comments say "an all-hostile (FFA) host", never the host's
  name (arch finding 12) — followed in the build.
- Confidence-spec knife-edge re-pin and the other P4+5 risk-register items stand
  unchanged (docs/AIBOT-P45-REVIEW.md).
