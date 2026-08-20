#include "Animation/BNLAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

void UBNLAnimInstance::ResolveOwner()
{
	if (!OwningCharacter)
	{
		OwningCharacter = Cast<ABNCharacter>(TryGetPawnOwner());
	}
	if (OwningCharacter)
	{
		OwningMovementComponent = OwningCharacter->GetCharacterMovement();
		MeshComponent = OwningCharacter->GetMesh();
	}
}

void UBNLAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	ResolveOwner();
	BindAbilitySystem();
}

void UBNLAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!bAbilitySystemBound)
	{
		ResolveOwner();
		BindAbilitySystem();
	}

	if (!OwningMovementComponent)
	{
		return;
	}

	CachedVelocity = OwningMovementComponent->Velocity;
	CachedAcceleration = OwningMovementComponent->GetCurrentAcceleration();
	CachedActorRotation = OwningCharacter ? OwningCharacter->GetActorRotation() : FRotator::ZeroRotator;
	bCachedFalling = OwningMovementComponent->IsFalling();
	bCachedOnGround = OwningMovementComponent->IsMovingOnGround();
	bCachedCrouching = GameplayTag_IsCrouching || (OwningCharacter && OwningCharacter->bIsCrouched);
	CachedGroundDistance = bCachedOnGround ? OwningMovementComponent->CurrentFloor.FloorDist : -1.f;

	// The ADS lens. Here and not in the thread-safe pass because it touches the camera component,
	// which is game-thread only — NativeUpdateAnimation is the game thread, and RESEARCH-ADS §3
	// names it as the lawful home for this interp precisely so no gameplay Tick has to exist.
	// The gate is the same one UBNAnimInstance uses: owner-only, and a player, never a bot.
	if (OwningCharacter)
	{
		const bool bOwnerFirstPerson = OwningCharacter->IsLocallyControlled() && OwningCharacter->IsPlayerControlled();
		ADSCameraBlend.Update(OwningCharacter->GetFirstPersonCamera(), GameplayTag_IsADS, bOwnerFirstPerson, DeltaSeconds);
	}
}

void UBNLAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	Velocity = CachedVelocity;
	const FVector Velocity2D(Velocity.X, Velocity.Y, 0.0);
	LocalVelocity2D = CachedActorRotation.UnrotateVector(Velocity2D);
	DisplacementSpeed = Velocity2D.Size();
	HasVelocity = !Velocity2D.IsNearlyZero();
	HasAcceleration = !CachedAcceleration.IsNearlyZero();
	IsFalling = bCachedFalling;
	IsOnGround = bCachedOnGround;
	isCrouching = bCachedCrouching;
	GroundDistance = CachedGroundDistance;
}

void UBNLAnimInstance::NativeUninitializeAnimation()
{
	UnbindAbilitySystem();
	Super::NativeUninitializeAnimation();
}

void UBNLAnimInstance::BindAbilitySystem()
{
	if (bAbilitySystemBound)
	{
		return;
	}

	UAbilitySystemComponent* ASC = OwningCharacter
		? OwningCharacter->GetAbilitySystemComponent()
		: UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningActor());
	if (!ASC)
	{
		return;
	}

	AbilitySystem = ASC;

	const TPair<FGameplayTag, bool*> Rows[] = {
		{ BNTags::State_Weapon_ADS,            &GameplayTag_IsADS },
		{ BNTags::State_Weapon_Firing,         &GameplayTag_IsFiring },
		{ BNTags::State_Weapon_Reloading,      &GameplayTag_IsReloading },
		{ BNTags::State_Movement_Sprinting,    &GameplayTag_IsDashing },
		{ BNTags::State_Weapon_Melee,          &GameplayTag_IsMelee },
		{ BNTags::State_Movement_Crouching,    &GameplayTag_IsCrouching },
		{ BNTags::State_Dead,                  &GameplayTag_IsDead },
	};

	TagBools.Reset(UE_ARRAY_COUNT(Rows));
	for (const TPair<FGameplayTag, bool*>& Row : Rows)
	{
		FTagBool Bound;
		Bound.Tag = Row.Key;
		Bound.Value = Row.Value;
		Bound.Handle = ASC->RegisterGameplayTagEvent(Row.Key, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UBNLAnimInstance::OnTagChanged);
		*Row.Value = ASC->HasMatchingGameplayTag(Row.Key);
		TagBools.Add(Bound);
	}

	bAbilitySystemBound = true;
}

void UBNLAnimInstance::UnbindAbilitySystem()
{
	if (AbilitySystem)
	{
		for (FTagBool& Bound : TagBools)
		{
			AbilitySystem->UnregisterGameplayTagEvent(Bound.Handle, Bound.Tag, EGameplayTagEventType::NewOrRemoved);
		}
	}

	TagBools.Reset();
	AbilitySystem = nullptr;
	bAbilitySystemBound = false;
}

void UBNLAnimInstance::OnTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const bool bActive = NewCount > 0;
	for (FTagBool& Bound : TagBools)
	{
		if (Bound.Tag == Tag)
		{
			*Bound.Value = bActive;
			return;
		}
	}
}
