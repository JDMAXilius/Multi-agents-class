# BREACHPOINT — The Authoring Matrix

**Policy in one line: zero Blueprint *classes*, and an engine asset exists only where UE
5.8 offers no C++ path at all — every one of those is named here, with its owner.**
Binding ruling: **R18** in `crew/docs/DESIGN-RULINGS.md`.

This is the practical answer to "who authors what": Claude terminal (C++/text), a generated
Python script (assets built headlessly), or you in the editor (graph/WYSIWYG work the
engine reserves). It exists because "as much C++ as possible" is a direction, not a plan —
without a named list, every asset becomes an argument.

---

## 1. The rule that decides every case

> **If UE can express it in C++, it is C++. If it cannot, the asset is authored — and it
> holds NO logic, NO numbers, and NO decisions. It is a wiring diagram or a picture.**

The reason is the crew, not taste: **binary assets are invisible to review.** No diff, no
merge, no grep, and the critic literally cannot read them. Everything that needs an audit
trail lives in text. So the cost of an asset is not "it's not C++" — it is *"this part of
the game cannot be reviewed by anyone but a human staring at the editor."*

Two consequences worth stating plainly:
- **No Blueprint classes.** Not thin ones, not "just glue." Actors, components, abilities,
  effects, controllers, game modes — all C++, spawned by class reference from data.
- Assets that survive are **graph or layout only**, and the C++ they read from is the
  source of truth.

---

## 2. The matrix

### Tier 1 — Pure C++ (no asset, agent-authorable, fully reviewable)

Everything here is text the crew writes and the critic reads. This is the overwhelming
majority of the game.

| Thing | Note |
|---|---|
| Gameplay abilities, attribute set, damage exec calc | GAS core; `gas-purity` skill |
| **GameplayEffects** — the six generic GEs | `UGameplayEffect` subclasses, fully constructor-authored. Do NOT author GEs as assets: SetByCaller + dynamic tags means six C++ classes cover the game |
| **GameplayCue handlers** | `UGameplayCueNotify_Static` / `AGameplayCueNotify_Actor` C++ subclasses registered by tag. The cue *class* is C++; only the VFX/SFX it plays is an asset |
| CMC subclass, grapple root-motion source | netcode-owned movement |
| Replication, RPCs, `_Validate` bodies | |
| **StateTree tasks, conditions, evaluators** | C++ structs — the logic of the bot brain |
| **EQS generators, tests, contexts** | C++ classes — the scoring math |
| `UBRBotBrain`, `BRBotFacts` | pure, headless, seed-pinned |
| **AnimInstance C++ base + thread-safe update + custom `FAnimNode_*`** | ALL animation *state and math* lives here |
| **Widget C++ classes + ViewModels** | ALL UI state, binding, and flow |
| Subsystems (sessions, telemetry, spotter), lifecycle interface | |
| Data row structs (`BRDataRows.h`), automation specs | |

### Tier 2 — Text data (agent-authorable, no editor, diffable)

| Thing | Owner |
|---|---|
| `DT_Weapons`, `CT_Combat`, `DT_BotTuning`, `DT_BotAmbitions`, `DT_MatchRules`, `DT_SpotterLines` CSVs | tuning-curator / spotter → builder |
| `arena_manifest.json` | arena-architect → builder |
| `Config/Default*.ini` | builder, one named owner per file |

### Tier 3 — Script-generated assets (headless Python; Claude terminal drives, no hand-placing)

These *are* binary assets, but no human clicks them into existence — a committed,
idempotent script does, so the generation is reviewable even though the output isn't.
See the `ue-editor` skill.

| Asset | Script | Why scripted |
|---|---|---|
| `BR_Arena01.umap` blockout | `Tools/py/build_arena.py` | The manifest is truth; the map is its projection. Re-runnable when the manifest changes |
| DataTable reimport | `Tools/reimport-tables.ps1` | CSV → asset, never manual |
| `UInputAction` + `IMC_Default` assets | `Tools/py/build_input_assets.py` | Tiny, mechanical, and reproducible beats remembered |
| Ability-set / input-config data instances | *see §4 — pending decision* | |

### Tier 4 — Editor-authored (you, by hand — the engine reserves these)

**This is the complete list. Anything not here does not get an asset.**

| Asset | Why C++ can't | What it may contain |
|---|---|---|
| **AnimBlueprint graph** (1 FP arms, 1 TP body) | The AnimGraph node network has no C++ authoring path. C++ can define nodes and the AnimInstance; it cannot wire the graph | Node wiring + state-machine transitions **reading C++ properties only**. Zero gameplay decisions — a damage number or cooldown in an AnimGraph is a contract violation |
| **Materials / material instances** | Material graphs are graph-only, full stop | Sourced-art materials, parameterized. C++ drives `UMaterialInstanceDynamic` params at runtime |
| **Niagara systems** | Graph-only | VFX for cues. The *cue class* is C++; Niagara is what it plays |
| **MetaSounds** | Graph-only | Audio for cues, params fed from cue parameters |
| **UMG widget layout (WBP)** | Possible in C++ (runtime widget trees) but **genuinely worse** — see §3 | Layout, anchors, animation ONLY. C++ parent class owns every binding and all state |
| **`ST_Bot` StateTree asset** | The tree structure is an asset; its tasks are C++ | State/transition wiring only |
| **EQS query assets** | Query assets are editor-authored; tests are C++ | Which C++ tests run, with what weights |
| Skeletal meshes, anim sequences, montages, textures | Sourced art | Notify *windows* (which raise `Event.*` per R17) — never notify logic |

---

## 3. Where the editor is genuinely the right tool (the honest part)

Maximal C++ is the direction; these four are where pushing further **costs more than it
returns**, and pretending otherwise would be dogma:

1. **UMG layout.** You *can* build widget trees in C++ (`NewObject` + `AddChild`) — and for
   a HUD with anchors, safe zones, and iteration, it is slower to write, impossible to
   preview, and produces worse results. **Verdict: WBP for layout, C++ parent for
   everything else.** Our existing law already forbids logic in widget graphs; that law is
   what makes the layout asset safe.
2. **AnimGraph.** Unavoidable *and* the editor is the correct tool — blend spaces and state
   transitions are spatial problems. Keep every value in `CT_Combat`/tables.
3. **EQS queries.** The visual debugger (run a query, see the scored points in the level) is
   worth more than text authoring for spatial tuning. The *tests* stay C++.
4. **Material instances on sourced art.** Fighting inherited material graphs is pure cost.
   Parameterize and drive from C++.

Everything outside these four defaults to C++ or a generated script.

---

## 4. Open decision — two `UDataAsset` instances (raise at BP01/BP02)

`BRInputConfig` and `BRAbilitySet` are currently `UDataAsset` *classes* in C++ (good) whose
*instances* are `.uasset` files (binary, unreviewable). Under R18 that deserves a ruling:

- **Option A — keep DataAssets.** Type-safe, editor-pickable, idiomatic UE.
- **Option B — make them DataTables** (`DT_InputMap`, `DT_AbilitySets` with
  `TSoftClassPtr` columns). Text, diffable, agent-authorable, critic-readable — and
  consistent with every other table we own.

**Recommendation: Option B for `BRAbilitySet`** (it is literally rows of
`{ability class, level, InputTag}` — a table pretending to be an asset), and **Option A for
`BRInputConfig`** *only if* the `UInputAction` references make a table awkward in practice.
Decide when BP01 lands the input layer, log it in the ticket, and record the ruling.

---

## 5. Who does what — the three lanes

| Lane | Owns | Examples |
|---|---|---|
| **Claude terminal (crew agents)** | All of Tier 1 + Tier 2, and the *scripts* in Tier 3 | C++ modules, CSVs, manifests, `build_arena.py`, specs |
| **Headless script (run from terminal)** | Tier 3 execution | Blockout generation, table reimport, input assets, screenshots |
| **You, in the editor** | Tier 4 only, plus every judgment call | ABP graphs, materials, Niagara, MetaSounds, WBP layout, StateTree/EQS wiring, **and the Review gate before any crew output enters `Content/`** |

**The standing question for any new asset:** *"Which tier is this, and if it's Tier 4, why
can't C++ express it?"* If that question has no crisp answer, the asset shouldn't exist.
