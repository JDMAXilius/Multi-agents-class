# `Content/UI/Icons/` — CONVENTION, no longer enforced

~98 glyphs (shapes UMG cannot draw). Naming: `T_UI_Icon_<Name>`
(`docs/ui/ue-frontend/ASSET-PIPELINE.md:81`).

## Why the folder name mattered, and what changed

`Tools/verify_notices.py` hard-coded `Content/UI/Icons/**/*.uasset` as a dependency probe: if the
glob matched anything and the token `Lucide` was absent from `THIRD-PARTY-NOTICES.md`, the run
failed. The Lucide set is ISC, and ISC requires the copyright and permission notice in **all
copies** — so the obligation is discharged by the notice *shipping*, not by existing.

**That script was deleted 4 Aug 2026 with R41 (founder direction). Nothing enforces this now.**
The obligation did not go away; only the check did. Keep importing icons here — not because a
gate can see it, but because the notice in `THIRD-PARTY-NOTICES.md` is written against this set
and nothing will warn you if it drifts.

## Known conflict (still unresolved)

`docs/ui/ue-frontend/ASSET-PIPELINE.md:90` (§4) tells importers to drop PNGs into
`Content/UI/Textures/<Family>/`, a different path from this one. Ticket BP63 existed to resolve
the contradiction and was deleted with the rest of the board; the contradiction itself is still
live. `Content/UI/Icons/` wins for icons.

## Never here

- Rank/medal marks, scene plates, weapon renders → `Content/UI/Art/`.
- Anything UMG can draw (rules, dividers, panel fills, text) — `ASSET-PIPELINE.md:15`,
  *export nothing UMG can draw*.
- A new icon set without adding its notice to `THIRD-PARTY-NOTICES.md` **and** re-copying
  `Content/Legal/THIRD-PARTY-NOTICES.txt` — drift between the two is now caught by nobody.
