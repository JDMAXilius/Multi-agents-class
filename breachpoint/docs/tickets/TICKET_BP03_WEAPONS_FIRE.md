# TICKET — BP03: Weapons — equipment, fire path, and the cheat tests

> STATUS: open — cut by lead session, 29 Jul 2026. Gated by BP02 (GAS core landed).

Founder directive: the FPS-critical path. Client traces for feel, server validates everything
(rate, ammo, cone, range), ONE damage GE for every source. The attack ships with the feature:
this ticket is not done until the forged-fire cheats are written and rejected.

**Ordering law:** Steps 1→2 strictly. Step 3 (cheats) is written WITH step 2, not after.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- Ticket BP02 is DONE: `BRAttributeSet` and the generic GE library compile, and
  `Breachpoint.Sim.Combat` is green
- `Content/Data/DT_Weapons.csv` exists (landed by BP13) and re-imports clean against
  `FBRWeaponRow` — including the `DamageDelivery` column
- owner_path: `Source/Breachpoint/Weapons/`, `Content/Data/DT_Weapons.csv`

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
   impact cues (`Executed`). `BRGA_WeaponUtility` (reload + swap, cancel-clean) — both commit
   on their montage notify events, `Event.Weapon.ReloadCommit` / `Event.Weapon.SwapCommit`
   (ruling R17, declared by BP01); a cancel before the commit event costs and refunds nothing.
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
- Contracts: `gas-purity.md` (abilities + the ONE damage GE) · `netcode.md` (server validation, `COND_OwnerOnly` ammo, the attack ships with the feature) · `data-and-assets.md` (`DT_Weapons` rows, soft refs) · `testing.md` (rungs 2 + 4)
- Binary files owned: `Content/Weapons/*` (sourced meshes), `Content/Data/DT_Weapons.csv`
- Out of scope: grenades, melee, grapple, Rocket row, pickup spawner placement in the map

## Log

(append findings here, dated, newest last)

**31 Jul 2026 — PRE-FILED CONTRACT_GAP (lead, from the BP01 session): `owner_path` misses this
ticket's headline deliverable, plus the systematic `Tests/` hole.**

Current: `Source/Breachpoint/Weapons/`, `Content/Data/DT_Weapons.csv`

| Deliverable | Lives in | Status |
|---|---|---|
| `BRGA_WeaponFire` + `BRGA_WeaponUtility` (step 2 — **the fire path, this ticket's point**) | `Source/Breachpoint/AbilitySystem/Abilities/` | BLOCKED |
| weapon `BRAbilitySet` granted on equip (step 1) | `Source/Breachpoint/AbilitySystem/` | BLOCKED |
| `FBRWeaponRow` — law 3 puts row structs in `BRDataRows.h` (step 1) | `Source/Breachpoint/Data/` | BLOCKED |
| The cheats (step 3) and fire-path specs (step 4) | `Source/Breachpoint/Tests/` | BLOCKED |

Step 1's actors are fine: `BREquipmentComponent`, `BRWeaponInstance`, `BRWeaponPickup` /
`ABRPowerWeaponSpawner` are all §3.5 `Weapons/` residents, and `DT_Weapons.csv` is granted
by exact file.

---

**SYSTEMATIC FINDING — `Source/Breachpoint/Tests/` belongs to nobody.** Recorded here once and
cross-referenced from BP02/BP05/BP06 rather than repeated four times.

ARCHITECTURE §3.12 puts three spec files in `Source/Breachpoint/Tests/` and says
*"sim-builder authors, verifier runs."* But **no ticket's `owner_path` contains `Tests/`**, and
four tickets must write there: BP02 (rung-2 red→green), BP03 (cheats + fire-path specs), BP05
(radial falloff, rear-lethal, grenade refund), BP06 (cooldown-not-consumed-on-rejection).
Every one of those writes is blocked as the board stands.

This is the same defect class as BP01's two corrections, but it is *shared* rather than local,
so fixing it per-ticket four times would be the wrong answer — the second packet to claim would
collide with the first over `BRCombatSpec.cpp`, and law 7's one-owner-per-file rule has no
answer for a file four packets append to.

*Escalated to the lead as a real decision, deliberately NOT settled inline* (options, cost
noted, no recommendation being enacted without the founder): (a) one spec file per packet with
a naming convention, so ownership is per-file and the collision disappears; (b) `Tests/` is
granted to whichever packet is in flight, serialized by the board; (c) specs become their own
follow-on packet per feature, authored by the verifier's counterpart — which collides with the
verifier having no write tools by capability. Option (a) looks cheapest and preserves one-owner,
but it changes §3.12's three-file layout, so it is an ARCHITECTURE amendment, not a ticket edit.
