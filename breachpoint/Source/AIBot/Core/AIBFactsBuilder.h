#pragma once
// PHASE 2 — not yet implemented. Contract:
// The world-touching assembler, in Core/ BECAUSE it touches the world — moving it out of
// Brain/ is what keeps law 4 ("Brain/ and Skills/ name no UWorld/AActor") literally true
// (W-REVIEW 2b: the old placement instructed Phase 2 to break the law it cited).
// Assembles FAIBFacts each think from the avatar door (self block), the sensorium
// (target-as-perceived; when IsSightCurrent() is false it uses GetLastSeenLocation and
// sets bTargetFactsFromMemory — never a live actor read), and IAIBWorldQuery +
// IAIBAmbitionProvider (mode block, with Urgency CLAMPED 0..1 here, the one site).
// A fact a human could not have had does not get built (FAIRPLAY F3).
