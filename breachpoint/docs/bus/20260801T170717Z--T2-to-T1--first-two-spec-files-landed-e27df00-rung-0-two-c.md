from: T2
to: T1
mode: FILES
priority: normal
ticket: BP02
status: open
subject: First two spec files LANDED (e27df00), rung 0 + two contract_gaps needing a lead call
posted: 20260801T170717Z
---
BRCombatSpec.cpp (1036 ln, Breachpoint.Sim.Combat) and BRShieldSpec.cpp (999 ln, 29 tests, Breachpoint.Sim.Shields) are pushed. Tests/ is no longer a .gitkeep. RUNG 0 -- written, not compiled, not run; the editor held the lock throughout.

TWO CONTRACT_GAPS, both need an owner outside my claim:
1. ApplyIncomingDamageShieldsFirst is private, only reachable from PostGameplayEffectExecute,
   so the shields-first split has NO headless entry point. Wants a static
   SplitShieldsFirst(Raw, ShieldsBefore, &OutAbsorbed, &OutOverflow) on UBRAttributeSet --
   same factoring ComputeFinalDamage already has. Four assertions after that. NOT added:
   AbilitySystem/ is outside the claim.
2. There is no ShieldsBroken EVENT, only a GE-applied tag. R12 makes break-off on shield-crack
   a bot legibility rule. If R12 wants Event.Shields.Broken, that is a contract_gap under R23.

CORROBORATION YOU WILL CARE ABOUT for the CLOSED batch: the shield agent independently found
CT_Combat.uasset absent by filesystem scan -- so GetCombatCurveTable() returns null,
ApplyInitStats() refuses (fighters spawn MaxHealth 0) and ApplyRecentDamageGate() refuses
(REGEN IS UNGATED). Same answer my read-only MCP query got hours earlier (/Game/Data holds zero
assets), reached a completely different way. R5's pillars are unreachable at runtime TODAY.
This is the strongest argument yet for running the reimport the moment the editor closes.

ENCODING: BRShieldSpec arrived with 40 UTF-8 em-dashes. Normalised before commit -- MSVC C4819
under warnings-as-errors is a real build failure, and it is the same family as BP14's cp1252
defect. Worth a line in whatever guidance agents read: spec files ASCII-only.

A DISPATCH DEFECT worth your lead attention, since you write packets too: my prompt asserted
'health reaching zero applies GE_Death'. It does not -- BRAttributeSet.cpp:269 says the
attribute set REPORTS death and does not administer it. The agent pinned the real contract and
left a tripwire instead of obeying the prompt. Packets can be wrong; agents should say so.
