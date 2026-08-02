# Measured motion — real timings extracted from reference animation

**Method.** Six animated GIFs decoded frame-by-frame with PIL/numpy. Per-frame change signal =
mean absolute RGB difference from the previous frame; peaks and plateaus in that signal are the
phase boundaries. Translating/scaling elements measured by thresholding bright pixels and tracking
the bounding box or the bright-run width on a fixed row band. Easing determined by normalising the
measured position/size series to 0..1 and brute-force fitting a cubic-bezier over a 0.05/0.1 grid,
reporting RMSE against that fit and against the standard named curves.

**Provenance.** Source material is 343 Industries / Microsoft. Used as a *measurement* reference
only. Durations, frame counts, stagger intervals and easing curves are facts about motion and are
what this document records. No artwork is reproduced or derived here.

**Correction to the intake table.** Every 30 ms GIF holds its **last frame for 1000 ms** (an export
setting, not authored motion). Actual playback totals therefore differ from `frames × 30 ms`.
`VISR+Component+Install.gif` additionally carries four *deliberate* internal holds. Both the
authored duration and the encoded duration are given below; **use the authored one**.

| file | frames | authored (frames×base) | encoded playback | delta explained by |
|---|---|---|---|---|
| Eagle+Loader | 60 | 1800 ms | 2770 ms | +1000 ms tail hold on f59 |
| Objective+Complete | 121 | 3630 ms | 4600 ms | +1000 ms tail hold on f120 |
| Mission+Complete | 231 | 6930 ms | 7900 ms | +1000 ms tail hold on f230 |
| VISR+Component+Install | 301 | 9030 ms | 12870 ms | holds on f112 (330), f175 (330), f244 (1330), f276 (1000), f300 (1000) |
| Shader+Cache | 96 | 9600 ms | 9600 ms | — |
| Narrative+Slideshow | 3 | 4500 ms | 4500 ms | — |

---

## Headline

| animation | total (authored) | phases | loop? | dominant easing |
|---|---|---|---|---|
| **Eagle Loader** | 1800 ms | 8 | GIF loops, **content does not cycle** — it is a one-shot sting replayed | `cubic-bezier(0.50, 0.00, 0.15, 1.00)` (RMSE 0.008) |
| **Objective Complete** | 3630 ms | 8 | no | `cubic-bezier(0.60, 0.20, 0.10, 1.00)` (RMSE 0.007 in, 0.005 out) |
| **Mission Complete** | 6930 ms | 13 | no | `cubic-bezier(0.45, 0.30, 0.00, 1.00)` (RMSE 0.008) |
| **VISR Component Install** | 9030 ms + 3840 ms of holds | 14 | no | `cubic-bezier(0.30, 0.20, 0.15, 1.00)` (RMSE 0.009) |
| **Shader Cache** | 9600 ms | **0 — no phase structure** | no | **not measurable — pre-rendered cinematic, see §5** |
| **Narrative Slideshow** | 4500 ms | 3 static slides | no | **none — 3 still frames, no motion exists** |

### The single most important result: one house curve fits everything

Four independent scale/translate series across three different assets all fit the **same** curve
family — a small hold at the start, then a hard flat landing:

| series | asset | best fit | RMSE |
|---|---|---|---|
| X-cross expand (half-width) | Eagle Loader f7–16 | `cubic-bezier(0.50, 0.00, 0.15, 1.00)` | 0.0077 |
| banner fill expand (width) | Objective Complete f36–47 | `cubic-bezier(0.60, 0.20, 0.10, 1.00)` | 0.0070 |
| banner fill collapse (width, time-reversed) | Objective Complete f106–117 | `cubic-bezier(0.60, 0.20, 0.10, 1.00)` | 0.0049 |
| sub-banner fill expand (width) | Mission Complete f39–48 | `cubic-bezier(0.45, 0.30, 0.00, 1.00)` | 0.0081 |
| chip left-edge slide (x) | VISR Install f27–39 | `cubic-bezier(0.30, 0.20, 0.15, 1.00)` | 0.0085 |

A single token **`cubic-bezier(0.45, 0.15, 0.10, 1.00)`** fits all five at mean RMSE **0.066**,
versus 0.140 for `ease-out` cubic `(0.33,1,0.68,1)`, 0.191 for `ease-in-out (0.42,0,0.58,1)` and
0.201 for linear. That is a 2–3× improvement and it is one curve, not five. Adopt it as
`Motion.Ease.Standard`.

### Secondary results reused across assets

- **Glitch flicker is a 2-frame alternation → 60 ms period, 16.7 Hz.** Identical cadence in Eagle
  Loader f41–50, Objective Complete f47–52 and Mission Complete f35–38. Not a fade; a hard toggle
  between two states on consecutive frames.
- **Staggered list reveal = 150 ms per beat** (Mission Complete rewards, entries at f61 / f66 / f70).
- **The container frame does not animate.** In Objective Complete the outer rectangle stroke goes
  from 43 px to its full 278 px in **one frame (30 ms)** — a hard cut. Only the *fill* is eased.
  This is a structural rule, not a timing: animate the fill, snap the frame.

---

## 1. Eagle Loader — 750×500, 60 frames @ 30 ms, authored 1800 ms

**What it actually is (§6 answer).** Not a rotation, not a sweep, not a pulse. It is a **sequence**:
a thin vertical seed tick → an X-cross that expands outward from centre → the UNSC eagle emblem
materialises inside it → a 2-frame glitch that dematerialises it → black. The GIF is flagged
`loop=0` (infinite) and first and last frames are both black (`wrap` diff = 0.045), so it *replays*
cleanly, but there is no cyclic sub-window: the bright-pixel count rises monotonically to 5695 and
then collapses to 0 exactly once.

**Loop period: 1800 ms authored** (60 frames × 30 ms), 2770 ms as encoded. The extra 970 ms is the
tail hold, not a designed pause. A GIF frame duplicates every 5th frame (indices 1, 5, 10, 15, 20,
25, 35, 40, 45, 50), so the encode carries 48 unique frames — effective 26.7 fps, giving a
**±37.5 ms confidence on every boundary in this file**.

| phase | frames | ms | what happens | easing |
|---|---|---|---|---|
| black | 0–1 | 60 | nothing | — |
| seed | 2–6 | 150 | 1 px vertical tick at (375, 250), 100 px tall, brightening | linear brighten |
| cross expand | 7–16 | **300** | X-cross scales out from half-width 7.5 → 97.5 px about centre (374.5, 250) | `cubic-bezier(0.50, 0.00, 0.15, 1.00)`, RMSE 0.008 |
| cross settle / thin | 17–21 | 150 | bbox frozen at max, bright count decays 465 → 89; the cross thins to hairlines | ease-out |
| emblem materialise | 22–35 | **420** | eagle emblem resolves inside the cross; count 962 → 5695, bbox contracts 264→257 wide | ease-out, saturating by f33 |
| settle pulse | 36–40 | 150 | count oscillates 4132 ↔ 5654 | 2-frame alternation, 60 ms |
| glitch dematerialise | 41–51 | **330** | emblem strobes between full (≈4000 px) and laurel-only (≈590 px), then drops to 0 | hard 2-frame toggle, no interpolation |
| black | 52–59 | 240 (+1000 ms export hold) | — | — |

**Direct answer to the "4-frame loading sprite" question.** This asset is **not a spinner and
cannot replace one.** A loading indicator must be rotationally or cyclically symmetric so it reads
the same whenever it is cut off. This one has a beginning, a middle and an end, and spends 240 ms
of its 1800 ms period fully black. If you loop it as a busy-indicator the user sees the UI go dark
once a second. What it *is* the correct reference for is a **branded load-screen sting** — play
once at load-screen entry, then run a separate spinner.

---

## 2. Objective Complete — 750×500, 121 frames @ 30 ms, authored 3630 ms

The cleanest, most reusable measurement in the set. It is the in-match "OBJECTIVE COMPLETE" banner:
a thin outlined rectangle whose solid cyan fill scales out from the centre.

| phase | frames | ms | what happens | easing |
|---|---|---|---|---|
| ambient rise | 0–6 | 210 | background bloom fades up; no UI geometry yet | linear |
| marker | 7–29 | **690** | a 16×17 px diamond glyph at centre, subtly pulsing (max lum 190 → 255) | slow pulse |
| frame snap | 29→30 | **30** | outer rectangle stroke jumps 43 px → **278 px** wide in a single frame | **hard cut, no tween** |
| fill expand | 36–47 | **330** | solid cyan fill scales out from 8 px → 275 px about centre x=374.5; widths 8, 17, 36, 69, 137, 205, 239, 256, 264, 269, 271, 275 | `cubic-bezier(0.60, 0.20, 0.10, 1.00)`, RMSE 0.007 |
| impact flicker | 47–52 | **180** | count alternates 10899 ↔ 10627 every frame | 2-frame toggle, 60 ms period |
| hold | 53–105 | **1590** | text legible and static; residual shimmer (mean delta 0.19) | — |
| fill collapse | 106–117 | **330** | mirror of the expand: 271 → 1 px, halving each frame in the tail | `cubic-bezier(0.60, 0.20, 0.10, 1.00)`, RMSE 0.005 |
| black | 118–120 | 90 (+1000 ms export hold) | — | — |

**The in and out are the same curve and the same duration.** Fitting the time-reversed collapse
gives an identical control-point set to the expand, to within the grid resolution. This is a
symmetric transition, not an in/out pair with different curves.

**Total on-screen life of the banner: 36 → 117 = 2460 ms**, of which 1590 ms (65%) is the readable
hold. Read time dominates; the transitions are 27% of the budget.

---

## 3. Mission Complete — 1800×600, 231 frames @ 30 ms, authored 6930 ms

The post-match summary. It reuses the *same* cyan fill component as Objective Complete at a larger
size, which is why its numbers corroborate rather than contradict.

| phase | frames | ms | what happens | easing |
|---|---|---|---|---|
| title expand | 0–12 | **360** | "MISSION COMPLETE" + eagle expand about centre (900, 300); bbox 316 → 450 px wide, 119 → 224 px tall | ease-out |
| glitch pass | 13–20 | **240** | count drops to 497 and 232 on f13/f16 — a two-beat scanline wipe over the title | 2-frame toggle |
| title settle | 21–38 | 540 | count 3859 → 12380; sub-banner outline appears beneath the title | ease-out |
| sub-banner fill | 39–48 | **270** | cyan fill expands 51 → 478 px about centre; widths 51, 98, 238, 378, 437, 462, 472, 476, 478 | `cubic-bezier(0.45, 0.30, 0.00, 1.00)`, RMSE 0.008 |
| banner flicker | 48–54 | 180 | count oscillates 11220 ↔ 23422 | 2-frame toggle, 60 ms |
| banner text settle | 55–60 | 180 | count settles to 28818 | ease-out |
| **reward stagger** | 61–75 | **450** | reward tiles and labels enter in **3 beats: f61, f66, f70 → 150 ms stagger**; block y-extent grows 385 → 504 px | per-item ease-out, staggered |
| hold A | 76–114 | **1170** | full summary readable, static (mean delta 0.10–0.2) | — |
| section swap | 115–130 | **480** | lower block content changes; y-extent drops 504 → 329 then rebuilds to 397 | ease-in-out |
| hold B | 131–200 | **2100** | second summary state readable | — |
| decay begin | 201–215 | 450 | glow and count start dropping, geometry still full-size | ease-in |
| banner collapse | 210–223 | **390** | cyan fill collapses 478 → 1 px about centre, halving in the tail (149, 58, 22, 7, 2, 1) | mirror of the expand |
| fade out | 216–229 | 420 | remaining elements fade; count 20219 → 174 | ease-in |
| black | 230 | (+1000 ms export hold) | — | — |

A frame duplicates every 5th frame here too (4, 9, 14, 19, 24, …) — same **±37.5 ms** caveat.

**The useful number here is the 150 ms stagger.** Three reward elements enter 5 frames apart. That
is the measured cadence for any list, grid or carousel that populates.

---

## 4. VISR Component Install — 750×500, 301 frames @ 30 ms, authored 9030 ms

A green terminal/diegetic-HUD piece: a "VISR" chip animates in, then lines of terminal text type on,
then an install progress bar runs and the whole thing glitches out. **Every frame is unique** (no
duplicates) — this is the highest-fidelity file in the set, and its four deliberate internal holds
are authored beats, not export artifacts.

| phase | frames | ms | what happens | easing |
|---|---|---|---|---|
| dark scene | 0–11 | 360 | ambient background only, no UI above threshold | — |
| chip grow | 12–26 | **450** | "VISR" chip appears at x≈38 as 1 px and grows to a 57 px-wide plate | ease-out |
| chip slide | 27–39 | **360** | chip's left edge slides 43 → 102 px while its right edge stays pinned at 221 — the plate *narrows in place*; x = 43, 51, 67, 82, 91, 96, 99, 100, 101, 102 | `cubic-bezier(0.30, 0.20, 0.15, 1.00)`, RMSE 0.009 |
| hold | 39–65 | **780** | chip at rest, subtle noise | — |
| chip dissolve | 66–95 | **900** | count decays 877 → 70; chip breaks up to a residual stub | ease-in with noise |
| **type-on** | 96–112 | **510** | "[UPGRADING] MJOLNIR SUIT" reveals left-to-right; reveal edge advances 391 → 641 px = **15.6 px/frame ≈ 520 px/s ≈ one 22 px glyph every 42 ms** | **linear** — no easing; constant-rate reveal |
| beat | 112 | **330 (authored hold)** | line sits before the flash | — |
| glitch flash | 113–120 | 240 | count spikes 2887 → 5227 → 3300 | 2-frame toggle |
| hold | 121–174 | 1620 | terminal block readable | — |
| beat | 175 | **330 (authored hold)** | — | — |
| hold | 176–243 | 2040 | second readable beat | — |
| **read beat** | 244 | **1330 (authored hold)** | the longest authored pause in the set, immediately before the payoff | — |
| install bar | 245–256 | **360** | progress bar fills in **4 discrete steps** (5 → 17 → 141 → 269 → 280 px of a 300 px track) at f246 / f248 / f251 / f253 | **stepped, not tweened — see caveat** |
| payoff glitch | 257–265 | **270** | the largest deltas in the whole file (2.76, 4.14, 3.98, 4.50, 4.57, 4.77, 3.82); the bar blinks fully off on f257, f260, f262, f265 | hard toggle |
| residual + second ramp | 266–278 | 390 | count 70 → 1471 | ease-out |
| flicker | 279–285 | 210 | 2-frame toggle | — |
| decay | 286–297 | 360 | count 66 → 46 → 0 | ease-in |
| black | 298–300 | 90 (+1000 ms export hold) | — | — |

**Caveat on the progress bar.** It advances in four unequal jumps roughly 60–90 ms apart. That is a
*scripted fake-progress* animation authored for a trailer, not a data-driven bar. **Do not copy its
step timings into a real loader** — a real bar is driven by actual completion. What *is* valid to
take is the total: a scripted install beat reads as complete in **360 ms**.

**The type-on rate is the strongest single number in this file: 15.6 px/frame, linear, no easing.**
It is genuinely constant across 16 frames — a text reveal must not be eased.

---

## 5. Shader Cache — 1500×844, 96 frames @ 100 ms — **NOT a valid timing source**

Say this plainly: this is a **pre-rendered 3D cinematic**, not a UI animation, and it must not be
used to derive any real-time widget timing.

The evidence is quantitative, not impressionistic. The change signal sits between **8.1 and 11.8**
mean-abs-diff for frames 1–90 and never drops below 8.1 until the closing fade at f90–95
(8.54 → 7.00 → 6.79 → 6.20 → 5.25 → 4.90). For comparison, Objective Complete's *hold* phase reads
0.19 and its most violent transition reads 1.5. There is no frame anywhere in Shader Cache where
the image is stable. That is the signature of continuous camera motion over a rendered scene:

- **Phase count: 0.** No in, no hold, no out. Nothing starts or stops.
- Visually it is a slow aerial flythrough of a landscape with hex-tessellated "streaming-in" tiles
  resolving across the terrain — a rendered environment, not composited UI layers.
- The 100 ms frame interval is an **export decimation choice**, not the source frame rate. At 10 fps
  the file cannot resolve any transition shorter than 100 ms, which is shorter than every transition
  measured in the other four files. Any easing curve fitted to it would be an artefact of the
  sampling.

The only defensible facts: the shot is **9.6 s long** and ends with a **600 ms fade** (f90–95). Both
are film-editorial numbers. Neither transfers to a widget.

---

## 6. Narrative Slideshow — 2238×1258, 3 frames @ 1500 ms — **no motion exists**

Three static designed screens (a VISR terminal "PASS PHRASE / [REQUIRED]" panel and two siblings)
held for 1500 ms each. There are no in-between frames, so there is nothing to measure: no transition,
no easing, not even a crossfade. The change signal has exactly two non-zero entries (2.59 and 1.78),
which are the whole-screen content swaps.

**The one fact it yields: a 1500 ms dwell per slide** for an auto-advancing narrative panel. That is
a real number and it is worth having. Everything else about how these slides transition is unknown
and must not be guessed from this file.

---

## 7. Replaces our guesses

Two things to state honestly before the table:

1. **There is no `MOTION-SPEC.md` in this repo.** Grepping every `.md` under `breachpoint/` for
   `ms`, `ease`, `easing`, `duration`, `animat` returns only performance budgets (frame time, tick
   rate, RTT) — no UI motion timings anywhere. The "guesses" being superseded are therefore the
   *unquantified assertions* in `SCREEN-BUILD-SPEC.md` §4 and §7, plus the implicit defaults any
   implementer would have reached for. Each row below names its source line.
2. Where the reference genuinely does not cover one of our claims, the row says so rather than
   inventing a number.

| our claim | source | old guess | measured replacement |
|---|---|---|---|
| "**4-frame Loading Icon** — the sprite animation is authored in the source; drive at a constant rate." | `SCREEN-BUILD-SPEC.md` §7 L195 | 4 frames, constant rate, rate unspecified | **Wrong asset class.** The reference loader is a **60-frame, 1800 ms one-shot sting** (seed → cross expand 300 ms → emblem materialise 420 ms → glitch out 330 ms → 240 ms black), not a cycling sprite. It is 13% black by duration, so it cannot loop as a busy-indicator. Use it as a **load-screen entry sting**; author a separate spinner for the busy state. |
| "**`Searching` / `Searching 2`** on the Load Bar — a looping two-state search animation." | `SCREEN-BUILD-SPEC.md` §7 L196 | two-state loop, period unspecified | **Not covered by these GIFs.** The only two-state alternation measured here is the **60 ms / 16.7 Hz glitch flicker** (three independent assets agree). That is a damage/impact effect, not a searching toggle — 16.7 Hz would strobe. Keep this one open; do not adopt 60 ms for it. |
| "The panel grows and shrinks **symmetrically about screen centre**. Animate height and y together." (material-picker accordion) | `SCREEN-BUILD-SPEC.md` §4 L117 | symmetric centre-anchored, **no duration, no curve** | **330 ms in, 330 ms out, identical curve both ways** — `cubic-bezier(0.60, 0.20, 0.10, 1.00)`. Measured on the centre-anchored fill expand/collapse in Objective Complete (RMSE 0.007 / 0.005); corroborated at 270 ms / 390 ms for the wider Mission Complete banner. |
| "**Reroll affordance** — on focus, the Challenge Card narrows (511.5 → 461.5) and a 40×40 swap button slides in to its left." | `SCREEN-BUILD-SPEC.md` §7 L189 | geometry known, **timing entirely unspecified** | **360 ms, `cubic-bezier(0.30, 0.20, 0.15, 1.00)`** — measured on the VISR chip, which performs exactly this move: one edge pinned, the other edge sliding to narrow the plate in place (43 → 102 px, RMSE 0.009). |
| "**Horizontal reward carousels** — tile rail and chip rail pan **in lockstep** at pitch 130 / 260." | `SCREEN-BUILD-SPEC.md` §7 L186 | lockstep pan, **no per-item timing** | **150 ms stagger per item** — Mission Complete's reward block enters in three beats 5 frames apart (f61 / f66 / f70), total block reveal 450 ms. |
| Implicit default for any panel/banner transition | — | the reflex `0.2s ease-in-out` / `0.3s ease` | **`cubic-bezier(0.45, 0.15, 0.10, 1.00)`** — one curve fitting all five measured series at mean RMSE 0.066, vs 0.191 for `ease-in-out (0.42,0,0.58,1)` and 0.201 for linear. `ease-in-out` is measurably the *wrong shape*: the reference lands hard and flat, it does not decelerate symmetrically. |
| Implicit default for a container/border reveal | — | fade or grow the whole component together | **The frame does not animate.** Objective Complete's outer rectangle stroke goes 43 px → 278 px in **one frame (30 ms)**. Snap the container, ease only the fill. |
| Implicit default for text reveal | — | fade in, or an eased type-on | **Linear, 15.6 px/frame (≈520 px/s, ≈42 ms per 22 px glyph)** — VISR type-on is constant-rate across 16 frames with no easing at either end. |
| Banner read time (how long a toast/banner stays up) | not specified anywhere | unspecified | **1590 ms hold**, 2460 ms total on-screen life (Objective Complete). Transitions are 27% of the budget; the hold is 65%. |
| Auto-advancing narrative panel dwell | not specified anywhere | unspecified | **1500 ms per slide** (Narrative Slideshow). Transition between slides is **unknown** — the file has no in-between frames. |
| Progress-bar fill timing | not specified anywhere | unspecified | **Do not take a number from the reference.** VISR's bar advances in 4 scripted jumps over 360 ms — that is trailer choreography, not a data-driven bar. Only the *total read* (≈360 ms for a completion beat) transfers. |
| Full-screen loading background | not specified anywhere | unspecified | **No timing available.** Shader Cache is a pre-rendered cinematic (§5) and is not a valid source for any real-time widget number. |

### Confidence and limits

- **Boundary precision.** Eagle Loader and Mission Complete duplicate every 5th GIF frame, so their
  effective sample rate is 26.7 fps and every boundary carries **±37.5 ms**. Objective Complete has
  irregular duplicates; VISR has none and is exact to **±30 ms**.
- **What "bright-pixel count" measures.** Where a phase is reported from pixel count rather than
  geometry (emblem materialise, fade-outs), the number is the duration of the *luminance* change and
  is trustworthy as a duration but is a weaker basis for a curve than the width/position series. All
  five curve fits above are taken from geometry (width or edge x), not from pixel count.
- **Ambiguity that was not resolved.** Mission Complete's f115–130 "section swap" is a genuine
  ambiguity: the change signal is low (0.2–0.4) but the lower-block extent moves 504 → 329 → 397 px.
  It is either a content swap or a second staggered reveal. 480 ms is the duration of *something*
  there; the phase label is inferred, not measured.
- **Nothing here is a claim about the source's implementation.** These are observed durations and
  fitted curves for a rendered result. The reference may have been authored in a timeline tool with
  different internal parameters that happen to produce this output.
