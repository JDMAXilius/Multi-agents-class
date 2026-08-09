#include "FPS/BPAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Character/BPCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Core/BRGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UBPAnimInstance::UBPAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBPAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwningCharacter = Cast<ABPCharacter>(TryGetPawnOwner());
	if (ABPCharacter* Character = OwningCharacter.Get())
	{
		OwningMovement = Character->GetCharacterMovement();
	}

	BindAbilitySystem();
}

void UBPAnimInstance::NativeUninitializeAnimation()
{
	UnbindAbilitySystem();
	Super::NativeUninitializeAnimation();
}

void UBPAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwningCharacter.IsValid())
	{
		OwningCharacter = Cast<ABPCharacter>(TryGetPawnOwner());
		if (ABPCharacter* Character = OwningCharacter.Get())
		{
			OwningMovement = Character->GetCharacterMovement();
		}
	}

	if (!BoundASC.IsValid())
	{
		BindAbilitySystem();
	}

	ABPCharacter* Character = OwningCharacter.Get();
	UCharacterMovementComponent* Move = OwningMovement.Get();

	if (Character)
	{
		CachedVelocity = Character->GetVelocity();
		CachedGroundDistance = ComputeGroundDistance(Character, Move);
	}
	else
	{
		CachedVelocity = FVector::ZeroVector;
		CachedGroundDistance = -1.f;
	}

	if (Move)
	{
		bCachedOnGround = Move->IsMovingOnGround();
		bCachedFalling = Move->IsFalling();
	}
	else
	{
		bCachedOnGround = false;
		bCachedFalling = false;
	}
}

void UBPAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	Velocity = CachedVelocity;
	GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.f).Size();
	bHasVelocity = GroundSpeed > KINDA_SMALL_NUMBER;
	bIsOnGround = bCachedOnGround;
	bIsFalling = bCachedFalling;
	GroundDistance = CachedGroundDistance;

	// Publish game-thread tag copy — worker is the only writer of graph-facing bools.
	GameplayTag_IsADS = TagState.bADS;
	GameplayTag_IsFiring = TagState.bFiring;
	GameplayTag_IsDashing = TagState.bDashing;
	GameplayTag_IsMelee = TagState.bMelee;
	GameplayTag_IsReloading = TagState.bReloading;
	GameplayTag_IsDead = TagState.bDead;
}

const TArray<UBPAnimInstance::FBPTagBinding>& UBPAnimInstance::GetTagBindings() const
{
	// C++ table — no ABP GameplayTagPropertyMap. Only tags that exist in BRGameplayTags.
	// ADS / Firing have no State.* tags yet (same gap as UBRAnimInstance) — bools stay false
	// until Core declares them; do not invent tags here.
	static const TArray<FBPTagBinding> Bindings = {
		{ BRGameplayTags::State_Movement_Sprinting, &FBPAnimTagState::bDashing },
		{ BRGameplayTags::State_Combat_Meleeing,    &FBPAnimTagState::bMelee },
		{ BRGameplayTags::State_Weapon_Reloading,   &FBPAnimTagState::bReloading },
		{ BRGameplayTags::State_Dead,               &FBPAnimTagState::bDead },
	};
	return Bindings;
}

void UBPAnimInstance::BindAbilitySystem()
{
	const AActor* OwnerActor = GetOwningActor();
	if (!OwnerActor)
	{
		return;
	}

	const IAbilitySystemInterface* AbilityInterface = Cast<const IAbilitySystemInterface>(OwnerActor);
	if (!AbilityInterface)
	{
		return;
	}

	UAbilitySystemComponent* ASC = AbilityInterface->GetAbilitySystemComponent();
	if (!ASC || BoundASC.Get() == ASC)
	{
		return;
	}

	UnbindAbilitySystem();

	for (const FBPTagBinding& Binding : GetTagBindings())
	{
		if (!Binding.Tag.IsValid())
		{
			continue;
		}

		TagHandles.Add(
			ASC->RegisterGameplayTagEvent(Binding.Tag, EGameplayTagEventType::NewOrRemoved)
			   .AddUObject(this, &UBPAnimInstance::OnStateTagChanged));

		TagState.*(Binding.Field) = ASC->HasMatchingGameplayTag(Binding.Tag);
	}

	BoundASC = ASC;
}

void UBPAnimInstance::UnbindAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		const TArray<FBPTagBinding>& Bindings = GetTagBindings();
		for (int32 Index = 0; Index < TagHandles.Num() && Index < Bindings.Num(); ++Index)
		{
			ASC->RegisterGameplayTagEvent(Bindings[Index].Tag, EGameplayTagEventType::NewOrRemoved)
			   .Remove(TagHandles[Index]);
		}
	}

	TagHandles.Reset();
	BoundASC.Reset();
	TagState = FBPAnimTagState{};
}

void UBPAnimInstance::OnStateTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	for (const FBPTagBinding& Binding : GetTagBindings())
	{
		if (Binding.Tag == Tag)
		{
			TagState.*(Binding.Field) = NewCount > 0;
			return;
		}
	}
}

float UBPAnimInstance::ComputeGroundDistance(const ACharacter* Character, const UCharacterMovementComponent* MoveComp) const
{
	if (!Character || !MoveComp)
	{
		return -1.f;
	}

	if (MoveComp->MovementMode == MOVE_Walking || MoveComp->MovementMode == MOVE_NavWalking)
	{
		return 0.f;
	}

	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (!Capsule || !Character->GetWorld())
	{
		return -1.f;
	}

	const float CapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const ECollisionChannel CollisionChannel = Capsule->GetCollisionObjectType();
	const FVector TraceStart = Character->GetActorLocation();
	const FVector TraceEnd(TraceStart.X, TraceStart.Y, TraceStart.Z - GroundTraceDistance - CapsuleHalfHeight);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BPAnimInstance_GroundDistance), false, Character);
	FHitResult HitResult;
	Character->GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, CollisionChannel, QueryParams);

	if (HitResult.bBlockingHit)
	{
		return FMath::Max(HitResult.Distance - CapsuleHalfHeight, 0.f);
	}

	return GroundTraceDistance;
}
