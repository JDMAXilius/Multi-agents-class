# TICKET — Equipment: slots, the weapon instance, and pickups

> **ARCHIVED 25 Aug 2026** — moved off the live board, contents untouched below.
> SUPERSEDED by BreachpointNext R1-R10. This ticket describes in `Source/Breachpoint/` what `Source/BreachpointNext/` has since built and shipped.
> Reversible: `git mv` kept the history, `git log --follow` still reaches it.
>
> STATUS: open — cut 7 Aug 2026. Blocked on BP96 DONE. Gates BP98.
> **Folder name depends on D-2** (`Weapons/` vs `Equipment/`) — settled in BP90.

Founder directive: a weapon is not an actor. Three replicated actors per player buys nothing
and costs relevancy, spawn cost and lifetime bugs — it is a replicated `UObject` holding a row
handle and its ammo. Equipping is **granting an AbilitySet**, so a weapon's verbs arrive and
leave with the weapon and nothing polls for "which gun am I holding".

**Ordering law:** `BRWeaponInstance` before `BREquipmentComponent` (the component holds them);
both before `BRWeaponPickup`.

## Kickoff (machine-checkable)

- requires: engine-installed
- BP96 DONE — `ABRCharacter` exists, `OnCharacterReady` fires once
- BP93 DONE — `UBRAbilitySet::GiveToAbilitySystem` returns exact revoke handles
- BP91 DONE — `DT_Weapons.csv` has ≥2 rows and `BRGameData::GetWeaponRow` resolves them
- owner_path: `Source/Breachpoint/Equipment/` (or `Weapons/` per D-2), `Content/Data/`

## Steps (in order)

1. **[sim-builder]** `BRWeaponInstance.h/.cpp` — a **replicated `UObject`**, not an actor:
   - `FDataTableRowHandle` (or `FName` row id) + `Ammo` / `ReserveAmmo`, both **`COND_OwnerOnly`**
   - `GetWeaponRow()` resolves through `BRGameData` — the instance never holds a table pointer
   - **Ammo is the named gas-purity exception.** Mutated ONLY inside `BRGA_WeaponFire` /
     `BRGA_WeaponUtility`; `COND_OwnerOnly` replication IS the correction path for a
     mispredicted decrement (Lyra parity). Put that sentence in the header as a comment so
     the next reader does not "fix" it into an attribute.
   - Implements `IsSupportedForNetworking()`; registered in the owner's replicated subobject list
2. **[sim-builder]** `BREquipmentComponent.h/.cpp` — lives on the **character**, not the
   PlayerState (equipment is per-life; the ASC is not):
   - Two weapon slots + one grenade slot; `CurrentSlot` replicated with `OnRep`
   - `Equip(Slot)` = grant the row's `BRAbilitySet` (storing the returned handles) + async-load
     the soft mesh through `BRGameData`; `Unequip` = `TakeFromAbilitySystem(handles)`. **Never
     revoke by iterating** — exact handles or it is a finding.
   - `ServerRequestSwap(Slot)` / `ServerRequestPickup(ABRWeaponPickup*)` with **real
     `_Validate`** (slot in range; pickup within interaction radius of the server-known pawn
     location; pickup not on cooldown)
   - Calls `ABRCharacter::CheckReady()` when its initial grant completes
   - On death, GameMode calls `DropAll()` — the component does not decide when
3. **[sim-builder]** `BRWeaponPickup.h/.cpp` — `ABRWeaponPickup` + `ABRPowerWeaponSpawner`:
   - The spawner replicates a **server timestamp** (`NextSpawnServerTime`) and `OnRep`s it;
     clients derive the countdown from `GetServerWorldTimeSeconds()`. **No replicated ticker,
     no per-second RPC.**
   - Pickup interaction is server-validated; the client never grants itself anything
4. **[verifier]** Rung 1 (three targets, Server PARTIAL-by-environment). Rung 2: a spec
   asserting grant→revoke round-trips exactly (ability count returns to baseline; no orphaned
   `FActiveGameplayEffectHandle`). **Rung 4a**: client A picks up a weapon; assert in threes
   that the ability set is granted on the server, usable on A, and A's held mesh is visible to
   B. Then swap slots and assert the previous set is revoked everywhere.
5. **[critic REFUTER]** Attack surface: can a client swap to an empty slot? Pick up from
   across the map? Does a swap mid-reload leave a dangling ability? Does dying with an
   unresolved async mesh load crash on the callback? Can two clients pick up the same weapon
   in the same frame?

## Done when

- [ ] `BRWeaponInstance` is a `UObject` — `grep` finds no `AActor` weapon class
- [ ] Ammo replicates `COND_OwnerOnly`; the exception comment is in the header
- [ ] Equip grants an AbilitySet and unequip revokes **the stored handles** — asserted by a
      grant→revoke→count spec, not by inspection
- [ ] Every `Server` RPC has a real `_Validate` that rejects out-of-range and out-of-distance
- [ ] Power weapon countdown derives from a replicated timestamp — no ticker, no repeating RPC
- [ ] Two clients cannot both acquire one pickup (asserted, not assumed)
- [ ] Rung 1 as above; rung 2 green; **rung 4a green, asserted in threes**
- [ ] Critic REFUTER pass recorded with findings verbatim
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: sim-builder builds; netcode-builder co-signs every replicated symbol and both RPCs;
  critic REFUTERs.
- Binary files this ticket OWNS: `Content/Data/DT_Weapons.uasset` (shared with BP91 — lock
  before editing).
- Out of scope: firing (BP98), the projectile (BP101), death/respawn policy (BP99). This
  packet makes a weapon *held*, not *used*.

## Log

(append findings here, dated, newest last)
