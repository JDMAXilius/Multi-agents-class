# Contract — Data & Assets (one source of truth per kind; text over binary)

Status: v1 (filled for BREACHPOINT) · Rule of thumb: **a number tied to a gameplay noun is data (owned by a
table); a rule that applies across nouns is logic (owned by C++).** If you are typing a damage
value into a Blueprint graph or a C++ literal, stop — it belongs in a DataTable row.

## Who owns what

| Data kind | The ONE owner | Never lives in |
|---|---|---|
| Tuning numbers (damage, costs, cooldowns, drop rates, XP curves) | **DataTables/CurveTables backed by CSV/JSON in git** (`Content/Data/` + `Source/.../Data/` structs) | BP graphs, C++ literals, per-instance overrides |
| Gameplay rules (how numbers combine) | C++ sim modules (sim-builder) | Blueprints, widget graphs |
| Replicated surface (what/when/to whom) | netcode-builder's modules per `netcode.md` | ad-hoc per-feature RPCs |
| Config | `Config/Default*.ini` — each file has a named owner | scattered `GConfig` writes |
| Save data schema | one versioned module with explicit migration on change | implicit SaveGame field drift |
| Visuals/layout/animation | Blueprint/asset land — designers iterate freely here | (this is the one place binaries are the point) |

Derived artifacts (cooked builds, generated headers, compiled shaders) are never hand-edited —
fix the source and regenerate.

## The Blueprint-vs-C++ line (R18 — stricter than "thin")

**There are no Blueprint classes.** Not thin ones, not "just glue." Actors, components,
abilities, effects, controllers, game modes, cues — all C++, spawned by class reference
from data. An engine asset exists ONLY where UE 5.8 offers no C++ authoring path, and the
complete list of those is Tier 4 in **`BREACHPOINT-AUTHORING-MATRIX.md`**:
AnimBlueprint graphs · materials/instances · Niagara · MetaSounds · UMG layout (WBP) ·
`ST_Bot` · EQS query assets · sourced art. **Nothing else gets an asset.**

Not because BP is bad — because **binary assets are invisible to review**: no diff, no
merge, no grep-audit, and the crew's critic cannot read them. Adding a Tier-4 asset means
adding a part of the game only a human staring at the editor can review.

What survives in those assets is a wiring diagram or a picture, never a decision:
- an AnimGraph carrying a gameplay number or branch → violation (numbers are `CT_Combat`;
  notifies announce a *moment* per R17, the sim decides the consequence)
- a WBP carrying anything but layout/anchors/animation → violation (state and binding live
  in the C++ parent + ViewModel)
- a GameplayEffect authored as an asset → violation (the six generic GEs are C++ classes;
  SetByCaller + dynamic tags cover the game)
- a GameplayCue authored as anything but a C++ handler class → violation (the asset is only
  the VFX/SFX the C++ cue plays)

**The standing question for any new asset:** *which tier, and if Tier 4, why can't C++
express it?* No crisp answer ⇒ no asset.

## Binary-asset discipline (the merge-conflict law)

1. **One owner per binary file per ticket.** The ticket names which `.uasset`/`.umap` files it
   may touch; `git lfs lock` (or P4 checkout) before editing; unlock on land.
2. Two writers on one binary is an unresolvable conflict — prevention is the only cure. If two
   tickets need the same map, split the map (level instances / data layers) or serialize the
   tickets.
3. Binary changes are reviewed by BEHAVIOR (ladder rungs + PIE walkthrough/screenshots named
   in the ticket), and the review says so explicitly.

## Repo hygiene fill-ins — BREACHPOINT (refilled 2026-07-29; supersedes the Slash Roller fill)

- [x] **Git + Git LFS with locks enabled is the binary authority** — `git lfs lock` IS the
  lock required by law 7, and `*.uasset *.umap` are LFS-tracked and lockable. This is the
  mechanism that actually exists: the planning repo is git, and the game repo will be too.
  *(If a Perforce server is ever stood up, P4 exclusive-checkout via the typemap replaces LFS
  locks as the mechanism — the one-owner law is unchanged either way. Naming P4 as primary
  before a server exists made law 7 unenforceable at BP07, the first `.umap`, which is
  exactly when it first bites.)* **Decision deadline: before BP07 is claimed.**
- [x] `Saved/`, `Intermediate/`, `DerivedDataCache/`, `Binaries/` ignored (P4 ignore +
  `.gitignore` kept in sync).
- [x] Naming convention: **`BR` class prefix** (`ABRCharacter`, `UBRAttributeSet`,
  `IBRServerLifecycle` — never "BP", which collides with Blueprint vocabulary), **one runtime
  module** `Source/Breachpoint/<Domain>` (Core, Input, AbilitySystem, Character, Weapons,
  Match, AI, Online, UI, Telemetry, Data, Tests — the folders ARE the crew owner_paths).
- [x] **Soft references only at the data boundary:** every asset reference in row structs and
  data assets is `TSoftObjectPtr`/`TSoftClassPtr`, resolved via the streamable manager at
  load points. A hard `UPROPERTY` asset ref (or `ConstructorHelpers`) in C++ is a finding.
- [x] **Generic-effect law:** GameplayEffects are parameterized templates (SetByCaller +
  dynamic tags) — `GE_Damage`, `GE_Regen`, `GE_Cooldown`, `GE_InitStats`, `GE_RecentDamage`
  are the library; new content adds rows/parameters, not effect assets.
- [x] Table source CSVs live at **`Content/Data/*.csv`**; reimport is scripted via commandlet
  (`Tools/reimport-tables.ps1`), never manual. **Two kinds, and the `DT_`/`CT_` prefix is the
  tell — they do not import the same way:**
  - **DataTables (`DT_`)** — `DT_Weapons`, `DT_BotTuning`, `DT_BotAmbitions`, `DT_MatchRules`,
    `DT_SpotterLines`. Each needs a `USTRUCT` row type, and ALL of them live in the one header
    `Source/Breachpoint/Data/BRDataRows.h`.
  - **CurveTables (`CT_`)** — `CT_Combat`, the damage/movement coefficient table
    (`BRDamageExecCalc` multipliers, sprint speed, radial falloff). A CurveTable imports as
    **named curves and has no row struct** — do not author an `FBRCombatRow` for it, and do
    not add it to `BRDataRows.h`. It is addressed by curve name + input value.

  Getting this backwards fails at first import, not at review — the importer rejects the
  asset type mismatch.
- [x] **`FBRWeaponRow` carries two distinct enums** (schema split forced by the verifier in
  the 29 Jul 2026 data-crew run): `FireMode` = trigger cadence ({Automatic, SemiAuto});
  `DamageDelivery` = how the shot reaches the target ({Hitscan, Projectile}). Invariant,
  asserted at import and in `Breachpoint.Sim.*`: `ProjectileSpeed == 0` **iff**
  `DamageDelivery == Hitscan`. Conflating them again is a finding.
- [x] **`arena_manifest.json` is a data artifact under this contract**: schema per the
  arena-architect's doctrine (`bounds`, `spawn_points[]` with scoring_hints, named
  `landmarks[]`, `cover[]`, `sightlines`, `hazards[]`, `doubts[]`); it is the source of
  truth the blockout `.umap` projects (one owner per binary, law 7), and the bots' spatial
  vocabulary (EQS scores its landmarks — `BREACHPOINT-AI-BOTS.md` §4). Hard floors the
  validators enforce: ≥ 8 spawns, ≥ 8 m min pairwise spacing, ≤ 35 m sightlines, a named
  rocket landmark.
