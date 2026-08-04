# `Content/UI/Fonts/`

Two families: **Rajdhani** (chrome, labels, the display ramp) and **Roboto Condensed** (body,
and the only italic — Rajdhani ships none, `docs/ui/ue-frontend/ROADMAP.md:334`).

Six faces, static TTFs from the Google Fonts catalogue, **unmodified**:
Rajdhani Regular / SemiBold / Bold, Roboto Condensed Medium / Medium Italic / SemiBold.

## The mapping that matters

`FSlateFontInfo` resolves a weight by `TypefaceFontName`. **If that FName is not an entry in
the composite font, Slate silently falls back to the first entry** — wrong weight, no error,
no log line. So the entries are named exactly as `Tools/gen_ui/figma_tokens.json` names the
weight. Read back off the assets on 2026-08-02, verbatim:

| Font asset | Figma weight | UE `TypefaceFontName` | FontFace asset |
| --- | --- | --- | --- |
| `F_Rajdhani` | Regular | `Regular` | `Rajdhani-Regular` |
| `F_Rajdhani` | Bold | `Bold` | `Rajdhani-Bold` |
| `F_Rajdhani` | SemiBold | `SemiBold` | `Rajdhani-SemiBold` |
| `F_RobotoCondensed` | Medium | `Medium` | `RobotoCondensed-Medium` |
| `F_RobotoCondensed` | Medium Italic | `Medium Italic` | `RobotoCondensed-MediumItalic` |
| `F_RobotoCondensed` | SemiBold | `SemiBold` | `RobotoCondensed-SemiBold` |

Note `Medium Italic` — one FName with a space, not a bold/italic flag. Slate has no synthetic
italic here; the italic is a separate face.

## Naming

`F_<Family>` for the composite `UFont`. Each `UFontFace` keeps its source file stem, so
`Rajdhani-SemiBold.ttf` sits beside `Rajdhani-SemiBold.uasset` — the same source-next-to-asset
rule the icons follow, and the one the importer already produces. Do not rename the faces to
`FF_*`: a re-imported `.ttf` would come back under its stem and you would own two of each.

## How they got here

`python3 Tools/gen_ui/import_fonts.py` — read its header first. The MCP surface has **no font
importer**; the editor's content-directory monitor turns a `.ttf` dropped in this folder into a
`FontFace` (plus a junk one-entry `<stem>_Font` wrapper the script deletes). The script builds
the composite fonts, then reads every typeface name back off the asset.

## Never here

- A third font family. Two is the decision (`ROADMAP.md:680`); a third is a design ruling, not
  an import.
- A fourth or fifth face in an existing family without a reason. Six is the set the type ramp
  in `figma_tokens.json` actually names; an unused face is texture memory for nothing.
- Type sizes, weights or letter-spacing. Those are text styles — C++ in
  `Source/Breachpoint/UI/Styles/`, never baked into a font asset.
- Rasterised text, SDF sprite sheets, or an image of a word.
