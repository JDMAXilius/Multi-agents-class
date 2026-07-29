# TICKET — BP03: Weapons — equipment, fire path, and the cheat tests

> STATUS: open — cut by lead session, 29 Jul 2026. Gated by BP02 (GAS core landed).

Founder directive: the FPS-critical path. Client traces for feel, server validates everything
(rate, ammo, cone, range), ONE damage GE for every source. The attack ships with the feature:
this ticket is not done until the forged-fire cheats are written and rejected.

**Ordering law:** Steps 1→2 strictly. Step 3 (cheats) is written WITH step 2, not after.

## Steps (in order)

1. `BRWeaponInstance` (replicated UObject: `FBRWeaponRow` handle, ammo mag/reserve
   `COND_OwnerOnly`) + `BREquipmentComponent` (two replicated slots, active index RepNotify;
   equip = grant weapon `BRAbilitySet` + async-load **soft** mesh ref; server-validated
   pickup/drop) + `BRWeaponPickup`/`ABRPowerWeaponSpawner` (90 s rocket node, replicated
   countdown). `FBRWeaponRow` (all soft refs) + `DT_Weapons.csv` (AR + Magnum rows; Rocket
   row lands in its own later ticket). Owner: **sim-builder**.
2. `BRGA_WeaponFire`: client trace → `FGameplayAbilityTargetData_SingleTargetHit` in a scoped
   prediction window → batched RPC → **server validation** (rate ≤ RPM+tolerance, ammo > 0,
   direction within cone of server muzzle, range ≤ row max) → `GE_Damage` w/ row damage +
   `Damage.Kinetic[.Headshot]` tags. Predicted cues for muzzle/tracer (`OnActive`), confirmed
   impact cues (`Executed`). `BRGA_WeaponUtility` (reload + swap, cancel-clean).
   Owner: **sim-builder** (ability flow) + **netcode-builder** (validation + replication).
3. **Write the cheats** (test client hooks or spec-level): fire faster than RPM, fire with 0
   ammo, fire 180° off-muzzle, fire beyond range ×2. Acceptance = every one REJECTED
   server-side with the client seeing only a whiff. Owner: **netcode-builder**.
4. Verify: rung 2 (fire-path specs added to `Breachpoint.Sim.Combat`), rung 4
   (`BRGauntlet.SmokeTS2C` now asserts A-shoots-B damage in threes, + emulation run).
   Owner: **verifier**.
5. **Critic REFUTER** independently re-attacks the fire path (its own cheat attempts, not
   step 3's) + reviews ammo replication for information leaks. Owner: **critic**.

## Done when

- [ ] AR + Magnum fire/reload/swap, values from `DT_Weapons` only (zero literals, grep-audited)
- [ ] All four step-3 cheats rejected server-side (test-proven)
- [ ] Headshot ×2 exact per table; whiff rollback leaves zero state (no cooldown, no ammo
      loss client-visible after reconcile)
- [ ] Rung 4 green incl. 120 ms / 5% emulation
- [ ] Critic findings addressed or waived in the Log

## Notes

- Crew: sim-builder + netcode-builder co-own · verifier proves · critic re-attacks
- Binary files owned: `Content/Weapons/*` (sourced meshes), `Content/Data/DT_Weapons.csv`
- Out of scope: grenades, melee, grapple, Rocket row, pickup spawner placement in the map

## Log

(append findings here, dated, newest last)
