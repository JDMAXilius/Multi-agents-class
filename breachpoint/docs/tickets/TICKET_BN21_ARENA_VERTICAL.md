# TICKET — BN21: the arena's stairs are solid boxes; the map is one-way downward

> STATUS: open — cut 26 Aug 2026 by mac terminal, from the AIB9 traversal measurement.
> Needs a founder ruling before any build: this is LEVEL GEOMETRY, not a bot fix.

## What the manifest intends

`Content/Data/arena_manifest.json`, verbatim:

- `stair_west` x[13,16] y[19,21] z[0,4] — *"ground-to-mid stair volume"*
- `stair_east` x[24,27] y[19,21] z[0,4] — the mirror
- **The Gantry**: *"nothing walkable continues to height 8 — the Gantry above is
  grapple-only by design"*

So ground↔mid is MEANT to be walkable, and mid→upper is grapple-only on purpose.

## What is actually built

`arena_plan.py` emits `landmark_stair` as a single AABB, and `build_arena.py`'s
`spawn_box` spawns **one scaled cube**. `stair_west` is therefore a solid
3m x 2m x **4m block** — a 400uu wall.

```
character MaxStepHeight :  45uu
character jump apex     :  90uu   (JumpZVelocity 420, gravity 980)
stair rise              : 400uu   -> unclimbable by a factor of 4.4
```

The manifest's own `doubts` list already suspected it: *"real ramp geometry may occlude
them"*.

## The consequence, measured

There is **no walkable route from ground to mid** for anyone — bot or player. Mid→upper is
grapple-only and the grappleshot is **not implemented** (one mention in `BNCollision.h`;
no ability, no input tag, no AI verb). So the whole map is one-way downward: SP7/SP8 spawn
on the Gantry, bodies drop off, and nothing returns.

AIB9's instrument, one match: 80,237 move refusals, 97% with the goal BELOW the bot. The
drop half is fixed (JumpMaxDepth 800 -> 1000, refusals/switch 9.54 -> 0.42). The climb half
cannot be fixed by any navlink — a link promising a 400uu climb against a 90uu apex is a
promise the body cannot keep, and the bot would launch and fall. That is the same class of
breach this project refused when raising JumpLength.

## The options (founder's call — do NOT pick one unilaterally)

1. **Make the stair volumes real steps.** Nine steps at <=45uu rise, 33uu tread inside the
   existing 3m run — walkable with no jump at all, navmesh covers it natively, and it
   restores ground<->mid for bots AND players. Smallest change; honours the manifest's
   stated intent.
2. **Implement the grappleshot.** The map's designed vertical answer, and the only thing
   that makes the Gantry reachable as authored. Much larger.
3. **Accept one-way down** and stop spawning anyone on the Gantry (drop SP7/SP8), so the
   upper tier becomes a drop-in vantage rather than a spawn that strands.

Option 1 and option 3 are compatible and both are small. Option 2 is a feature.

## Kickoff

- requires: engine-installed, editor-live (the rebuild), and a founder ruling first
- owner_path: `Tools/blockout/arena_plan.py`, `Content/Maps/BR_Arena01.umap` (BINARY —
  law 7: one owner, lock before editing), this ticket

## Done when

- [ ] Founder ruling recorded here
- [ ] If option 1: steps emitted by the committed script, never hand-placed (law 7)
- [ ] Ground<->mid traversal proven live: bots observed ascending, five-log discipline
- [ ] The AIB9 climb-band failures (dz +300..+499, 501 in the sample) fall to ~0

## Log
