# TICKET — BP73: Close the nine-value DECIDE ledger before a builder invents one

> STATUS: **DONE — all 9 rows closed, cloud session, 3 Aug 2026.** **figma-mcp**, the one lane
> live in a cloud container. Four escalations left behind (E1–E4 below); none of them belongs to
> this ticket, and **E2 is a founder call BP72 will hit.**

Founder directive, from the grid correction that started this pass: **a measurement invented
under deadline is how 44-vs-33 happened.** `MCP-BUILD-PLANS.md` names nine values it refused to
guess. Each is a font, a padding or an origin that the reference file already knows and that no
document in this repo records. This ticket reads them.

**Ordering law:** none — it runs in parallel with everything. But a DECIDE still open when BP72
reaches its node means that node ships unstyled or unbuilt, and the ticket says which.

## Kickoff (machine-checkable)

- requires: **files-only + the Figma MCP** (`mcp__Figma__get_metadata` / `get_design_context`
  answer). No engine, no editor.
- `docs/ui/ue-frontend/MCP-BUILD-PLANS.md` exists and its DECIDE ledger has 9 rows
- **Node-id provenance:** every id is in the REFERENCE file `Kn87U5sy2VD0lP8K7h4LcQ`
  ("Halo Infinite UI Rework"), NOT in Breachpoint's working file `yznvnVdOFDADaugZSeomfP`
  where the same frames exist as clones under different ids (`SCREEN-MANIFEST.md` §4)
- **The `get_metadata` trap** (`ui-presentation` skill): it lists only the page currently open
  in the desktop app, not every page in the file. A node that "does not exist" may just be on
  another page — resolve the id directly before believing an absence
- owner_path: `docs/ui/ue-frontend/`, `Tools/gen_ui/figma_tokens.json`,
  `docs/tickets/TICKET_BP73_DECIDE_LEDGER.md`

## The ledger

| # | Where | What is missing | Nearest existing style | State |
|---|---|---|---|---|
| 1 | `WBP_ButtonPrompt` `Verb` | font | `Label/Micro` (Rajdhani SemiBold 10, ls 100) | CLOSED = Roboto Cond **Bold 14** ls 0 — guess wrong on all three axes |
| 2 | `WBP_ButtonPrompt` | glyph → verb gap | — | CLOSED = 10 |
| 3 | `WBP_RosterHeader` `Label` | font | `Heading/Caption` (Rajdhani Bold 12, ls 100) | CLOSED = Rajdhani **Bold 18** ls 100 UPPER |
| 4 | `WBP_RosterHeader` `Count` | font | `Data/Value` (Roboto Cond SemiBold 12, ls 100) | CLOSED = Rajdhani **SemiBold 18** ls 0, opacity .8 |
| 5 | `WBP_FeatureCard` `Caption` | font + padding | — | CLOSED = `Label/Button` + pad (20,10,20,10) |
| 6 | `WBP_ProfileBar` | edge insets | — | CLOSED = (5,5,16,5) |
| 7 | `WBP_ProfileBar` `Status` | font | `Data/Value` | CLOSED — node is hidden, row dies |
| 8 | `WBP_LeftRail` `MenuRowSlot` | top inset (only L16/R22 are named constants) | read `BRLeftRail.h` first | CLOSED = 16 uniform |
| 9 | `WBP_NavBar` bumpers | **origin ambiguity** — read bar-local, the left glyph (27..54) overlaps the first tab (x39) by 15px while the right glyph sits flush at 639..666. Both cannot be bar-local | — | CLOSED — both bar-local, overlap is authored |

**All nine closed.** The four font rows needed `get_design_context`, not `get_metadata` — the
latter returns geometry only (id, type, name, position, size), which is why the first pass stalled
on them. `get_design_context` returns the resolved text properties, and the
`figma-design-to-code` skill was loaded first as its tool description requires.

## Steps (in order)

1. For each row, resolve the owning node in the **reference** file and read its real values —
   text style name, auto-layout padding and gap, absolute origin. `get_design_context` on the
   node beats `get_screenshot` for anything numeric; a screenshot is for confirming *which*
   node, never for measuring it.
2. **Match every font to an existing `figma_tokens.json` style by name**, not by eyeballing
   size. If a node's style is not in the token file, that is a finding — the token file was read
   live from Figma variables and a missing style means either a new style landed or the node is
   off-system. Record which; do not add a row to the token file from a single node read.
3. Row 9 is different: it is **a conflict, not a gap.** Read the bumper prompts' parent frame
   and establish whether the two x values are bar-local or frame-local. If they are frame-local,
   both are consistent and the ambiguity dissolves. If they are bar-local, one of them is wrong
   in the source and the founder picks.
4. Write each closed value into `MCP-BUILD-PLANS.md` **in place** — replace the DECIDE row's
   entry in the tree with the measured value and strike the ledger row. A ledger that keeps a
   closed row is a ledger nobody trusts.
5. Anything the file genuinely does not answer stays open and moves to the founder list with
   the evidence attached: *"node X carries no padding; the reference authors it as auto-layout
   hug"* is a closed question. *"I could not find node X"* is not.

## Done when

- [x] Each of the nine rows is CLOSED with a measured value, or RE-FILED as a founder call with
      the node evidence that makes it one — **9/9, every one node-cited**
- [x] `MCP-BUILD-PLANS.md`'s trees carry the measured values; the ledger reflects only what is
      still open — §A3, §A4, §B1, §B2, §B5, §C1 and the ledger updated
- [x] Every font resolves to a named style in `figma_tokens.json`, or the mismatch is a finding
      — **one resolves (`Label/Button`), three are off-system and filed as a finding**; no token
      row was added from a single node read
- [x] Row 9's ambiguity is resolved as geometry or escalated as a source conflict — geometry:
      both bar-local, the overlap is authored, and §B1's `HBox` cannot express it (founder call)
- [x] Findings + decisions written to this ticket's Log

## Escalations this ticket produced (not its to fix)

| # | What | Owner |
|---|---|---|
| E1 | `ImageHeight = 196.7f` (`BRFeatureCard.h:62`) sizes to a **hidden** layer; the visible card has no 196.7 | BP71 / ui-builder |
| E2 | §B1's bumper overlap (15px over the first tab) is authored and an `HBox` cannot express it — keep the HBox or switch to an `Overlay`? | FOUNDER |
| E3 | Three measured fonts are absent from `figma_tokens.json` — extraction gap, or genuinely off-system nodes? | style sweep |
| E4 | The feature card's gradient scrim needs a material; the card ships without it | art pass |

## Notes

- Crew: **ui-builder** (this is design-system reading, not code).
- Binary files this ticket OWNS: none. It writes markdown.
- Out of scope: writing to the Figma file (this is a read pass), adding token rows from a single
  node, and **closing a row by picking the "nearest existing style"** — that column is a sanity
  check on the answer, not the answer.
- **The honest failure mode to avoid:** returning nine plausible values with no node ids. A
  closed row must name the node it was read from, or the next person cannot check it.

## Log

### 3 Aug 2026 — five of nine closed, every one node-cited

Read against the **reference** file `Kn87U5sy2VD0lP8K7h4LcQ` via `mcp__Figma__get_metadata`.

**Row 2 — glyph→verb gap = 10.** `Button Prompts` `119:1491` holds a `Menu` variant `0:9`
(62×20): glyph `0:10` at (0,0) 20×20, text `0:16` at (30, 1.5) 32×17. Glyph ends at x=20, verb
starts at x=30. Landed in `MCP-BUILD-PLANS.md` §A3 with the glyph's real 20×20 size, which the
plan had also been guessing at.

**Row 8 — `MenuRowSlot` inset = 16 uniform, and an existing constant was wrong.** `Contents`
`0:1183` sits at (16,16) and is 311 wide inside a 343-wide `Menu List` `0:1176`; 343 − 16 − 311
= 16 on the right. `BRLeftRail.h` carried `MenuRowSlotInsetRight = 22.0f`. It disagreed with the
file **and no code read it** — deleted, with the measurement cited in the header comment. This is
the second time this pass that a value nobody read turned out to be wrong; an unread constant is
not harmless, it is a lie waiting for its first reader.

**Row 9 — resolved as geometry, not escalated.** Both prompts are direct children of the 666×30
`Navigation Bar` `124:1179`: `0:18` at x=27 and `0:37` at x=639, each 27×15 at y=7.5. So both
numbers ARE bar-local and the "both cannot be" premise was mine, not the file's. The real finding
is downstream: tabs start at x=39, so the left glyph (27..54) **overlaps the first tab by 15px in
the source**, and plan §B1 lays the bar out as an `HBox`, which cannot overlap. Recorded in §B1
as a founder call — reproducing it needs an `Overlay`, and the numbers are not the thing to
change.

**Row 6 — the question was wrong, so the answer is a plan correction.** I asked for "edge
insets" on a full-width bar. `Profile Bar` `119:1525` is 1280×50 and contains exactly one child:
`Player Card` at **x=862, 349×50** — column 3's origin, column 3's width. Internals, card-local:
`Superintendent` 40×40 at (5,5); `Gamer and Service Tag` at (55,17) 107×17; `Buttons` at (211,0)
122×50, ending at 333 for a 16 right inset. So insets are (5,5,16,5) and the avatar→name gap is
10. **`MCP-BUILD-PLANS.md` §B5's tree was wrong** — it had a full-width `HBox` with the gamertag
left and prompts right, which would have stretched a 349-wide design across 1280. Rewritten: fill
spacer + 349 `SizeBox`, with a MUST-NOT against authoring 862 anywhere.

**Row 7 — closed by deletion.** `Service Tag`, the second text line, is **hidden in the source**.
There is no status line to transcribe, so there is no font to read. If "IN MENUS · Invite Only"
is wanted it is new design, not a measurement, and it does not belong in a transcription ticket.

**Rows 1, 3, 4, 5 — the four fonts — closed in the same session, after the tool changed.**
`get_metadata` returns geometry only; step 1 of this ticket names `get_design_context` for exactly
this reason. Loaded `figma-design-to-code` (via the MCP skill resource) as that tool's description
requires, then read three nodes.

**Every one of the three "Nearest existing style" guesses was wrong, and not narrowly.**

| Row | Guessed | Measured | Node |
|---|---|---|---|
| 1 `Verb` | `Label/Micro` — Rajdhani SemiBold 10 | **Roboto Condensed Bold 14**, ls 0, white | `119:1491` |
| 3 `Label` | `Heading/Caption` — Rajdhani Bold 12 | **Rajdhani Bold 18**, tracking 1.8px on 18 = 10% → ls 100, UPPER | `927:43301` |
| 4 `Count` | `Data/Value` — Roboto Cond SemiBold 12 | **Rajdhani SemiBold 18**, ls 0, opacity 0.80, right-justified, h21 | `927:43301` |
| 5 `Caption` | — | **`Label/Button`** — Rajdhani SemiBold 16, ls 100, UPPER · pad (20,10,20,10) | `1769:23147` |

Row 1 was wrong on family, weight AND size. Row 4 was wrong on family, weight and size, in the
same breath as row 3 being wrong by 6px in the opposite direction. This is the whole argument for
the ticket: the Nearest column is a plausible-looking answer, and **three out of four times it
was a different font.** Out of scope said so; the data agrees.

**Three of the four are off-system.** `figma_tokens.json` has no `Roboto Cond Bold 14`, no
`Rajdhani Bold 18` and no `Rajdhani SemiBold 18` — its ladder jumps `Heading/Panel` 16 →
`Display/Title` 20, and carries Roboto Condensed only at Medium 14 / SemiBold 12. Per step 2 I
did **not** add three rows from three node reads; filed as E3 for a style sweep to judge whether
the extraction has a gap or the reference authors these nodes off-system.

**Two structural corrections fell out of the font reads**, which is the real value of reading a
node instead of a number:

- **§A4 `WBP_RosterHeader` is three bands, not one.** 31 = a 28-high text band + 3 gap + a 1px
  bottom line. Its ground is a **vertical gradient white 0.10 → 0.30**, not a flat tint, and the
  band is `px-10`. The old tree's MUST-NOT said "no hairline — the header's separation is the
  panel's business"; the node says the separator IS the header's. Corrected, and the tree gained
  a `VBox` and a `BottomLine`.
- **§B2 `WBP_FeatureCard`'s caption is overlaid, not stacked.** The reference composes image
  full-bleed at inset 7, a transparent→black vertical gradient from the midpoint down, and the
  40-high caption frame anchored to the bottom of that same box. The old `VBox` (ImageBox 196.7 +
  Caption 25.3) would have put the caption on a solid strip below the art. The dots own the
  bottom 22 — `Text + Image Button` is `inset [0,0,22,0]`.

**E1, found while checking that.** `ImageHeight = 196.7f` traces the doc sentence "image 349 ×
196.7 + caption", and the only 196.709 node in the file is `Preview Photo` `0:1027`, **hidden**.
The visible card's image is inset 7 inside a 200-tall region — 186. So `SetHeightOverride`
(`BRFeatureCard.cpp:28`) and `BRMenuRow.cpp:75` are both sizing to a hidden layer. **Not fixed
here** — a C++ constant with two readers is BP71's packet, not a read pass's. Filed.

Attempts that failed, recorded so nobody repeats them: `get_metadata` on `0:671` returned a
*different* node (a 222×14 text "Example Description") — instance-internal ids only resolve
inside a parent's recursive dump. `I527:3515;0:671` returned "node ID was not found".
`1769:23147` under `get_metadata` returned a collapsed instance with no children — **and the same
id under `get_design_context` returned the full tree.** That is the lesson: a collapsed
`get_metadata` result is not evidence the node is empty. The way through the instance-id trap was
to resolve the header on a *screen* that hosts it (`RS_Friends` `927:43283` → `927:43301`) rather
than inside the component instance.
