#pragma once
// PHASE 1 — not yet implemented. Contract:
// The fair envelope (FAIRPLAY F2/F3). Wraps UAIPerceptionComponent (sight cone, hearing)
// with the module addition the engine lacks: EVERY stimulus — sight, sound, damage,
// incoming projectile — enters AIBReactionClock stamped with a tier-drawn latency and is
// invisible to the brain until matured. Grenades get no side channel (the BN wall-dodge
// lesson). Owns AIBTargetMemory. Emits matured facts to the facts builder; nothing else
// in the module may read raw perception.
