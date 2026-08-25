# TICKET — Sprint, jump, and melee: the movement-state pattern and the notify-window trace

> **ARCHIVED 25 Aug 2026** — moved off the live board, contents untouched below.
> SUPERSEDED by BreachpointNext R1-R10. This ticket describes in `Source/Breachpoint/` what `Source/BreachpointNext/` has since built and shipped.
> Reversible: `git mv` kept the history, `git log --follow` still reaches it.
>
> STATUS: open — cut 7 Aug 2026. Blocked on BP99 DONE (the game is playable first).

Founder directive: sprint is the pattern-prover. It shows that a movement *state* is a GAS
decision (a tag) while the *motion* is CMC's, and that "firing cancels sprint" is a Halo-feel
rule expressible with **zero code inside the sprint ability**. Melee proves the montage→
gameplay seam: the trace window opens and closes on anim notifies, not on a timer.

**Ordering law:** sprint before melee — melee's `CancelAbilitiesWithTags` references the
sprint tag, and a dangling tag reference is a silent no-op rather than a compile error.

## Kickoff (machine-checkable)

- requires: engine-installed
- BP99 DONE — the shoot/kill/respawn loop is green on rung 4a
- BP96 DONE — `FSavedMove_BR` already declares the sprint compressed flag
- BP94 DONE — `BRDamage::ApplyPoint` accepts a null `WeaponRow` (melee has none)
- owner_path: `Source/Breachpoint/AbilitySystem/Abilities/`, `Source/Breachpoint/Character/`,
  `Source/Breachpoint/Tests/`

## Steps (in order)

1. **[sim-builder]** `Abilities/BRGA_Movement.h/.cpp` — two sibling `UCLASS`es, one pair:
   - **`UBRGA_Sprint`** — activation policy **WhileHeld**. Grants `State.Movement.Sprinting`
     through `ActivationOwnedTags` (predicted and replicated by GAS for free). **No cost**
     (Halo sprint is free — if a later design wants one it is a `GE_AbilityCost` row, not code).
     The ability contains **no speed number and no CMC call**.
   - **`UBRGA_Jump`** — activation policy OnPressed; routes to `ACharacter::Jump`. It exists
     so death blocks jumping through the same `State.Dead` mechanism as every other verb,
     rather than a special case in the controller.
2. **[netcode-builder]** `BRCharacterMovementComponent`: the sprint compressed flag in
   `FSavedMove_BR` is now **set** from the presence of `State.Movement.Sprinting`, and
   `GetMaxSpeed()` multiplies by `SprintSpeedMultiplier` when the flag is set.
   **The flag is the state; the attribute is only the magnitude** (gas-purity 2 Aug
   amendment). A correction must replay the flag, never re-read the attribute.
3. **[sim-builder]** Cancel rules: `BRGA_WeaponFire`, `BRGA_Melee` and (later) `BRGA_Grenade`
   list the sprint tag in `CancelAbilitiesWithTags`. Firing ends sprint with **no code in the
   sprint ability** — verify by reading `BRGA_Sprint.cpp` and finding nothing about firing.
4. **[sim-builder]** `Abilities/BRGA_Melee.h/.cpp`:
   - Trace window **opens on `Event.Melee.WindowBegin`, closes on `Event.Melee.WindowEnd`** —
     both anim notifies. No timer, no fixed delay.
   - Rear arc re-checked **server-side** using `ABRCharacter`'s combat helper (BP96 step 1)
   - Routes to `BRDamage::ApplyPoint` with `Damage.Melee` or `Damage.Melee.Rear`
   - If the montage is missing (Tier-4 asset not yet authored), the editor-only fallback opens
     the window immediately and closes it after a fixed window, logs a warning, and is
     **compiled out of shipping**. Say so in the Log.
5. **[verifier]** Rung 1 (three targets, Server PARTIAL-by-environment). Rung 2:
   `Breachpoint.Sim.Movement` — sprint tag present ⇒ max speed multiplied; tag absent ⇒
   `Super::GetMaxSpeed()`; `SprintSpeedMultiplier` of zero falls back rather than freezing.
   Extend `Breachpoint.Sim.Combat` with rear-melee lethality from the table.
   **Rung 4a**: A sprints, B observes the speed; A melees B from behind and from the front,
   assert damage differs and both views agree. **Rung 4b REQUIRED** — sprint is a predicted
   movement state, so the host path differs; assert authority / host-local / remote separately.
   Re-run 4a under `-PktLag=120 -PktLoss=5` and assert no rubber-band on sprint start/stop.
6. **[critic REFUTER]** Attack surface: does releasing sprint mid-correction leave the flag
   set? Can a client claim a rear hit from the front? Does melee during a reload double-apply?
   Does sprint survive death (it must not — `State.Dead` blocks activation but does the *tag*
   get removed)? What happens if `WindowEnd` never arrives because the montage was interrupted?

## Done when

- [ ] `BRGA_Sprint.cpp` contains no speed value and no reference to firing — read and confirmed
- [ ] Sprint state is a compressed flag in `FSavedMove_BR`; the attribute supplies magnitude only
- [ ] Firing cancels sprint via `CancelAbilitiesWithTags`, with no code in the sprint ability
- [ ] Melee's window opens and closes on anim notifies; `grep` finds no timer in the shipping path
- [ ] The missing-montage fallback is editor-only and compiled out of shipping
- [ ] An interrupted melee montage cannot leave the trace window open — asserted
- [ ] Rung 1 as above; rung 2 `Breachpoint.Sim.Movement` GREEN and PINNED; **rung 4a green
      (clean AND emulated) and rung 4b green**, each asserted in threes
- [ ] Critic REFUTER pass recorded with findings verbatim
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: sim-builder owns both ability pairs; netcode-builder owns the saved-move change and
  both rung-4 axes; critic REFUTERs.
- Binary files this ticket OWNS: none. Melee montage + notifies are Tier 4 (anim packet).
- Out of scope: grenade (BP101), grapple (BP102), crouch (native, already CMC's). Do not add
  a second movement ability "while we are here" — each one is a saved-move flag and a wire
  change.

## Log

(append findings here, dated, newest last)
