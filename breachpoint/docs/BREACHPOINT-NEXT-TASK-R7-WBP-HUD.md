# TICKET — the HUD's nine WBPs and the Tab key

**Cut:** 21 August 2026 by the cloud lead · **For:** `bn-editor` / the terminal
**Prerequisite:** a build containing R7 Waves 0–4 (the `UI/` folder — `UBNHUDLayout` must appear
in the reparent picker; if it does not, the build is stale: **stop and report**).
**Read [`ASSET-RULES`](BREACHPOINT-NEXT-ASSET-RULES.md) §5 first.**

## THE SCOPE

Create **nine** WidgetBlueprints under `/Game/BN/UI/`, add **one** input action + two rows.
**Layout, anchors and children ONLY — zero graph nodes, zero variables, zero bindings, zero
colors** (every color is C++'s; a WBP that sets one is a finding). Positions are the measured
1280×720 grid; anchors are given per element. The proven lane is `Tools/bn/6x`-style scripting;
WidgetBlueprints DO have a Python factory surface. Read-backs are the deliverable.

## Step 1 — the widgets

Every WBP is REPARENTED to its named C++ class before saving. `BindWidget` names must match
EXACTLY — a mismatch fails at asset load, not at build, and presents as an empty HUD.

| Asset | Parent class | Tree (child widget class · name · slot) |
|---|---|---|
| `WBP_BNRootLayout` | `UBNRootLayout` | Overlay root → 4 × `CommonActivatableWidgetStack`, fill-all, in z-order: `GameLayerStack`, `GameMenuLayerStack`, `MenuLayerStack`, `ModalLayerStack` |
| `WBP_BNHUD` | `UBNHUDLayout` | CanvasPanel root (NO SafeZone) → `WBP_BNVitals` anchored (0.5,0) pos (0,66) · `Image` `ReticleDot` anchored (0.5,0.5) size 6×6, white circle brush (C++ never touches it — a plain dot until per-weapon reticles) · `WBP_BNAmmoBlock` anchored (1,1) pos (−62,−36) · `WBP_BNMatchBand` anchored (0.5,1) pos (0,−76) · `WBP_BNKillfeed` anchored (0,1) pos (60,−189) · `TextBlock` `BannerText` anchored (0.5,0) pos (0,132), justified center |
| `WBP_BNVitals` | `UBNVitalsWidget` | VerticalBox → `ProgressBar` `ShieldBar` (274×12) · `ProgressBar` `HealthBar` (274×8) · HorizontalBox → `TextBlock` `ShieldText` · spacer · `TextBlock` `HealthText` |
| `WBP_BNAmmoBlock` | `UBNAmmoBlock` | VerticalBox right-aligned → `TextBlock` `WeaponNameText` · HorizontalBox → `TextBlock` `MagAmmoText` (34pt) · `TextBlock` `ReserveAmmoText` (20pt, " / N") |
| `WBP_BNMatchBand` | `UBNMatchBand` | HorizontalBox → cell(`TextBlock` label "YOU" + `TextBlock` `MyKillsText`) · divider · cell(label "TIME" + `TextBlock` `ClockText`) · divider · cell(label "LEAD" + `TextBlock` `TopKillsText` + `TextBlock` `ScoreLimitText`) |
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

## Step 4 — read back

1. Each WBP: parent class + full child tree with the exact `BindWidget` names, fresh load.
2. PIE, solo: expect `BNUI: root layout up` then `BNUI: HUD up` in `LogBN`, the vitals/ammo/
   band/killfeed on screen with live values, **no** `BNUI: … did not resolve` lines, and no
   `BNKillfeed/BNScoreboard: … placed no … rows` warnings.
3. Die to a bot: the death overlay with "Eliminated by <bot>" and a counting-down respawn line.
4. Hold Tab: the scoreboard over the HUD; release: gone. Let the match end: it pins itself with
   the winner line.

## Log

_(terminal: the read-backs, and anything handed back)_
