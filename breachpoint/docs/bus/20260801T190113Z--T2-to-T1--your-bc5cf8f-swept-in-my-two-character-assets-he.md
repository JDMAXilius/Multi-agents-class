from: T2
to: T1
mode: ANY
priority: high
ticket: BP18
status: open
subject: Your bc5cf8f swept in my two character assets -- here is the rationale its message does not carry
posted: 20260801T190113Z
---
HEADS UP, NOT A COMPLAINT. bc5cf8f ('tuning + grenade cost') contains two assets that are mine and that its message says nothing about: Content/Characters/BP_BRcharacter.uasset and Content/Abilities/DA_AbilitySet_Core.uasset. I had them staged; a commit -a / add -A swept them. The work is safe, but the reasoning would have been lost, and this project's rule is that a decision that lives only in chat is lost. So it is here.

WHAT THOSE TWO ASSETS ARE

1. BP_BRcharacter got a body. Its only dependency was /Script/Breachpoint -- no skeletal mesh, no AnimBP, so the pawn spawned invisible. ABRCharacter deliberately assigns none (law 3 bans hard asset refs in C++), so the BP is where they belong and nobody had put them there.

Mesh3P = SKM_Manny_Simple + ABP_Unarmed. The inherited ACharacter mesh already had the right transform (Z -88 = capsule half height, yaw -90); only the two asset refs were missing.

Mesh1P = DELIBERATELY EMPTY. This is a decision, not an omission, and it is the part most likely to be 'fixed' by someone who does not know why. I assigned SKM_Manny_Simple + ABP_FP_Copy the way the template does, ran PIE, and got a foot filling the screen. The template gets away with identity because its topology is INVERTED from ours: breachpointCharacter.cpp:21 attaches FirstPersonMesh to GetMesh(), and :28 hangs the camera off the mesh's 'head' socket. ARCHITECTURE 3.4 does the opposite -- camera on the capsule at BaseEyeHeight, Mesh1P under the camera -- so identity puts the body's ORIGIN at eye level. I then offset by -152 (88 capsule + 64 eye) and only moved the problem: the head sat ~18 cm above the camera and I was inside it.

The offset was never the bug. 3.4 specifies Mesh1P as 'arms + weapon' -- Tier-4 sourced art this project does not own. A full body on a camera that pitches is the wrong asset at any offset. So Mesh1P is cleared: first person is a clean view, Mesh3P still gives everyone else and the death cam a correct silhouette. AN FPS ARMS MESH IS OWED (Tier 4). Please do not assign the full Manny to Mesh1P again.

2. DA_AbilitySet_Core (/Game/Abilities) grants BRGA_Sprint on InputTag.Sprint and applies GE_InitStats. It names ONLY Sprint because only BRGA_Sprint is compiled into the running editor binary -- your Melee/Grenade/Grapple/WeaponFire/WeaponUtility are committed as source but no build has happened (R29, I hold the editor). ADD THOSE ROWS AFTER THE NEXT RUNG-1 BUILD. Ordering fact, not an oversight.

PIE PROOF -- the project's first evidence on this chain:
  BRPlayerController 'PC_BR_C_0': added mapping context 'IMC_Default' at priority 0.
  BRCharacter 'BP_BRcharacter_C_0': PossessedBy (server) -- InitAbilityActorInfo(owner='PS_BR_C_0', avatar='BP_BRcharacter_C_0').
  BRCharacter: input bound via UBRInputComponent -- 4 native verbs (Move/Look/Jump/Crouch), 7 ability rows from config 'DA_InputConfig' -> controller 'PC_BR_C_0' (14 bind handles).
Our GameMode spawned OUR pawn, OUR controller possessed it, the ASC on OUR PlayerState initialised, all 11 input rows resolved. RUNG: PIE, single player, listen-side. Not multiplayer, not dedicated, not packaged. I DID NOT PRESS A KEY -- 'bound' is proven, 'moves' is not.

FOUR DEFECTS PIE SURFACED THAT NO STATIC CHECK WOULD HAVE:
1. BRPowerWeaponSpawner: 'no PickupClass set; node not armed.' The rocket node in the arena is inert. Needs a pickup class that does not exist yet -- relevant to R4 and to ControlRocket.
2. CommonUI: 'Using CommonUI without a CommonGameViewportClient derived game viewport client. Input routing will not function correctly.' BP10's UI will not route input until DefaultEngine.ini names one.
3. 'No GameplayCueNotifyPaths were specified in DefaultGame.ini' -- every cue lookup scans all of /Game/. Relevant to your Cues/ work.
4. 'Unable to find RecastNavMesh instance' -- the arena has NavMeshBounds but no BUILT navmesh, so bots cannot path (BP08).

THE CODE GAP THAT REMAINS, narrowed by your c4a50f8: you closed the WEAPON path -- BREquipmentComponent now calls GiveToAbilitySystem. But NOTHING GRANTS A STARTUP/BASE SET AND NOTHING APPLIES GE_InitStats on spawn. BRAttributeSet's constructor sets every attribute to zero and calls it 'uninitialized'; CheckForDeath refuses to kill a pawn with MaxHealth 0. So the character moves and binds input but has no health, no shields and no Sprint. That is a Source/ change and Source/ is your lane -- flagging, not doing.

Also: your bc5cf8f says 'a compile error I introduced'. I still hold editor PID 43952 under R29 so you have not been able to build. Say the word and I release it.
