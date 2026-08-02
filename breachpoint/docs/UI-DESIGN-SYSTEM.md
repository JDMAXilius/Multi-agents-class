# BREACHPOINT — UI Design System

**Status:** v1, 2 Aug 2026. Companion to the `ui-presentation` skill (that is the *method*;
this is the *reference table*). Founder's reference decision is in `docs/UI-REFERENCE.md`.
Binding law: `CLAUDE.md` laws 3/4/5/7 and `R18`/`R26`.

**Every number below is measured, not designed.** Geometry came out of the reference Figma
file's node metadata through the Figma MCP; colour semantics came from 343's published VISR OS
writeup. Where something is our own decision it says so.

---

## 1. Sources, and how much to trust each

| Source | What we took | Confidence |
|---|---|---|
| Figma community file `Kn87U5sy2VD0lP8K7h4LcQ` — "Halo Infinite UI Rework" | All front-end geometry in §3; component inventory in §4 | **Measured.** Node metadata, exact px |
| Inside Infinite, Oct 2021 (343) | VISR OS two-channel colour; the tier rule; "helmet removed, single bottom-right tray" | **Sourced.** 343's own words |
| Eric Dies, UI + Realization Lead, 343 | "Presentation" = UI + the in-world scenes that host it; design-system-first | **Sourced** (portfolio summary; the page itself is blocked by this environment's network policy) |
| Halo Support — Shields / Equipment in Infinite | Health bar is yellow and appears only after damage; shield break flashes red + low tone; equipment cooldown refills, prints uses remaining | **Sourced.** Official |
| GamesRadar / PlayStation.Blog on Campaign Evolved | Vitals top-centre, tracker top-right, ammo bottom-right, grenades bottom-left, diegetic visor | **Sourced** |
| Stroke weights, corner radii, typeface proportions | — | **NOT sourced.** Ours |

**The two references contradict each other and that is a real finding.** Halo Infinite (2021)
removed the helmet interior entirely and collapsed grenades + weapon + equipment into one
bottom-right tray, leaving only the shield bar at the top. Campaign Evolved (2026) restored the
diegetic visor and spread elements across four corners with vitals top-centre. **The newer game
is the more skeuomorphic one.** "Replicate Halo one-to-one" therefore has no single answer;
§5 records which philosophy each Breachpoint surface follows.

---

## 2. Colour tokens

Semantic, not decorative. **A token means one thing.** Defined once (see `ui-presentation` §9)
and read by every widget — never typed into a WBP.

| Token | Hex | Meaning — and only this |
|---|---|---|
| `Shield` | `#35D0F2` | You: your shields, your team, reticle at rest, grapple ready |
| `ShieldLow` | `#0E7E9B` | Shield bar gradient floor |
| `Health` | `#F5C542` | Health beneath shields. **Hidden until damaged** |
| `Amber` | `#FFA333` | A clock is running: rocket countdown, medals, host authority |
| `Enemy` | `#FF4A3D` | Threat: enemy reticle, incoming damage, shield-break flash |
| `TeamThem` | `#FF7A45` | Opposing team in lists, feeds, scoreboards |
| `SelfWhite` | `#FFFFFF` | You in a list of people; header bars |
| `Void` | `#05080C` | Ground — near-black with a blue bias, deliberately not neutral grey |
| `Deep` | `#0A1018` | Panel ground |
| `Edge` | `#1E2C3A` | Hairline borders |
| `InkDim` | `#8397A9` | Secondary text |
| `Dead` | `#4A5A6B` | Disabled / unavailable |

**Rules that come with the palette:**
- Red is a threat channel. Not for non-lethal warnings, not for UI errors.
- Health is yellow, never green — green reads "fine" when the player is one shot from dead.
- Cyan means *you*. One hue, one meaning, across HUD, feed, scoreboard and lobby.

---

## 3. Grid and geometry (base 1280×720; ×1.5 for 1920×1080)

```
Side margin        69           →  content width 1143
Columns            3 × 349   |   4 × 249.75
Gutter             48           (identical in both — a screen can change column
                                 count without re-deriving spacing. Keep it.)
Nav bar            y=45, h=30
Feature card       349 × 222    (image 349 × 196.7 + caption band)
Menu row           h=28, pitch 40
Description strip  349 × 37
Roster panel       w=349 main menu / 310 lobby
  header bar       h=31, inset 16, white fill, dark text
  player row       h=30, pitch 35
Profile bar        1280 × 50 at y=670   (always reserved)
```

Conversion for CSS mockups authored in `cqw`: **1 cqw = 12.8 px** at the 1280 base.

**Composition law, from the reference file:** left third is UI, centre is the subject
(character / 3D scene), right is status. The front end is a camera in `BR_Arena01`, not a
rectangle on black — see `ui-presentation` §1.

---

## 4. Component inventory

One row per component. **A component missing from either column is a defect** — the design has
no implementation, or the implementation has no spec.

| Component | Figma name | UE class | Built? |
|---|---|---|---|
| Nav tab bar | `Navigation Bar` | `UBRNavBar` | ☐ |
| Menu list row | `Text Button` | `UBRMenuRow` | ☐ |
| Feature card | `News Button` | `UBRFeatureCard` | ☐ |
| Description strip | `Decription Frame` | `UBRDescriptionStrip` | ☐ |
| Roster panel | `Party List` | `UBRRosterPanel` | ☐ |
| Roster row | roster row instances | `UBRRosterRow` | ☐ |
| Profile bar | `Profile Bar` | `UBRProfileBar` | ☐ |
| Button prompt | `Button Prompts` | `UBRButtonPrompt` | ☐ |
| Progress bar | `Progress Bar` | `UBRProgressBar` | ☐ |
| HUD vitals | — (HUD not in the file) | `UBRVitalsWidget` | ☐ |
| Killfeed row | — | `UBRKillfeedRow` | ☐ (pooled) |
| Reticle + hit markers | — | `UBRReticleWidget` | ☐ |

`Content/UI/` currently holds `WBP_RootLayout`, `WBP_HUDLayout`, `WBP_KillfeedEntry` — three
assets against twelve components. **The system is specified and largely unbuilt.**

---

## 5. Which reference governs which surface

| Surface | Follows | Why |
|---|---|---|
| Main menu, lobby, settings, matchmaking | **The Figma file** (founder decision, `UI-REFERENCE.md`) | Measured geometry exists; the file is front-end only |
| Post-match carnage report | **Infinite's PGCR structure**, restyled into the file's language | The file's "Post Game XP" is a *progression* screen, not a scoreboard |
| In-match HUD | **Campaign Evolved's corner layout** + Infinite's element behaviour | Our top band carries score/clock/rocket; Infinite's single-tray assumes a HUD with less to say |
| Death / respawn | Infinite's arena lineage | Campaign Evolved has no PvP death screen |

**Deliberate deviations from every reference, with reasons:**
- ~~**No motion tracker.**~~ **REVERSED by founder decision, 2 Aug 2026 — the motion tracker IS
  in scope**, built 1:1 from Halo Infinite (Arena: 18 m precise blips, 30 m edge-direction;
  BTB 24 m / 40 m; crouch-walk and Walk-binding movement undetected; disabled in Ranked and
  Tactical Slayer).

  *Correction to the record:* the original entry claimed this was backed by "R12's legibility
  argument". **R12 is "Bots are legible before they are optimal" and says nothing about radar** —
  it was cited by analogy, not as a ruling. No closed ruling ever banned a motion tracker, so
  nothing in `DESIGN-RULINGS.md` needed reversing. The GDD §2.8 "no privileged state" citation
  is also weak here: a tracker shows the *same* information to every player, which is the
  opposite of privileged state. The real argument against it was a design opinion — that in a
  4v4 arena with callouts as a pillar a radar replaces communication — and the founder has
  decided against that opinion.
- **Vitals top-left, not top-centre.** ~~Our top-centre carries score, match clock and the rocket
  countdown.~~ **SUPERSEDED — see below.** Halo Infinite puts shield and health in ONE trapezoid
  at **top-centre** (health nested in its bottom-centre, no gap) and puts score + timer at
  **bottom-centre**. Since the founder's instruction is 1:1 with Infinite, the HUD now follows
  Infinite's four anchors: top-centre survivability, bottom-left awareness, bottom-centre match
  state, bottom-right loadout, centre reticle.
- **Killfeed top-right, not far-left.** Infinite's left edge is free; ours holds vitals
  (top-left) and grenades/grapple (bottom-left).
- **Visor frames dialled back** to a vignette and one arc. Campaign Evolved's heaviest signature
  is also its most-criticised — the top community HUD mod exists to strip the boxes — and an
  arena shooter tolerates less chrome than a campaign.

---

## 6. C++ gaps this design pass found

Drawing the screens against the real ViewModels surfaced four. **All are C++ work; none is a UI
problem, and no widget can be bound until they land.**

| Gap | Blocks | Owner |
|---|---|---|
| No per-player stat block — score, K/D/A, shots fired/hit, medals earned | The entire carnage report | BP04 + BP10 |
| No reticle target-state field (over enemy / ally / neutral) | Reticle colour change | BP03 (owns the trace) |
| No per-player respawn countdown | The death screen timer | BP04 |
| No lobby ViewModel at all | Every front-end screen | BP10 + BP11 |

**Bindable today, with no new C++:** the entire in-match HUD except the reticle colour state —
vitals, ammo, stowed weapon, grenades, grapple ring, score, clock, rocket countdown, killfeed.
Every one of those getters exists on `UBRVM_Combat` / `UBRVM_Match` now.

---

## 7. Legal boundary

**Layout, hierarchy and behaviour: matched as closely as we like.** Where an element sits and
how it reacts is functional design and following convention is normal practice.

**Art: original, always.** Halo's typeface, medal iconography, emblems, rank insignia, visor
artwork and brand marks belong to 343/Microsoft. Breachpoint ships on Steam. Medal icons in
particular need original designs regardless of which reference a screen follows.

The target is **one-to-one on geometry and behaviour, original on art** — which is also the
only version that survives a store review.
