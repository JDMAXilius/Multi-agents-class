#pragma once
// PHASE 1 — not yet implemented. Contract:
// Last-known positions with freshness decay (FAIRPLAY F5). Worldless C++: Remember(actor,
// where, now), GetFresh(now) -> position while age < window (tier data), Forget on death.
// Infinite memory is banned at every tier; a bot that lost you searches, never tracks.
