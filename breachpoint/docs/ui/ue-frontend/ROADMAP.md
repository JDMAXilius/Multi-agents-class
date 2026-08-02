# BREACHPOINT — UE 5.8 front-end roadmap

**Status:** proposal, 2 Aug 2026. Written from a headless terminal session with no editor and no
build. Binding law: `CLAUDE.md` laws 1–8, `docs/DESIGN-RULINGS.md` (R18, R21, R26, R29, R30, R36,
R37). Method: `.claude/skills/ui-presentation` (design system + Figma handoff) and
`.claude/skills/ue5-ui-architecture` (CommonUI/MVVM mechanics). Reference tables:
`docs/UI-DESIGN-SYSTEM.md`, `docs/ui/*`.

**How to read the two kinds of claim in this file.** Every statement is marked:

- **[V]** — verified in the repo this session by reading the file, grepping the tree, or both.
  The evidence is named inline.
- **[P]** — proposed by this roadmap. Not law, not built, not ruled. A `[P]` that touches an
  owner path outside `Source/Breachpoint/UI/` or `Content/UI/` is a **contract_gap to file**,
  not an edit to make (law 5).

Nothing in this file was run. No asset was authored, no target was compiled, no rung was climbed.

---

## 0. Ground truth — what actually exists on 2 Aug 2026

### 0.1 The C++ UI layer is built and is wired to nothing

**[V]** `Source/Breachpoint/UI/` holds seven units, all with real bodies:

| File | What it is |
|---|---|
| `BRUITypes.h/.cpp` | 4 native `FUITag` layers (`Layer.Game`, `Layer.GameMenu`, `Layer.Menu`, `Layer.Modal`), `EBRUIDataState{Unknown,Live,Stale}`, `EBRHitMarkerKind`, `FBRKillfeedViewEntry`, `FBRCombatAttributeBindings` |
| `BRActivatableWidget.h/.cpp` | `UCommonActivatableWidget` base, `GetDesiredInputConfig()` override, `EBRWidgetInputMode`, VM accessors, `DisableNativeTick` |
| `BRRootLayout.h/.cpp` | `UCommonUserWidget` with **four non-optional `meta=(BindWidget)` `UCommonActivatableWidgetStack`** members: `GameLayerStack`, `GameMenuLayerStack`, `MenuLayerStack`, `ModalLayerStack` |
| `BRHUDLayout.h/.cpp` | `UBRKillfeedEntryWidget` + `UBRHUDLayout`; `FUserWidgetPool` killfeed pooling; one `BindWidgetOptional` `KillfeedContainer` |
| `BRUIManagerSubsystem.h/.cpp` | GameInstance subsystem; per-local-player VMs; `CreateLayoutForLocalPlayer`, `PushWidgetToLayer`, `ShowHUD/ShowMainMenu/ShowDeathOverlay/ShowCarnageReport`; publishes VMs to the MVVM global collection |
| `BRUISettings.h` | `config=Game, defaultconfig`; **6 `TSoftClassPtr` screen/layout slots** + killfeed cap/lifetime + two MVVM context names |
| `BRViewModels.h/.cpp` | `UBRVM_Combat` (vitals, ammo, weapon names, grenades, grapple, 8 `FMVVMEventField`s) and `UBRVM_Match` (clock derived from one replicated end-time, scores, phase, rocket countdown, killfeed ring + expiry) |

**[V] Nothing outside `Source/Breachpoint/UI/` references any of it.** `grep -rn` across `Source/`
for `BRUIManagerSubsystem`, `CreateLayoutForLocalPlayer`, `ShowHUD`, `BindToAbilitySystem`,
`SetTimeSource`, `PushKillfeedEntry` returns **only hits inside `UI/` itself**. So:

- No root layout is ever created — nothing calls `CreateLayoutForLocalPlayer`.
- No HUD is ever pushed — nothing calls `ShowHUD`.
- `UBRVM_Combat` never binds to an ASC; `UBRVM_Match` never gets a time source, a score, a phase
  or a killfeed entry. **Both ViewModels are constructed and then starved.**

`UBRUIManagerSubsystem::HandleLocalPlayerAdded` **[V]** creates both ViewModels and publishes
them, but never creates the layout — and `CreateLayoutForLocalPlayer` early-returns `nullptr` when
`LocalPlayer->GetPlayerController(World)` is null, which is the state at local-player-added time.

> **This is the single most important fact in this document.** The UI layer is not "partially
> built" — it is complete as a mechanism and has zero connection to the game. Every phase below is
> downstream of fixing that, and it is a handful of lines, not a project.

### 0.2 Config: one prerequisite done, the rest empty

**[V]** `Config/DefaultEngine.ini:143` sets
`GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient` — the CommonUI
input-routing prerequisite is **done**, with the `LogUIActionRouter` error it prevents quoted in a
comment above it.

**[V]** `Config/DefaultGame.ini` sets three `CommonUI.*` console variables and configures
`BRPlayerState` and `BRPlayerController`. It contains **no `[/Script/Breachpoint.BRUISettings]`
section at all.** Every one of the six `TSoftClassPtr` slots is therefore null, and
`CreateLayoutForLocalPlayer` returns at its `Settings.RootLayoutClass.IsNull()` guard even when a
PlayerController exists.

**[V]** No `CommonInputSettings` / `CommonUIInputData` / input-action data table for Back/Confirm
appears anywhere in `Config/`. Button prompts and stack back-navigation need one.

**[V]** No `[/Script/Engine.UserInterfaceSettings]` DPI section. The design system's "base 1280×720,
×1.5 for 1920×1080" is currently unenforced by anything.

### 0.3 Assets: three WidgetBlueprints exist; their contents are unverifiable from here

**[V]** `Content/UI/` holds `WBP_RootLayout.uasset`, `WBP_HUDLayout.uasset`,
`WBP_KillfeedEntry.uasset`. All three are **git-LFS pointers** in this checkout (130 bytes each;
`WBP_RootLayout`'s pointer declares `size 19848`). Landed by commit `238e4ce` (BP18).

**[V]** `docs/bus/20260801T180038Z--T2-to-T1--bp18-...md` records: *"Confirmed real
UMGEditor.WidgetBlueprint assets, correct BR parents, zero graph nodes."*

**What that message does not say, and what nobody has checked:** whether `WBP_RootLayout` contains
four child widgets **named exactly** `GameLayerStack`, `GameMenuLayerStack`, `MenuLayerStack`,
`ModalLayerStack`, each a `UCommonActivatableWidgetStack`. `UBRRootLayout` declares all four as
non-optional `BindWidget` **[V]**, so a WBP without them **does not compile in the widget editor**.
An empty-but-correctly-parented WBP is exactly what an MCP-created asset would be.

**[P] Treat `WBP_RootLayout` as unverified until an editor session opens it and reports its
compile result.** This is the first thing Phase 0 does and it is cheap.

**[V] R37 gap, live:** the ruling requires *a committed plan **and** a committed receipt* for every
MCP-landed asset. `find . -iname "*receipt*"` returns **nothing** in the repo. BP18's evidence is a
bus message, not a committed receipt. That debt is inherited by every WBP this roadmap adds.

### 0.4 The component ledger is specified and unbuilt

**[V]** `docs/UI-DESIGN-SYSTEM.md` §4 lists 12 components, all `☐`. **[V]**
`docs/ui/REFERENCE-EXTRACTION.md` §7 conflict #1 already resolved that §4 is *"the HUD-era slice"*
and that the real inventory is **~45 component sets / ~200 variants**, with `Menu Combo` adopted as
`UBRLeftRail`. §4 has not been updated to match.

**[V] A naming defect exists today.** §4 names `UBRKillfeedRow`; the code ships
`UBRKillfeedEntryWidget`. The skill's own naming law says a component present on one side only is a
defect. One of the two names has to move.

### 0.5 The design inputs are unusually complete

**[V]** These are done and are inputs, not work:

| Input | Where | State |
|---|---|---|
| Measured grid, 1280×720 | `UI-DESIGN-SYSTEM.md` §3, `ui-presentation` §5 | done |
| Screen invariants, grid math, two-state frame, drill-down taxonomy | `SCREEN-BUILD-SPEC.md` §1–§4 | done |
| Component geometry 1:1 (fills, strokes, padding, letter-spacing units, effects) | `COMPONENT-SPECS.md` | done |
| Type tokens + the author's palette + typeface licence question closed (Rajdhani + Roboto Condensed, OFL) | `REFERENCE-EXTRACTION.md` §2, §7b | done |
| Motion tokens, measured not guessed | `MOTION-MEASURED.md` | done |
| Screen inventory, 78 screens with KEEP/ADAPT/DROP | `REFERENCE-EXTRACTION.md` §4 | done |
| Art ledger: 815 image fills → ~192 distinct assets in 14 families | `ART-PASS-STAGE-2.md` | done |
| Nomenclature ledger: 1,561 Halo occurrences, ~130-term mapping table | `ART-PASS-STAGE-3.md` | done |
| Asset production ladder (5 tiers, generation last) | `ASSET-METHODS.md` | done |

**Note on screen counts.** The task brief says 75 screens across 12 `FE /` pages + 5 `HUD /` pages.
`REFERENCE-EXTRACTION.md` §4 inventories **78** and calls 5 of them (Campaign) DROP → **73**
KEEP/ADAPT. **[P]** The roadmap uses 73 and treats the delta as a counting difference between the
pasted-file page set and the original inventory, not as missing screens. Reconcile once, in Phase 0,
by counting frames on the 17 pages — it costs one Figma MCP call and stops the number drifting.

### 0.6 The ladder is blocked at the bottom

**[V]** `docs/BUILD-STATE.md` "Ladder blockers": rung 1 (UBT, three targets) **BLOCKED** — editor
and build must not overlap (R29.3/R36). Rung 2 (specs) **BLOCKED** — same lock, and
`Source/Breachpoint/Tests/` holds only `.gitkeep`. Rung 4b **BLOCKED** upstream by BP00's
Gauntlet/NuGet failure.

**[V]** `Tools/audit_blueprints/audit_r26.py` exists, has never been run, and is not wired into
rung 2 — R26 is enforced by goodwill (the ruling says so itself). **This roadmap is about to
multiply WBP count by ~20×. That is the moment goodwill stops being adequate.**

---

## 1. The two structural decisions this roadmap has to make before any WBP is authored

Both are `[P]`. Both change C++. Both are cheap now and expensive after twenty WBPs exist.

### D1 — `BlueprintImplementableEvent` hooks vs `BindWidgetAnim`

**[V]** `UBRHUDLayout` and `UBRKillfeedEntryWidget` expose **seven** `BlueprintImplementableEvent`s
— `BP_OnEntrySet`, `BP_OnShieldHit`, `BP_OnFleshHit`, `BP_OnHeadshotHit`, `BP_OnKillConfirmed`,
`BP_OnVitalsStateChanged`, `BP_OnMatchStateChanged`, `BP_OnKillfeedRebuilt` — and C++ calls all of
them (`BRHUDLayout.cpp:15,62,66,78,83,139,142,145,148,162,171,213`).

**The conflict.** `data-and-assets.md` permits a WBP to carry *"layout/anchors/**animation**"*.
`ui-presentation` §8 additionally states *"Zero graph nodes. R26's five conditions apply to WBPs
exactly as to the `BP_BR*` containers."* **Implementing a `BlueprintImplementableEvent` requires an
event graph node.** So as designed, these seven hooks are either unusable or they are eight
per-WBP law violations waiting to be filed.

**[P] Resolution: replace the hooks with named UMG animations driven from C++.**

```
UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
TObjectPtr<UWidgetAnimation> Anim_ShieldHit;   // …FleshHit, Headshot, KillConfirmed, …
```

C++ calls `PlayAnimation(Anim_ShieldHit)`. The WBP then carries **layout + anchors + a named
animation and zero graph nodes** — squarely inside the contract's own wording, and reviewable,
because the *trigger* stays in diffable C++ and only the *curve* lives in the binary.

Keep exactly one escape hatch **[P]**: if a treatment genuinely cannot be expressed as a widget
animation, it becomes a C++-side state enum the WBP reflects via an animation or a bound style
token — never a graph branch. **Decision owed to the founder before Phase 2 starts.**

### D2 — where the root layout is created

`CreateLayoutForLocalPlayer` needs a PlayerController **[V]**, which does not exist at
local-player-added time. Three ways to close it:

| Option | Owner path | Law-5 status |
|---|---|---|
| (a) Call from `ABRPlayerController::BeginPlay` | `Source/Breachpoint/Match/` | **Outside BP10's owner path** → contract_gap against BP04, not an edit |
| (b) **[P] Subsystem subscribes to `ULocalPlayer::OnPlayerControllerChanged`** and creates the layout when a PC arrives | `Source/Breachpoint/UI/` | **Inside** BP10's owner path. Preferred |
| (c) A `UBRUIPolicy`/game-state listener in `UI/` | `Source/Breachpoint/UI/` | inside, but more machinery than (b) |

**[P] Take (b).** It keeps the whole fix inside `UI/`, it handles seamless travel and re-possession
for free, and it is the smallest diff. If (b) turns out not to fire on this engine version, (a) is
the fallback **and it is a contract_gap, filed, not worked around**.

---

## 2. Phase map

```
P0 ─ P1 ─┬─ P2 ─ P3 ─┬─ W1 HUD ────────────────── P9 motion ─ P12 verify
         │           ├─ W2 FE spine ─────────────┘
         │           ├─ W3 session  (needs P5/BP24)
         │           └─ W4 post-match (needs P5/BP21+BP23)
         └─ P5 C++ gaps (BP21–25) ── parallel from day one
P10 art (Figma track) ─────────────── parallel, headless, from day one
W5 out-of-slice screens ───────────── after W1–W4, mostly Phase 2 of the game
```

**Critical path: P0 → P1 → P2 → P3 → W1.** Everything else hangs off it.

**Runs in parallel with everything:** P5 (C++ gap tickets — different owner paths, different
crew), P10 (the Figma-side art and nomenclature track — headless, no editor, no build).

**Sizes:** `S` = one packet/session · `M` = 2–3 · `L` = 4–8 · `XL` = more than 8.

---

## P0 — Ground truth, in an editor, once

**Goal:** stop guessing about three binary assets and one config file.

**Entry:** an editor session claimed under R29 (one editor, one driver) with **no build running**
(R21/R36). `git lfs pull` so `Content/UI/*.uasset` are real files, not pointers.

**Work**
1. Open `WBP_RootLayout`, `WBP_HUDLayout`, `WBP_KillfeedEntry`. Record for each: parent class,
   compile result, child-widget tree, graph node count.
2. Assert `WBP_RootLayout` contains the four exactly-named `CommonActivatableWidgetStack`s. If not,
   add them — that is pure layout, Tier 4, lawful.
3. Run `Tools/audit_blueprints/audit_r26.py` for the first time against the five `BP_BR*` assets
   **and** the three WBPs. Record the output verbatim.
4. Write the missing R37 receipts for BP18's three WBPs **[P]** into `docs/ui/receipts/`. Late
   receipts are worth more than no receipts, and the directory has to exist before Phase 3 lands
   twenty more assets into it.
5. **[P]** One Figma MCP `get_metadata` sweep over the 17 pages to settle the 73-vs-75-vs-78 count.

**Exit:** a Log entry naming, per asset, parent + compile + node count; `audit_r26.py` has one
recorded run; `docs/ui/receipts/` exists with three receipts; the screen count is one number.

**Unblocks:** everything. Phase 1's config values are meaningless if `WBP_RootLayout` does not
compile.

**Size:** S. **Risk:** the WBPs are empty shells and `WBP_RootLayout` does not compile — in which
case Phase 0 grew a small authoring job. That is the *good* outcome; the bad one is discovering it
in Phase 4 with a HUD built on top.

**Rung:** none claimed. This is inspection, not verification.

---

## P1 — Foundation: make the existing layer able to put a pixel on screen

**Goal:** the root layout appears in PIE, a placeholder HUD pushes onto `Layer.Game`, and a gamepad
can move focus and go back.

**Entry:** P0 exit. D1 and D2 decided.

**Work**

1. **Wire `BRUISettings` in `Config/DefaultGame.ini`** — one new section, six soft paths **[P]**:
   ```ini
   [/Script/Breachpoint.BRUISettings]
   RootLayoutClass=/Game/UI/WBP_RootLayout.WBP_RootLayout_C
   HUDLayoutClass=/Game/UI/WBP_HUDLayout.WBP_HUDLayout_C
   KillfeedEntryClass=/Game/UI/WBP_KillfeedEntry.WBP_KillfeedEntry_C
   ; MainMenuScreenClass / DeathOverlayClass / CarnageReportClass land with their screens
   ```
   Config over a BP default, per R26's corollary: diffable, greppable, survives a clone.
2. **D2: create the layout** when the PlayerController arrives. Then call `ShowHUD` once the local
   pawn is possessed.
3. **Feed the ViewModels.** `UBRVM_Combat::BindToAbilitySystem` and `UBRVM_Match::SetTimeSource`
   have to be called by *something*. The producers live in `Match/`, `AbilitySystem/` and
   `Weapons/` — **outside BP10's owner path**. **[P]** Put the subscription inside `UI/`: the
   subsystem watches for pawn possession and pulls the ASC via `UAbilitySystemGlobals`, rather than
   asking those modules to push. Anything that genuinely needs a new field on a producer is a
   **contract_gap**, not an edit (law 5).
4. **DPI + grid contract.** **[P]** `[/Script/Engine.UserInterfaceSettings]` with
   `UIScaleRule=ShortestSide` and a curve pinned so 720 → 1.0 and 1080 → 1.5, which *is* the design
   system's ×1.5 exactly. Consequence: **every asset is authored once at 1280×720 and no ×1.5
   arithmetic ever appears in a WBP.** This is the highest-leverage config line in the roadmap.
5. **The palette source.** `ui-presentation` §9 requires one source, never hex in a WBP. **[P]**
   Add colour + type tokens to `UBRUISettings` (config-backed, so they live in
   `DefaultGame.ini` — diffable) rather than a `DT_UIPalette` asset, because a DataTable of colours
   is a binary the critic cannot diff and this is not a tuning number under law 3. Tokens: the 12
   VISR semantics from `UI-DESIGN-SYSTEM.md` §2, plus the five discrete black alphas
   (0.2/0.4/0.5/0.6/0.8) and the rarity/premium *decorative* ramp — **flagged decorative, and
   forbidden on the HUD** per `REFERENCE-EXTRACTION.md` §7 conflict #2.
6. **CommonUI input data.** A `UCommonUIInputData` subclass **[P] in C++** declaring the default
   Click/Back actions, plus `CommonInputSettings` in config. Without it the activatable stack has no
   Back and `UBRButtonPrompt` has no action to render a glyph for.
7. **Gamepad + KBM parity, declared not assumed.** `bAutoActivate`, desired-focus targets, and back
   routing per `ue5-ui-architecture` §6. **[P]** Parity is a Phase-12 acceptance test, but the
   *hooks* are set here or every screen retrofits them later.
8. **Rung-2 gates wired.** **[P]** Add to `contracts/testing.md`: `audit_r26.py` over all `BP_BR*`
   **and** `WBP_*`, plus the greps from `ue5-ui-architecture` §8 (`NativeTick`, UMG property
   bindings, `CreateWidget` in a per-kill path, hard widget-class `UPROPERTY`,
   `ConstructorHelpers`, `SetInputMode*` outside `GetDesiredInputConfig`).

**Exit criteria**
- `WBP_RootLayout` is on screen in PIE with four working layer stacks.
- A placeholder HUD pushes to `Layer.Game` and a placeholder modal pushes over it and pops back,
  **on a gamepad, with the mouse unplugged**.
- `UBRVM_Combat` shows a real shield value; `UBRVM_Match` shows a running clock.
- A window resize from 1280×720 to 1920×1080 moves nothing relative to the grid.
- `audit_r26.py` runs green in rung 2 and `contracts/testing.md` names it.

**Unblocks:** every component and every screen. Nothing downstream is meaningful without this.

**Size:** M. **Risk (top):** `ue5-ui-architecture` carries an explicit *"UNVERIFIED DRAFT — never
run against a build"* banner **[V]**, and names CommonUI activation/input-routing and the MVVM
`FieldNotify` macros as the specific API risk. **This phase is where that draft meets a compiler.**
Budget a correction pass on the skill in the same packet — the skill says BP10 owns doing exactly
that.

**Rung:** 1 (three targets, R19 timestamp proof) → 2 (greps + R26 audit) → 3 (PIE). Rung 3 only;
no multiplayer claim is available or needed yet.

---

## P2 — The component layer, part A: C++ classes

**Goal:** every component named in the ledger exists as a `UBR*` class with its `BindWidget` slots
and style `UPROPERTY`s, compiled, **before any WBP is authored**. This is `ui-presentation` §8's
pipeline step 1 and 343's own stated order ("design system first").

**Entry:** P1 exit; D1 decided (it determines whether these classes expose animations or events).

**Sequencing law: by screens unblocked, descending.** Numbers below are from
`SCREEN-BUILD-SPEC.md` §1 (invariants), `COMPONENT-SPECS.md` §2–§6 and
`REFERENCE-EXTRACTION.md` §4.

| # | Class | Geometry (measured) | Screens unblocked | Note |
|---|---|---|---|---|
| 1 | **`SBRHairlineBorder`** (Slate, C++ only) | 4 vector lines, opacity 1 / 0.3 / 0.3 / 0.3, weights 0.5 / 1 / 2 px | **all 73** | The atom under the atoms. `COMPONENT-SPECS.md` §0 counts **404× 1px, 323× 0.5px, 177× 2px** strokes. Building this once instead of drawing four lines per WBP is the single biggest asset-count saving available |
| 2 | `UBRMenuRow` | 250×28, text frame 246×24, gap 10, pad L/R 10 | ~60 | 27 variants; 10-value Type axis (Default/Disabled/DropDown/DigDown/IconOnly/Slider/Checkbox/Radio/MapVoting/Image). **Hover is an inversion, not a highlight** |
| 3 | `UBRButtonPrompt` + prompt bar | 62×20 at (60,685); bar width tracks count: 58/62 · 133/146 · 253 | **all 73** | Auto-layout hug. Driven by the CommonUI input action from P1.6 |
| 4 | `UBRProfileBar` | 1280×50 at y=670, `#000000@0.5` + BACKGROUND_BLUR | **all 73** | Screen invariant. Always reserved |
| 5 | `UBRNavBar` + `UBRNavTab` | bar 666×30 at (33,45); tab 138×26, pitch 150, x = 39/189/339/489; bumper glyphs 27×15 at x=27/639 | ~40 | Active border **3px OUTSIDE**; inactive whole-component opacity 0.6 |
| 6 | `UBRPageTitle` / `UBRItemTitle` | 1280×75 / 1280×105 | ~45 | The 75↔105 swap shifts everything below by 40 and moves the nav bar y=45→75/110 |
| 7 | `UBRDescriptionStrip` | 349×37, Roboto Condensed Medium Italic 14, ls 8% | ~15 | Rajdhani ships no italic — the font split is deliberate |
| 8 | `UBRFeatureCard` | 349×222 (image 349×196.7 + caption), fill `#000000@0.5` | ~10 | Carousel dots |
| 9 | `UBRLeftRail` (`Menu Combo`) | 349×510 at (69,138) | ~12 | Composes 8 + 2 + 7. Adopted per `REFERENCE-EXTRACTION.md` §7 conflict #4 — it is why the rail stays aligned across Play/Create/Community |
| 10 | `UBRRosterPanel` + `UBRRosterRow` | panel 349×273 at (862,397); row 349 in-panel (390 standalone), h30 pitch 35; header h31 inset 16 | ~14 | Row anatomy: emblem 26 · gamertag · rank 30×26 · mic 16×18 · external icons gap **−5** (deliberate overlap). Text-colour axis flips against team-fill luminance |
| 11 | `UBRProgressBar` | vitals, cooldowns, match record | ~20 | **Two treatments, not one** — flat in the front end, full VISR in the HUD (`ui-presentation` §4). If they look identical, one is wrong |
| 12 | `UBRScrim` | 1280×720 dim plate | 11 | `SCREEN-BUILD-SPEC.md` §5 |
| 13 | `UBRScrollBar` | slim 8×N (thumb 8×70), wide 13×N (thumb 7×70) | every grid + Settings | |
| 14 | `UBRVitalsWidget` | HUD | 1 | Not in Figma — the reference file has no HUD. **Original design**, per `REFERENCE-EXTRACTION.md` §8 gap 2 |
| 15 | `UBRReticleWidget` | HUD | 1 | **Geometry must not change with state — colour only** (BP22's binding constraint). A state that resizes or nudges is a `high` finding |
| 16 | `UBRKillfeedRow` | HUD | 1 | **Naming defect [V]:** ledger says `UBRKillfeedRow`, code ships `UBRKillfeedEntryWidget`. **[P] keep the code name** and fix `UI-DESIGN-SYSTEM.md` §4 — renaming a compiled class to satisfy a doc is the tail wagging the dog |
| 17 | `UBRItemTile` | 114×114; art 100×100 at (7,7); bottom line = rarity colour | ~20, all out-of-slice | Defer to W5 |

**Exit:** every class above compiles on three targets; each declares its `BindWidget` slots and
reads colour/type from the P1.5 token source; **zero hex literals** in any of them; the ledger table
in `UI-DESIGN-SYSTEM.md` §4 is replaced by the real inventory with a maintained `Built?` column.

**Unblocks:** P3, and through it every wave.

**Size:** L. **Risk:** over-building. `COMPONENT-SPECS.md` documents ~45 sets / ~200 variants and
the slice needs perhaps 16 of them. **[P] Build 1–16; stop.** A component authored for a screen in
wave 5 is a component that will be rewritten before wave 5 arrives.

**Rung:** 1 + 2. A C++ class with no asset cannot make a rung-3 claim, and must not pretend to.

---

## P3 — The component layer, part B: WBP layout assets

**Goal:** one WBP per P2 class, reparented to it, carrying **layout + anchors + animation and
nothing else**.

**Entry:** P2 exit (classes compiled). A claimed editor session, no build running (R21/R29/R36). A
**committed plan file** per batch (R37.1).

**Work**
- One WBP per component, built to `COMPONENT-SPECS.md` geometry exactly.
- **[P] Batch by ledger group, one receipt per batch** — chrome (1–7), rail (8–10), feedback
  (11–13), HUD (14–16). Four batches, four receipts, four reviewable units. `Content/UI/` is one
  owner per ticket (law 7); batching is what keeps that from serialising the whole phase.
- Each batch: committed plan → MCP or editor authoring → committed receipt naming every call and
  its result → `audit_r26.py` green.

**Exit:** every WBP compiles; `audit_r26.py` reports zero graph nodes and zero added members across
all of them; every batch has a committed plan and a committed receipt in `docs/ui/receipts/`; a
gallery map renders all components at 1280×720 and is screenshotted.

**Unblocks:** all four in-slice waves simultaneously.

**Size:** L. **Risk (top):** **R37's enforcement gap, stated in the ruling itself** — `guard_laws.py`
gates `Edit`/`Write` by `file_path` and **an MCP call has neither**. Twenty binary assets are about
to land through the one authoring route with no mechanical control on it. The receipt is the only
thing standing there. **[P] A batch with no committed receipt does not land**, and Phase 12 asserts
receipt-count == batch-count.

**Second risk:** BP18 proved **[V]** the MCP *can* create WidgetBlueprints but *cannot* rename
assets (modal dialog auto-cancels) and cannot create levels. **Name every WBP correctly on
creation** — there is no rename path.

**Rung:** 3 (the gallery map renders in PIE). Not 4 — a component gallery has no networked state.

---

## W1 — Screen wave 1: the in-match HUD

**Goal:** the surface with the most gameplay value and the fewest external dependencies.

**Entry:** P3 exit (HUD batch). **[V]** `UI-DESIGN-SYSTEM.md` §6: *"Bindable today, with no new
C++: the entire in-match HUD except the reticle colour state."*

**Scope**

| Surface | Anchor | Binds to | Blocked? |
|---|---|---|---|
| Vitals (shield over health) | top-centre trapezoid, health nested bottom-centre | `ShieldPercent`, `HealthPercent`, `bShieldsBroken` | no |
| Ammo + active/stowed weapon | bottom-right | `MagazineAmmo`, `ReserveAmmo`, `ActiveWeaponName`, `StowedWeaponName` | icon needs **BP25** |
| Grenades + grapple ring | bottom-left | `GrenadeCount`, `GrappleCooldownDuration`, `bGrappleReady` | no |
| Match band (score, clock, rocket) | bottom-centre | `Team0Score`, `Team1Score`, `MatchClockText`, `RocketCountdownText` | no |
| Killfeed + Spotter line | top-right | `KillfeedEntries` + `FUserWidgetPool` | no |
| Reticle + hit markers | centre | hit-marker event fields exist | **colour state needs BP22** |
| Motion tracker | per founder reversal, 2 Aug | — | **no producer in C++ at all — file it** |

**On the motion tracker:** `UI-DESIGN-SYSTEM.md` §5 records the founder reversing the cut and
putting a Halo-Infinite-1:1 tracker **in scope** (18 m precise / 30 m edge-direction, crouch-walk
undetected, disabled in Ranked/Tactical). **[V] Nothing in `Source/` produces that data.** It is a
sixth C++ gap and is not on any ticket. **[P] File it. Do not draw a tracker with no feed.**

**Exit:** all six surfaces bound, no polling (grep-gated), honest empty states on join-in-progress
(`ue5-ui-architecture` §7: null PlayerState, ASC before `InitAbilityActorInfo`, GameState before
team assignment — **all three happen**), killfeed pool exhaustion drops the oldest and **logs** it.

**Unblocks:** W4 (death/carnage reuse the vitals and match bindings), and it is the first surface a
playtest can react to.

**Size:** M. **Risk:** the first frame. A confident `0/100` where the truth is "unknown" is a
finding, not a nit — the ViewModels already carry `EBRUIDataState::Unknown` **[V]** and the widgets
must actually render it.

**Rung: 4b (listen + one remote client), and this is the phase where the ladder stops being
theoretical.** Law 6: multiplayer claims come in threes — server, acting client, observing client.
A scoreboard right on the host and wrong on a remote is the exact bug this layer exists to surface.
**[V] Rung 4b is BLOCKED upstream by BP00's Gauntlet/NuGet failure.** Until that clears, W1's honest
verdict is **rung 3, PIE only**, said out loud.

---

## W2 — Screen wave 2: the front-end spine (runs parallel with W1)

**Goal:** boot → main menu → into a match. Five screens, all `KEEP`, none needing a ViewModel that
does not exist.

**Entry:** P3 exit (chrome + rail batches).

**Scope:** `FE_Splash` (`266:1762`) · `FE_Loading` (`572:10452`) · `FE_Play` (`1:2`, the measured
reference screen — nav bar + `Menu Combo` + Party List + Profile Bar) · Control Panel /
pause (`619:4854`) · Settings (`1031:13111`).

**The composition law is a scope item, not a polish item.** `ui-presentation` §1: the front end is a
camera in `BR_Arena01`, not a rectangle on black; left third UI, centre subject, right status;
right band `x>=650` reserved with exactly two documented exceptions. **[V]** `BR_Arena01` exists as
a blockout (BP18 step 4: 44 elements, 0 failures). **[P] Budget the camera rig here.** A menu with
no scene behind it is a different, worse design that no widget polish fixes.

**Exit:** boot → menu → Solo-vs-bots → match, entirely on a gamepad; Settings persists at least one
value; every screen respects the invariants (profile bar reserved, prompt bar tracking count,
nothing crossing into the right band).

**Unblocks:** W3. **Size:** M. **Risk:** Settings is a schema problem wearing a UI hat — audio,
input rebinding, video. `SCREEN-BUILD-SPEC.md` §5 flags the Input Map Diagram (591×291) as
**must-be-original art**. **[P] Ship Settings v1 with the three settings that exist and no
rebinding**; rebinding is a wave-5 screen.

---

## W3 — Screen wave 3: session and roster (gated)

**Entry:** W2 exit **and BP24 landed**. **[V]** `UI-DESIGN-SYSTEM.md` §6 gap 4: *"No lobby ViewModel
at all — blocks every front-end screen."* **[V]** BP24 is cut and open: `UBRVM_Lobby` subscribing to
the ten `UBRSessionsSubsystem` delegates that already fire and that **no line of UI listens to**.

**Scope:** Host/Join entry · `MM_Searching` (`933:8346`) · Lobby + roster · `RS_Squad` (`76:5392`) ·
`Warning Message` (349×60) and `Full Page Warning` (`940:10676`) · `Pop-Up Options` (451×682 —
**same footprint as `Filter Page`, so one component with two variants**, per `SCREEN-BUILD-SPEC.md`
§3.9).

**Exit:** host → invite → join → lobby → travel → match, gamepad-only; every session failure and
disconnect renders as player-facing `FText` from the VM, never a blank; join-in-progress roster
shows honest unknowns.

**Unblocks:** the M6 stranger test and anything resembling a playtest with other people.

**Size:** M (assuming BP24 lands clean). **Risk:** **[V] BP24's ordering law forbids adding a
session API** — if the VM needs state the subsystem does not expose, that is a contract_gap against
BP11, not an edit. Expect at least one. **Second risk:** the lobby roster width is **unmeasured**
(310 vs 349, `REFERENCE-EXTRACTION.md` §8 gap 4). One Figma call settles it; do it before authoring.

**Rung: 4b, mandatory and non-negotiable.** This is session code. PIE proves nothing here.

---

## W4 — Screen wave 4: the match's memory (gated)

**Entry:** W1 exit **and BP21 + BP23 landed**. **[V]** `UI-DESIGN-SYSTEM.md` §6 gaps 1 and 3:
*"no object in this repo holds a per-player number"* (BP21) and the respawn clock is
server-private (BP23).

**Scope:** death overlay (killer cam + respawn timer) · carnage report (K/D/A, accuracy, medals,
coach-line slot) · rematch flow.

**Exit:** the carnage report reads correctly on **all three of server, acting client and observing
client** (law 6); the death timer counts down from a replicated server time rendered locally — **not
a ticking replicated countdown** (the shape BP04 already proved with `MatchEndServerTime`).

**Size:** M. **Risk:** the medal list. **[V] The repo holds two irreconcilable medal lists** — 11
shipped rows in `DT_Medals.csv` vs 16 doc-only names in `ART-PROMPT-LIBRARY.md`, overlapping
partially: nine shipped medals have no icon, eight icons have no medal
(`ART-PASS-STAGE-3.md` §5). The carnage report is where that lands. **Settle it with decision N5
before authoring, not during.**

**Rung: 4b.** A scoreboard is the canonical three-views bug.

---

## W5 — Screen wave 5: out of slice, named so it stops being invisible

**[V]** `REFERENCE-EXTRACTION.md` §4: of 78 screens, 5 are DROP (Campaign) and **~55 of the
remaining 73 are progression, store, operator customization, Forge, file browser and news** — none
of which is vertical-slice scope, and **[V]** BP10's own ticket lists settings screens, radar and
cosmetics as out of scope.

**[P] Do not build these in the slice. Do keep them in the ledger**, because they are what the
component layer was measured against and dropping them silently would make P2's variant count look
like over-building when it was scoped deliberately.

Three things from W5 are worth stealing early **[P]**:
- The **two-state frame** (list ↔ grid on one screen, `SCREEN-BUILD-SPEC.md` §2) — because getting
  it wrong doubles the screen count for no reason, and the mistake is cheapest to avoid now.
- The **selection caret** (`Rectangle 278`, 3×65 at x=−4, authored y encoding the focus index) and
  the **panel reveal notches** (`Rectangle 258/259`, 88×4.7 chamfers — *the wipe originates from
  these, which is why the panel unzips rather than fades*). Both are P9 motion primitives that every
  in-slice screen uses.
- Campaign's **Difficulty Select radial** — explicitly flagged as the one carry-over from a DROP
  page, and it is the bot-difficulty picker.

---

## P5 — Data plumbing: the ViewModels and the C++ gaps (parallel from day one)

**This phase runs beside everything and is on nobody's critical path except W3 and W4's.**

### What each screen needs, and where it comes from

| Wave | ViewModel | State |
|---|---|---|
| W1 HUD | `UBRVM_Combat`, `UBRVM_Match` | **[V] Exist and are complete for the HUD** — every getter the HUD needs is on them today. They are simply never fed (§0.1) |
| W2 spine | none (static + settings) | — |
| W3 session | `UBRVM_Lobby` | **BP24, open** |
| W4 post-match | `UBRVM_Match` + a stat-block source | **BP21, open** |

### The gaps, all filed as tickets — work them, do not work around them

| Gap | Ticket | Blocks | Shape |
|---|---|---|---|
| Per-player stat block | **BP21** | the whole carnage report | `FBRPlayerStatBlock` + **one** `RepNotify` for the struct, not seven properties. Match meta is a named exception in `gas-purity.md` |
| Reticle target state | **BP22** | reticle colour only | **Client-local read of a trace the client already runs.** Never a replicated surface — telling a client about an enemy through a wall is a wallhack we shipped ourselves |
| Respawn countdown | **BP23** | death screen timer | One `COND_OwnerOnly` replicated float; client renders the clock. Copies the `MatchEndServerTime` pattern |
| Lobby ViewModel | **BP24** | **every** front-end screen | One VM listening to ten delegates that already fire |
| Weapon icon field | **BP25** | ammo block icon, item tiles | `TSoftObjectPtr<UTexture2D>` on the row + CSV column + VM field. **Order is strict: C++ field before CSV column**, or reimport warns on an unknown column |
| **Motion tracker feed** | **none — [P] file it** | the tracker | Founder put it in scope; no producer exists |

**The law that matters here, stated once:** *"If a field does not exist on a ViewModel, that is a
C++ gap — file it, do not work around it in the widget"* (`ui-presentation` §8.3). A widget that
reaches into the pawn, the ASC or the GameState to cover a missing field is a finding, not a
shortcut.

**Size:** L across the six. **Risk:** **[V] BP21 step 2 and BP22 both need `BRGA_WeaponFire`, which
does not exist** — `BUILD-STATE.md` ranks it MISSING with two open blockers (`FBRWeaponRow` carries
no trace range and no spread; `DT_Weapons.csv` has no `AbilitySet` column). **So the reticle state
and the accuracy counters are blocked behind BP03's fire path, not behind UI.** Say that plainly
rather than scheduling around it.

---

## P9 — Motion and interaction

**Goal:** apply `MOTION-MEASURED.md`. **[V] These are measured facts, not preferences**, extracted
frame-by-frame with curve fits and RMSE — the document explicitly supersedes the reflex
`0.2s ease-in-out`, showing `ease-in-out` is *measurably the wrong shape* (RMSE 0.191 vs 0.066).

**Entry:** P3 exit. **In practice this interleaves with W1–W4** — a component's animation is
authored in the same WBP as its layout. It is a separate phase here because the *tokens* are set
once and the review is one pass.

**The token set — [P] name them in `UBRUISettings` beside the colours so a WBP never types a number:**

| Token | Value | Source |
|---|---|---|
| `Motion.Ease.Standard` | `cubic-bezier(0.45, 0.15, 0.10, 1.00)` | fits **five independent series across three assets**, mean RMSE 0.066 |
| `Motion.Panel.InOut` | **330 ms**, same curve both directions | centre-anchored expand/collapse, RMSE 0.007/0.005 |
| `Motion.Stagger.List` | **150 ms** per beat | three-beat reveal, 450 ms block |
| `Motion.Chip.Narrow` | **360 ms** | one edge pinned, the other slides |
| `Motion.Glitch` | **60 ms period / 16.7 Hz**, hard 2-frame toggle | three assets agree. **Damage/impact only — 16.7 Hz would strobe as a searching indicator** |
| `Motion.Type.On` | **linear, ≈520 px/s** (≈42 ms per 22 px glyph) | constant rate, no easing at either end |
| `Motion.Banner.Hold` | **1590 ms** hold, 2460 ms total life | transitions are 27% of the budget; the hold is 65% |

**Two structural rules that are not timings:**
1. **The frame does not animate.** A container stroke goes 43 px → 278 px in **one frame (30 ms)** —
   a hard cut. **Snap the container, ease only the fill.**
2. **The loader is a one-shot sting, not a spinner.** `SCREEN-BUILD-SPEC.md` §7's "4-frame Loading
   Icon" is the **wrong asset class** — the reference is a 60-frame 1800 ms sting that is 13% black
   by duration and therefore cannot loop. **[P] Use it as a load-screen entry sting and author a
   separate busy spinner.**

**Interaction rules, from geometry not taste:**
- **Idle → Hover is an *inversion*, not a highlight**: fill goes solid white, text goes black,
  bottom-line opacity 0.3 → 1. *"That single rule explains most of the file."*
- **Selection caret** slides on the rail; its authored y encodes the focus index (54/78/98/138/180).
- **Focus is CommonUI's job.** Hand-rolled focus math is a finding (`ue5-ui-architecture` §6).
- **Two things this project must not take from the reference:** progress-bar fill timing (VISR's bar
  is trailer choreography — 4 scripted jumps — not a data-driven bar) and any full-screen loading
  timing (Shader Cache is a pre-rendered cinematic and is not a valid source for a real-time
  number). Both are named as invalid in the source document; only the ≈360 ms *total read* transfers.

**Exit:** zero hard-coded durations in any WBP; the seven tokens are the only timing source; the
one-shot-vs-spinner split exists as two assets; the searching-loop period is still **open** and is
labelled open rather than guessed.

**Size:** M. **Risk:** UMG animation curves are authored in a binary. **[P] The token is the C++
value and the animation is normalised 0→1**, so the curve is reviewable as a number even when the
asset is not.

---

## P10 — Art integration (parallel, headless, from day one)

**[V] The art pass is a seven-stage track with its own document set.** Stages 1–3 are done as
*survey*; execution is not.

| Stage | State | Plugs into |
|---|---|---|
| 1 strip hidden underlays | **DONE** | — |
| 2 inventory | **DONE** — 815 image fills → **~192 distinct assets** in 14 families | scoping |
| 3 nomenclature | **SURVEYED, blocked** — 1,561 occurrences, ~130-term mapping | before any string ships |
| **3a component ownership** | **NOT STARTED — the real blocker** | before stage 3 |
| 4 produce art | not started; **~110 of 192 are one scripted render pass** | W1–W5 |
| 5 swap into Figma | gated on 4, family by family | — |
| 6 export to UE | gated on 5 | P3/waves |
| 7 verification | gates shipping | P12 |

**Stage 3a is the finding that reorders this track. [V]** Of 393 text nodes carrying Halo strings,
**346 sit inside instances**, and **227 of 290 instance-borne strings resolve to components living
on a *reference* page**. Consequences: a layer inside an instance **cannot be renamed**; editing
text creates 346 per-instance overrides that diverge forever; and the only correct fix site is a
main component on a page the founder asked to preserve. **The prerequisite is small and
well-shaped: author the 8 missing components on our own pages and repoint the instances — 22
components, not 1,561 nodes.** *"Doing stage 3 before stage 3a means doing it twice."*

**Ordering, and how it meets the UE track:**
- **[P] Do 3a first, then stage 3's decision-free half.** That mapping table clears **828 of 1,561
  occurrences (53%) with no decision from anyone** — it is grounded in names already committed to
  the repo (`DT_Weapons.csv`, `DT_Medals.csv`, `ART-PROMPT-LIBRARY.md`, `BRGameplayTags.h`).
- **[P] Do the 132 Halo-named nodes on our *own* pages today** — no prerequisite, and the HUD ones
  are the most visible thing in the file.
- Stage 4 order is already optimised by node-count-per-asset: **NP + PC (256 nodes, 3 assets) →
  EM (134 nodes, 9 drawings) → BG (75 nodes, ~8 cameras)**. Those three clear **465 of 815 nodes
  (57%) with no editor and no credits.**
- **IT + CH + MAP (222 nodes) need the render rig, which needs a live editor** — and **[V] R21/R29
  means that cannot happen in a headless session.** Schedule it in the *same* editor session as a
  P3 WBP batch; do not open the editor twice for it.
- **[V] Six founder decisions are owed** and gate the remaining 47%: N1 map roster (147 nodes;
  strong seed already in `arena_manifest.json` — The Core, The Gantry, Mezzanine Catwalks…), N2
  armour/coating names (235), N3 faction names (161), N4 seasons (79), N5 commendations (58), N6
  mode roster (53).
- **[V] N6 carries a live conflict:** `DT_SpotterLines.csv:49` ships *"Team Slayer. Live."* in VO
  while stage 3 treats `slayer` as a removal term. **Keep it or rename it and re-record — it cannot
  be both.**

**Cost discipline, learned the expensive way. [V]** `ASSET-METHODS.md` was written after burning
28.48 of 34 generation credits on four sheets: **generation is Tier 5, the last resort.** Rank
insignia (16), grades, mode icons (14), gametype icons, difficulty icons, currency marks,
checkbox/radio, button prompts, carousel dots, progress arcs, **the reticle set**, damage wedges,
Forge quadrants, scroll bars and the VISR linework are **Tier 1 parametric geometry** — *"a
specification for code, not a brief for an artist."*

**Legal boundary, restated because it is not negotiable [V]:** one-to-one on geometry and
behaviour, **original on art**. Typeface, medal icons, emblems, rank insignia, visor artwork and
brand marks are 343/Microsoft's. Breachpoint ships on Steam. The typeface question is already closed
— Rajdhani + Roboto Condensed is *the same substitution the reference file itself makes*, and is
OFL.

**Size:** XL — *"the other six stages together are smaller than this one."* **Risk:** stage 5 is
gated family-by-family on stage 4, and stage 7 gates shipping any of it.

---

## P12 — Verification: what proves each phase, and on which rung

**[V] Law 6, the ladder:** compiles ≠ works · PIE ≠ multiplayer · listen ≠ dedicated · editor ≠
packaged. Every "works" names its rung. Multiplayer claims come in threes.

**[P] Rung 0 exists and is not an engine rung.** `ui-presentation` §7's headless-Chromium mockup at
1600×900 is a real reviewable artifact and the skill's §11 self-check requires that a screen be
*rendered and looked at*. **It proves layout intent and nothing else.** Calling a Chromium
screenshot a UI verification is the false-PASS class R19 exists for, wearing a different hat.

| Phase | Rung | The proof | Currently |
|---|---|---|---|
| P0 | — | inspection log: parent + compile + node count per asset | needs an editor |
| P1 | 3 | root layout on screen; modal push/pop **on a gamepad, mouse unplugged**; resize 720→1080 moves nothing | **[V] rungs 1–2 BLOCKED** (editor/build lock; `Tests/` is a `.gitkeep`) |
| P2 | 1 + 2 | three targets green under R19's five-item timestamp proof; grep gates; `audit_r26.py` | rung 1 blocked |
| P3 | 3 | component gallery renders; **receipt count == batch count**; `audit_r26.py` zero graph nodes | needs an editor |
| W1 | **4b** | HUD read on server + acting client + observing client | **[V] 4b BLOCKED** upstream by BP00's NuGet failure → honest verdict today is **rung 3** |
| W2 | 3 | boot → menu → match, gamepad only | |
| W3 | **4b** | host → invite → join → lobby → travel; failures render as text | mandatory; session code |
| W4 | **4b** | carnage report correct on all three views | mandatory |
| P9 | 3 | zero hard-coded durations; tokens are the only source | |
| P10 | 7 | **no Halo-owned pixel or string survives in Figma or in `Content/`**; attribution ledger settled | gates shipping |

**Standing gates [P], added to `contracts/testing.md` in P1:**
1. `audit_r26.py` over every `BP_BR*` **and** every `WBP_*` — node count, added-member count, parent
   chain. **[V] R26 says in its own text that without this the exception "erodes to *only a little
   logic* within a month."**
2. The `ue5-ui-architecture` §8 greps: `NativeTick` · UMG property bindings · `GetPlayerState()` /
   `GetASC()` polled from a widget · `CreateWidget` in a per-kill path · hard widget-class
   `UPROPERTY` or `ConstructorHelpers` · gameplay literal in a widget · `SetInputMode*` outside
   `GetDesiredInputConfig()` · unbound delegate surviving `NativeOnDeactivated`.
3. **[P]** Zero hex literals and zero raw durations in `Content/UI/` — both have a single named
   source after P1.5 and P9.
4. **[P]** Receipt count == WBP batch count. The only control R37 has.

**Acceptance tests that are not rungs:**
- **Gamepad parity, unplugged mouse, no coaching**: menu → match → death → rematch. BP10's own
  Done-when.
- **First-frame honesty**: a client that joined **mid-match**, not one present at map load. All
  three join-in-progress states happen and a confident wrong number is a finding.

---

## 3. What cannot be done from a headless terminal session

Stated plainly because half this roadmap is gated on it.

**Cannot, at all:**
- Author, open, or compile any WBP. Every Phase 3 batch and all of P0 needs a live editor.
- Verify `Content/UI/*.uasset` contents — they are **LFS pointers** in this checkout **[V]** and
  there is no editor to open them with.
- Run any rung. Rung 1 needs a build; **[V] a build and an editor must not overlap (R29.3, widened
  by R36 to anything taking the project lock)**; rungs 2–5 are downstream of rung 1.
- The stage-4 render rig for IT/MAP/BG. **[V]** Named as editor-blocked in `ART-PASS-STAGE-2.md` §7.
- **[V]** Rename an asset via MCP — `AssetTools.move()` returns false because a modal dialog
  auto-cancels. Names are chosen at creation or not at all.
- **[V]** Create a level via MCP — no `new_level`; the workaround is duplicate-and-strip.

**Can, headless, right now — and this is the parallelism to exploit:**
- The entire **Figma track**: stage 3a component ownership, the decision-free nomenclature mapping,
  the 132 Halo-named nodes on our own pages, the screen-count reconciliation.
- All **C++** authoring — P2's classes, P5's gap tickets — written editor-closed and built later
  (`ui-presentation` §8.1: *"Written with the editor CLOSED, then built"*).
- Every **config** change in P1.
- Tier-1 **parametric icon generators** — code, not assets.
- **Rung-0 mockups** in headless Chromium, labelled as rung 0.

**The scheduling consequence [P]:** editor time is the scarce resource and it is serialised by two
separate locks. **Batch it.** One editor session should do: P0 inspection → a P3 WBP batch → the
stage-4 render rig. Opening the editor for one WBP is the most expensive way to author one WBP.

---

## 4. Risk register — the five that would actually hurt

| # | Risk | Why it is real | Mitigation |
|---|---|---|---|
| 1 | **`ue5-ui-architecture` is an unverified draft** | **[V]** the skill says so itself; CommonUI's activation/input surface moved across 5.x and MVVM `FieldNotify` macros are *"either exactly right or a compile error"* | P1 is its first contact with a compiler. Budget a correction pass in the same packet; the skill says BP10 owns it |
| 2 | **R37 has no mechanical enforcement** | **[V]** `guard_laws.py` gates by `file_path`; an MCP call has neither. ~20 binaries are about to land through the one blind route | Receipts, batched and committed. No receipt, no landing. Asserted in P12 |
| 3 | **Building for 73 screens instead of ~14** | 55 of 73 are out-of-slice; `COMPONENT-SPECS.md` documents ~200 variants | P2 builds components 1–16 and stops. W5 is named, ledgered, and not built |
| 4 | **The reticle and accuracy work is blocked behind BP03, not behind UI** | **[V]** `BRGA_WeaponFire` is MISSING with two open data blockers | Do not schedule BP22/BP21-step-2 as UI work. W1 ships with a fixed-colour reticle and says so |
| 5 | **Art volume mis-scoped as 815 instead of ~192** | **[V]** four families are 500 of 815 nodes and are all Tier 1/3 repetition; *"doing them as 500 assets would be the single most expensive mistake available"* | Schedule by `ART-PASS-STAGE-2.md` §3 multipliers, never by node count |

---

## 5. What this roadmap deliberately does not decide

- **D1 and D2** are proposals awaiting a founder call. D1 changes a compiled C++ surface; making it
  after twenty WBPs exist means editing twenty binaries.
- **The six nomenclature decisions (N1–N6)** and the **N6 Slayer conflict**. Not UI's to settle.
- **The medal-list contradiction** (11 shipped rows vs 16 doc names). It lands in W4's screen; it is
  not W4's decision.
- **Whether the motion tracker is in the slice.** The founder put it in scope; **[V] no C++ produces
  its data and no ticket owns it.** This roadmap files the gap and draws nothing.
- **The searching-loop animation period.** **[V]** `MOTION-MEASURED.md` explicitly refuses to
  transfer the 60 ms glitch cadence to it. It stays open rather than guessed.
