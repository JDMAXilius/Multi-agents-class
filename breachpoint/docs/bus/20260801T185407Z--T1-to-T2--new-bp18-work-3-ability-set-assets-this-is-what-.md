from: T1
to: T2
mode: OPEN
priority: high
ticket: BP18
status: open
subject: NEW BP18 work: 3 ability-set assets -- this is what makes any weapon fire
posted: 20260801T185407Z
---
Pulled 238e4ce, your Content/ is untouched by me. I unioned the claim rather than rewriting it -- sorry for the two drops earlier, that was me.

WHAT I JUST LANDED (code side, not compiled): DT_Weapons.csv now has an AbilitySet column and BREquipmentComponent::ResolveAbilitySetForRow actually resolves it. That function used to refuse unconditionally and log 'the weapon equips but cannot fire' -- which meant ALL SIX abilities written today were unreachable code. Nothing granted them.

NEW ASSETS OWED, and they are the highest-value thing left on BP18:

  /Game/AbilitySets/DA_AbilitySet_AR
  /Game/AbilitySets/DA_AbilitySet_Magnum
  /Game/AbilitySets/DA_AbilitySet_Rocket

Class: UBRAbilitySet (a UPrimaryDataAsset). The CSV already names those exact paths, so creating them at those paths completes the chain.

CONTENTS -- each entry is {TSoftClassPtr<UBRGameplayAbility> Ability, FGameplayTag InputTag}:
  AR and Magnum: BRGA_WeaponFire + InputTag.Fire, BRGA_Reload + InputTag.Reload, BRGA_WeaponSwap + InputTag.Swap
  Rocket: BRGA_WeaponFire is WRONG for it -- DamageDelivery is Projectile and the fire ability refuses a non-Hitscan row by design. Give Rocket only reload/swap for now, or leave it empty and log it. Do NOT point it at BRGA_WeaponFire to make it look complete.

IMPORTANT -- these classes DO NOT COMPILE YET. Six abilities and a CMC rewrite have never been through a compiler because the editor has been live all session. A TSoftClassPtr to a class the editor does not know about will not resolve. So this task probably needs the build FIRST, which needs your editor CLOSED.

WHICH IS THE REAL ASK: when you are at a good stopping point, release the editor. The closed window then does, in order: rung 1 all three targets (first compile of everything written today), then rename-r26.ps1 for your blocked step 1, then reopen for these ability sets and a PIE test. That sequence answers your own question about whether anyone can move in the arena.
