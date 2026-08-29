# Halo Infinite — the levels (spatial gazetteer, BN28)

> 29 Aug 2026. Compiled from documented callout guides via search snippets (the
> cloud proxy blocks all image hosts and most direct fetches — every fact below
> is sourced; unestablished positions are declared, never guessed). The drawn
> boards live in `docs/design/reference/boards/` and are generated from
> `reference/halo_maps.json` by `Tools/blockout/gen_halo_boards.py`.

## Boards drawn (adjacency + floors + pads established)

| Map | Shape | Floors | Spawns | Center prize | Long-range prize |
|---|---|---|---|---|---|
| **Aquarius** | square, mirror-sym | 2 | Yellow/Blue Base | camo/OS on Top Mid bridge | shock rifle mid-Hydro |
| **Live Fire** | rect, ASYM | 2.5 | House vs Tower | camo/OS on Turbine platform | sniper/skewer mid-Hallway |
| **Recharge** | indoor, ASYM | 3 | Elevator vs Long Hall | camo/OS in the Pit (bottom) | — (sword on isolated platform) |
| **Empyrean** | rect, mirror-sym (The Pit) | 2 | East/West Base | cindershot in Center Pit | rockets mid-corridor, 2 sniper pads |
| **Bazaar** | town, mirror-sym | 1 + upper hall | East/West Base | camo+OS near Truck; rockets on Tower | — |
| **Streets** | city block, ASYM | 1 + balconies | Subway vs Station Sq | SPNKr/cinder mid-Main-Street | stalker on Subway Balcony |
| **Argyle** | ship interior, mirror-sym | 3 | 2 bases | camo on floating platform | snipers on pit-walkway corners |

Key spatial facts each board encodes (all sourced on the sheet):
- **Aquarius**: base -> courtyard -> Bottom Mid -> opposite courtyard; Top Mid
  bridge above with the power-up; Hydro hall (Utility<->Utility) one flank,
  Planters-under-Pump (Refrigeration<->Refrigeration) the other.
- **Live Fire**: Turbine field at the top, Yard at the bottom; Hallway OVER
  Tunnel as the double-decker mid; Canal beneath Tower; Nest links Green
  Building to Tower.
- **Recharge**: the ring — Whirlpool Dam -> Orange Pipes -> Maintenance Bay ->
  Long Hall, with Attic off the pipes hall; Pit (bottom) -> Blue Pipes -> lower
  Hydro; Control Room two-story between Elevator and Hydro; sword/hammer on a
  platform BEHIND Hydro, off the loop.
- **Empyrean**: The Pit's geometry — facing sniper towers, rockets in the mid
  corridor, heatwave in the old sword room, underpass beneath it, OS moved to
  a balcony over the open (deadly) edge.
- **Bazaar**: three routes at two heights — alleys south, courtyard-market-
  courtyard middle, indoor Upper Hall north; camo AND OS both near Truck.
- **Streets**: Main Street spine with Old Town and Commercial District flanks;
  power weapon dead-center of the spine.
- **Argyle**: one long central hallway with a true base-to-base sightline;
  pit-walkways with snipers; grav-lift battlements holding grappleshots.

## Documented but not drawn

- **Detachment** (drawable on request — Halopedia gives the full verbal plan:
  man-cannons to a rocket island with a teleporter return that needs the
  grapple; camo perch over the courtyard; heatwave+repulsor indoors).
- **Solitude** (= H5 Plaza geometry, spawn sides unestablished), **Interference**
  (= H5 The Rig, borrowed wholesale), **Forbidden** (Dried vs Overgrown sides
  named, interior unestablished), **Catalyst** (symmetric tube, light-bridge
  center, callouts unretrievable), **Chasm / Illusion / Prism / Forest /
  Origin** (concept-level only). BTB out of scope.

## The patterns the whole roster repeats (for whenever we design again)

1. Center prize elevated and exposed (Top Mid / Turbine platform / Tower) OR
   sunken and surrounded (the Pit) — never on flat safe ground.
2. Long-range prize on the most exposed direct lane, mid-map, every time.
3. CQB prize (sword/hammer) on an isolated appendix off the loop.
4. Double-decker mid lanes (Hallway/Tunnel, Top Mid/Bottom Mid) are the
   signature Infinite move — a vertical sandwich instead of a third lane.
5. Asymmetric maps still balance by corner-vs-corner spawns with equal route
   counts (House 3 ways vs Tower 3 ways).

Sources: per-map source lines are printed on each board; the full link list is
in the BN28 ticket log and REFERENCE-LINKS.md.
