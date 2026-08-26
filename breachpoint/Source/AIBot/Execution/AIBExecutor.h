#pragma once
// PHASE 3 — not yet implemented (gated on AIB1 rung-1 green). FULL DESIGN, decided at the
// barrier so implementation is transcription, not invention:
//
// The seam that keeps StateTree and Behavior Tree interchangeable. Pure C++ interface:
//   Start(AAIBBotController&)  — begin running; loads whatever asset the impl needs.
//   Stop()                     — possession ended; leave nothing running.
//   (No SetActiveAmbition call: the executor READS the ambition from the controller each
//    relevant tick via GetAmbitionEngine()->GetCurrent() — pushing it in would create a
//    second copy of arbitration state, and the P2 respawn bug showed what copies cost.)
//
// UAIBStateTreeExecutor first; a BT executor may join later with zero brain changes.
