# TICKET — BP68: The PARAM rank insignia are pinned to (0,0) and cannot be exported

> STATUS: open — cut 2 Aug 2026 from the Figma→UE export pass. Found by a pre-flight
> validator, not by eye: the art looks correct in Figma and in any image viewer, and is
> only wrong once composited in engine. **This is a defect in the design source, not in the
> export pipeline.**

Founder directive during the export pass: *"make sure correct dimensions, background, etc —
make sure everything is proper before exporting to ue project."* This is the first thing that
check caught.

## The finding, measured

Figma file `yznvnVdOFDADaugZSeomfP`, page `Art / Insignia` (`48:2`) holds **two** rank
families of 16 components each, both 76×76:

| Family | Child offset | Verdict |
|---|---|---|
| `Rank / 01` … `Rank / 16` | e.g. `(7.6, 22.7)` 60.8×30.7 — equals the centred position exactly | **correct** |
| `PARAM Rank / 01 Recruit` … `16 Vanguard` | `(0, 0)` on **all 16** | **broken** |

Audited programmatically across all 32 components: **16 of 32 are pinned to top-left, and
they are exactly the 16 `PARAM` ones.** The `Rank / NN` set's child position matched its
computed centred position in every sample checked.

Consequence for an export: `PARAM Rank / 01 Recruit` at 4× produces a 304×304 PNG whose ink
bbox is `(0, 0, 192, 72)` — the mark sits in the top-left corner with ink touching x=0 and
y=0, and 76% of the canvas empty. In UMG that renders as an insignia jammed into the corner
of its slot, clipped on two edges.

## Why it matters more than it looks

**The `PARAM` set is the one carrying the rank names** — Recruit, Trooper, Corporal,
Sergeant, Specialist, Technician, Chief, Warrant, Lieutenant, Captain, Major, Colonel,
Commander, General, Marshal, Vanguard. The `Rank / NN` set has the correct art and no names.
So neither set is usable on its own.

## Ordering law

Step 1 is a founder call and gates 2. Step 3 is independent and can run first.

## Kickoff

- requires: `files-only` for step 1 (a Figma decision); `editor-live` for step 3
- Figma file `yznvnVdOFDADaugZSeomfP` page `48:2` still holds both families (verify — the art
  passes have restructured this file before)
- `python3 Tools/gen_ui/preflight_textures.py` exists and exits 0 on a clean texture
- owner_path: `Tools/gen_ui/`, `Content/UI/Icons/`

## Steps

1. **Decide which set is canonical.** Three options, and this is a design call:
   - **(a) Fix `PARAM` in Figma** — reposition all 16 vectors to centred. Correct at source,
     benefits every future export, and is 16 nudges. **Recommended.**
   - **(b) Ship the `Rank / NN` art with `PARAM` names grafted on by index.** Zero Figma
     edits; the generator already does this (`Tools/gen_ui`, export loop reads the `PARAM`
     name and exports the `Rank / NN` node). Leaves the defect in the file for the next
     person to rediscover.
   - **(c) Delete the `PARAM` set** if it is a superseded experiment. Its name suggests a
     parametric generation pass whose output was never repositioned.
2. **Re-export the 16 insignia** through `Tools/gen_ui` at the agreed scale and re-run
   pre-flight. All 16 must pass with zero edge ink.
3. **Audit the other art pages the same way** — `Art / Icons` (`68:2`, 64 nodes) and
   `Art / UI Glyphs` (`80:2`, 98 nodes). The same pinning defect may exist there and nobody
   has checked. The audit is one read-only `use_figma` call comparing each child's offset to
   its centred position.

## Done when

- [ ] Step 1 is decided and recorded here
- [ ] All 16 rank insignia pass `preflight_textures.py` — transparent, correct dimensions,
      no edge ink, neutral ink, anti-aliased
- [ ] `Art / Icons` and `Art / UI Glyphs` audited for the same defect, result recorded
      whether clean or not
- [ ] Imported into `Content/UI/Icons/Ranks/` with the four UI texture settings verified by
      read-back (`Tools/gen_ui/import_textures.py`)
- [ ] **Not a rung.** An imported texture is not a rendered one. A rung-3 claim needs the
      insignia drawn in a widget and looked at

## Notes

- **Do not "fix" this by trimming the export to the ink.** `ASSET-PIPELINE.md` §3 says trim
  to visible ink, but these are grid-designed marks: all 16 share a 76×76 frame so they align
  in a rank ladder. Trimming each to its own bbox makes 16 different sizes and breaks the
  column. The frame is the contract.
- The export path matters as much as the art. `download_assets` composites against the page
  backdrop and produced 41 unusable files before this was caught; the Plugin API
  `exportAsync` via `use_figma` renders in isolation. Recorded in
  `Tools/gen_ui/check_transparency.py`.

## Log

**2 Aug 2026 — cut.** Found while exporting `Art / Insignia`. The pre-flight validator
flagged `T_UI_Rank_01_Recruit` for edge ink; the follow-up read of the source showed the
`PARAM` children are all at `(0,0)` while the `Rank / NN` children are correctly inset.
Nothing was imported into UE — the validator refused, which is what it is for.
