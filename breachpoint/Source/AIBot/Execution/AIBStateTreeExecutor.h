#pragma once
// PHASE 3 — not yet implemented (gated on AIB1 rung-1 green). FULL DESIGN:
//
// IAIBExecutor over UStateTreeAIComponent — the host controller PROVED the shape:
// component subobject on the controller, SetStartLogicAutomatically(false), soft ini path
// to the tree, resolve at Start(), StartLogic(); a null resolve is ONE loud Error and a
// standing bot.
//
// ONE TREE, ONE BRANCH PER AMBITION, gated by FAIBAmbitionGateCondition (enter condition
// comparing the controller engine's GetCurrent() to a branch tag — the executor MIRRORS
// arbitration, never re-does it):
//   Root
//   |- Engage  [gate: AIBot.Ambition.Engage]     > FaceBelief . MoveNearBelief . FireWhenAble
//   |- Retreat [gate: AIBot.Ambition.Retreat]    > FleeFromBelief (away-vector MoveTo)
//   |- Search  [gate: AIBot.Ambition.Search]     > MoveToLastKnown . SweepLook
//   |- Seek    [gate: AIBot.Ambition.SeekWeapon] > MoveToPOI(kind=Weapon, IAIBWorldQuery)
//   |- Roam    [gate: AIBot.Ambition.Roam]       > MoveToPOI(kind=Roam) . SweepLook
// Every branch: completion/failure transitions back to Root (the host tree's proven
// pattern), so a changed ambition re-selects within one tree update.
//
// Mode branches (Phase 6) are ADDED by authoring from IAIBAmbitionProvider's ambitions —
// same gate condition, mode tag.
