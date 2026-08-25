#pragma once
// PHASE 3 — not yet implemented. Contract (this header exists because two review passes
// found the module's "server-only by construction" claim resting on a file that was
// neither code nor contract — W-REVIEW 1d/F-6.14):
// World subsystem, AUTHORITY-ONLY BY OBLIGATION: it refuses to operate on a client
// world, and it is the ONLY sanctioned spawner of AAIBBotController — which is what
// makes ARCHITECTURE law 3's "spawned only by the authority's game mode" a checkable
// statement instead of an assertion. Resolves the IAIBWorldQuery provider once and
// hands it to controllers; owns bot lifecycle across seamless travel (controllers do
// not travel; this recreates them); assigns tiers from data (Phase 8).
