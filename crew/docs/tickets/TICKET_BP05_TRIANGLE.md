# TICKET — BP05: Grenades + melee — completing the Golden Triangle

> STATUS: open — cut by lead session, 29 Jul 2026. Gated by BP02 (GAS core) and BP03
> (fire path). **This ticket closes the gap between BP03 and the M2 fun gate** — BP03
> deliberately excluded grenades/melee; the triangle is not testable without them.

Founder directive: the M2 gate judges shoot → grenade → melee as one rhythm. Both verbs ride
the ONE damage pipeline (`GE_Damage` + tags) — the engine damage API stays banned
(`gas-purity.md` law 3): radial damage is our own overlap query.

**Ordering law:** steps 1 and 2 parallel (different files); 3 needs both; 4–5 close.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- Tickets BP02 and BP03 are DONE (grenade/melee reuse the fire path's damage
  pipeline and the cost/cooldown GEs)
- `Content/Data/CT_Combat.csv` exists and re-imports clean (the triangle's coefficients)
- owner_path: `Source/Breachpoint/AbilitySystem/Abilities/`, `Content/Data/CT_Combat.csv`

## Steps (in order)

1. `BRGA_Grenade`: cook (WhileHeld reuse), server-authoritative projectile spawn (client
   ghost for feel), bounce physics, fuse timer, **our radial damage**: overlap query →
   per-target `GE_Damage` with `Damage.Explosive` + falloff from `CT_Combat`. Grenade count
   as a cost (2 carried, reset on respawn via `GE_InitStats` path). Owner: **sim-builder**,
   **netcode-builder** consults projectile replication (spawn on server, `bReplicates`,
   low NetUpdateFrequency + dormancy after rest).
2. `BRGA_Melee`: notify-window trace, 70 dmg via `GE_Damage` + `Damage.Melee`; **rear-arc
   check server-side** (character helper) → `Damage.Melee.Rear` = lethal in the ExecCalc.
   Sprint-cancel via CancelAbilitiesWithTags proven here. Owner: **sim-builder**.
3. The rhythm pass: swap/fire/grenade/melee interleave cleanly under the input buffer; no
   ability dead-zones; cancel hygiene between all three verbs. Owner: **sim-builder**.
4. Verify: specs extend `Breachpoint.Sim.Combat` (radial falloff exact cases, rear-lethal,
   grenade count refund-on-rollback); rung 4 smoke adds a grenade kill asserted in threes.
   Owner: **verifier**.
5. **Critic REFUTER:** grenade through walls (overlap vs LOS), self-damage rules stated,
   melee through-floor, rear-arc spoofing from client view angles, cook-cancel residue.
   Owner: **critic**.

## Done when

- [ ] All three triangle verbs land kills via the ONE pipeline (grep: zero engine damage API)
- [ ] Radial falloff + rear-lethal spec-proven red→green against `CT_Combat`
- [ ] Cook-cancel and whiff rollback leave zero state (count, cooldown, cues)
- [ ] Rung 4: grenade kill asserted in threes + emulation variant
- [ ] **M2 playtest build tagged** — this ticket ends with the fun-gate build
- [ ] Critic findings addressed or waived in the Log

## Notes

- Crew: sim-builder leads · netcode-builder consults · verifier proves · critic refutes
- Binary files owned: grenade/melee GA assets, `Content/VFX/` grenade set
- Out of scope: Rocket (BP09), grapple (BP06), any UI beyond debug HUD

## Log

(append findings here, dated, newest last)
