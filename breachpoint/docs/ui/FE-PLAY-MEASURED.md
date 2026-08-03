# FE_Play — measured 1:1, and why the current WBP cannot match it

**Read 3 Aug 2026 from the WORKING file `yznvnVdOFDADaugZSeomfP`, node `21:32824`, via
`get_metadata`.** Every number below is off that read — no number here is inferred, and none
comes from `REFERENCE-EXTRACTION.md` or any recorded id (BP67 exists because those two files
disagree; this bypasses the question by reading the node the founder linked).

## The frame's visible children (absolute, frame-local)

| # | Node | id | x | y | w | h |
|---|---|---|---|---|---|---|
| 1 | Background | 21:32825 | 0 | 0 | 1280 | 720 |
| 2 | **Progression Button** | 21:32826 | **869** | **55** | 334 | 115 |
| 3 | Player (3D stage) | 21:32827 | 480 | 118 | 320 | 602 |
| 4 | Party List | 21:32861 | 862 | 397 | 349 | 273 |
| 5 | Profile Bar | 21:32862 | 0 | 670 | 1280 | 50 |
| 6 | Button Prompts | 21:32863 | 60 | 685 | 62 | 20 |
| 7 | Navigation Bar | 21:32864 | 33 | 45 | 666 | 30 |
| 8 | Menu Combo | 21:32877 | 69 | 138 | 349 | 510 |

HIDDEN in the frame, and therefore NOT layout: `Play Menu & Description` (21:32830),
`Menu & News BUtton` (21:32839), `Grid - 3 Collumn` (21:32868), `Grid - 4 Collumn` (21:32872),
`Menu Background` (21:32865, at y=720 — off-frame).

**The 3-column grid is hidden.** It is a guide, not the layout. Building the screen as three
columns was reading the guide as the design.

## Why `WBP_Screen_FrontEnd` cannot match this, structurally

The built tree is `SafeZone > VBox[ NavBar, ContentBand[Col1, Col2, Col3] ]`.

**`Progression Button` is at y=55. The Navigation Bar is at y=45, 30 tall, so it occupies
45..75.** The two OVERLAP vertically. A VBox stacks its children, so every content child is
forced below the nav band — there is no padding, alignment or fill that puts a VBox sibling
level with an earlier sibling. The layout is not "wrong numbers in the right shape"; the shape
cannot express it.

Second, smaller, same cause: `Progression Button` x=869 and `Party List` x=862 are **not**
left-aligned (7px apart), and their y gap is 397-(55+115) = 227. A single right-hand column
with one padding value produces neither.

## The conflict this forces, stated rather than resolved

LAYOUT-DOCTRINE §6 forbids a root `CanvasPanel`. `UBRLeftRail` has the one documented
exception, granted because `ApplyCaret` needs an arbitrary y.

FE_Play is an absolutely-positioned frame with overlapping bands. Matching it 1:1 needs
either:

- **(a) a root CanvasPanel** on this screen, with a documented exception in the same shape as
  `BRLeftRail`'s — five point-anchored `canvas_slot()` children at the measured coordinates.
  Honest, exact, and it reopens §6 for one screen.
- **(b) an Overlay root** with per-child alignment + margins. Expressible today and needs no
  ruling, but every position becomes a margin computed off 1280x720, so it is correct at 16:9
  and drifts at every other aspect — which is the defect `canvas_slot()`'s docstring exists to
  warn about.
- **(c) keep bands and accept it is not 1:1.** Rejected by the founder's own instruction.

**Recommendation: (a).** The frame is authored as absolute geometry, and the honest way to
reproduce absolute geometry is a canvas. §6 exists to stop canvases being used as a default,
not to stop them where the design IS a canvas — and `BRLeftRail` already establishes the
"documented exception, stated in the header" pattern this would follow.

## What is NOT in the current WBP at all

`Background` (21:32825), the `Player` 3D stage (a camera's box, not a widget), `Profile Bar`
(21:32862) and `Button Prompts` (21:32863). The last two are root-layout chrome by
`SCREEN-MANIFEST` §7.1 and `UBRScreen_FrontEnd` declares no member for either, so they are
correctly absent from THIS screen — but they are visible in the frame, which is why the
render looks emptier than the design even where the geometry is right.

## Not claimed

No screenshot was compared. This is a metadata read: positions and sizes only. Fills, strokes,
type and the `Background` instance's contents were not read, so "1:1" here means geometry,
not appearance.
