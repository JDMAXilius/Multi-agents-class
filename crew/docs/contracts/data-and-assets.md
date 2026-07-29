# Contract — Data & Assets (one source of truth per kind; text over binary)

Status: v1 template · Rule of thumb: **a number tied to a gameplay noun is data (owned by a
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

## The Blueprint-vs-C++ line

Blueprints are THIN by law: glue, cosmetic reaction, layout, designer iteration. Not because
BP is bad — because **binary assets are invisible to review**: no diff, no merge, no
grep-audit, and the crew's critic cannot read them. Everything that needs an audit trail
(logic, numbers, replication) lives in text. A gameplay branch discovered inside a widget or
actor graph is a finding with a named home to move it to.

## Binary-asset discipline (the merge-conflict law)

1. **One owner per binary file per ticket.** The ticket names which `.uasset`/`.umap` files it
   may touch; `git lfs lock` (or P4 checkout) before editing; unlock on land.
2. Two writers on one binary is an unresolvable conflict — prevention is the only cure. If two
   tickets need the same map, split the map (level instances / data layers) or serialize the
   tickets.
3. Binary changes are reviewed by BEHAVIOR (ladder rungs + PIE walkthrough/screenshots named
   in the ticket), and the review says so explicitly.

## Repo hygiene fill-ins — BREACHPOINT (refilled 2026-07-29; supersedes the Slash Roller fill)

- [x] **Perforce** is the binary authority: P4 checkout IS the lock; the typemap marks
  `*.uasset *.umap` exclusive-checkout. (Any git mirror used for cloud/agent sessions tracks
  binaries via Git LFS **with locks enabled** — same one-owner law, different mechanism.)
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
- [x] DataTable source CSVs live at: **`Content/Data/*.csv`** with ALL row structs in the one
  header **`Source/Breachpoint/Data/BRDataRows.h`** (`DT_Weapons`, `CT_Combat`,
  `DT_BotTuning`, `DT_MatchRules`, `DT_SpotterLines`) · reimport is scripted via commandlet
  (`Tools/reimport-tables.ps1`), never manual.
