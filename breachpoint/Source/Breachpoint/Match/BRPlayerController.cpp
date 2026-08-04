#include "Match/BRPlayerController.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Camera/BRPlayerCameraManager.h"
#include "Core/BRCore.h"
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

		for (UInputMappingContext* CurrentContext : AdditionalMappingContexts)
		{
			Subsystem->AddMappingContext(CurrentContext, 0);
		}
	}
	// create the bullet counter widget and add it to the screen
	HUDUI = CreateWidget<UUserWidget>(this, HUDUIClass);

	if (HUDUI)
	{
		HUDUI->AddToPlayerScreen(0);
	}
	
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	// EVERY verb binds Started AND Completed - there is no press-only verb here, and binding one
	// that way was a real bug: AbilityInputTagPressed adds the tag to the ASC's held buffer and
	// ONLY AbilityInputTagReleased removes it. A verb with no release binding sticks in that
	// buffer after its first press and every later press is silently swallowed. That is not a
	// property of WhileInputHeld abilities; it is a property of the buffer, so it applies to all.
	TArray<const UInputAction*> BoundActions;

	auto Bind = [this, EnhancedInput, &BoundActions](const TSoftObjectPtr<UInputAction>& SoftAction, const TCHAR* VerbName, auto PressFunc, auto ReleaseFunc)
	{
		if (const UInputAction* Action = ResolveAction(SoftAction, VerbName))
		{
			EnhancedInput->BindAction(Action, ETriggerEvent::Started, this, PressFunc);
			EnhancedInput->BindAction(Action, ETriggerEvent::Completed, this, ReleaseFunc);
			EnhancedInput->BindAction(Action, ETriggerEvent::Canceled, this, ReleaseFunc);
			BoundActions.Add(Action);
		}
	};

	Bind(JumpAction, TEXT("JumpAction"), &ABRPlayerController::OnJumpPressed, &ABRPlayerController::OnJumpReleased);
	Bind(FireAction, TEXT("FireAction"), &ABRPlayerController::OnFirePressed, &ABRPlayerController::OnFireReleased);
	Bind(GrenadeAction, TEXT("GrenadeAction"), &ABRPlayerController::OnGrenadePressed, &ABRPlayerController::OnGrenadeReleased);
	Bind(SprintAction, TEXT("SprintAction"), &ABRPlayerController::OnSprintPressed, &ABRPlayerController::OnSprintReleased);
	Bind(ReloadAction, TEXT("ReloadAction"), &ABRPlayerController::OnReloadPressed, &ABRPlayerController::OnReloadReleased);
	Bind(SwapAction, TEXT("SwapAction"), &ABRPlayerController::OnSwapPressed, &ABRPlayerController::OnSwapReleased);
	Bind(MeleeAction, TEXT("MeleeAction"), &ABRPlayerController::OnMeleePressed, &ABRPlayerController::OnMeleeReleased);
	Bind(GrappleAction, TEXT("GrappleAction"), &ABRPlayerController::OnGrapplePressed, &ABRPlayerController::OnGrappleReleased);

	LogKeyCensus(BoundActions);
}

void ABRPlayerController::LogKeyCensus(const TArray<const UInputAction*>& BoundActions) const
{
	// THE QUESTION NO OTHER LINE ANSWERS: does the mapping context contain a KEY for the action
	// the controller just bound? A binding with no key succeeds, reports success, and can never
	// fire - indistinguishable from a dead binding from the chair. An IMC row can also exist with
	// its key left as None, which reads identically to a correct row in the asset.
	TMap<const UInputAction*, TArray<FString>> KeysPerAction;

	auto CountContext = [&KeysPerAction](const UInputMappingContext* Context)
	{
		if (!Context)
		{
			return;
		}

		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (const UInputAction* Action = Mapping.Action.Get())
			{
				if (Mapping.Key.IsValid())
				{
					KeysPerAction.FindOrAdd(Action).Add(Mapping.Key.ToString());
				}
			}
		}
	};

	for (const UInputMappingContext* Context : DefaultMappingContexts)
	{
		CountContext(Context);
	}
	for (const UInputMappingContext* Context : AdditionalMappingContexts)
	{
		CountContext(Context);
	}

	for (const UInputAction* Action : BoundActions)
	{
		const TArray<FString>* Keys = KeysPerAction.Find(Action);
		if (Keys && Keys->Num() > 0)
		{
			UE_LOG(LogBRAbility, Log, TEXT("KEY CENSUS: %s <- %s"),
				*GetNameSafe(Action), *FString::Join(*Keys, TEXT(", ")));
		}
		else
		{
			UE_LOG(LogBRAbility, Error, TEXT("KEY CENSUS: %s has NO KEY in any added mapping context. It is bound and can never fire. Add a key for it in IMC_Default."),
				*GetNameSafe(Action));
		}
	}
}

const UInputAction* ABRPlayerController::ResolveAction(const TSoftObjectPtr<UInputAction>& SoftAction, const TCHAR* VerbName)
{
	// ensureAlways, not ensure: every verb shares this one call site, so a plain ensure would
	// report whichever failed first and stay silent about the rest.
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

void ABRPlayerController::OnJumpPressed()     { ActivateByInputTag(BRGameplayTags::InputTag_Jump); }
void ABRPlayerController::OnJumpReleased()    { ReleaseInputTag(BRGameplayTags::InputTag_Jump); }
void ABRPlayerController::OnFirePressed()     { ActivateByInputTag(BRGameplayTags::InputTag_Fire); }
void ABRPlayerController::OnFireReleased()    { ReleaseInputTag(BRGameplayTags::InputTag_Fire); }
void ABRPlayerController::OnReloadPressed()   { ActivateByInputTag(BRGameplayTags::InputTag_Reload); }
void ABRPlayerController::OnReloadReleased()  { ReleaseInputTag(BRGameplayTags::InputTag_Reload); }
void ABRPlayerController::OnSwapPressed()     { ActivateByInputTag(BRGameplayTags::InputTag_Swap); }
void ABRPlayerController::OnSwapReleased()    { ReleaseInputTag(BRGameplayTags::InputTag_Swap); }
void ABRPlayerController::OnGrenadePressed()  { ActivateByInputTag(BRGameplayTags::InputTag_Grenade); }
void ABRPlayerController::OnGrenadeReleased() { ReleaseInputTag(BRGameplayTags::InputTag_Grenade); }
void ABRPlayerController::OnMeleePressed()    { ActivateByInputTag(BRGameplayTags::InputTag_Melee); }
void ABRPlayerController::OnMeleeReleased()   { ReleaseInputTag(BRGameplayTags::InputTag_Melee); }
void ABRPlayerController::OnGrapplePressed()  { ActivateByInputTag(BRGameplayTags::InputTag_Grapple); }
void ABRPlayerController::OnGrappleReleased() { ReleaseInputTag(BRGameplayTags::InputTag_Grapple); }
void ABRPlayerController::OnSprintPressed()   { ActivateByInputTag(BRGameplayTags::InputTag_Sprint); }
void ABRPlayerController::OnSprintReleased()  { ReleaseInputTag(BRGameplayTags::InputTag_Sprint); }
