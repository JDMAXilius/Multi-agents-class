# TICKET — the HUD's TEN WBPs and the Tab key

**Cut:** 21 August 2026 by the cloud lead · **For:** `bn-editor` / the terminal
**Prerequisite:** a build containing R7 Waves 0–4 (the `UI/` folder — `UBNHUDLayout` must appear
in the reparent picker; if it does not, the build is stale: **stop and report**).
**Read [`ASSET-RULES`](BREACHPOINT-NEXT-ASSET-RULES.md) §5 first.**

## THE SCOPE

Create **TEN** WidgetBlueprints under `/Game/BN/UI/` (the nine in the table below **plus
`WBP_BNScoreRow`**, which the scoreboard's row list needs — counting nine and stopping leaves an
empty board), add **one** input action + two rows.
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
| `WBP_BNAmmoBlock` | `UBNAmmoBlock` | Canvas **280×110** — the design's ONE "Loadout Tray" unit; R7 fills its ammo third and the rest is the deferral: `TextBlock` `WeaponNameText` @ **x60 y44, 87×14** right-aligned · `TextBlock` `MagAmmoText` @ **x74 y58, 36×43** (cap 30pt) · `TextBlock` divider "\|" @ **x126 y70** · `TextBlock` `ReserveAmmoText` @ **x138 y70, 28×26** · `Image` **`WeaponIcon`** @ **x190 y56, 88×32** (the silhouette — C++ sets its brush from the row's `Icon` column and hides it when unset, so leave the brush EMPTY here). Leave the grenade (x0/x46, 40×34), equipment (x100, 40×34) and stowed rule (x190 y96) slots EMPTY — they are the named blocked elements |
| `WBP_BNMatchBand` | `UBNMatchBand` | Canvas **302×22** → mode pip @ x0 y2 18×18 · self bar @ **x24 y7, 60×8** · `TextBlock` `MyKillsText` @ **x90 y1, 34×20** · sep "\|" @ x128 y2 · `TextBlock` `ClockText` @ **x138 y1, 43×20** · sep "\|" @ x190 y2 · `TextBlock` `TopKillsText` @ **x200 y1, 34×20** · leader bar @ **x240 y7, 44×8** · mode pip @ x284 y2 18×18 · `TextBlock` `ScoreLimitText` small, under the leader cell. **Do not centre this band** — 474.67 + 302 puts its midpoint at 625.67, deliberately 14.33px left of screen centre |
| `WBP_BNKillfeed` | `UBNKillfeed` | VerticalBox `EntryContainer` → **5 ×** `WBP_BNKillfeedEntry` children |
| `WBP_BNKillfeedEntry` | `UBNKillfeedEntry` | `TextBlock` `LineText` (14pt) — one line, no color set |
| `WBP_BNScreen_Death` | `UBNScreen_Death` | Overlay → dim `Image` (fill, black 55%) · VerticalBox centered at 40% height → `TextBlock` `KilledByText` (30pt, center) · `TextBlock` `RespawnText` (17pt, center) |
| `WBP_BNScreen_Scoreboard` | `UBNScreen_Scoreboard` | Overlay → centered Border (430 wide, dark 92%) → VerticalBox → `TextBlock` `BannerText` (26pt center) · header row (labels PLAYER · KILLS · DEATHS) · VerticalBox `RowContainer` → **8 ×** `WBP_BNScoreRow` children — **note `WBP_BNScoreRow` is a TENTH asset**, parent `UBNScoreRow`, tree: HorizontalBox → `TextBlock` `NameText` (fill) · `TextBlock` `KillsText` (right) · `TextBlock` `DeathsText` (right) |

The killfeed and scoreboard rows are FIXED CHILDREN — the C++ collects them from the container
at initialize and never creates widgets. Row counts (5 / 8) are the pool sizes; the C++ logs a
one-time notice if it ever wants more.

## Step 2 — the input

1. `Content/BN/Input/IA_BNScoreboard` — InputAction, bool.
2. `IMC_BNNext` — add mapping: **Tab** → `IA_BNScoreboard`.
3. `DA_BNInput` — add row: `IA_BNScoreboard` → tag `Input.Scoreboard`.

## Step 3 — verify the ini (no edits)

`DefaultGame.ini` already names all four classes under
`[/Script/BreachpointNext.BNUIManager]` at exactly the paths above (`_C` suffixes). If an asset
landed elsewhere, MOVE THE ASSET — the ini is the contract.

## Step 4 — fill the weapon `Icon` column (R7.1)

`DT_BNWeapons` gained an **`Icon`** column (soft `UTexture2D`). Fill it per row — art already
exists: `Content/UI/HUD/HUD_Weapon_AR`, `_BR`, `_Magnum`, `_Rocket`, `_Shotgun`, `_Sniper`.
Map each BN row to its closest existing texture; an unset row simply draws no silhouette.

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
5. The weapon silhouette draws beside the ammo and CHANGES on a swap — that is Step 4's column
   working. A blank slot with everything else alive means the column read back `None` (the
   refPath trap), not a code fault.

## Log

_(terminal: the read-backs, and anything handed back)_
