# TICKET — create `DT_BNWeapons` through the Unreal MCP
> STATUS: in-progress — mac terminal 13 Aug 2026 (8befcd9). Editor live, MCP on :8000.

**Cut:** 13 August 2026 by the cloud lead · **For:** the terminal session (editor + MCP in reach)
**Binds to:** the NEXT doc family only · **Owner path:** `Content/BN/Data/`
**Unblocks:** Roadmap 2 Wave 2 (G2 — the weapon in the hands)

The one editor handoff Roadmap 2 Wave 1 announced. The C++ side is landed (`faf4e9a`); this is
the asset it points at.

## Kickoff — check before starting

1. **The module must be compiled.** `FBNWeaponRow` has to be loadable or the DataTable factory
   cannot pick it. If `Source/BreachpointNext/Data/BNDataRows.h` is newer than the built DLL,
   build first.
2. `Config/DefaultGame.ini` already carries the path — do **not** edit it:
   `[/Script/BreachpointNext.BNAssetSettings] WeaponTable=/Game/BN/Data/DT_BNWeapons.DT_BNWeapons`
3. Until this asset exists, `UBNGameData::FindWeaponRow` returns null on every lookup. That is the
   designed miss, not a bug — so a red here is this ticket, not a regression.

## Step 1 — create the table

A **DataTable** whose Row Structure is **`FBNWeaponRow`** (`/Script/BreachpointNext.BNWeaponRow`),
saved at exactly **`/Game/BN/Data/DT_BNWeapons`**.

`DataTableFactory` requires its `RowStruct` set **before** the asset is created — created without
one, the editor raises its row-struct picker, and an unattended MCP call then blocks the game
thread. That is the same failure shape as `BlueprintTools.create`'s parent-class modal
(`BREACHPOINT-NEXT-CONTENT-LAYOUT.md`), which wedged every later request until the editor was
killed. Set the struct in the factory call, not after.

## Step 2 — seed two rows, with values READ from the template weapons

Two rows is what Wave 2 needs to prove switching. **Row names: `Rifle` and `Pistol`** (they are
keys — `FindWeaponRow(FName)` takes them literally).

**Do not invent the numbers.** The template's weapon Blueprints carry real values, and
`MyCharacter` reads exactly these by reflection at runtime. Read them off the assets through the
MCP and transcribe:

| Row field | Read from the template weapon BP | Notes |
|---|---|---|
| `FireDelay` | `FireDelay` | a **period in seconds**, not RPM |
| `SpreadAngle` | `SpreadAngle` | degrees, cone half-angle |
| `ShotCount` | `ShotCount` | 1 on both of these (6 is the shotgun's) |
| `FireMode` | `GetCurrentFireMode` / its `WeaponType` default | ordinals match `EBNFireMode`: 0 Single · 1 Auto · 2 Burst |
| `MagazineSize` | the weapon's magazine/ammo capacity | |
| `AttachSocketName` | `AttachSocketName` | a socket on the **CHARACTER** mesh |
| `MuzzleSocketName` | `MuzzleSocketName` | a socket on the **WEAPON's own** mesh — not the character's |
| `WeaponMesh` | the weapon BP's skeletal mesh | soft ref |
| `AnimLayerClass` | `LinkAnimLayerClass` | soft **class** ref — needs the `_C` suffix |
| `FireMontage` / `ReloadMontage` | `FireAnimMontage` / `ReloadAnimMontage` | soft refs; leave empty if the asset has none |

Source assets (from `Config/DefaultGame.ini`, the `[/Script/Breachpoint.MyCharacter]` section):
`/Game/FPSTemplate/Blueprints/Weapons/BP_FPST_Weapon_Rifle` and `…_Pistol`.

`Damage`, `HeadshotMultiplier`, `Range`, `ReloadTime` and `BurstShotCount` have no template
source — `MyCharacter` never read them (its damage was a hardcoded 10.0 it deliberately did not
apply). **Leave the C++ defaults** and say so in the Log; they are tuning for a later pass, and a
number invented here would look authored.

**If a property does not resolve, leave the field at its default and record the miss.** A wrong
socket name is silent: the weapon attaches at the mesh root and nobody sees why.

## Step 3 — read back and report

Reload the asset fresh and print an intent-vs-actual table: row struct path, the two row names,
and per row the fields above with their actual values plus where each came from (`read from
BP_FPST_Weapon_Rifle` / `C++ default` / `unresolved`). **The read-back is the proof — that the
call returned is not.**

## Done means

`/Game/BN/Data/DT_BNWeapons` exists on `FBNWeaponRow` with rows `Rifle` and `Pistol`, saved and
committed, the read-back table pasted into the Log below, and every unresolved property named.
Then Wave 2 can spawn a weapon.

## Rules that govern this ticket

- **No scripts.** The founder's revised operating rules (Roadmap 1) removed the scripts-first
  doctrine; drive the MCP directly. If the MCP fights back, stop and hand the remaining steps to
  the founder as clicks rather than sinking time into automating them.
- **Announce, do not assume.** If this needs a C++ change — a field that cannot be authored, a
  type the factory refuses — say so and stop. Do not edit `Source/BreachpointNext/`.
- **The MCP client may fail to attach at startup and does not re-bind mid-session.** The proven
  fallback is JSON-RPC over its HTTP transport (`.mcp.json` → `http://127.0.0.1:8000/mcp`).

## Log

_(terminal session: append the read-back table, anything unresolved, and anything handed back)_
