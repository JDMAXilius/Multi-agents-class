# TICKET — Core vocabulary and the data layer: tags, logs, teams, row structs, BRGameData

> STATUS: open — cut 7 Aug 2026. Blocked on BP90 DONE. Runs in parallel with BP92.

Founder directive: nothing in this layer activates. It is the vocabulary every later packet
speaks — tags, log channels, collision aliases, row structs — plus the ONE subsystem that is
allowed to hold a table pointer or resolve a soft reference. Get this wrong and every packet
above it inherits the mistake. Numbers live in CSV (law 3); C++ holds tags and row handles.

**Ordering law:** `BRGameplayTags` lands before `BRDataRows.h` (rows reference tags), which
lands before `BRGameData` (the subsystem types its lookups on the rows).

## Kickoff (machine-checkable)

- requires: engine-installed
- BP90 is DONE (all boxes ticked) — no template sources under `Source/Breachpoint/`
- `docs/DESIGN-RULINGS.md` carries dated rulings for D-1, D-2, D-3
- owner_path: `Source/Breachpoint/Core/`, `Source/Breachpoint/Data/`, `Content/Data/`

## Steps (in order)

1. **[builder]** Rewrite `Core/BRGameplayTags.h/.cpp` — every native tag, `UE_DEFINE_GAMEPLAY_TAG`,
   four families and nothing else:
   - `InputTag.*` — Move, Look, Jump, Crouch, Sprint, Weapon.Fire, Weapon.Reload,
     Weapon.Swap, Grenade, Melee, Grapple
   - `Ability.*` — one per ability class, for cancel/block queries
   - `State.*` — `Dead`, `Movement.Sprinting`, `Shields.Broken`, `Combat.RecentDamage`
   - payload — `Damage.*` (Kinetic, Explosive, Melee, Melee.Rear, Headshot),
     `SetByCaller.*` (BaseDamage, RegenRate, CooldownDuration, Cost),
     `Event.*` (Death, Kill, Melee.WindowBegin, Melee.WindowEnd, Weapon.ReloadCommit,
     Weapon.SwapCommit) — extension rule stays `Event.<Verb>.<Moment>` (R17)
   A tag not declared here does not exist. No `RequestGameplayTag` by string anywhere.
2. **[builder]** Rewrite `Core/BRCore.h/.cpp`: log channels (`LogBRCombat`, `LogBRNet`,
   `LogBRAI`), collision channel aliases that MATCH `Config/DefaultEngine.ini` (verify, do not
   assume), and `namespace BRTeams { ETeamAttitude::Type GetAttitude(const AActor*, const AActor*); }`
   wrapping UE's native `IGenericTeamAgentInterface`. **This is the entire team system** — no
   team subsystem, no team objects, no `UZoransTeamObject` equivalent. Attitude for a null or
   team-less actor is `Neutral`, never `Hostile`.
3. **[sim-builder]** Rewrite `Data/BRDataRows.h` — every row struct, one header, **no `.cpp`**:
   - `FBRWeaponRow` : numbers (DamagePerShot, RPM, MagSize, ReserveMax, SpreadDegrees,
     RangeMax) · shaping (`FRuntimeFloatCurve DamageFalloff`, `TMap<FName,float> BodySectionMods`)
     · `EBRFireMode` (Hitscan / Projectile) · **soft** refs only
     (`TSoftObjectPtr<USkeletalMesh>`, `TSoftClassPtr<UBRAbilitySet>`)
   - `FBRMatchRules`, `FBRLoadoutRow`
   A hard asset `UPROPERTY` or a `ConstructorHelpers` call in this header is a finding.
4. **[sim-builder]** New `Data/BRGameData.h/.cpp` — `UGameInstanceSubsystem`. Owns every
   `UDataTable`/`UCurveTable` (paths from `Config/DefaultGame.ini`, not hard-coded), exposes
   `const FBRWeaponRow* GetWeaponRow(FName) const` and `float EvalCombatCurve(FName, float) const`,
   and is the **only** call site of `FStreamableManager::RequestAsyncLoad` in the module.
   Missing row = null return + one `LogBRCombat` warning, never a crash and never a default
   that silently plays.
5. **[curators propose → builder lands]** `Content/Data/DT_Weapons.csv` and `CT_Combat.csv`
   with schemas matching step 3. Two weapon rows minimum (one hitscan AR, one projectile) so
   BP98 and BP101 both have data waiting. Re-import via `Tools/reimport-tables.ps1`.
6. **[verifier]** Rung 1 (three targets, Server PARTIAL-by-environment). Rung 2: a new
   `Breachpoint.Sim.Data` spec asserting (a) every `FBRWeaponRow` in the CSV re-validates,
   (b) `GetWeaponRow` on a missing name returns null and logs, (c) no row carries a hard
   asset reference.

## Done when

- [ ] `BRGameplayTags` declares all four families; `grep -rn "RequestGameplayTag" Source/` is empty
- [ ] `BRCore` collision aliases match `DefaultEngine.ini` verbatim (paste the diff in the Log)
- [ ] `BRTeams::GetAttitude` returns Neutral for null/team-less; asserted in a spec
- [ ] `BRDataRows.h` has no `.cpp`, no hard asset ref, no `ConstructorHelpers`
- [ ] `BRGameData` is the only `RequestAsyncLoad` call site — `grep` proves it
- [ ] `DT_Weapons.csv` has ≥2 rows (one per `EBRFireMode`) and re-imports clean
- [ ] Rung 1 as above; rung 2 `Breachpoint.Sim.Data` GREEN with the three assertions pinned
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: builder owns `Core/`; sim-builder owns `Data/`; curators propose CSV values.
- Binary files this ticket OWNS: `Content/Data/DT_Weapons.uasset`, `CT_Combat.uasset`
  (lock before editing — table reimports are script-generated, never hand-edited).
- Out of scope: any ability, attribute, or actor. This packet declares vocabulary and data
  and activates nothing. Do not add tags "we will need later" — a tag with no consumer is
  dead weight and BP93+ will add what they use.

## Log

(append findings here, dated, newest last)
