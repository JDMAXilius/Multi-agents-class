#pragma once
// PHASE 3 — not yet implemented (gated on AIB1 rung-1 green). FULL DESIGN — the node
// inventory, each THIN (verbs + sensorium belief + engine state; no ASC, no game types,
// no decisions):
//
//   FAIBAmbitionGateCondition  — controller engine GetCurrent() == BranchTag.
//   FAIBFaceBeliefTask         — steps control rotation toward GetLastSeenLocation();
//                                explicit SetControlRotation stepping (the host proved
//                                focus-based aim never runs without tick). Aim ERROR is
//                                Phase 4's AimPolicy; Phase 3 faces the belief exactly.
//   FAIBMoveNearBeliefTask     — MoveToLocation(belief, AcceptanceRadius from spec);
//                                bUseAccelerationForPaths rides the pawn (host-proven).
//   FAIBFireWhenAbleTask       — presses Verb_Fire in bursts while the FACTS say the
//                                weapon can fight (one info door, never the avatar
//                                directly); releases on exit ALWAYS — a stuck held verb
//                                on the persistent ASC is the leak the host's sprint fix
//                                names.
//   FAIBFleeFromBeliefTask     — MoveToLocation(self + normalize(self - belief) * FleeUU).
//   FAIBMoveToLastKnownTask    — MoveToLocation(memory GetFresh position); fails when
//                                stale -> Root re-selects (the Search ambition's score
//                                decays with freshness anyway — brain and body agree
//                                by data, not by coupling).
//   FAIBSweepLookTask          — slow control-rotation sweep (the searching look).
//   FAIBMoveToPOITask          — IAIBWorldQuery::QueryPointsOfInterest(kind), pick by
//                                Worth over distance, MoveToLocation; no provider ->
//                                fail -> Root (Roam with no world query = standing bot,
//                                loudly — F7).
//
// TRANSCRIPTION SOURCES, named now: node struct shape (USTRUCT + FStateTreeTaskCommonBase
// / FStateTreeConditionCommonBase + InstanceDataType) from the host's compiled tasks
// file; EnterState/Tick/ExitState signatures verbatim from there; MoveTo* from
// AAIController (host-proven); control-rotation stepping from the host's steer-helper
// pattern. NOTHING from memory, NOTHING from doc summaries.
