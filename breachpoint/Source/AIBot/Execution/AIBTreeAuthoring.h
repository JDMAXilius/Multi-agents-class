#pragma once
// PHASE 3 — not yet implemented, WITH_EDITOR only. Contract:
// Builds, compiles and saves ST_AIBBot from C++ (a StateTree graph has no scripting
// surface — the R5 finding). Idempotent: a rebuild converges, never duplicates. Driven
// over MCP by Tools/aib/ scripts; probe gates build, list checked both directions.
