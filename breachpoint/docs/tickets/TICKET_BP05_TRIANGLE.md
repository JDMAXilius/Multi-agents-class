# TICKET — BP05: Grenades + melee — completing the Golden Triangle

> STATUS: open — cut by lead session, 29 Jul 2026. Gated by BP02 (GAS core) and BP03
> (fire path). **This ticket closes the gap between BP03 and the M2 fun gate** — BP03
> deliberately excluded grenades/melee; the triangle is not testable without them.

Founder directive: the M2 gate judges shoot → grenade → melee as one rhythm. Both verbs ride
the ONE damage pipeline (`GE_Damage` + tags) — the engine damage API stays banned
(`gas-purity.md` law 3): radial damage is our own overlap query.

**Ordering law:** steps 1 and 2 parallel (different files); 3 needs both; 4–5 close.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: engine-installed
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
   check server-side** (character helper) → the PAIR `Damage.Melee` + `Damage.Rear` = lethal in
   the ExecCalc.
   > Corrected 31 Jul 2026 by **ruling R22**: this line previously read `Damage.Melee.Rear`,
   > a nested tag that does not exist. `Damage.*` is flat — types (`Kinetic`/`Explosive`/
   > `Melee`) and modifiers (`Headshot`/`Rear`) are siblings that compose, and the ExecCalc
   > queries each axis independently. Grepping for `Damage.Melee.Rear` will find nothing;
   > that is correct, not a missing tag.
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
- Contracts: `gas-purity.md` (law 3 — radial damage is OUR overlap query; the engine damage
  API stays banned) · `netcode.md` (server-authoritative projectile spawn, dormancy at rest) ·
  `animation.md` (melee notify window raises `Event.Melee.WindowBegin`/`WindowEnd` — ruling
  R17; declared by BP01, consumed here) · `testing.md` (rung 4 in threes)
- Binary files owned: grenade/melee GA assets, `Content/VFX/` grenade set
- Out of scope: Rocket (BP09), grapple (BP06), any UI beyond debug HUD

## Log

(append findings here, dated, newest last)

**31 Jul 2026 — PRE-FILED CONTRACT_GAP (lead, from the BP01 session), plus an ARCHITECTURE
gap that is NOT an owner_path problem and cannot be fixed at claim time.**

Current: `Source/Breachpoint/AbilitySystem/Abilities/`, `Content/Data/CT_Combat.csv`

| Deliverable | Lives in | Status |
|---|---|---|
| Rear-arc check — the ticket says **"character helper"** in so many words (step 2) | `Source/Breachpoint/Character/` | BLOCKED |
| Grenade count as a cost, reset on respawn via the `GE_InitStats` path (step 1) | `Source/Breachpoint/AbilitySystem/Effects/` | BLOCKED |
| Radial-falloff / rear-lethal / grenade-refund specs (step 4) | `Source/Breachpoint/Tests/` | BLOCKED |

Note the second row: owning `AbilitySystem/Abilities/` does **not** grant `AbilitySystem/Effects/`
— `guard_laws.py` matches on a `startswith(o + "/")` prefix, and `Effects/` is a sibling of
`Abilities/`, not a child. Sibling folders under a shared parent are a recurring trap in this
board's owner_paths; check for it at every claim.

`CT_Combat.csv` is granted by exact file, so the falloff numbers are reachable. Good.

---

**ARCHITECTURE GAP — the grenade projectile has no home, and neither does the rocket's.**

Step 1 requires a server-authoritative projectile with bounce physics and a fuse. ARCHITECTURE
§3.5 `Weapons/` enumerates exactly three units — `BREquipmentComponent`, `BRWeaponInstance`,
`BRWeaponPickup`(+`ABRPowerWeaponSpawner`) — and **none of them is a projectile.** No other §3
folder claims one either. BP09 (rocket) inherits the same hole: its ticket says the rocket "is a
ROW plus a spawner, not a system: projectile + radial damage already exist (BP05)" — so BP09 is
explicitly relying on a class that §3 never allocates.

This is **not** fixable by amending `owner_path` at claim time, which is why it is filed
separately and loudly: you cannot grant a path to a file the architecture never named. It needs
an ARCHITECTURE §3 amendment naming the projectile's home (`Weapons/` is the natural reading,
taking the folder from 3 units to 4) and an owner assigned — likely **sim-builder** for the
ballistics with **netcode-builder** on spawn/replication/dormancy, matching how step 1 already
splits its owners.

*Escalated to the lead; deliberately not decided inline.* A packet that invents a home for a
class the architecture does not name is exactly the improvisation law 5 exists to prevent.

See BP03's Log for the systematic `Tests/` finding — four tickets need it, no ticket owns it.
