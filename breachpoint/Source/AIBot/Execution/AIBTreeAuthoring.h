#pragma once
// PHASE 3 — not yet implemented, WITH_EDITOR only (and the .cpp must guard its whole TU:
// UBT feeds every module .cpp to the Server target — the P1 review's preventive note).
// FULL DESIGN:
//
// Builds, compiles, saves Content/AIBot/AI/ST_AIBBot from C++ (a StateTree graph has no
// scripting surface — the R5 finding; the host's authoring file is the compiled, proven
// source for every editor API: asset creation, AddChildState, AddEnterCondition<T>,
// AddTask<T>, transition wiring, the compile call, the save). Idempotent: a rebuild
// converges. The tree shape comes from AIBStateTreeExecutor.h's design block — ONE
// source. Also mints DT_AIBTiers from FAIBTierRow C++ defaults (one direction of flow).
//
// Driven over MCP by Tools/aib/70_aib_assets.py (aib-editor's, the Phase 3 ticket):
// probe gates build; the probe list is derived from THIS module's node structs and
// checked both directions (the 14-of-21 lesson).
