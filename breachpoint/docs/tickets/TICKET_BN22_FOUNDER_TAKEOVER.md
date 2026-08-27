# TICKET — BN22: the founder's 27 Aug takeover packet (teams default · team play ·
# traversal · health regen · visible strafe)

> STATUS: landed cloud-side 27 Aug 2026 (WRITTEN, NOT COMPILED) — cut from the founder's
> direct instruction: "take over… teams should be the default… the AI should work as a
> team… traversal/jumping between platforms back, moving all the time… health
> regeneration as a GE, gas purity… make sure they are strafing in an arc." Three
> read-only audits ran first (traversal, GAS wiring, team-play seams); every change
> below transcribes an idiom the audits located, none invents.

## 1. Teams is the DEFAULT — founder ruling, recorded at the value

`bTeamsEnabled=True` (DefaultGame.ini), knowingly overriding the 26 Aug measured revert.
The collapse (12 kills/461 switches vs 38/1329 FFA) is a problem to fix UNDER teams —
items 2 and 3 are that fix — not a reason to develop against FFA.

## 2. Team play — the Rally want (adapter-only; zero AIBot-module edits)

The audit's ranked candidate #1: teammate positions are HUD-grade by the interface's own
ruling (radar shows them), so the adapter publishes the asker's LIVING teammates as
`AIBot.POI.Ally` POIs (Worth 0.3, ZONE, Actor deliberately null — never tempts the
claims board's agents-are-never-slots refusal), and the mode registers
`AIBot.Ambition.Mode.Rally` (BaseUtility 1.0) whose urgency is "how alone am I": zero
inside RallyNearUU=600 (the POIs' ReachRadiusUU — arrival quiets the want, no
no-progress thrash), scaling to a 0.55 cap at RallyFarUU=3000. The cap sits BELOW the
empty hill's 0.6 on purpose — want order is Engage/Seek/Search > Hold > Rally > Roam.
An isolated bot now walks toward its team instead of wandering alone — the collapse's
mechanism (lone wanderers on halved density) attacked directly. FFA-inert by
construction twice over: no ambition registered, no POI published. The existing Mode
tree branch serves the new want by tag hierarchy with no plugin change.

DEFERRED, next team-play packet (audit candidate #2, biggest per-event impact): "move
toward friendly gunfire" — a separate matured hearing channel (never target memory),
new fact + selector, Teamwork-gated. Cut it as its own ticket when this packet proves.
Also deferred: hill guard-slot POIs (#3) until the hill is re-enabled.

## 3. Traversal — audit verdict: the machinery is INTACT; the block is elsewhere

`bGenerateNavLinks=True`, BN_Drop JumpLength 400 / JumpMaxDepth 1000 (the AIB9 fix) all
unchanged since b2d315f; the re-issue is exonerated by A/B; the wedge-jump is ungated;
no kill-switch cvar anywhere in Config. Bots CAN drop between tiers. What is missing is
UPWARD traversal: the proven 13-tread stairs (16 mid-flight pawns vs 1) exist only in an
editor that closed — World Partition OFPA needs a level save no MCP tool provides.
**Terminal, one action: run `Tools/blockout/bn21_stairs_mcp.py` against a live editor
and SAVE THE LEVEL BY HAND once.** (Or fix build_arena.py's commandlet spawning —
its own packet.) "Moving all the time" is items 2+5 plus that save.

## 4. Health regeneration — GAS-pure, the codebase's own settled idiom

The audit found the complete precedent pair (UBNGE_ShieldRecharge + UBNHealthComponent's
tag-event gate — OngoingTagRequirements was REJECTED twice in writing for the
CDO-construction-order reason; law 8, not re-litigated). Landed:

- `UBNGE_HealthRegen`: infinite periodic +2.5 Health per 0.1s (25/s, full bar ~4s —
  deliberately slower than the ~1s shield; disengaging buys recovery, not a reset).
  Both existing clamps (current + BASE) cap it at MaxHealth.
- Its window: a SECOND spec of UBNGE_RecentDamage per landed hit, own tag
  `State.Combat.HealthRegenDelay` (a SIBLING of RecentDamage — a child's count would
  propagate into the parent query and silently hold the shield down for the longer
  health window), own Config knob `HealthRegenDelay=5.0` on the attribute set.
- The gate in UBNHealthComponent: one handler over TWO tags — the window AND
  `State.Dead`. The dead half is load-bearing: the corpse outlives its lethal hit's
  window on a persistent ASC, and an ungated regen resurrects it (the death-latch
  reset hazard the audit traced). Belt on `bDeathReported` for the frame before the
  tag lands. `bHealthRegenEnabled` Config kill-switch, the shield switch's contract.
  EndPlay strips handle + listeners off the CACHED ASC (the corpse-leak cure).
- Bot loop closes with ZERO AI changes (audit §8): Retreat scores on HealthNorm
  (high want below 0.6), regen raises it, confidence rises with it, Engage outbids —
  retreat → heal → re-engage emerges from existing considerations.

## 5. Strafe visible — footwork owns the fight range

Landed just before this ticket (commit "footwork owns the fight range"): FightRangeUU
(900) on BOTH the strafe and MoveNearBelief; inside it with a visible target the mover
stands down (releasing its held sprint) and the arc strafe owns the legs; the spiral
clamp rebands [280, 900] with the chord ratchet kept as deliberate closing pressure.
Param renamed so the authored tree's stale serialized 350 drops to the new default.

## 6. SHIELDS ON (founder, 27 Aug: "everything we need for shield and shield
## regeneration, but even more time since not taking damage")

The 13-Aug pause ends the way its own comment promised: MaxShield and the Shield init
back to 100 in UBNGE_InitAttributes, nothing else — recharge GE (100/s), RecentDamage
window, State.Shields.Broken, the HUD's HasShields() gate and the shield-then-health
drain were all built and waiting. The founder's delay: `ShieldRechargeDelay=7.0` in
DefaultGame.ini (the Config knob built for this), deliberately LONGER than the health
regen's 5s — health recovers first, a full shield needs a real disengage. And the edge
the pause was hiding is closed: the recharge now shares the health regen's dead gate
(one recomputing handler over the window tags + State.Dead) — a corpse neither heals
nor recharges, whatever the window/respawn timing knife-edges do.

## Terminal proof list

- [ ] Rung 1 all targets; spec suites green (no new specs in this packet — the regen
      gate is delegate-driven; eyes-on + log proof below is its rung)
- [ ] Stairs: bn21_stairs_mcp.py + ONE hand level-save; then nav proves itself (bots
      climb; `offmesh_self`/refusal counters vs the AIB9 baseline)
- [ ] Teams ON five-match run: kills/switches vs the 12/461 collapse baseline —
      Rally + strafe + traversal are the three levers; measure with all three in
- [ ] Rally visible: isolated bots walk toward teammates (Mode want lines with
      Rally tag); FFA run shows zero Rally wants (inert check)
- [ ] Regen visible: `BNDamage:` log lines show health RISING between fights
      (health x -> y with y > x across hits); a corpse never regens (kill, watch 5s,
      health stays 0 until respawn); suppression under sustained fire holds
- [ ] Shields visible: damage lines read `shield 100 -> N` before health moves; the
      shield bar renders (HasShields flipped the HUD gate); after a clean 7s the shield
      refills at ~100/s; health starts refilling at 5s — BEFORE the shield window ends;
      a corpse's shield stays down; ShieldsBroken raises at 0 and clears on recharge
- [ ] Strafe: `strafe leg` lines at ranges 350-900 (the old gate's impossible band);
      `strafe_denied_seconds` collapses vs the 26 Aug instrument baseline

## Log

**27 Aug — W-REVIEW ×2 (bn-critic GAS/lifecycle: PASS, 1 medium 3 lows; aib-critic
fairness/behavior: BLOCK, 1 high 3 mediums 3 lows). All highs and mediums FIXED at the
barrier; register below.**

- **H1 (aib) / M1 (bn), one root — Rally statued at a stale snapshot of a moving ally.**
  FAIBMoveToObjectiveTask snapshotted its goal once (correct for a hill, wrong for a
  pawn): a bot "arrived" at a teammate's abandoned spot and stood while the live
  urgency kept the want winning — a 10-60s statue reproducing the isolation collapse
  as standing. FIXED: the pick is a helper re-run on a 0.5s cadence in Tick; the hill
  re-picks itself byte-identically (nothing resets), a moved ally re-aims the walk,
  progress tracking resets only when the goal moved >50uu (the no-progress law keeps
  its teeth), and an empty re-pick fails loudly (the ally died — F7 + suppression).
  Worth ties now break NEAREST (L2 folded in).
- **M1 (aib) — the sag band**: a 0→0.55 urgency ramp dipped under Roam's floor at
  ~1470uu, so a mid-rally bot abandoned the approach a kilometre short and hovered in
  an annulus. FIXED: floored at 0.3 the moment it is nonzero (lerp 0.3→0.55); exactly
  0 inside the near radius stays, so arrival still quiets the want.
- **M2 (aib) — the radar premise**: ally positions are HUD-grade but no teammate HUD
  marker exists yet in this build. RECORDED as a dated FAIRPLAY amendment (accepted on
  the genre premise, no enemy derivative rides it — the critic verified) with THE DEBT
  named: land the teammate marker (BN11 family), then revisit.
- **M3 (aib) — the 350-beeline survived at EnterState**: Engage re-enters per belief
  blink and each re-entry issued a close-to-350 request the Tick yield never aborted.
  FIXED: EnterState mirrors the fight-range yield.
- **L1 (aib)** all-teammates-dead urgency churn → urgency now 0 with no living ally.
  **L3 (bn)** regen zero-max gate added (the shield's HasShieldPool twin).
- **REGISTER (accepted, dated)**: bn-L1 a dead man's grenade forms no AIB memory
  (dropped at the Neutral attitude — not poisoned; rare, AI-only); bn-L2 two
  RecentDamage GE instances per hit — perf/bandwidth note for rung 5, no stacking
  policy set; aib-L3 a corpse can hold sprint until branch exit (movers' verbs ride
  persistent grants — outside the fire task's gate scope, cannot feed BN20's warning);
  aib-P8's verify hook: add hit-rate-at-range to the strafe proof (weapon rows live in
  a binary table — behavior is the check).
- Passes worth keeping: a corpse cannot regen through ANY ordering (three independent
  gates, each sufficient); no persistent-ASC regen leak; the respawn sweep covers the
  new window tag with correct ordering; PlayerState-as-instigator breaks no BN
  consumer and self-damage survives; Rally's laundering attack held (no enemy
  derivative crosses; Actor=null verified end-to-end); FFA-inert twice over; the 600
  accordion has a ~900uu geometric dead-band (no oscillation); teams-ON + hill-OFF
  coherent; the corpse press gate reads live state and re-acquires cleanly.
