# GAS INTEGRATION — bringing the framework onto a template that already works

**Status: written, NOT executed.** Companion to `PHASE2-RELAYER.md`, which re-layers the INPUT
stack. This file covers what that plan's Step 4 defers as *"blocked on GAS grants"* — the
gameplay framework itself.

## The one rule everything else follows from

**The template works. Our framework does not, yet.** Every claim in this repo above "it compiles"
is unproven, and today alone produced three defects that read as working code. So the integration
is ordered by ONE question: *if this step is wrong, can the founder still play?*

> **Every stage is a switch that defaults OFF, flips ON alone, and reverts in one edit.**
> One stage per test run. Two at once and a regression has two suspects — which is the exact
> hole `PHASE2-RELAYER` was dug to escape, and re-digging it would be the more embarrassing
> mistake because we would have done it knowingly.

**A stage is not "done" when it compiles.** It is done when a named log line appears in PIE **and
the character still moves.** Movement is the canary in every single stage below — if WASD dies,
the stage that just landed owns it, regardless of how unrelated it looks.

## The mechanism: one config-driven stage gate

A single `EBRGasStage` on `ABRCharacter`/`ABRPlayerState`, read from `Config/DefaultGame.ini` so a
regression is reverted **without a rebuild** — which matters because a rebuild needs the editor
closed (R36) and the founder is mid-playtest.

```
Off              nothing GAS-side runs. The template, exactly as it plays today.
AttributesOnly   ASC init + GE_InitStats. Numbers exist; nothing reads them.
Granting         ability sets granted. Abilities exist; no input reaches them.
InputRouted      ability input tags reach the ASC. Abilities activate; most do nothing.
Sprint           the first ability with a visible effect.
Weapons          fire, reload, swap.
FullSandbox      melee, grenade, grapple.
Cues             FX layer last, because it is the only layer that cannot break gameplay.
```

Stages are **ordered by blast radius, not by ticket number.** A stage may only be entered when
the one below it has a recorded PIE observation.

---

## Stage 0 — GATE. The baseline moves.

**This is `PHASE2-RELAYER` Step 0 and it is NOT done.** Nothing below is meaningful until WASD,
mouse, jump and crouch are observed in PIE on a freshly built binary, with
`BRCharacter: FIRST Move input` in the log — a line that has **never appeared in any log this
project has written.**

*Exit:* the founder plays for 60 seconds. Record `movement ready` numbers; every later stage
compares against them.

---

## Stage 1 — `AttributesOnly`. Numbers, and nothing that can move a character.

**Why first:** it is the only GAS stage that is *provably* incapable of breaking movement. It
adds no ability, no input, no effect on the CMC. If movement breaks here, something is very
wrong in a way we need to know about before anything harder.

- ASC init already runs (`InitAbilityActorInfo` on possess — observed in the log today).
- `GE_InitStats` applies Health/Shields/Grenades from `CT_Combat`.

*Log:* `BRAttributeSet: init — Health=100/100 Shields=100/100 Grenades=2/2` (owed; the applier
currently logs a refusal shape but not a success shape).
*Exit:* the numbers print, and the character still moves.
*Known risk:* `ApplyInitStats` refuses the WHOLE application if `Fighter.MaxGrenades` is missing.
The row exists. If that refusal fires, the CSV did not reimport — a data problem, not a code one.

---

## Stage 2 — `Granting`. Abilities exist; nothing can trigger them.

**The blocker is an asset, not code.** `ResolveAbilitySetForRow` reads `DT_Weapons.AbilitySet`,
which names `/Game/AbilitySets/DA_AbilitySet_{AR,Magnum,Rocket}` — **none of which exist.** They
are `UBRAbilitySet` data assets and only the editor lane can author them.

*Log:* `BREquipmentComponent: granted N ability(ies) … for slot 0`.
*Exit:* the grant line prints, and the character still moves.
*Trap, already found once:* the row field is `TSoftObjectPtr`, not `TSoftClassPtr`. A soft CLASS
ref resolves to the CDO and grants nothing while looking correct.

---

## Stage 3 — `InputRouted`. Keys reach the ASC. Most abilities still do nothing.

`PHASE2-RELAYER` Step 4. Press → `AbilityInputTagPressed` → ASC input buffer → activation.

*Log (CORRECTED 1 Aug — the line this originally named does not exist and could not be written):*
the press edge is logged by the **ASC**, not the controller — `BRAbilitySystemComponent … 'InputTag.Fire' PRESSED (edge)`. `BRPlayerController` logs only its *failure* path today.
The exit criterion that actually matters is the **negative** one, which is new:
`… 'InputTag.Fire' PRESSED and matched NO granted ability (N ability(ies) granted on this ASC in total)`.
It names the total granted count because **zero-granted (no set landed) and non-zero (set landed,
tag misspelled) are different bugs** that look identical from the chair.
*Exit:* the tag line prints on keypress, **and movement is unaffected.** Abilities that refuse
(no ammo, no row) must refuse LOUDLY here — this is the stage where a silent refusal would be
mistaken for a dead key, and we have already burned a day on exactly that confusion.

---

## Stage 4 — `Sprint`. The first ability that changes what the player feels.

**Deliberately first among real abilities, and deliberately alone**, because it is the only one
that touches the **CMC** — the same component that owns walking. If sprint is wrong, walking is
what breaks, and that is precisely why it must not share a test run with anything else.

*Log:* `State.Movement.Sprinting` granted; `GetMaxSpeed` returns base × `CT_Combat` multiplier.
*Exit:* hold sprint → faster; release → base speed **exactly**, not approximately.
*Known trust gap (already written down):* the server accepts the sprint bit without checking the
ability is active. Bounded, named in `BRCharacterMovementComponent.h`, not this stage's job.

---

## Stage 5 — `Weapons`. Fire, reload, swap.

Needs Stage 2's ability sets and a target to shoot. **Damage is the first thing here that can
kill the player**, so the order inside the stage is: fire with no target → fire at geometry →
fire at a pawn.

*Log:* `BRGA_WeaponFire REJECTED …` on every refusal — the anti-cheat's only witness until the
cheat specs exist.
*Exit:* ammo decrements, cooldown gates RPM, a hit applies damage through `GE_Damage` and NOT
through the engine damage API (which is hook-blocked anyway).
*Watch:* `Rocket` must refuse — its `DamageDelivery` is `Projectile` and the fire ability rejects
non-hitscan rows by design. A Rocket that fires hitscan means the ability set was mis-authored.

---

## Stage 6 — `FullSandbox`. Melee, grenade, grapple.

Grapple is **last within the stage** and arguably deserves its own: it is the only ability that
**moves the player**, it is `LocalPredicted`, and prediction bugs are invisible in single-process
PIE by construction (`cmc-prediction` §6). A green PIE run is not evidence for it.

*Exit for grapple:* multi-process PIE minimum. Rung 4 with emulation is the real claim, and rung
4 is blocked on the Gauntlet/NuGet failure.

---

## Stage 7 — `Cues`. Last, because it is the only layer that cannot break gameplay.

Handlers are registered and verified by read-back; **no FX assets exist**, so every cue currently
logs once and draws a placeholder marker. That is the correct resting state and it is why this
stage is safe to leave for last.

---

## What is owed before Stage 1 can even start

| Owed | Who | Blocks |
|---|---|---|
| Stage 0 observed — the character moves | founder, PIE | everything |
| `EBRGasStage` + the ini switch | code lane | every stage |
| A success-shaped log line in `ApplyInitStats` | code lane | Stage 1's exit |
| 3 × `DA_AbilitySet_*` assets | **editor lane** | Stage 2 onward |
| `CT_Combat` reimported after today's 29 rows | editor lane | Stages 1, 4, 6 |

## What this roadmap deliberately does NOT do

- **It does not touch the template's working input path.** `PHASE2-RELAYER` owns that, one switch
  at a time, and this plan waits on its Step 0.
- **It does not chase rung 4.** Blocked upstream on the engine's NuGet/SDK failure.
- **It does not add features.** Every unit named here is already written and compiles. This is an
  activation plan, not a build plan — and that distinction is the whole reason it can be safe.
