# TICKET — AIB19: the climb — bots grapple up and drop down

> STATUS: BUILT 27 Aug 2026 (cloud lead; WRITTEN, NOT COMPILED). Founder directive
> (the takeover, standing): "the AI should be moving all the time. They should be
> going to climb or drop down." The Gantry is grapple-only by design (GDD §2.6,
> manifest: "no walkable structure reaches height 8") — so before this packet, bots
> could NEVER contest the arena's upper level, and after BN23 landed the Grappleshot
> for humans, bots were the only players locked out of it.

## The behavior

While ROAMING, a bot with Movement >= Trained sometimes walks to a grapple route's
ground approach, aims at its anchor, and fires the Grappleshot **through the same
input path a human uses** — Verb press -> adapter -> `Input.Grapple` ->
`UBNGA_Grapple`, with the server's own range/LOS/cooldown validation judging the
bot like any player. Once up, a bot that finds itself well above a route's approach
sometimes walks to the lip and steps off — the drop back down. Idle bots patrol the
arena in three dimensions; Engage/Seek/Hold are untouched.

## The seams (every one located before building)

1. **The verb** — `AIBTags::Verb_Grapple` ("AIBot.Verb.Grapple"); adapter maps it to
   `Input.Grapple` beside the other nine (`BNAIBAvatarAdapter::MapVerb`). Bots press
   abilities via `ASC->AbilityInputTagPressed` directly, so NO InputConfig row is
   needed for bots — the human row (BN23's queued asset step) is independent.
2. **The door** — `IAIBWorldQuery::GetGrappleRoute(NearLocation, OutApproach,
   OutAnchor)`, default **false** (the AreAllies precedent: hosts without grapple
   routes are byte-identical). FAIRPLAY class: static MAP knowledge, same grade as
   POIs — humans learn anchor spots; nothing perceptual, nothing about enemies.
3. **The route data** — `arena_manifest.json` `grapple_points[]` (GP1–GP5) is the
   source of truth; NOTHING reads JSON at runtime and the metre->cm conversion
   happens once at a committed boundary (the profile's own invariant). So a
   committed generator, `Tools/blockout/gen_grapple_routes.py`, derives
   approach+anchor pairs and writes them into `Config/DefaultGame.ini` under
   `[/Script/BreachpointNext.BNAIBWorldQuery]` (law 7's generated-data idiom;
   regenerate when the manifest changes). Approach = 4 m outward of the lip (the
   notes' south/north token names the direction), z from the named approach surface;
   anchor = the GP point aimed 0.25 m below deck top so the trace hits the face,
   never skims the lip.
4. **The consumer** — `FAIBWanderTask` only, via `MayGrappleTraverse()` (default
   false on `FAIBMoveToPOITask`; Wander overrides true — Seek inherits nothing).
   The whole climb/descend machine lives in the shared task's phase fields; the
   tree asset is UNCHANGED (new instance-data UPROPERTYs default on load; no
   ST_Bot regen, no terminal asset step).

## The machine (phases in instance data, all timers guarded)

- **CLIMB** (entry roll `ClimbChance` 0.35 when grounded, skill-gated, off
  cooldown, door answers, anchor >= `MinClimbRiseUU` 250 above): walk to approach
  (normal locomotion, sprint and wedge-jump included) -> stop, steer the control
  rotation onto the anchor (`AimToleranceDeg` 4, timeout 1.5 s -> whiff) -> ONE
  press+release of the verb -> ride (watch airborne/grounded, timeout 4 s). Landed
  meaningfully above the approach = "made the deck"; anything else logs a whiff.
  Either way the task SUCCEEDS and the branch re-selects — a failed hook can never
  strand a bot (F7: the fallback is exactly the wander it interrupted).
- **DESCEND** (entry roll `DescendChance` 0.5 when the door's route sits >=
  `MinClimbRiseUU` BELOW the bot): walk to the lip (anchor xy at own height,
  nav-projected) -> direct move at the approach point with pathfinding OFF —
  walking off the edge is the mechanism, the fall is the traversal — grounded
  after airborne ends it ("dropped back down"). Timeout 5 s.
- One climb/descend per `ClimbCooldownSeconds` (30; whiffs retry sooner at 10) —
  the GA's own 4 s cooldown backstops spam besides.
- Logs, formats frozen for the harness: `grapples for the high ground`,
  `made the deck`, `the hook did not take`, `steps off the lip`,
  `dropped back down`.

## FAIRPLAY analysis (pre-answered)

- No perception change: no new stimulus, no facts field, no enemy knowledge. The
  door hands back LEVEL GEOMETRY, fixed at build time.
- The press is the same one door (F6); authority validation applies unchanged —
  a bot pressing with a bad angle is REFUSED by the same code that refuses a human.
- Skill-gated at Trained (the CanEvadeBlast/Teamwork precedent): a Recruit-tier
  match never climbs, which is also the countable spec-equivalent for the gate.

## Watch-list (transcription honesty)

- `AAIController::MoveToLocation`'s `bUsePathfinding=false` direct-move overload:
  standard engine API, but this repo has no compiled precedent for the flag. First
  compile (and first drop) tells.
- The landing physics: pull arrival is 120uu short of the face point and the
  momentum-carry over the lip is the same mechanism a human uses, but it is
  UNPROVEN for a stationary bot pull. If live bots stall at the lip: regenerate
  routes with a deeper `ANCHOR_DROP_M` or wider `STANDOFF_M` (generator constants,
  one rerun) — the module needs no change to re-tune.

## Done when

- [ ] All three targets compile (terminal)
- [ ] Module specs green (no new pin: NO spec double implements IAIBWorldQuery —
      the AreAllies precedent pinned its predicate, not the interface default, and
      inventing a UHT-in-spec double for one default is more fragility than proof.
      The default's countable equivalent is live: Recruit-tier logs zero climbs)
- [ ] Live, listen server: a Marine+ bot observed grappling from ground to the
      Gantry deck AND one observed stepping off back to ground (log lines count
      both); a Recruit-tier match logs ZERO climb lines (the gate's proof)
- [ ] Humans unaffected: the BN23 human proof list unchanged (threes)

## Log

### 27 Aug — built (cloud lead; WRITTEN, NOT COMPILED)

- Everything above verbatim; generator run once, ini block committed
  (5 routes: GP1–GP4 Gantry from ground, GP5 Core Top from the south deck).
- DECISION — anchor selection serves both directions with ONE query: the host
  returns the route minimising min(dist-to-approach, dist-to-anchor) from the
  asker, so a ground bot gets its nearest way UP and a deck bot its nearest way
  DOWN without the module naming either.
- The manifest carries EIGHT points, not five: GP6 (spire north), GP7/GP8 (the
  rocket-terrace lips — the manifest's own "fast attack route onto the rocket").
  All eight generated.
- GEOMETRY ANALYSIS, done blind and recorded for the live pass (worked from the
  manifest's boxes, unproven in engine): **GP1–GP4 are clean** — from an 8 m
  stand-off the sightline crosses the gantry slab's underside plane (z 7.6)
  at y≈17.5, BEFORE the slab starts at y 18, so the trace grazes the authored
  lip corner. **GP7/GP8 are whiff-prone as authored**: the mezzanine deck
  (y[12,16], z[3.6,4]) roofs the entire southern approach, and any ray from
  below z 3.6 aimed at the anchor's z 4.0 crosses 3.6 strictly before reaching
  y 16 — it strikes the deck UNDERSIDE ~0.7 m shy of the lip at every stand-off.
  The hook will take, the pull will ride, and the bot will fall short: a logged
  whiff, cost one cooldown, stranding nothing. If the live pass confirms it,
  the fix is the MANIFEST's (the true front lip of the continuous mid level is
  the deck's south edge at y 12, not the drum face at y 16) — file it to the
  manifest owner rather than teaching the generator to second-guess authored
  anchors. GP5/GP6 (spire from the deck, 55°) sit between: steep, corner-hit
  plausible, momentum-carry unproven — the live pass rules.
- Bot activation path VERIFIED against compiled code: the grapple spec carries
  Input.Grapple in its dynamic source tags (BNPlayerState.cpp:230) and
  AbilityInputTagPressed matches HasTagExact on those (BNAbilitySystemComponent
  .cpp:18) — the adapter's press reaches UBNGA_Grapple with no PlayerController
  in the loop.
