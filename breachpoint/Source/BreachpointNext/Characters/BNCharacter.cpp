#include "Characters/BNCharacter.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "Match/BNPlayerState.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ABNCharacter::ABNCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(GetCapsuleComponent());
	CameraComponent->SetRelativeLocation(FVector(0.f, 0.f, CameraStandingHeight));
	CameraComponent->bUsePawnControlRotation = true;

	bUseControllerRotationYaw = true;

	// The 3P body is everyone else's view of us; the owner sees the 1P arms instead.
	GetMesh()->SetOwnerNoSee(true);

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCharacterMovement()->bCanWalkOffLedgesWhenCrouching = true;
}

void ABNCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CameraComponent->SetRelativeLocation(FVector(0.f, 0.f, CameraStandingHeight - HalfHeightAdjust));
}

void ABNCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CameraComponent->SetRelativeLocation(FVector(0.f, 0.f, CameraStandingHeight));
}

void ABNCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The ASC outlives the pawn (it is the PlayerState's); an unregistered binding
	// per respawn accumulates on it forever.
	if (MoveSpeedChangedHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBNAttributeSet::GetMoveSpeedAttribute())
				.Remove(MoveSpeedChangedHandle);
		}
		MoveSpeedChangedHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

UAbilitySystemComponent* ABNCharacter::GetAbilitySystemComponent() const
{
	const ABNPlayerState* PS = GetPlayerState<ABNPlayerState>();
	return PS ? PS->GetBNAbilitySystemComponent() : nullptr;
}

void ABNCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitializeAbilitySystem();

	if (ABNPlayerState* PS = GetPlayerState<ABNPlayerState>())
	{
		PS->GrantDefaults();
	}
}

void ABNCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializeAbilitySystem();
}

void ABNCharacter::InitializeAbilitySystem()
{
	ABNPlayerState* PS = GetPlayerState<ABNPlayerState>();
	if (!PS)
	{
		return;
	}

	UBNAbilitySystemComponent* ASC = PS->GetBNAbilitySystemComponent();
	ASC->InitAbilityActorInfo(PS, this);

	if (!MoveSpeedChangedHandle.IsValid())
	{
		MoveSpeedChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBNAttributeSet::GetMoveSpeedAttribute())
			.AddUObject(this, &ABNCharacter::OnMoveSpeedChanged);
	}

	const float MoveSpeed = ASC->GetNumericAttribute(UBNAttributeSet::GetMoveSpeedAttribute());
	if (MoveSpeed > 0.f)
	{
		GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	}
}

void ABNCharacter::OnMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
	GetCharacterMovement()->MaxWalkSpeed = Data.NewValue;
}

// The current-weapon seam: no weapons exist yet — the weapons roadmap returns the equipped layer here.
UClass* ABNCharacter::GetCurrentWeaponAnimLayer() const
{
	return nullptr;
}

UClass* ABNCharacter::ResolveAnimLayerClass()
{
	if (UClass* WeaponLayer = GetCurrentWeaponAnimLayer())
	{
		return WeaponLayer;
	}

	if (!bUnarmedAnimLayerResolveAttempted)
	{
		bUnarmedAnimLayerResolveAttempted = true;
		CachedUnarmedAnimLayer = UnarmedAnimLayer.IsNull() ? nullptr : UnarmedAnimLayer.TryLoadClass<UAnimInstance>();
	}
	return CachedUnarmedAnimLayer;
}
