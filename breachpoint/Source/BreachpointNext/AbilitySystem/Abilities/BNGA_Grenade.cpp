#include "AbilitySystem/Abilities/BNGA_Grenade.h"

#include "BreachpointNext.h"
#include "Weapons/BNProjectile.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

UBNGA_Grenade::UBNGA_Grenade()
{
	// See the header: a predicted projectile is two simulations that diverge on the first bounce,
	// with nothing to reconcile them. The throw ANIMATION still reaches every machine, because the
	// ASC replicates the montage it plays.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
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

	// ServerOnly already guarantees this, but the ability could be granted wrongly one day and a
	// client-spawned projectile is invisible to everyone else — a silent failure worth a loud line.
	if (!ActorInfo->IsNetAuthority())
	{
		UE_LOG(LogBN, Warning, TEXT("BNGA_Grenade: reached SpawnGrenade without authority — nothing thrown."));
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
