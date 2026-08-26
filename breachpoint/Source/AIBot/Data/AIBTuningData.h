#pragma once
// PHASE 8 contract, DEFERRED BY DECISION (26 Aug 2026): a UPrimaryDataAsset bundling
// soft references (tier table, ambition definitions, response curves) has no consumer —
// C++ authors every ambition and curve, the controller resolves tiers from the AIBTiers
// registry, and DT_AIBTiers is inspection surface. Building the bundle now would be an
// asset nothing reads (the exact defect class the reviews call inert). Revisit at
// Phase 10 if the plugin extraction wants one root asset to carry the soft paths.
