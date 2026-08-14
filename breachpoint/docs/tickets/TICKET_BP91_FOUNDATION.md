# TICKET — Core vocabulary and the data layer: tags, logs, teams, row structs, BRGameData

> STATUS: in-progress — mac terminal 14 Aug 2026 (BP90 closed same session)
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

### 14 Aug 2026 — builder, steps 1–2 landed (Core/ only)

Full rewrite of `BRGameplayTags.h/.cpp` and `BRCore.h/.cpp`. Editor-target compile
`Result: Succeeded` (clean-compile evidence only; full rung 1 is step 6).

**Tags:** the four ticket families exactly, plus a COMPAT block kept only because
pre-rework code outside Core/ still references the old strings (delete-with-last-consumer):
`Ability.Weapon.{Fire,Reload,Swap}`, `InputTag.{Fire,Reload,Swap}`, `Damage.Rear`,
`State.Cooldown`, `State.Movement.Grappling`, `State.Weapon.{Reloading,Swapping}`,
`State.Combat.{Meleeing,ThrowingGrenade}`, `GameplayCue.Weapon.{AR,Magnum,Rocket}.Fire`.
NOTE for BP94: `Damage.Melee.Rear` (new) and `Damage.Rear` (old) are BOTH registered;
`BRCombatSpec` pins the old semantics — the exec-calc rewrite must migrate the spec.

**BRCore:** added `LogBRCombat/LogBRNet/LogBRAI`; kept `LogBRAbility` (14 pre-rework log
sites, removing = build break until BP93/BP95) and `BRUnits::MetresToUU` (8 call sites).
Collision aliases already matched the ini — `BRCollision::{Projectile, WeaponTrace,
MeleeTrace, GrappleTrace}` = GameTraceChannel1–4, mirrored verbatim (diff: none needed).

**Findings — `RequestGameplayTag` outside Core/ (not fixed, outside owner path):**
`Input/BRInputConfig.cpp:116` (BP92 kills it) · `AbilitySystem/Abilities/BRGA_Grenade.cpp:34`
(runtime-string tag; BP101) · `Tests/BRCombatSpec.cpp:543,549` (BP94). The "grep is empty"
done-box cannot tick until those packets land — recorded as the box's blocker, not routed
around.

### 14 Aug 2026 — sim-builder, steps 3–4 landed (Data/ only, tree left uncommitted for lead)

`BRDataRows.h` rewritten (header-only; no `.cpp` ever existed) and `BRGameData.h/.cpp`
created. `./Tools/run-ubt.sh BreachpointEditor` → `Result: Succeeded` (editor target only —
PARTIAL by the script's own ruling; full rung 1 is step 6). No hard asset ref, no
`ConstructorHelpers` — self-grepped.

**FBRWeaponRow, canonical:** DamagePerShot · RPM · MagSize · ReserveMax (rounds) ·
SpreadDegrees · RangeMax (m) · `FRuntimeFloatCurve DamageFalloff` (EMPTY curve = constant
1.0, BP94 must pin that) · `TMap<FName,float> BodySectionMods` (absent section = 1.0) ·
`EBRFireMode {Hitscan, Projectile}` · `TSoftObjectPtr<USkeletalMesh> HeldMeshSoftPath` ·
`TSoftObjectPtr<UBRAbilitySet> AbilitySet`.

**DECISION — ability-set soft-ref typing:** ticket says `TSoftClassPtr<UBRAbilitySet>`; kept
`TSoftObjectPtr`. `UBRAbilitySet` EXISTS (`AbilitySystem/BRAbilitySet.h`, a
`UPrimaryDataAsset` — rework §3.4 keeps it a DataAsset) and the live consumer
(`BREquipmentComponent::ResolveAbilitySetForRow`, line 423) `LoadSynchronous()`s an
INSTANCE. A class ptr to a DataAsset type would mean BP subclasses, which R18 bans. BP93
owns the class and may overrule; the column moves with it.

**DECISION — enum rename with alias:** `EBRDamageDelivery` → `EBRFireMode` (real UENUM, so
future UPROPERTYs can use the rework name); `using EBRDamageDelivery = EBRFireMode;` keeps
the two consumers (`BRGA_WeaponFire.cpp:66`, `BRCombatSpec.cpp:310`) compiling. Field keeps
the compat NAME `DamageDelivery` — ONE field, no divergent twin. Old
`EBRWeaponFireMode {Automatic, SemiAuto}` + `FireMode` field DELETED (zero consumers).
Step 5's reimport re-types the stale `DT_Weapons.uasset` enum column; until then the old
CSV's `FireMode` column imports as an unknown-column warning in `BRCombatSpec` (AddWarning
path, not an error).

**COMPAT fields kept (delete-with-last-consumer, consumers named):** DisplayName
(`UI/BRHUDDirector.cpp:369`) · ReserveMags + `GetStartingReserveAmmo` (`BRCombatSpec` R4
pins) · ReloadTime_s + EquipTime_s (`BRGA_WeaponUtility.cpp:33`, spec R3 swap-TTK pins) ·
HeadshotMult (spec TTK pins; BP94 supersedes with BodySectionMods) · ProjectileSpeed +
SplashRadius_m + SplashDamage (spec + `ValidateSchema`; BP101) · Range_m + Spread_deg
(`BRGA_WeaponFire.cpp:71,233,247,346` validation; superseded by RangeMax/SpreadDegrees) ·
MeshSoftPath (`BRWeaponPickup.cpp`) · First/ThirdPersonAnimBP (`BREquipmentComponent.cpp`)
· FireCueTag (fire cue routing). Row helpers + `ValidateSchema` kept VERBATIM — `BRCombatSpec`
pins them; no pin moved.

**Structs kept out-of-rework-scope, verbatim:** `FBRSpotterLineRow`, `FBRMedalRow`,
`FBRBot*Row` — their systems (UI, AI) are out of rework scope and their `DT_*.uasset`
tables live in `Content/Data/`. **`FBRMatchRulesRow` KEEPS its pre-rework name** (ticket
says `FBRMatchRules`): `DT_MatchRules.uasset` is serialized against the old name and there
are zero code consumers to gain from a rename — rename when BP96 touches the table. New
`FBRLoadoutRow`: PrimaryWeaponRow/SecondaryWeaponRow (FName keys into DT_Weapons) +
GrenadeCount — no asset refs at all.

**BRGameData:** `UCLASS(Config=Game)` GameInstance subsystem; `UPROPERTY(Config)
FSoftObjectPath` table paths; loads via ONE `RequestAsyncLoad` at Initialize; missing
table/row/curve = null/0 + one `LogBRCombat` warning, plus a row-struct check
(`GetRowStruct() != FBRWeaponRow::StaticStruct()` → warned null, not a silent misread).
No Tick, no timers. **Lead must add to `Config/DefaultGame.ini`** (outside my owner path;
no section exists yet):

```
[/Script/Breachpoint.BRGameData]
WeaponTablePath=/Game/Data/DT_Weapons.DT_Weapons
CombatCurveTablePath=/Game/Data/CT_Combat.CT_Combat
```

**Findings — `RequestAsyncLoad` outside Data/ (not fixed, outside owner path):** the
"only call site" done-box is blocked by 7 live sites + 1 comment:
`UI/BRUIManagerSubsystem.cpp:72` · `UI/Components/BRFeatureCard.cpp:111` ·
`UI/HUD/BRReticleWidget.cpp:402` · `Input/BRInputConfig.cpp:108` (BP92) ·
`AI/BRBotController.cpp:91` (AI seam, out of rework scope — needs a ruling) ·
`AbilitySystem/Cues/BRGameplayCues.cpp:322` (BP94) · `Equipment/BREquipmentComponent.cpp:706`
and `Equipment/BRWeaponPickup.cpp:319` (BP97, which should route through BRGameData per
rework §3.6). UI sites are out of rework scope entirely — the done-box needs either a
scope ruling ("module" = the rework's folders) or those packets. Recorded as the box's
blocker, not routed around.
