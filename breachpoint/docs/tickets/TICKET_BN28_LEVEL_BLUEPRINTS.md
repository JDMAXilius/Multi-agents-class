# TICKET — BN28: the drawing board — engineering blueprints and Arena 02

> STATUS: OPEN — pipeline half BUILT and PROVEN on Arena01 (29 Aug, cloud lead);
> the Arena 02 design half in flight behind the Halo Infinite research pass.
> Founder directive (29 Aug): level design in the manner of a civil engineer /
> architect — layout, ALL heights and measurements, blueprint sheets in multiple
> views (top / front / side / 3-4), black and white, nothing abnormal; Halo
> Infinite multiplayer maps as the reference corpus; the result must feed the
> Unreal MCP / blockout pipeline AND land in Figma; iterate freely.

## The pipeline ruling (the ue-editor doctrine, extended to drawings)

The manifest is the source of truth. The .umap is one projection of it (the
blockout generator); **the blueprint sheets are another** — never hand-drawn,
never AI-imaged, because a drawing that does not come FROM the measured model
lies about the measurements the first time the model moves. Iterating the level
IS editing the manifest and rerunning one script. AI image generation was
considered for the sheets and REJECTED: a diffusion image cannot hold a
dimension chain; chromium-rasterized SVG can, exactly.

## Built — `Tools/blockout/gen_blueprint.py` (stdlib; --png shells to chromium)

- S1 PLAN — top view, architectural cut at +1.5 m (cut solids hatched, overhead
  decks dashed, low cover thin), 1 m/5 m grid, spawn circles with facing ticks,
  rocket star, grapple-anchor crosses with z, dimension strings, section mark
  A-A, north arrow, scale bar, title block.
- S2 ELEVATIONS — SOUTH (looking north) + WEST (looking east) + SECTION A-A
  through the fight corridor, level datums LVL0/1/2 at 0/4/8 m, height dims.
- S3 3/4 AXONOMETRIC — iso 30°, viewer NE, painter-sorted, leader-labeled.
- Reads ANY manifest of the arena schema (landmark footprints are authoritative
  AABBs; cover from profile cover_dims_m; walls from bounds).

Proven by rendering `arena_manifest.json` (Arena01) — sheets committed under
`docs/design/blueprints/breachpoint_vs01/` and reviewed at PNG level.

## Iteration ledger (the founder predicted iteration; it took three)

1. It. 1: axonometric projection INSIDE-OUT (a double Y-flip drew south/west
   faces of a NE view — every box a bowtie); center labels overprinted
   (THE CORE on MEZZANINE CATWALKS); rasterizer canvas 4x the sheet.
2. It. 2: projection rebuilt (hand-checked on a unit cube: z up-screen, x+y
   toward viewer, east+north+top faces), LabelBed collision nudging, window
   from the SVG's own size — found chromium's headless viewport runs short of
   --window-size and clips ~40 px (the title blocks).
3. It. 3: capture window over-provisioned (+24/+70); all three sheets clean.

## Still owed (this ticket stays open)

- [ ] The Halo Infinite research digest lands in this Log (agent in flight)
- [ ] `arena02_manifest.json` — the new arena, full schema, kickoff-gate clean
      (`arena_plan.py --verbose` accepts it: >=8 spawns @ >=8 m, <=35 m
      sightlines, named rocket node, >=2 grapple points per upper position)
- [ ] Its blueprint sheets, founder-reviewed through iterations
- [ ] The sheets in Figma (top/front/side/3-4 as frames; SVG import)
- [ ] Handoff: terminal builds Arena 02 from the manifest via the SAME
      build_arena.py (new -Manifest arg or a copy step — terminal's call),
      then editor screenshots close the loop

## Log

### 29 Aug — pipeline half built and proven (cloud lead)

- Everything above. The generator is manifest-agnostic on purpose: Arena01
  proved it so Arena 02 iterates on a proven instrument (the AIB10 lesson —
  instrument first).
