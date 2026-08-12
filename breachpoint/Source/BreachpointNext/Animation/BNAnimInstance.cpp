#include "Animation/BNAnimInstance.h"
#include "Characters/BNCharacter.h"
#include "Core/BNGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

void UBNAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Character = Cast<ABNCharacter>(TryGetPawnOwner());
	MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;

	ResolveAbilitySystem();
	RefreshAnimLayer();
}

void UBNAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!Character)
	{
		Character = Cast<ABNCharacter>(TryGetPawnOwner());
		MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	}
	if (!Character || !MovementComponent)
	{
		return;
	}

	if (!AbilitySystem)
	{
		ResolveAbilitySystem();
	}
	RefreshAnimLayer();

	const FVector Velocity = MovementComponent->Velocity;
	SnapVelocity2D = FVector(Velocity.X, Velocity.Y, 0.0);
	SnapRotation = Character->GetActorRotation();
	bSnapHasAcceleration = !MovementComponent->GetCurrentAcceleration().IsNearlyZero();
	bSnapFalling = MovementComponent->IsFalling();
	bSnapInAirTag = bTagInAir;
	bSnapCrouching = bTagCrouching;
	bSnapJumping = bTagJumping;
}

void UBNAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	DisplacementSpeed = SnapVelocity2D.Size();
	LocalVelocity2D = SnapRotation.UnrotateVector(SnapVelocity2D);
	LocalVelocityDirectionAngle = CalculateDirection(SnapVelocity2D, SnapRotation);
	HasVelocity = DisplacementSpeed > UE_KINDA_SMALL_NUMBER;
	HasAcceleration = bSnapHasAcceleration;
	// Tag OR CMC: walking off a ledge activates no jump ability, so the InAir tag alone
	// would miss it — the movement component covers that half.
	IsFalling = bSnapInAirTag || bSnapFalling;
	IsOnGround = !IsFalling;
	isCrouching = bSnapCrouching;
	IsJumping = bSnapJumping;
}

void UBNAnimInstance::NativeUninitializeAnimation()
{
	if (AbilitySystem)
	{
		AbilitySystem->UnregisterGameplayTagEvent(CrouchTagHandle, BNTags::State_Movement_Crouching, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(JumpTagHandle, BNTags::State_Movement_Jumping, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem->UnregisterGameplayTagEvent(InAirTagHandle, BNTags::State_Movement_InAir, EGameplayTagEventType::NewOrRemoved);
		AbilitySystem = nullptr;
	}
	CrouchTagHandle.Reset();
	JumpTagHandle.Reset();
	InAirTagHandle.Reset();

	Super::NativeUninitializeAnimation();
}

void UBNAnimInstance::ResolveAbilitySystem()
{
	if (!Character)
	{
		return;
	}

	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	AbilitySystem = ASC;
	CrouchTagHandle = ASC->RegisterGameplayTagEvent(BNTags::State_Movement_Crouching, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UBNAnimInstance::OnTagChanged);
	JumpTagHandle = ASC->RegisterGameplayTagEvent(BNTags::State_Movement_Jumping, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UBNAnimInstance::OnTagChanged);
	InAirTagHandle = ASC->RegisterGameplayTagEvent(BNTags::State_Movement_InAir, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UBNAnimInstance::OnTagChanged);

	bTagCrouching = ASC->HasMatchingGameplayTag(BNTags::State_Movement_Crouching);
	bTagJumping = ASC->HasMatchingGameplayTag(BNTags::State_Movement_Jumping);
	bTagInAir = ASC->HasMatchingGameplayTag(BNTags::State_Movement_InAir);
}

void UBNAnimInstance::RefreshAnimLayer()
{
	if (!Character)
	{
		return;
	}

	UClass* LayerClass = Character->ResolveAnimLayerClass();
	if (!LayerClass || LayerClass == LinkedLayerClass)
	{
		return;
	}

	if (USkeletalMeshComponent* Mesh = GetOwningComponent())
	{
		Mesh->LinkAnimClassLayers(LayerClass);
		LinkedLayerClass = LayerClass;
	}
}

void UBNAnimInstance::OnTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	const bool bActive = NewCount > 0;
	if (Tag == BNTags::State_Movement_Crouching)
	{
		bTagCrouching = bActive;
	}
	else if (Tag == BNTags::State_Movement_Jumping)
	{
		bTagJumping = bActive;
	}
	else if (Tag == BNTags::State_Movement_InAir)
	{
		bTagInAir = bActive;
	}
}
