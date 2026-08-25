# TICKET — The GAS spine: ASC, AttributeSet, AbilitySet, ability base, the eight generic effects

> **ARCHIVED 25 Aug 2026** — moved off the live board, contents untouched below.
> SUPERSEDED by BreachpointNext R1-R10. This ticket describes in `Source/Breachpoint/` what `Source/BreachpointNext/` has since built and shipped.
> Reversible: `git mv` kept the history, `git log --follow` still reaches it.
>
> STATUS: open — cut 7 Aug 2026. Blocked on BP91 + BP92 DONE. Gates BP94 and everything above.

Founder directive: this is the trust line. Every purity law in `contracts/gas-purity.md`
either becomes true here or becomes a lie for the rest of the project. Attributes mutate only
through GEs; costs and cooldowns ARE GEs; state is a tag applied by a GE; death is a GE.
Eight effects, parameterized — new content is new rows, never new effects.

**Ordering law:** `BRAttributeSet` and `BRGameplayEffects` land together (the set's transition
points apply the effects) and both gate `BRGameplayAbility`, which gates `BRAbilitySet`.

## Kickoff (machine-checkable)

- requires: engine-installed
- BP91 DONE — `BRGameplayTags` declares `State.*`, `SetByCaller.*`, `Event.*`; `BRGameData` resolves `CT_Combat`
- BP92 DONE — `UBRInputComponent` forwards tags to a stub the ASC will take over
- owner_path: `Source/Breachpoint/AbilitySystem/`, `Source/Breachpoint/Tests/`

## Steps (in order)

1. **[sim-builder]** `BRAttributeSet.h/.cpp` — ONE set:
   - Health / MaxHealth, Shields / MaxShields
   - `IncomingDamage` (meta — never replicated, zeroed in `PostGameplayEffectExecute`)
   - movement **magnitudes** only, per the 2 Aug gas-purity amendment: `MoveSpeedBase`,
     `SprintSpeedMultiplier`. **Zero means unset** — a joining client or an attribute-less bot
     falls back to the CMC default, never freezes at zero.
   - `PreAttributeChange` clamps. `PostGameplayEffectExecute` is the **single transition
     point**: shields absorb first then health, apply `GE_RecentDamage`, flip
     `GE_ShieldsBroken` on the 0-crossing (applied/removed ONLY here — never polled), and on
     Health ≤ 0 broadcast `Event.Death` exactly once (re-entrancy guarded).
2. **[sim-builder]** `BRGameplayEffects.h/.cpp` — eight `UGameplayEffect` subclasses in ONE
   header, constructor-authored (R18: GEs are C++ classes, not Content assets):
   `UBRGE_Damage` (SetByCaller.BaseDamage + `Damage.*` dynamic tags) ·
   `UBRGE_Regen` (attribute + SetByCaller.RegenRate, blocked by `State.Combat.RecentDamage`) ·
   `UBRGE_Cooldown` (SetByCaller.CooldownDuration + per-ability tag) ·
   `UBRGE_InitStats` (curve row per archetype) · `UBRGE_RecentDamage` (2.5 s tag) ·
   `UBRGE_Death` (infinite; applies `State.Dead`) · `UBRGE_ShieldsBroken` (infinite,
   structural, no magnitude) · `UBRGE_AbilityCost` (instant, SetByCaller.Cost).
   **Any ninth class is a design change, not an implementation detail** — file it, do not add it.
3. **[netcode-builder]** `BRAbilitySystemComponent.h/.cpp`:
   - `AbilityInputTagPressed/Released(FGameplayTag)` + the input buffer (the buffer lives
     HERE, at the one choke point — not in the controller, not in the pawn)
   - `ReplicationMode::Mixed` for every fighter, humans and bots alike
   - `ServerAbilityRPCBatch` enabled — a shot must be ONE packet (activate + TargetData + end)
   - `SetShieldsBrokenState(bool)` — the only applier/remover of `UBRGE_ShieldsBroken`,
     called from the attribute set's transition point
   - projectile `LocalIndex` minting + registry (`GenerateLocalProjectileId()`,
     `RegisterLocalProjectile`, `ProcessProjectileHit` client RPC). **Declared here, consumed
     by BP101** — put the seam in now so the projectile packet is not also an ASC packet.
4. **[sim-builder]** `BRGameplayAbility.h/.cpp` — the base: activation-policy enum
   (OnPressed / WhileHeld / Toggle), `State.Dead` in `ActivationBlockedTags` (**death disables
   every verb through ONE mechanism** — no `if (bIsDead)` anywhere in the codebase), cost and
   cooldown routed to `UBRGE_AbilityCost` / `UBRGE_Cooldown`, cancel hygiene on `EndAbility`,
   typed accessors (`GetBRCharacter`, `GetBRPlayerState`, `GetEquipment`).
5. **[sim-builder]** `BRAbilitySet.h/.cpp` — `UDataAsset` listing abilities (each with its
   `InputTag`), effects, and attribute sets. `GiveToAbilitySystem()` fills an
   `FBRAbilitySetHandles` out-param; `TakeFromAbilitySystem(Handles)` revokes exactly what was
   given. Unequip never guesses and never iterates "everything that looks like ours".
6. **[sim-builder]** Rewrite `Tests/BRShieldSpec.cpp` against the new names: recharge gate
   (blocked while `State.Combat.RecentDamage`), rate from `CT_Combat`, the 0-crossing applies
   and removes `GE_ShieldsBroken` exactly once each.
7. **[verifier]** Rung 1 (three targets). Rung 2: `Breachpoint.Sim.Shield` +
   `Breachpoint.Sim.Attributes`, both PINNED (exact values, asserted against the DataTable —
   a suite asserting "no crash" is a reportable finding).
8. **[critic REFUTER]** Standing questions for a combat packet: what mutates outside a GE?
   what state is a bool that should be a tag? what applies `State.*` by hand? is `Event.Death`
   re-entrant? Does a rejected ability refund its cost with zero custom code?

## Done when

- [ ] `grep -rn "SetHealth\|SetShields\|Health +=\|Shields +=" Source/` finds hits ONLY inside
      `BRAttributeSet`'s own Pre/Post hooks
- [ ] `grep -rn "AddLooseGameplayTag" Source/` is empty or every hit is marked cosmetic-only
- [ ] `BRGameplayEffects.h` declares exactly 8 `UCLASS`es — asserted by a spec, not by eye
- [ ] `bIsDead` / `bDead` / `IsDead()` appear nowhere; death gates on `State.Dead` only
- [ ] ASC replication mode is `Mixed`; `ServerAbilityRPCBatch` is on — both asserted in a spec
- [ ] `TakeFromAbilitySystem` revokes exactly the handles `GiveToAbilitySystem` returned
      (grant → revoke → assert the ASC is back to its pre-grant ability count)
- [ ] Rung 1 as above; rung 2 `Breachpoint.Sim.Shield` + `.Attributes` GREEN and PINNED
- [ ] Critic REFUTER pass recorded in the Log with its findings, not a summary
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: sim-builder owns the set/effects/base/abilityset; netcode-builder owns the ASC and
  co-signs every replicated property; critic runs REFUTER before the ticket closes.
- Binary files this ticket OWNS: none (GEs are C++ under R18 — if a session reaches for a
  `GE_*.uasset`, that is the finding).
- Out of scope: any concrete ability (`BRGA_*` — Phase 4/5), the damage door (BP94), the
  character (BP96). This packet builds the machine, not the verbs.

## Log

(append findings here, dated, newest last)
