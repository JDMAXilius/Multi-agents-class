#include "Match/BNPlayerController.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "BreachpointNext.h"
#include "Core/BNGameplayTags.h"
#include "Input/BNInputComponent.h"
#include "Input/BNInputConfig.h"
#include "Match/BNPlayerState.h"
#include "Match/BNPlayerCameraManager.h"
#include "UI/BNHUDDirector.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "GameFramework/Pawn.h"

ABNPlayerController::ABNPlayerController()
{
	PlayerCameraManagerClass = ABNPlayerCameraManager::StaticClass();
}

void ABNPlayerController::SetupInputComponent()
{
	if (!InputComponent)
	{
		InputComponent = NewObject<UBNInputComponent>(this, TEXT("BNInputComponent0"));
		InputComponent->RegisterComponent();
	}

	Super::SetupInputComponent();

	if (!IsLocalController())
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			if (MappingContexts.IsEmpty())
			{
				UE_LOG(LogBN, Error, TEXT("BNPlayerController: MappingContexts is empty — set +MappingContexts under [/Script/BreachpointNext.BNPlayerController] in DefaultGame.ini."));
			}
			for (int32 Index = 0; Index < MappingContexts.Num(); ++Index)
			{
				if (UInputMappingContext* Context = MappingContexts[Index].LoadSynchronous())
				{
					Subsystem->AddMappingContext(Context, Index);
				}
				else
				{
					UE_LOG(LogBN, Error, TEXT("BNPlayerController: MappingContexts[%d] '%s' failed to load."), Index, *MappingContexts[Index].ToString());
				}
			}
		}
	}

	UBNInputComponent* BNInput = Cast<UBNInputComponent>(InputComponent);
	const UBNInputConfig* Config = InputConfig.LoadSynchronous();
	if (!BNInput || !Config)
	{
		UE_LOG(LogBN, Error, TEXT("BNPlayerController: input not bound (component=%s, InputConfig='%s')."),
			BNInput ? TEXT("ok") : TEXT("wrong class"), *InputConfig.ToString());
		return;
	}

	const auto Bind = [this, BNInput, Config](FGameplayTag Tag, ETriggerEvent Event, auto Func)
	{
		if (!BNInput->BindActionByTag(Config, Tag, Event, this, Func))
		{
			UE_LOG(LogBN, Error, TEXT("BNPlayerController: %s has no InputAction in '%s'."), *Tag.ToString(), *InputConfig.ToString());
		}
	};

	Bind(BNTags::Input_Move, ETriggerEvent::Triggered, &ABNPlayerController::HandleMove);
	Bind(BNTags::Input_Look, ETriggerEvent::Triggered, &ABNPlayerController::HandleLook);
	Bind(BNTags::Input_Jump, ETriggerEvent::Started, &ABNPlayerController::HandleJumpPressed);
	Bind(BNTags::Input_Jump, ETriggerEvent::Completed, &ABNPlayerController::HandleJumpReleased);
	Bind(BNTags::Input_Crouch, ETriggerEvent::Started, &ABNPlayerController::HandleCrouchPressed);
	Bind(BNTags::Input_Crouch, ETriggerEvent::Completed, &ABNPlayerController::HandleCrouchReleased);
	Bind(BNTags::Input_Weapon_Next, ETriggerEvent::Started, &ABNPlayerController::HandleWeaponNextPressed);
	Bind(BNTags::Input_Weapon_Previous, ETriggerEvent::Started, &ABNPlayerController::HandleWeaponPreviousPressed);
	Bind(BNTags::Input_Weapon_Fire, ETriggerEvent::Started, &ABNPlayerController::HandleFirePressed);
	Bind(BNTags::Input_Weapon_Fire, ETriggerEvent::Completed, &ABNPlayerController::HandleFireReleased);
	Bind(BNTags::Input_Weapon_Reload, ETriggerEvent::Started, &ABNPlayerController::HandleReloadPressed);
	Bind(BNTags::Input_Sprint, ETriggerEvent::Started, &ABNPlayerController::HandleSprintPressed);
	Bind(BNTags::Input_Sprint, ETriggerEvent::Completed, &ABNPlayerController::HandleSprintReleased);
	Bind(BNTags::Input_Lean_Left, ETriggerEvent::Started, &ABNPlayerController::HandleLeanLeftPressed);
	Bind(BNTags::Input_Lean_Left, ETriggerEvent::Completed, &ABNPlayerController::HandleLeanLeftReleased);
	Bind(BNTags::Input_Lean_Right, ETriggerEvent::Started, &ABNPlayerController::HandleLeanRightPressed);
	Bind(BNTags::Input_Lean_Right, ETriggerEvent::Completed, &ABNPlayerController::HandleLeanRightReleased);
	Bind(BNTags::Input_Weapon_ADS, ETriggerEvent::Started, &ABNPlayerController::HandleADSPressed);
	Bind(BNTags::Input_Weapon_ADS, ETriggerEvent::Completed, &ABNPlayerController::HandleADSReleased);
	Bind(BNTags::Input_Melee, ETriggerEvent::Started, &ABNPlayerController::HandleMeleePressed);
	Bind(BNTags::Input_Grenade, ETriggerEvent::Started, &ABNPlayerController::HandleGrenadePressed);
	Bind(BNTags::Input_Grapple, ETriggerEvent::Started, &ABNPlayerController::HandleGrapplePressed);
	// THE LINE THAT MADE KEY 2 DO NOTHING. Every other link in the dash's chain existed —
	// the IA asset, the IMC binding, the Input.Dash tag pairing in DA_BNInput, the granted
	// ability — but this list is EXPLICIT, so a tag nobody binds here reaches no handler and
	// the press dies silently between Enhanced Input and the ASC. Nothing logs it, because
	// from the input system's side the key worked perfectly.
	Bind(BNTags::Input_Dash, ETriggerEvent::Started, &ABNPlayerController::HandleDashPressed);
	Bind(BNTags::Input_Scoreboard, ETriggerEvent::Started, &ABNPlayerController::HandleScoreboardPressed);
	Bind(BNTags::Input_Scoreboard, ETriggerEvent::Completed, &ABNPlayerController::HandleScoreboardReleased);
	Bind(BNTags::Input_Menu, ETriggerEvent::Started, &ABNPlayerController::HandleMenuPressed);
}

void ABNPlayerController::HandleMove(const FInputActionValue& Value)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn || IsDead())
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotationMatrix YawMatrix(FRotator(0.f, GetControlRotation().Yaw, 0.f));
	ControlledPawn->AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Axis.Y);
	ControlledPawn->AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Axis.X);
}

void ABNPlayerController::HandleLook(const FInputActionValue& Value)
{
	if (IsDead())
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	AddYawInput(Axis.X);
	AddPitchInput(Axis.Y);
}

void ABNPlayerController::HandleJumpPressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Jump);
	}
}

void ABNPlayerController::HandleJumpReleased()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(BNTags::Input_Jump);
	}
}

void ABNPlayerController::HandleCrouchPressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Crouch);
	}
}

void ABNPlayerController::HandleCrouchReleased()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(BNTags::Input_Crouch);
	}
}

void ABNPlayerController::HandleWeaponNextPressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Weapon_Next);
	}
}

void ABNPlayerController::HandleWeaponPreviousPressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Weapon_Previous);
	}
}

void ABNPlayerController::HandleFirePressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Weapon_Fire);
	}
}

void ABNPlayerController::HandleFireReleased()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(BNTags::Input_Weapon_Fire);
	}
}

void ABNPlayerController::HandleReloadPressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Weapon_Reload);
	}
}

void ABNPlayerController::HandleSprintPressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Sprint);
	}
}

void ABNPlayerController::HandleSprintReleased()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(BNTags::Input_Sprint);
	}
}

void ABNPlayerController::HandleLeanLeftPressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Lean_Left);
	}
}

void ABNPlayerController::HandleLeanLeftReleased()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(BNTags::Input_Lean_Left);
	}
}

void ABNPlayerController::HandleLeanRightPressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Lean_Right);
	}
}

void ABNPlayerController::HandleLeanRightReleased()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(BNTags::Input_Lean_Right);
	}
}

void ABNPlayerController::HandleADSPressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Weapon_ADS);
	}
}

void ABNPlayerController::HandleADSReleased()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(BNTags::Input_Weapon_ADS);
	}
}

void ABNPlayerController::HandleMeleePressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Melee);
	}
}

void ABNPlayerController::HandleGrenadePressed()
{
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Grenade);
	}
}

void ABNPlayerController::HandleDashPressed()
{
	// A tap, like the grapple and the grenade: the dash owns its own lifetime through its
	// launch and its timer, so there is no release to forward.
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Dash);
	}
}

void ABNPlayerController::HandleGrapplePressed()
{
	// A tap like the grenade — the pull's lifecycle is the movement component's, so
	// there is no release to forward.
	if (UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(BNTags::Input_Grapple);
	}
}

void ABNPlayerController::HandleScoreboardPressed()
{
	// Deliberately NOT dead-gated and NOT frozen-gated: reading the score is always legal —
	// warmup, mid-fight, dead, post-match. The director owns what "show" means in each.
	if (UBNHUDDirector* Director = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UBNHUDDirector>() : nullptr)
	{
		Director->SetScoreboardHeld(true);
	}
}

void ABNPlayerController::HandleScoreboardReleased()
{
	if (UBNHUDDirector* Director = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UBNHUDDirector>() : nullptr)
	{
		Director->SetScoreboardHeld(false);
	}
}

void ABNPlayerController::HandleMenuPressed()
{
	if (UBNHUDDirector* Director = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UBNHUDDirector>() : nullptr)
	{
		Director->OpenPauseMenu();
	}
}

void ABNPlayerController::LeaveMatch()
{
	if (LeaveMatchMapPath.IsEmpty())
	{
		UE_LOG(LogBN, Warning, TEXT("BNPlayerController: LeaveMatch has nowhere to go — set LeaveMatchMapPath in [/Script/BreachpointNext.BNPlayerController]. No front-end map exists yet."));
		return;
	}

	// THE HOST CASE (critic): ClientTravel is right for a remote client and destructive for the
	// listen-server host — browsing the host away takes the server world with it and strands
	// every connected client. The old module routed this through a session subsystem's
	// ReturnToMainMenu(); BN has no session layer yet, so a host with clients simply cannot leave.
	// Alone on the listen server (or in PIE), there is nobody to strand and travel is allowed.
	if (HasAuthority())
	{
		const UNetDriver* NetDriver = GetWorld() ? GetWorld()->GetNetDriver() : nullptr;
		if (NetDriver && NetDriver->ClientConnections.Num() > 0)
		{
			UE_LOG(LogBN, Warning, TEXT("BNPlayerController: LeaveMatch REFUSED on the LISTEN HOST — %d client(s) connected, and travelling would end the match for all of them. BN has no session layer to hand the server off."),
				NetDriver->ClientConnections.Num());
			return;
		}
	}

	UE_LOG(LogBN, Log, TEXT("BNPlayerController: leaving the match -> %s"), *LeaveMatchMapPath);
	ClientTravel(LeaveMatchMapPath, TRAVEL_Absolute);
}

UBNAbilitySystemComponent* ABNPlayerController::GetBNAbilitySystemComponent() const
{
	const ABNPlayerState* PS = GetPlayerState<ABNPlayerState>();
	return PS ? PS->GetBNAbilitySystemComponent() : nullptr;
}

bool ABNPlayerController::IsDead() const
{
	const UBNAbilitySystemComponent* ASC = GetBNAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(BNTags::State_Dead);
}
