# Art pass — Stage 1: strip the hidden underlays

Figma file `yznvnVdOFDADaugZSeomfP`. Script:
`scratchpad/scripts/strip-underlays.js` (not committed to the game repo — it is a one-shot
ops script; run it from the Figma plugin console, it is safe to re-run).

## What this stage does

The 75 screens were cloned 1:1 from the reference file, which means they carried over the
original designer's working scaffolding: screenshot references left in the frame with
visibility switched off. `SCREEN-BUILD-SPEC.md` §6 names them. The script walks the twelve
`FE / …` pages, matches nodes by name, and removes the ones that qualify:

| Pattern | Removed |
|---|---|
| `Xbox …` (2022/2023 console screengrabs) | always |
| `Screenshot …` (2023/2024) | always |
| `IMG_…` (`IMG_3979`, `IMG_3980`) | always |
| `Update Text` — third-party attribution string, must not ship | always |
| `image <digits>` (`image 9`, `image 55`, `image 60`, …) | **only if hidden** |
| `Ref` | **only if hidden** |

It records page, screen, node name, type, size and hidden-state for everything it touches
and returns a manifest (capped at 200 entries, with the true totals alongside). Failures —
a page that will not load, a node that will not delete — go into `notFound` rather than
throwing, so one bad node cannot abort the run. A second run finds nothing and reports zero.

Deliberate safety valve: a **visible** `image NN` or `Ref` may be real content, not
scaffolding. Those are collected into `visibleMatches` and reported, never deleted. A human
reads that list. Do not automate it away.

## What this stage does NOT do

**It does not replace any Halo artwork.** Nothing that ships is fixed by this stage. Every
visible Halo-owned asset in the §6 table — the season key art, `Heroes of Reach`, the UNSC
mark, the seven rank insignia, the Xbox controller diagram, the ~180 `Items` tiles, the five
commendation cards, the `Start Menu Background` — is still sitting in the file, untouched, on
purpose. It also does not touch strings, layer names, or geometry.

This stage is the cheap one. It exists so the later stages are counting real work instead of
tripping over hidden PNGs.

## The rest of the art pass, in order

| # | Stage | Scale |
|---|---|---|
| 1 | **Strip hidden underlays** — this doc | minutes; one script run |
| 2 | **Inventory the visible Halo-owned art** — walk the same pages, emit every image fill and Halo-owned string with node id, size, screen and intended replacement family. §6 is the sketch; this is the ledger. | half a day; ~250 nodes |
| 3 | **Nomenclature pass** — Halo-owned words out of layer names and copy (UNSC, Spartan, Heroes of Reach, 343 mode names) and Breachpoint tokens in. Mechanical but wide. | 1–2 days; hundreds of text nodes |
| 4 | **Produce the replacement art** — the ladder in `ASSET-METHODS.md`, the families in `ART-PROMPT-LIBRARY.md` (A scene plates, B ranks, C modes, D medals, E emblems), weapon silhouettes rendered from the meshes per `WEAPON-RENDER-PLAN.md`, small-tier redraws per `ICON-CONSTRUCTION-SPEC.md`. This is the whole job — the other six stages together are smaller than this one. | weeks; several hundred assets, plus the icon-set attribution currently owed (`ASSET-METHODS.md` Tier 2) |
| 5 | **Swap the art into Figma** — replace fills in place, geometry untouched, batched by family so each batch is reviewable. | 2–3 days, gated on stage 4 landing family by family |
| 6 | **Export to UE** — naming, texture import settings and destinations per the render plan; WBP bindings per the UI pipeline. | days; scriptable, one-time rig |
| 7 | **Verification** — no Halo-owned pixel or string survives in Figma or in `Content/`; attribution ledger settled. | half a day, but it is the gate on shipping any of it |

Stages 2 and 3 can run in parallel with the front of stage 4. Stage 5 cannot start before a
family is finished in stage 4, and stage 7 gates everything.
