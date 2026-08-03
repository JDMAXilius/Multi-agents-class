# Widget layout doctrine — panels, grids, dynamic art, and CommonUI

**Status:** v1, 3 Aug 2026. Fills three gaps the existing `ue-frontend/` docs leave open:
which **panel** to use (grids in particular), how **dynamic art** binds to our data, and the
**CommonUI** rules that decide widget class and input.

Everything here is expressed in `Tools/gen_ui/wbp_plan.py`'s vocabulary — that file is the
committed plan R37.1 requires and the MCP executes it. **This doctrine is what a plan author
follows; it invents no new format.**

Binding law: `CLAUDE.md` 3/4/5/7, `R18`/`R26`. Geometry: `UI-DESIGN-SYSTEM.md` §3.
Mechanics: `ue5-ui-architecture` skill. Assets: `ASSET-PIPELINE.md`.

---

## 1. Panel selection — the whole decision, in one table

**There is no "grid overlay" widget in UMG.** The Figma concept of a layout grid is a *guide*,
not a runtime object; in UE it becomes **anchors + fixed offsets**, which is why our 3-column
law survives resolution changes without any grid widget existing. Reach for a grid panel only
when content is genuinely tabular.

| You have | Use | Why, and the trap |
|---|---|---|
| A stack with a rhythm (menu rows, roster rows) | **`VerticalBox`** / **`HorizontalBox`** | Slot `Padding` + `Size: Fill/Auto` reproduces Figma auto-layout exactly. **Pitch is padding, never a spacer widget** — a spacer is a widget nobody can find later |
| Things layered on each other | **`Overlay`** | Z-order = child order. Slot alignment centres without constraining — see §2 |
| A few regions pinned to screen edges | **`CanvasPanel`** | The ONLY panel that needs anchors. Use once per screen, at the top, and never nested |
| A fixed measured size | **`SizeBox`** | `WidthOverride`/`HeightOverride`. Our measured numbers land here |
| **Equal-sized cells** — loadout grid, emblem picker, medal wall | **`UniformGridPanel`** | Every cell identical. `SlotPadding` once, not per cell |
| **Unequal columns that must align across rows** — scoreboard | **`GridPanel`** + `ColumnFill`/`RowFill` | This is the carnage report. `ColumnFill` is a *ratio* array; leave a column out and it collapses to auto |
| Items that reflow at width — icon trays | **`WrapBox`** | `InnerSlotPadding` is the gap. Do not hand-wrap |
| Content longer than its box | **`CommonHierarchicalScrollBox`** | CommonUI's version; plain `ScrollBox` breaks gamepad focus scrolling |
| A whole screen that must letterbox | **`ScaleBox`** | `ScaleBoxStretch`. Rare — the DPI curve handles our scaling (`SCREEN-MANIFEST.md` §7.2) |
| Swap between mutually exclusive panels | **`CommonVisibilitySwitcher`** | Beats toggling `Visibility` on siblings by hand; keeps focus sane |

**The scalability rule, stated once:** *one* `CanvasPanel` at the root of a screen holds the
anchored regions; **everything inside a region is a Box.** Canvas children need anchors and
drift on aspect change; Box children inherit. A screen with a nested Canvas is the defect —
it is how a layout that looks right at 1080p breaks at 21:9.

---

## 2. Slot rules that are not obvious

- **`Overlay` slot alignment `Center` does not constrain the child.** That is why the reticle
  sits in one — `UBRReticleWidget::ApplyArt` calls `SetDesiredSizeOverride` and the *size is
  the spread readout*. A constraining slot would silently delete the only accuracy cue on
  screen. (`wbp_plan.py` already names this `CENTER`.)
- **`Fill` needs a parent that can distribute.** `layoutSizing` equivalents apply after
  parenting, never before — append first, then set.
- **Canvas anchors: set the anchor to the edge you are measuring from.** Left-rail items anchor
  top-left with positive offsets; the status column anchors top-**right** with negative X. That
  is what makes the rails hold and the centre grow at 21:9 (`SCREEN-MANIFEST.md` §8.2).
- **Profile bar anchors bottom-stretch** (min 0,1 → max 1,1), height 50. Never a fixed Y of 670
  — 670 is a 720-space number and the bar must sit on the bottom edge at any height.

---

## 3. Dynamic art — the weapon icon, and the rule behind it

**The question:** the HUD, the death screen and the carnage report all need "the icon for the
weapon this row is about", and there are three weapons today and more later.

**The answer is `UCommonLazyImage`, driven by a soft path in `DT_Weapons`.** CommonUI ships this
widget exactly for this: it takes a `TSoftObjectPtr<UTexture2D>`, async-loads it, and shows a
loading state meanwhile — so a soft reference never stalls the frame and never hard-links the
texture into everything that includes the row.

### 3.1 The data change — one column, one field

`DT_Weapons.csv` carries `MeshSoftPath` (the world mesh) and **no icon column**.
`FBRWeaponRow::IconSoftPath` is referenced by `BRScreen_DeathRespawn` today and **does not
exist** — the screen holds a collapsed frame with a comment naming it as the one visibility to
flip. That is **BP25**, still open.

```
DT_Weapons.csv   + IconSoftPath        /Game/UI/Icons/Weapons/T_UI_Weapon_AR
                 + SilhouetteSoftPath  /Game/UI/Art/Weapons/T_UI_Sil_AR   (bottom-right tray)

FBRWeaponRow     + UPROPERTY(EditDefaultsOnly) TSoftObjectPtr<UTexture2D> IconSoftPath;
                 + UPROPERTY(EditDefaultsOnly) TSoftObjectPtr<UTexture2D> SilhouetteSoftPath;
```

**Two paths, not one.** The tray silhouette is a wide, side-on 94×30.67 rendered from the mesh
(`WEAPON-RENDER-PLAN.md`); a killfeed or scoreboard icon is a small square glyph. One image
cannot be both, and forcing it produces a stretched silhouette in a 24px slot.

### 3.2 How a widget gets it — the flow, and where the rule lives

```
DT_Weapons row  →  UBREquipmentComponent (already resolves the row)
                →  UBRVM_Combat exposes the soft path  ← the ViewModel field is the seam
                →  CommonLazyImage.SetBrushFromLazyTexture(SoftPath)
```

**The widget never reads the DataTable.** It reads a ViewModel field, exactly like ammo and
weapon name do today. A widget that opens `DT_Weapons` is a widget that has to know about
row handles, load order and the equipment component — that is the coupling MVVM exists to
prevent.

**Availability, not just selection.** The user asked for "the selected weapon and all the
weapons available." Selected = `GetActiveWeapon*`. The full set is a **`UniformGridPanel` of
`WBP_WeaponSlot`**, one entry per row, each with its own `CommonLazyImage` — which is why the
grid row of §1 exists.

### 3.3 The default-asset rule

Every dynamic slot needs a **default** that shows the system is working before the real art
lands: `T_UI_Weapon_Unknown`, a neutral silhouette at the same dimensions. Set it as the
`CommonLazyImage`'s brush in the plan, so an unresolved or missing path renders a visible
placeholder rather than a blank hole. **An empty slot and a broken binding look identical;**
a placeholder distinguishes them at a glance.

---

## 4. CommonUI — the rules that decide class and input

Researched against Epic's guidelines and the community's hard-won ones. `ue5-ui-architecture`
owns the mechanics; these are the **decisions**.

### 4.1 Which widget class

| Need | Class | Reason |
|---|---|---|
| A screen that takes over input, can be pushed/popped, handles Back | `UCommonActivatableWidget` | Only this participates in the stack and can seize input |
| A reusable piece inside a screen — a row, a card, a bar | **`UCommonUserWidget`** | **Default choice.** Activatable is heavier and brings input semantics you do not want on a row |
| A tooltip, a decoration, a passive readout | `UCommonUserWidget` or plain `UUserWidget` | **Never activatable.** Epic names tooltips explicitly — they neither forward input nor need to seize it |

**The pitfall this prevents:** making every component activatable. Each one becomes an input
routing node, and the stack starts fighting itself over who owns focus.

### 4.2 Input, focus and gamepad

- **`GetDesiredFocusTarget()` is not optional on any activatable screen.** Return the widget
  the gamepad should land on. Without it, focus is undefined the moment a controller is picked
  up — and it will be undefined *only* on controller, which is how this ships broken.
- **`GetDesiredInputConfig()`** declares Menu / Game / GameAndMenu and mouse capture per screen.
  `UBRActivatableWidget` already overrides it — set it per screen, do not fight it globally.
- **Gamepad runs on a synthetic cursor.** If a thing works with a mouse and is focusable,
  gamepad generally follows. Non-focusable decoration should be explicitly non-focusable so it
  cannot swallow navigation.
- **Accept vs simulated click:** a widget listening for a bound Accept action and a button
  listening for a click can consume each other. Pick one per interactive element — for us,
  **`CommonButtonBase` handles the click; bound actions are for screen-level verbs** (Start
  Match, Back, Invite).
- **`CommonBoundActionBar`** renders the prompt row from the actions actually registered. Our
  `UBRButtonPrompt` inventory should feed it rather than hand-placing glyphs per screen —
  hand-placed prompts drift out of sync with the real bindings, silently.
- **A stack needs a root content class.** An empty `CommonActivatableWidgetStack` leaves the
  input config wherever the last screen left it. A tiny always-present stub activatable that
  requests the Game input config is the fix.

### 4.3 Styling — one place, three assets

`CommonTextStyle`, `CommonButtonStyle`, `CommonBorderStyle` in `Content/UI/Styles/`
(`ASSET-PIPELINE.md` §7). A WBP **references** a style; it never sets a colour, font or brush
locally. The idle→hover inversion is authored once and every menu row inherits it.

`CommonTextBlock` takes a style asset directly — which is why `wbp_plan.py` uses
`/Script/CommonUI.CommonTextBlock` rather than `UMG.TextBlock` for every label.

---

## 5. The main menu tree — the concrete answer

Measured numbers from `UI-DESIGN-SYSTEM.md` §3, at the 1280×720 base. **One Canvas, three
regions, Boxes inside.**

```
WBP_Screen_MainMenu                       : UBRScreen_MainMenu (UBRActivatableWidget)
│
└── Root_Overlay                          Overlay              full-bleed
    │
    ├── [0]  (nothing — the 3D scene in BR_Arena01 shows through; §1 of ui-presentation)
    │
    ├── [1] Content_Canvas                CanvasPanel          the ONLY canvas on this screen
    │   │
    │   ├── NavBar            anchor TL   pos (69, 45)    size (666, 30)
    │   │   └── WBP_NavBar                UBRNavBar        bumper · tabs · bumper
    │   │
    │   ├── LeftRail_VBox     anchor TL   pos (69, 138)   size (349, 510)
    │   │   ├── WBP_FeatureCard           349 × 222        pad-bottom 10
    │   │   ├── CarouselDots_HBox         349 × 8          pad-bottom 12
    │   │   ├── MenuList_VBox             349 × auto
    │   │   │   ├── WBP_MenuRow "HOST MATCH"      h 28  pad-bottom 12   ← pitch 40
    │   │   │   ├── WBP_MenuRow "JOIN BY INVITE"  h 28  pad-bottom 12
    │   │   │   ├── WBP_MenuRow "TRAINING"        h 28  pad-bottom 12
    │   │   │   └── WBP_MenuRow "QUIT"            h 28
    │   │   └── WBP_DescriptionStrip      349 × 37         pad-top 12
    │   │
    │   ├── Status_VBox       anchor TR   pos (-69, 45)   size (349, auto)
    │   │   ├── WBP_RecordPanel           334 × 115        pad-bottom 24
    │   │   └── WBP_RosterPanel           349 × 273
    │   │       ├── RosterHeader          h 31, inset 16
    │   │       └── RosterRows_VBox       rows h 30, pad-bottom 5   ← pitch 35
    │   │
    │   └── WBP_ProfileBar    anchor BOTTOM-STRETCH (0,1)→(1,1)   h 50
    │
    └── [2] CommonBoundActionBar          anchor BL        prompts, from real bindings
```

**Why it is shaped this way:**

- **Overlay at the root**, not Canvas — so the scene, the content and the prompt bar are three
  independent layers with no positioning relationship. Adding a modal dim later is one more
  Overlay child, not a re-layout.
- **One Canvas, three anchored regions.** Left rail anchors top-**left**, status column top-
  **right** with negative X, profile bar bottom-**stretch**. At 21:9 the rails hold their margin
  and the centre — where the character stands — grows. That behaviour falls out of the anchors;
  no code, no grid widget.
- **Every region is a VerticalBox**, so pitch is slot padding and the whole rail reflows if a
  row is added. Menu pitch 40 = h28 + pad12; roster pitch 35 = h30 + pad5.
- **Rows are `WBP_MenuRow`, not inline text.** Four instances of one component, so the hover
  inversion is authored once.

**The pause menu is the same tree minus the feature card and status column** — a
`CommonActivatableWidget` pushed onto the Menu layer over the running game, with its own
`GetDesiredInputConfig` returning Menu + mouse capture. It reuses `WBP_MenuRow`,
`WBP_DescriptionStrip`, `WBP_NavBar` and the action bar unchanged. That reuse is the point of
building components before screens.

---

## 6. What the MCP receives

Nothing new. A plan author writes this tree into `Tools/gen_ui/wbp_plan.py` using the existing
node vocabulary — class path, name, `bind`, slot dict, `font`, `brush` — and `build_wbp.py`
executes it against the editor MCP.

**The two properties of that file that matter more than its format:**

1. **`bind: True` is validated against the real C++ header at plan time.** A `BindWidget` desync
   otherwise fails at *asset load*, not at build — rung 1 stays green and the HUD is just empty
   in PIE. Plan-time checking makes it a text error nobody can miss.
2. **Tool names are never guessed.** Five rounds were lost to guessed MCP names
   (`create_expression` → `add_expression`, `connect_property` → `connect_to_output`,
   `folderPath` → `folder_path`, a required `material_function` that only accepts null). **Probe
   the schema, then write the call.** The receipts in `docs/ui/receipts/` are the record of what
   the tools actually are.
