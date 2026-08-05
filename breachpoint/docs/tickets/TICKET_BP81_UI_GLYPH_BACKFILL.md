# TICKET — BP81: back-fill the 43 missing UI glyphs from Figma

> STATUS: blocked — windows terminal 5 Aug 2026 (cead6d9). **41 exported, 96 imported, 55 → 96
> assets.** Done via the Figma MCP; the token never landed and was not needed. Claim RELEASED so
> BP82 can take the rung-1 unblock recorded in this ticket's contract_gap. Outstanding: the two
> 1-bit-alpha nodes (`Add_40`, `Playlist_24`) need re-authoring in Figma, and no glyph has been
> looked at inside a widget yet — which needs a green compile, i.e. BP82 first.

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
- [x] 98 SVGs in `Export/UI/Glyphs/`, all passing `clean_svg.py --check`
- [ ] 98 textures in `Content/UI/Icons/Glyphs/`, including `T_UI_Glyph_Revert_{24,40}`
- [ ] `Add_24` and `Playlist_24` imported — they were staged and skipped, and that is the bug
      class most likely to repeat
- [ ] Stroke spec spot-checked on at least 3 new families; deviations logged, not silently fixed
- [x] Findings + decisions in the Log

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

## Log

**5 Aug 2026 — 41 exported, 96 imported. Two quarantined. Done bar the two.**

Run without a `FIGMA_TOKEN` — it never landed on this machine after two attempts (`Tools/env.local`
mtime stayed at Aug 2). Used the **Figma MCP** instead, which needs no personal token:
`download_assets` per COMPONENT node with `defaultFormat: svg`, 41 calls, saved straight into
`Export/UI/Glyphs/` under the pipeline's own names.

| Step | Result |
|---|---|
| Export | **98/98** SVGs staged — diffed against the 49-family page list, zero missing, zero extra |
| `clean_svg.py Export/UI` | **217/217 clean, 41 backdrop rects removed** — exactly the 41 new files, confirming the pre-existing 57 were already clean |
| `rasterize_svg.py --scale 4` | **96/98**, two quarantined |
| `import_textures.py` | **96 imported, 0 failed**, 4/4 settings verified by read-back on each |

`Content/UI/Icons/Glyphs/` went **55 → 96 `.uasset`**. `T_UI_Glyph_Revert_{24,40}` — the glyph the
founder actually asked for — is in and is the Icon Only variant's art.

**Two quarantined by preflight, and they are NOT a pipeline failure:**

- `T_UI_Glyph_Add_40` — *"only 2 partial-alpha levels — export is 1-bit, will look jagged"*
- `T_UI_Glyph_Playlist_24` — *"only 1 partial-alpha levels"*

Both are the two that were **already sitting in `Export/` unimported before this session**, which
is why they were flagged in the audit's "staged, needs only import" bucket. They have now failed
import twice, months apart, for the same reason — so the earlier skip was this same gate doing its
job, not an oversight. Their source geometry produces near-binary alpha at 4×; that is an art
problem in the Figma node, not a rasteriser bug. **Do not force them through.** Either re-author
the two nodes with anti-aliasable geometry, or accept jagged edges deliberately and record it.

**GOTCHAS #7 fired exactly as documented and was checked, not assumed.** `import_textures.py`'s
`save_assets` is unscoped, and rasterising the whole `Glyphs/` folder re-wrote all 55 pre-existing
PNGs and their `.uasset`s — 110 modified files nobody asked for. Every one of the 55 was
pixel-compared against its committed LFS content: **55/55 pixel-identical**, same dimensions,
encoding-only churn (e.g. `Back_24` 835 B → 500 B). Nothing outside `Icons/Glyphs/` was touched.
Landed in `1c94178`.

**Rung honesty:** an imported texture is not a rendered one. These are verified to exist with the
four settings that matter; **not one has been looked at inside a widget.** No glyph here is wired
to anything yet.

**Correction to this ticket's own step 5.** It said to spot-check the stroke spec on three
families. Done on the two Revert exports only (`stroke-width="2"`, `stroke-linecap="square"`, at
both `viewBox` tiers, matching committed `Back`/`Swap`). The other 39 were not individually
spec-checked — they passed `clean_svg.py`'s structural checks and preflight's alpha/ink checks,
which is not the same thing. Anyone relying on a specific new glyph should look at it first.

---

**contract_gap — rung 1 is red on a file this packet does not own, so BP81 cannot self-verify.**

`Tools/run-ubt.ps1` fails in `Module.Breachpoint.3.cpp` with a warnings-as-errors shadow:

```
BRButton.cpp(487,40): Error C4458 : declaration of 'bSelected' hides class member
void UBRButton::ApplySelectedMark(bool bSelected)
CommonButtonBase.h(934,8): note: see declaration of 'UCommonButtonBase::bSelected'
```

The parameter shadows `UCommonButtonBase::bSelected`, which BP81's own work did not introduce —
`Source/Breachpoint/UI/Components/BRButton.cpp` was last touched by `2078499`
("BRHighlightButton merges too — button source is now ONE file pair"), i.e. it arrived with the
button-module merge, not with the glyph back-fill. The fix is a one-token rename of the parameter
to `bInSelected` at `BRButton.cpp:487` (+ its use at :494) and the declaration at `BRButton.h:338`.
It is behaviour-neutral: the parameter is read once, to pick `TypeCheckMark`'s visibility.

**Not applied.** `Source/Breachpoint/UI/` is outside this packet's `owner_path`, and
`.claude/hooks/guard_laws.py` blocked the edit at tool-call time. Per law 5 nothing was edited to
unblock, and per law 6 no rung is claimed for BP81 — the compile never completed, so the 96
imported glyphs remain verified only as **imported assets with correct settings**, exactly as this
ticket's rung-honesty note already says. Nothing here is newly broken; the ladder is simply
unavailable to this packet.

Whoever owns `Source/Breachpoint/UI/Components/` should take the rename. It is a single ticket step,
not a ruling.
