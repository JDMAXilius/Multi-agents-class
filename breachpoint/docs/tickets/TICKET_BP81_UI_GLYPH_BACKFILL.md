# TICKET — BP81: back-fill the 43 missing UI glyphs from Figma

> STATUS: in-progress — mac terminal 5 Aug 2026. Blocked on ONE thing: a `FIGMA_TOKEN` in
> `Tools/env.local`. Everything else is ready; the measurement below is done and reproducible.

Cut 5 Aug 2026 from a founder request against Figma **Menu Row** (`12:724`): *"make sure we have
all the image assets of the buttons and export the ones we still don't have."* The audit that
followed found the gap is **43 assets, not one** — and that the second named example was a false
alarm.

## What the audit found (measured, not assumed)

The Figma page **Art / UI Glyphs** (`80:2`) publishes **49 glyph families × 2 sizes = 98 assets**.
`Content/UI/Icons/Glyphs/` holds **55**. Nothing in `Content/` is stale — the delta is purely
missing art.

| Bucket | Count | Detail |
|---|---|---|
| Already staged in `Export/UI/Glyphs/`, needs only import | 2 | `T_UI_Glyph_Add_24`, `T_UI_Glyph_Playlist_24` |
| Never exported | 41 | 21 families, listed below |
| In `Content/` but not on the page (stale) | 0 | — |

Families never exported: **Add · Close · Combat · Countdown · Defence · Delete · Event · Friends
· Objective · Rank · Recommended · Reset · Restart · Revert · Search · Server · Settings · Share
· Speaking · TableView · Warning**

**`Revert` is the only one the Menu Row itself needs today.** It is `Glyph / Revert / 24`
(`80:498`) and `/ 40` (`80:502`), and it appears in the frame as instance `12:979` (32 × 32)
inside `Status=Idle, Alignment=Center, Type=Icon Only` (`12:977`).

**The "Slayer skull" named in the request is NOT missing.** Figma `12:1175` is a composite —
Shield + two Swords + Skull at 120 × 120 — and `T_UI_Icon_GametypeV2_Slayer` already exists and
is already wired into the rebuilt `WBP_ButtonImage` at exactly 120 × 120 (see BP80's build
receipt). Nothing to export. Recorded here so it is not re-investigated.

**Every other Menu Row image resolves to something we already have:** `MenuRow_Arrows`,
`MenuRow_Hatch` (the 69-line Dig Down hatch), `MenuRow_Triangle` (Drop Down `Polygon 13`, 6 × 6),
`MenuRow_Dot` / `MenuRow_Tick` (Slider), the four `Sides/` lines, and the 20 × 20 Slayer in Map
Voting. The leading `Icon` slot is `hidden="true"` in **all seven** row types, so it needs no
asset at all.

## Kickoff (machine-checkable)

- requires: **files-only** for the export; **editor-live** only for the final UE import step
- `FIGMA_TOKEN=figd_...` present in `Tools/env.local` (read-only scope is sufficient — the
  script never writes to Figma). `Tools/env.local` is gitignored (`.gitignore:26`) and untracked,
  so the token cannot be committed. **This is the one blocker.**
- `python3 mcp-ui/gen_ui/figma_export.py --list-pages` succeeds (proves the token works)
- owner_path: `Export/UI/Glyphs/`, `Content/UI/Icons/Glyphs/`, `docs/ui/receipts/`,
  `docs/tickets/TICKET_BP81_UI_GLYPH_BACKFILL.md`

## Steps (in order)

1. **Audit before exporting.** `python3 mcp-ui/gen_ui/figma_export.py --audit 80:2` — confirm it
   reports 49 families / 98 assets. If it disagrees with the table above, the page changed since
   5 Aug and this ticket's numbers must be re-cut before anything lands.
2. **Export the page.** `python3 mcp-ui/gen_ui/figma_export.py --export 80:2`. `PAGES` already
   maps `80:2 -> ("Glyphs", 4)`, so naming and the 4× scale come from the pipeline, not from a
   flag. Do not hand-name anything: `asset_name()` is what turns `Glyph / Table View / 24` into
   `T_UI_Glyph_TableView_24`.
3. **Clean and verify the SVGs.** `python3 mcp-ui/gen_ui/clean_svg.py Export/UI` — Figma
   composites every export against the page backdrop, which arrives as a full-canvas `<rect>`
   before the content group. Confirmed present on this page: the exports carry
   `<rect width="24" height="24" fill="#17171A"/>`. The cleaner identifies it structurally, not
   by colour. Then `--check` to prove none survived.
4. **Rasterize and import** via `rasterize_svg.py` + `import_textures.py`, with
   `preflight_textures.py` first. Editor needed only here.
5. **Spot-check the spec.** House rule is a 2 px absolute stroke with square terminals at both
   tiers. Verified already on the two Revert exports: `stroke-width="2"` at both `viewBox="0 0 24
   24"` and `"0 0 40 40"`, `stroke-linecap="square"` — matching the committed `Back`/`Swap`
   glyphs. Any family that comes out different is a finding, not a fix-in-place.

## Done when

- [ ] `--audit 80:2` output recorded in the Log
- [ ] 98 SVGs in `Export/UI/Glyphs/`, all passing `clean_svg.py --check`
- [ ] 98 textures in `Content/UI/Icons/Glyphs/`, including `T_UI_Glyph_Revert_{24,40}`
- [ ] `Add_24` and `Playlist_24` imported — they were staged and skipped, and that is the bug
      class most likely to repeat
- [ ] Stroke spec spot-checked on at least 3 new families; deviations logged, not silently fixed
- [ ] Findings + decisions in the Log

## Notes

- **Two Revert SVGs are already downloaded and verified** (via the Figma MCP, which needs no
  token) and are sitting in this session's scratchpad. They were NOT landed: `Export/` was
  outside the then-active packet's owner_path, and mixing one hand-pulled asset into a
  pipeline-generated family is how naming and scale drift starts. Re-export them with the rest.
- **Export the COMPONENT, never the instance.** Pulling instance `12:979` returns the art
  composited with its parent frame's chrome — a `#F5F5F5` backing, the Menu Row frame's black
  rect, and Figma's purple `#7B61FF` dashed component boundary. Pulling component `80:498`
  returns clean house-format art. This cost a round trip; it should not cost a second one.
- Out of scope: the `Art / Icons` (`68:2`) and `Art / Insignia` (`48:2`) pages. Same script, same
  method, different ticket — do not widen this one.
- Related: `TICKET_BP80_BUTTON_MODULE_ASSETS.md` rebuilt the nine button WBPs that consume these
  glyphs. BP80 is in-progress and its remaining boxes are decisions, not execution.
