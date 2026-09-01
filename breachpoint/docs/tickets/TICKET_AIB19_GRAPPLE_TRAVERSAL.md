# TICKET — AIB19: the climb — bots grapple up and drop down

> STATUS: BUILT 27 Aug 2026 (cloud lead) — ~~WRITTEN, NOT COMPILED~~ **COMPILED and
> PARTLY PROVEN LIVE, 28 Aug 2026: bots grapple. One match logged 5 ACTIVATED / 6 REFUSED,
> so the verb, the door, the routes and the aim-then-press machine all work end to end.
> OPEN FINDING, not a fix: roughly half of all attempts still fail to reach the standoff
> point.** The descend half and the Recruit-tier zero-climb gate are still unobserved.
> Founder directive
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

- [x] Targets compile (28 Aug, clean; `BreachpointServer` unsatisfiable on this launcher
      install — environmental, AIB1's precedent). The watch-list's unproven flag,
      `MoveToLocation` with `bUsePathfinding=false`, compiled
- [x] Module specs green (119/119/0, 28 Aug) (no new pin: NO spec double implements IAIBWorldQuery —
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

### 2026-08-28 — board-hygiene pass: bots grapple. Half of them do not arrive.

Compile and spec state corrected; the live line below is this session's verified fact,
recorded here rather than re-measured.

- **"WRITTEN, NOT COMPILED" is stale.** All targets clean, AIBot 119/119/0. The
  watch-list's one real unknown — `AAIController::MoveToLocation` with
  `bUsePathfinding=false`, which had no compiled precedent in this repo — compiles.
- **The verb reaches the ability, live.** One match: **5 ACTIVATED / 6 REFUSED** for the
  bot grapple. That closes the whole activation chain the Log above verified by
  inspection (dynamic source tag → `AbilityInputTagPressed` → `HasTagExact` → the GA,
  with no PlayerController in the loop) and it closes it the right way: the REFUSED lines
  are the server's own range/LOS/cooldown validation judging a bot exactly as it judges a
  human. FAIRPLAY F6 held in practice, not just in the design.
- **OPEN FINDING — roughly half of attempts fail to reach the standoff point.** Logged as
  a finding, NOT as a fix, and nothing here should be read as one. The standing suspect is
  already written down two entries above and was written BEFORE the run: the blind
  geometry analysis predicted **GP7/GP8 are whiff-prone as authored** — the mezzanine deck
  roofs the southern approach, so any ray from below z 3.6 strikes the deck underside
  ~0.7 m shy of the lip at every stand-off, and the bot rides a hook that lands it short.
  A prediction agreeing with an outcome is not a diagnosis: **nobody has correlated the
  failures against route id**, and until someone does, GP5/GP6's steep corner-hit and the
  unproven momentum-carry for a stationary pull are equally live. The remedy branches on
  that correlation — if it is GP7/GP8, the ticket's own ruling stands (fix the MANIFEST,
  the true front lip is the deck's south edge at y 12, and file it to the manifest owner
  rather than teaching the generator to second-guess authored anchors); if it is the
  physics, it is `ANCHOR_DROP_M` / `STANDOFF_M` and one generator rerun.
- **Costs nothing while it is broken**, which is why this is a finding and not a stop:
  a whiff logs, burns one cooldown, and the task succeeds anyway — F7's fallback is
  exactly the wander it interrupted. No bot is stranded by it.

Boxes 3 and 4 stay `[ ]`. Box 3 is a THREE-part observation and only the first part
happened: nobody has watched a bot step off the lip, and no Recruit-tier match has been
run to prove the skill gate logs zero climbs. Box 4 needs BN23's human proof list, which
is itself unrun.

### 1 Sep (cloud) — the finding is now MEASURABLE. Instrument landed, diagnosis owed.

The open finding said the remedy branches on a correlation nobody had run, and the
reason nobody had run it was that **the correlation was not loggable**: `GetGrappleRoute`
returned two vectors and no name, so every traverse line named only the bot. Two whiffs
on GP7 and two on GP5 read identically in the log, and they need opposite fixes.

**What changed — the route's own name travels the whole way:**

| Layer | Change |
|---|---|
| `Tools/blockout/gen_grapple_routes.py` | emits `Id="GP1"`..`Id="GP8"` from the manifest's own `gp["id"]` — generated, never hand-typed |
| `Config/DefaultGame.ini` | the block is regenerated; all 8 routes carry `Id=` |
| `FBNGrappleRoute` | a `UPROPERTY(Config) FName Id` — an older ini with no `Id=` still loads and logs `?` |
| `IAIBWorldQuery::GetGrappleRoute` | fourth out-param `FName& OutRouteId`; a host that names nothing may leave it `NAME_None` |
| traverse instance data | `RouteId`, set at the pick, read at every outcome |
| every traverse log line | now carries the route |

**And the failure line was split, which is the other half of the problem.** One message
— "hook did not take" — covered two failures with opposite remedies. `bAirborneSeen`
already knew the difference and nothing asked it:

```
AIBot: <bot> traverse FAILED on GP7 (REFUSED - never left the ground, rose 3uu of 400uu)
AIBot: <bot> traverse FAILED on GP7 (SHORT - rode the hook, landed under the lip, rose 260uu of 400uu)
```

- **REFUSED** = the host's own range/LOS/cooldown validation said no. The bot never
  left the ground, so the **stand-off** is wrong → `STANDOFF_CAP_M` / `DECK_STANDOFF_M`
  in the generator, one rerun.
- **SHORT** = it rode the hook and landed under the lip. The **anchor** is wrong →
  the manifest (and for GP7/GP8 specifically, the ticket's own standing ruling: the true
  front lip is the deck's south edge at y 12, filed to the manifest owner).

Raised from `Verbose` to `Log` deliberately: a whiff IS the measurement, and a
diagnostic that nobody's default verbosity prints is not an instrument.

**The success lines carry the shortfall too** — `made the deck on GP3 (760uu up, wanted
800uu)` — so a route that technically succeeds while consistently landing 40 cm low is
visible before it becomes a whiff.

### What the terminal owes now — one match, one grep, and the branch resolves

```
grep 'AIBot:.* traverse FAILED on ' <log> | sed 's/.*on \(GP[0-9]*\).*(\([A-Z]*\).*/\1 \2/' | sort | uniq -c
```

Tally REFUSED and SHORT per route id and paste it here. Then:

- **failures concentrate on GP7/GP8** → the blind prediction was right, the manifest
  ruling stands, file the anchor fix to the manifest owner. Do NOT teach the generator
  to second-guess authored anchors.
- **REFUSED dominates across routes** → stand-off, not anchors. Retune the generator
  constants and rerun; the C++ needs nothing (the generator's docstring says so).
- **SHORT dominates across routes** → the pull cannot carry the authored rises. That is
  a BN23 tuning conversation, not a bot bug, and it goes to the founder as one.

**Rung: WRITTEN, NOT COMPILED.** The signature change touches four files; only one
call site exists and it is updated. The generator's dry-run output was verified and the
ini block regenerated from it in this session.

