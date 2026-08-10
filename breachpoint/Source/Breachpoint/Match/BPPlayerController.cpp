#include "Match/BPPlayerController.h"

#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "Camera/BRPlayerCameraManager.h"
#include "Core/BRCore.h"
#include "GameFramework/Character.h"
#include "Core/BRGameplayTags.h"
#include "Match/BPPlayerState.h"

ABPPlayerController::ABPPlayerController()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	PlayerCameraManagerClass = ABRPlayerCameraManager::StaticClass();
}

UBRAbilitySystemComponent* ABPPlayerController::GetBRAbilitySystemComponent() const
{
	if (const ABPPlayerState* PS = GetPlayerState<ABPPlayerState>())
	{
		if (UBRAbilitySystemComponent* ASC = PS->GetBRAbilitySystemComponent())
		{
			return ASC;
		}
	}

	if (const APawn* ControlledPawn = GetPawn())
	{
		if (const ABPPlayerState* PawnPS = ControlledPawn->GetPlayerState<ABPPlayerState>())
		{
			return PawnPS->GetBRAbilitySystemComponent();
		}
	}

	return nullptr;
}

void ABPPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		auto AddContext = [this, Subsystem](const TSoftObjectPtr<UInputMappingContext>& SoftContext, TObjectPtr<UInputMappingContext>& OutResolved, const TCHAR* Name)
		{
			OutResolved = nullptr;
			if (SoftContext.IsNull())
			{
				UE_LOG(LogBRAbility, Warning, TEXT("BPPlayerController: '%s' unset. Pin under [/Script/Breachpoint.BPPlayerController] in DefaultGame.ini."), Name);
				return;
			}

			if (UInputMappingContext* Context = SoftContext.LoadSynchronous())
			{
				Subsystem->AddMappingContext(Context, 0);
				OutResolved = Context;
			}
			else
			{
				UE_LOG(LogBRAbility, Error, TEXT("BPPlayerController: '%s' path '%s' failed to load."), Name, *SoftContext.ToSoftObjectPath().ToString());
			}
		};

		AddContext(DefaultMappingContext, ResolvedDefaultMappingContext, TEXT("DefaultMappingContext"));
		AddContext(MouseLookMappingContext, ResolvedMouseLookMappingContext, TEXT("MouseLookMappingContext"));
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogBRAbility, Error, TEXT("BPPlayerController: InputComponent is not EnhancedInput."));
		return;
	}

	// EVERY verb: Started + Completed + Canceled. A press-only bind leaves the ASC held
	// buffer stuck after the first press (same bug BR documents).
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

	Bind(JumpAction, TEXT("JumpAction"), &ABPPlayerController::OnJumpPressed, &ABPPlayerController::OnJumpReleased);
	Bind(FireAction, TEXT("FireAction"), &ABPPlayerController::OnFirePressed, &ABPPlayerController::OnFireReleased);
	Bind(GrenadeAction, TEXT("GrenadeAction"), &ABPPlayerController::OnGrenadePressed, &ABPPlayerController::OnGrenadeReleased);
	Bind(SprintAction, TEXT("SprintAction"), &ABPPlayerController::OnSprintPressed, &ABPPlayerController::OnSprintReleased);
	Bind(ReloadAction, TEXT("ReloadAction"), &ABPPlayerController::OnReloadPressed, &ABPPlayerController::OnReloadReleased);
	Bind(SwapAction, TEXT("SwapAction"), &ABPPlayerController::OnSwapPressed, &ABPPlayerController::OnSwapReleased);
	Bind(MeleeAction, TEXT("MeleeAction"), &ABPPlayerController::OnMeleePressed, &ABPPlayerController::OnMeleeReleased);
	Bind(GrappleAction, TEXT("GrappleAction"), &ABPPlayerController::OnGrapplePressed, &ABPPlayerController::OnGrappleReleased);

	LogKeyCensus(BoundActions);
}

void ABPPlayerController::LogKeyCensus(const TArray<const UInputAction*>& BoundActions) const
{
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

	CountContext(ResolvedDefaultMappingContext);
	CountContext(ResolvedMouseLookMappingContext);

	for (const UInputAction* Action : BoundActions)
	{
		const TArray<FString>* Keys = KeysPerAction.Find(Action);
		if (Keys && Keys->Num() > 0)
		{
			UE_LOG(LogBRAbility, Log, TEXT("BP KEY CENSUS: %s <- %s"),
				*GetNameSafe(Action), *FString::Join(*Keys, TEXT(", ")));
		}
		else
		{
			UE_LOG(LogBRAbility, Error, TEXT("BP KEY CENSUS: %s has NO KEY in any added mapping context. Bound but can never fire."),
				*GetNameSafe(Action));
		}
	}
}

const UInputAction* ABPPlayerController::ResolveAction(const TSoftObjectPtr<UInputAction>& SoftAction, const TCHAR* VerbName)
{
	if (SoftAction.IsNull())
	{
		ensureAlwaysMsgf(false, TEXT("BPPlayerController: %s is unset. Expected [/Script/Breachpoint.BPPlayerController] %s in DefaultGame.ini."),
			VerbName, VerbName);
		return nullptr;
	}

	if (const UInputAction* Resident = SoftAction.Get())
	{
		return Resident;
	}

	const UInputAction* Loaded = SoftAction.LoadSynchronous();
	ensureAlwaysMsgf(Loaded, TEXT("BPPlayerController: %s is set to '%s' but that asset failed to load."),
		VerbName, *SoftAction.ToSoftObjectPath().ToString());
	return Loaded;
}

void ABPPlayerController::SetPawn(APawn* InPawn)
{
	const APawn* PreviousPawn = GetPawn();

	Super::SetPawn(InPawn);

	if (PreviousPawn == InPawn)
	{
		return;
	}

	// Flush held input on pawn change — ASC outlives the pawn; stuck keys survive respawn otherwise.
	if (UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent())
	{
		ASC->ClearAbilityInput();
	}
}

void ABPPlayerController::ActivateByInputTag(FGameplayTag InputTag)
{
	if (UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void ABPPlayerController::ReleaseInputTag(FGameplayTag InputTag)
{
	if (UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

void ABPPlayerController::OnJumpPressed()
{
	ActivateByInputTag(BRGameplayTags::InputTag_Jump);

	// Scaffold fallback: no jump ability granted yet, so drive the pawn directly.
	// Remove when a Jump ability is granted into the BP ASC.
	//
	// ACharacter, not ABPCharacter: ABPFPSCharacter is a sibling reparent target for the
	// template Blueprint, not a subclass, and a narrower cast would leave it unable to jump
	// with no error anywhere -- the input arrives, the tag activates nothing, the cast fails.
	if (ACharacter* Pawn = Cast<ACharacter>(GetPawn()))
	{
		Pawn->Jump();
	}
}

void ABPPlayerController::OnJumpReleased()
{
	ReleaseInputTag(BRGameplayTags::InputTag_Jump);

	if (ACharacter* Pawn = Cast<ACharacter>(GetPawn()))
	{
		Pawn->StopJumping();
	}
}

void ABPPlayerController::OnFirePressed()     { ActivateByInputTag(BRGameplayTags::InputTag_Fire); }
void ABPPlayerController::OnFireReleased()    { ReleaseInputTag(BRGameplayTags::InputTag_Fire); }
void ABPPlayerController::OnReloadPressed()   { ActivateByInputTag(BRGameplayTags::InputTag_Reload); }
void ABPPlayerController::OnReloadReleased()  { ReleaseInputTag(BRGameplayTags::InputTag_Reload); }
void ABPPlayerController::OnSwapPressed()     { ActivateByInputTag(BRGameplayTags::InputTag_Swap); }
void ABPPlayerController::OnSwapReleased()    { ReleaseInputTag(BRGameplayTags::InputTag_Swap); }
void ABPPlayerController::OnGrenadePressed()  { ActivateByInputTag(BRGameplayTags::InputTag_Grenade); }
void ABPPlayerController::OnGrenadeReleased() { ReleaseInputTag(BRGameplayTags::InputTag_Grenade); }
void ABPPlayerController::OnMeleePressed()    { ActivateByInputTag(BRGameplayTags::InputTag_Melee); }
void ABPPlayerController::OnMeleeReleased()   { ReleaseInputTag(BRGameplayTags::InputTag_Melee); }
void ABPPlayerController::OnGrapplePressed()  { ActivateByInputTag(BRGameplayTags::InputTag_Grapple); }
void ABPPlayerController::OnGrappleReleased() { ReleaseInputTag(BRGameplayTags::InputTag_Grapple); }
void ABPPlayerController::OnSprintPressed()   { ActivateByInputTag(BRGameplayTags::InputTag_Sprint); }
void ABPPlayerController::OnSprintReleased()  { ReleaseInputTag(BRGameplayTags::InputTag_Sprint); }

