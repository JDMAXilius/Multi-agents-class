# TICKET — BP07: BR_Arena01 — the three-level playground

> STATUS: open — cut by lead session, 29 Jul 2026. Runs parallel with BP06 (different
> owners). Curator manifest first; blockout lands against it.

Founder directive: one symmetric arena, three elevations, upper level grapple-primary. The
manifest is the source of truth; the .umap is its projection. One owner per binary file.

**Ordering law:** 1 → 2; 3 parallel with 2; 4 closes.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- `Content/Data/arena_manifest.json` exists (landed by BP13) and re-validates:
  >= 8 spawns, min pairwise spacing >= 8 m, max sightline <= 35 m, named rocket landmark
- Ticket BP01 is DONE (a project to open) and `git lfs` is installed — this ticket owns
  a binary `.umap` and must lock it
- owner_path: `Content/Maps/`, `Tools/py/`

## Steps (in order)

1. **arena-architect** returns `arena_manifest.json` v2 against the GDD constraints: bounds,
   3 levels, 2 team spawn zones + 4 scored neutrals (LOS-break ≤ 5 m), rocket node visible
   from all levels, ≥ 2 grapple points per upper position, sightline cap 35 m, named
   landmarks. Critic refutes the manifest (spawn-camp geometry, dead zones, single-point
   grapple chokes) BEFORE any editor work. Owner: **arena-architect**, **critic**.
2. Blockout `BR_Arena01.umap` from the manifest (UE MCP where available): geometry, spawn
   actors, `ABRPowerWeaponSpawner` at the rocket node, grapple-point markup, nav mesh,
   player-clamped test lighting. Owner: **builder** (owns the .umap lock).
3. **Character presentation**: sourced anim pack integrated — 1P arms ABP + 3P body ABP on
   shared C++ base; ability montage timings authored TO the pack (`CT_Combat` values
   updated with rationale). Owner: **anim-builder**.
4. Verify: nav/reachability sweep (every spawn → every level), rung 3 functional walk,
   screenshot set attached to Log (binary review is by behavior + screenshots).
   Owner: **verifier**.

## Done when

- [ ] Manifest v2 critic-refuted and landed in-repo (JSON, diffable)
- [ ] Blockout matches manifest (spot-audit table in Log); upper level grapple-only
- [ ] Both ABPs live on the character; montage timings match pack (no retimed clips)
- [ ] Reachability sweep green; screenshots in Log
- Crew: arena-architect · builder (.umap owner) · anim-builder · verifier · critic
- Contracts: `data-and-assets.md` (the manifest is a data artifact; one owner per binary — and read its LFS-lock fill-in, the mechanism decision lands here) · `animation.md` (step 3 ABPs; `Character/` owner-path note applies) · `testing.md` (rung 3 walk + screenshots)
- Out of scope: art dressing (W4–5 pass), second arena (Phase 2)

## Log

(append findings here, dated, newest last)
