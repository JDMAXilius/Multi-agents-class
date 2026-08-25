#pragma once
// PHASE 1 — not yet implemented. Contract:
// The single point stimuli mature (FAIRPLAY F1). Plain C++ (worldless, spec-testable):
// Push(stimulus, nowSeconds, drawnLatency) clamps latency to >= AIB::MinReactionSeconds
// at THIS one site — never trust N call sites to remember a law. PopMatured(now) is the
// only exit. One latency draw per stimulus, at push, so a tier cannot re-roll.
