#include "AbilitySystem/Abilities/BNGA_Grenade.h"

#include "BreachpointNext.h"
#include "Core/BNGameplayTags.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "Weapons/BNProjectile.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

UBNGA_Grenade::UBNGA_Grenade()
{
	// LocalPredicted, and NOT ServerOnly — that was a real bug the critic caught. The ASC replicates
	// RepAnimMontageInfo COND_SkipOwner, and a ServerOnly ability runs no code on the owning client,
	// so a remote client throwing would have seen NO throw animation on its own screen while every
	// other machine saw one. The listen host is authority AND locally controlled, which is exactly
	// why it looked fine in one window.
	//
	// What is predicted is only the ANIMATION. The projectile is still the server's alone —
	// SpawnGrenade is authority-gated — so nothing here predicts a flight that would diverge on the
	// first bounce, which was the real reason for the original policy.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

const FGameplayTagContainer* UBNGA_Grenade::GetCooldownTags() const
{
	// Built on first use rather than in the constructor, per the tag-registration order rule.
	if (CooldownTags.IsEmpty())
	{
		CooldownTags.AddTag(BNTags::Cooldown_Grenade);
	}
	return &CooldownTags;
}

void UBNGA_Grenade::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (CooldownDuration <= 0.f)
	{
		return;
	}

	const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(UBNGE_GrenadeCooldown::StaticClass(), GetAbilityLevel());
	if (!Spec.IsValid())
	{
		return;
	}

	Spec.Data->SetSetByCallerMagnitude(BNSetByCaller::GrenadeCooldown, CooldownDuration);
	Spec.Data->DynamicGrantedTags.AddTag(BNTags::Cooldown_Grenade);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}

void UBNGA_Grenade::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	UWorld* World = GetWorld();
	if (!ASC || !World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The reference's two parallel branches: the montage starts NOW and the spawn waits out the
	// delay (MyCharacter.cpp:999-1022). It is not a montage notify because the template's throw
	// montage does not carry one — unlike melee, where AN_FPST_Melee exists and is used.
	float MontageLength = 0.f;
	if (UAnimMontage* Montage = ThrowMontage.IsNull() ? nullptr : ThrowMontage.LoadSynchronous())
	{
		MontageLength = ASC->PlayMontage(this, ActivationInfo, Montage, 1.f);
	}

	World->GetTimerManager().SetTimer(ThrowTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		SpawnGrenade();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}), FMath::Max(ThrowDelay, KINDA_SMALL_NUMBER), /*bLoop=*/false);
}

void UBNGA_Grenade::SpawnGrenade()
{
	const FGameplayAbilityActorInfo* ActorInfo = CurrentActorInfo;
	APawn* Avatar = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	UWorld* World = GetWorld();
	if (!Avatar || !World)
	{
		return;
	}

	// THE line that keeps the projectile the server's while the animation is predicted. Every
	// machine reaches here; only one throws. Not a warning — a client arriving here is the normal,
	// expected half of a LocalPredicted ability, not a fault.
	if (!ActorInfo->IsNetAuthority())
	{
		return;
	}

	// Defaults to ABNProjectile — BN's own C++ grenade. It is NOT the template's BP_FPST_Grenade,
	// and that is purity law 3: a Blueprint grenade explodes through ApplyRadialDamage, the banned
	// engine damage API, which would be a second damage pipeline bypassing attributes, shields and
	// death. ABNProjectile does the contract's own prescription instead — overlap query, one GE per
	// target through BNDamage. It still WEARS the template's assets (SM_grenade, NS_Grenade_Trail).
	UClass* Class = GrenadeClass.IsNull() ? ABNProjectile::StaticClass() : GrenadeClass.LoadSynchronous();
	if (!Class)
	{
		UE_LOG(LogBN, Warning, TEXT("BNGA_Grenade: GrenadeClass is set but failed to load — nothing thrown."));
		return;
	}

	// The throw goes where the player LOOKS, not where the body faces: GetBaseAimRotation carries
	// the controller's view on the authority, which is the machine running this.
	const FVector Origin = Avatar->GetActorLocation()
		+ Avatar->GetActorForwardVector() * SpawnForwardOffset
		+ Avatar->GetActorUpVector() * SpawnUpOffset;

	FActorSpawnParameters Params;
	// AlwaysSpawn, as the reference does: a grenade leaving the hand while the thrower's own
	// capsule overlaps the spawn point must not be dropped for that overlap.
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = Avatar;
	Params.Instigator = Avatar;

	const FRotator ThrowRotation = Avatar->GetBaseAimRotation();
	AActor* Grenade = World->SpawnActor<AActor>(Class, FTransform(ThrowRotation, Origin), Params);

	// Launched after the spawn rather than through the transform: a projectile movement component
	// reads its velocity, not its rotation, and a grenade spawned facing the right way with zero
	// speed simply drops at the thrower's feet.
	if (ABNProjectile* Projectile = Cast<ABNProjectile>(Grenade))
	{
		Projectile->Launch(ThrowRotation.Vector());
	}

	UE_LOG(LogBN, Log, TEXT("BNGA_Grenade: %s threw %s."), *GetNameSafe(Avatar), *GetNameSafe(Grenade));
}

void UBNGA_Grenade::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThrowTimer);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
