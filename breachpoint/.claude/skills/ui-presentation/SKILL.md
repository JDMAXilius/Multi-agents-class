---
name: ui-presentation
description: The BREACHPOINT UI design system and the Figma↔UE 5.8 pipeline — VISR-style colour semantics, the measured 1280×720 grid, component-first authoring, and how a Figma node becomes a WBP layout bound to a ViewModel. Load for ANY front-end or HUD work: designing a screen, pulling geometry out of the reference Figma file, authoring a WBP, or deciding what may live in an asset vs C++. Pairs with ue5-ui-architecture (that skill is CommonUI/MVVM mechanics; this one is the design system and the handoff).
---

# Presentation — the design system, and the road from Figma to UE

**Authority note (read first).** `CLAUDE.md` laws 3, 4, 5 and 7 are LAW, and `R18`/`R26` in
`docs/DESIGN-RULINGS.md` decide what may be an asset. This skill never overrides them — on any
conflict the law wins and the conflict is a finding against this skill. `ue5-ui-architecture`
owns the CommonUI/MVVM *mechanics*; this skill owns the *design system* and the *handoff*.
`docs/UI-REFERENCE.md` records the founder's reference decision.

---

## 1. The word "Presentation", and why it is the right frame

Halo Infinite's UI discipline was not called "UI" — it was **Presentation**, and it covered
*"everything from the in-game & front-end user interfaces (UX/UI) to the realization of the
grounded, in-world scenes (Levels) that occupy them"* (Eric Dies, UI + Realization Lead,
343 Industries).

**That is one job, not two.** A menu is not a rectangle of widgets floating on black; it is a
camera looking at a real place, with UI composed into it. The reference Figma file shows this
directly — every front-end frame is a 3D scene with a character standing in it and the UI laid
into the negative space beside them. Campaign Evolved does the same with the ring model.

**What it means for us in practice:**

- Our front end lives over **`BR_Arena01`**, not over a black quad. The main menu's background
  is a camera in the arena. Budget for that in BP10/BP07, because a menu with no scene behind
  it is a different, worse design that no amount of widget polish fixes.
- **The left third is UI, the middle is the subject, the right is status.** That is the
  reference file's composition and it is a load-bearing choice: the eye lands on the character,
  the hand works the left rail, and status (roster, rank) sits where it can be ignored.
- Negative space is not empty. If a screen has no scene, it needs a reason.

## 2. The first move is a design system, not a screen

> *"When first joining the project, creating a design system was a top priority in gaining
> forward momentum, which not only wrangled the design language of the interface but also
> provided the design team with clear guidance and components to use, resulting in a reliable
> toolset they could leverage while focusing on consistency and cohesion."* — Eric Dies

**Do this in the same order.** Before authoring any screen, the components below must exist as
named, reusable things — a Figma component set *and* a `UBRActivatableWidget`/`UUserWidget`
subclass. A screen assembled from ad-hoc boxes is a screen nobody can restyle later.

**The BREACHPOINT component inventory** (create once, reuse everywhere):

| Component | Figma name | UE class | Notes |
|---|---|---|---|
| Nav tab bar | `Navigation Bar` | `UBRNavBar` | bumper glyph + tabs; gamepad LB/RB routes it |
| Menu list row | `Text Button` | `UBRMenuRow` | 28 h, underlined, selected = 2px rule + full white |
| Feature card | `News Button` | `UBRFeatureCard` | 349×222 image + caption + carousel dots |
| Description strip | `Decription Frame` | `UBRDescriptionStrip` | 349×37, italic, explains the focused row |
| Roster panel | `Party List` | `UBRRosterPanel` | white header bar + 30 h rows, one per player |
| Roster row | `Roster Group Header` + rows | `UBRRosterRow` | emblem · name · team fill · diamond · mute |
| Profile bar | `Profile Bar` | `UBRProfileBar` | 1280×50 at y=670, always present |
| Button prompts | `Button Prompts` | `UBRButtonPrompt` | glyph + verb; CommonUI input-action driven |
| Progress bar | `Progress Bar` | `UBRProgressBar` | used for vitals, cooldowns, match record |

**Naming law:** the Figma component name and the UE class name are recorded together in
`docs/UI-DESIGN-SYSTEM.md`. A component that exists on one side only is a defect — either the
design has no implementation or the implementation has no spec.

## 3. VISR OS — two colour channels, split by audience

343's published system (Inside Infinite, Oct 2021) splits the HUD by **who the message is for**,
not by what looks good:

- **Cool blue = "Windows"** — front-end, player-facing. The things a player acts on.
- **Neon green = "VISR BIOS"** — back-end, system messaging, deliberately reading like DOS.

**Breachpoint's adaptation** (we are not Halo and must not use its art, but the *semantics*
transfer, and semantic colour is what makes a HUD readable under stress):

| Channel | Hex | Means, and ONLY means |
|---|---|---|
| `--shield` | `#35D0F2` | **You.** Your shields, your team, your reticle at rest, your grapple |
| `--health` | `#F5C542` | Health beneath shields. Appears only after damage (Infinite's rule) |
| `--amber` | `#FFA333` | **A clock is running.** Rocket countdown, medals, host authority |
| `--enemy` | `#FF4A3D` | **Threat.** Enemy, incoming damage, shield-break flash |
| `--team-them` | `#FF7A45` | The other team in lists and feeds |
| white | `#FFFFFF` | **You, in a list of people.** Killfeed self-entries, header bars |

**The discipline is the point.** Red is a threat channel — do not spend it on a low-ammo warning
that is not lethal, or on a UI error. Health is yellow, never green: green reads as "fine" at
exactly the moment the player is one shot from dead.

## 4. Fidelity scales with nearness to gameplay

343's tier rule: *minimal and flat when far from play; the VISR colour philosophy and more
skeuomorphic treatment once in-game.* One sentence that explains both the menus and the HUD.

| Distance from play | Treatment |
|---|---|
| Front end (menus, lobby, settings) | Flat. Sharp corners. 1px borders. Near-monochrome + one accent. The UI defers to the 3D scene |
| Loading / matchmaking | Flat, but the accent channel wakes up |
| In-match HUD | Full VISR semantics. Glow, segmentation, state colour, motion |

**Consequence:** do not carry a menu treatment into the HUD or vice versa. If the HUD's shield
bar and the lobby's progress bar look identical, one of them is wrong.

## 5. The grid — measured, not invented

Extracted from the reference file's node metadata via the Figma MCP. **These are the numbers to
build to.** Base canvas **1280×720**; multiply by **1.5** for 1920×1080.

```
Side margin        69          (content width 1143)
Columns            3 × 349  or  4 × 249.75
Gutter             48         (both layouts — this is what makes them interchangeable)
Nav bar            y=45,  h=30
Feature card       349 × 222   (image 349 × 196.7 + caption)
Menu row           h=28,  pitch 40      (12 px gap)
Description strip  349 × 37
Roster panel       w=349 (main menu) / 310 (lobby)
  header           h=31, inset 16
  rows             h=30, pitch 35
Profile bar        1280 × 50 at y=670
Safe bottom        720 − 670 = 50 reserved for the profile bar, always
```

**Why the 48px gutter matters:** it is identical in the 3-column and 4-column layouts, so a
screen can change column count without re-deriving spacing. Keep it.

## 6. Pulling geometry out of Figma (the actual commands)

The Figma MCP is connected. **Never eyeball a screenshot when the metadata is available.**

1. `get_metadata` with no `nodeId` → lists the file's top-level pages.
2. `get_metadata` with a page id → the XML tree. **This is often >100k tokens** for a real
   page; it gets written to a file instead of returned. Parse it with a script, do not Read it.
3. `get_screenshot` with `nodeId` + `maxDimension` → the rendered frame. **Set
   `enableBase64Response: true`** — this container's proxy denies direct fetches of
   `figma.com` asset URLs, so the curl path in the tool's response will fail with exit 56.
4. `get_design_context` for a node when you want its properties as code.

**Parsing the metadata dump** — depth-limited walk, which is what produced §5:

```python
import json, re
xml = "".join(b["text"] for b in json.load(open(DUMP)))
depth = 0
for t in re.finditer(r'<(/?)([a-zA-Z-]+)([^>]*?)(/?)>', xml):
    close, tag, attrs, selfc = t.groups()
    if close: depth -= 1; continue
    if depth <= 3:
        g = lambda k: (re.search(k+r'="([^"]+)"', attrs) or [None,"?"])[1]
        print("  "*depth, tag, g("name"), g("x"), g("y"), g("width"), g("height"))
    if not selfc: depth += 1
```

## 7. Rendering a screen to a PNG for review

There is no engine in a cloud session, but there **is** headless Chromium, and a mockup
rendered at 1600×900 is a real reviewable artifact.

```bash
CHROME=$(ls -d /opt/pw-browsers/chromium-*/chrome-linux/chrome | head -1)
"$CHROME" --headless=new --disable-gpu --no-sandbox --hide-scrollbars \
  --force-device-scale-factor=1.5 --window-size=1600,900 \
  --screenshot=out.png "file://$PWD/screen.html"
```

Author mockups with `container-type: inline-size` and size everything in `cqw`, so one stylesheet
renders at any output size. **1 cqw = 12.8 px at the 1280 base** — that is the conversion between
§5's measured pixels and the mockup.

## 8. Figma → UE 5.8: what crosses the boundary, and what does not

**Crosses (design owns it):** layout, position, size, spacing, hierarchy, state treatment
(selected/disabled/focused), colour token *names*, motion timing.

**Does NOT cross (code owns it):** every value the sim reads, every binding, all state, all flow.

The pipeline, in order:

1. **Component in Figma** → **C++ class** first (`UBRMenuRow`, `UBRRosterPanel`). The class
   declares the `BindWidget` slots and every `UPROPERTY` the layout needs. Written with the
   editor CLOSED, then built (R36).
2. **WBP layout asset** reparented to that C++ class. **Layout, anchors and animation ONLY**
   (`AUTHORING-MATRIX` Tier 4). Zero graph nodes — R26's five conditions apply to WBPs exactly
   as to the `BP_BR*` containers.
3. **Binding via MVVM** to `UBRVM_Combat` / `UBRVM_Match`. A widget never reaches into the
   pawn, the ASC or the GameState. If a field does not exist on a ViewModel, **that is a C++
   gap — file it, do not work around it in the widget.**
4. **Numbers come from `Content/Data/*.csv`.** A threshold that turns ammo red is a
   `CT_Combat` row, not a literal typed into a details panel (law 3).

**The one-way rule:** when Figma and the game disagree, the game is right about *behaviour* and
Figma is right about *appearance*. Fix the side that is wrong; never let both drift.

## 9. Colour tokens live in one place

Define the palette **once** as a `UBRUISettings` (developer settings) or a
`DT_UIPalette` row set, and have every widget read it. Do not type hex into a WBP.

Reason: a WBP is binary and unreviewable. Twelve widgets with hand-typed hex is twelve places a
rebrand breaks silently and the critic cannot diff any of them. One data source is greppable,
diffable, and changeable in one commit.

## 10. What a UI packet must never do

- **Create a Blueprint class.** WBPs are layout assets parented to C++ (R18/R26). A sixth
  `BP_*` class is a `high` finding.
- **Put a decision in a widget graph.** No branches, no arithmetic, no gameplay reads. If the
  layout needs a computed value, it belongs on the ViewModel.
- **Tick.** Law 4. Bind to `FieldNotify`; never poll.
- **Reproduce Halo's art.** Typeface, medal icons, emblems, rank insignia, visor artwork and
  brand marks are 343/Microsoft's. We follow *layout and behaviour*; the art is original.
  Breachpoint ships on Steam and this is not negotiable.
- **Assert a UI works from PIE alone.** PIE is single-process. Split-screen states, join-in-
  progress, and anything driven by replicated data need rung 4 (R30), and a HUD claim names its
  rung like any other.

## 11. Self-check before handoff

- Every element on the screen traces to a **named component** in §2, or a new one was added
  to `docs/UI-DESIGN-SYSTEM.md` with both its Figma and UE names.
- Every dynamic value traces to a **ViewModel getter that exists** — or is filed as a C++ gap.
- Colours are **token names**, not hex, and each one is used for its §3 meaning only.
- The WBP has **zero graph nodes** and declares no new variables.
- No gameplay number is set in an asset.
- Geometry matches §5, or the deviation is written down with a reason.
- The screen was **rendered and looked at** (§7), not just described.
