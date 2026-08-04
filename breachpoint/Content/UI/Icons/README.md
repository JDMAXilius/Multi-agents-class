# `Content/UI/Icons/`

~98 glyphs (shapes UMG cannot draw). Naming: `T_UI_Icon_<Name>`
(`docs/ui/ue-frontend/ASSET-PIPELINE.md:81`).

## Why the folder name is a convention worth keeping

Every icon brush in `Tools/gen_ui/wbp_plan.py` resolves against this path, and the generator's
`texture_problem()` check verifies each one exists ON DISK at plan time. An icon imported
somewhere else does not fail loudly — it produces a brushless `UImage`, which renders as a blank
white rectangle (BP70 D2). Import icons here. Only here.

## Known conflict (still unresolved)

`docs/ui/ue-frontend/ASSET-PIPELINE.md:90` (§4) tells importers to drop PNGs into
`Content/UI/Textures/<Family>/`, a different path from this one. Ticket BP63 existed to resolve
the contradiction and was deleted with the rest of the board; the contradiction itself is still
live. `Content/UI/Icons/` wins for icons.

## Never here

- Rank/medal marks, scene plates, weapon renders → `Content/UI/Art/`.
- Anything UMG can draw (rules, dividers, panel fills, text) — `ASSET-PIPELINE.md:15`,
  *export nothing UMG can draw*.
- An icon that duplicates one already here under a second name. The set is deduplicated by
  intent, not by pixels, and two names for one glyph is two things to restyle later.
