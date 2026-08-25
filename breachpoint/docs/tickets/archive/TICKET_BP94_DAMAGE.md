# TICKET — The one damage door: BRDamage, the exec calc, and the cue handlers

> **ARCHIVED 25 Aug 2026** — moved off the live board, contents untouched below.
> SUPERSEDED by BreachpointNext R1-R10. This ticket describes in `Source/Breachpoint/` what `Source/BreachpointNext/` has since built and shipped.
> Reversible: `git mv` kept the history, `git log --follow` still reaches it.
>
> STATUS: open — cut 7 Aug 2026. Blocked on BP93 DONE. Gates every packet that deals damage.

Founder directive: purity law 3 says one damage pipeline. The reference audit showed what a
codebase looks like when that is a convention instead of a structure — ZoransResistance grew
**five** damage entry points, each re-implementing spec construction, with one bug
copy-pasted into two of them, and an execution calculation that reads the magnitude and does
nothing with it. We make the law structural: **one namespace with two functions is the only
thing in this codebase that may build a damage spec.** The engine damage API stays banned.

**Ordering law:** `BRDamage` and `BRDamageExecCalc` land together — the door is meaningless
without the room. Cues may land after, but before BP98.

## Kickoff (machine-checkable)

- requires: engine-installed
- BP93 DONE — `UBRGE_Damage` exists, `BRAttributeSet` has `IncomingDamage` as a meta attribute
- `Content/Data/CT_Combat.csv` re-validates and `BRGameData::EvalCombatCurve` returns from it
- owner_path: `Source/Breachpoint/AbilitySystem/`, `Source/Breachpoint/Tests/`

## Steps (in order)

1. **[sim-builder]** `AbilitySystem/BRDamage.h/.cpp` — a namespace, not a class, with exactly
   two public functions and no others:
   ```cpp
   struct FBRDamageRequest {
       AActor* Instigator; AActor* EffectCauser;
       float BaseDamage; FGameplayTagContainer DamageTags;
       const FBRWeaponRow* WeaponRow;   // nullable — melee and hazards have none
   };
   namespace BRDamage {
       void ApplyPoint (const FBRDamageRequest&, const FHitResult&);
       void ApplyRadial(const FBRDamageRequest&, const FVector& Origin, float Radius);
   }
   ```
   Both build the same `UBRGE_Damage` spec, set `SetByCaller.BaseDamage`, add the dynamic
   `Damage.*` tags, and apply to the target ASC. `ApplyRadial` is the sanctioned replacement
   for the banned `ApplyRadialDamage`: sphere overlap → **per-body-section** visibility trace
   (not one trace to the actor origin — that is what makes cover work) → falloff curve → one
   GE per victim. Friendly fire is gated by `BRTeams::GetAttitude`, once, here.
2. **[sim-builder]** `AbilitySystem/BRDamageExecCalc.h/.cpp` — the one execution and the
   **only place damage math happens**: read `SetByCaller.BaseDamage` → `Damage.*` tag
   multipliers from `CT_Combat` → body-section modifier from the weapon row → absorb into
   Shields → overflow to Health via `IncomingDamage`. **Weak point is derived** (any section
   modifier > 1.0), never a declared flag. No literal multiplier in this file — `2.0f` for a
   headshot is a curve row, not code (law 4).
3. **[sim-builder]** `AbilitySystem/BRGameplayCues.h/.cpp` — cue handler **classes** (R18),
   the only thing in the module that spawns FX, plays sound, or shakes camera. Predicted
   presentation on `OnActive`/`WhileActive` (GAS rolls them back on misprediction);
   confirmed-only one-shots (kill toast, shield break) on `Executed`, server path. Asset refs
   are soft and resolved through `BRGameData`.
4. **[verifier]** Add the grep gate to the rung-2 run and record it in
   `contracts/testing.md`'s gate table:
   `grep -rn "MakeOutgoingSpec.*Damage\|TakeDamage\|ApplyRadialDamage\|ApplyPointDamage\|FDamageEvent" Source/`
   — the only permitted hits are inside `BRDamage.cpp`. **Any other hit is a `high` finding
   and blocks the landing.**
5. **[sim-builder]** Rewrite `Tests/BRCombatSpec.cpp` (`Breachpoint.Sim.Combat`), PINNED
   against `DT_Weapons.csv` + `CT_Combat.csv`: TTK per weapon row · headshot math ·
   shields-absorb-then-overflow · falloff at 0 %, 50 %, 100 % range · radial falloff with a
   blocker between origin and victim (must deal zero) · friendly fire off.
6. **[critic REFUTER]** Attack surface named: can any ability reach a target ASC without
   going through `BRDamage`? Can `ApplyRadial` damage through a wall? Does a section modifier
   of exactly 1.0 register as a weak point (it must not)? Is `IncomingDamage` observable by a
   client?

## Done when

- [ ] `BRDamage` has exactly two public functions; nothing else in `Source/` builds a damage spec
- [ ] The grep gate is wired into rung 2 and listed in `contracts/testing.md`'s gate table
- [ ] `BRDamageExecCalc.cpp` contains no numeric literal used as a multiplier
- [ ] Weak point is computed from the modifier; `> 1.0f` exactly, asserted at the 1.0 boundary
- [ ] `ApplyRadial` deals zero through a blocker — asserted in a spec, not observed in PIE
- [ ] No FX/audio/camera-shake call outside `BRGameplayCues` — grep clean
- [ ] Rung 1 (three targets, Server PARTIAL-by-environment); rung 2 `Breachpoint.Sim.Combat` GREEN and PINNED
- [ ] Critic REFUTER pass recorded with findings verbatim
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: sim-builder builds; critic REFUTERs; verifier owns the grep gate wiring.
- Binary files this ticket OWNS: none. Cue *assets* (Niagara/MetaSound) are Tier 4 and belong
  to a later art packet — the handler classes here reference them softly and tolerate null.
- Out of scope: abilities. Nothing calls `BRDamage` yet in this packet except its own specs.
  If you find yourself writing `BRGA_*` to test it, stop — the spec applies the GE directly.

## Log

(append findings here, dated, newest last)
