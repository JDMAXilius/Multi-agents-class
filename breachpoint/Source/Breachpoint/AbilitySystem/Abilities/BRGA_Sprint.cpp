// Breachpoint. Sprint: the WhileHeld prover and the first predicted movement state.

#include "AbilitySystem/Abilities/BRGA_Sprint.h"

#include "GameFramework/Character.h"

#include "Character/BRCharacterMovementComponent.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"

UBRGA_Sprint::UBRGA_Sprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The policy this ability exists to prove. The base wires the WaitInputRelease task; nothing
	// about ending sprint is written in this file.
	ActivationPolicy = EBRAbilityActivationPolicy::WhileInputHeld;

	// The ability's ASSET tag — what other abilities list in CancelAbilitiesWithTag to end a
	// sprint. SetAssetTags is the 5.5+ spelling; the AbilityTags property it replaces is deprecated
	// and is documented as becoming private.
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BRGameplayTags::Ability_Sprint);
	SetAssetTags(AssetTags);

	// THE STATE. Granted for as long as the ability is active and removed automatically when it
	// ends — which is why nothing in this file adds or removes a tag by hand (law 5). GAS
	// replicates these to the owner (GameplayAbilitiesDeveloperSettings::ReplicateActivationOwnedTags
	// defaults true); simulated proxies read a sprinting player from their velocity, not this tag.
	ActivationOwnedTags.AddTag(BRGameplayTags::State_Movement_Sprinting);

	// No cost effect, no cooldown tag. Sprint is free (GDD). Stated by omission AND by this
	// comment, because "no cooldown" and "someone forgot the cooldown" look identical in code.
}

UBRCharacterMovementComponent* UBRGA_Sprint::GetBRCharacterMovement(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	return Character ? Cast<UBRCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
}

void UBRGA_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// ---------------------------------------------------------------------
	// THE STAGE GATE — Stage 4 `Sprint` (docs/GAS-INTEGRATION-ROADMAP.md).
	//
	// FIRST statement, before Super, so nothing commits and — the part that matters for this
	// ability — `SetSprintIntent(true)` below is never reached. That single call is the ONLY
	// producer of `bWantsToSprint` on an honest client (`BRCharacterMovementComponent.h` says so:
	// "THE ONLY caller is UBRGA_Sprint"), and `bWantsToSprint` is what `FSavedMove_BR::SetMoveFor`
	// copies, what `GetCompressedFlags` puts in FLAG_Custom_0, and therefore the only thing that
	// can make the server's `UpdateFromCompressedFlags` set the bit. Gate this and the CMC's whole
	// intent path is starved at its source; the multiplier in `GetMaxSpeed` is unreachable.
	//
	// WHERE THIS GATE IS *NOT* IDEAL, stated rather than hidden: it is in ActivateAbility and not
	// in CanActivateAbility, because this class declares no `CanActivateAbility` override and the
	// header is another lane's to write in this packet. The activation is therefore refused one
	// step later than it should be — after PreActivate has taken a prediction key, before any
	// cost, cooldown, tag grant or CMC write. Sprint is free (no cost, no cooldown), so the only
	// thing paid here is the key itself, which GAS releases on the EndAbility below. The owed
	// promotion is one line in BRGA_Sprint.h; it is named in this packet's report.
	// ---------------------------------------------------------------------
	if (!BRGas::IsStageEnabled(EBRGasStage::Sprint))
	{
		// Verbose, matching ABRCharacter's gate: a default-verbosity playtest log is unchanged, but
		// "the gate refused" is discoverable by turning the channel up rather than by reading this
		// file. A silently dead ability is indistinguishable from a broken one.
		UE_LOG(LogBRCombat, Verbose, TEXT("UBRGA_Sprint: activation refused — GAS stage gate is '%s'; sprint needs at least 'Sprint'. Set GasStage in Config/DefaultGame.ini."),
			BRGas::ToString(BRGas::GetStage()));

		// Cancelled, not completed: this activation did not happen. bWasCancelled=true is what tells
		// GAS to roll back anything the prediction key covered on the predicting client.
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// The base commits (free here) and wires the release watcher. Skipping it would leave a sprint
	// that never ends — see the subclass contract in BRGameplayAbility.h.
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!IsActive())
	{
		// The base already ended us (commit failed, or the key was released before activation
		// finished and bTestAlreadyReleased fired). EndAbility has run and cleared the intent.
		return;
	}

	UBRCharacterMovementComponent* Movement = GetBRCharacterMovement(ActorInfo);
	if (!Movement)
	{
		// REFUSED, not ignored. The ASC outlives the pawn, so an ability activating between death
		// and respawn genuinely has no avatar. Ending here keeps the invariant "sprint is active
		// iff something is sprinting" — an active sprint ability with nothing to speed up would
		// hold State.Movement.Sprinting and make every observer wrong.
		UE_LOG(LogBRCombat, Warning, TEXT("UBRGA_Sprint: no UBRCharacterMovementComponent on avatar '%s'; ending immediately."),
			*GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr));
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// The ONE line of motion coupling in this ability. Runs on the predicting client and on the
	// authority, unbranched — both are running the same ability, which is what makes the predicted
	// result and the authoritative result the same result.
	Movement->SetSprintIntent(true);
}

void UBRGA_Sprint::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UBRCharacterMovementComponent* Movement = GetBRCharacterMovement(ActorInfo))
	{
		Movement->SetSprintIntent(false);
	}

	// State.Movement.Sprinting is removed by GAS as this returns — ActivationOwnedTags are the
	// engine's business, and taking it off by hand here would be law 5's loose-tag mistake with
	// extra steps.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
