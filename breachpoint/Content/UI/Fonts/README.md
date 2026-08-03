# `Content/UI/Fonts/`

Two families, both OFL: **Rajdhani** (the UI type ramp) and **Roboto Condensed** (italic —
Rajdhani ships no italic, `docs/ui/ue-frontend/ROADMAP.md:334`). Licence closed at
`ROADMAP.md:116`.

**Naming:** `F_<Family>` for the `UFont`, `FF_<Family>_<Weight>` for each face —
`F_Rajdhani`, `FF_Rajdhani_SemiBold`, `FF_RobotoCondensed_MediumItalic`. The `.ttf`/`.otf`
sources sit beside them.

**Never here:**
- A third font family. Two is the decision (`ROADMAP.md:680`); a third is a design ruling, not an
  import.
- A font whose licence is not OFL/permissive **and** recorded in `THIRD-PARTY-NOTICES.md` +
  re-copied to `Content/Legal/THIRD-PARTY-NOTICES.txt`. `Tools/verify_notices.py:98-116` fails
  the build on drift between the two.
- Type sizes, weights or letter-spacing. Those are text styles — C++ in
  `Source/Breachpoint/UI/Styles/`, never baked into a font asset.
- Rasterised text, SDF sprite sheets, or an image of a word.
