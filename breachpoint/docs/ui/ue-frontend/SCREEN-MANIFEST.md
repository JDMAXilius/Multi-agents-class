# Screen manifest — Figma → UE 5.8 front end

**Status:** v1, 2 Aug 2026. This is the **build document**. It maps every screen in
`REFERENCE-EXTRACTION.md` §4 to a UE asset, a parent C++ class, a grid, a component list, a
ViewModel binding, a navigation edge and a build wave.

Sources, in authority order: `COMPONENT-SPECS.md` (live-node measurements — wins on any
geometry conflict) → `SCREEN-BUILD-SPEC.md` (invariants and patterns) →
`REFERENCE-EXTRACTION.md` (inventory) → `UI-DESIGN-SYSTEM.md` (tokens) →
`ui-presentation` skill §5/§8 (grid, boundary). Binding law: `CLAUDE.md` law 7,
`DESIGN-RULINGS.md` R18/R26.

> **Where the Figma data does not say something, this document says "UNMEASURED" and names
> the node to go read.** Nothing here is invented to fill a hole. There are 14 such flags,
> collected in §8.

---

## 0. The headline numbers

| | |
|---|---|
| Figma frames inventoried | **78** (`REFERENCE-EXTRACTION.md` §4) |
| Campaign frames dropped | **5** |
| Frames that are not screens (3D camera setups) | **2** (`FE_Background`, `OP_Background`) |
| **WBP layout assets proposed** | **31** |
| **New C++ classes proposed** | **25 screens + 3 panels + 22 components = 50** |
| New ViewModels required (all are C++ gaps) | **11** |
| Components owned today (name-matched) | **14 of 22** — `ART-PASS-STAGE-3.md` §3 |
| Components missing and specified here | **8** — §6 |
| Existing WBP assets in the repo | **3** (`WBP_RootLayout`, `WBP_HUDLayout`, `WBP_KillfeedEntry`) |

**78 frames is not 78 widgets, and this is the single most valuable thing in this document.**
`SCREEN-BUILD-SPEC.md` §2 and §3 prove that most "screens" in the file are *states* of one
frame: the two-state list↔grid frame (§2), item tabs as siblings (§3.6), and filters as an
orthogonal overlay (§3.9). Building one WBP per Figma frame more than doubles the asset count,
doubles the binary-merge surface (law 7 locks one owner per `.uasset`), and produces 78 places a
palette change breaks. The collapse is recorded per-screen below with the frames it absorbs.

### Frame-count reconciliation

The enumerated rows in `REFERENCE-EXTRACTION.md` §4 total **83 distinct node ids**, not 78 — and
the File-browser heading says "8 screens" while listing 6. Best-fit hypothesis (**stated as a
hypothesis, not verified**): the 78 counts only frames authored at **1280×720**, and the five
off-size entries — `Pop-Up Options` 451×682, `Challenge Pane` 1298×546, `Warning Message` 349×60,
`Post Game XP Cards` 1320×740, `Gear Detail` 626×600 — were counted as components, not screens.
83 − 5 = 78 exactly. **Confirm before locking wave scope**; do not assume two file-browser
screens are missing.

---

## 1. The grid, resolved against the measured screens

The brief's grid is restated here **only** where a screen assignment depends on it.

```
base 1280×720            ×1.5 → 1920×1080
side margin 69           content 1143
3 columns × 349          pitch 397   origins x = 69, 466, 863
4 columns × 249.75       pitch 297.75  origins x = 69, 366.75, 664.5, 962.25
gutter 48 in BOTH        ← what makes them interchangeable
nav bar y=45 h=30        feature card 349×222     menu row h=28 pitch 40
description strip 349×37 roster panel w=349       header h=31, rows h=30 pitch 35
profile bar 1280×50 at y=670                      bottom 50 always reserved
```

### The 3-column grid is confirmed live. Here is the arithmetic.

Measured off `Play` (`1:2`) in `COMPONENT-SPECS.md` §6:

| Node | Measured x | 3-col column | Verdict |
|---|---|---|---|
| `Menu Combo` (left rail) | 69 | col 1 origin **69** | exact |
| `Player` (3D subject box) | 480, w 320 → 480–800 | col 2 spans **466–815** | subject sits **inside column 2** |
| `Party List` | 862 | col 3 origin **863** | exact (1px) |
| `Progression Button` | 869, w 334 | col 3 = 863, w 349 | +6 / −15, on col 3 |

**Column 1 is UI, column 2 is the subject, column 3 is status.** The composition law in
`ui-presentation` §1 is not a metaphor — it is the 3-column grid, and the Figma file's
`Grid - 3 Collumn` overlay is the artifact that proves it. Build to this.

### The 4-column grid is present but unconfirmed on any screen

`Grid - 4 Collumn` exists as a real 1143×570 frame at (69,38), but **no measured content matches
249.75 or pitch 297.75**. `Store Card` is 280 / 570 wide; `File Card` is 157×132. Every 4-col
assignment in §2–§4 below is therefore marked **`4-col (UNMEASURED)`** and means "author against
the 4-col overlay, then verify the card pitch against the live node before locking".

### Panel widths that are NOT on the column grid — do not force them

| Width | Where | On grid? |
|---|---|---|
| 349 · 1143 · 1280 | left rail, content, full bleed | **yes** |
| **536** | `Menu List`, `Filter & Sort Bar`, `Item Grid`, `Progress Bar` | no — its own module |
| **504** | customization item grid (4 × 114 tile @ 130 pitch, origin 86,260) | no — starts at x=86, ends 590, mid-gutter-2 |
| **451** | `Pop-Up Options` / `Filter Page` at x=48 | no — modal, deliberately off-grid |
| **626 / 586** | `Gear Detail` | no — lives in the right band (650→1236) |
| **660** | `Table Buttons` | no |
| **1180** | `Decorative Line`, `Roster Group Header`, `Team Label`, `Small Header` | no — implies a **50px** margin, not 69 |

**The 1180 family is a second margin system (50, not 69) and it is unresolved.** It appears on
list/roster/table pages; the 69 margin appears on the front-end rail pages. Two possibilities:
(a) the file has two grids and both ship, (b) the 1180 chrome is an older layer. **UNMEASURED —
read `Roster Group Header` in-screen on `RS_Friends` (`927:43283`) and decide once.** Until then,
list-page chrome is authored as a `SizeBox` of `max-w 1180` with `HAlign Center` inside the
content band (§7.1) — a second width, not a second layout mechanism — and the deviation is
recorded here rather than silently reconciled.

### Grid conflict — CLOSED 2 Aug 2026

Nav bar x was **33** in `COMPONENT-SPECS.md` §6 and **44** in `SCREEN-BUILD-SPEC.md` §1.
**Resolved: x=33.** Live node `124:1179` in `Kn87U5sy2VD0lP8K7h4LcQ` reads Navigation Bar
**x=33, y=45, 666×30**, matching `COMPONENT-SPECS` §6 exactly. **Build the nav bar at (33,45)
666×30**; 18 screens inherit it. `SCREEN-BUILD-SPEC.md` §1's `44,45` is wrong for the *root*
bar — that correction is filed in `docs/tickets/TICKET_BP67_FIGMA_NODE_PROVENANCE.md` (that file
is not this document's to edit). The **sub-level 516×30 bar's x=44 is untouched by this** and
still unverified.

---

## 2. Naming law

Extends the existing repo convention (`UBRRootLayout` → `WBP_RootLayout`) and R26 condition 5:

```
Screen      C++  UBRScreen_<Name>      WBP  WBP_Screen_<Name>
Panel       C++  UBRPanel_<Name>       WBP  WBP_Panel_<Name>
Modal       C++  UBRModal_<Name>       WBP  WBP_Modal_<Name>
Component   C++  UBR<Name>             WBP  WBP_<Name>
```

**The WBP name is mechanically derived: strip `UBR`, prefix `WBP_`.** That is what makes an
R26-style audit possible — a script can assert every WBP's parent class from its name alone.

Every screen class derives from `UBRActivatableWidget` (`Source/Breachpoint/UI/`), which already
exists and already owns `GetDesiredInputConfig()`, `NativeOnActivated/Deactivated` and the
ViewModel accessors. **There is no second widget base** (`ue5-ui-architecture` §2).

Every WBP is **layout, slots and animation only. Zero graph nodes.** R18 + R26 apply to a WBP
exactly as to a `BP_BR*` container (`ui-presentation` §8.2). A branch in a WBP graph is a `high`
finding, not a style note.

---

## 3. Layer routing

`FBRUITags` already declares four layers: `Layer.Game`, `Layer.GameMenu`, `Layer.Menu`,
`Layer.Modal`. Every screen below names the layer it pushes to. `UBRUIManagerSubsystem` already
has `PushWidgetToLayer` taking a **soft** class ptr (law 3) — no screen adds a hard reference.

| Layer | Holds |
|---|---|
| `Layer.Game` | HUD only |
| `Layer.GameMenu` | in-match pause / scoreboard |
| `Layer.Menu` | **every screen in §4** — the front-end stack |
| `Layer.Modal` | §4.9 overlays, filters, warnings, toasts |

Back = pop the `Layer.Menu` stack. CommonUI restores focus to the layer beneath; hand-rolled
back handling is a finding (`ue5-ui-architecture` §6). **This is why the drill-downs below do not
each carry a "back target" — the stack is the back target.** Only the exceptions are named.

---

## 4. The screens

> **Node-id provenance — every id in every §4 table.** These ids are ids in the **reference
> community file `Kn87U5sy2VD0lP8K7h4LcQ`** ("Halo Infinite UI Rework"), inherited verbatim from
> `REFERENCE-EXTRACTION.md` §4. They are **not** ids in Breachpoint's working file
> `yznvnVdOFDADaugZSeomfP`, where the same frames exist as clones under different ids — resolving
> `1:2` there returns *"node ID was not found"*. Verified 2 Aug 2026 on `1:2` and `124:1179`
> only; the rest are **unsampled**, and the per-id sweep is
> `docs/tickets/TICKET_BP67_FIGMA_NODE_PROVENANCE.md`. Quote the file key alongside the id
> whenever you send anyone to read one.

Column key:
- **Grid** — column layout + any off-grid module.
- **Components** — named from the Breachpoint library. Per `REFERENCE-EXTRACTION.md` §7b the 68
  cloned components were **renamed to their UE class names**, so the library name and the UE
  class name are the same string. `†` = one of the 8 missing (§6). `‡` = must be created,
  no reference component covers it (`SCREEN-BUILD-SPEC.md` §5).
- **ViewModel** — `GAP:` prefix means the field does not exist on `UBRVM_Combat` /
  `UBRVM_Match` and is a **C++ gap to file** per `ui-presentation` §8.3.

---

### 4.0 Invariants — build once, inherit everywhere

These are on **every** screen in §4 and are therefore not repeated per row. They live in
`WBP_RootLayout` / `UBRRootLayout`, not in each screen.

| Element | Base measurement | Where it lives (§7.1/§7.3) | Component |
|---|---|---|---|
| Title-safe margin | inset 0 (PC) / 5% (console) | `Screen_SafeZone`, the screen's outermost node | — |
| Background plate | 0,0 1280×720 | behind everything | **not a widget** — the arena camera |
| Profile Bar | h50 | root layout footer band, `HAlign Fill` | `UBRProfileBar` † |
| Button Prompts | h20, width hugs | root layout `CommonBoundActionBar` | `UBRButtonPrompt` |
| Scrim (when a panel is up) | full bleed | `Layer.Modal`, beneath the modal | `UBRScrim` ‡ |

`UBRProfileBar` is `#000000@0.5` + **BACKGROUND_BLUR**. Prompt-bar width tracks prompt count
(58/62 for 1, 133/146 for 2, 253 for 3) so it **must be auto-layout hug**, never fixed.

---

### 4.1 Wave 1 — Front-end spine

| Figma screen · node · page | UE asset / parent class | Grid | Components | ViewModel | Navigation |
|---|---|---|---|---|---|
| `FE_Play` `1:2`<br>`FE_Create` `1054:13588`<br>`FE_Community` `276:1764`<br>*Main Menu* | **`WBP_Screen_FrontEnd`**<br>`UBRScreen_FrontEnd` | 3-col. col1 rail (69,138), col2 subject, col3 status | `UBRNavBar` † (Menu=Main, 666×30 @33,45) · `UBRLeftRail` † (Menu Combo 349×510) · `UBRFeatureCard` (349×222) · `UBRMenuRow` ×5 (h28 pitch 40) · `UBRDescriptionStrip` (349×37) · `UBRRosterPanel` † (349×273 @862,397) · `UBRRosterRow` · `UBRProgressionButton` † (334×115 @869,55) · `UBRCarouselDots` | `GAP: UBRVM_Lobby` — party members, host, presence.<br>`GAP: UBRVM_Player` — gamertag, emblem, career rank, rank %, credits, marks.<br>`GAP: UBRVM_FrontEnd` — feature-card carousel entries (image, title, body, target), menu-row list per tab, focused-row description text | opened by `ShowMainMenu()` on boot after `FE_Splash`. Nav tabs swap the **rail data, not the widget**. Rows open `MM_*`, `OP_*`, `SH_*`, `PR_*`, `ST_*`. Feature card opens `NW_*`. Back at root = quit confirm (`OV_Warning`) |
| `FE_Splash` `266:1762`<br>`FE_Splash_Seasonal` `2409:37164` | **`WBP_Screen_Splash`**<br>`UBRScreen_Splash` | full bleed, no columns | `UBRButtonPrompt` only | `GAP: UBRVM_FrontEnd` — season key art (soft ptr), press-to-start prompt state | boot → `FE_Play`. Seasonal is the **same widget with different data**, not a second asset |
| `FE_Loading` `572:10452`<br>`CG_Loading` `1699:22861` | **`WBP_Screen_Loading`**<br>`UBRScreen_Loading` | full bleed | `UBRLoadBar` † (1280×50, State=Loading/Searching/Searching 2) · `UBRLoadingIcon` (4-frame sprite — **motion is authored in the source**, drive at constant rate) | `GAP: UBRVM_Lobby` — load phase, map name, mode name, tip text | pushed by travel; popped on `PostLogin` |
| `ST_ControlPanel` `619:4854`<br>`OP_Customize` `19:2` | **`WBP_Screen_ControlPanel`**<br>`UBRScreen_ControlPanel` | 3-col. Menu Combo (69,286) 6 rows / (69,327) 5 rows | `UBRNavBar` † (Menu=Start Menu) · `UBRLeftRail` † · `UBRMenuRow` · `UBRDescriptionStrip` | `GAP: UBRVM_Player` | **`SCREEN-BUILD-SPEC` §3.1–3.2: same chrome, the 3D subject does not re-frame.** Two Figma frames, one widget, one data set swapped. Opens `OP_Loadout`, `ST_Settings` |
| `FE_Background` `98:763`<br>`OP_Background` `621:6661` | **NOT A WIDGET** | — | — | — | These are **camera + level setups in `BR_Arena01`** (`ui-presentation` §1). Zero UMG. They belong to the level/lighting packet, not BP10. Recorded here so nobody authors an empty WBP for them |

**Wave 1 ships 4 WBPs and covers 9 Figma frames.**

---

### 4.2 Wave 2 — Match flow

| Figma screen · node | UE asset / parent class | Grid | Components | ViewModel | Navigation |
|---|---|---|---|---|---|
| `MM_Root` `2058:28286`<br>`MM_Social` `33:2`<br>`MM_Searching` `933:8346` | **`WBP_Screen_Lobby`**<br>`UBRScreen_Lobby` | 3-col. Roster panel width **349 or 310 — UNMEASURED**, see §8 | `UBRNavBar` † · `UBRLeftRail` † · `UBRMenuRow` · `UBRRosterPanel` † · `UBRRosterRow` · `UBRRosterHeader` · `UBRMicIcon` · `UBRLoadBar` † (Searching states) · `UBRTeamLabel` · `UBRCountdown` ‡ (324×152) | `GAP: UBRVM_Lobby` — members[], ready flags, host id, team assignment, playlist, selected map/mode, search elapsed, est. wait, players found/needed, countdown seconds | from `FE_Play` → Play row. Root/Social/Searching are **three states of one state machine**, not three widgets. Searching → `MM_Voting` → match |
| `MM_PlaylistSelect` `936:8543` | **`WBP_Screen_PlaylistSelect`**<br>`UBRScreen_PlaylistSelect` | 3-col | `UBRPageTitle` (1280×75, Type=Drill Down) · `UBRMenuRow` (Type=Image 250×120) · `UBRLargeModeIcon` (256) · `UBRDescriptionStrip` · `UBRScrollBar` ‡ | `GAP: UBRVM_Lobby` — playlists[] (name, icon, description, player count, locked) | drill-down from `MM_Root`. Selecting returns to `MM_Root` with the playlist set |
| `MM_Composer` `1226:13396` | **`WBP_Screen_MatchComposer`**<br>`UBRScreen_MatchComposer` | 3-col | `UBRMenuRow` (Checkbox / Radio / Drop Down types) · `UBRCheckbox` · `UBRRadio` · `UBRDropDown` · `UBRGameSettings` (349×469) | `GAP: UBRVM_Lobby` — composer axes[], selected values, resulting-playlist preview | drill-down from `MM_PlaylistSelect` |
| `MM_Voting` `2494:36113`<br>`MM_Chosen` `2506:36682` | **`WBP_Screen_MapVote`**<br>`UBRScreen_MapVote` | 3-col | `UBRMenuRow` (**Type=Map Voting**, 250×60 — its emblem art is 343-owned, needs original) · `UBRCountdown` ‡ · `UBRProgressBar` | `GAP: UBRVM_Lobby` — vote options[], per-option vote count, my vote, vote deadline, chosen index | auto-pushed on match found. Chosen is the **terminal state of the same widget** |
| `RS_Squad` `76:5392`<br>`RS_Friends` `927:43283`<br>`RS_Recents` `927:43697` | **`WBP_Screen_Roster`**<br>`UBRScreen_Roster` | list-page chrome, **1180-wide family** — see §1 | `UBRNavBar` † (Menu=Roster Menu, **516×30**) · `UBRRosterHeader` (1180×31) · `UBRRosterRow` (349 in-panel) · `UBRRule` (1180×23 / 1180×3) · `UBRMicIcon` · `UBRScrollBar` ‡ | `GAP: UBRVM_Roster` — squad[], friends[], recents[], presence, invite state, block state | opened from the Profile Bar or a shoulder button. Three nav tabs, one widget. Row → `RS_PlayerInspect` |
| `RS_PlayerInspect` `2092:29214` | **`WBP_Modal_PlayerInspect`**<br>`UBRModal_PlayerInspect` | modal, off-grid | `UBRScrim` ‡ · `UBRPageTitle` · `UBRRosterRow` · `UBRItemTile` (mini 30) · `UBRRating` · `UBRHighlightButton` | `GAP: UBRVM_Roster` — inspected player: emblem, rank, service record, equipped operator, mutual friends | `Layer.Modal`. Opened from any roster row **and** from the killfeed. Back pops |
| `RS_TextChat` `1062:12996` | **`WBP_Panel_TextChat`** (396×322)<br>`UBRPanel_TextChat` | panel, off-grid | `UBRRule` · `UBRMenuRow` | `GAP: UBRVM_Roster` — messages[] (author, team, body, timestamp), compose buffer | **Not a screen — a panel** hosted by `WBP_Screen_Lobby` and the HUD. Authoring it as a screen is the mistake this row exists to prevent |
| `CG_Lobby` `1414:15140` | **`WBP_Screen_CustomGame`**<br>`UBRScreen_CustomGame` | 3-col | `UBRLeftRail` † · `UBRGameSettings` (349×469) · `UBRRosterPanel` † · `UBRTeamLabel` (1180×32) · `UBRMenuRow` | `GAP: UBRVM_Lobby` + `GAP: UBRVM_CustomGame` — settings tree, per-team slots, spectators, start-eligibility | from `FE_Create`. Opens `CG_LobbyOptions`, `CG_Browser*` |
| `CG_LobbyOptions` `1650:21830` | **`WBP_Screen_LobbyOptions`**<br>`UBRScreen_LobbyOptions` | 3-col | `UBRMenuRow` (Slider / Checkbox / Drop Down) · `UBRSliderRow` (138×26) · `UBRPageTitle` | `GAP: UBRVM_CustomGame` — the same settings tree, editable | drill-down from `CG_Lobby` |

**Wave 2 ships 9 WBPs and covers 14 frames.**

---

### 4.3 Wave 3 — Operator customization

This wave is where the collapse pays for itself: **14 Figma frames → 5 WBPs**, because
`SCREEN-BUILD-SPEC.md` §2/§3 established that the drill-down is data, not widgets.

| Figma screen · node | UE asset / parent class | Grid | Components | ViewModel | Navigation |
|---|---|---|---|---|---|
| `OP_Loadout` (Armor Hall) `208:1603` | **`WBP_Screen_OperatorLoadout`**<br>`UBRScreen_OperatorLoadout` | 3-col chrome; **504 equipped-slot grid at (86,260)** — off-grid module, 2×4 tiles @ 114/130 | `UBRPageTitle` (**replaces the Nav Bar** at this level — §3.3) · `UBRItemTile` (114) · `UBRGroupLabel` ‡ · `UBRHighlightButton` ×2 shortcuts · `UBRScrollBar` ‡ | `GAP: UBRVM_Customization` — slots[] (id, label, equipped item, icon, locked), shortcut targets | from `OP_Customize`. Any tile → `OP_Slot` |
| `OP_Slot_Helmet` `317:2434`<br>`OP_CoreSelect` `621:6717`<br>`OP_Kits` `3598:37973`<br>`OP_WeaponBench` `572:4207` | **`WBP_Screen_OperatorSlot`**<br>`UBRScreen_OperatorSlot` | **the two-state frame (§2)**. State A: `Shade` + Menu List 536×446/542. State B: Filter Bar (86,206) 504×40 + Item Grid (86,260) 504×374 + scrollbar x=62 + Gear Detail (650,464) 586×161 + Currencies (992,47) | `UBRScrim` ‡ · `UBRMenuList` (536×446) · `UBRFilterBar` (536×53) · `UBRItemGrid` (536×388) · `UBRItemTile` (114, rarity ramp) · `UBRScrollBar` ‡ (8×374) · `UBRGearDetail` (586×161) · `UBRCurrencyRow` ‡ (216×34) · `UBRCurrency` (256×74) | `GAP: UBRVM_Customization` — slot id, list rows[], grid items[] (art soft ptr, rarity, owned, locked, favourite, price), focused item detail (name, maker, description, rarity), filter + sort state | **ONE widget, four Figma frames, two internal states.** The 3D subject **re-frames to the slot** (§3.4) — that is a camera call published by the ViewModel, not a widget call. Tile → `OP_Item` |
| `OP_Item_Appearance` `1029:12297`<br>`OP_Item_Attachments` `944:10641`<br>`OP_Item_Coating` `2415:29855` | **`WBP_Screen_OperatorItem`**<br>`UBRScreen_OperatorItem` | as above, **shifted up 40px**: Item Title (1280×105) replaces Page Title (75); a **sub-level Nav Bar appears at (44,110)** | `UBRItemTitle` (1280×105, rarity variants) · `UBRNavBar` † (sub-level 516×30) · `UBRBumperTabStrip` ‡ (284×36) · `UBRItemGrid` · `UBRItemTile` · `UBRGearDetail` | `GAP: UBRVM_Customization` — item id, tab set[], per-tab grid | **§3.6: item tabs are siblings, not children.** Same title + sub-nav; only the left column swaps. Three frames, one widget, a tab index |
| `OP_Channel_Color` `984:12045`<br>`OP_Channel_Finish` `1004:13231`<br>`OP_Channel_Pattern` `2095:28981` | **`WBP_Screen_OperatorChannel`**<br>`UBRScreen_OperatorChannel` | channel row 3 tiles at (86,202); Color Picker at (88,357); 13px tick drops from the COLOR tile | `UBRColorPicker` ‡ (373×220 — **6×6 quantised cells**, hue bar 367×10, hex row) · `UBRItemGrid` · `UBRItemTile` · `UBRGearDetail` (**shrinks 586×161 → 586×125** — materials carry no maker row, §3.8) | `GAP: UBRVM_Customization` — active channel (Color/Finish/Pattern), zone id, palette cells[], selected cell, hex string | terminal level. The three channels are **one widget with a channel enum**; the Color Picker is visible only for Color |
| `OP_GearDetail` `484:3279` | **`WBP_Panel_GearDetail`** (626×600)<br>`UBRPanel_GearDetail` | right band, 650→1236 — **one of only two nodes permitted to break the right band** (§1 of the build spec) | `UBRGearDetail` · `UBRPriceTag` (266×32) · `UBRTagFrame` (182×85) | `GAP: UBRVM_Customization` — focused item detail block | **Not a screen — a panel** instanced by the three screens above. Its own Figma frame exists only as a component art-board |

**Wave 3 ships 5 WBPs and covers 14 frames.** Building 14 would have been the default mistake.

---

### 4.4 Wave 4 — Progression

| Figma screen · node | UE asset / parent class | Grid | Components | ViewModel | Navigation |
|---|---|---|---|---|---|
| `PR_Career` `934:9674` | **`WBP_Screen_Career`**<br>`UBRScreen_Career` | 3-col; `UBRProgressionRow` is **1143 wide = the content width**, on grid | `UBRPageTitle` · `UBRProgressionRow` ‡ (1143×193, **hideable 3rd column sharing x=794 with the 2nd — column count is data-driven**) · `UBRRewardTrack` ‡ (1156 viewport, tile rail pitch 130 + chip rail pitch 260) · `UBRProgressBar` (536×152) · `UBRRankInsignia` (76, **Medal 3D effect**) | `GAP: UBRVM_Progression` — career rank, rank index (0–15), XP into rank, XP to next, track nodes[], per-node reward + claimed state, visible column count | from `FE_Play` → Career row, and from the Profile Bar. Opens `PR_CareerUnlocks` |
| `PR_CareerUnlocks` `1396:14822` | **`WBP_Screen_CareerUnlocks`**<br>`UBRScreen_CareerUnlocks` | 3-col | `UBRItemTile` · `UBRRankInsignia` (116×135 + 7× rank label — **art is 343-owned, needs original insignia**) · `UBRScrollBar` ‡ | `GAP: UBRVM_Progression` — unlocks[] per rank | drill-down from `PR_Career` |
| `PR_Challenges` `82:930`<br>`PR_ChallengesMP` `934:9948` | **`WBP_Screen_Challenges`**<br>`UBRScreen_Challenges` | 3-col | `UBRChallengeCard` (364×68 / 364×54; **on focus narrows 511.5 → 461.5 and a 40×40 swap button slides in** — §7 of the build spec) · `UBRCommendationCard` (364×68) · `UBRCountdownChip` ‡ (88×40) · `UBRScrollBar` ‡ | `GAP: UBRVM_Progression` — challenges[] (title, description, progress, target, reward, rerollable), weekly reset seconds, commendations[] | two frames, one widget, a list-source enum |
| `PR_BattlePass` `82:840` | **`WBP_Screen_BattlePass`**<br>`UBRScreen_BattlePass` | full-bleed key art + 819-wide preview panel | `UBRPreviewPanel` ‡ (819×720 render viewport with a **diagonal boolean mask**) · `UBRRewardTrack` ‡ · `UBRItemTile` · `UBRPriceTag` · `UBRCurrencyRow` ‡ | `GAP: UBRVM_Progression` — pass id, tier, XP, tiers[] (free/premium reward, claimed, locked), premium owned | from `FE_Play` and from `NW_PassPromo` |
| `PR_PostGameXP` `1860:25253`<br>`PR_PostGameXPCards` `1862:25791` (1320×740) | **`WBP_Screen_PostGameXP`**<br>`UBRScreen_PostGameXP` | full bleed. **1320×740 exceeds 1280×720 — the card layer deliberately overscans.** `Overlay` + negative slot padding, **outside** the SafeZone (§7.3); do not clip | `UBRProgressBar` · `UBRItemTile` · `UBRRule` · `UBRHighlightButton` | `GAP: UBRVM_PostGame` — XP sources[] (label, amount), total, rank before/after, medals[] · **plus the already-filed gap: no per-player stat block** (`UI-DESIGN-SYSTEM.md` §6) | auto-pushed on match end, before `PR_RankUp` if the rank changed. Back → `FE_Play` |
| `PR_RankUp` `3606:39204` | **`WBP_Screen_RankUp`**<br>`UBRScreen_RankUp` | full bleed, centred | `UBRRankInsignia` (**Medal 3D effect**) · `UBRItemTitle` · `UBRHighlightButton` | `GAP: UBRVM_Progression` — old rank, new rank, unlocks granted | conditional, after `PR_PostGameXP` |

**Wave 4 ships 6 WBPs and covers 7 frames.**

---

### 4.5 Wave 5 — Store, browsers, news

| Figma screen · node | UE asset / parent class | Grid | Components | ViewModel | Navigation |
|---|---|---|---|---|---|
| `SH_Root` `276:2013`<br>`SH_Page2` `2530:37007`<br>`SH_Operator` `306:2286`<br>`SH_Style` `1486:18704` | **`WBP_Screen_Store`**<br>`UBRScreen_Store` | **4-col (UNMEASURED)** — `Store Card` is 280/570, neither matches 249.75 | `UBRNavBar` † · `UBRStoreCard` (280×245 / **Wide 570×245**) · `UBRShopPassesCard` † (**geometry UNMEASURED — §6.8**) · `UBRPriceTag` (266×32) · `UBRCurrencyRow` ‡ · `UBRCarouselDots` · `UBRScrollBar` ‡ | `GAP: UBRVM_Store` — sections[], offers[] (art, title, price, currency, owned, expiry), page index, wallet balances | from `FE_Play` → Store row. Card → `SH_Detail`. Nav tabs page the **same widget** |
| `SH_Exchange` `3594:37799`<br>`SH_ExchangeRedux` `3652:39683` | **`WBP_Screen_StoreExchange`**<br>`UBRScreen_StoreExchange` | 4-col (UNMEASURED) | `UBRStoreCard` · `UBRCurrency` · `UBRProgressBar` · `UBRCountdownChip` ‡ | `GAP: UBRVM_Store` — exchange stock[], exchange currency, refresh seconds | tab of `SH_Root`. **Redux is a redesign of the same screen — pick one before building.** |
| `SH_BundleDetail` `861:7724`<br>`SH_ItemDetail` `868:8415` | **`WBP_Screen_StoreDetail`**<br>`UBRScreen_StoreDetail` | item-title chrome (1280×105) | `UBRItemTitle` · `UBRItemTile` · `UBRPriceTag` · `UBRPreviewPhoto` (330×158) · `UBRTagFrame` (182×85) · `UBRHighlightButton` (Type=Premium) | `GAP: UBRVM_Store` — offer detail, contents[], owned-per-item, purchase eligibility | drill-down from `SH_Root` |
| `FB_Cards` `581:4459`<br>`FB_CardsFilters` `831:7286`<br>`FB_CardsSelect` `1595:19125`<br>`FB_Table` `1428:18627`<br>`FB_TableSelect` `1595:19068`<br>`FB_TableSelectOptions` `1603:20121`<br>`CG_BrowserCards` `1421:16805`<br>`CG_BrowserTable` `755:6805` | **`WBP_Screen_Browser`**<br>`UBRScreen_Browser` | 4-col (UNMEASURED) for cards; 1180 list chrome for table | `UBRNavBar` † · `UBRFilterBar` · `UBRFileCard` (157×132, 8 variants incl. **Add New** and Selection Mode) · `UBRTableRow` † (660×28, **Type=Servers \| Files** — this is what makes one widget serve both browsers) · `UBRSmallHeader` (1180×27) · `UBRDropDown` (107×27) · `UBRScrollBar` ‡ · `UBRRating` (85×17) | `GAP: UBRVM_Browser` — source (Files\|Servers), view (Cards\|Table), entries[], filter set, sort key + direction, selection set, selection-mode flag | **8 frames → 1 widget.** The three axes (source × view × selection-mode) are data. `FB_CardsFilters` and `FB_TableSelectOptions` are **not levels** — they are `WBP_Modal_Options` over this screen (§3.9). Row/card → `FD_Detail` |
| `FD_Overview` `1548:19329`<br>`FD_Edit` `1548:19700`<br>`FD_Credits` `1562:22399`<br>`FD_BundleOverview` `1613:23277`<br>`FD_BundleEdit` `1613:23660`<br>`FD_BundleFiles` `1615:24051` | **`WBP_Screen_FileDetail`**<br>`UBRScreen_FileDetail` | item-title chrome + sub-nav | `UBRItemTitle` · `UBRNavBar` † (sub-level) · `UBRFileDetail` · `UBRPreviewPhoto` / `UBRThreePhotoPreview` (330×158) · `UBRRating` · `UBRMenuRow` · `UBRTagFrame` | `GAP: UBRVM_Browser` — file detail (title, author, description, tags, thumbnails, rating, plays), edit buffer, credits[], bundle contents[] | **6 frames → 1 widget.** File vs Bundle is a type flag; Overview/Edit/Credits/Files are sub-nav tabs — the same sibling-tab pattern as §4.3 |
| `NW_Feature` `2371:97304`<br>`NW_StoreFeature` `2377:28958`<br>`NW_Article` `2377:29265` | **`WBP_Screen_NewsArticle`**<br>`UBRScreen_NewsArticle` | full-bleed art + 3-col text block | `UBRPageTitle` · `UBRPreviewPhoto` · `UBRHighlightButton` · `UBRCarouselDots` | `GAP: UBRVM_News` — article (art, headline, body, CTA label, CTA target) | destination of the `FE_Play` feature card. Back pops to `FE_Play` |
| `NW_BattlePass` `103:815`<br>`NW_PremiumPass` `2448:34490` | **`WBP_Screen_PassPromo`**<br>`UBRScreen_PassPromo` | full-bleed key art | `UBRPreviewPanel` ‡ · `UBRShopPassesCard` † · `UBRHighlightButton` (Type=Premium) · `UBRPriceTag` | `GAP: UBRVM_Progression` + `GAP: UBRVM_Store` | split from `NW_Article` **because of the 819×720 diagonal-masked preview panel** — that one component is the reason these two are not the same widget |

**Wave 5 ships 7 WBPs and covers 21 frames.**

---

### 4.6 Wave 6 — Settings

| Figma screen · node | UE asset / parent class | Grid | Components | ViewModel | Navigation |
|---|---|---|---|---|---|
| `ST_Settings` `1031:13111` | **`WBP_Screen_Settings`**<br>`UBRScreen_Settings` | 3-col; settings list in col 1–2, preview in col 3 | `UBRNavBar` † · `UBRMenuRow` (**Slider / Checkbox / Radio / Drop Down** types — this screen exercises 4 of the 10 Type variants) · `UBRSliderRow` (138×26) · `UBRCheckbox` (16×16) · `UBRRadio` (16×16) · `UBRDropDown` · `UBRDescriptionStrip` · `UBRScrollBar` ‡ (**wide 13×N variant**) · `UBRInputMapDiagram` ‡ (591×291, **must be original art**) | `GAP: UBRVM_Settings` — categories[], settings[] (id, type, value, range, options, default, description), dirty flag, keybind map | from `ST_ControlPanel`. Back prompts on unsaved changes (`OV_Warning`) |

**One WBP.** Settings is the screen most likely to be built badly as N sub-screens; the
`Main Button` Type axis exists precisely so it does not have to be.

---

### 4.7 Wave 7 — Forge

**These screens have no node ids anywhere in the repo.** `SCREEN-BUILD-SPEC.md` §4 documents the
Forge UI in unusual geometric detail but never records the frame ids, and
`REFERENCE-EXTRACTION.md` §7b lists `Refences - Forge` as a page without enumerating its frames.
**UNMEASURED — run `get_metadata` on the `Refences - Forge` page and fill this table.** The
components below are specified well enough to author from §4 of the build spec; the *screens* are
not.

| Screen | UE asset / parent class | Components |
|---|---|---|
| Forge radial menu | `WBP_Overlay_ForgeRadial` / `UBROverlay_ForgeRadial` | `UBRRadialMenu` ‡ (366×366, hub 130×130, Exit 62×62 @152,152; centre lands on 640,360) · `UBRRadialQuadrant` ‡ (**overlapping 366×136 / 136×366 slabs, not pie slices**; z-order Bottom→Right→Left→Top) · `UBRThumbstickIndicator` ‡ (35×5 at ring-space 183,129, **one asset rotated in 90° steps**) |
| Forge colour picker | `WBP_Panel_ForgeColor` / `UBRPanel_ForgeColor` | `UBRColorPicker` ‡ (**shared with §4.3** — 6×6 grid of flat 55×32.667 cells, whole-cell selection rect, hue rail 330×10, caret 1×12 overshooting by 1px each end) |
| Forge material picker | `WBP_Panel_ForgeMaterial` / `UBRPanel_ForgeMaterial` | `UBRInPlaceExpandingTree` ‡ · `UBRBreadcrumbStepper` ‡ (`← UNSC →` — back control *and* sibling pager in one row). **Panel 336 wide, rows 28 @ pitch 29 — NOT the front end's 250/40.** Vertical centre pinned at y=360 in all three levels; animate height and y together |
| Forge object browser / properties | `WBP_Panel_ForgeObjects` / `UBRPanel_ForgeObjects` | `UBRTransformReadout` ‡ (82px right-aligned label column + 298px dark value pane) · `UBRFilterBar` · `UBRItemGrid` |

`GAP: UBRVM_Forge` for all four — object palette, categories, selected object, transform, material
tree, colour state.

---

### 4.8 Wave 0 / anytime — overlays and modals

Cheap, shared, and needed by every wave. **Build these in Wave 0 with the foundation**, not last.

| Figma screen · node | UE asset / parent class | Layer | Components | ViewModel | Navigation |
|---|---|---|---|---|---|
| `OV_Warning` (Full Page Warning) `940:10676` | `WBP_Modal_Warning` / `UBRModal_Warning` | `Layer.Modal` | `UBRScrim` ‡ · `UBRPageTitle` · `UBRHighlightButton` ×2 | none — **takes an `FBRConfirmRequest` payload**, not a ViewModel. Confirm/cancel resolve a delegate on the *caller* | pushed by any screen. Popping is the cancel path |
| `OV_PopupOptions` `2387:32475` (451×682)<br>`OV_Filters` (Gear Filters) `891:8026` | `WBP_Modal_Options` / `UBRModal_Options` | `Layer.Modal` | `UBRPopupOptions` (451×682) · `UBRMenuRow` · `UBRCheckbox` · `UBRRadio` · `UBRDropDown` | takes a row payload from the caller | **§3.9: same 451×682 footprint at x=48 over a full scrim → ONE component, two variants.** Layers over *whatever* grid is beneath; it is not a navigation level |
| `OV_ChallengePane` `375:2694` (1298×546) | `WBP_Overlay_ChallengePane` / `UBROverlay_ChallengePane` | `Layer.GameMenu` | `UBRChallengeCard` · `UBRCountdownChip` ‡ | `GAP: UBRVM_Progression` | in-match challenge peek. **1298 > 1280 — overscans; `Overlay` + negative slot padding outside the SafeZone (§7.3)** |
| `OV_Toast` (Warning Message) `707:5770` (349×60) | `WBP_Panel_Toast` / `UBRPanel_Toast` | `Layer.Modal` | `UBRRule` | takes a message payload | non-blocking. **349 wide = column 1.** Auto-dismiss on a timer, never a Tick |

---

### 4.9 Dropped

`Campaign - Missons` `36:0` · `Mission Select` `793:9083` · `Difficulty Select` `817:6977` ·
`Skulls Select` `817:7221` · `Campaign` `1659:22234` — Breachpoint has no campaign.

**One carry-over:** `Difficulty Select`'s radial-icon layout is the right shape for the
bot-difficulty picker. It is not scheduled here; it belongs to whichever ticket adds bot
difficulty to `CG_LobbyOptions`, and it can reuse `UBRRadialQuadrant` from Wave 7.

---

### 4.10 Reference coverage — which screens we can copy, and which we design

**Cut 3 Aug 2026 from the founder render of `FE_Play`.** That render settled the grid (§1) and
it is the reason §7.1 was rewritten. It is also the *only* screen anyone has looked at in
resolution, and the risk this section exists to kill is the assumption that it generalises.

**It covers one screen.** The nav/menu/status split confirmed at ~27% · subject · ~26%, bands
present (nav and the status panel's label share a header row; the profile bar is a full-width
footer). Every measurement in §1 and §6 of the layout doctrine traces to it.

**The five things it does not cover, each of which is a screen someone will otherwise invent:**

| Gap | Why the render can't answer it | Where it gets decided |
|---|---|---|
| **The screens the menu leads to.** No settings, no searching, no loading frame | The render is a menu at rest. Settings is conventionally a two-column category+options list — that is **not** our 3-column grid and we have not ruled on it | §4.6 (`ST_Settings`), §4.1 (`FE_Loading`) — **UNSOURCED**, ours to design |
| **Lobby occupancy states.** The roster on the render is a social/friends list | Our lobby is **4 Fireteam slots with empty `Invite…` states**, not a friends list. Different component, different empty state, different focus behaviour | §4.2 `MM_Root`. `UBRRosterPanel` † §6.3 must specify the empty slot or it will ship as a blank row |
| **The modal layer.** `Layer.Modal` has nothing to reference | Nothing is up in the render. Scrim opacity, modal entry, and what the action bar shows *while a modal is up* are all unobserved | §4.8, and `MOTION-INTERACTION.md` |
| **The action bar.** The render shows one verb (`Menu`) | Ours renders several, and `CommonBoundActionBar` sizes to the actions registered. The one-verb case is the case that never reveals the wrapping rule | §7.3 row 6 · `LAYOUT-DOCTRINE.md` §5 |
| **What occupies the status column.** The render puts progression there | We have progression, but the slice may not. If the status column is empty on a screen, **column 3 still exists** (§7.3) — it just carries nothing | Per-screen, §4 |

**The rule this yields:** a §4 row whose `Grid` cell says `3-col` is inheriting a grid that one
screen proved. A row that describes anything else — full bleed, 4-col, list chrome, off-grid
modules — is **unsourced**, and the first person to build it is designing, not transcribing.
Say so in the ticket rather than implying a reference exists.

---

## 5. Component dependency graph — this drives build order

Screen coverage counted across the 31 WBPs in §4. **Build top-down; a component at depth N
blocks every screen below it.**

```
TIER 0 — on every screen. Nothing ships until these do.        blocks
  UBRProfileBar                    †  ...................... 31/31
  UBRButtonPrompt                     ...................... 31/31
  UBRScrim                         ‡  ...................... 31/31 (all modal paths)
  UBRMenuRow  (Main Button, 27 variants, 250×28)  .......... 26/31
  UBRButtonBorder (the 4-line border every panel reuses) ... 26/31
  UBRScrollBar                     ‡  ...................... 14/31
  UBRRule (Decorative Line)           ...................... 12/31

TIER 1 — page chrome. Unblocks whole waves.
  UBRNavBar                        †  ...................... 18/31   ← 4 Menu variants
  UBRPageTitle (1280×75)              ...................... 15/31
  UBRItemTitle (1280×105)             ......................  6/31
  UBRDescriptionStrip (349×37)        ......................  7/31
  UBRHighlightButton (13 variants)    ...................... 11/31

TIER 2 — the front-end rail. Unblocks Waves 1–2 entirely.
  UBRLeftRail (Menu Combo 349×510) †  ......................  6/31
  UBRFeatureCard (349×222)            ......................  4/31
  UBRRosterPanel (Party List)      †  ......................  5/31
  UBRRosterRow  ──depends on──> UBRMicIcon, UBRRankInsignia   5/31
  UBRProgressionButton             †  ......................  3/31
  UBRLoadBar                       †  ......................  3/31
  UBRCarouselDots                     ......................  4/31

TIER 3 — the grid stack. Unblocks Waves 3–5 entirely.
  UBRItemTile (114 / mini 30, 4 rarities)  ................. 10/31
  UBRItemGrid (536×388) ──depends on──> UBRItemTile, UBRScrollBar
  UBRFilterBar (536×53) ──feeds──> UBRModal_Options
  UBRMenuList (536×446)               ......................  4/31
  UBRGearDetail (586×161 / 586×125)   ......................  4/31
  UBRCurrencyRow ‡ ──depends on──> UBRCurrency (256×74)  ...  6/31

TIER 4 — leaf components. Each blocks 1–3 screens; parallelisable.
  UBRStoreCard · UBRPriceTag · UBRFileCard · UBRTableRow † · UBRRating ·
  UBRDropDown · UBRCheckbox · UBRRadio · UBRSliderRow · UBRTagFrame ·
  UBRPreviewPhoto · UBRChallengeCard · UBRCommendationCard · UBRProgressBar ·
  UBRTeamLabel · UBRRosterHeader · UBRSmallHeader · UBRGameSettings ·
  UBRShopPassesCard † · UBRCountdown

TIER 5 — bespoke, no reference component (SCREEN-BUILD-SPEC §5). Expensive; schedule late.
  UBRColorPicker ‡ (Waves 3 + 7 — build ONCE, two callers)
  UBRRewardTrack ‡ (Waves 4 ×2)      UBRPreviewPanel ‡ (Wave 4 ×2)
  UBRProgressionRow ‡ (Wave 4)       UBRCountdownChip ‡ (Waves 4 + 5 + overlays)
  UBRGroupLabel ‡ (Wave 3)           UBRBumperTabStrip ‡ (Waves 3 + 7)
  UBRInputMapDiagram ‡ (Wave 6, original art)
  UBRRadialMenu / UBRRadialQuadrant / UBRThumbstickIndicator /
  UBRBreadcrumbStepper / UBRInPlaceExpandingTree / UBRTransformReadout ‡ (Wave 7)
  UBRCRTScanline ‡ (VISR/boot — ship as a gradient, NOT the 180 authored rects)
```

### What the graph says, in three lines

1. **Seven Tier-0 components unblock everything.** They are ~15% of the work and 100% of the
   dependency. Nothing else should start first.
2. **`UBRMenuRow` alone unblocks 26 of 31 screens.** Its 27-variant matrix (10 Types × 6 Statuses
   × 2 Alignments) is the highest-leverage single asset in the project. `Settings` and
   `MatchComposer` exist only because it does.
3. **`UBRColorPicker` has two callers in two different waves.** Build it once in Wave 3 and let
   Wave 7 instance it. Building it twice is the most likely duplication in this plan.

---

## 6. The 8 missing components — specified to author

From `ART-PASS-STAGE-3.md` §3. These are the **Stage 3a prerequisite**: the shipping screens
currently instance mains that live on `Refences - Main Menu - Ideal`, a *reference* page. A layer
inside an instance cannot be renamed, and editing the main corrupts the reference. **Until these
8 exist on our own pages and every instance is repointed, no nomenclature pass can run and no
screen in §4 can be authored against a component we own.**

Seven of the eight are fully measured below. The eighth is not, and says so.

### 6.1 `UBRNavBar` — Navigation Bar (blocks 18 screens)

```
COMPONENT   666 × 30   at (33, 45)            ← CONFIRMED, node 124:1179 (Kn87U5s…), 2 Aug 2026
            516 × 30   at (44, 75) or (44, 110)   sub-level variant
├ Tab ×4    138 × 26   at x = 39, 189, 339, 489 · y = 2     ← PITCH 150
│  ├ Border  RECTANGLE 138×26 · stroke #ffffff · align OUTSIDE
│  │         weight 3 when Active=True, 2 when Active=False
│  ├ Text    (13,5) 120×14 · Rajdhani SemiBold 14 · ls 15% · #ffffff · LEFT · UPPER
│  └ Icon    (113,1) 24×24 · INSTANCE_SWAP, optional
└ Bumper ×2  27 × 15   at x = 27 and x = 639 · y = 7.5

Active=False → whole tab opacity 0.6
```
**Variant axis:** `Menu = Template | Main | Start Menu | Roster Menu` (4). Tab count varies with
`Menu`; the 516-wide variants carry 3 tabs. **Icons go through INSTANCE_SWAP, never a variant per
icon** (`figma-generate-library` rule 9).

### 6.2 `UBRLeftRail` — Menu Combo (blocks 6 screens; the reason the rail stays aligned)

```
COMPONENT              349 × 510   at (69, 138) on FE_Play
├ News & Menu          349 × 418
│  ├ Feature Card      349 × 222   fill #000000@0.5     ← instance of UBRFeatureCard
│  └ Menu List         349 × 148   rows h=28, pitch 40
│     └ Rectangle 278    3 × 65 at x = −4  ← the SELECTION CARET, rail-hugging
│        authored y values encode the focus index: 54 / 78 / 98 / 138 / 180
└ Description Frame    349 × 37    at y = 473
                                   Roboto Condensed Medium Italic 14 · ls 8%
```
Alternate y origins observed: **(69,286) 6 rows** on Control Panel, **(69,327) 5 rows** on
Customize. **The rail's y is data; its x and width are not.**

Two reveal notches are subtracted from the panel border: `Rectangle 258` (top) and
`Rectangle 259` (bottom), **88×4.7 chamfers**. The open/close wipe originates from these — that
is why the panel unzips rather than fades. Author them as geometry, animate the wipe from them.

> **Do not build the `Play Menu & Description` variant at y=0/98.** `COMPONENT-SPECS.md` §6
> footnote: `Menu Combo` at (69,138) is the shipped one; the other is an earlier layer.

### 6.3 `UBRRosterPanel` — Party List (blocks 5 screens)

```
COMPONENT   349 × 273   at (862, 397) on FE_Play     ← column 3 origin is 863
├ Fill        linear gradient @ 0.5
├ Background  BOOLEAN  #000000@0.4, inset 3
├ Stroke      #ffffff@0.2 · 1px · align INSIDE       ← INSIDE, not centre
├ Header      349 × 31   inset 16   (white fill, dark text)
└ Rows        349 × 30   pitch 35   ← instances of UBRRosterRow
```
**Width conflict, unresolved:** 349 (main menu, measured) vs 310 (lobby, unmeasured —
`REFERENCE-EXTRACTION.md` conflict #3). Build **349**; expose the width as a layout property so
the lobby variant is a value change, not a second asset. The `Player Buttons` component's 390
art-board width is the standalone board, **not** the in-screen instance — same discrepancy class
as `News Button` 330 vs feature card 349.

### 6.4 `UBRProgressionButton` — Progression Button (blocks 3 screens)

```
COMPONENT   334 × 115   at (869, 55)
├ Content     fill #000000@0.5
├ Title       "CAREER RANK" · Rajdhani BOLD 16 · ls 10% · #ffffff · + DROP_SHADOW
├ Left Side   167 × 94        ├ Right Side  167 × 94
└ Switcher     72 × 10 at (130, 121)   ← instance of UBRCarouselDots
```
**Note the Bold.** This is one of the few places the Style Guide's stated Bold intent survives in
live usage (`REFERENCE-EXTRACTION.md` §7b) — it maps to `Heading/Panel`, not `Label/Button`.
The rank insignia inside carries the **Medal 3D** effect; the artwork is 343-owned and needs
original Breachpoint insignia.

### 6.5 `UBRTableRow` — Table Buttons (blocks the Browser, 8 frames)

```
COMPONENT   660 × 28
Variant axes: State × Type (Servers | Files) × Selection Mode × Selected   = 8 variants
Border: the UBRButtonBorder 4-line treatment (top 1px @1.0, bottom 1px @0.3, side ticks @0.3)
Text:   Rajdhani SemiBold 16 · ls 10% · UPPER
```
**`Type = Servers | Files` is what lets ONE `WBP_Screen_Browser` serve both the custom-game
browser and the file browser.** Losing that axis costs a whole extra screen.
**UNMEASURED:** the internal column stops (how the 660 divides into name / author / players /
rating) are not recorded anywhere. Read `CG_BrowserTable` (`755:6805`) before authoring.

### 6.6 `UBRLoadBar` — Load / Search Bar (blocks 3 screens)

```
COMPONENT   1280 × 50
Variant axis: State = Loading | Searching | Searching 2
```
`Searching` / `Searching 2` are a **looping two-state search animation** — the second state is
not an error state. Same footprint as `UBRProfileBar` (1280×50); they are the top and bottom
bands of the loading screen and must not be merged into one component.
**UNMEASURED:** internal anatomy (spinner position, text baseline, progress fill) is not in any
doc. Read `572:10452`.

### 6.7 `UBRMenuPanel` — Menu in Border (blocks 2–3 screens)

```
COMPONENT   349 × 226
```
Column-1 width, a bordered menu list without the feature card — the `UBRLeftRail` minus its news
half. **UNMEASURED beyond the outer box.** Row count, padding and whether it reuses the
`Rectangle 278` caret are unrecorded. Read it before authoring; do **not** assume it is
`UBRLeftRail` cropped, because the 226 height does not divide cleanly by the 40 row pitch
(226 / 40 = 5.65).

### 6.8 `UBRShopPassesCard` — Shop Passes Card — **NOT SPECIFIABLE FROM ANY SOURCE**

This component appears in `ART-PASS-STAGE-3.md` §3's list of 8 and **nowhere else**. It is not in
`REFERENCE-EXTRACTION.md` §5's component inventory, not in `COMPONENT-SPECS.md`, and carries no
node id anywhere in the repo.

**What is known, and it is only inference:**
- It is instanced by the store and/or pass-promo screens (`276:2013`, `2448:34490`).
- Its sibling `UBRStoreCard` is 280×245 (Default) / 570×245 (Wide), so 245 tall is the likely
  row height it must sit in.

**What must happen:** one Figma read against `Shop - Page 1` (`276:2013`) and `Premium Pass`
(`2448:34490`), locating the instance and recording its main. **Do not author it from the
inference above** — a wrong 8th component blocks the Stage 3a repoint just as effectively as a
missing one.

---

## 7. Wireframe and layout conventions — how a Figma frame becomes UMG

### 7.1 Bands and columns, not canvases — the rule, and why

> **Superseded 3 Aug 2026.** This section previously specified one `UCanvasPanel` per screen
> root, with Figma coordinates typed into anchored slots. `LAYOUT-DOCTRINE.md` §6 corrected that
> against the reference grid overlay: **the grid is structural, not a guide.** A front-end screen
> has **no `CanvasPanel` at all**. The numbers below are unchanged — only where they live is.

| Figma construct | UMG construct | Never |
|---|---|---|
| The 1280×720 screen frame | a `SafeZone` → `UVerticalBox` of **bands** (§7.1.1) | a `UCanvasPanel` at a screen root |
| The 3-column content region | one `UHorizontalBox`, three `Fill 1.0` children | three separately-anchored canvas slots |
| A gutter between columns | slot padding, **half the gutter on each neighbour** (24 + 24 = 48) | a spacer widget, or a full 48 on one side |
| A panel's designed width inside a column | `USizeBox` `MaxDesiredWidth = 348.67` + `HAlign` outward | a fixed-width column |
| A component with absolute children (`UBRItemTile`, `UBRProgressionButton`) | `UOverlay` + `USizeBox`, or a single `UCanvasPanel` **at the component root only** | absolute positioning inside a list entry |
| Figma auto-layout `HORIZONTAL` / `VERTICAL` | `UHorizontalBox` / `UVerticalBox` + slot padding | a canvas with hand-typed x offsets |
| A repeated row / tile (menu rows, roster rows, item tiles, table rows, file cards) | `UListView` / `UTileView` with an **entry WBP** implementing `IUserObjectListEntry` | N hand-placed children |
| Full-bleed stacked layers (scene → scrim → content → chrome) | `UOverlay`, one child per layer | visibility toggling on a canvas |

**`UCanvasPanel` survives in exactly two places, and neither is a front-end screen:** the HUD,
where elements genuinely are positioned against the viewport rather than against each other, and
the root of a component whose *internal* design is absolute. Nested canvases defeat auto-sizing,
make anchors meaningless below the first level, and are the reason a WBP becomes unreviewable —
which matters more here than usual, because a WBP is binary and the critic cannot diff it
(`ui-presentation` §9).

#### 7.1.1 Screen root skeleton, identical on all 31

```
UBRActivatableWidget  (screen root — pushed to a layer, owns no action bar)
└ Screen_SafeZone     SafeZone                 the outer margin, all four sides
  └ Bands_VBox        UVerticalBox             ← NO CanvasPanel on a screen
    ├ HeaderBand      Auto                     nav bar / page title / item title
    ├ ContentBand     Fill 1.0
    │ └ Columns_HBox  UHorizontalBox
    │   ├ Col1        Fill 1.0 · pad-right 24  panel inside: max-w 348.67, HAlign Left
    │   ├ Col2        Fill 1.0 · pad 24        RESERVED — the 3D subject reads through
    │   └ Col3        Fill 1.0 · pad-left 24   panel inside: max-w 348.67, HAlign Right
    └ FooterBand      Auto · height 50         (inherited — see below)
```

**The scrim, the profile bar and the button prompts are not in this tree.** Scrim is
`Layer.Modal`'s own business; the profile bar and prompts live in `WBP_RootLayout` (§5 of the
layout doctrine) — one `CommonBoundActionBar`, one profile bar, both persistent. Re-authoring the
profile bar per screen is 31 places one blur setting drifts. `FooterBand` appears above only so
the band arithmetic is legible; a screen that owns no footer simply omits it and the content band
takes the height.

**Full-bleed screens** (`FE_Splash`, `FE_Loading`, `PR_PostGameXP`, `PR_RankUp`, `NW_*`) keep the
`SafeZone` → band structure and use a **single** content band with no `Columns_HBox`. Full-bleed
means "one column", not "back to a canvas".

### 7.2 The 1280 → 1920 conversion: do it with the DPI curve, not by hand

**Author every WBP in the 1280×720 coordinate space. Type the numbers from `COMPONENT-SPECS.md`
verbatim.**

Set in `Config/DefaultEngine.ini`:

```
[/Script/Engine.UserInterfaceSettings]
UIScaleRule=ShortestSide
UIScaleCurve=(Keys=((Time=720.0,Value=1.0),(Time=1080.0,Value=1.5),(Time=1440.0,Value=2.0)))
ApplicationScale=1.0
```

The curve is exactly linear (`Scale = ShortestSide / 720`) because 1080/720 = 1.5 — the same 1.5
the design system already states. **This is the whole conversion.** No number in this manifest,
in `COMPONENT-SPECS.md`, or in a details panel is ever multiplied by hand.

The alternative — authoring at 1920 with every measurement ×1.5 — was rejected: it puts 1.5× on
every value a human types, and the first person who types 523.5 instead of 524 introduces a
half-pixel that nobody can trace back to a source document.

**Consequence to respect:** at 1280 authoring, `1 UMG unit = 1 design px`, and
`1 cqw = 12.8 px` for the HTML mockups in `ui-presentation` §7. The mockup, the Figma node and
the WBP all agree numerically. That is the point.

### 7.3 Placement rules — the whole responsive story falls out of the 3-column law

**Superseded with §7.1: these are slot rules, not anchors.** Every Figma x/y below is preserved
because it is the *source measurement* — but it is now expressed as a band, a column and a
padding, so no anchor number is ever typed.

| Element | Where it lives | Sizing | Why |
|---|---|---|---|
| **Column 1** — menu lists, feature card, description strip | `Columns_HBox` child 0, `pad-right 24` | `SizeBox` max-w 348.67, **HAlign Left** | Figma x=69 is the SafeZone margin, not an offset. The panel keeps its designed width and pins to the outer edge |
| **Column 2** — the 3D subject | `Columns_HBox` child 1, `pad 24` | `Fill 1.0`, **holds nothing** | It is the camera. The child exists to reserve 466–815 so columns 1 and 3 cannot drift inward — deleting it breaks the grid |
| **Column 3** — party list, progression button, gear detail | `Columns_HBox` child 2, `pad-left 24` | `SizeBox` max-w 348.67, **HAlign Right** | Figma x=862 + 349 = 1211 = 69 from the right edge. Aligning outward is what makes ultrawide correct with zero extra work |
| Nav bar, page title, item title | `HeaderBand`, `Auto` | `HAlign Fill` | Titles are full-bleed bands. Figma's left=33 is band padding |
| Profile bar | `WBP_RootLayout` footer, `Auto` h50 | `HAlign Fill` | Law: bottom 50 always reserved. It is the last child of a Fill layout — not `y = 670` |
| Button prompts | `WBP_RootLayout` → `CommonBoundActionBar` | **size to content** | Width tracks the actions actually registered, not a typed count |
| Modals (`451×682` at x=48) | `Layer.Modal`, own `Overlay` | `HAlign Left · VAlign Center`, pad-left 48 | Deliberately off-grid; it is an overlay, not a column |
| Forge material panel | `Overlay`, `HAlign/VAlign Center` | animate **height only** | Centring is structural, so the y=360 pin is free. The old rule said "animate height AND y together" — that was a consequence of anchoring, and it is now gone |
| Item grid (504 @ 86,260) | inside its column, `SizeBox` w504 | **do not stretch** | Off-grid module. Add columns instead if space allows |
| Deliberate overscans (`OV_ChallengePane` 1298, `PR_PostGameXPCards` 1320) | outside `Screen_SafeZone` | `Overlay` + **negative padding** | These exceed 1280 on purpose. Negative slot padding on an overlay child does what negative anchor margins did; do not clip, and do not put them inside the SafeZone |

**The rule in one sentence: column 1 aligns left, column 3 aligns right, bands stretch, the middle
is the camera.** Nothing stretches horizontally except the bands — and the columns themselves,
which is exactly how the extra ultrawide space lands on the subject instead of on the panels.

### 7.4 Colour, type and state

- **No hex in a WBP.** Every colour reads from `UBRUISettings` (already exists at
  `Source/Breachpoint/UI/BRUISettings.h`) or a `DT_UIPalette` row. Twelve widgets with typed hex
  is twelve places a rebrand breaks silently and the critic cannot diff any of them
  (`ui-presentation` §9).
- **No gameplay number in a WBP.** Law 3. A threshold that turns ammo red is a `CT_Combat` row.
- **Letter-spacing is PERCENT, not units** (`COMPONENT-SPECS.md` §0): 15% tabs, 10% buttons,
  8% flavor. At 14px, 15% ≈ 2.1px. UMG's `Letter Spacing` is in 1/1000 em, so 15% → **150**.
- **Corner radius is 0.** The only radii in the file are 5 (17 badges), 1, 3, 0.25, 0.75. Sharp
  corners are the language; a rounded panel is a defect.
- **Idle → Hover is an inversion, not a highlight.** Fill goes solid `#ffffff`, text goes
  `#000000`, the bottom line goes 0.3 → 1.0. Build it as a state, not a tint.
- **Three stroke weights, and only three:** 1px chrome (404 uses), 0.5px fine rules (323),
  2px item tiles and emphasis (177). Nav tab active is **3px OUTSIDE**.
- **Animation lives in the WBP** (Tier 4 permits it) — but it may only animate *appearance*.
  A widget animation that gates on gameplay state is the branch R18 forbids, wearing a costume.

---

## 8. Responsive and safe-zone rules

### 8.1 16:9 — the design target

1920×1080: shortest side 1080 → DPI scale 1.5 → the 1280×720 layout maps 1:1. Nothing special
happens. 2560×1440 → 2.0. 3840×2160 → 3.0 (extend the curve).

### 8.2 21:9 and wider — the centre grows, the rails do not

At 2560×1080 the shortest side is still 1080, so DPI stays **1.5** and the virtual canvas becomes
**1706×720** in design units.

The three `Fill 1.0` columns each become (1706 − 138 margins − 96 gutters) / 3 = **490.67**. The
panels inside them do not.

| Element | Behaviour | Result at 1706 wide |
|---|---|---|
| Column 1 panel | `max-w 348.67`, **HAlign Left** inside a 490.67 column | x = 69–417.67, unchanged |
| Column 3 panel | `max-w 348.67`, **HAlign Right** inside a 490.67 column | x = 1288.33–1637, right margin still 69 |
| Centre gap (the subject) | grows | 213 → **870** design px |
| Full-bleed bands (profile, title, nav) | stretch | 1706 wide |
| Item / tile grids | **add columns, do not stretch tiles** | `UTileView` wrap; tile stays 114, pitch stays 130 |
| Modals | stay 451 wide, stay at pad-left 48 | unchanged — a modal that stretches to 21:9 is unreadable |

**The columns stretch; the panels never do.** 349 and 249.75 are instrument widths, not
proportions, and the `SizeBox` is what holds them — so all 563 design px of new width lands
between the panels, on the subject, with no aspect-specific code. The 48px gutter is preserved
because it is slot padding on the columns, not a measured distance between panels.

**The one thing this breaks:** at 32:9 (3840×1080 → 2560 design px) the centre gap reaches
1862 and the character is a small figure in a lot of nothing. **UNRESOLVED — the Figma file has
no ultrawide frame and no source states an intent.** Recommendation to decide, not to assume:
clamp the *content* band to a max of 1143 design px centred, letting the background scene fill
beyond it. That keeps the 3-column relationship intact at any aspect. Do not implement it until
someone signs it off.

### 8.3 Taller than 16:9 (16:10, 4:3)

1920×1200: shortest side 1200 → scale 1.667 → virtual 1152×720. **Width shrinks below 1280.**
The 69 margins hold, but content width falls from 1143 to 1015 and column 3 collides with column
2's subject box.

**Rule:** below 1280 design px of width, the status band (column 3) collapses to a
shoulder-button-summoned overlay rather than a persistent panel. `UBRRosterPanel` is the only
Tier-2 component that needs this, and it is one visibility binding on
`GAP: UBRVM_FrontEnd.bStatusBandPersistent`.

### 8.4 Console title-safe — and the violation already in the design

Standard console title-safe is a **5% inset per edge**: at 1280×720 that is **64 horizontal,
36 vertical**.

| Element | Base position | Title-safe? |
|---|---|---|
| Side margin 69 | 69 from each edge | **OK** — clears 64 by 5px |
| Nav bar y=45 | 45 from top | **VIOLATION** — needs 36+; it clears, but the 3px outside stroke at y=42 is 6px inside the boundary. Marginal |
| Profile bar 670–720 | bottom edge = 720 | **VIOLATION — the bar's bottom edge is the screen edge.** 36px of it is outside title-safe on console |
| Button prompts y=685–705 | 15 from bottom | **VIOLATION** — 21px outside |
| Full-bleed bands | edge to edge | intentional; only their *text* must clear |

**This is a real finding, not a hypothetical.** The reference file is a PC front end and never
had to clear console title-safe.

**Fix, in exactly one place:** the `Screen_SafeZone` of §7.1.1 — but hoisted, so it is authored
once in `WBP_RootLayout` rather than 31 times. `SafeZone` already reads the platform's title-safe
ratio, which is **0 on PC and non-zero on console**, so the two-platform behaviour is the widget's
default and not a branch we write. `UBRUISettings::TitleSafeInsetPercent` remains as the override
for platforms that report nothing (law 3 — a settings row, never a WBP literal). Every screen sits
inside it and inherits the inset for free, because the bands measure themselves against whatever
box they are handed.

The cost of that inset on console: the virtual canvas becomes 1152×648, which is the §8.3 case —
so **the status-band collapse rule in §8.3 is also the console rule**. One mechanism, two
triggers. Do not build two.

---

## 9. Open questions — the 14 things this document refuses to invent

| # | Question | Blocks | How to close |
|---|---|---|---|
| 1 | ~~Nav bar x — **33** (`COMPONENT-SPECS` §6) or **44** (`SCREEN-BUILD-SPEC` §1)?~~ **RESOLVED 2 Aug 2026 → x=33** | ~~18 screens~~ unblocked — but **18 screens inherit this number**, so any that were already authored at 44 must be re-checked | **CLOSED** by live node `124:1179` in file `Kn87U5sy2VD0lP8K7h4LcQ`: Navigation Bar **x=33, y=45, 666×30**. `SCREEN-BUILD-SPEC.md` §1's `44,45` is a defect — correction filed as `TICKET_BP67_FIGMA_NODE_PROVENANCE.md`, not edited here (not this doc's owner path). Sub-level 516×30 at x=44 is a *separate* number and still unverified |
| 2 | Roster panel width in lobby — **349** or **310**? | `WBP_Screen_Lobby` | measure a lobby node; `REFERENCE-EXTRACTION` conflict #3 |
| 3 | The **1180 / 50px-margin** chrome family vs the 69px grid — two grids, or a stale layer? | all list pages | read `Roster Group Header` in-screen on `927:43283` |
| 4 | Does **any** screen use the 4-column grid? No measured content matches 249.75 | Waves 5 layout | overlay `Grid - 4 Collumn` on `276:2013` and `581:4459` |
| 5 | **`Shop Passes Card`** geometry — the 8th missing component, specified nowhere | Stage 3a, Wave 5 | §6.8 |
| 6 | `Table Buttons` internal column stops within the 660 | `WBP_Screen_Browser` | read `755:6805` |
| 7 | `Load / Search Bar` internal anatomy | `WBP_Screen_Loading` | read `572:10452` |
| 8 | `Menu in Border` 349×226 row count and padding | 2–3 screens | read the component main |
| 9 | **Forge frame node ids** — the page is named, the frames are not | all of Wave 7 | `get_metadata` on `Refences - Forge` |
| 10 | Frame count 78 vs 83 enumerated; File-browser heading says 8, lists 6 | wave scoping | §0 hypothesis needs confirming |
| 11 | `SH_Exchange` vs `SH_ExchangeRedux` — two designs of one screen; which ships? | Wave 5 | founder call |
| 12 | 32:9 behaviour — no ultrawide frame exists in the file | §8.2 | design decision, recommendation in §8.2 |
| 13 | Feature card **330** (component board) vs **349** (in-screen instance) — same discrepancy class as Player Buttons 390/349 | `UBRFeatureCard` | recommend 349 + hug; confirm on `1:2` |
| 14 | `Post Game XP Cards` 1320×740 and `Challenge Pane` 1298×546 overscan 1280×720 — deliberate bleed or authoring artifact? | 2 screens | read both frames' clip settings |

---

## 10. C++ gaps to file — the real blocker

`ui-presentation` §8.3: *"If a field does not exist on a ViewModel, that is a C++ gap — file it,
do not work around it in the widget."* Against `UBRVM_Combat` and `UBRVM_Match` as they stand
today (`Source/Breachpoint/UI/BRViewModels.h`), **not one field in §4 exists.** Both shipped
ViewModels are in-match only: vitals, ammo, grenades, grapple, clock, scores, killfeed, rocket.

**Every front-end screen in this manifest is unbindable today.** That is not a new discovery —
`UI-DESIGN-SYSTEM.md` §6 already filed *"No lobby ViewModel at all — blocks every front-end
screen."* This manifest sizes it: **it is 11 ViewModels, not one.**

| # | ViewModel | Feeds | Screens blocked | Wave it gates |
|---|---|---|---|---|
| G1 | `UBRVM_Player` | PlayerState, Steam identity, save profile | 8 | 1 |
| G2 | `UBRVM_FrontEnd` | menu definition data, news service, carousel | 4 | 1 |
| G3 | `UBRVM_Lobby` | GameState pre-match, party service, matchmaking | 8 | 1–2 |
| G4 | `UBRVM_Roster` | friends/presence service, party service | 4 | 2 |
| G5 | `UBRVM_CustomGame` | custom-game settings tree | 2 | 2 |
| G6 | `UBRVM_Customization` | inventory + equipped loadout | 5 | 3 |
| G7 | `UBRVM_Progression` | career, battle pass, challenges, commendations | 7 | 4 |
| G8 | `UBRVM_Store` | offers, wallet, ownership | 4 | 5 |
| G9 | `UBRVM_Browser` | file + server browse/search | 2 | 5 |
| G10 | `UBRVM_Settings` | settings tree, keybinds | 1 | 6 |
| G11 | `UBRVM_Forge` | Forge editor session | 4 | 7 |
| — | `UBRVM_PostGame` | **already filed** as "no per-player stat block" (`UI-DESIGN-SYSTEM.md` §6) | 1 | 4 |
| — | reticle target state | **already filed** — HUD, not front end | — | — |
| — | respawn countdown | **already filed** — HUD, not front end | — | — |

**Sequencing consequence, and it is the schedule-critical fact in this document:** G1–G3 gate
Wave 1, which gates every other wave through the shared Tier-0/1/2 components. They are three
C++ classes and they are the front end's critical path. Filing them as one ticket per ViewModel,
in G-number order, matches the wave order exactly.

**What is bindable today with no new C++:** nothing in this manifest. The entire in-match HUD is
(minus reticle colour), and it is covered by `HUD-AUDIT.md`, not here.

---

## 11. Self-check for any packet that builds from this file

Per `ui-presentation` §11, plus what this manifest adds:

- [ ] Every element traces to a named component in §5, or a new one was added to
      `UI-DESIGN-SYSTEM.md` §4 with **both** its Figma and UE names.
- [ ] Every dynamic value traces to a ViewModel getter that **exists**, or is filed against §10.
- [ ] Colours are token names read from `UBRUISettings`. Zero hex in the WBP.
- [ ] The WBP has **zero graph nodes** and declares **no new variables** (R18/R26).
- [ ] **Zero `UCanvasPanel` on a screen** (§7.1). One is permitted at a *component* root, and on
      the HUD. A canvas on a front-end screen is the defect this rule exists to catch.
- [ ] Authored in the **1280×720** space; the DPI curve does the 1920 conversion (§7.2).
- [ ] Column 3's panel `HAlign`s **Right**, column 1's **Left**, and both carry
      `max-w 348.67` (§7.3) — verified by resizing the designer preview to 21:9, not by
      reasoning about it.
- [ ] Column 2 exists as a `Fill 1.0` child and **holds nothing**. Deleting it is the other
      defect this rule exists to catch.
- [ ] Every gutter is **24 + 24 slot padding**, never a spacer widget and never 48 on one side.
- [ ] The screen was **rendered and looked at** (`ui-presentation` §7), not just described.
- [ ] Any deviation from §1's geometry is written down **here**, with a reason, in the same
      commit that makes it.
- [ ] Rung honesty (law 6): a front-end claim from PIE is rung 2. Party lists, join-in-progress
      and roster presence need rung 4 (R30) before "works" is said out loud.
