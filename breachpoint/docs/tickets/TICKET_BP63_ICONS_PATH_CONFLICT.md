# TICKET — BP63: The documented icon import path silently escapes the licence gate

> STATUS: open — `contract_gap` G4, filed by BP26's cut, 2 Aug 2026. Decide-and-record: **keep
> `Content/UI/Icons/`, fix the doc.** Must land before the first icon is imported (~98 glyphs).

Founder directive: `Tools/verify_notices.py:50` hard-codes the probe
`("Content/UI/Icons/**/*.uasset", "Lucide", "icons are in Content/ but no Lucide notice")` — the
moment an icon lands under `Content/UI/Icons/`, rung 2 requires the Lucide ISC notice to be
present. `docs/ui/ue-frontend/ASSET-PIPELINE.md` §4 (line 90) tells importers *"Drop the PNG into
`Content/UI/Textures/<Family>/`"*. **An icon imported per the doc is invisible to the gate.** This
is a licence-compliance hole with a documentation shape, and it costs one line to close *before*
98 glyphs land in the wrong folder and someone has to choose between a mass move and a silent
exemption.

**Ordering law:** this ticket gates the first icon import. Nothing else gates it.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: files-only
- The conflict is still live: `Tools/verify_notices.py:50` still names
  `Content/UI/Icons/**/*.uasset`, and `docs/ui/ue-frontend/ASSET-PIPELINE.md:90` still says
  `Content/UI/Textures/<Family>/`.
- `ls Content/UI/` returns **only** the three WBPs — no icon has been imported yet, so no asset
  has to move. **If that is no longer true, stop and re-scope**: this becomes a move plus a
  notices audit, not a doc fix.
- owner_path: `docs/ui/ue-frontend/ASSET-PIPELINE.md`

## Steps (in order)

1. Correct `ASSET-PIPELINE.md` §4 to `Content/UI/Icons/<Family>/` for anything Lucide-derived, and
   state **in the doc, at the import step**, *why* the folder name is load-bearing: it is the
   string `verify_notices.py` matches, and importing elsewhere skips the licence check. A reader
   who does not know that will "tidy" it back within a month. Owner: **builder**.
2. In the same pass, make the split explicit for non-icon UI textures: `T_UI_<Family>_<Name>`
   under `Content/UI/Textures/<Family>/` remains correct for art plates and marks that carry **no
   third-party licence**; only licensed glyph sets are bound to `Icons/`. Owner: **builder**.
3. Verify: `python3 Tools/verify_notices.py` still passes on the current tree, and a **deliberate
   throwaway probe** — one dummy `.uasset` under `Content/UI/Icons/` with the Lucide notice
   removed — makes it **fail**. Delete the probe. A gate nobody has seen fire is a gate nobody
   knows works. Owner: **verifier**.

## Done when

- [ ] `ASSET-PIPELINE.md` §4 names `Content/UI/Icons/` for licensed glyphs and says why, at the
      import step where an importer will actually read it
- [ ] `verify_notices.py` is **unchanged** — the decision is "fix the doc", and a session that
      edits `Tools/` here is outside the owner path (law 5)
- [ ] The gate is demonstrated firing on a temporary probe and the probe is removed; the run is
      pasted into this Log
- [ ] `python3 Tools/verify_notices.py` passes on the tree at hand-off
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: builder (the doc) · verifier (the probe)
- Binary files this ticket OWNS (lock before editing): none — **and no icon may be imported by
  this ticket.** If one lands here, the ticket has become an asset packet under R37 and needs a
  receipt.
- Out of scope: importing any icon · editing `Tools/verify_notices.py` · the Rajdhani/Roboto
  Condensed OFL font notices (different dependency, same file, not this gap) · renaming
  `Content/UI/Textures/`
- Verified against both files, 2 Aug 2026: `verify_notices.py:50` and `ASSET-PIPELINE.md:90` say
  exactly what the proposal claims. `Content/UI/` currently holds three `.uasset` files, all WBPs,
  so **the cheap window is open** — no asset has to move.

## Log

(append findings here, dated, newest last — this is what the next session reads)
