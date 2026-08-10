#include "FPS/BPFPSCharacter.h"

#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"

#include "Match/BPPlayerState.h"

ABPFPSCharacter::ABPFPSCharacter()
{
	// EMPTY ON PURPOSE, INCLUDING THE TICK FLAGS.
	//
	// An earlier version set `bCanEverTick = false` here on law 4 grounds. That is the right
	// default for a pawn this project authors and the wrong one for a reparent target:
	// `BP_FPST_Character` inherits `bCanEverTick` from `ACharacter` (true) and drives five
	// procedural components from its own Event Tick, so a parent that flips the flag switches
	// off the Blueprint's animation without touching the Blueprint. Law 4 governs gameplay Tick
	// in code we write; it does not get enforced by silently disabling a bought asset's graph.
	//
	// Every component, capsule dimension, movement default and tick setting belongs to the
	// Blueprint that reparents onto this class -- see the class comment.
}

UAbilitySystemComponent* ABPFPSCharacter::GetAbilitySystemComponent() const
{
	const ABPPlayerState* PS = GetPlayerState<ABPPlayerState>();
	return PS ? PS->GetAbilitySystemComponent() : nullptr;
}

void ABPFPSCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystem();
}

void ABPFPSCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilitySystem();
}

void ABPFPSCharacter::InitializeAbilitySystem()
{
	ABPPlayerState* PS = GetPlayerState<ABPPlayerState>();
	if (!PS)
	{
		return;
	}

	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// Both paths reach here -- server through PossessedBy, client through OnRep_PlayerState --
	// and whichever arrives second re-points the avatar at the same pawn harmlessly.
	ASC->InitAbilityActorInfo(PS, this);
}

const UInputAction* ABPFPSCharacter::ResolveAction(const TSoftObjectPtr<UInputAction>& SoftAction, const TCHAR* Name)
{
	if (SoftAction.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("BPFPSCharacter: '%s' is unset -- pin it under [/Script/Breachpoint.BPFPSCharacter] in DefaultGame.ini."), Name);
		return nullptr;
	}

	const UInputAction* Action = SoftAction.LoadSynchronous();
	if (!Action)
	{
		UE_LOG(LogTemp, Error, TEXT("BPFPSCharacter: '%s' path '%s' failed to load."), Name, *SoftAction.ToSoftObjectPath().ToString());
	}
	return Action;
}

void ABPFPSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("BPFPSCharacter: InputComponent is not EnhancedInput."));
		return;
	}

	// Move and Look only. Jump and every ability verb are bound by ABPPlayerController against
	// the ASC's held-input buffer; binding Jump here as well would fire it twice per press.
	if (const UInputAction* Move = ResolveAction(MoveAction, TEXT("MoveAction")))
	{
		EnhancedInputComponent->BindAction(Move, ETriggerEvent::Triggered, this, &ABPFPSCharacter::MoveInput);
	}

	if (const UInputAction* Look = ResolveAction(LookAction, TEXT("LookAction")))
	{
		EnhancedInputComponent->BindAction(Look, ETriggerEvent::Triggered, this, &ABPFPSCharacter::LookInput);
	}

	if (const UInputAction* MouseLook = ResolveAction(MouseLookAction, TEXT("MouseLookAction")))
	{
		EnhancedInputComponent->BindAction(MouseLook, ETriggerEvent::Triggered, this, &ABPFPSCharacter::LookInput);
	}
}

void ABPFPSCharacter::MoveInput(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void ABPFPSCharacter::LookInput(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoAim(LookAxisVector.X, LookAxisVector.Y);
}

void ABPFPSCharacter::DoAim(float Yaw, float Pitch)
{
	if (GetController())
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ABPFPSCharacter::DoMove(float Right, float Forward)
{
	if (!GetController())
	{
		return;
	}

	// Off the CONTROL rotation, not the actor's. The template Blueprint can toggle to a
	// spring-arm camera and does not force bUseControllerRotationYaw, so the actor's forward
	// vector is not reliably where the player is looking. Yaw only: pitch would let a player
	// walk into the floor by looking down.
	const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
	const FRotationMatrix YawMatrix(YawRotation);

	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Forward);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Right);
}
