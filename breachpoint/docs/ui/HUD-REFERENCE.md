# HUD reference — measured, and what is still missing

**Status:** in progress, 2 Aug 2026. The first HUD attempt was **invented rather than extracted**
and has been deleted. This file records only what is measured from real captures, and states
plainly what is not yet sourced.

**Why the first attempt failed:** the reference Figma file is front-end only, so I inferred the
HUD from `UI-DESIGN-SYSTEM.md` §5 instead of measuring it. Two of those inferences are now
contradicted by official documentation (below). Inference is not extraction — if it isn't
measured, it doesn't go in.

---

## 1. Sources

| Source | Gives | Status |
|---|---|---|
| `interfaceingame.com/wp-content/uploads/halo-infinite/halo-infinite-hud-2.png` etc. | **1920×1080 campaign HUD captures** — exactly **1.5×** our 1280×720 base, so every measurement divides cleanly with no estimation | **Downloaded, measured.** Fetch needs a browser UA **and** a `Referer` header of the screenshot page; WebFetch is 403 |
| Halo Support — Shields in Halo Infinite | Shield / health bar behaviour and position | **Official** |
| 343 "Inside Infinite" Oct 2021 | VISR OS two-tier colour system | **Sourced** |
| `gameuidatabase.com` | would have supplied multiplayer HUD | **Blocked — Cloudflare, 403 on every path** |

**The campaign captures do not show the multiplayer HUD.** Shield bar, health bar, score, match
timer, medal feed and scoreboard are all Arena-only and are **not yet measured**.

---

## 2. Corrections forced by official documentation

| I had assumed | Actually |
|---|---|
| Vitals top-**left** | Shield is a light-blue bar at the **TOP**; health is a **smaller yellow bar BELOW it**, visible only after damage |
| Score / clock **top-centre** | Score counter and timer are **BOTTOM MIDDLE** in Arena multiplayer |
| One reticle asset | **Reticle shape varies by weapon class** — the AR and the plasma weapon in the captures have visibly different reticles |
| Ammo is always `mag / reserve` | **Energy weapons show a battery percentage** (`100%`) instead |

`UI-DESIGN-SYSTEM.md` §5's deliberate deviations (vitals top-left, killfeed top-right, no motion
tracker) were written as *deviations from Campaign Evolved*, not as claims about Infinite. They
remain design decisions — but they are decisions, not 1:1, and the file must not pretend otherwise.

---

## 3. Measured elements (1920 → ÷1.5 → base)

| Element | Measured @1920 | Base 1280×720 | Notes |
|---|---|---|---|
| **Objective banner** | x75 y128 w485 h48 | **x50 y85 w323 h32** | Cyan 1px border, blue gradient plate fading top→bottom, diamond bullet, uppercase. Two-row variant adds an indented italic sub-objective |
| **Location label** | y100 | **y67** | Uppercase, white, wide tracking, **no plate** — sits above the banner, outside it |
| **Motion tracker** | centre (175,935) d200 | **centre (117,623) d133** | Concentric rings + cardinal ticks + centre chevron; radial cyan falloff. Range readout `59 m` below-left in amber |
| **Weapon tray ammo** | baseline y978, x≈1560 | **y652, x≈1040** | `36 | 108` — large cyan current, dim reserve after a thin divider |
| **Weapon silhouette** | x1700–1830 | **x1133–1220** | Original art required |
| **Equipment pips** | row above ammo, y≈890 | **y≈593** | Grenade count and equipment count in small bordered slots |
| **Reticle** | d40 | **d27 inside a 40 box** | Ring + four ticks + rotated centre pip |
| **Toast** | x1450 y540 w470 h175 | **x967 y360 w313 h117** | Cyan border, solid blue header bar, body plate, inline key glyph |
| **Waypoint diamond** | 24 | **16** | Amber outer, white inner |
| **Interaction prompt** | x805–1105 y765 | **x537–737 y510** | Circular key glyph + uppercase verb, centred below the reticle |
| **Subtitle** | centred bottom | — | Black plate hugging the text, speaker name in a per-speaker colour |

### Built in Figma (page `HUD / Elements`)
Objective Banner (2 variants) · Location Label · Toast · Waypoint · Subtitle · Motion Tracker ·
Reticle (3 states) · Weapon Tray.

### NOT yet built — needs multiplayer reference
Shield bar · Health bar · Score counter · Match timer · Grenade type/count detail · Equipment
cooldown + uses-remaining · Scope/ADS overlay · Killfeed · Medal feed and medal art · Damage
direction indicator · Teammate nameplates · Energy-weapon battery readout · Scoreboard (TAB) ·
Death/respawn screen · Pause menu.

---

## 3b. MEASURED FROM REAL GAMEPLAY — these supersede §3's estimates

A pixel-measurement pass was run against two candidate sources. **One turned out to be a fan
recreation and was rejected.**

> ⚠️ **`HINF_HUD.png` (halo.wiki.gallery, 3840×2160, "isolated HUD on black") is a rendered HTML
> mockup, not a game capture** — the directory it came from contains matching `.html` files. It
> agrees with real gameplay on the shield bar to within 0.7 px, but **diverges badly elsewhere**:
> tracker major diameter +14.7 %, tracker rotation off by 14.6°, ticks at 45° instead of at
> 12/3/6/9, and it omits the score bar and the entire ammo block. **Do not measure from it.**
> It is only safe as corroboration for the shield bar.

Everything below is from **`mp_overview.jpg` — real Halo Infinite gameplay, 1920×1080 ÷ 1.5.**

| Element | Measured @base-1280 | Notes |
|---|---|---|
| **Shield bar** | **x503.33 y66.00 w273.33 h20.00** | Centre x **639.67** = screen centre. Fill peak `#AFD5FC` |
| **Shield geometry** | constant thickness ≈16, sag ≈2.7 | **It is an ARC, not a trapezoid.** Halopedia's prose says "trapezoidal space"; the pixels say a constant-thickness arc with a slight downward sag. **Geometry beats prose.** |
| **Health bar** | slot ≈ x503 y86 w273 h5 | Not rendered in any capture found — every shot has full shields. Slot inferred, height is an estimate, **flagged as unmeasured** |
| **Score / timer bar** | **x474.67 y622.00 w302.00 h22.00** | mode icon · team bar · score · `|` · timer · `|` · score · team bar · mode icon |
| **Motion tracker** | centre **(111.61, 630.45)**, **122.47 × 109.83**, rotation **−9.3°** | Axis ratio 0.897 — this is a **circle in perspective**, tilted out of plane, not an ellipse by design. Spokes at 12/3/6/9. Range label `20 m`, map callout beside it |
| **Grenade / equipment panel** | ≈ x1125–1179, y591–620 | Visual read, ±3 px — JPEG artefacts over textured ground defeated edge detection. **Flagged, not presented as measured** |
| **Ammo block** | band ≈ y630–680 | |
| **Timer digits** | cap height 8.0–8.67 | Same size class as the mockup's `18 m` label |

**Caveat that applies to every stroke figure:** the HUD renders with heavy bloom, so measured
stroke weights are FWHM of the glow profile and **overstate a Figma stroke by roughly 1.5–2×**.
Halve them when rebuilding.

**Still unmeasured after this pass:** reticle geometry per weapon class · hitmarker art ·
low-ammo colour state · medal-feed anchor · health bar height · fill opacity vs fill colour
(captures are already composited on black, so alpha cannot be separated from base colour).

---

## 4. What Infinite removed, and why the layout looks like it does

343 removed the **helmet interior entirely** in 2021 and collapsed grenades + weapon + equipment
into **one bottom-right tray**, leaving the shield bar alone at the top. That is the whole
explanation for the layout: there is no visor frame to hang elements on, so everything migrated to
two corners and one top edge.

**This is the opposite direction from Campaign Evolved (2026)**, which restored the diegetic visor
and spread elements across four corners. `UI-DESIGN-SYSTEM.md` §1 already records this
contradiction — *the newer game is the more skeuomorphic one* — and it is why "replicate Halo
one-to-one" has no single answer for the HUD specifically.

---

## 5. Motion tracker — decided, and a correction to the record

**Founder decision, 2 Aug 2026: the motion tracker IS in scope**, built 1:1 with Halo Infinite.

**Correction.** An earlier version of this file (and `UI-DESIGN-SYSTEM.md` §5) claimed the
tracker "conflicts with closed ruling R12". That was wrong and should not be repeated:

- **R12 is "Bots are legible before they are optimal"** — a bot-AI ruling. It says nothing about
  radar. It was cited by *analogy* in a design doc, then re-read as if it were a prohibition.
- **No closed ruling ever banned a motion tracker.** Nothing in `DESIGN-RULINGS.md` needed
  reversing, and CLAUDE.md law 8 was never engaged.
- The **GDD §2.8 "no privileged state"** citation is also weak: a tracker shows the *same*
  information to every player, which is the opposite of privileged state.

The genuine argument against it was a design opinion — in a 4v4 arena with callouts as a pillar,
a radar substitutes for communication. The founder has weighed that and decided against it.

**Behaviour to build (all officially sourced):** Arena 18 m precise blips / 30 m edge-direction;
BTB 24 m / 40 m. Detects walking, sprinting, shooting, vehicles. Does **not** detect crouch-walk
or the Walk binding at reduced throttle. Edge-direction is non-Ranked only. Enemies render in the
Enemy UI Colour, allies in the Friendly UI Colour — both user-configurable. Disabled in Ranked
and Tactical Slayer. Threat Sensor forcibly places targets on it; Shroud Screen hides from it.

---

## 6. Fetch recipe (for whoever needs more captures)

```bash
UA="Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36"
curl -sSL -A "$UA" -e "https://interfaceingame.com/screenshots/halo-infinite-hud-2/" \
  "https://interfaceingame.com/wp-content/uploads/halo-infinite/halo-infinite-hud-2.png" -o hud2.png
```

Both the UA **and** the `-e` referer are required. Strip any URL containing a `-WxH` suffix — those
are thumbnails. `gameuidatabase.com` will not work by any method; it needs a manual export.
