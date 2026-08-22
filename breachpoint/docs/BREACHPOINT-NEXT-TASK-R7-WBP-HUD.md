# TICKET — the HUD's ELEVEN WBPs, Tab, and Esc

**Cut:** 21 August 2026 by the cloud lead · **For:** `bn-editor` / the terminal
**Prerequisite:** a build containing R7 Waves 0–4 (the `UI/` folder — `UBNHUDLayout` must appear
in the reparent picker; if it does not, the build is stale: **stop and report**).
**Read [`ASSET-RULES`](BREACHPOINT-NEXT-ASSET-RULES.md) §5 first.**

## THE SCOPE

Create **ELEVEN** WidgetBlueprints under `/Game/BN/UI/` — every row of the table below, and
note two of them are easy to miss: **`WBP_BNScoreRow`** (the scoreboard's row list is empty
without it) and **`WBP_BNScreen_Pause`**. Add **two** input actions + two mapping rows.
**Layout, anchors and children ONLY — zero graph nodes, zero variables, zero bindings, zero
colors** (every color is C++'s; a WBP that sets one is a finding). **The single carved-out
exception is `ReticleDot`'s white circle brush** — C++ never touches that image, so its brush is
layout, not styling. Positions are the measured
1280×720 grid; anchors are given per element. The proven lane is `Tools/bn/6x`-style scripting;
WidgetBlueprints DO have a Python factory surface. Read-backs are the deliverable.

## Step 1 — the widgets

Every WBP is REPARENTED to its named C++ class before saving. `BindWidget` names must match
EXACTLY — a mismatch fails at asset load, not at build, and presents as an empty HUD.

**THE GEOMETRY IS MEASURED, NOT INVENTED.** Every number below is read from the project's own
Figma (`yznvnVdOFDADaugZSeomfP`, node `6:47` — "HUD / Core"), whose wireframe page says it in
plain words: *"Every value measured from 1920×1080 capture ÷ 1.5. Build to these numbers."*
Positions are **absolute in 1280×720 canvas space** — set them on the canvas slot as Position
with the anchor named; do not eyeball, and do not re-centre anything the design deliberately
offset.

| Asset | Parent class | Tree (child class · name · MEASURED slot) |
|---|---|---|
| `WBP_BNRootLayout` | `UBNRootLayout` | Overlay root → 4 × `CommonActivatableWidgetStack`, fill-all, in z-order: `GameLayerStack`, `GameMenuLayerStack`, `MenuLayerStack`, `ModalLayerStack` |
| `WBP_BNHUD` | `UBNHUDLayout` | CanvasPanel root (NO SafeZone) → `WBP_BNVitals` @ **x503.33 y66, 273.33×34** · `Image` `ReticleDot` @ **x620 y340, 40×40** (centre lands exactly 640,360) · `WBP_BNMatchBand` @ **x474.67 y622, 302×22** · `WBP_BNKillfeed` @ **x60 y455, 340×76** · `WBP_BNAmmoBlock` @ **x940 y580, 280×110** · `TextBlock` `BannerText` @ x503 y132 w274, centre-justified |
| `WBP_BNVitals` | `UBNVitalsWidget` | Canvas 273.33×34 → `ProgressBar` `ShieldBar` @ y0 **273.33×20** (the shield reads as an ARC, not a trapezoid — thickness 16, sag 2.7; a straight bar is acceptable for the first pass, the arc is the target) · `ProgressBar` `HealthBar` @ **y20 273.33×5** (design slot y86 abs) · `Image` centre tick @ **x135.9 y20, 1.33×10** · `TextBlock` `ShieldText` / `HealthText` beneath, small |
| `WBP_BNAmmoBlock` | `UBNAmmoBlock` | Canvas **280×110** — the design's ONE "Loadout Tray" unit; R7 fills its ammo third and the rest is the deferral: `TextBlock` `WeaponNameText` @ **x60 y44, 87×14** right-aligned · `TextBlock` `MagAmmoText` @ **x74 y58, 36×43** (cap 30pt) · `TextBlock` divider "\|" @ **x126 y70** · `TextBlock` `ReserveAmmoText` @ **x138 y70, 28×26** · `Image` **`WeaponIcon`** @ **x190 y56, 88×32** (the silhouette — C++ sets its brush from the row's `Icon` column and hides it when unset, so leave the brush EMPTY here) · **R7.3, the stowed slot at the rule:** `TextBlock` **`StowedNameText`** @ **x190 y96, 88×12** right-aligned (10pt) and `Image` **`StowedIcon`** @ **x138 y92, 44×16**, brush EMPTY — C++ fills and hides both together. · **R7.4:** `TextBlock` **`GrenadeCountText`** centred INSIDE the first grenade slot (x0, 40×34 — the slot is measured, the text's own box is not: fill the slot). C++ writes the number and hides it when the mode carries no grenades. Leave the second grenade pip (x46) and the equipment slot (x100, 40×34) EMPTY — BN has ONE grenade type and no equipment system at all |
| `WBP_BNMatchBand` | `UBNMatchBand` | Canvas **302×22** → mode pip @ x0 y2 18×18 · self bar @ **x24 y7, 60×8** · `TextBlock` `MyKillsText` @ **x90 y1, 34×20** · sep "\|" @ x128 y2 · `TextBlock` `ClockText` @ **x138 y1, 43×20** · sep "\|" @ x190 y2 · `TextBlock` `TopKillsText` @ **x200 y1, 34×20** · leader bar @ **x240 y7, 44×8** · mode pip @ x284 y2 18×18 · `TextBlock` `ScoreLimitText` small, under the leader cell. **Do not centre this band** — 474.67 + 302 puts its midpoint at 625.67, deliberately 14.33px left of screen centre |
| `WBP_BNKillfeed` | `UBNKillfeed` | VerticalBox `EntryContainer` → **5 ×** `WBP_BNKillfeedEntry` children |
| `WBP_BNKillfeedEntry` | `UBNKillfeedEntry` | HorizontalBox → `TextBlock` `LineText` (14pt) · **R7.3:** `Image` **`WeaponIcon`** @ **22×8**, brush EMPTY — the killing weapon's glyph, set and COLLAPSED by C++. One line, no color set |
| `WBP_BNScreen_Death` | `UBNScreen_Death` | Overlay → dim `Image` (fill, black 55%) · VerticalBox centered at 40% height → `TextBlock` `KilledByText` (30pt, center) · **R7.3:** `TextBlock` **`WeaponText`** (15pt, center — the weapon under the name; C++ collapses it when the death has no named cause) · `TextBlock` `RespawnText` (17pt, center) |
| `WBP_BNScreen_Pause` | `UBNScreen_Pause` | **ELEVENTH asset (R7.2).** Overlay → scrim `Image` (fill, black 78%) → Border **451×682** at x60 y19 → VerticalBox: `TextBlock` "PAUSED" (24pt) · 118×2 rule · `Button` **`ResumeButton`** (h22, label RESUME) · `Button` **`LeaveButton`** (h22, label LEAVE MATCH) · spacer · `TextBlock` `WarningText` · `TextBlock` `WarningBodyText`. Plain UMG `Button`s deliberately — a `CommonButtonBase` needs a style asset and R7 ships none |
| `WBP_BNScreen_Scoreboard` | `UBNScreen_Scoreboard` | Overlay → centered Border (430 wide, dark 92%) → VerticalBox → `TextBlock` `BannerText` (26pt center) · header row (labels PLAYER · KILLS · DEATHS) · VerticalBox `RowContainer` → **8 ×** `WBP_BNScoreRow` children — **note `WBP_BNScoreRow` is a TENTH asset**, parent `UBNScoreRow`, tree: HorizontalBox → `TextBlock` `NameText` (fill) · `TextBlock` `KillsText` (right) · `TextBlock` `DeathsText` (right) |

The killfeed and scoreboard rows are FIXED CHILDREN — the C++ collects them from the container
at initialize and never creates widgets. Row counts (5 / 8) are the pool sizes; the C++ logs a
one-time notice if it ever wants more.

## Step 2 — the input

1. `Content/BN/Input/IA_BNScoreboard` — InputAction, bool.
2. `Content/BN/Input/IA_BNMenu` — InputAction, bool.
3. `IMC_BNNext` — add mappings: **Tab** → `IA_BNScoreboard`, **Escape** → `IA_BNMenu`, and
   **`Gamepad_Special_Right`** (Start/Menu) → `IA_BNMenu` as well. Two keys, one action: without
   the pad row a controller player can never OPEN the menu — the screen's own key handler closes
   it on `Gamepad_FaceButton_Right`, but nothing opens it.
4. `DA_BNInput` — add rows: `IA_BNScoreboard` → `Input.Scoreboard`, `IA_BNMenu` → `Input.Menu`.

**THE ESCAPE TRAP — read before testing:** in the editor, Escape is *Stop PIE*. Pressing it in a
PIE window kills the session before the mapping is ever consulted, which will read as "the pause
menu is broken". **Test the pause menu in Standalone** (or bind a spare key temporarily). This
costs an hour the first time it is met; it is a tooling behaviour, not a code fault.

## Step 3 — verify the ini (no edits)

`DefaultGame.ini` already names all five classes under
`[/Script/BreachpointNext.BNUIManager]` at exactly the paths above (`_C` suffixes). If an asset
landed elsewhere, MOVE THE ASSET — the ini is the contract.

## Step 4 — fill the weapon `Icon` column (R7.1)

`DT_BNWeapons` gained an **`Icon`** column (soft `UTexture2D`). Fill it per row — art already
exists: `Content/UI/HUD/HUD_Weapon_AR`, `_BR`, `_Magnum`, `_Rocket`, `_Shotgun`, `_Sniper`.
Map each BN row to its closest existing texture; an unset row simply draws no silhouette.

**R7.3 makes this column do double duty:** the same `Icon` is the killfeed's weapon glyph and the
stowed slot's silhouette, so a row filled here lights up three places at once. A **`Grenade` row**
added to this table (name it exactly `Grenade`) would also give grenade kills a name and a glyph
for free — the code already looks the cause up here and falls back to the raw name. Not required
by this ticket; recorded so it is not rediscovered.

**THE TRAP, paid for once already** (`TASK-DT-BNWEAPONS` Log): `set_rows` **silently drops**
`{"refPath": …}` objects for soft-pointer columns — all four soft refs read back `None` on that
ticket's first write. **Write plain soft-path strings.** Also: the existing `.uasset` needs a
resave now that the row struct gained a member.

## Step 5 — read back

1. Each WBP: parent class + full child tree with the exact `BindWidget` names, fresh load.
2. PIE, solo: expect `BNUI: root layout up` then `BNUI: HUD up` in `LogBN`, the vitals/ammo/
   band/killfeed on screen with live values, **no** `BNUI: … did not resolve` lines, and no
   `BNKillfeed/BNScoreboard: … placed no … rows` warnings.
3. Die to a bot: the death overlay with "Eliminated by <bot>" and a counting-down respawn line.
4. Hold Tab: the scoreboard over the HUD; release: gone. Let the match end: it pins itself with
   the winner line.
5. **Standalone** (not PIE — see the Escape trap): press Escape. The pause menu comes up over
   the HUD, focus sits on RESUME, and the "the match does not pause" warning is visible. Escape
   again closes it. Click RESUME: closes. Click LEAVE MATCH: expect exactly one
   `LeaveMatch has nowhere to go` warning in `LogBN` and NO travel — the ini path ships unset on
   purpose. **The edge worth one deliberate try:** open the menu, then let a bot kill you — the
   pause menu must VANISH and the death screen take the layer; on respawn no menu returns.
6. **R7.3 — the cause.** Kill a bot with the rifle: that feed line carries a weapon glyph.
   Kill one with a **melee**: that line carries none (Melee has no table row, by design).
   Then let a bot kill you and read the DEATH SCREEN — it names the weapon under the killer,
   and a bot that beat you down must read `Melee`, never the gun it was carrying. The server's
   own `BNGameMode: X eliminated Y with '…'` line is the arbiter: right there and wrong on
   screen is a UI fault; `None` there means the capture never happened. A glyph missing while
   the death screen still names the weapon is Step 4's `Icon` column, not the R7.3 chain.
   **The stowed slot:** the tray's lower line shows the NEXT weapon in the swap cycle and moves
   on every swap; when the next slot is the unarmed one it reads `Unarmed`, and the press after
   really does empty your hands.
7. The weapon silhouette draws beside the ammo and CHANGES on a swap — that is Step 4's column
   working. A blank slot with everything else alive means the column read back `None` (the
   refPath trap), not a code fault.

## Log

_(terminal: the read-backs, and anything handed back)_
