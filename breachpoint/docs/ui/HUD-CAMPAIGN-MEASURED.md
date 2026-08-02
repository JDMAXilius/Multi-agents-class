# Campaign HUD — measured element by element

**Source:** `halo-infinite-hud-2.png` and `halo-infinite-hud-3.png`, both **1920×1080 = exactly
1.5× our 1280×720 base**, so every value divides cleanly. Measured by connected-component
analysis on a colour-isolated UI mask, not by eye. Both frames agree to ~1px on every persistent
element, which is the cross-check.

**This is the CAMPAIGN HUD and it is a different layout from the Arena multiplayer HUD.** Our
`HUD / Core` was built to the multiplayer layout (shield trapezoid top-centre, score+timer
bottom-centre). Neither is wrong — they are two different screens — but they must not be mixed.

| | Campaign | Arena multiplayer |
|---|---|---|
| Top-left | **Objective banner + location label** | — |
| Top-centre | — (no shield bar in either capture) | Shield + health trapezoid |
| Bottom-centre | — | Score · timer · score |
| Bottom-left | Motion tracker + range | Motion tracker + event feed |
| Bottom-right | Weapon tray | Weapon tray |

---

## 1. Measurement method

```python
# UI ink = cyan glow: blue clearly above red, and bright
mask = ((B - R) > 38) & (B > 110)
# reticle is white, not cyan — separate mask:
mask = (max(R,G,B) > 150) & ((max - min) < 70)
# waypoint is amber:
mask = (R>185) & (G>135) & (B<100) & ((R-B)>105) & ((G-B)>55)
```
Then `scipy.ndimage.label` → per-component bounding boxes, filtered by size and aspect.

---

## 2. The elements, at base 1280×720

### Objective block — TOP LEFT
| Part | @1920 | **@base** |
|---|---|---|
| Location label | x65 y122 · 59×3* | **x43.33 y81.33** |
| Banner plate — **1 row** | x65 y125 · 489×62 | **x43.33 y83.33 · 326.00×41.33** |
| Banner plate — **2 rows** | x65 y125 · 489×92 | **x43.33 y83.33 · 326.00×61.33** |

\* the label's mask height is only the glow core; the type is ~10 base cap height sitting above
the plate.

**Structure:** location label sits ABOVE the plate, outside it, no background. The plate holds a
diamond bullet + uppercase objective. The second row is an indented italic sub-objective and adds
**exactly 20 base** to the plate height (41.33 → 61.33).

**We had:** x50 y85 w323 h32/h57. **x and width were close; both heights were wrong.**

### Motion tracker — BOTTOM LEFT
| Part | @1920 | **@base** |
|---|---|---|
| Disc | x64 y854 · **210×180** | **x42.67 y569.33 · 140.00×120.00** |
| Range label `59 m` | x56 y1023 · 12×13 glyphs | **x37.33 y682.00**, cap ≈8.67 |

**Axis ratio 180/210 = 0.857** — the tracker is a **circle drawn in perspective**, tilted out of
plane, not a designed ellipse. The Arena capture gave 0.897 for the same element, so the tilt
varies slightly with FOV but the perspective read is consistent.

**We had:** 122.47×109.83 at (50.38, 575.54) — that is the *multiplayer* tracker. The campaign
tracker is **larger: 140×120 at (42.67, 569.33)**.

### Reticle — DEAD CENTRE
Assault rifle (`hud-2`):
| Part | @1920 | **@base** |
|---|---|---|
| Full extent | x928 y508 · **64×64** | **x618.67 y338.67 · 42.67×42.67** |
| Corner bracket ×4 | 25×25 each | **16.67×16.67** |
| Centre tick — vertical | 2×9 | **1.33×6.00** |
| Centre tick — horizontal | 9×2 | **6.00×1.33** |

Centre = (960, 540) @1920 = **(640, 360) base** — exact screen centre.

**Structure: four corner brackets at the corners of a 42.67 box, plus a small centre cross.**
Not a ring. Our built reticle is a ring with four ticks — that is the *multiplayer* reticle.

**Per-weapon reticles are confirmed.** The second capture's weapon shows a completely different
reticle: four bars forming an open square bracket, 86×86 @1920 = **57.33 base** — a different
shape AND a different size. Reticle must be a variant set keyed to weapon class, not one asset.

### Weapon tray — BOTTOM RIGHT
Overall extent **x1031.33 → 1227, y576 → 680 base**.

| Part | @1920 | **@base** |
|---|---|---|
| Slot pip A | x1712 y864 · 15×15 | **x1141.33 y576.00 · 10.00×10.00** |
| Slot pip B | x1781 y866 · 17×15 | **x1187.33 y577.33 · 11.33×10.00** |
| Grenade icon | x1627 y897 · 33×25 | **x1084.67 y598.00 · 22.00×16.67** |
| Equipment block | x1755 y886 · 76×45 | **x1170.00 y590.67 · 50.67×30.00** |
| Mag digits (`3`,`6`) | x1557,x1577 y963 · 15×21 | **x1038.00, x1051.33 y642.00 · 10.00×14.00** |
| Reserve digits (`1`,`0`) | x1631,x1646 y968 · 12×15 | **x1087.33, x1097.33 y645.33 · 8.00×10.00** |
| Weapon silhouette | x1686 y959 · 141×46 | **x1124.00 y639.33 · 94.00×30.67** |
| Base rule | x1671 y1011 · 170×8 | **x1114.00 y674.00 · 113.33×5.33** |

**Reading order left→right: mag count · reserve count · weapon silhouette.** The silhouette is
on the RIGHT, and it is large (94×30.67) — bigger than either number. Mag digits are **14 base
cap height**; reserve digits are **10** — a 1.4× ratio, so the reserve is visibly subordinate.

The two 10px pips on the top row and the grenade/equipment blocks form the upper band at
**y576–621**; the ammo band is **y639–674**; the rule closes it at **y674**.

### Objective waypoint — WORLD SPACE
| @1920 | **@base** |
|---|---|
| x1034 y324 · 24×23 | **x689.33 y216.00 · 16.00×15.33** |

A compact amber diamond, **16 base**, with the distance readout beneath it.

---

## 3. What our build got wrong

| Element | We built | Measured | Verdict |
|---|---|---|---|
| Objective banner height | 32 / 57 | **41.33 / 61.33** | wrong |
| Motion tracker | 122.47×109.83 @ (50.4, 575.5) | **140×120 @ (42.67, 569.33)** | wrong — used the MP tracker |
| Reticle | ring + 4 ticks | **4 corner brackets + centre cross**, 42.67 | wrong shape |
| Reticle count | one | **one per weapon class** | wrong model |
| Ammo layout | mag · divider · reserve · silhouette | **mag · reserve · silhouette**, silhouette largest | divider unconfirmed |
| Tray x-origin | 940 | **1031.33** | wrong |

---

## 4. Rules that fall out of the measurements

1. **Everything anchors to a screen edge or the exact centre.** The reticle is at (640, 360) to
   the pixel. Nothing floats.
2. **The tracker is a perspective circle**, not an ellipse — build it as a circle with a tilt, so
   the tilt can be tuned without redrawing.
3. **Reticles are a variant set keyed to weapon class**, differing in both shape and size.
4. **Type sizes inside the tray are ratio-locked**: mag 14, reserve 10.
5. **A second objective row costs exactly 20 base**, so the banner must be auto-layout with a
   hug height, not two fixed variants.
6. **The tray reads left→right toward the weapon**, ending on the silhouette — the largest
   element in the group and the one identifying what you are holding.
