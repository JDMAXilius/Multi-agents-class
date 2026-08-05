# Button module — the move/delete/keep ledger

**Status:** v2, 4 Aug 2026. **Files-only half EXECUTED; editor half is `TICKET_BP80`.**

> Founder confirmed 4 Aug: the sourced pack goes to the **tracked** `Content/UI/Reference/Buttons/`
> (NOT the gitignored `Content/Reference/`), and our 4 Aug WBPs to `Content/UI/OldWidgets/Buttons/`.
>
> **Landed already:** `BRButton.h/.cpp` (six files merged into two), the 24 consumer repoints,
> the `button()` factory with all nine assets through it, the five deleted plan entries, and the
> six `DefaultGame.ini` repoints. `PLAN OK` passes — which also re-validates every `BindWidget`
> name in all nine trees against the merged header.
>
> **Not landed:** anything needing an editor, a compiler, or a render. That is `TICKET_BP80`.

Founder directive, this session: the button system becomes **one modular C++ class** with the
type as data; the sourced third-party pack is **archived, not deleted**; the ten button WBPs
built on 4 Aug are **archived, not deleted** ("those ones are perfect … but because we are
making it modular"); everything else in the button family goes.

Scope is the **button family only**. `WBP_ButtonCheckbox` is the reference case and the first
thing rebuilt.

---

## 1. The three verdicts

| Verdict | Meaning |
|---|---|
| **ARCHIVE** | moved out of the active path, kept in the repo, still readable |
| **DELETE** | removed from the repo |
| **KEEP** | stays exactly where it is |

---

## 2. ARCHIVE — 27 assets, zero deletions

### 2.1 The sourced third-party pack → `Content/UI/Reference/Buttons/`

Seventeen `W_*` assets that landed with BP78 and currently sit interleaved with ours in
`Content/UI/Components/Buttons/`:

```
W_AbilityCooldownButton   W_AbilityReady        W_ButtonChangeSelection
W_CheckBox                W_ConfiguratorButton  W_DialogPrompt
W_Dropdown                W_EditableText        W_EscMenu
W_GlassRectangleButton    W_GlassSquareButton   W_Icon
W_IconButtons             W_PlayButton          W_ProgressBar
W_RoboButton              W_TypeText
```

The sharpest reason to move them: **`W_CheckBox` currently sits directly beside
`WBP_ButtonCheckbox`.** Different origin, different rules, one letter apart.

> ### ✅ DECIDED 4 Aug — tracked folder, NOT the gitignored one
>
> `Content/Reference/` already exists and is **gitignored** (`.gitignore:42`, `Content/Reference/*`)
> because it holds extracted game content and uncleared packs — "committing to a remote is
> distribution, which is the exposure worth preventing mechanically."
>
> Moving the sourced pack there would **untrack all 17 assets**: they would vanish from every
> fresh clone. That is almost certainly not what "move it to a references folder" meant for a
> pack we deliberately sourced and licensed (R40).
>
> **CONFIRMED: `Content/UI/Reference/Buttons/` — a NEW tracked folder**, distinct from the
> gitignored `Content/Reference/`. Same intent, no silent data loss.

### 2.2 Our 4 Aug button WBPs → `Content/UI/OldWidgets/Buttons/`

Ten assets, built and audited 8/8 against the Figma measurements (`73d4d85`). They are correct;
they are simply built on the pre-modular shape.

```
WBP_Button          WBP_ButtonDefault    WBP_ButtonCheckbox   WBP_ButtonRadio
WBP_ButtonDropDown  WBP_ButtonDigDown    WBP_ButtonIconOnly   WBP_ButtonSlider
WBP_ButtonMapVoting WBP_ButtonImage
```

They stay as the **visual reference for the rebuild** — the new modular assets must render
identically, and these are what "identically" is measured against.

---

## 3. DELETE

### 3.1 Source — 6 files, 857 lines

| File | Lines | Why |
|---|---|---|
| `Source/Breachpoint/UI/Components/BRSettingsRow.h` + `.cpp` | 248 | folds into `UBRButton` as a type, not a subclass |
| `Source/Breachpoint/UI/Styles/BRButtonStyles.h` + `.cpp` | 190 | folds into `BRButton.h` — styles are referenced by path, so zero include churn |
| `Source/Breachpoint/UI/Components/BRHighlightButton.h` + `.cpp` | 419 | folds in — zero files included it |

**Renamed, not deleted:** `BRMenuRow.h/.cpp` → `BRButton.h/.cpp`. The class keeps its behaviour
and gains the chrome-via-style-brush simplification; further shrinkage is real but unquantified
until written.

**Also merged, 4 Aug (second pass):** `BRHighlightButton.h` + `.cpp` (419 lines) — **zero files
included it**, so the merge cost no include churn at all. File-only: `EBRHighlightButtonType`
stays its own axis and the accent-fill rule is untouched. Founder: *"we should only have BRButton
for the source."* **Button source is now one file pair — 8 files → 2.**

**Not touched, and the measurement that decided it:** `BRHairlineBorder` (12 includers, **11 not
buttons**), `BRComponentTokens.h` (**23 consumers**), `BRUITokens.h` (9), `BRTextStyles.h` (2).
None is button source — they are the drawing layer and the design system buttons consume, no more
this module's than `CommonButtonBase.h` is. Folding the hairline in would make eleven unrelated
widgets include the button module to paint a line.

### 3.2 Content — 5 widget assets

```
✗ Content/UI/Components/WBP_MenuRow.uasset               identical tree to WBP_SettingsRow
✗ Content/UI/Components/WBP_SettingsRow.uasset           identical tree to WBP_MenuRow
✗ Content/UI/Components/WBP_SettingsRow_Checkbox.uasset  = ButtonCheckbox + Selection
✗ Content/UI/Components/WBP_SettingsRow_DropDown.uasset  identical to ButtonDropDown
✗ Content/UI/Components/WBP_SettingsRow_Slider.uasset    = ButtonSlider + Selection
```

> ### ✅ RESOLVED 4 Aug — the six config lines are repointed
>
> `Config/DefaultGame.ini` resolves four settings-row classes and the menu-row atom to assets on
> this delete list:
>
> | Line | Key | Currently → |
> |---|---|---|
> | 56 | `SettingsRowClass` | `WBP_SettingsRow` |
> | 60 | `SettingsRowSliderClass` | `WBP_SettingsRow_Slider` |
> | 61 | `SettingsRowCheckboxClass` | `WBP_SettingsRow_Checkbox` |
> | 62 | `SettingsRowDropDownClass` | `WBP_SettingsRow_DropDown` |
> | 89 | `MenuRowWidgetClass` | `WBP_MenuRow` |
> | 95 | `RowWidgetClass` | `WBP_MenuRow` |
>
> **All six now point at their surviving `Buttons/` twin** (`SettingsRow*` → `WBP_ButtonSlider` /
> `WBP_ButtonCheckbox` / `WBP_ButtonDropDown`, the plain rows → `WBP_ButtonDefault`). Repointed in
> the files-only commit, i.e. BEFORE the delete rather than after — a soft class ref that resolves
> to nothing fails silently at runtime, the same shape as the missing-ini bug that made the entire
> UI stack inert (CPP-AUDIT D1).

### 3.3 Art — 47 textures, 133 files

| Deleted | Textures | Files | Why |
|---|---|---|---|
| `Assets/Sides/` (whole folder) | 40 | 112 | 4 referenced; the rest never were |
| `ButtonBorder_*` (6 sets) | 6 | 18 | **referenced zero times** by the plan |
| `MenuRow_Tick` | 1 | 3 | duplicate of `Icons/Glyphs/T_UI_Glyph_Check_24` |

**Kept:** `MenuRow_Arrows` · `MenuRow_Dot` · `MenuRow_Hatch` · `MenuRow_Triangle` (12 files),
the four measurement `.md` docs, and `T_UI_Glyph_Check_24`.

> **Verify-before-delete, per the honesty ladder.** The 133-file number assumes a RoundedBox
> outline brush reproduces those borders faithfully. `Sides/` carries `Fade` variants and `Tab`
> shapes a plain outline may not express. **The 4 currently-referenced textures get an
> eyes-on comparison in the editor BEFORE the other 43 go.** Everything else in this ledger is
> structural and safe; this row is the one that needs a render.

### 3.4 Pipeline — 5 plan entries, ~60 lines

No scripts deleted. Inside `mcp-ui/gen_ui/wbp_plan.py`: 14 button entries → 9, with the repeated
`parent_class` / `class` / `header` / `class_defaults` boilerplate (~70 lines) collapsing into a
`button(type, body)` factory (~10 lines).

---

## 4. BUILD — 9 modular assets

> ### ❌ RETRACTED 4 Aug — the "154 → 66 nodes" reduction in v1/v2 WAS WRONG
>
> v1 claimed the chrome nodes (`BackgroundLine`, `TextFrameFill`, `Border`) could be deleted
> because "a `FSlateBrush` RoundedBox draws fill + outline + corners per state." **I never read
> what those three nodes do.** Having now read `BRButton.cpp` and `BRHairlineBorder.h`, every
> one of them is load-bearing and a RoundedBox brush can express none of it:
>
> | Node | Why it survives | Evidence |
> |---|---|---|
> | `Border` | The side lines are **0 × 20 centred TICKS on a 28-tall row**, not full-height edges. A RoundedBox outline is a continuous rectangle. | `SideTickLength = 20.0f`, `BRButton.h:236` / `BRHairlineBorder.h:69` |
> | `Border` | `SetEdgeDimmed(Bottom, !bInverted)` — the bottom line alone goes 0.3 → 1.0. That IS one of COMPONENT-SPECS §1's three inversion moves. A RoundedBox outline is **uniform on all four sides**. | `BRButton.cpp:385` |
> | `TextFrameFill` | It is the **material target**. `ApplyPlateMaterialState` fetches the dynamic material instance and eases a scalar for the measured 330 ms symmetric hover. A state brush swaps **instantly**. | `BRButton.cpp:355, 448-471` |
> | `BackgroundLine` | A **second layer** behind the plate, collapsed at idle, shown at 0.3 on hover. One state brush is one layer. | `BRButton.cpp:389` |
> | `CheckStack` | `UBRHairlineBorder : UWidget` is backed by `SLeafWidget` — **it cannot hold children**. The square and the tick must be siblings in an Overlay, and the Overlay must be sized by a SizeBox. | `BRHairlineBorder.h:88, 137` |
>
> **`UBRHairlineBorder` exists as an `SLeafWidget` precisely because Slate brushes cannot draw
> this border.** Proposing to replace it with a brush was proposing to undo the reason it was
> written. The 4,500-widgets-saved figure in `COMPONENT-BREAKDOWN.md` §4 is the same argument
> from the other direction.
>
> **Available reduction, honestly: one node per button, and it costs more than it saves.**
> `FBRHairlineStyle::FillToken` ("optional solid plate behind the strokes") means `Border` could
> absorb `TextFrameFill` — **−9 nodes across the family**. The cost is BP79's eased hover: a fill
> token is a colour, not a material, so the measured 330 ms transition becomes an instant swap.
> Trading a measured motion property for one widget per button is a bad trade. **Not recommended,
> founder's call.**
>
> **Net: the nine assets build at their current node counts. 101 nodes, not 66.**

The nine, at their real counts:

| Asset | Nodes | Asset | Nodes |
|---|---|---|---|
| `WBP_ButtonDefault` | 12 | **`WBP_ButtonCheckbox`** ★ | **11** |
| `WBP_ButtonDropDown` | 10 | `WBP_ButtonRadio` | 11 |
| `WBP_ButtonDigDown` | 10 | `WBP_ButtonMapVoting` | 15 |
| `WBP_ButtonIconOnly` | 8 | `WBP_ButtonImage` | 10 |
| `WBP_ButtonSlider` | 14 | **Total** | **101** |

`Radio` stays a separate asset on purpose: `ButtonType` lives on the CDO, so one asset = one type
— the designer drags "a checkbox," and the preview is right without a host screen. Its *plan
definition* is fully shared; only the asset is distinct.

The checkbox's real tree — 11 nodes, and every one earns its place:

```
RootSizeBox        SizeBox              C++ drives the height from ButtonType
└ RowOverlay       Overlay              child order IS z-order
  ├ BackgroundLine BRRule               hover-only halo, collapsed at idle
  ├ TextFrameFill  Image                the plate — the MATERIAL target (330 ms ease)
  ├ Border         BRHairlineBorder     4 lines, 0x20 centred side ticks, per-edge dimming
  └ TextFrame      HorizontalBox
    ├ Label        CommonTextBlock      Fill 1.0
    └ TypeBody     SizeBox 16x16
      └ CheckStack Overlay              required: the hairline is a LEAF, no children
        ├ CheckBox      BRHairlineBorder   the square outline
        └ TypeCheckMark Image              the tick, Active only
```

---

## 5. The totals

| Layer | DELETE | ARCHIVE | BUILD |
|---|---|---|---|
| Source (C++ files) | **4** | — | 2 (`BRButton.h/.cpp`, renamed) |
| Content (widget assets) | **5** | **27** | **9** |
| Widget nodes | **53** (inside the 5 deleted assets) | — | **101** |
| Art (textures) | **47** | — | — |
| Art (files) | **133** | — | — |
| Pipeline (plan entries) | **5** | — | 9 via factory |
| **Files removed from repo** | **≈ 142** | | |

**Button-related files: ~210 → ~68 active, +27 archived.**

---

## 6. Lane assignment — who can actually do each part

| Part | Lane | Can a cloud container do it? |
|---|---|---|
| C++ merge (`BRButton.h/.cpp`), delete 4 files | files-only | ✅ yes — **unverified until compiled** |
| `wbp_plan.py` factory + 9 entries | files-only | ✅ yes (`PLAN OK` is checkable here) |
| `DefaultGame.ini` repoint (6 lines) | files-only | ✅ yes |
| This ledger | files-only | ✅ done |
| Compile the merge | engine-installed | ❌ no toolchain |
| **Move 27 assets** | **editor-live** | ❌ **see below** |
| Delete 5 assets | editor-live | ❌ same reason |
| Delete 133 art files | files-only *after* the render check | ⚠️ needs eyes first |
| Build the 9 new assets | editor-live | ❌ needs the MCP + editor |

> ### Why the asset moves are NOT a filesystem operation
>
> Two independent reasons, either one sufficient:
>
> 1. **Redirectors.** Moving a `.uasset` outside the editor leaves every in-content reference
>    dangling with no redirector. The editor's Move creates them; `git mv` does not.
> 2. **I cannot see inside them.** Every `.uasset` in a cloud clone is a **Git LFS pointer stub**
>    (verified: `WBP_ButtonCheckbox.uasset` is 130 bytes of `version https://git-lfs...`). So no
>    analysis here can enumerate which Content assets reference which — the C++ and ini refs are
>    greppable, in-content refs are not.
>
> Moving them from here would look like it worked and would break references nobody could list.

---

## 7. Recommended order

1. **Founder confirms §2.1** — `Content/UI/Reference/Buttons/` (tracked), not the gitignored
   `Content/Reference/`.
2. **Files-only packet** (can run now): `BRButton.h/.cpp` merge · plan factory · ini repoint ·
   this ledger. Lands unverified, by design.
3. **Compile gate** — nothing below proceeds until the merge builds.
4. **Editor packet:** archive the 27 · delete the 5 · build the 9 · render the checkbox beside
   its archived twin.
5. **Art delete** — only after step 4's render confirms the RoundedBox outline matches.

**Step 2 is executed.** Steps 3–5 are `TICKET_BP80`.

---

## 8. What actually landed in the files-only pass

| Change | Detail |
|---|---|
| `BRButton.h` | 541 lines — 4 style classes, `EBRButtonType`, `UBRButton`, `UBRSettingsRow` |
| `BRButton.cpp` | 715 lines |
| Deleted | `BRMenuRow.h/.cpp`, `BRSettingsRow.h/.cpp`, `BRButtonStyles.h/.cpp` |
| Renames | `UBRMenuRow`→`UBRButton`, `EBRMenuRowType`→`EBRButtonType`, `RowType`→`ButtonType`, `SetRowType`→`SetButtonType`, `ApplyRowType`→`ApplyButtonType` |
| Consumers repointed | 24 files (6 with real code refs, 18 doc mentions) |
| Plan | `button()` factory; 9 entries through it; `parent_class` appears **once** in 3,200 lines |
| Plan entries deleted | 5 |
| Config | 6 soft class refs repointed |

**The header is native-only, as directed:** `CommonButtonBase.h` (CommonUI), UMG forward
declarations, and `BRComponentTokens.h` — which is a header-only constant table, data not a
class. No BR class dependency. `BRHairlineBorder.h` appears in the `.cpp` and deliberately not
in the header: the border is a bind target the button drives, never part of its public shape.

**Unverified.** Nothing here has been compiled — no toolchain in the container it was written
in. `PLAN OK` proves the plan and the header agree on every bind name and type; it proves
nothing about whether the module builds. BP80 step 1 is that gate.
