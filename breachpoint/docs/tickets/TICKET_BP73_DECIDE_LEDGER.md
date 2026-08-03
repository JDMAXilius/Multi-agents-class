# TICKET — BP73: Close the nine-value DECIDE ledger before a builder invents one

> STATUS: **in-progress — cloud session, 3 Aug 2026.** **figma-mcp**, and it is the one lane
> live in a cloud container right now. Gates nothing; every hour it stays open is an hour a
> BP72 builder might type a plausible number instead of a measured one.

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

| # | Where | What is missing | Nearest existing style |
|---|---|---|---|
| 1 | `WBP_ButtonPrompt` `Verb` | font | `Label/Micro` (Rajdhani SemiBold 10, ls 100) |
| 2 | `WBP_ButtonPrompt` | glyph → verb gap | — |
| 3 | `WBP_RosterHeader` `Label` | font | `Heading/Caption` (Rajdhani Bold 12, ls 100) |
| 4 | `WBP_RosterHeader` `Count` | font | `Data/Value` (Roboto Cond SemiBold 12, ls 100) |
| 5 | `WBP_FeatureCard` `Caption` | font + padding | — |
| 6 | `WBP_ProfileBar` | edge insets | — |
| 7 | `WBP_ProfileBar` `Status` | font | `Data/Value` |
| 8 | `WBP_LeftRail` `MenuRowSlot` | top inset (only L16/R22 are named constants) | read `BRLeftRail.h` first |
| 9 | `WBP_NavBar` bumpers | **origin ambiguity** — read bar-local, the left glyph (27..54) overlaps the first tab (x39) by 15px while the right glyph sits flush at 639..666. Both cannot be bar-local | — |

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

- [ ] Each of the nine rows is CLOSED with a measured value, or RE-FILED as a founder call with
      the node evidence that makes it one
- [ ] `MCP-BUILD-PLANS.md`'s trees carry the measured values; the ledger reflects only what is
      still open
- [ ] Every font resolves to a named style in `figma_tokens.json`, or the mismatch is a finding
- [ ] Row 9's ambiguity is resolved as geometry or escalated as a source conflict
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: **ui-builder** (this is design-system reading, not code).
- Binary files this ticket OWNS: none. It writes markdown.
- Out of scope: writing to the Figma file (this is a read pass), adding token rows from a single
  node, and **closing a row by picking the "nearest existing style"** — that column is a sanity
  check on the answer, not the answer.
- **The honest failure mode to avoid:** returning nine plausible values with no node ids. A
  closed row must name the node it was read from, or the next person cannot check it.

## Log

(append findings here, dated, newest last)
