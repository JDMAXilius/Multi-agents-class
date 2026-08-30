# Halo reference sourcing & the metric standard (BN35 research)

> 30 Aug 2026, cloud lead. Three parallel research passes answering the
> founder's question: *"find any Halo Infinite level or something in Unreal
> Engine 5 that I could use."* Every claim below carries a source or is
> declared as absent. The cloud proxy blocks most non-GitHub fetches, so
> non-GitHub items are **search-snippet evidence**; GitHub items were cloned
> and read first-hand and are marked as such.

## 0. The one-line answer

**There is no Halo Infinite level you can use.** Not legally, not as files.
What you *can* use is 343's own published movement metrics and a free
first-party UE5 arena greybox (Lyra) as a metric gym. So:

> **POLICY — adopt as law:** Halo is a source of *measured numbers and design
> lessons*, never of geometry, art, audio, or names. **Numbers may enter the
> repo; files may not.**

---

## 1. Legal line (pass 1)

- Microsoft's **Game Content Usage Rules** grant a *personal, non-commercial,
  revocable, non-sublicensable* licence and state plainly:
  *"You can't use Game Content in an app that you sell in an app store."*
  BREACHPOINT is intended to ship. That closes the door.
- **Installation 01** (the fan Halo game, now on UE5) publishes no source and
  no assets. **ElDewrito** was shut down. Neither is a supply.
- Extraction is technically possible — `ekur` (GPL-3.0 Blender importer, has
  an experimental Infinite **level** importer) and `Reclaimer` (reads Infinite
  `.module`) — but yields meshes only: no collision, no materials, no
  gameplay. It requires owning and reading your local install, which breaches
  the EULA's reverse-engineering terms, and **redistributing** extracted
  geometry is straightforward infringement.
- **Verdict:** reference-only, at your own risk, and *nothing extracted ever
  enters this repo*. Measurements derived from looking at something are yours;
  the something is not.

## 2. The metric standard — USE THESE NUMBERS (pass 1 + pass 3)

343's published Forge documentation pins the unit system exactly. This is the
part of the research that is immediately actionable.

| Quantity | Value | In metres | Source | Confidence |
|---|---|---|---|---|
| Forge UI unit | 1 unit = 1 foot | **0.3048 m** | Halo Support, *Community Forge Map Requirements* | solid |
| World unit (series-wide) | 1 wu = 10 ft = 10 Forge units | **3.048 m** | Halopedia *World unit*; c20 Reclaimers | solid |
| Engine→metres factor (Infinite) | `FEET_TO_METER = 0.3048 * 10.0` = 3.048 | — | `ekur/addon/src/constants.py` L23 — **cloned and read** | solid |
| Jump height, no clamber | 8 units | **2.44 m** | Halo Support | solid |
| Comfortable clamber height | 12 units | **3.66 m** | Halo Support | solid |
| Overhead clearance (no head-bump on a jump) | 18 units | **5.49 m** | Forge scaling tutorial + Halo Support | thin (one number, two snippets) |
| Grappleshot range | 80 units | **24.38 m** | Halo Support | solid |
| Spartan sprint speed | — | **8.5 m/s** | Halo Waypoint, *Closer Look: Online Experience* | solid |
| Grapple travel speed | — | **24 m/s** | same | solid |
| Motion tracker radius (arena) | — | **25 m** | Halopedia *Motion tracker* | solid-ish |
| Forge canvas play space | 400 × 400 × 150 | units unstated | GamesRadar / Halo Support (Forge lead M. Schorr) | thin on units |
| Forge grid square | 2 units *vs* 1 wu — **sources conflict** | — | XboxEra / scaling tutorial | do not rely |

### 2.1 Two findings that hit BN33/BN34 directly

**(a) BREACHPOINT is authored in round metres; Halo is authored in feet.**
Our derived numbers are all off Halo's grid:

| Ours | In feet | Nearest design-intent figure |
|---|---|---|
| long axis 52.0 m | 170.6 ft | **170 ft (51.82 m)** or 160 ft |
| short axis 30.0 m | 98.4 ft | **100 ft (30.48 m)** |
| wall height 8.00 m | 26.2 ft | **26 ft (7.92 m)** or 24 ft |
| deck top +4.00 m | 13.1 ft | **12 ft (3.66 m)** — see (b) |
| deck soffit +3.60 m | 11.8 ft | 12 ft |

Nobody designs a map at 170.6 × 98.4 ft. Re-expressing our derived values in
feet and snapping to whole feet is a **free accuracy gain available today**,
before any measurement — it can only move us toward the real map.

**(b) Our +4.00 m decks sit just above the clamber band, and that is a
decision, not an accident.** 12 units = 3.66 m is the *comfortable clamber*
height. At 4.00 m a Spartan cannot mantle a deck; at 3.66 m they can.
`TICKET_BN34` currently states decks are "ramps and Grappleshot only, by
design". If Aquarius' real decks are clamberable, that single 0.34 m is a
gameplay-level fidelity error, not a cosmetic one. **BN34 must settle it by
observation** (§3), and 3.66 m is the more likely truth.

## 3. Aquarius' real dimensions: not published, but measurable (pass 3)

**No published dimension for Aquarius exists in any unit.** ~20 varied queries
across map guides, HCS/callout resources, the uxdesign.cc level-design series,
ArtStation dev portfolios and Forge communities returned no length, width,
ceiling height, deck height or base-to-base distance. The uxdesign.cc article
we lean on (Ketul Majmudar, Jan 2022) is **purely qualitative** — no number
appears in it. Callout maps are unscaled. **No Forge remake with a published
map code was found**, and shipped MP maps still cannot be opened as Forge
canvases. Our 52 m cannot be corrected by citation.

> Noise warning: ExpertBeacon publishes map footprints (Behemoth 172 × 174 m
> etc.). It is an AI-content SEO site, cites no diagram, and 172 m for a small
> 4v4 arena is BTB scale. **Do not use it.** It has no Aquarius figure anyway.

### Ways to measure it, best first — all for BN34

1. **In-game clamber/jump binary search on the actual ledges.** Free, no
   extraction, surprisingly tight. A ledge you can mantle but not jump onto is
   between **8 and 12 units (2.44–3.66 m)**; one you clear with a plain jump is
   ≤ 2.44 m. This alone settles finding (b) above.
2. **Grapple as a tape measure.** The Grappleshot attaches only within
   **24.38 m**. Stand at a callout and back up until the reticle stops
   accepting a surface — that surface is 24.38 m out, ± a step. Two or three
   of these across mid and along the long sightline confirm or kill our 52 m.
3. **Motion tracker as an on-HUD ruler.** A blip at the tracker's edge is
   **25 m** out — good for cross-checking long sightlines in theatre footage.
4. **Timed sprint at 8.5 m/s.** Record a straight base-to-base run at 60 fps,
   count frames, multiply. Systematically over-reads (accel/decel), so it is an
   upper bound and a sanity check, not a final number.
5. **Screenshot calibration against a Spartan** (~2.1 m — and that figure is
   *thin*: 0.7 wu is Halo 1's capsule, not measured in Infinite). ±10–15%.
   This is what we did; it is the weakest rung on the ladder.
6. **Extraction, for completeness and NOT recommended:** `ekur`'s level
   importer dumps one JSON per BSP. Aquarius' internal name is `aquarius`,
   map id `-1628100737` (`ReclaimerFiles/map_ids.txt` line 1 — cloned and
   read); the dump lands at `<data>/levels/aquarius.json` with per-instance
   `position/scale/forward/left/up`. Multiply raw coordinates by **3.048** for
   metres. Exact — and squarely against the EULA. If anyone ever does this,
   **no extracted file may touch this repo**; only scalars measured from it.

**Whatever method wins, snap the result to a 1-foot grid** (§2.1a).

## 4. What to actually use in UE5 (pass 2)

Ranked by what it buys us against what it costs.

1. **UE5 built-in Modeling Mode + CubeGrid** — free, in-engine, no licence
   question. Gets a two-level shell up in an afternoon and has a Stairs
   primitive. *This is the closest thing to a "Halo Forge" we can legally use.*
2. **Lyra Starter Game** — free, UE-only licence, and it ships
   `L_Convolution_Blockout` and `L_ShooterGym`: the only free **first-party
   shipped arena greyboxes** in existence. Best available metric gym — this is
   the reference BN32 should be measured against.
3. **Level Instances + Packed Level Actors** — makes a mirror-symmetric map
   affordable: author one half, edits propagate to both. Directly relevant to
   Aquarius' single axis of symmetry.
4. **Kenney Prototype Textures (CC0)** for greybox surfacing, plus the Fab
   *Limited-Time Free* habit (*Modular SciFi Station*, 147 modular assets, was
   free-to-claim June 2026 — **check the team library, it may already be
   owned**).
5. **One architecture kit, later:** *Modular SciFi: Interiors* (~$39.99;
   4-colour material instances give a clean white Aquarius palette) or the
   Season 1 bundle (~$149.99).
6. **One hydroponics kit, later:** *Sci-Fi Bio Laboratory* or *SciFi
   Hydroponic Lab Environment* for the glass hydro towers.

Also noted: **UE 5.6+ First Person template, Arena Shooter variant** — free,
in-engine, a two-level arena with boost pads and bots, but single-player-shaped
and not server-authoritative; useful to walk, not to build on.
**Skip:** Valley of the Ancient, City Sample, paid FPS multiplayer templates —
all wrong shape or wrong architecture for a GAS/listen-server project.
**The procedural path** (Geometry Script + Python `EditorActorSubsystem`,
JSON → level) is what BN30–BN33 already are; nothing found beats it.

> All Fab prices are search-snippet only — Fab itself is egress-blocked from
> the cloud. Verify before spending.

## 5. What changes because of this

| # | Change | Where | Status |
|---|---|---|---|
| 1 | Adopt the policy line in §0 | this doc, cited from tickets | **done** |
| 2 | Adopt Halo's foot grid as our authoring grid; re-express the level schedule in feet and snap | `gen_aquarius_kit.py` schedule constants | **proposed, not applied** — needs the founder's call, it moves every piece |
| 3 | Settle the +4.00 m deck vs 3.66 m clamber band by observation | `TICKET_BN34` Part B | **added to the ticket** |
| 4 | Measure the long axis with the grapple/clamber methods rather than the Spartan-height method | `TICKET_BN34` Part B | **added to the ticket** |
| 5 | Use Lyra's `L_ShooterGym` as BN32's comparison gym | `TICKET_BN32` | **added to the ticket** |

**Sources** — Halo Support *Community Forge Map Requirements* · Halo Support
*Forge Overview* · TSG Forge Wiki *Movement and Player Character Info* ·
XboxEra *Forge units and scaling* · c20 Reclaimers *Scale and unit conversions*
· Halopedia *World unit* / *Motion tracker* · Halo Waypoint *Closer Look:
Halo Infinite's Online Experience* · Microsoft *Game Content Usage Rules* ·
uxdesign.cc Majmudar *…level design series: AQUARIUS* · GitHub
`TheHaloArchive/ekur`, `TheHaloArchive/ReclaimerFiles`,
`Coreforge/blender-halo-infinite` (read first-hand) · GamesRadar+ Forge canvas
size · ArtStation Aquarius breakdowns (Kirchstein, Feddeler, Gasorntip, Lewis,
Kao; LD Tyler Ensrude, map lead Shawn Priester).
