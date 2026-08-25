# TICKET — BN12: two questions the engine has to answer before any traversal code is written

> STATUS: open — cut 25 Aug 2026 by the cloud lead, at the founder's direction. Needs the ENGINE
> ON DISK and nothing else — no live editor, no PIE. **This ticket writes no C++ and no ini.** It
> answers two questions and records the answers. That is the whole job.

Founder directive: bots must derive traversal — drop down, jump up, move platform to platform —
**from the navigation mesh itself**, with nothing placed in the level but the nav bounds volume
it already has. That approach is sound and it is how UE's own generator works. But it rests on
two engine facts that this repo cannot see: the cloud session has no engine, and
`dev.epicgames.com`, `docs.unrealengine.com` and `forums.unrealengine.com` are all blocked by its
egress proxy. Every API name below came from a **search summary**, which is exactly the source
that put three APIs into `BNDamageSpec` that had to be torn out again.

**So nothing gets written until these are transcribed from the headers on the machine that will
compile them.** Question 2 in particular can kill an entire approach, and it fails at PACKAGE
time rather than in PIE — which is the worst possible place to discover it.

## Kickoff (machine-checkable)

- requires: engine-installed
- `Tools/env.local` has a valid `ENGINE_ROOT` (`Tools/run-specs.sh` reads the same line)
- owner_path: `docs/tickets/TICKET_BN12_NAVLINK_PROBE.md`
  <!-- THIS FILE AND NOTHING ELSE. No Source/ edit, no Config/ edit, no Content/. If answering a
       question below seems to need one, that is a contract_gap: write it in the Log and STOP.
       A probe that starts implementing is no longer a probe, and its answers stop being
       trustworthy because they were shaped by what the implementer wanted to be true. -->

## QUESTION 1 — the built-in jump-down generator

UE 5.5+ generates navigation links from navmesh geometry during tile generation, configured by a
`Nav Link Jump Down Config` section on the navmesh. It is flagged **Experimental** through 5.8,
and there is an open community report that `Jump Max Depth` does not behave as documented — so
these are numbers to measure, never to trust.

Paste the **verbatim** answer to each. Struct definitions in full, not summarised.

1a. The config struct. Search the engine tree, do not assume a path:
```
grep -rn "NavLinkGenerationJumpDownConfig" "$ENGINE_ROOT/Engine/Source" --include=*.h
```
Paste the whole `USTRUCT` — **every** `UPROPERTY`, with its type, its name and its default.

1b. The navmesh property that turns it on, and the class that owns it:
```
grep -rn "JumpDownConfig\|bGenerateNavLinks\|GenerateNavLinks" "$ENGINE_ROOT/Engine/Source" --include=*.h
```

1c. **The ini section name, which is the crux of the C++-first path.** Find the `UCLASS(...)`
line for `ARecastNavMesh` and paste it. What is needed is whether it carries `config=`/
`defaultconfig` and which properties are `UPROPERTY(config)` — that is what decides whether
`Config/DefaultEngine.ini` can own these settings at all, or whether they are per-instance map
data. Report the section name in `[/Script/Module.Class]` form as it would be written.

1d. The proxy class to subclass:
```
grep -rn "GeneratedNavLinksProxy" "$ENGINE_ROOT/Engine/Source" --include=*.h
```
Is `UBaseGeneratedNavLinksProxy` a `UObject` or an `AActor`? Is it the class to derive from
directly, or is there a concrete subclass meant for that? What must a subclass override?

1e. Does it generate **upward** links, or downward only? Answer from the code and quote the lines
that settle it — the docs only ever describe jumping down, and the whole shape of the plan
depends on this being true.

## QUESTION 2 — can C++ read the navmesh border edges, in a SHIPPING target?

This is the question that can end an approach, so it gets answered before anything is built on it.

A navmesh edge with no neighbouring polygon is a ledge, by definition. Reading those edges is the
input to a link generator that derives everything from the mesh and places nothing in the level.
The route that appears to exist is `FRecastDebugGeometry` with `bGatherNavMeshEdges = true`, then
`ARecastNavMesh::GetDebugGeometry()` / `GetDebugGeometryForTile()`, reading `NavMeshEdges`.

Two things make that route doubtful, and both are load-bearing:

2a. **Is it exported?** There are reports of `LNK2019 unresolved external symbol` on
`GetDebugGeometry`. Find the declaration and paste it with its export macro and any
`#if WITH_EDITOR` / `#if !UE_BUILD_SHIPPING` / `ENABLE_DRAW_DEBUG` guard around it:
```
grep -rn "GetDebugGeometry" "$ENGINE_ROOT/Engine/Source" --include=*.h -B4 -A2
```
**State plainly whether a non-editor Game target can call this.** If it is editor-only or
debug-only, say so — that is a useful answer, not a failure, and it is the entire reason this
ticket exists.

2b. **Is there a better door?** If 2a says no, look for a supported way to walk polygons and
their neighbours — the candidates worth grepping are `GetPolyNeighbors`, `GetPolyEdges`,
`FNavigationPortalEdge`, `GetPolysInBox`, `BeginBatchQuery`. Paste what exists on
`ARecastNavMesh` with its export macro. Do not evaluate which is nicer; just report what is
callable.

2c. The registration side, needed either way. Confirm these exist and paste their declarations:
`ANavLinkProxy`, `UNavLinkCustomComponent`, `INavLinkCustomInterface`, and whichever
`UNavigationSystemV1` call registers a custom link. Also: **is there a delegate that fires when
navmesh generation finishes?** — that is when a generator would run.

## Three supporting facts, cheap while you are in there

- **Is this project's navmesh static or dynamic?** (`RuntimeGeneration` on the navmesh.) It
  decides whether rebuilding navigation writes into `BR_Arena01` and its external actor packages,
  or whether it is rebuilt at load and saved nowhere.
- **Does `BR_Arena01` actually have a navmesh?** The cloud clone stores every `.uasset` and
  `.umap` as a Git LFS pointer, so this could not be checked from there and **must not be assumed**
  — bots pathing today is strong evidence, not a read-back. `Tools/blockout/arena_plan.py` emits a
  `BR_NavBounds` actor, so the bounds volume should be there; confirm the `RecastNavMesh` is too.
- **Fall damage.** Does BN apply any damage on landing? The gantry lips sit at 8 m / 800 uu, and
  a drop rule that assumes survivability without checking is a bot that kills itself politely.

## Done when

- [ ] Q1a–1e answered in the Log, structs quoted verbatim, 1e's verdict backed by quoted lines
- [ ] Q1c states whether `DefaultEngine.ini` can own these settings, and under which section name
- [ ] Q2a states plainly whether a Game (non-editor) target can call `GetDebugGeometry`
- [ ] Q2b lists what else is callable on `ARecastNavMesh`, if 2a is no
- [ ] Q2c confirms the registration classes and names the generation-finished delegate
- [ ] The three supporting facts answered
- [ ] **No file outside this one has changed** (`git status` pasted in the Log proves it)

## What this ticket does NOT do

It does not turn the generator on, write `UBNGeneratedNavLinks`, write `UBNNavLinkForge`, touch
`Config/`, or rebuild navigation. Those are BN13's, and BN13 cannot be written honestly until
this Log exists. If an answer here makes an approach impossible, **say so in the Log** — killing
a plan on an engine fact is this ticket succeeding, not failing.

## Log

_(terminal: the greps, verbatim, and the verdicts)_
