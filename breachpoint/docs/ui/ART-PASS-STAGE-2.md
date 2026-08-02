# Art pass — Stage 2: the ledger of visible Halo-owned art

Figma file `yznvnVdOFDADaugZSeomfP`, the twelve `FE / …` pages, hidden subtrees excluded
(stage 1 already removed the hidden underlays; anything still hidden does not ship and is not
counted here).

**Headline: 815 visible image fills across 66 distinct sizes, and 31 Halo-owned strings.**

`SCREEN-BUILD-SPEC.md` §6 estimated ~250 nodes. It was low by 3×, and it was wrong about
three specific assets — see §4. §6 is superseded by this file.

---

## 1. Where the work is, by page

| Page | Image fills | Halo strings |
|---|---:|---:|
| Store | 143 | 2 |
| Operator | 101 | 6 |
| Matchmaking | 95 | 1 |
| File Browser | 91 | 13 |
| Custom Games | 83 | 3 |
| Progression | 75 | 0 |
| Roster & Social | 73 | 1 |
| Main Menu | 59 | 2 |
| Campaign | 41 | 2 |
| News | 28 | 1 |
| Settings | 20 | 0 |
| Boot & Loading | 6 | 0 |
| **Total** | **815** | **31** |

Store and Operator are the two heaviest pages and they are heavy for the same reason: both are
walls of item tiles. Settings and Boot are nearly free.

---

## 2. The families — 815 nodes, all of them

Grouped by **production method**, because that is the axis that decides who does the work and
when. Tier column is the ladder in `ASSET-METHODS.md`. Counts are exact and sum to 815.

| # | Family | Sizes (count) | n | Tier | Method |
|---|---|---|---:|:---:|---|
| **PC** | Player-card art | 40×40 (72), 349×50 (70), 273×46 (4) | **146** | 3 | Nameplate is a gradient + token, not a picture. Emblem is family EM. |
| **EM** | Emblems | 26×26 (127), 24×24 (7) | **134** | 1 / 3 | 9 drawings × 6 palettes = 54 emblems. Parametric, then recoloured. |
| **NP** | Roster nameplates | 313×26 (66), 306×26 (25), 307×26 (8), 345×26 (7), 250×26 (3), 272×26 (1) | **110** | 3 | Same asset at six widths — one 9-slice, not six images. |
| **MAP** | Map & mode preview photos | 208×113 (30), 266×168 (18), 343×191 (9), 195×113 (9), 432×240 (9), 260×147 (8), 414×237 (7), 454×237 (6), 342×190 (4), 209×113 (3), 335×186 (2), 345×191 (2), 484×231 (2), 260×145 (1) | **110** | 4 | In-engine captures of our own maps. Fixed camera rig, one per map per aspect. |
| **IT** | Item tiles | 100×100 (101), 28×28 (4), 183×183 (4) | **109** | 4 | Rendered from the real meshes. See `WEAPON-RENDER-PLAN.md`. |
| **BG** | Scene plates / backgrounds | 1280×720 (65), 980×720 (3), 772×644 (2), 1000×670 (2), 819×720 (1), 798×670 (1), 753×720 (1) | **75** | 4 | A camera in `BR_Arena01`, not an image. `ui-presentation` §1. |
| **FG** | Forge materials & swatches | 225×225 (26), 180×180 (18), 225×200 (4), 186×171 (4), 225×226 (2) | **54** | 3 / 4 | Material sphere renders; the ramp variants are recolours. |
| **SH** | Store & bundle art | 266×423 (6), 266×146 (6), 436×245 (4), 386×300 (4), 448×251 (2), 250×250 (2), 266×231 (2), 224×457 (2), 600×600 (2), 266×104 (2), 281×281 (1), 172×80 (1) | **34** | 4 / 5 | Composed from IT renders over a BG plate. Genuine Tier 5 candidates live here and nowhere else. |
| **IC** | Manufacturer / core / category glyphs | 34×34 (8), 23×20 (5), 31×31 (2), 32×34 (2), 23×23 (2) | **19** | 1 / 2 | Small, geometric, and ours to invent. |
| **PG** | Progression badges | 80×80 (9), 160×160 (1) | **10** | 1 | Season badge + level mark. Parametric. |
| **BR** | Faction mark (`UNSC LOGO`) | 140×140 (6), 280×280 (1) | **7** | 1 | The Breachpoint mark, drawn once, placed seven times. |
| **BT** | Boot logotype | 304×118 (2), 152×59 (1) | **3** | 1 | The wordmark. |
| **CH** | Weapon charms | 98×72 (2), 142×104 (1) | **3** | 4 | Mesh renders. |
| **CT** | Controller diagram (`Controls`) | 591×291 (1) | **1** | 1 | One drawing, `ST_Settings` only. |
| | | | **815** | | |

**The shape of the job:** four families — PC, EM, NP, MAP — are **500 of the 815**, and every
one of them is Tier 1 or Tier 3. They are *repetition*, not *volume*: 110 nameplates are one
9-slice, 134 emblems are nine drawings and a palette, 146 player cards are a gradient and a
token. Doing them as 500 assets would be the single most expensive mistake available in this
pass.

The genuinely expensive families are **IT (109)** and **MAP (110)**, and both are Tier 4 — the
render rig pays for both at once, and the rig is already specified.

---

## 3. Distinct assets, not node instances

The number that matters for scheduling is not 815.

| Family | Nodes | Distinct assets to author | Multiplier |
|---|---:|---:|---|
| PC | 146 | 2 (nameplate slice + card frame) | token + emblem |
| EM | 134 | 9 drawings | × 6 palettes |
| NP | 110 | 1 nine-slice | 6 widths |
| MAP | 110 | ~14 maps × 2 aspects | camera rig |
| IT | 109 | ~109 | one render each, no multiplier |
| BG | 75 | ~8 camera setups | reused per page |
| FG | 54 | ~10 materials | × ramp |
| SH | 34 | ~12 compositions | built from IT + BG |
| IC | 19 | ~12 glyphs | |
| PG | 10 | 2 | |
| BR / BT / CH / CT | 14 | 5 | |
| **Total** | **815** | **≈ 192** | |

**815 nodes, ~192 assets.** That is the real stage-4 scope, and roughly 110 of the 192 are a
single scripted render pass over meshes we already own.

---

## 4. Where §6 was wrong

Stage 2 exists to replace a sketch with a count. Three of §6's named targets do not survive
contact:

| §6 claim | Reality | Consequence |
|---|---|---|
| "~180 `Items` 114×114" | 285 nodes named `Items`, **277 of them carry no paint at all** — they are tile *containers*. The art is the 100×100 child inside. 75 are hidden. | Real count is **101 visible tiles**, not 180. The 114×114 box is layout and needs no art. |
| "`Rank Image` 116×135 + 7× `Rank Label`" | `Rank Image` exists **once**, on `PR_CareerUnlocks`, and is a **SOLID fill** — a grey placeholder, not Halo art. The 8 `Rank Label` nodes carry no paint; they are text frames. | **Nothing to replace.** This is a gap to fill in stage 4, not infringement to remove. Move it off the art-pass list. |
| "5 `Commendation Card` icons" | 7 `Commendation*` nodes, **none with an image fill**. | Same — containers. No art present. |

Also worth recording: **32 nodes are named `Heroes of Reach` and not one of them carries an
image fill.** They are frames and text. That entire cluster is a **stage 3 nomenclature**
problem, not a stage 4 art problem, and counting it as art would have inflated the estimate
with work that is a find-and-replace.

Confirmed exactly as §6 described: `UNSC LOGO` (7 real image fills), `Controls` 591×291 (1),
`Start Menu Background` (28 named, all image-filled, plus 37 further 1280×720 plates under
other names).

---

## 5. The 31 Halo-owned strings — handoff to stage 3

Small enough to list in full. These are **strings**, so they are stage 3's problem, not stage
4's — no art is produced to fix any of them.

| Term | n | Screens |
|---|---:|---|
| `ODST` | 9 | `CG_Lobby`, `CG_Browser_Table`, `CG_Browser_Cards`, `FD_Overview`, `FD_Edit`, `FD_Credits`, `OP_Item_Attachments`, `OP_Item_Coating`, `OP_Item_Appearance` |
| `Halo` | 9 | `FB_Cards`, `FB_Cards_Filters`, `FB_Cards_Select`, `FB_Table`, `FB_Table_Select`, `FB_Table_SelectOptions`, `FBundle_Files`, `NW_Article`, `CP_DifficultySelect` |
| `Covenant` | 3 | `FD_Overview`, `FD_Edit`, `FD_Credits` |
| `Firefight` | 2 | `FE_Play`, `MM_Root` |
| `Anubis` | 2 | `SH_Page1`, `SH_Page2` |
| `Zeta Halo` | 1 | `FE_Play` |
| `Xbox` | 1 | `RS_PlayerInspect` |
| `Spartan` | 1 | `OP_Loadout` |
| `MA40` | 1 | `OP_WeaponsBench` |
| `BR75` | 1 | `OP_WeaponsBench` |
| `Banished` | 1 | `CP_MissionSelect` |

The File Browser is the worst offender (13 of 31) and for one reason: a single map-description
string — *"Salvation is the Damnation remake for Halo Infinite…"* — is duplicated across seven
screens. **One rewrite clears seven nodes.**

Not caught by term matching, and to be swept in stage 3 by layer-name pass rather than
content pass: the ~32 `Heroes of Reach` frames above, plus layer names like
`Superintendent`, `Falcon`, `Rumble Pit`, `Live Fire`, `Catalyst`, `Foundry`,
`Lethbridge Industrail` *(sic — the reference file's own typo)*, `Sevine Arms`,
`Last Spartan Standing`, `Echoes Within`, `LOCUS Set`, `Transgressor Set`, `Warmaster Set`.

---

## 6. What this stage did not do

No node was modified. This is a read-only survey and the file is byte-identical to its
post-stage-1 state.

It also does not decide **art direction** for any family — it says a 26×26 emblem must be
replaced and that nine drawings cover 134 nodes, not what the nine drawings depict.
`ART-PROMPT-LIBRARY.md` owns that.

---

## 7. Recommended stage-4 order

Driven by the §3 multipliers — highest node-count-per-asset first, so the file stops looking
like Halo as early as possible for the least authoring:

1. **NP + PC (256 nodes, 3 assets).** One nine-slice and a gradient token. Largest single
   drop in Halo-owned pixels available, and it is a day.
2. **EM (134 nodes, 9 drawings).** Tier 1 parametric, then the palette multiplier.
3. **BG (75 nodes, ~8 cameras).** Needs `BR_Arena01` dressed enough to photograph; gate this
   on the level, not on the art.
4. **IT + CH (112 nodes).** The render rig. Blocked on running the spike in a live editor —
   R21/R29 means this cannot happen in a headless session.
5. **MAP (110 nodes).** Same rig, different subject; do it in the same editor session as 4.
6. **FG, SH, IC, PG, BR, BT, CT (139 nodes, ~44 assets).** The tail. `BR` and `BT` need the
   brand mark settled first, so they may in practice come earlier.

Steps 1–3 need no editor and no credits. **They clear 465 of the 815 nodes**, 57% of the pass,
before the render rig is touched.

---

## 8. Reconciliation with stage 1

Stage 1 spared **608 visible name-matched nodes** for human review. That list is fully
accounted for here: they are the `image NN` nodes carrying the PC, NP, MAP, SH and FG
families. **Sparing them was correct** — every one is real content on a shipping screen, and
deleting them by name pattern would have stripped the layouts of the very art this stage is
scheduling replacements for.
