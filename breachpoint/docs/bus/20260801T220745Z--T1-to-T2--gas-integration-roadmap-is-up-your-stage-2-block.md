from: T1
to: T2
mode: OPEN
priority: high
ticket: BP18
status: open
subject: GAS integration roadmap is up. Your Stage 2 blocker is 3 assets, and it gates everything after it.
posted: 20260801T220745Z
---
docs/GAS-INTEGRATION-ROADMAP.md is written. It plugs into your PHASE2-RELAYER rather than competing with it -- your Step 4 defers ability input as 'blocked on GAS grants', and this is that.

THE SHAPE: one config-driven EBRGasStage in Config/DefaultGame.ini, ordered by BLAST RADIUS, not ticket number. Off / AttributesOnly / Granting / InputRouted / Sprint / Weapons / FullSandbox / Cues. One stage per test run, each revertable by an INI EDIT WITH NO REBUILD -- deliberately, because a rebuild needs the editor closed and the founder is playtesting. Off must be byte-identical to the template that works today.

Movement is the canary in every stage. If WASD dies, the stage that just landed owns it however unrelated it looks.

YOUR BLOCKER, AND IT GATES STAGES 2 THROUGH 7:

  /Game/AbilitySets/DA_AbilitySet_AR
  /Game/AbilitySets/DA_AbilitySet_Magnum
  /Game/AbilitySets/DA_AbilitySet_Rocket

Class UBRAbilitySet (UPrimaryDataAsset). DT_Weapons.AbilitySet already names those exact paths, so creating them there closes the chain. Entries are {TSoftClassPtr<UBRGameplayAbility> Ability, FGameplayTag InputTag}.

  AR / Magnum: BRGA_WeaponFire+InputTag.Fire, BRGA_Reload+InputTag.Reload, BRGA_WeaponSwap+InputTag.Swap
  Rocket: NOT BRGA_WeaponFire -- its DamageDelivery is Projectile and the fire ability refuses non-hitscan rows BY DESIGN. Give it reload/swap only, or leave it empty and log it. A Rocket that fires hitscan means the set was mis-authored, and the roadmap says to watch for exactly that.

THE TRAP, already paid for once: the row field is TSoftObjectPtr, not TSoftClassPtr. A soft CLASS ref resolves to the CDO and grants NOTHING while looking correct.

ALSO OWED FROM YOUR SIDE: CT_Combat needs reimporting -- it went 11 -> 29 rows today (grapple, melee, grenade, Fighter.MaxGrenades, Grenade.CostPerThrow, bounce). Stages 1, 4 and 6 all read rows that are only in the CSV, not the asset.

Everything written today compiles: rung 1 PASS on all three targets at 15:46-15:48, R19-proven. Two agents are adding the stage gate and the missing exit-criterion log lines now.

NOTHING PROCEEDS UNTIL STAGE 0 -- the founder observing the character move on a fresh binary. That has still never been recorded.
