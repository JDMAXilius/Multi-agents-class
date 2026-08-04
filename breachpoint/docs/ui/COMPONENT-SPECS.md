# Component specs — measured 1:1 from the reference file

**Every number here was read off the reference file's live nodes** via the Figma Plugin API
(`use_figma` read-only against `Kn87U5sy2VD0lP8K7h4LcQ`), not from a screenshot and not from the
XML metadata dump. Fills, strokes, stroke alignment, auto-layout padding, letter-spacing units and
effect parameters are exact. Build to these.

**Node-id provenance:** every node id quoted here (`1:2`, `124:1179`, …) is an id in the
**reference file `Kn87U5sy2VD0lP8K7h4LcQ`** named above — **not** in Breachpoint's working file
**BREACHPOINT — UI/UX System** (`figma.com/design/yznvnVdOFDADaugZSeomfP`), which holds cloned
copies under different ids. Verified 2 Aug 2026 on `1:2` and `124:1179`; the earlier "Target
file:" line pointed at the working file and was wrong — it contradicted the measurement method
stated two lines above. A full per-id sweep is owed and has not been run.

---

## 0. Corrections to `UI-DESIGN-SYSTEM.md` this extraction forced

| Was | Actually |
|---|---|
| "letter spacing 15 / 10 / 8" (unitless) | **PERCENT**: `15%`, `10%`, `8%`. At 14px, 15% ≈ 2.1px. |
| Palette described as 12 semantic VISR tokens | The front end is **white/black at varying alpha**. 729 strokes in the component library are `#ffffff` 1px. Colour is *accent only*. |
| "1px white borders" | Three weights in use: **1px** (chrome, 404×), **0.5px** (fine rules, 323×), **2px** (item tiles + emphasis, 177×). Nav tab active border is **3px OUTSIDE**. |
| Corner radius unspecified | Effectively **0**. Only radii present: 5 (17×, badges), 1, 3, 0.25, 0.75. Sharp corners are the language. |
| Reference typeface unknown | The file uses **Industry Demi / Industry Medium Italic** (Halo's real commercial face) in 19 places and **Rajdhani** everywhere else. Rajdhani is the file's own substitute — we use it throughout, which is both consistent and licence-clean (OFL). |

---

## 1. The language, stated once

Flat panels. Sharp corners. Hairline white rules that are **partial, not closed boxes** — a full
top line, a dimmed bottom line, and short left/right ticks. Uppercase Rajdhani SemiBold with
enormous tracking. Hover **inverts**: the fill goes solid white and the text goes black. Nothing
glows, nothing rounds, nothing gradients except item rarity.

**Idle → Hover is an inversion, not a highlight.** That single rule explains most of the file.

---

## 2. `Main Button` → `UBRMenuRow` — the atom (250 × 28)

The most-used component in the system; 27 variants.

```
COMPONENT                       250 × 28
└ Text Frame        (2, 2)      246 × 24   auto-layout HORIZONTAL
                                           gap 10 · padding T0 R10 B0 L10
                                           primaryAxis MIN · counterAxis CENTER
  ├ Icon            INSTANCE    16 × 16    (INSTANCE_SWAP slot)
  ├ Text            TEXT        Rajdhani SemiBold 16 · ls 10% · UPPER · align LEFT
  ├ Selection       TEXT        same style · align RIGHT   (the value on a settings row)
  ├ Filter Button   GROUP       20 × 20    white rect + black glyph
  └ Border          (-2, -2)    250 × 28   four VECTOR lines, stroke align CENTER
      Top Line          250 × 2   #ffffff  1px   opacity 1
      Bottom Line       250 × 2   #ffffff  1px   opacity 0.3
      Left Line           0 × 20  #ffffff  1px   opacity 0.3
      Right Line          0 × 20  #ffffff  1px   opacity 0.3
```

| Status | What changes |
|---|---|
| **Idle** | as above — transparent fill, white text |
| **Hover** | `Text Frame` fill → `#ffffff`; Text + Selection → `#000000`; Bottom Line opacity 0.3 → **1**; an extra `Background Line` (opacity 0.3) appears behind |
| **Disabled** | every child opacity → **0.5**. Geometry unchanged |
| **Active** (drop-down / dig-down) | as Hover plus the disclosure glyph rotates |

**Type axis** (10 values) swaps what sits inside the same 250×28 shell: `Default`, `Disabled`,
`Drop Down`, `Dig Down`, `Icon Only` (40×40), `Slider`, `Checkbox`, `Radio`, `Map Voting` (250×60),
`Image` (250×120). **Alignment axis**: `Left`, `Center`.

---

## 3. `Menu Slider Button` → nav tab (138 × 26)

```
COMPONENT              138 × 26
├ Border   RECTANGLE   138 × 26   stroke #ffffff · align OUTSIDE
│                                 weight 3 when Active=True, 2 when Active=False
├ Text     (13, 5)     120 × 14   Rajdhani SemiBold 14 · ls 15% · #ffffff · align LEFT
└ Icon     (113, 1)     24 × 24   INSTANCE (optional, e.g. Settings)

Active=False → whole component opacity 0.6
```

**Nav bar** (`666 × 30` at `(33, 45)`): four tabs at `x = 39, 189, 339, 489`, `y = 2` — **pitch
150**. Bumper prompts `27 × 15` at `x = 27` and `x = 639`, `y = 7.5`.

---

## 4. `Player Buttons` → `UBRRosterRow` (390 × 30 standalone; 349 in-panel)

```
COMPONENT                    390 × 30
├ Team Fill   INSTANCE  (2,2) 386 × 26   ← per-player colour / emblem art
├ Border      FRAME           390 × 30   opacity 0.3, four lines as §2
└ Content     (2, 2)          386 × 26   auto-layout HORIZONTAL
                                         gap 10 · padding T0 R15 B0 L5 · CENTER
   ├ Emblem       26 × 26
   ├ Gamertag     TEXT   Roboto Condensed Medium 14 · ls 0% · align LEFT
   ├ Rank         FRAME  30 × 26   fill #ffffff@0.5
   │  └ insignia  26 × 26   + Medal 3D Effect (see §7)
   ├ Microphone   16 × 18   (Mic / Speaking / Muted × White / Black)
   └ External Icons  40 × 30  auto-layout gap **-5** (deliberate overlap)
        ├ Party Leader Frame  30 × 30
        └ Current Player Icon 10 × 10
```

`Text Color` axis is `Black` | `White` — the gamertag flips against the team fill's luminance.

---

## 5. `Items` → `UBRItemTile` (114 × 114)

```
COMPONENT                       114 × 114
├ Masks          (-2,-2) 118×118   incl. Multi-Core + Consumable cutouts in the bottom edge
├ Button Border  INSTANCE 114×114   ALL LINES 2px, align CENTER
│    Top Line        112 × 10   #ffffff
│    Bottom Line     112 × 11   ← RARITY COLOUR
│    Left Line        82 × 0    #ffffff@0.4
│    Right Line       82 × 0    #ffffff@0.4
│    Left/Right Full  12 × 0    #ffffff        (the corner ticks)
├ Background     (7,7) 100×100   #000000@0.5 + linear gradient RARITY TINT (0 → 1 alpha)
├ Gradient       (7,7) 100×100   linear #ffffff 0 → 1
├ Art            (7,7) 100×100   item image
├ Locked         (11,11)  22×22
├ Currency       (81,11)  22×22
├ Checkbox       (14,84)  16×16
├ Favourite      (81,81)  22×22
└ Multi-Core     (42.88, 107.74) 28 × 10.33   ← sits in the bottom-edge cutout
```

| Rarity | Bottom-line stroke | Background gradient tint |
|---|---|---|
| common | `#ffffff` | `#c4c4c4` |
| rare | `#6295da` | `#70cddf` |
| epic | `#ab55ff` | `#7b61ff` |
| legendary | `#e8ba3d` | `#ffed4f` |

`Size=mini` is **30 × 30**. `Type` = `Default` | `Empty` | `Non-Interactive`.

---

## 6. Screen chrome — measured off `Play` (`1:2`)

| Element | Position | Size | Treatment |
|---|---|---|---|
| Frame | — | 1280 × 720 | fill `#000000` |
| Background | (0, 0) | 1280 × 720 | image + **LAYER_BLUR** |
| Navigation Bar | (33, 45) | 666 × 30 | §3 |
| `Menu Combo` (left rail) | (69, 138) | 349 × 510 | News & Menu 349×418 + Description Frame 349×37 at y=473 |
| News Button (feature card) | (69, 155)* | 349 × 222 | fill `#000000@0.5` |
| Menu List | (69, 385)* | 349 × 148 | rows h28 pitch 40 |
| Description Frame | (69, 563)* | 280–349 × 37 | Roboto Condensed Medium Italic 14 · ls 8% |
| Progression Button (career rank) | (869, 55) | 334 × 115 | Content fill `#000000@0.5`; Title "CAREER RANK" Rajdhani **Bold** 16 ls 10% `#ffffff` + DROP_SHADOW; Left/Right Side 167×94 each; Switcher 72×10 at (130,121) |
| Party List | (862, 397) | 349 × 273 | fill linear gradient @0.5; Background boolean `#000000@0.4` inset 3; stroke `#ffffff@0.2` 1px INSIDE |
| Profile Bar | (0, 670) | 1280 × 50 | `#000000@0.5` + **BACKGROUND_BLUR** |
| Button Prompts | (60, 685) | 62 × 20 | glyph + verb |
| Grid - 3 Column | (69, 38) | 1143 × 570 | 349 wide, **pitch 397** (gutter 48) |
| Grid - 4 Column | (69, 38) | 1143 × 570 | 249.75 wide, **pitch 297.75** (gutter 48) |
| Player (3D subject) | (480, 118) | 320 × 602 | the character's occupied box |

\* the two left-rail variants in the file disagree slightly (`Play Menu & Description` at y=0/98
vs `Menu Combo` at y=138). **`Menu Combo` at (69, 138) is the shipped one** — the other is an
earlier layer left in the file.

---

## 7. Effects

| Effect | Definition | Use |
|---|---|---|
| **Medal 3D** | `INNER_SHADOW #ffffff@0.8 (0,1) r0.5` · `INNER_SHADOW #000000@0.5 (0,-1) r0.5` · `DROP_SHADOW #000000@0.25 (0,2) r4` · `DROP_SHADOW #000000@0.5 (0,1) r1` | Rank insignia, medals **only** — 343's "skeuomorphic near gameplay" tier rule |
| **Panel blur** | `BACKGROUND_BLUR` | Profile bar |
| **Scene blur** | `LAYER_BLUR` | The 3D background behind the front end |
| **Title shadow** | `DROP_SHADOW` | Headings sitting over the 3D scene |

---

## 8. Accent palette, with observed usage

| Hex | Observed as | Breachpoint role |
|---|---|---|
| `#ffffff` | 729 strokes, 297 fills, 83 text | Chrome, everything |
| `#000000` @ 0.2 / 0.4 / 0.5 / 0.6 / 0.8 | panel grounds | Panel grounds — five discrete alphas, not arbitrary |
| `#ff5c00` | 13 fills | Event / urgent |
| `#ffc11c` | `Premium Yellow` variable | Premium / battle pass |
| `#2ec3e5` | grid overlay + accents | **Our `Shield` cyan** (doc has `#35D0F2` — close; keep ours for the HUD, use `#2ec3e5` where matching the file) |
| `#ab55ff` / `#7b61ff` | epic rarity | Epic |
| `#e8ba3d` / `#ffed4f` / `#ffd436` | legendary rarity | Legendary |
| `#6295da` / `#70cddf` | rare rarity | Rare |
| `#ff4b4b` | 1 fill | Danger / destructive |
| `#d9d9d9` | 53 fills | **Image placeholder** (Figma default) — becomes a real token: `surface/placeholder` |

`#9747ff` and `#7b61ff` also appear as Figma's own component/slot annotation colour — ignore
those instances; only the rarity uses are real.
