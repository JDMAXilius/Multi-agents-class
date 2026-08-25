# RESEARCH — Bot traversal: how a bot learns it can drop, jump, and cross

> Written 25 Aug 2026 by the cloud lead, from the founder's question: *"is that something I need
> to add directly in the level, or can the AI know automatically from the navigation mesh?"*
>
> **Status: RESEARCH, not law.** Every engine API named here came from a web search summary,
> because this session has no engine and Epic's doc hosts are blocked by its egress proxy. The
> unverified list at the bottom is the honest inventory of what must be transcribed from real
> headers before a line is written. `TICKET_BN12_NAVLINK_PROBE` is that transcription.

## The founder's ruling, 25 Aug

Bots derive traversal — drop down, jump up, platform to platform — **from the navigation mesh
itself**. **Nothing is placed in the level** beyond the nav bounds volume it already carries. No
hand-placed link actors, and no link data in the arena manifest either.

This ruling is what makes the rest of the document short. It rules out two approaches that were
on the table on 25 Aug (hand-placed `ANavLinkProxy`, and links emitted from
`Content/Data/arena_manifest.json` through the blockout generator) and it is a good ruling: the
derived approach is smaller, has no binary files in it, and survives a map regeneration.

## The problem, stated from measurements

Five call sites in `Source/BreachpointNext/AI/` spend a jump. Every one is **reactive**:

| call site | owning node | trigger |
| --- | --- | --- |
| wedge jump | `FBNMoveToTargetTask` | path says Moving, bot is not moving |
| blocked-leg jump | `FBNMoveToPointOfInterestTask` | stopped short of its point |
| pinned jump | `FBNStrafeTask` | cornered mid-fight |
| the juke | `FBNStrafeTask` | every Nth sidestep |
| evade jump | `FBNEvadeBlastTask` | on the way out of a blast |

Not one plans a route that requires a jump, and not one can decide to drop off a ledge. Going
somewhere by jumping is **absent by design, not mis-tuned** — no tier number changes it.

Two separate reasons a bot does not jump today, and they need different fixes:

1. **Three of the five cannot fire at all.** `FBNStrafeTask` and `FBNEvadeBlastTask` are not in
   the shipped `ST_BNBot` — that asset's content last changed 20 Aug, and those nodes landed
   23–24 Aug. `TICKET_BN10_BOT_ASSETS` is the fix and is unrelated to this document.
2. **Traversal was never written.** That is this document.

**Why it is a navigation problem, not a behaviour one.** The navmesh is the bot's reachability
oracle. If the mesh does not connect a ledge to the floor below it, the pathfinder returns no
path, the brain believes it, and the bot never considers the drop. No StateTree work fixes that,
because the behaviour layer is never told the drop exists.

## The mechanism, which is the founder's instinct

A navmesh is polygons. **An edge with no neighbouring polygon is a border edge — that is a
ledge, by definition.** It is the entire input:

1. Enumerate border edges.
2. Sample along each one.
3. Trace outward and down (a fall) or outward and up (a jump).
4. Land on navmesh on the far side, inside the fall/jump budget → emit a link.

Nothing is authored and nothing is placed; it is re-derived from the mesh on every build. This is
what Detour's own `dtNavLinkBuilder` does underneath UE's generator, and it is what a hand-rolled
generator would do.

## What UE 5.5+ already ships

**Automatic Navigation Link Generation.** A `Nav Link Jump Down Config` section
(`FNavLinkGenerationJumpDownConfig`) on the navmesh, plus a link proxy class derived from
`UBaseGeneratedNavLinksProxy`. Links are built **during tile generation**; Epic's own note is to
keep jump length reasonable because it costs build time.

Three things to hold onto:

- **It requires nothing in the level.** The proxy class is a *class named in settings*, not an
  actor anyone places. This already satisfies the founder's ruling for the downward case.
- **It is downward only** — as far as every source found says. That is the single most
  consequential unverified claim in this document; BN12 Q1e settles it from the code.
- **It is flagged Experimental** through 5.8, with an open community report that `Jump Max Depth`
  does not behave as documented. These are numbers to measure, never to trust.

**Project settings are the right home, not the actor.** Epic's guidance runs opposite to
intuition here: editing the navmesh actor **in the level** is the thing that does not stick —
settings visibly reset on reload, because Project Settings is what seeds new navmesh actors. So
`Config/DefaultEngine.ini` is both the C++-first path and the durable one. Today
`DefaultEngine.ini` has **no navigation section at all** — nothing about this project's navmesh
has ever been configured from data.

## What Halo Infinite does, published

343's Forge shipping requirements document their bot navigation model, and it maps 1:1:

| Halo Infinite | BREACHPOINT / UE 5.8 |
| --- | --- |
| jump hint, one-way (down) | auto-generated jump-down link |
| jump hint, two-way (up + down) | `UBNNavLinkForge`, derived (see step 2) |
| "building the nav mesh will auto-generate jump hints" | tile-generation link building |
| Bot Nav Marker · Explore | `ABNPointOfInterest` — already exists |
| Bot Nav Marker · Hide | R10's cover rosette — already exists |

Worth knowing for morale: Halo Infinite shipped with a bug where bots "could have trouble
jumping, climbing, clambering, or otherwise scaling edges, **including stairs**."

## The approach, in cost order

**Step 1 — turn on the built-in generator and MEASURE it.** It is the derive-from-navmesh system
for the downward case, it needs no custom code, and measuring first is the only thing that keeps
step 2 bounded. If it links the gantry lips to the floor, the half the founder asked about is
finished.

**Step 2 — `UBNNavLinkForge`, scoped to the gap step 1 leaves.** Hook the navmesh-generation-
finished delegate, pull border edges, pair them by trace, spawn links. Spawned actors are runtime
objects and are **never saved into the level**, so the ruling holds. Upward and
platform-to-platform only — whatever step 1 already covers, this does not touch.

**Step 3 — a drop task as the floor.** `FBNDropDownTask`: target is below me, the fall is
survivable, there is floor under this ledge. Needs no navmesh change. Its ceiling is real and
should be stated whenever it is discussed: it is local and reactive, so it can take a drop the
bot is standing on but can never make the *pathfinder* route through one. A bot with only step 3
will never plan "go left, drop, flank."

**A ceiling on step 2 that is a game-design fact, not an engine one:** BN has **one jump and no
mantle or clamber** — `UBNGA_Jump` is a single hold-to-height jump. The forge can only emit
up-links a bot can actually make. The 4 m terrace lips are plausible; the 8 m gantries are
almost certainly not, which is what the grapple points are for.

## Files this would touch

Steps 1 and 3 are cheap; step 2 is the only real build.

| step | | file |
| --- | --- | --- |
| 1 | add | `Source/BreachpointNext/AI/BNGeneratedNavLinks.h` / `.cpp` |
| 1 | change | `Config/DefaultEngine.ini` — first navigation section in the project |
| 2 | add | `Source/BreachpointNext/AI/BNNavLinkForge.h` / `.cpp` |
| 2 | add | `Source/BreachpointNext/Tests/BNNavLinkForgeSpec.cpp` |
| 2 | change | `Config/DefaultGame.ini` — the forge's budgets |
| 3 | change | `AI/BNBotStateTreeTasks.h` / `.cpp`, `AI/BNBotAuthoring.cpp` (a `Drop` state), `AI/BNBotController.h` / `.cpp`, `Data/BNDataRows.h` (two knobs on `FBNBotTuningRow`) |

`BreachpointNext.Build.cs` needs **no change** — `NavigationSystem` is already a public
dependency. Step 1 adds **no `.uasset`**: generated links are built into the navmesh, not saved
as assets. Step 3 rebuilds `ST_BNBot`, so it rides in the same editor session as BN10.

## UNVERIFIED — every name below is from a search summary

Nothing here has been read from a header. This list is BN12's scope, and it is written out in
full so that no one mistakes a plausible name for a checked one.

- `FNavLinkGenerationJumpDownConfig` — the struct, and every field in it. Only "Jump Length" and
  a vertical-trajectory-sampling field were ever named by a source.
- `UBaseGeneratedNavLinksProxy` / `GeneratedNavLinksProxy` — which is the class to derive from,
  whether it is a `UObject` or an `AActor`, and what a subclass must override.
- The ini section name. `[/Script/NavigationSystem.RecastNavMesh]` is a **guess**. Whether these
  settings are `UPROPERTY(config)` at all decides whether the C++-first path exists.
- **Downward-only.** Asserted by every source, proven by none.
- `FRecastDebugGeometry::bGatherNavMeshEdges`, `ARecastNavMesh::GetDebugGeometry` /
  `GetDebugGeometryForTile`, `NavMeshEdges`. **This is the load-bearing risk.** There are reports
  of `LNK2019 unresolved external symbol` on `GetDebugGeometry`, meaning it may be editor- or
  debug-only. If a Game target cannot call it, step 2 needs a different door into the navmesh —
  and that failure lands at PACKAGE time, not in PIE.
- `ANavLinkProxy`, `UNavLinkCustomComponent`, `INavLinkCustomInterface`, and whatever registers a
  custom link at runtime.
- Any delegate that fires when navmesh generation finishes.

Three project facts also unverified from the cloud, because every `.uasset` and `.umap` in that
clone is a **Git LFS pointer**: whether `BR_Arena01` actually carries a `RecastNavMesh` (bots
path today, which is evidence, not a read-back); whether the navmesh is static or dynamic; and
whether BN applies fall damage, which the gantry lips at 800 uu make load-bearing.

## Prior art

A public UE generator does exactly this from recast data —
[swastik1992/Navigation-Link-Generator](https://github.com/swastik1992/Navigation-Link-Generator).
Its author calls it experimental and not thoroughly tested, so treat it as proof the technique is
real, not as something to depend on.

## Sources

- [Automatic Navigation Link Generation](https://dev.epicgames.com/documentation/en-us/unreal-engine/automatic-navigation-link-generation)
- [UBaseGeneratedNavLinksProxy — UE 5.8 API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/NavigationSystem/UBaseGeneratedNavLinksProxy)
- [Jump Max Depth does not behave as expected](https://forums.unrealengine.com/t/5-5-5-6-automatic-nav-link-generation-jump-max-depth-doesnt-behave-as-expected/2538152)
- [Community Forge Map Requirements — Halo Support](https://support.halowaypoint.com/hc/en-us/articles/14796740242708-Community-Forge-Map-Requirements)
- [How Halo Infinite's bots became so ruthless — Game Informer](https://gameinformer.com/preview/2021/11/15/how-halo-infinites-bots-became-so-ruthless-and-helped-343-develop-multiplayer)
- [LNK2019 on NavMeshData::GetDebugGeometry](https://forums.unrealengine.com/t/error-when-trying-to-use-the-navmeshdata-getdebuggeometry-function-error-lnk2019-unresolved-external-symbol/458175)
- [Get edges of navmesh tiles with tile index/poly ref?](https://forums.unrealengine.com/t/get-edges-of-navmesh-tiles-with-tile-index-poly-ref/459679)
