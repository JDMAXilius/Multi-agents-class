# TICKET — BN33: build BR_Aquarius from the kit (TERMINAL)

> OWNER: **terminal**. DEPENDS ON BN31 (the 15 assets).
> This is the first time the level exists in the engine.

## What lands

`/Game/Maps/BR_Aquarius` — 292 kit instances, 8 spawns, the rocket marker,
a nav volume. Everything from committed data; nothing hand-placed.

## Inputs (all committed, all validated)

- `Content/Data/aquarius_blockout_kit.json` — **the source of truth.**
  292 placements · 94 size variants · 6 families · 0.75 m grid.
  Carries `spawn_points` ALREADY converted into the kit frame,
  `piece_budget`, `repairs`, `unresolved_capsules`, `asset_map`.
- `Content/Data/blockout_kit_assets.json` — the 15 asset specs.
- `Content/Data/aquarius_manifest.json` — rocket node + bounds (metres,
  **+y NORTH** — see the frame trap below).
- Drawings for eyeball comparison: `docs/design/blueprints/
  breachpoint_aquarius/` — A-101/A-102/A-103 (plans), A-201 (sections),
  AK-101/AK-102/AK-301 (assembly), `fidelity_overlay.png`.

## Run it

```
python3 Tools/blockout/gen_aquarius_kit.py                 # (only if data changed)
python3 Tools/blockout/validate_aquarius_blockout.py --png # must exit 0
UnrealEditor-Cmd Breachpoint.uproject -run=pythonscript \
  -script="Tools/blockout/build_aquarius_blockout.py" -stdout -unattended -nosplash
```

`build_aquarius_blockout.py` is idempotent: everything it spawns is tagged
`BN30_BlockoutGenerated` and it deletes its own previous actors first. A
re-run is a clean rebuild — prove that (same actor count twice).

## Traps already found (do not re-discover them the hard way)

- **THE FRAME TRAP.** The arena manifest is **+y NORTH**; the kit is **+y
  SOUTH**. Spawns must come from the KIT (`kit["spawn_points"]`, already
  converted and snapped onto walkable ground); the rocket node gets
  `y_kit = bounds.y - y_manifest`. The builder does this — do not "simplify"
  it. `validate_aquarius_blockout.py` catches a regression as `P3_FRAME`.
- **Rotator argument order** is `unreal.Rotator(roll, pitch, yaw)`. The
  manifest stores named fields so this cannot be transposed silently.
- **Nav volume from Python may spawn without brush geometry** (known UE
  quirk). If nav does not build, scale the default brush or place that ONE
  volume by hand — and record it here.
- Ramps carry a real `pitch`; they are the only rotated-in-pitch pieces.

## Done when

- [ ] Map exists; actor count matches the manifest (292 + 8 + 1 + 1)
- [ ] Re-run proves idempotency (identical count, no duplicates)
- [ ] Every actor is tagged and foldered (`Blockout/Floors`, `/Walls`, …)
- [ ] Nav mesh BUILDS and covers the playable floor (screenshot the nav
      overlay — bots need this from day one)
- [ ] Screenshot set to the founder via `EditorAppToolset.CaptureViewport`:
      top-down · each named region (Base 1, Arena 1, Hallway 1/2/3, Bridge,
      Base 2) · one from a player-eye height in each arena
- [ ] Log records actor counts, the nav result, and whether kit assets or
      the cube fallback were used

## Log
