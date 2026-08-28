# TICKET — AIB15: Phase 10 proof — the plugin compiles, here and in a blank project

> STATUS: open — cut 26 Aug 2026 by the cloud lead with the extraction
> (~~"WRITTEN, NOT COMPILED"~~ — **corrected 28 Aug 2026: the extracted plugin COMPILES.**
> Editor and Game targets build clean from `Plugins/AIBot/`; module suite 119/119/0,
> superseding the 118 below). TERMINAL WORK REMAINING: **the blank-project proof** — the
> roadmap's actual Phase-10 claim — plus the PIE sanity pass. The extraction followed the
> module's own recorded delta VERBATIM (ARCHITECTURE.md carries the EXECUTED note and the
> two deliberate deviations: Config=Game stands; content stays project-side).

## What landed (cloud)

- `Source/AIBot/` → `Plugins/AIBot/Source/AIBot/` (git mv — history follows renames).
- NEW `Plugins/AIBot/AIBot.uplugin` — Runtime module, `CanContainContent: false`,
  `EnabledByDefault: true`, and `Plugins[]` deps on StateTree + GameplayStateTree
  (delta item 3 — previously satisfied by the host's own plugin list).
- `Breachpoint.uproject` — `AIBot` removed from `Modules[]`, added to `Plugins[]`
  (delta item 1). All three `Source/*.Target.cs` dropped `"AIBot"` (item 2).
- Content did NOT move: `/Game/AIBot/…` literals in the authoring, the ini soft path,
  and `Tools/aib/70_aib_assets.py` all stay true (item 5's alternative, chosen so no
  bot can stand still on a null soft path while a second tree rebuilds unloaded).
- Path-mention updates only: roadmap law 1 + seam-ledger item 6, tool docstrings.
- ZERO source-code changes inside the module — the extraction is the boundary laws'
  own proof: engine-only deps, `PublicIncludePaths.Add(ModuleDirectory)` move-invariant.

## Steps (terminal)

1. Regenerate project files (the move invalidates the old solution), then rung 1:
   ALL THREE targets (`./Tools/run-ubt.sh` game + editor + server) — the editor-only
   authoring block and the DeveloperSettings link are the two likely break points.
2. Specs: **118/118** (the terminal measured 117 = 114 + AIB16's three suppression pins; the cloud's fuse-noise pin adds one more once its commit compiles — a 117 here means the fuse pin did not run, reconcile).
3. PIE sanity (5 min): bots possess, resolve tiers, fight — and the tree soft path
   still loads (`/Game/AIBot/AI/ST_AIBBot` — content stayed, so it must). If AIB11's
   rebuild has not run yet, do that FIRST (the rename!) — this ticket does not add a
   rebuild of its own.
4. THE BLANK-PROJECT PROOF (the roadmap's own line): fresh minimal UE 5.8 C++ project,
   copy `Plugins/AIBot/` in, enable StateTree + GameplayStateTree if not already, and
   compile. Expect: compiles clean (engine deps only — any BN symbol reaching the
   linker here is a boundary breach the greps missed and a HIGH finding). Bots are NOT
   expected to play there (no adapter, no providers): compiling is the claim, playing
   in a bare host is Phase-Next for a second consumer.
5. Watch-list: `EnabledByDefault` + explicit uproject entry coexisting; the uplugin
   `Plugins[]` dependency block's exact schema against 5.8 (transcribed from the
   host's uproject shape, not from a compiled uplugin — we had none in-repo).

## Done when

- [x] Editor + Game compile after regen (28 Aug, clean). **THIRD TARGET UNSATISFIABLE
      HERE** — `BreachpointServer` has no server binaries on this launcher install; closed
      as environmental and NOT ticked as a pass, per AIB1's precedent
- [x] Module suite green — **count re-pinned 118 → 119/119/0** (28 Aug)
- [ ] PIE sanity: tier lines + a fight + the tree loads from /Game/AIBot
- [ ] Blank-project compile pasted (the roadmap's Phase-10 proof, verbatim)
- [ ] Watch-list confirmed or errors pasted

## Log

_(terminal: outputs verbatim)_

### 2026-08-28 — board-hygiene pass: the extraction holds; the portability claim is unproven

Corrected, not measured.

The header called this uncompiled text. It is not: the module now lives at
`Plugins/AIBot/Source/AIBot/` (BN15's own wave table names that path as the one
pre-sanctioned module edit site, and it built there), Editor and Game targets are clean,
and the suite reads 119/119/0. Step 1's two predicted break points — the editor-only
authoring block and the DeveloperSettings link — survived. Step 5's watch-list is answered
the same way: `EnabledByDefault` coexisting with the explicit uproject entry, and the
uplugin's `Plugins[]` schema against 5.8, are both confirmed by a build that loads the
plugin.

**What is NOT proven, and is the whole point of Phase 10:** step 4, the blank-project
compile. Compiling *here* proves the boundary laws held in a tree that still contains
BreachpointNext; it does not prove engine-only deps. A BN symbol reaching the linker is
exactly the thing that only shows up in a bare host, and this ticket stays open for it.
Step 3's PIE sanity (the `/Game/AIBot/AI/ST_AIBBot` soft path still loading after the
move — content deliberately did not move) is also unrun, and it is the one that would
catch a bot standing still on a null path.
