// Copyright Zollpa LLC.

#include "ZoransPlayerCharacter.h"
#include "Components/ZoransInteractionComponent.h"
#include "ZoransResistance/AbilitySystem/ZoransAbilitySystemComponent.h"


AZoransPlayerCharacter::AZoransPlayerCharacter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	InteractionComponent = CreateDefaultSubobject<UZoransInteractionComponent>(TEXT("InteractionComponent"));
	InteractionComponent->SetAutoActivate(false);
}

void AZoransPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (GetInteractionComponent() && IsPlayerControlled() && IsLocallyControlled())
	{
		GetInteractionComponent()->Activate();
	}
}

UZoransInteractionComponent* AZoransPlayerCharacter::GetInteractionComponent() const
{
	return InteractionComponent.Get();
}

void AZoransPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	GetMesh()->bOnlyAllowAutonomousTickPose = false;
	GetHeadMesh()->bOnlyAllowAutonomousTickPose = false;
}

void AZoransPlayerCharacter::CalculateAimVector_Implementation()
{
	if (APlayerController* PC = GetController<APlayerController>())
	{
		FVector CameraLocation = FVector::ZeroVector;
		FRotator DirectionRot = FRotator::ZeroRotator;
		PC->GetPlayerViewPoint(CameraLocation, DirectionRot);

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		const FVector TraceEnd = CameraLocation + DirectionRot.Vector() * 3000.f;
		
		FHitResult AimHit;
		if (GetWorld()->LineTraceSingleByChannel(AimHit, CameraLocation, TraceEnd, ECC_Visibility, QueryParams))
		{
			AimVector = AimHit.ImpactPoint;
		}
		else
		{
			AimVector = TraceEnd;
		}
		
		if (GetNetMode() == NM_Client)
		{
			SRV_SetAimVector(AimVector);
		}
	}
}

void AZoransPlayerCharacter::SRV_SetAimVector_Implementation(const FVector InVector)
{
	AimVector = InVector;
}
void AZoransPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (IsPlayerControlled() && IsLocallyControlled())
	{
		Super::SetupPlayerInputComponent(PlayerInputComponent);

		if (AbilitySystemComponent.Get())
		{
			AbilitySystemComponent->SetupAbilityEnhancedInputBinds();
		}
	}
}