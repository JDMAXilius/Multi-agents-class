# TICKET — BP03: Weapons — equipment, fire path, and the cheat tests

> STATUS: open — cut by lead session, 29 Jul 2026. Gated by BP02 (GAS core landed).

Founder directive: the FPS-critical path. Client traces for feel, server validates everything
(rate, ammo, cone, range), ONE damage GE for every source. The attack ships with the feature:
this ticket is not done until the forged-fire cheats are written and rejected.

**Ordering law:** Steps 1→2 strictly. Step 3 (cheats) is written WITH step 2, not after.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: engine-installed
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

---

**1 Aug 2026 — STEP 1 WRITTEN (sim-builder, parallel-pod packet). Code only; NOT compiled
(ruling R21: four builders in the tree, one build lock). No rung claimed.**

Landed: `Data/BRDataRows.h` (created — did not exist; `FBRWeaponRow` + `EBRWeaponFireMode` +
`EBRDamageDelivery`), `Weapons/BRWeaponInstance.{h,cpp}`, `Weapons/BREquipmentComponent.{h,cpp}`,
`Weapons/BRWeaponPickup.{h,cpp}` (`ABRWeaponPickup` + `ABRPowerWeaponSpawner`).

`FBRWeaponRow` matches all 16 columns of `DT_Weapons.csv`; column 1 (`Name`) maps to the row
KEY, not a member, per the DataTable importer. `ValidateSchema()` passes on all three shipped
rows (checked by hand against the CSV, not by running anything).

contract_gaps opened by this step — each blocks something later, none worked around:
1. **No `AbilitySet` column in `DT_Weapons.csv`.** Step 1's "equip = grant the weapon's
   `BRAbilitySet`" has no data source. `ResolveAbilitySetForRow()` refuses and logs; the grant
   call site is one commented function in `BREquipmentComponent.cpp`. **Blocks step 2** — the
   fire ability cannot be granted until a `TSoftClassPtr<UBRAbilitySet>` column exists.
2. **`FireCueTag`'s three tags do not exist.** `GameplayCue.Weapon.{AR,Magnum,Rocket}.Fire` are
   named by the CSV, but `BRGameplayTags.h` declares no `GameplayCue.*` leaves (deliberately —
   §3.1 enumerates none). Kickoff's "re-imports clean" is therefore NOT satisfiable today: the
   column imports empty with warnings. Needs §3.1 amended, then the tags declared.
3. **R4's 90 s has no table home.** `ABRPowerWeaponSpawner::RespawnIntervalSeconds` defaults to
   an INVALID -1 and the node refuses to arm rather than hard-code 90. Wants a column on
   `FBRMatchRulesRow` (BP02) or `DT_MatchRules`.
4. **Pickup interaction radius has no table home** (`InteractionRadiusCm`, EditDefaultsOnly
   placeholder). Lower severity than #3: it carries no design ruling.
5. **`ABRCharacter` must set `bReplicateUsingRegisteredSubObjectList = true`** or the weapon
   subobjects replicate via the legacy path. Both paths are implemented here so neither
   silently replicates nothing, but the pawn's flag decides which runs. Character/ is BP04's.
6. Ticket text says `DT_Weapons.csv` carries "AR + Magnum rows; Rocket row lands in its own
   later ticket" — the landed CSV already has all three. Not a defect; the ticket text is stale.

---

**1 Aug 2026 — CONTRACT_GAP filed FROM OUTSIDE, by BP15 step 4. Read this before restarting
step 2.** (Cross-filed: BP15's architect selected `BRGA_WeaponFire` as the highest-value next
unit, dispatched a builder packet, and it stopped here. Full context in BP15's Log.)

HANDOFF says *"restart BP03 step 2 — the fire path"* and notes the packet *"also has to add an
`AbilitySet` column and declare the three `GameplayCue.Weapon.*.Fire` tags."* **Verified against
disk today, and it is worse than that line implies — there are three gaps, not two, and none is
inside this packet's `owner_path`:**

1. **Tags do not exist.** `BRGameplayTags.h` declares 31 tags. `InputTag.Fire`, `Damage.Kinetic`
   and `Damage.Headshot` are there; **`Ability.Weapon.Fire` and `GameplayCue.Weapon.{AR,Magnum,
   Rocket}.Fire` are not.** The header's own comment pre-refuses this packet: *"Whoever needs
   `Ability.Weapon.Fire` or `GameplayCue.Weapon.Fire` must first get §3.1 amended."*
   → **BP01 / `Core/`.**
2. **`FBRWeaponRow` carries no trace range and no spread.** Not in the row, not in `CT_Combat`
   (11 rows, none of them). A hitscan ability cannot trace without them, and inventing either as
   a literal is a law-3 violation and a `gas-purity` §9 self-check hit.
   → **BP02 / `Data/`** for the fields, curator for the values.
3. **No `AbilitySet` column in `DT_Weapons.csv`**, so equip grants nothing — the ability would
   never reach a player even if it existed.

**The sharpest one:** `DT_Weapons.csv`'s `FireCueTag` column **already contains all three cue
tags**, landed 29 Jul with verifier PASS. The verifier checked the CSV's schema, not whether the
symbols it names exist on the other side. **Data has been referencing undeclared tags for three
days and every gate passed.** Worth a validator, not just a fix.

*Suggested order (all small):* BP01 declares the four tags → curator proposes `Range_m`,
`Spread_deg`, `AbilitySet` → BP02 adds the row fields → **then** step 2 restarts with its inputs
present. Restarting before that means the fire path's first act is editing three other owners'
files, which is what law 5 forbids.

**1 Aug 2026 — the four tags are DECLARED. And the gap above named the wrong owner; corrected.**

*The correction first, because it changes who does the work.* The entry above routed the tags to
**BP01 / `Core/`**. **That was wrong.** **R23** rules `Ability.*` and `GameplayCue.*` **OPEN
families**: the packet that authors an ability or cue declares its own tag, under an **exact-file
`owner_path` grant** to `BRGameplayTags.h`/`.cpp` — never a grant to the `Core/` folder. So these
tags were always BP03's to declare, and no BP01 packet or §3.1 amendment was ever required.

*Where the wrong owner came from — this is the useful part.* `BRGameplayTags.h`'s **file-level
comment contradicted its own `Ability.*` block twelve lines below it.** The file comment said
*"Whoever needs `Ability.Weapon.Fire` or `GameplayCue.Weapon.Fire` must first get §3.1 amended
with the enumeration"* — written before R23 and never updated — while the block beneath already
read *"OPEN family (ruling R23)."* BP15 step 4 read the top of the file, believed it, and filed a
`contract_gap` that R23 says does not exist. **Fixed in the file**, so the next reader cannot make
the same call: the stale paragraph now states R23, names the exact-file grant, and records that it
was corrected. Same shape as session 2's four defects and this session's own — two documents
pointing opposite ways, and the one a reader hits *first* won.

*Landed, all inside the claim's `owner_path` (proven by `git status`: only these two files):*

| Tag | Why |
|---|---|
| `Ability.Weapon.Fire` | `BRGA_WeaponFire`'s asset tag. Firing cancels sprint by listing `Ability.Sprint`; this is what a future ability would list to cancel firing |
| `GameplayCue.Weapon.AR.Fire` | named by `DT_Weapons.csv` row `AR` |
| `GameplayCue.Weapon.Magnum.Fire` | named by row `Magnum` |
| `GameplayCue.Weapon.Rocket.Fire` | named by row `Rocket` |

The cue leaf is **per-weapon, not per-ability** — one fire ability plays a different cue for each
weapon, chosen by the row, which is why the tag travels in data rather than in the ability's C++.

*Verification (this machine, stated at its real rung):*

| Check | Result |
|---|---|
| EXTERN/DEFINE balance | **34 declared, 34 defined**, zero declared-but-undefined, zero defined-but-undeclared |
| `DT_Weapons.csv` `FireCueTag` → native registry | all three **RESOLVE**; unresolved set empty |
| Law-5 confinement | only `BRGameplayTags.h`/`.cpp` + the claim file |
| **Rung 1** | **NOT RUN — BLOCKED.** A UE editor is live (MCP session); R29.3. **These four declarations have never been compiled.** Declaration-only, no logic, but that is an argument about risk, not evidence |

**PROPOSAL, filed not built (outside this claim's `owner_path`).** `DT_Weapons.csv` named three
tags that no C++ declared, and it passed **every gate for three days** — the data crew's verifier
checked the CSV's *schema*, not whether the symbols it names resolve. Declaring the tags closes
today's instance; it does not close the class. The durable fix is a validator that resolves
**tag-valued CSV columns against the native tag registry** and fails the reimport when one
dangles. It belongs in `Tools/` (builder's owner_path per §9), which this claim does not grant —
so it is filed here rather than written. Cheap, and it would have caught this on 29 Jul.

*Still blocking `BRGA_WeaponFire`* — two of the three gaps are untouched: `FBRWeaponRow` still has
**no trace range and no spread**, and `DT_Weapons.csv` still has **no `AbilitySet` column**. Both
are `Data/` + curator, not this packet.
