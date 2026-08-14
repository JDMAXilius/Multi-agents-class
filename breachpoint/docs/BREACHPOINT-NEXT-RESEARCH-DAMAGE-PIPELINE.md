# The damage pipeline — what it must carry, what it already does, and the one thing it loses

**Cut:** 13 August 2026 by the cloud lead, at the founder's request: *"deep research and
ultrathink… advanced programming, multiplayer, GAS purity, best practices — and do not
overcomplicate. It's actually not that hard."*

That last sentence is the design constraint, and it is correct. This document's job is as much to
say what we are NOT building as what we are.

---

## 0. First, the question the founder opened with: is respawn handled properly as a GA?

**Death is a GameplayAbility and should be. Respawn is not, and should not be.** The asymmetry is
principled, not an accident:

- **`UBNGA_Death` is a GA** because dying is something that happens *to a fighter's state* — it
  cancels every verb, holds `State.Dead`, executes `GameplayCue.Character.Death` (the ragdoll,
  multicast to every machine). An ability is exactly the right container: activation IS the state
  change, `EndAbility` IS the cleanup, and staying active IS what holds the tag.
- **Respawn is the GameMode's**, because when it runs there is *no fighter*: the corpse is
  destroyed, no avatar exists, and the work is match bookkeeping — pick a start, make a pawn,
  possess it. That is the contract's own named exception (*"match meta… server-only mutation in
  GameMode"*). A respawn GA would need an ASC with no avatar and would activate on a body that does
  not exist yet. The GAS-shaped parts of respawn — the `State.*` sweep, the init GE — already go
  through GAS.

So: death = GA + cue ✓, respawn = GameMode reacting to the death announcement ✓. Verified, no
change needed there.

---

## 1. What the founder asked the pipeline to carry, against what it carries today

| Requirement (founder's list) | Today | Verdict |
|---|---|---|
| **That you got damaged** | `UBNGE_Damage` → `IncomingDamage` meta attribute → drain. Replicated Health is what tells every machine | ✓ works |
| **The amount** | SetByCaller magnitude; headshot multiplier applied inside the door (`ApplyWeaponDamage`) | ✓ works |
| **Where you were hit** | `Context.AddHitResult(Hit)` — fire, melee and the grenade all pass their server-validated hit | ✓ carried… |
| **Who killed you — instigator and victim, so the kill is credited** | `Context.AddInstigator` is set, reaches `PostGameplayEffectExecute`, is printed in the log line — **and is then thrown away** | ✗ **THE GAP** |

The chain today: `PostGEE` drains shields→health, logs `instigator -> target, amount`, and returns.
`UBNHealthComponent::OnDeath` fires carrying only *itself*. `BroadcastDeath()` carries only the
victim. `HandlePlayerDeath` respawns. **At no point after the drain does anyone know who did it.**
Kill credit, the killfeed, scoring — all impossible until this one piece of information survives
three hops.

## 2. The design: capture at the one reaction point, read at the one death point

The fix is small because the architecture is already right. There is exactly ONE place every point
of damage passes through (`PostGameplayEffectExecute`, server-only) and exactly ONE place death is
decided (the same function is where Health hits zero). So:

```
PostGameplayEffectExecute (AUTHORITY, the one reaction point)
   ├─ capture: LastDamage { Instigator, HitResult, Amount }   ← from the spec's context, NEW
   ├─ drain: shields first, then health                        (unchanged)
   └─ log                                                      (unchanged)

UBNGA_Death (AUTHORITY)
   └─ read LastDamage off the attribute set → BroadcastDeath(Killer)   ← NEW

ABNPlayerState::OnPlayerDeath(Victim, Killer)                          ← signature grows
   └─ ABNGameMode::HandlePlayerDeath → respawn (unchanged) + THE KILL LINE:
      "BNGameMode: <Killer> eliminated <Victim>"
```

Properties of this shape, checked against the founder's three requirements:

- **Multiplayer:** every step is authority-only. Instant damage GEs execute only on the server;
  `PostGEE` therefore never runs this code on a client; the capture is plain non-replicated fields.
  What clients learn, they learn the way they already do — replicated Health, replicated tags, the
  multicast death cue. **No new replication, no new RPC.**
- **GAS purity:** the instigator travels inside the `FGameplayEffectContext`, which is GAS's own
  channel for exactly this. Nothing new mutates an attribute; nothing bypasses the door. The
  attribute set still never talks to game flow — it *records*; the death GA *reads*.
- **Not overengineered:** one tiny struct, one read, one widened delegate signature. No new class,
  no new subsystem, no damage-event bus.

**Why capture on the attribute set and not broadcast a rich event from `PostGEE`:** the skill's own
rule — *"the attribute set never talks to game flow directly."* Recording a fact is not talking;
the death GA choosing to read that fact is where flow belongs. It also means the SAME capture
serves the next packet free of charge: **hit reactions need the hit's direction, and it is already
sitting there** — `LastDamage.HitResult` on the victim's set, refreshed by every landed hit, not
just the lethal one.

**Edge cases, decided now so they are not rediscovered:**
- **Self-kill** (grenade at your feet, `BNKillSelf`): instigator == victim. The kill line says
  "eliminated themselves"; when scoring exists, a suicide is credited to nobody.
- **No instigator** (world damage, a future hazard): killer is null, the line says "died". The
  delegate signature allows null on purpose.
- **Killer disconnected between the hit and the death** (grenade in flight when they quit): the
  capture is a weak pointer; it resolves null and degrades to the "died" case instead of dangling.
- **Stale capture:** `LastDamage` could be minutes old if the victim then dies to something that
  never routed damage (there is no such path today — falling out of the world would be one later).
  Accepted and recorded; the cure (a timestamp window) is cheap to add when such a path exists.

## 3. What we are deliberately NOT building, and the trigger that would change each answer

This is the "do not overcomplicate" half, made concrete:

| Not building | Why not | Build it when |
|---|---|---|
| **An ExecutionCalculation** (`BRDamageExecCalc`, which the contract names) | Today's math is `min(shield, dmg)` then the remainder — `PostGEE` holds it in six lines that are built and reviewed. An ExecCalc earns its existence when damage must CAPTURE attributes from both sides (armor, damage buffs, falloff curves) | The first source-side stat that modifies damage. The door's callers never change either way — that was the point of the door |
| **Damage-type tags** (`Damage.Bullet`, `Damage.Blast`…) | Nothing reads them: no resistances, no type-specific reactions | The first consumer. Adding tags nothing reads is the definition of overengineering |
| **A damage-taken GameplayCue / floating numbers** | UI roadmap. The HUD does not exist; a cue with no presentation is dead weight | With the HUD (Checkpoint on hitmarkers). The seam it will need — amount + location on the victim, server-truth — is exactly the `LastDamage` capture |
| **Assists** | Needs damage HISTORY (who dealt what within N seconds), which is a real data structure with real pruning | When scoring exists and the founder asks |
| **Team/friendly-fire rules** | No teams exist | With teams, as a check inside the door — one place, which is why the door exists |
| **A kill-confirmation RPC to the killer's client** | The killer's machine learns from replicated state already; a dedicated "you killed X" channel is HUD work | With the hitmarker/killfeed HUD pass |

## 4. Sequenced next, per the founder: hit reactions

Named here so the seam is visible: **modular hit-reaction GA**, activated on the victim, playing
directional `AM_MM_HitReact_*` montages (~20 exist in FPSTemplate, nothing plays them). The
capture this packet adds is its input — direction comes from `LastDamage.HitResult`. The likely
shape is a passive GA triggered by a gameplay event from `PostGEE` — which will be the moment to
decide whether the "attribute set records, others read" rule gets its one sanctioned event. That
decision belongs to that packet, not this one.
