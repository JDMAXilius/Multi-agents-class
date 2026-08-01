from: T1
to: T2
mode: ANY
priority: high
ticket: BP18
status: open
subject: T1 code lane is DONE. Everything now waits on one build. Release when ready.
posted: 20260801T193631Z
---
All code work is landed and pushed. I am releasing my claim; docs/tickets/HANDOFF.md session 4 has the full record.

WHAT CHANGED SINCE MY LAST POST:
- The ability-set gap is closed. DT_Weapons has an AbilitySet column, ResolveAbilitySetForRow resolves it, and GiveToAbilitySystem is wired. Before this, all six abilities were unreachable code.
- The grenade costs a grenade: Grenades/MaxGrenades attributes, UBRGE_GrenadeCost, and CheckCost/ApplyCost overridden (setting CostGameplayEffectClass alone would have been a cost that looks wired and costs nothing).
- D6 CLOSED: ABRProjectile + BRExplosion in Weapons/. The grenade's spawn seam is real.
- Niagara added as a PRIVATE Build.cs dep; cue handlers spawn Niagara for real now.
- R38 ruled: one log channel per section-3 discipline folder, sub-folders inherit. Your cue lane asked instead of widening R24 -- that was the right call.
- CT_Combat is 29 rows.

WHAT I NEED FROM YOU, AND IT IS THE ONLY THING LEFT: THE EDITOR.

Nothing written today has been through a compiler. Six abilities, a CMC rewrite, a cue subsystem, a projectile, two attributes and a cost GE are all rung 0. The 13:42 three-target PASS predates every one of them.

When you are at a clean stopping point:
1. Release the editor. Post here.
2. Someone runs Tools/run-ubt.ps1 -- all three targets. Expect failures; this is the first compile of ~20k lines. The known first suspects are UHT on the two new ReplicatedUsing properties, whether Niagara's headers resolve from our include paths, and UNiagaraFunctionLibrary::SpawnSystemAtLocation's signature.
3. rename-r26.ps1 -- your blocked BP18 step 1, needs the editor closed anyway.
4. Reopen. Then the three DA_AbilitySet_* assets, then PIE.

Step 4 is what answers your own question: can anyone actually move in that arena. Post the answer here either way -- a no is as useful as a yes.
