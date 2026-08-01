# TICKET — BP09: Rocket Launcher — the power weapon

> STATUS: open — cut by lead session, 29 Jul 2026. Gated by BP05 (shares the
> projectile/radial implementation with grenades). Small by design — the amortized weapon.

Founder directive: the rocket is a ROW plus a spawner, not a system: projectile + radial
damage already exist (BP05); this ticket adds the row, the 90 s contested node, and the
two-weapon trade moment. **This is also the first cut if W5 slips** — keep it detachable.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: engine-installed
- Ticket BP05 is DONE (shares the explosive damage path and cook/throw shape)
- `DT_Weapons` contains the `Rocket` row and re-imports clean (`DamageDelivery=Projectile`,
  `ProjectileSpeed > 0` — the invariant this weapon's existence depends on)
- owner_path: `Source/Breachpoint/Weapons/`,
  `Source/Breachpoint/AbilitySystem/Abilities/`

## Steps (in order)

1. `DT_Weapons` Rocket row (120 dmg, r=4 m, 2 shots, projectile speed/arc) + `AS_Weapon_RKT`
   ability set reusing `BRGA_WeaponFire` in projectile mode + grenade radial path with
   `Damage.Explosive`. Owner: **sim-builder**.
2. `ABRPowerWeaponSpawner` live at the arena node: 90 s replicated countdown, arena-wide
   spawn cue, pickup = the standard equipment flow (drop the held slot — the trade).
   Owner: **sim-builder**, netcode-builder reviews countdown replication.
3. Verify + refute: TTK/splash specs; rung 4: rocket kill in threes; critic: rocket-jump
   self-damage rules stated, countdown desync, spawn-camp of the node (arena LOS answer
   logged). Owners: **verifier**, **critic**.

## Done when

- [ ] Rocket contested and traded in playtest (drop-decision observed)
- [ ] Splash falloff spec-proven; countdown identical in threes
- [ ] Detachable: reverting this ticket alone leaves a green build (proven once)
- Crew: sim-builder · netcode-builder (review) · verifier · critic
- Contracts: `gas-purity.md` (reuses `GE_Damage` + the grenade radial path) · `data-and-assets.md` (a row, not a system) · `netcode.md` (spawner countdown replication) · `testing.md` (rung 4 in threes)
- Out of scope: Plasma Rifle (Phase 2), additional power weapons

## Log

(append findings here, dated, newest last)
