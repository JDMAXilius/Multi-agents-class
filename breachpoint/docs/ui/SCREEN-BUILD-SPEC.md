# Screen build spec — invariants, flows and the components no front-end kit has

Produced by a parallel read-only pass over the pasted reference pages in
`figma.com/design/yznvnVdOFDADaugZSeomfP`. Companion to `REFERENCE-EXTRACTION.md` (inventory)
and `COMPONENT-SPECS.md` (component geometry). **This file is the one to read before building a
screen.**

---

## 1. Screen invariants — true on every 1280×720 frame, build once

```
Background plate     0,0    1280x720   the 3D subject IS the background, not a UI node
Profile Bar          0,670  1280x50    always reserved
Button Prompts       74,685 Wx20       W tracks prompt COUNT: 58/62 (1) · 133/146 (2) · 253 (3)
Title band           0,0    1280x75    Page Title
                  or 0,0    1280x105   Item Title  (carries name + rarity tag + equipped check)
Nav Bar              44,45  666x30     root level
                  or 44,75 / 44,110  516x30   sub level
Left column          x=69/70 for text lists · x=86 for grids · never exceeds x=620
Right band           x>=650 reserved for the 3D subject
```

**Only two nodes ever break into the right band:** `Gear Detail` (650→1236) and the
`Currencies` row (992→1208). Everything else respects it. That is the composition law from
`ui-presentation` §1 expressed as a hard number.

**Grid math, constant everywhere:** tile **114×114**, pitch **130** (16 gutter), 4 columns =
**504** wide, grid origin **(86, 260)**, scrollbar **8×374 at x=62**.

---

## 2. The two-state frame — the single most important structural finding

Nearly every customization screen ships **two authored states inside one frame**, one visible and
one hidden:

- **State A (list):** `Shade` scrim + `Menu List` (536×542 or 536×446)
- **State B (browse):** `Filter & Sort Bar` (86,206 504×40) + `Item Grid` (86,260 504×374) +
  `Vertical Scroll Bar` (x=62) + `Gear Detail` (650,464 586×161) + `Currencies` (992,47)

This is a **list ↔ grid transition on one screen**, not two screens. Build it as one WBP with a
state switch, not two widgets. Getting this wrong doubles the screen count for no reason.

Two more animation affordances are authored into the panels themselves:

- **Selection caret** — every Menu List border contains `Rectangle 278`, 3×65 at x=−4: a
  rail-hugging caret that slides vertically to the focused row. Its authored y encodes the focus
  index (54 / 78 / 98 / 138 / 180).
- **Panel reveal notches** — `Rectangle 258` (top) and `Rectangle 259` (bottom), 88×4.7 chamfers
  subtracted from the panel border. **The open/close wipe originates from these**, which is why
  the panel appears to unzip rather than fade.

---

## 3. Customization drill-down — the transferable taxonomy

Halo's armour art does not transfer. **This navigation model does**, and it is what Breachpoint's
operator customization should be:

1. **Control Panel** — Start Menu root, Nav Bar y=45, Menu Combo 69,286 (6 rows)
2. **Customize** — same chrome, Menu Combo 69,327 (5 rows). *The 3D subject does not re-frame.*
3. **Armor Hall** → operator loadout — Nav Bar is **replaced by Page Title**. Left column becomes
   a 504-wide **equipped-slot grid** at (86,260): 2×4 tiles = 8 slots, plus 2 shortcut buttons.
4. **Slot page** — the two-state frame of §2. The 3D subject **re-frames to the slot**.
5. **Item page** — Page Title (75) is **replaced by Item Title (105)**; a **sub-level Nav Bar
   appears at 44,110** with per-item tabs. Everything below shifts up 40px.
6. **Item tabs are siblings, not children** — Attachments / Coating / Appearance reuse the same
   Item Title + sub-nav; only the left column swaps.
7. **Zone selected** → a 3-tile **channel row** at (86,202): `COLOR | FINISH | PATTERN`, with a
   13px tick dropping from the COLOR tile into the Color Picker at (88,357).
8. **Channel drill-down** (terminal) — `Gear Detail` shrinks 586×161 → **586×125** because
   materials carry no manufacturer attribution row.
9. **Filters are orthogonal** — `Filter Page` 451×682 at x=48 over a full scrim, layered over
   *whatever* grid is beneath. Not a navigation level. Same 451×682 footprint as `Pop-Up Options`
   → **one component, two variants.**

**Persists across all levels:** background plate, Profile Bar, Button Prompts.
**Swaps:** the title band (75 ↔ 105) and the Nav Bar y (45 → 75/110).

---

## 4. Forge — the map editor, documented nowhere else

### Radial menu ring
Outer **366×366**, hub cutout **130×130** concentric, Exit button **62×62** at (152,152) so its
centre is exactly ring centre. Ring centre lands on screen centre: 457+183 = **640**,
177+183 = **360**.

Quadrants are **overlapping full-span slabs, not pie slices**: Top/Bottom are 366×136 at y=0/230,
Left/Right are 136×366 at x=0/230. 136 + 230 = 366, so they overlap in all four corners; the wedge
shape comes from a boolean fill *inside* the component. Z-order: Bottom → Right → Left → Top.

Inside a quadrant the visible wedge is **256 wide centred in the 366 slab** (55px dead margin each
side) with an 80×80 icon centred at x=183 over a 90×90 dot ring. `Fill` and `Stroke` are the two
layers that switch on for Hover.

**Thumbstick indicator** — 35×5 tick at ring-space (183,129), in the gap between hub wall and
wedge. Build as **one indicator rotated in 90° steps**, not four assets.

### Quantised colour picker
The swatch field is **not a continuous 2D gradient**. It is a **6×6 grid of flat 55×32.667 cells**
sampled from a hue+white+black ramp stack, with a **whole-cell selection rectangle**. This is a
deliberately controller-friendly, D-pad-navigable picker. Hue rail 330×10 with a 1×12 caret that
**overshoots the rail by 1px top and bottom**. Hex row: `HEX` left, value right-aligned.

### Material picker — centre-anchored accordion
Three levels, **one panel, rows injected in place**:

| Level | y | height | rows |
|---|---|---|---|
| Categories | 273.5 | 173 | 6 category rows |
| › UNSC | 288 | 144 | header + 4 children |
| › UNSC › Concrete | 244.5 | 231 | header + Concrete + **3 injected options** + 3 siblings |

1. **Vertical centre is pinned at y=360** in all three: 273.5+86.5 = 288+72 = 244.5+115.5 = 360.
   The panel grows and shrinks **symmetrically about screen centre**. Animate height and y together.
2. Row 0 is a **breadcrumb-stepper**: `← UNSC →`. The back affordance and the sibling pager are
   **the same row**.
3. Level 2→3 is an **in-place expansion, not a navigation**. Selecting `Concrete` injects its
   three options beneath it and pushes siblings down; every sibling stays on screen and clickable.

Nothing in a standard UI kit does this. Panels are **336 wide with 28px rows at 29 pitch** — note
this is *not* the front end's 250/40.

---

## 5. Components that must be created — no reference component covers them

| Component | Spec | Needed by |
|---|---|---|
| **Scroll Bar** | slim 8×N (thumb 8×70) and wide 13×N (thumb 7×70) | every grid + Settings |
| **Scrim** | 1280×720 dim plate between the 3D plate and any panel | 11 screens |
| **Color Picker** | 373×220: SV field 367×173 as 6×6 quantised cells + hue bar 367×10 + hex row | customization, Forge |
| **Group Label** | 18px caps label above a grid row (FOREHEAD, PRIMARY, ZONE 3). Distinct from Small Header (512×30, panel-width) | grouped/zone grids |
| **Reward Track** | 1156-wide horizontal viewport, 114 tile rail @ pitch 130 + chip rail @ pitch 260, content to x=3510 | Battle Pass, Career |
| **Countdown Chip** | 88×40 = 20×20 icon + timer text | Challenges |
| **Currency Row** | 216×34 = 2× Currency Widget @ pitch 110, anchored (992,47) | 4 screens |
| **Preview Panel** | 819×720 render viewport with a diagonal boolean mask | Battle Pass |
| **Progression Row** | 1143×193 multi-column, hideable 3rd column (data-driven count) | Career |
| **Radial Menu** + **Radial Quadrant** | §4 — orientation-specific, not one rotated component | Forge |
| **Thumbstick Indicator** | 35×5, snaps to 4 rotations | Forge |
| **Breadcrumb Stepper** | `← LABEL →`, back control *and* sibling pager in one row | Forge |
| **In-Place Expanding Tree** | accordion in a centre-anchored panel | Forge |
| **Bumper Tab Strip** | 284×36 rail flanked by LB/RB glyphs, 57×36 tabs | Forge, item pages |
| **Transform Readout** | 82px right-aligned label column + 298px dark value pane | Forge |
| **CRT Scanline Overlay** | 4px-pitch 2px lines — ship as a gradient, **not** the 180 authored rects | VISR / boot |
| **Input Map Diagram** | 591×291 keybind visual — **must be original art** | Settings |

---

## 6. Art that must be replaced, by screen

> **Superseded by `ART-PASS-STAGE-2.md`.** This table was a sketch written before the file was
> counted. The survey found **815 visible image fills, not ~250**, and three entries below are
> wrong: the `Items` tiles are 101 visible (not ~180, and the 114×114 boxes carry no paint),
> `Rank Image` is a grey placeholder rather than Halo art, and the `Commendation Card` icons do
> not exist. Kept for its per-screen framing; use the stage-2 ledger for any count or schedule.

Every `image 55` / `image 60` / `image 9` / `image 51` is a **hidden screenshot underlay** — delete,
never port. Visible Halo-owned art needing original Breachpoint replacements:

| Node | Screen | What |
|---|---|---|
| `image 56` 1280×720 visible | Battle Pass | season key art |
| `Heroes of Reach` 160×160 | Battle Pass | season emblem |
| `UNSC LOGO` 140×140 | Career Unlocks | UNSC/343 mark |
| `Rank Image` 116×135 + 7× `Rank Label` | Career Unlocks | rank insignia |
| `Controls` 591×291 | Settings | Xbox controller diagram |
| ~180× `Items` 114×114 | all grids | armour / weapon / coating / material renders |
| `Commendation Card` icons ×5 | MP Challenges | medal art |
| `Start Menu Background` | every screen | Spartan / environment render |
| `Update Text` 220×42 | Career Unlocks | third-party attribution — **delete** |

### Gear / Items taxonomy (transfers; the art does not)
**Slots:** Helmet · Visor · Helmet Attachment (forehead / chin / left / right) · Chest · Left
Shoulder · Right Shoulder · Wrist · Gloves · Weapon Model.
**Cosmetic layers, orthogonal to slot:** Coating · Finish · Pattern · Colour.
**Non-cosmetic:** Credits · XP Boost · Challenge Swap · Kits (a preset filling several slots).
Attachment names encode `ANCHOR/MANUFACTURER-CODE NAME` — a two-part convention worth keeping.
Tile anatomy: 100 outer → 80 border → 70 art, on a 110 pitch.

### Emblems taxonomy
**Emblem design × named palette** are two independent axes — one artwork, N reusable recolours —
rendered at three fixed sizes: **116 badge · 680×128 banner · 1000×776 backdrop** (1:1, 5.31:1,
1.29:1). Backdrops are a separate pool and are *not* palette-varianted.

---

## 7. Interactions the structure proves (not invented)

- **Horizontal reward carousels** — 1156 viewport, content to x=2210–3510; tile rail and chip rail
  pan **in lockstep** at pitch 130 / 260.
- **Reroll affordance** — on focus, the Challenge Card narrows (511.5 → 461.5) and a 40×40 swap
  button slides in to its left. Unfocused rows use full width.
- **Live countdown** — `5:01:42` on Challenges; a dedicated `Countdown` frame uses squares +
  subtract masks for digit flips.
- **Prompt bar reacts to context** — width tracks prompt count, so it must be auto-layout hug.
- **Variable column counts** — Career's 3rd column is hidden and shares x=794 with the 2nd.
  Columns are data-driven.
- **4-frame Loading Icon** — the sprite animation is authored in the source; drive at a constant rate.
- **`Searching` / `Searching 2`** on the Load Bar — a looping two-state search animation.
