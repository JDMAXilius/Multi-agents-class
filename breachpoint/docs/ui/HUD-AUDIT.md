# HUD audit — Figma `yznvnVdOFDADaugZSeomfP` vs the measured reference

**Date:** 2 Aug 2026 · **Read-only pass.** Nothing was written to Figma; another process holds the
write lock. Repairs are scripted, not applied — see §5.

**Ground truth:** `HUD-CAMPAIGN-MEASURED.md` (campaign, element by element) and
`HUD-REFERENCE.md` §3 / §3b (multiplayer, and the measured-vs-unverified ledger).
All values are base 1280×720.

**Scope:** 85 components / named elements across `HUD / Elements` (6:48) and `HUD / Core` (6:47).
**33 fail**, 52 pass. Tolerance: any delta > 0.5 px is a flag.

**Reading the numbers.** Figma metadata reports the *axis-aligned bounding box* of a rotated node,
not its intrinsic size. Several elements are deliberately rotated (diamonds are squares at 45°, the
MP tracker is tilted −9.3°, the weapon silhouette carries a 0.42° tilt). Where the AABB
back-solves exactly to the measured size at a plausible angle, it is scored PASS and the rotation
is noted. Text nodes are compared loosely: the measurements are ink bounding boxes from a pixel
mask, a Figma text box includes leading, so a few px of y difference is not a finding.

---

## 1. `HUD / Core` — `HUD — Campaign (MEASURED 1:1 from 1920 captures)` (57:8359)

This is the screen that claims 1:1. It is very nearly right.

| Element | id | Measured | Actual | Δ | Verdict |
|---|---|---|---|---|---|
| Location Label | 57:8361 | x43.33, above plate | 43.33, 68 · 135×15 | 0 | PASS |
| Objective Banner (2 rows) | 57:8362 | 43.33, 83.33 · 326×61.33 | identical | 0 | **PASS** |
| ├ Diamond | 57:8363 | — | 12,12 · 9.90 (7×7 @45°) | — | PASS |
| ├ Objective | 57:8364 | — | 28,7 · 198×17 | — | PASS |
| ├ Sub Diamond | 57:8365 | — | 38,36 · 7.07 (5×5 @45°) | — | PASS |
| └ Sub Objective | 57:8366 | indented italic, +20 rows | 52,32 · 82×14 | — | PASS |
| Motion Tracker | 57:8367 | 42.67, 569.33 · 140×120 | identical | 0 | **PASS** (ratio 0.857 ✓) |
| Range label | 57:8376 | 37.33, 682 · cap 8.67 | 37.33, 678 · 26×14 | y −4 (box vs ink) | PASS |
| Reticle / AR | 57:8377 | 618.67, 338.67 · 42.67² | identical, centre (640,360) | 0 | **PASS** |
| Weapon Tray | 57:8384 | 1031.33,576 → 1227,680 | 1031.33, 576 · 196×104 | w +0.33 | PASS |
| ├ Slot Pip A | 57:8385 | 1141.33, 576 · 10×10 | AABB 10.13² (10×10 @0.75°) | 0 | PASS |
| ├ Slot Pip B | 57:8386 | 1187.33, 577.33 · 11.33×10 | AABB 11.46×10.15 (@0.75°) | 0 | PASS |
| ├ Grenade | 57:8387 | 1084.67, 598 · 22×16.67 | identical | 0 | PASS box / **FAIL content** |
| ├ Equipment | 57:8389 | 1170, 590.67 · 50.67×30 | identical | 0 | **PASS** |
| ├ Mag | 57:8391 | 1038, 642 · cap 14 | 1038, 638 · 21×26 | box vs ink | PASS |
| ├ Reserve | 57:8392 | 1087.33, 645.33 · cap 10 | 1087.33, 643 · 20×18 | box vs ink | PASS |
| ├ Weapon Silhouette | 57:8393 | 1124, 639.33 · 94×30.67 | AABB 94.22×31.36 (94×30.67 @0.42°) | 0 | PASS box / **FAIL content** |
| └ Base Rule | 57:8394 | 1114, 674 · 113.33×**5.33** | 1114, 674 · 113.35×**3.38** | **h −1.95** | **FAIL** |
| Waypoint | 57:8395 | 689.33, 216 · 16×15.33 | 689.33, 216 · 16×34 (incl. distance) | composite | PASS |
| ├ Diamond | 57:8396 | left edge x**689.33** | x**691.63** · 15.56 (11×11 @45°) | **x +2.30** | **FAIL** |
| └ Distance | 57:8397 | beneath | −6, 20 · 22×12 | — | PASS |

**Content failures (geometry is right, what is inside the box is not):**
- `Weapon Silhouette` is a flat blue rounded rectangle — a placeholder. The real art exists on the
  other page as `SET Weapon / Assault Rifle` (94×30.67, correct silhouette). The measured screen is
  not using it.
- `Grenade` holds only a count digit. The measured 22×16.67 block is a grenade **icon**; the glyph
  exists as `SET Grenade / Frag` but is not instanced here.

---

## 2. `HUD / Core` — `HUD — Core (1:1 Halo Infinite Arena)` (30:2)

| Element | id | Measured (§3b) | Actual | Δ | Verdict |
|---|---|---|---|---|---|
| Vitals frame | 42:2 | 503.33, 66 · 273.33×20 | 503.33, 66 · 273.33×34 | frame includes health | PASS |
| ├ **Shield Arc** | 42:3 | **273.33×20** @ 503.33,66 | **261.30×17.35** @ 509.33,67.5 | **w −12.03, h −2.65, x +6.00** | **FAIL** |
| ├ Health | 42:4 | slot 503, 86 · 273×5 *(inferred)* | 573.33, 86 · 133×5 | **x +70.33, w −140** | **FAIL (advisory)** |
| └ Centre Tick | 42:5 | bar centre x639.67 | 639.23 | 0.44 | PASS |
| Match State | 42:6 | 474.67, 622 · 302×22 | identical | 0 | **PASS** |
| ├ Mode/Bar/Score/Timer ×9 | 42:7–15 | composition unmeasured | — | — | OURS |
| Motion Tracker | 42:16 | centre (111.61,630.45) · 122.47×109.83 · −9.3° | identical | 0 | **PASS** |
| ├ Field | 42:17 | ratio 0.897 | AABB 138.61×128.18 = 122.47×109.83 @ −9.3° | 0 | PASS |
| ├ Rings 2–3, Spokes 1–4 | 42:18–23 | spokes at 12/3/6/9 | concentric, on centre | — | PASS |
| ├ Self / Blips / Range / Callout | 42:24–28 | — | — | — | OURS |
| Loadout Tray + 14 children | 30:34–48 | no MP tray measured | 940, 580 · 280×110 | — | OURS |
| Reticle | 30:49 | **unmeasured** per §3b | 620, 340 · 40×40 | — | OURS |
| Event Feed (3 rows) | 30:21–33 | not measured | 60, 455 · 340×76 | — | OURS |
| Damage Direction Wheel | 30:56 | not measured | 580, 300 · 120×120 | — | OURS |
| Interaction Prompt | 30:57–61 | §3 estimate x537–737 y510 | 560, 420 · 240×22 | est. only | OURS |
| Objective Waypoint | 30:62–64 | 16 amber diamond | 880, 250 · 40×34, diamond 18.38 (13×13) | diamond +2.4 vs 16 | OURS (arena screen) |

The `Health` failure is scored **advisory**: `HUD-REFERENCE.md` §3b explicitly flags the health slot
as *inferred, not measured* ("every shot has full shields"). Our 133-wide bar at x573 is a
half-full fill, not a slot. It is not a 1:1 miss — but it disagrees with our own reference and
should be reconciled deliberately rather than by accident.

---

## 3. `HUD / Elements` — the `SET` component set

### 3.1 Vitals (2)

| Component | id | Measured | Actual | Δ | Verdict |
|---|---|---|---|---|---|
| SET Vitals / Shield + Health | 61:5 | shield 273.33×20 | box 273.33×34 | — | **FAIL** |
| ├ Shield Arc | 61:6 | 273.33×20 | 261.30×17.35 | **w −12.03, h −2.65** | **FAIL** |
| └ Health | 61:7 | slot y+20, h5 | **(0,0)** 133×6 | **y −20** | **FAIL — collides with the arc** |
| SET Vitals / Shield Broken | 61:9 | shield 273.33×20 | box 273.33×34 | — | **FAIL** |
| ├ Shield Arc Empty | 61:10 | 273.33×20 | 261.30×17.35 | **w −12.03, h −2.65** | **FAIL** |
| └ Health Exposed | 61:11 | slot y+20, h5 | **(0,0)** 90×6 | **y −20** | **FAIL — collides with the arc** |

**Visual:** rendered, the yellow health bar sits *on top of* the left third of the shield arc — a
yellow trapezoid punched through the shield. The same component drawn inside `HUD — Core` (42:4)
has the health at y+20 and reads correctly, so the two copies disagree with each other.

### 3.2 Ammo (3) — no measured basis beyond the cap-height ratio

| Component | id | Measured | Actual | Verdict |
|---|---|---|---|---|
| SET Ammo / Readout | 61:15 | mag cap 14 / reserve cap 10 (1.4×) | 190×40; mag 21×26, reserve 20×18 | PASS ratio · OURS box |
| SET Ammo / Low | 61:18 | — | 190×40 | OURS |
| SET Ammo / Battery | 61:21 | energy weapons show `100%` (§2) | 190×40 | OURS |

The 190-wide box is ~120 px of dead space to the right of the digits. Cosmetic; harmless.

### 3.3 Grenades (8) — **all eight fail**

| Component | id | Measured | Actual | Δ | Verdict |
|---|---|---|---|---|---|
| SET Grenade / Frag | 61:27 | **22 × 16.67** | **22 × 30** | **h +13.33** | **FAIL** |
| ├ glyph | 61:28 | — | 10×15 @ (0,0) | **6 off-centre** | **FAIL** |
| └ Count | 61:29 | — | 6×14 @ (6,18) | stacked below | **FAIL** |
| SET Grenade / Plasma | 61:30 | 22 × 16.67 | 22 × 30 | h +13.33 | **FAIL** |
| SET Grenade / Spike | 61:33 | 22 × 16.67 | 22 × 30 | h +13.33 | **FAIL** |
| SET Grenade / Dynamo | 61:36 | 22 × 16.67 | 22 × 30 | h +13.33 | **FAIL** |
| SET Grenade / Frag Unselected | 61:39 | 22 × 16.67 | 22 × 30 | h +13.33 | **FAIL** |
| SET Grenade / Plasma Unselected | 61:42 | 22 × 16.67 | 22 × 30 | h +13.33 | **FAIL** |
| SET Grenade / Spike Unselected | 61:45 | 22 × 16.67 | 22 × 30 | h +13.33 | **FAIL** |
| SET Grenade / Dynamo Unselected | 61:48 | 22 × 16.67 | 22 × 30 | h +13.33 | **FAIL** |

The measured grenade block is 16.67 tall — it physically cannot hold a 15 px glyph *and* a 14 px
count stacked. The count belongs beside the glyph, not under it.

### 3.4 Abilities (12) — all pass

Every `SET Ability / *` is 50.67 × 30, matching the measured Equipment block at (1170, 590.67).
Ready variants carry a glyph + uses digit; Cooling variants carry a glyph + a fill-up band
(48.67 × 11 at y18), which is the documented behaviour.

| Component | id | Actual | Verdict |
|---|---|---|---|
| Grapple Ready / Cooling | 61:54, 61:57 | 50.67×30 | **PASS** |
| Repulsor Ready / Cooling | 61:60, 61:63 | 50.67×30 | **PASS** |
| Threat Ready / Cooling | 61:66, 61:69 | 50.67×30 | **PASS** |
| Drop Wall Ready / Cooling | 61:72, 61:75 | 50.67×30 | **PASS** |
| Thruster Ready / Cooling | 61:78, 61:81 | 50.67×30 | **PASS** |
| Overshield Ready / Cooling | 61:84, 61:87 | 50.67×30 | **PASS** |

Minor: the glyph sits at (0,0), touching the box border. Not a failure — no interior is measured.

### 3.5 Reticles (6)

| Component | id | Measured | Actual | Δ | Verdict |
|---|---|---|---|---|---|
| SET Reticle / Assault Rifle | 62:5 | **42.67**, 4 quadrant arcs + centre cross | 42.67², 4 arcs + ticks 1.33×6 / 6×1.33, both on centre | 0 | **PASS** |
| SET Reticle / Battle Rifle | 62:12 | none | 42.67², 3 dots + top tick | — | OURS |
| SET Reticle / Sidekick | 62:17 | none | 36²; chev 16×12 @ **(0,0)**, dot @ (17,22) | chev 10 off-centre, dot 4 low | **FAIL (defect)** |
| SET Reticle / Sniper | 62:20 | **57.33**, different shape | 57.33²; 4 bars centred on **26.50**, box centre **28.665** | **2.17 in both axes** | **FAIL** |
| SET Reticle / Shotgun | 62:26 | none | 52², ring + centred pip | — | OURS |
| SET Reticle / Enemy State | 62:29 | none | 42.67², same build as AR | — | OURS |

Sniper: the **size** 57.33 matches the second measured weapon exactly. The four bars inside are
mis-centred — they sit 2.17 px up and left of the centre dot, which *is* centred. Visible.
Its shape (a plus of four bars) is not the measured "four bars forming an open square bracket";
that remains unverified and is flagged, not repaired.

### 3.6 Minimap / motion tracker (3)

| Component | id | Measured | Actual | Δ | Verdict |
|---|---|---|---|---|---|
| SET Minimap / Clear | 62:54 | disc **140×120**, ratio 0.857 | Field 140×120, box 140×138 (disc + labels) | 0 | **PASS** |
| ├ Ring 2 / Ring 3 | 62:56–57 | concentric | centres (70,60) ✓ | 0 | PASS |
| ├ Spokes 1–4 | 62:58–61 | 12/3/6/9 | on the axes ✓ | 0 | PASS |
| ├ **Self** | 62:62 | disc centre (70,60) | centre **(71.78, 61.78)** | **1.78, 1.78** | **FAIL** |
| └ Range | 62:63 | — | x0, italic — **first glyph clipped by the component bound** | — | **FAIL (defect)** |
| SET Minimap / Contacts | 62:65 | disc 140×120 | identical to Clear + blips | 0 | **PASS** |
| ├ Self / Range | 62:73, 62:77 | — | same two faults | 1.78 / clip | **FAIL** |
| SET Minimap / Disabled | 62:79 | disc 140×120 | Field 140×120 ✓ | 0 | PASS |
| └ Off label | 62:81 | — | x34, 83 wide → centre 75.5 vs 70 | **5.50** | **FAIL (defect)** |

The disc is the **campaign** tracker (140×120, ratio 0.857), not the multiplayer one
(122.47×109.83, ratio 0.897) that `HUD — Core` uses. Both are correct for their screen; they must
not be swapped. Worth a variant, not a fix.

### 3.7 Feedback (6)

| Component | id | Measured | Actual | Δ | Verdict |
|---|---|---|---|---|---|
| SET Feedback / Hitmarker | 62:85 | **unmeasured** (§3b) | 40×40; marks at x6/24, y6/32.4 → centre **(19.1, 23.3)** | **0.9 / 3.3 off centre; bottom overflows the box by 0.60** | **FAIL (defect)** |
| SET Feedback / Hitmarker Kill | 62:90 | unmeasured | identical geometry | same | **FAIL (defect)** |
| SET Feedback / Damage Direction | 62:95 | unmeasured | 120×120 arc | — | OURS |
| SET Feedback / Nameplate | 62:97 | not built / needs reference | 130×30; star, name, bar | — | OURS |
| SET Feedback / Interaction Prompt | 62:101 | §3 estimate 200 wide | 220×24, key glyph + verb + rule | est. only | OURS |
| SET Feedback / Medal Chip | 62:106 | unmeasured (medal art) | 64×64; **plate 44×54 @ (0,0)** and **inlay 26×34 @ (0,0)** | **plate 10/5 off centre, inlay 9/10 off the plate** | **FAIL (defect)** |

**Medal Chip renders as two misaligned hexagons** — a grey plate with a yellow inlay hanging off its
top-left corner. Both children are pinned at the origin; neither was ever centred.

### 3.8 Weapon silhouettes (6) — all pass

| Component | id | Measured | Actual | Verdict |
|---|---|---|---|---|
| SET Weapon / Assault Rifle | 78:2 | **94 × 30.67** | 94 × 30.67 | **PASS** |
| SET Weapon / Battle Rifle | 78:4 | 94 × 30.67 | 94 × 30.67 | **PASS** |
| SET Weapon / Sidekick | 78:6 | 94 × 30.67 | 94 × 30.67 | **PASS** |
| SET Weapon / Sniper | 78:8 | 94 × 30.67 | 94 × 30.67 (art inset 88×24.5) | **PASS** |
| SET Weapon / Shotgun | 78:10 | 94 × 30.67 | 94 × 30.67 | **PASS** |
| SET Weapon / Rocket | 78:12 | 94 × 30.67 | 94 × 30.67 | **PASS** |

Real silhouette art, correctly sized. The measured campaign screen does not use them (see §1).

---

## 4. `HUD / Elements` — the legacy `HUD / *` components (superseded)

These predate the `SET` set and the measured campaign frame. They were built to
`HUD-REFERENCE.md` §3's **estimates**, which §3b supersedes. Every one of them is wrong against
the measurements, and every one except the objective banner is duplicated by a `SET` component.

| Component | id | Measured | Actual | Δ | Verdict |
|---|---|---|---|---|---|
| Rows=One (Objective Banner) | 23:2 | 326 × **41.33** | 323 × **32** | **w −3, h −9.33** | **FAIL** — no SET replacement, repair it |
| Rows=Two (Objective Banner) | 23:6 | 326 × **61.33** | 323 × **57** | **w −3, h −4.33** | **FAIL** — no SET replacement, repair it |
| HUD / Location Label | 23:13 | no width measured | 240×16 | — | OURS |
| HUD / Toast | 23:15 | §3 **estimate** 313×117 | 313×117 | 0 vs estimate | OURS (estimate, not 1:1) |
| HUD / Waypoint | 23:24 | 16 × 15.33 | **20 × 20**; Outer 18.38 @ (3,3) **overflows the box by 1.38** | **4 / 4.67** | **FAIL — superseded** |
| HUD / Subtitle | 23:27 | not measured | 360×26 | — | OURS |
| HUD / Motion Tracker | 24:2 | 140×120 (campaign) or 122.47×109.83 (MP) | **133×133**; ticks at −3; `59 m` label at y138, **outside the 133 box** | **7 / 13** | **FAIL — superseded by SET Minimap** |
| HUD / Reticle State=Rest | 24:13 | 42.67 (AR) | 40×40, ring d27 | 2.67 | **FAIL — superseded by SET Reticle** |
| HUD / Reticle State=Enemy | 24:20 | 42.67 | 40×40 | 2.67 | **FAIL — superseded** |
| HUD / Reticle State=Ally | 24:27 | 42.67 | 40×40 | 2.67 | **FAIL — superseded** |
| HUD / Weapon Tray | 24:35 | 196 × 104 | **240 × 90** | **44 / 14** | **FAIL — superseded** |
| ├ Weapon Silhouette | 24:43 | 94 × 30.67 | 80 × 30 | 14 / 0.67 | **FAIL** |
| └ Tray Rule | 24:44 | 113.33 × 5.33 | **200 × 1** | **86.67 / 4.33** | **FAIL — largest raw delta in the file** |

Also present on this page and **out of audit scope** (its own reference, ADS/scoped):
`HUD — Scoped / Smart-Link (ADS) — MEASURED 1:1` (44:2), 60+ children. It carries a
`SOURCE GAP` note frame of its own and was not compared here.

---

## 5. Verdict by category

### 5.1 Genuinely 1:1 (matches a measured number to < 0.5 px)

- Campaign objective banner — 326 × 61.33 at (43.33, 83.33), and the +20 second-row rule.
- Campaign motion tracker — 140 × 120 at (42.67, 569.33), ratio 0.857.
- Campaign reticle box — 42.67 at (618.67, 338.67), centre exactly (640, 360).
- Campaign weapon tray box, both slot pips, the grenade box, the equipment block, and the
  weapon-silhouette box (all boxes; two of them hold placeholders — see §1).
- Core shield-bar **frame** coordinates — 273.33 × 20 slot at (503.33, 66), centre x 639.67.
- Core score/timer bar — 302 × 22 at (474.67, 622).
- Core motion tracker — 122.47 × 109.83, −9.3°, centre (111.61, 630.45).
- `SET Reticle / Assault Rifle` 42.67 and `SET Reticle / Sniper` 57.33 — both sizes exact.
- All 12 `SET Ability / *` at 50.67 × 30.
- All 6 `SET Weapon / *` at 94 × 30.67.
- The `SET Minimap / *` disc at 140 × 120.

### 5.2 Ours by necessity — **no reference exists, must not be presented as 1:1**

`HUD-REFERENCE.md` §3b lists these as still unmeasured: reticle geometry per weapon class,
hitmarker art, low-ammo colour state, medal-feed anchor, health-bar height, fill opacity vs colour.
Everything below is original work built on top of that gap:

- `SET Reticle / Battle Rifle`, `Sidekick`, `Shotgun`, `Enemy State` — invented shapes.
  Only the AR (42.67) and Sniper (57.33) **sizes** are sourced; no reticle **shape** is.
- `SET Feedback / Hitmarker`, `Hitmarker Kill`, `Damage Direction`, `Nameplate`,
  `Interaction Prompt`, `Medal Chip` — all six.
- `SET Ammo / Readout`, `Low`, `Battery` — the 190×40 box; only the 14/10 cap ratio is measured.
- `SET Ability / *` interiors (glyphs, uses digit, cooldown band). Only the 50.67×30 box is measured.
- Core: `Loadout Tray`, `Event Feed`, `Reticle`, `Damage Direction Wheel`, `Interaction Prompt`,
  `Objective Waypoint` — the whole arena composition except the shield bar, the score bar and
  the tracker.
- `HUD / Toast`, `HUD / Location Label`, `HUD / Subtitle` — §3 estimates or invented.
- The Core `Health` bar — the slot is explicitly inferred, never measured.
- All ability, grenade and weapon *identities* (Grapple, Repulsor, Threat, Drop Wall, Thruster,
  Overshield; Frag, Plasma, Spike, Dynamo) are BREACHPOINT's, not Halo's.

### 5.3 Still wrong

**Against a measured number (19):**

1. `SET Grenade / *` × 8 — 22 × 30 vs 22 × 16.67.
2. `SET Vitals / Shield + Health` and `Shield Broken` — arc 261.30 × 17.35 vs 273.33 × 20.
3. Core `Shield Arc` (42:3) — same 12.03 px shortfall.
4. Campaign `Base Rule` — 3.38 tall vs 5.33.
5. Campaign `Waypoint / Diamond` — 2.30 px right of the measured left edge, overflows its frame.
6. `SET Minimap / Clear` + `Contacts` `Self` marker — 1.78 px off the disc centre.
7. Legacy `Rows=One` / `Rows=Two` — 323 × 32 / 57 vs 326 × 41.33 / 61.33.
8. Legacy `HUD / Waypoint`, `HUD / Motion Tracker`, `HUD / Weapon Tray`, and the three
   `HUD / Reticle` states — all superseded duplicates, all wrong.

**Defects with no measured reference (7)** — nothing to be 1:1 *against*, but visibly broken:

9. `SET Feedback / Medal Chip` — plate and inlay both at (0,0); the inlay hangs off the corner.
10. `SET Feedback / Hitmarker` + `Hitmarker Kill` — cluster centred at (19.1, 23.3) not (20, 20);
    the bottom marks overflow the 40 box by 0.60.
11. `SET Reticle / Sniper` — four bars 2.17 px off the centre the dot sits on.
12. `SET Reticle / Sidekick` — chevron jammed into the top-left corner.
13. `SET Minimap / *` `Range` label — italic, at x0, first glyph clipped by the component bound.
14. `SET Minimap / Disabled` `Off` label — 5.5 px off centre.

**Content, not geometry (3)** — needs a decision, not a coordinate:

15. Campaign `Weapon Silhouette` is a placeholder rectangle while `SET Weapon / Assault Rifle`
    holds the correct art at the correct size.
16. Campaign `Grenade` holds a count with no grenade glyph, while `SET Grenade / Frag` exists.
17. Core `Health` bar disagrees with our own (inferred) slot by 140 px of width.

**Three worst discrepancies**

| # | Element | Measured | Actual | Δ |
|---|---|---|---|---|
| 1 | `SET Grenade / *` (×8) | 22 × **16.67** | 22 × **30** | **13.33 px** |
| 2 | Shield arc — Core 42:3 and both SET Vitals | **273.33 × 20** | **261.30 × 17.35** | **12.03 × 2.65 px** |
| 3 | `SET Feedback / Medal Chip` | (centred) | plate and inlay both at (0,0) | **10 / 9 px** |

Larger raw deltas exist in the legacy `HUD / Weapon Tray` (its rule is 200 × 1 against a measured
113.33 × 5.33, Δ 86.67), but that component is superseded by the campaign tray and should be
deleted rather than repaired.

### 5.4 What is NOT built

Unchanged from `HUD-REFERENCE.md` §3: scope/ADS overlay is partially covered by the ADS frame
(44:2, out of scope here); killfeed exists only inside `HUD — Core`, not as a component; medal art
is a single placeholder chip; scoreboard, death/respawn and pause menu do not exist.

---

## 6. Repair scripts

Two, because a single script may call `setCurrentPageAsync` at most once — and in fact neither
needs it, both use `page.loadAsync()`:

- `scripts/hud-repair-elements.js` — page `HUD / Elements`: vitals arcs and the health collision,
  all 8 grenades, sniper bars, sidekick chevron, minimap self marker + labels, both hitmarkers,
  the medal chip, both objective-banner variants, and a rename pass marking the 4 superseded
  legacy components.
- `scripts/hud-repair-core.js` — page `HUD / Core`: the campaign base rule and waypoint diamond,
  and the Core shield arc + health slot.

Neither script mutates text content or type metrics, so neither loads fonts — they move and
resize only. Both find nodes by name with a null guard, collect misses into `notFound`, and return
the ids they touched. **Deliberately not scripted:** swapping the campaign tray's placeholder
silhouette and grenade box for instances of `SET Weapon / *` / `SET Grenade / *`. That is
destructive (delete + createInstance across pages) and is a design call, not a repair.
