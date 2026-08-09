// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/NMCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Characters/Components/NMHealthComponent.h"
#include "Characters/Components/NMWeaponComponent.h"
#include "Engine/DamageEvents.h"
#include "Game/NMGameMode.h"
#include "InventorySystem/Weapons/NMWeaponActor.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Player/NMPlayerController.h"
#include "Player/NMPlayerState.h"
#include "UI/NMHUDBase.h"

// Sets default values
ANMCharacterBase::ANMCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RightHandSocket(FName{ "RightHandSocket" })
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SpringArmComponent = CreateOptionalDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	if (SpringArmComponent.Get())
	{
		SpringArmComponent->SetupAttachment(RootComponent);
		SpringArmComponent->TargetArmLength = 250.f;
		SpringArmComponent->SocketOffset = FVector{ 0.f, 45.f, 80.f };
		SpringArmComponent->bUsePawnControlRotation = true;
		SpringArmComponent->bDoCollisionTest = true;
		SpringArmComponent->bInheritYaw = true;
		SpringArmComponent->bEnableCameraLag = false;

		CameraComponent = CreateOptionalDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
		CameraComponent->SetupAttachment(SpringArmComponent.Get(), USpringArmComponent::SocketName);
		CameraComponent->bUsePawnControlRotation = false;
	}

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	GetCapsuleComponent()->SetCapsuleHalfHeight(90.f);

	GetMovementComponent()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->bCanWalkOffLedgesWhenCrouching = true;

	// Create health component
	HealthComponent = CreateDefaultSubobject<UNMHealthComponent>(TEXT("HealthComponent"));
	check(HealthComponent)
	HealthComponent->OnDeathStarted.AddDynamic(this, &ThisClass::OnDeathStarted);
	HealthComponent->OnDeathFinished.AddDynamic(this, &ThisClass::OnDeathFinished);
	HealthComponent->OnHealthChanged.AddDynamic(this, &ThisClass::OnHealthChanged);
	
	// Create weapon component
	WeaponComponent = CreateDefaultSubobject<UNMWeaponComponent>(TEXT("WeaponComponent"));
	check(WeaponComponent)
}

void ANMCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANMCharacterBase, GameplaytoTag);
}

// Called when the game starts or when spawned
void ANMCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	
}

ANMPlayerState* ANMCharacterBase::GetNMPlayerState()
{
	if (!NMPlayerState.Get())
	{
		if (APlayerState* PS = GetPlayerState())
		{
			if (ANMPlayerState* NMPS = Cast<ANMPlayerState>(PS))
			{
				NMPlayerState = NMPS;
			}
		}
	}

	return NMPlayerState.Get();
}

void ANMCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	NewController->SetPawn(this);
	ClientShowInGameMenu(NewController);
}

void ANMCharacterBase::EnableRagdoll()
{
	ScheduleFinishDeath();
	// Enable physics on the skeletal mesh
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->WakeAllRigidBodies();
	GetMesh()->bBlendPhysics = true;
	
	if (GetMesh()->IsSimulatingPhysics())
	{
		// Clear any existing forces
		GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	
	// Apply a force/impulse based on the cause of death
	FVector DeathImpulse = CalculateDeathImpulse(); // Implement your logic
	//GetMesh()->AddImpulse(DeathImpulse, NAME_None, true);
	
	// Detach the capsule from the mesh so the player mesh doesn't move with the capsule
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

	// Adjust Character Movement
	GetCharacterMovement()->BrakingFrictionFactor = 2.0f; // Adjust as needed
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->GravityScale = 1.0f;
	GetCharacterMovement()->GroundFriction = 8.0f;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	
	// Disable character controls
	DisableInput(nullptr);

	GetWeaponComponent()->GetEquippedWeapon()->SetActorEnableCollision(false);
	//GetWeaponComponent()->GetEquippedWeapon()->Destroy();
	//USkeletalMeshComponent* SkeletalMesh = GetMesh()->SkeletalMesh;
	/*FBodyInstance* BodyInstance = SkeletalMesh->GetBodyInstance(BodyName);
	BodyInstance->SetCollisionEnabled(ECollisionEnabled::NoCollision);*/
	GetMesh()->SetLinearDamping(0.3f);
	GetMesh()->SetAngularDamping(0.3f);
	UPhysicsSettings* PhysSettings = GetMutableDefault<UPhysicsSettings>();
	PhysSettings->bSubstepping = true;
	PhysSettings->MaxSubstepDeltaTime = 0.01f;
	PhysSettings->MaxSubsteps = 3;
	PhysSettings->MaxPhysicsDeltaTime = 0.01f;
	PhysSettings->bTickPhysicsAsync = true;
	// Reset velocities to avoid jitter
	GetMesh()->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
	GetMesh()->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	
	FTimerDelegate DeathCallback = FTimerDelegate::CreateLambda([this]() 
	{ 
		//GetCharacterMovement()->StopMovementImmediately();
		//GetMesh()->PutAllRigidBodiesToSleep();
		GetMesh()->bBlendPhysics = false;
	});
	// Set a 2-second timer to call the lambda after the delay
	FTimerHandle DeathTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(DeathTimerHandle, DeathCallback, 1.5, false);
}

FVector ANMCharacterBase::CalculateDeathImpulse()
{
	// Example logic: Adjust force based on the cause of death
	FVector ImpulseDirection = FVector::UpVector + FVector::ForwardVector; // Example direction
	float ImpulseStrength = 1000.0f; // Adjust as needed
	return ImpulseDirection * ImpulseStrength;
}

void ANMCharacterBase::ClientShowInGameMenu_Implementation(AController* NewController)
{
	if (const APlayerController* PlayerController = Cast<APlayerController>(NewController))
	{
		if (ANMHUDBase* HUD = PlayerController->GetHUD<ANMHUDBase>())
		{
			HUD->ShowInGameMenu();
		}
	}
}

void ANMCharacterBase::CharacterHitReaction(const FVector InDirection, const FVector HitLocation)
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HitReactMontage)
	{
		AnimInstance->Montage_Play(HitReactMontage);
	}
}

void ANMCharacterBase::OnHealthChanged(UNMHealthComponent* inHealthComponent, float OldValue, float NewValue,
	float ScalarValue, AActor* DamageInstigator, AActor* DamageCauser)
{
	if (NewValue <= 0.0f) return; //TODO:Replication server to client on getting shot

	if (GetHealthComponent()->GetHealth() < OldValue)
	{
		CharacterHitReaction(FVector::ZeroVector,FVector::ZeroVector);
	}
}

void ANMCharacterBase::OnDeathStarted(APlayerState* KillerState, APlayerState* VictimState)
{
	if (HasAuthority())
	{
		if (DeathMontage)
		{
			MulticastPlayAnimation_Implementation(DeathMontage, TEXT("OnDeathMontageEnded"));
		}
	}
	EnableRagdoll();
	//ScheduleFinishDeath();
	
	ANMGameMode* NMGameMode = Cast<ANMGameMode>(UGameplayStatics::GetGameMode(this));
	if (NMGameMode)
	{
		if (ANMPlayerState* Killer = Cast<ANMPlayerState>(KillerState))
		{
			if (ANMPlayerState* Victim = Cast<ANMPlayerState>(VictimState))
			{
				// Call the GameMode's PlayerEliminated function with PlayerStates
				NMGameMode->PlayerEliminated(Killer, Victim);
			}
			else
			{
				// Call the GameMode's PlayerEliminated function with PlayerStates
				NMGameMode->PlayerEliminated(Killer, GetNMPlayerState());
			}
		}
	}
	if (Controller)
	{
		Controller->SetIgnoreMoveInput(true);
		if (const APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			if (ANMHUDBase* HUD = PlayerController->GetHUD<ANMHUDBase>())
			{
				HUD->HideInGameMenu();
			}
		}
	}

	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	check(CapsuleComp);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleComp->SetCollisionResponseToAllChannels(ECR_Ignore);

	UCharacterMovementComponent* MoveComp = CastChecked<UCharacterMovementComponent>(GetCharacterMovement());
	MoveComp->StopMovementImmediately();
	MoveComp->DisableMovement();
	MoveComp->SetComponentTickEnabled(false);
}

void ANMCharacterBase::OnDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	ScheduleFinishDeath();
}

void ANMCharacterBase::OnDeathFinished()
{
	HealthComponent->FinishDeath();
	FTimerDelegate DeathCallback = FTimerDelegate::CreateLambda([this]() 
	{ 
		if (GetLocalRole() == ROLE_Authority)
		{
			//DetachFromControllerPendingDestroy();
			SetLifeSpan(DeathTtime);
		}
	});
	if(ANMPlayerController* NMController = Cast<ANMPlayerController>(GetController()))
	{
		NMController->Server_RespawnCharacter();
	}
	// Set a 2-second timer to call the lambda after the delay
	FTimerHandle DeathTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(DeathTimerHandle, DeathCallback, DeathTtime, false);
}

void ANMCharacterBase::MulticastPlayAnimation_Implementation(UAnimMontage* AnimMontage, const FName& InFunctionName)
{
	if (AnimMontage)
	{
		float MontageDuration = PlayAnimMontage(AnimMontage);

		// Bind function to execute when montage ends
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUFunction(this, InFunctionName);
		GetMesh()->GetAnimInstance()->Montage_SetEndDelegate(EndDelegate, AnimMontage);
	}
}

void ANMCharacterBase::ScheduleFinishDeath()
{
	// Set a 2-second timer for finishing death
	FTimerHandle FinishDeathTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(FinishDeathTimerHandle, this, &ANMCharacterBase::OnDeathFinished, DeathTtime, false);
}

float ANMCharacterBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Get the PlayerState for both the instigator (killer) and this character (victim)
	APlayerState* InstigatorState = EventInstigator ? EventInstigator->GetPlayerState<APlayerState>() : nullptr;
	APlayerState* VictimState = GetPlayerState<APlayerState>();
	HealthComponent->TakeDamage(DamageAmount, EventInstigator, DamageCauser,InstigatorState,VictimState);
	return DamageAmount;
}

float ANMCharacterBase::InternalTakePointDamage(float Damage, FPointDamageEvent const& PointDamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	HealthComponent->TakeDamage(Damage);
	CharacterHitReaction(PointDamageEvent.ShotDirection,PointDamageEvent.HitInfo.Location);//need to be replicated 
	return Super::InternalTakePointDamage(Damage, PointDamageEvent, EventInstigator, DamageCauser);
}

void ANMCharacterBase::OnRep_GameplaytoTag()
{
	OnGameplayTagChanged.Broadcast(GameplaytoTag);
}