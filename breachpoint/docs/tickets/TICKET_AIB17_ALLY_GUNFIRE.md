# TICKET — AIB17: converge on a teammate's fight — the ally-gunfire channel

> STATUS: BUILT 27 Aug 2026 (cloud lead) — ~~WRITTEN, NOT COMPILED~~ **COMPILED, 28 Aug
> 2026: all targets clean, AIBot suite 119/119/0. The watch-list item resolved with it —
> `FAIStimulus::Tag` is real and reads.** The BN22 barrier cleared and the held build
> landed the same day, to this design verbatim. **UNPROVEN LIVE: no run has counted an
> `ally fight heard` or `wandering toward the team's fight` line yet.**
> Law: FAIRPLAY F1-F8; the two guarded channels are F-4.5 (a sense added later must not
> silently become vision) and F5-C (a friendly must never become a stimulus/target).

## The behavior

A teammate's gunfire, HEARD within the bot's own hearing envelope, pulls an idle bot
toward that fight. Enemy gunfire already does this (noise → matured memory → Search);
a TEAMMATE's own shots are today dropped whole at the hostility filter
(`AIBBotController.cpp:~527` — correctly, so a friendly footstep cannot evict enemy
memory). The fix is a separate, smaller door at the same site — never a target.

## The design (transcription-grounded, every seam located)

1. **The tap** — in `OnPerceptionUpdated`'s Note boundary, BEFORE the non-Hostile drop:
   a HEARING stimulus from a FRIENDLY whose `Stimulus.Tag` is a weapon-fire tag becomes
   an ally-fight note, then falls through to the existing drop. The tag exists: BN
   reports `BNWeaponFire` (BNGA_Fire.cpp:97) and `BNGrenadeBlast` (BNProjectile.cpp:284)
   on every authority shot/blast. The module must not name BN's tags (boundary law) —
   the HOST maps them: add `IAIBWorldQuery::IsWeaponNoiseTag(FName)` default-false (the
   AreAllies precedent — defaulted, adapter implements) OR simpler: note ANY friendly
   hearing stimulus and let loudness/decay carry it (footsteps are quiet + the envelope
   is short — decide at build with aib-critic's input; the tag door is the safer cut).
2. **The memory** — controller-side (`FAIBAllyFightMemory`, beside the movement state):
   ONE point + stamp, newest-wins, decays after ~8s. Matured on the reaction clock (F1)
   like every stimulus; NEVER enters `FAIBTargetMemory` (F5-C) and is its own kind, not
   a sighting (F-4.5).
3. **The gate** — Teamwork skill at note time (the CanEvadeBlast precedent: a Novice
   never even receives): Novice deaf, Trained+ notes. Scaling headroom: Skilled+ could
   sprint the approach later; not in the first cut.
4. **The consumer** — `FAIBWanderTask` (Roam) only: with a fresh ally-fight point, the
   wander destination draws from a radius AROUND the heard point instead of around
   self (range-capped by the existing WanderRadiusUU so it never becomes a cross-map
   teleport of intent). Roam stays the 0.2-floor want — this changes WHERE idle bots
   wander, never WHETHER something real outbids idling. No new ambition, no new
   selector, no facts change — the smallest surface that produces "bots show up to
   their teammate's fight".
5. **Instrument** — one Verbose line at note time (`ally fight heard — %s at %.0fuu`)
   and one when a wander biases (`wandering toward the team's fight`), countable, exact
   formats frozen for the harness before the terminal measures.

## FAIRPLAY analysis (pre-answered for the critic)

- Perception-bounded end to end: the note exists only if the bot's own ears (2200uu
  default) heard it; matured on the reaction clock; decays; position is the HEARD
  point, not the ally's live location afterward.
- Never a target: the note carries a PLACE, no actor; consumed only by Roam's
  destination draw. The existing hostility filter still drops the stimulus itself.
- The enemy-knowledge derivative (a fight's location correlates with an enemy):
  identical in kind to the ENEMY-gunfire path that already exists and to a human
  hearing their teammate's rifle — bounded by the same ears.

## Wave plan (after the BN22 barrier)

Serial (lead): the memory struct + controller field + note-time tap and gate.
Then ONE builder: wander consumer + adapter tag-door + instrument lines.
Then aib-critic, one dimension: the two guarded channels above.

## Done when

- [x] Rung 1 (all targets clean, 28 Aug); module specs green (119/119/0). The promised
      "+1 pin" is NOT in that count — the Log below moved it to the live protocol on
      purpose (the tap lives in the perception handler, out of headless reach), so a
      Recruit-tier match logging zero ally-fight lines is the outstanding equivalent
- [ ] Live: idle bots converge on a staged fight (log lines count it); FFA unchanged
      (friendly = nobody in FFA — the tap is unreachable by construction)
- [ ] The 26 Aug collapse metric re-run with Rally + this: kills/switches vs 12/461

## Log

### 27 Aug — built, to the design verbatim (WRITTEN, NOT COMPILED)

- The tap took the SAFER cut (the tag door): `IAIBWorldQuery::IsWeaponNoiseTag`
  default-false, adapter answering BN's two authority tags (BNWeaponFire,
  BNGrenadeBlast) — footsteps stay outside. Friendly ONLY at the boundary: Neutral
  (scenery, a dead man's PlayerState-attributed blast) stays fully dropped.
- `FAIBAllyFightMemory` (AIBTypes.h): one place + stamp, 8s fresh window, newest-wins,
  reset at possession. Controller-owned, one writer (the tap), one reader (the wander).
- The consumer: the idle wander's destination draw re-centres on the heard place with
  a 0.4x spread — still a navmesh RANDOM point, never a beeline (F6); unreachable
  heard-points fall back to the plain self-centred draw.
- The log fires once per fight-heard SPELL, not per shot — countable without drowning.
  Harness: ally_fights_heard + wanders_to_fight, proven on synthetic lines.
- WATCH-LIST (transcription): `FAIStimulus::Tag` is read nowhere else in this repo —
  the engine field ReportNoiseEvent's Tag parameter lands in. First compile tells.
- The spec pin promised in Done-when ("Novice never notes") moved to the LIVE protocol:
  the tap lives in the perception handler, out of headless reach — a Recruit-tier match
  logging zero ally-fight lines is the countable equivalent.

### 2026-08-28 — board-hygiene pass: compiles, and the transcription risk is retired

Corrected, not measured.

- **"WRITTEN, NOT COMPILED" is stale.** All targets build clean; AIBot 119/119/0.
- **The watch-list is closed by that build.** `FAIStimulus::Tag` — "read nowhere else in
  this repo", the one thing in this packet that could have been a phantom field — compiles
  and reads. `IsWeaponNoiseTag`'s default-false door and the adapter's two answers
  (BNWeaponFire, BNGrenadeBlast) are real code.
- **Nothing about the BEHAVIOUR is proven.** A compiled tap is a tap nobody has heard
  fire. Both remaining boxes are live and unrun.

**Partial credit, recorded honestly rather than ticked:** the third box asks for the
26 Aug collapse metric re-run "with Rally + this", against 12 kills / 461 switches. Since
then, teams went ON by default and the rally-deadlock fix landed, and **ambition switches
recovered 461 → ~1200** — back to the teams-off level. That is half the box: the switches
half, measured with Rally and the strafe rebanding and the traversal fix all in at once.
The **kills** half is unmeasured, and no run has isolated this ticket's contribution from
the other three levers. Box stays `[ ]`.
