#include "AbilitySystem/Abilities/BNGA_Death.h"

#include "Core/BNGameplayTags.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "Match/BNPlayerState.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

UBNGA_Death::UBNGA_Death()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBNGA_Death::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || !ActorInfo->IsNetAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Every other verb stops here, and each one's own EndAbility takes its state GE off the
	// PERSISTENT ASC as it goes — jump's Jumping/InAir, sprint's speed, lean's side, fire's loop.
	// This is also structural cover for debt A1, which is still fixed at its source below,
	// because a pawn can be destroyed without ever dying.
	ASC->CancelAbilities(nullptr, nullptr, this);

	DeadHandle = ApplyStateTag(BNTags::State_Dead);

	// THE corpse, and the only route to one. This ability is ServerOnly, so nothing in it runs on a
	// client — before this line, UBNHealthComponent::OnDeath fired on every machine and its one
	// listener discarded it everywhere but here. An executed cue multicasts, so each machine
	// ragdolls the copy it is rendering. No prediction key is involved: nothing predicts a death.
	{
		FGameplayCueParameters DeathParams;
		DeathParams.Instigator = ActorInfo->AvatarActor.Get();
		K2_ExecuteGameplayCueWithParams(BNTags::GameplayCue_Character_Death, DeathParams);
	}

	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr)
	{
		// MovementMode replicates, so the owning client's own moves stop producing motion too —
		// the corpse does not walk on the machine that used to steer it.
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	AController* Controller = Character ? Character->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}

	// Input: ability keys are refused by State.Dead in UBNGameplayAbility::CanActivateAbility,
	// which runs on the client as well as the server, so nothing is even mispredicted. These two
	// cover the authority's own copy and are cleared by ClientRestart on the respawn.
	Controller->SetIgnoreMoveInput(true);
	Controller->SetIgnoreLookInput(true);

	// ANNOUNCE, do not call. Law 7: "cross-system consequences travel as gameplay events
	// (Event.Death -> GameMode) and delegates — an ability never reaches into GameMode". This used
	// to be GameMode->RequestRespawn(Controller), which made a death ability depend on a game mode
	// class, welded corpse lifetime to respawn delay, and left no room for a killfeed or scoring to
	// hear about a death without another call bolted in here.
	if (ABNPlayerState* PS = Controller->GetPlayerState<ABNPlayerState>())
	{
		// The killer, read from the attribute set's capture — the instigator the damage door put in
		// the spec's context, recorded at the one reaction point instead of being thrown away after
		// the log line. A weak pointer that no longer resolves (killer disconnected while their
		// grenade flew) degrades to null, which the subscriber words as "died", never a crash.
		ABNPlayerState* KillerPS = nullptr;
		if (const UBNAttributeSet* Attributes = ASC->GetSet<UBNAttributeSet>())
		{
			const FBNLastDamage& LastDamage = Attributes->GetLastDamage();

			// The PLAYER STATE first: it outlives every pawn, so a grenade that kills after its
			// thrower has died and respawned still credits the thrower. The pawn is the fallback.
			KillerPS = Cast<ABNPlayerState>(LastDamage.InstigatorPlayerState.Get());
			if (!KillerPS)
			{
				if (const APawn* KillerPawn = Cast<APawn>(LastDamage.Instigator.Get()))
				{
					KillerPS = KillerPawn->GetPlayerState<ABNPlayerState>();
				}
			}
		}
		PS->BroadcastDeath(KillerPS);
	}

	// Deliberately still ACTIVE: this ability is what holds the dead tag.
}

void UBNGA_Death::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	RemoveStateTag(DeadHandle);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
