# HUD / Weapon Tray — measured 1:1, and the three things C++ blocks

**Read 3 Aug 2026 from the WORKING file `yznvnVdOFDADaugZSeomfP`, node `24:35`, via
`get_metadata`.** Symbol is **240 × 90**. All coordinates below are symbol-local.

| node | name | x | y | w | h |
|---|---|---|---|---|---|
| 24:36 | GRENADE Slot | 96 | 0 | 44 | 26 |
| 24:37 | `2` (grenade count) | 104 | 4 | 8 | 19 |
| 24:38 | EQUIP Slot | 148 | 0 | 44 | 26 |
| 24:39 | `1` (equipment count) | 156 | 4 | 6 | 19 |
| 24:40 | `36` (magazine) | 62 | 40 | 32 | 38 |
| 24:41 | `\|` (separator) | 104 | 48 | 5 | 26 |
| 24:42 | `108` (reserve) | 116 | 48 | 28 | 26 |
| 24:43 | **Weapon Silhouette — "ORIGINAL ART REQUIRED"** | 160 | 42 | 80 | 30 |
| 24:44 | Tray Rule | 40 | 78 | 200 | 1 |

## Mapping onto what exists

`UBRAmmoBlock` binds: `MagazineText`, `ReserveText` (both required),
`ActiveWeaponText?`, `StowedWeaponText?`, `RootSizeBox?`
`UBREquipmentTray` binds: `GrenadeCountText`, `CooldownBar` (both required)

| Figma part | Lands on |
|---|---|
| `36` | `UBRAmmoBlock::MagazineText` |
| `108` | `UBRAmmoBlock::ReserveText` |
| `2` | `UBREquipmentTray::GrenadeCountText` |
| GRENADE / EQUIP Slot rects | chrome — a `UBRHairlineBorder` each, unbound |
| `\|` separator | unbound text |

## The three blockers — none is a geometry problem

**B1 — the symbol is ONE tray; the project ships TWO widgets, and the split is not clean.**
`Tray Rule` runs x=40..240 at y=78: it spans the ammo group AND the slot group, so neither
`WBP_AmmoBlock` nor `WBP_EquipmentTray` can own it without drawing a line under the other
widget's content. This is already an open **FOUNDER DECIDE** in BP70's Log ("Figma measures ONE
Loadout Tray; the code ships EquipmentTray + AmmoBlock as siblings"). 1:1 requires ruling it.
Merging is a C++ change (one class, one BindWidget set), not a plan edit.

**B2 — the equipment count `1` (24:39) has NO C++ member.** `UBREquipmentTray` declares
`GrenadeCountText` and `CooldownBar`, and nothing else. The tray can draw the EQUIP slot's
rectangle but cannot put a number in it. Adding `EquipmentCountText` is a `Source/` edit.

**B3 — `CooldownBar` is required by C++ and DOES NOT EXIST in the symbol.** So the two
disagree in both directions: Figma has a number C++ cannot bind, and C++ requires a bar Figma
does not draw. A "1:1" tray that satisfies the BindWidget contract must invent a position for
the bar, or the WBP fails at asset load.

## The one thing that is NOT blocked

**`Weapon Silhouette` says "ORIGINAL ART REQUIRED" and the art already exists.**
`Content/UI/HUD/` holds `HUD_Weapon_{AR,BR,Magnum,Rocket,Shotgun,Sniper}.uasset`, imported
3 Aug. The layer name is stale — the requirement was met. It maps to an unbound `UImage` at
160,42 80×30, brushed per active weapon, and is the natural home for the silhouette that
`UBRAmmoBlock`'s `ActiveWeaponText`/`StowedWeaponText` currently express as words.

## Not claimed

Geometry only — no fills, strokes, type sizes or the `Tray Rule`'s stroke token were read.
Nothing here has been rendered.
