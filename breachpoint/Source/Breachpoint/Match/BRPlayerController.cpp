#include "Match/BRPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Camera/BRPlayerCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "Breachpoint.h"
#include "Widgets/Input/SVirtualJoystick.h"

#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "Core/BRGameplayTags.h"
#include "Match/BRPlayerState.h"

ABRPlayerController::ABRPlayerController()
{
	PlayerCameraManagerClass = ABRPlayerCameraManager::StaticClass();
}

UBRAbilitySystemComponent* ABRPlayerController::GetBRAbilitySystemComponent() const
{
	if (const ABRPlayerState* BRPlayerState = GetPlayerState<ABRPlayerState>())
	{
		if (UBRAbilitySystemComponent* ASC = BRPlayerState->GetBRAbilitySystemComponent())
		{
			return ASC;
		}
	}

	// Transition fallback: the pawn forwards to the same PlayerState ASC, but it resolves in
	// the window where GetPlayerState<> on the controller has not caught up yet.
	if (const APawn* ControlledPawn = GetPawn())
	{
		if (const ABRPlayerState* PawnPlayerState = ControlledPawn->GetPlayerState<ABRPlayerState>())
		{
			return PawnPlayerState->GetBRAbilitySystemComponent();
		}
	}

	return nullptr;
}

void ABRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			MobileControlsWidget->AddToPlayerScreen(0);
		}
	}
}

void ABRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
		{
			Subsystem->AddMappingContext(CurrentContext, 0);
		}

		if (!ShouldUseTouchControls())
		{
			for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}
		}
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	// Started/Completed on every verb, not just the held ones: the ASC's buffer is what makes
	// WhileInputHeld abilities work, and it only unwinds if the release actually arrives.
	auto BindPress = [this, EnhancedInput](const TSoftObjectPtr<UInputAction>& SoftAction, const TCHAR* VerbName, auto PressFunc)
	{
		if (const UInputAction* Action = ResolveAction(SoftAction, VerbName))
		{
			EnhancedInput->BindAction(Action, ETriggerEvent::Started, this, PressFunc);
		}
	};

	auto BindPressRelease = [this, EnhancedInput](const TSoftObjectPtr<UInputAction>& SoftAction, const TCHAR* VerbName, auto PressFunc, auto ReleaseFunc)
	{
		if (const UInputAction* Action = ResolveAction(SoftAction, VerbName))
		{
			EnhancedInput->BindAction(Action, ETriggerEvent::Started, this, PressFunc);
			EnhancedInput->BindAction(Action, ETriggerEvent::Completed, this, ReleaseFunc);
			EnhancedInput->BindAction(Action, ETriggerEvent::Canceled, this, ReleaseFunc);
		}
	};

	BindPressRelease(JumpAction, TEXT("JumpAction"), &ABRPlayerController::OnJumpPressed, &ABRPlayerController::OnJumpReleased);
	BindPressRelease(FireAction, TEXT("FireAction"), &ABRPlayerController::OnFirePressed, &ABRPlayerController::OnFireReleased);
	BindPressRelease(GrenadeAction, TEXT("GrenadeAction"), &ABRPlayerController::OnGrenadePressed, &ABRPlayerController::OnGrenadeReleased);
	BindPressRelease(SprintAction, TEXT("SprintAction"), &ABRPlayerController::OnSprintPressed, &ABRPlayerController::OnSprintReleased);
	BindPress(ReloadAction, TEXT("ReloadAction"), &ABRPlayerController::OnReloadPressed);
	BindPress(SwapAction, TEXT("SwapAction"), &ABRPlayerController::OnSwapPressed);
	BindPress(MeleeAction, TEXT("MeleeAction"), &ABRPlayerController::OnMeleePressed);
	BindPress(GrappleAction, TEXT("GrappleAction"), &ABRPlayerController::OnGrapplePressed);
}

const UInputAction* ABRPlayerController::ResolveAction(const TSoftObjectPtr<UInputAction>& SoftAction, const TCHAR* VerbName)
{
	// ensureAlways, not ensure: seven verbs share this one call site, so a plain ensure would
	// report whichever failed first and stay silent about the other six.
	if (SoftAction.IsNull())
	{
		ensureAlwaysMsgf(false, TEXT("BRPlayerController: %s is unset, so that verb binds nothing and the key is dead. Expected [/Script/Breachpoint.BRPlayerController] %s in Config/DefaultGame.ini, or a value on PC_BR's details panel."),
			VerbName, VerbName);
		return nullptr;
	}

	if (const UInputAction* Resident = SoftAction.Get())
	{
		return Resident;
	}

	const UInputAction* Loaded = SoftAction.LoadSynchronous();
	ensureAlwaysMsgf(Loaded, TEXT("BRPlayerController: %s is set to '%s' but that asset failed to load."),
		VerbName, *SoftAction.ToSoftObjectPath().ToString());
	return Loaded;
}

void ABRPlayerController::SetPawn(APawn* InPawn)
{
	const APawn* PreviousPawn = GetPawn();

	Super::SetPawn(InPawn);

	if (PreviousPawn == InPawn)
	{
		return;
	}

	// The pawn's death takes its input component and every binding with it, so no release
	// event will ever arrive for a key that was down at that moment. The ASC outlives the
	// pawn, so without this a respawned fighter starts holding a key nobody is pressing.
	//
	// SetPawn and not OnUnPossess: OnUnPossess runs on the authority only, while the buffer
	// being flushed is the local client's. A remote client learns its pawn is gone through
	// OnRep_Pawn -> SetPawn and never sees OnUnPossess at all.
	if (UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent())
	{
		ASC->ClearAbilityInput();
	}
}

bool ABRPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void ABRPlayerController::ActivateByInputTag(FGameplayTag InputTag)
{
	if (UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void ABRPlayerController::ReleaseInputTag(FGameplayTag InputTag)
{
	if (UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

void ABRPlayerController::OnJumpPressed()      { ActivateByInputTag(BRGameplayTags::InputTag_Jump); }
void ABRPlayerController::OnJumpReleased()     { ReleaseInputTag(BRGameplayTags::InputTag_Jump); }
void ABRPlayerController::OnFirePressed()     { ActivateByInputTag(BRGameplayTags::InputTag_Fire); }
void ABRPlayerController::OnFireReleased()    { ReleaseInputTag(BRGameplayTags::InputTag_Fire); }
void ABRPlayerController::OnReloadPressed()   { ActivateByInputTag(BRGameplayTags::InputTag_Reload); }
void ABRPlayerController::OnSwapPressed()     { ActivateByInputTag(BRGameplayTags::InputTag_Swap); }
void ABRPlayerController::OnGrenadePressed()  { ActivateByInputTag(BRGameplayTags::InputTag_Grenade); }
void ABRPlayerController::OnGrenadeReleased() { ReleaseInputTag(BRGameplayTags::InputTag_Grenade); }
void ABRPlayerController::OnMeleePressed()    { ActivateByInputTag(BRGameplayTags::InputTag_Melee); }
void ABRPlayerController::OnGrapplePressed()  { ActivateByInputTag(BRGameplayTags::InputTag_Grapple); }
void ABRPlayerController::OnSprintPressed()   { ActivateByInputTag(BRGameplayTags::InputTag_Sprint); }
void ABRPlayerController::OnSprintReleased()  { ReleaseInputTag(BRGameplayTags::InputTag_Sprint); }
