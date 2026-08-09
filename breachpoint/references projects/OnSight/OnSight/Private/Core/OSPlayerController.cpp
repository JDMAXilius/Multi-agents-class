#include "Core/OSPlayerController.h"
#include "Core/OSPlayerState.h"
#include "Characters/Player/OSPlayer.h"
#include "Components/OSCameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GAS/Components/OSAbilitySystemComponent.h"
#include "Data/OSGameplayTags.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "UI/OSHUDWidget.h"
#include "AkComponent.h"
#include "MovieSceneObjectBindingID.h"
#include "UI/Layers/OSPauseLayerWidget.h"
#include "UI/Layers/OSScoreboardLayerWidget.h"
#include "Subsystems/OSSessionsSubsystem.h"
#include "Core/OSGameInstance.h"

#include "Characters/OSPunchingBag.h"

#if !UE_BUILD_SHIPPING
#include "Core/OSCheatManager.h"
#include "Core/OSDebugGlobals.h"
#include "Core/OSGameMode.h"
#include "Characters/OSCharacter.h"
#include "GameplayEffect.h"
#include "OSLogCategories.h"
#endif


AOSPlayerController::AOSPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CameraComponent = CreateDefaultSubobject<UOSCameraComponent>(TEXT("CameraComponent"));
#if !UE_BUILD_SHIPPING
	CheatClass = UOSCheatManager::StaticClass();
#endif
}

UAbilitySystemComponent* AOSPlayerController::GetAbilitySystemComponent() const
{
	// Multiplayer GAS best practice: for players the ASC lives on PlayerState.
	if (AOSPlayerState* PS = GetPlayerState<AOSPlayerState>())
	{
		if (UAbilitySystemComponent* PSASC = PS->GetAbilitySystemComponent())
		{
			return PSASC;
		}
	}

	// Fallback: try the currently possessed pawn (covers edge cases / transitions).
	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn());
}

AOSPlayer* AOSPlayerController::GetOSPlayer() const
{
	if (_player) return _player;
	auto pawn = GetPawn();
	if (!pawn) return nullptr;
	_player = Cast<AOSPlayer>(pawn);
	return _player;
}

//void AOSPlayerController::Server_SubmitDisplayName_Implementation( const FString& InName )
//{
//	AOSPlayerState* PS = GetPlayerState<AOSPlayerState>();
//	if (!PS) return;
//	
//	FString Clean = InName;
//	Clean.TrimStartAndEndInline();
//	Clean.ReplaceInline(TEXT("\n"), TEXT(" "));
//	Clean.ReplaceInline(TEXT("\r"), TEXT(" "));
//
//	if (Clean.IsEmpty())
//		Clean = PS->GetPlayerName();
//
//	PS->SetDisplayName_Server(Clean);
//}

void AOSPlayerController::SwapToMappingContext( UInputMappingContext* OriginalContext, UInputMappingContext* NewContext,
	int32 Priority )
{
	auto LP = GetLocalPlayer();
	auto Subsys = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Subsys) return;
	
	if (OriginalContext) Subsys->RemoveMappingContext(OriginalContext);
	if (NewContext) Subsys->AddMappingContext(NewContext, Priority);
}

void AOSPlayerController::ToggleBetweenMappingContexts( UInputMappingContext* ContextA, UInputMappingContext* ContextB,
	bool bFromAToB, int32 Priority )
{
	if (bFromAToB)
		SwapToMappingContext(ContextA, ContextB, Priority);
	else
		SwapToMappingContext(ContextB, ContextA, Priority);
}

void AOSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		UE_LOG(LogTemp, Log, TEXT("[OSPlayerController] BeginPlay: forcing InputModeGameOnly (recovers from menu UIOnly state carried over on LocalPlayer)"));
		FInputModeGameOnly Mode;
		SetInputMode(Mode);
		bShowMouseCursor = false;
	}
}

void AOSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		return;
	}

	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Started, this, &AOSPlayerController::OnMoveStarted);
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AOSPlayerController::Move);
	}
	if (LookAction)
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AOSPlayerController::Look);
	}
	if (SprintAction)
	{
		EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &AOSPlayerController::OnSprint);
	}
	if (LightAttackAction)
	{
		EIC->BindAction(LightAttackAction, ETriggerEvent::Started, this, &AOSPlayerController::OnLightAttack);
	}
	if (HeavyAttackAction)
	{
		EIC->BindAction(HeavyAttackAction, ETriggerEvent::Started, this, &AOSPlayerController::OnHeavyAttack);
	}
	if (BlockAction)
	{
		EIC->BindAction(BlockAction, ETriggerEvent::Started, this, &AOSPlayerController::OnBlockStarted);
		EIC->BindAction(BlockAction, ETriggerEvent::Completed, this, &AOSPlayerController::OnBlockCompleted);
		EIC->BindAction(BlockAction, ETriggerEvent::Canceled, this, &AOSPlayerController::OnBlockCompleted);
	}
	if (DodgeAction)
	{
		EIC->BindAction(DodgeAction, ETriggerEvent::Started, this, &AOSPlayerController::OnDodge);
	}
	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &AOSPlayerController::OnJump);
	}
	if (TargetLockAction)
	{
		EIC->BindAction(TargetLockAction, ETriggerEvent::Started, this, &AOSPlayerController::OnTargetLock);
	}
	if (Ability01)
	{
		EIC->BindAction(Ability01, ETriggerEvent::Started, this, &AOSPlayerController::OnAbility01);
	}
	if (Ability02)
	{
		EIC->BindAction(Ability02, ETriggerEvent::Started, this, &AOSPlayerController::OnAbility02);
	}
	if (Ability03)
	{
		EIC->BindAction(Ability03, ETriggerEvent::Started, this, &AOSPlayerController::OnAbility03);
	}
	if (Ability04)
	{
		EIC->BindAction(Ability04, ETriggerEvent::Started, this, &AOSPlayerController::OnAbility04);
	}
	if (TargetCycleUpAction)
	{
		EIC->BindAction(TargetCycleUpAction, ETriggerEvent::Triggered, this, &AOSPlayerController::OnTargetCycleUp);
	}
	if (TargetCycleDownAction)
	{
		EIC->BindAction(TargetCycleDownAction, ETriggerEvent::Triggered, this, &AOSPlayerController::OnTargetCycleDown);
	}
	if (PrimaryUtilityAction)
	{
		EIC->BindAction(PrimaryUtilityAction, ETriggerEvent::Started, this, &AOSPlayerController::OnPrimaryUtility);
	}
	if (SecondaryUtilityAction)
	{
		EIC->BindAction(SecondaryUtilityAction, ETriggerEvent::Started, this, &AOSPlayerController::OnSecondaryUtility);
	}
	if (WildcardUtilityAction)
	{
		EIC->BindAction(WildcardUtilityAction, ETriggerEvent::Started, this, &AOSPlayerController::OnWildcardUtility);
	}
	if (ScoreboardAction)
	{
		EIC->BindAction(ScoreboardAction, ETriggerEvent::Started, this, &AOSPlayerController::OnScoreboardPressed);
		EIC->BindAction(ScoreboardAction, ETriggerEvent::Completed, this, &AOSPlayerController::OnScoreboardReleased);
	}
	if (PauseAction)
	{
		EIC->BindAction(PauseAction, ETriggerEvent::Started, this, &AOSPlayerController::OnPauseMenuPressed);
	}
	if (ChargedAttackAction)
	{
		EIC->BindAction(ChargedAttackAction, ETriggerEvent::Started, this, &AOSPlayerController::OnChargedAttackPressed);
		EIC->BindAction(ChargedAttackAction, ETriggerEvent::Completed, this, &AOSPlayerController::OnChargedAttackReleased);
		EIC->BindAction(ChargedAttackAction, ETriggerEvent::Canceled, this, &AOSPlayerController::OnChargedAttackReleased);
	}
	if (GrabAction)
	{
		EIC->BindAction(GrabAction, ETriggerEvent::Started, this, &AOSPlayerController::OnGrab);
	}

#if !UE_BUILD_SHIPPING
	if (DebugMenuAction)
	{
		EIC->BindAction(DebugMenuAction, ETriggerEvent::Started, this, &AOSPlayerController::OnDebugMenu);
	}
	else
	{
		UE_LOG(LogOSDebug, Warning, TEXT("DebugMenuAction not assigned — debug menu toggle disabled. Assign IA_DebugMenu in Blueprint."));
	}
#endif
}

void AOSPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	// PlayerState ASC + loadout: AOSCharacter::InitializeGAS (PossessedBy / OnRep_PlayerState).
	if (HasAuthority() && InPawn && HUDWidget)
	{
		HUDWidget->NotifyGASReady();
	}

	// Create HUD widget if it doesn't exist, or rebind if it already exists (respawn case)
	if (HUDWidget && IsValid(HUDWidget))
	{
		// Widget already exists (from previous spawn), just rebind to new pawn's attributes
		//HUDWidget->BindToAttributeSet();
	}
	else
	{
		// Widget doesn't exist yet, create it
		CreateHUDWidget();
	}

	// OSCameraComponent caches pawn/SpringArm/Camera in Tick when pawn changes; no manual call needed.
}

void AOSPlayerController::OnUnPossess()
{
	_player = nullptr; // Clear cached pawn so GetOSPlayer() re-fetches after respawn
	Super::OnUnPossess();
}

void AOSPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// PlayerState may arrive after HUD construction on clients; bind once it's ready.
	if (HUDWidget && IsLocalController())
	{
		BindHUD();
	}
}

void AOSPlayerController::BeginPlayingState()
{
	Super::BeginPlayingState();
	CreateHUDWidget();
	AOSPlayerState* OSPState = GetPlayerState<AOSPlayerState>();
	if (!OSPState) return;
	OSPState->OnRep_Stats();
}


//void AOSPlayerController::ReceivedPlayer()
//{
//	auto instance = GetGameInstance();
//	if (!instance) return;
//	
//	Super::ReceivedPlayer();
//	if (!IsLocalController()) return;
//	
//
//	auto subsystem = instance->GetSubsystem<UOSSessionsSubsystem>();
//	if (!subsystem) return;
//	
//	const FString name = subsystem->GetSteamPlayerName();
//	if (!name.IsEmpty())
//	{
//		Server_SubmitDisplayName(name);
//	}
//
//}

void AOSPlayerController::CreateHUDWidget()
{
	if (!IsLocalController())
	{
		return;
	}

	if (HUDWidget && IsValid(HUDWidget))
	{
		return;
	}

	if (HUDWidget && !IsValid(HUDWidget))
	{
		HUDWidget = nullptr;
	}

	if (!HUDWidgetClass)
	{
		return;
	}

	HUDWidget = CreateWidget<UOSHUDWidget>(this, HUDWidgetClass);
	if (HUDWidget && IsValid(HUDWidget))
	{
		HUDWidget->AddToViewport();
		BindHUD();
	}
	else
	{
		HUDWidget = nullptr;
	}
}

void AOSPlayerController::RemoveHUDWidget()
{
	if (HUDWidget && IsValid(HUDWidget))
	{
		HUDWidget->RemoveFromParent();
	}
	HUDWidget = nullptr;
}

// ========================================
// INPUT HANDLERS (GAS: input -> tag -> TryActivateAbilitiesByTag)
// ========================================

void AOSPlayerController::BindHUD()
{
	if (!HUDWidget) return;
	
	// HUDWidget->BindToPlayerState();   
	// HUDWidget->BindToGameState();
	// HUDWidget->BindToAttributeSet();   
	// HUDWidget->RefreshModeLayers();
}

void AOSPlayerController::OnMoveStarted(const FInputActionValue& Value)
{
	if (!GetPawn() || !IsAlive())
	{
		return;
	}

	// Cancel attacks once when movement begins (instead of spamming CancelAbilities every frame in Move()).
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		const FOSGameplayTags& GameplayTags = FOSGameplayTags::Get();
		if (ASC->HasMatchingGameplayTag(GameplayTags.IsAttacking) && ASC->HasMatchingGameplayTag(GameplayTags.CanCancel_Movement))
		{
			const bool bComboQueued = ASC->HasMatchingGameplayTag(GameplayTags.State_Attack_ComboBuffered);
			if (!bComboQueued)
			{
				FGameplayTagContainer CancelTags;
				if (GameplayTags.Attack.IsValid()) CancelTags.AddTag(GameplayTags.Attack);
				if (GameplayTags.Ability_ComboAttack.IsValid()) CancelTags.AddTag(GameplayTags.Ability_ComboAttack);
				if (GameplayTags.Ability_ComboAttackHeavy.IsValid()) CancelTags.AddTag(GameplayTags.Ability_ComboAttackHeavy);
				if (GameplayTags.Ability_ChargedAttack.IsValid()) CancelTags.AddTag(GameplayTags.Ability_ChargedAttack);
				if (!CancelTags.IsEmpty())
				{
					ASC->CancelAbilities(&CancelTags, nullptr, nullptr);
				}
			}
		}
	}
}

void AOSPlayerController::Move(const FInputActionValue& Value)
{
	if (!GetPawn() || !IsAlive())
	{
		return;
	}

	const FVector2D MovementVector = Value.Get<FVector2D>();
	const FRotator YawRotation(0.0f, GetControlRotation().Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right   = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	GetPawn()->AddMovementInput(Forward, MovementVector.Y);
	GetPawn()->AddMovementInput(Right,   MovementVector.X);
}

void AOSPlayerController::Look(const FInputActionValue& Value)
{
	AOSPlayer* OSPlayer = GetOSPlayer();
	if (!OSPlayer) return;
	
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	AddYawInput(LookAxisVector.X);
	AddPitchInput(LookAxisVector.Y);
}

void AOSPlayerController::OnSprint()
{
	AOSPlayer* OSPlayer = GetOSPlayer();
	if (!OSPlayer) return;
	
	if (UOSAbilitySystemComponent* ASC = Cast<UOSAbilitySystemComponent>(OSPlayer->GetAbilitySystemComponent()))
	{
		const FOSGameplayTags& GameplayTags = FOSGameplayTags::Get();
		FGameplayTagContainer SprintTagContainer;
		SprintTagContainer.AddTag(GameplayTags.Ability_Sprint);
		
		TArray<FGameplayAbilitySpec*> MatchingAbilities;
		ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(SprintTagContainer, MatchingAbilities);
		
		bool bSprintActive = false;
		for (FGameplayAbilitySpec* Spec : MatchingAbilities)
		{
			if (Spec && Spec->IsActive())
			{
				bSprintActive = true;
				break;
			}
		}
		
		if (bSprintActive)
		{
			ASC->CancelAbilitiesWithTags(SprintTagContainer);
		}
		else
		{
			ASC->TryActivateAbilitiesByTag(SprintTagContainer);
		}
	}
}

void AOSPlayerController::OnLightAttack()
{
	HandleComboInput(FOSGameplayTags::Get().Attack_Light);
}

void AOSPlayerController::OnHeavyAttack()
{
	if (!IsAlive()) return;
	HandleComboInput(FOSGameplayTags::Get().Attack_Heavy);
}

void AOSPlayerController::OnBlockStarted()
{
	AOSPlayer* OSPlayer = GetOSPlayer();
	if (!OSPlayer) return;
	
	if (UOSAbilitySystemComponent* ASC = Cast<UOSAbilitySystemComponent>(OSPlayer->GetAbilitySystemComponent()))
	{
		const FOSGameplayTags& GameplayTags = FOSGameplayTags::Get();
		FGameplayTagContainer BlockTagContainer;
		BlockTagContainer.AddTag(GameplayTags.Ability_Block);
		ASC->TryActivateAbilitiesByTag(BlockTagContainer);
	}
}

void AOSPlayerController::OnBlockCompleted()
{
	AOSPlayer* OSPlayer = GetOSPlayer();
	if (!OSPlayer) return;
	
	const FOSGameplayTags& GameplayTags = FOSGameplayTags::Get();
	FGameplayTagContainer TagContainer;
	TagContainer.AddTag(GameplayTags.Ability_Block);
	
	if (UOSAbilitySystemComponent* ASC = Cast<UOSAbilitySystemComponent>(OSPlayer->GetAbilitySystemComponent()))
	{
		ASC->CancelAbilitiesWithTags(TagContainer);
	}
}

void AOSPlayerController::OnDodge()
{
	AOSPlayer* OSPlayer = GetOSPlayer();
	if (!OSPlayer) return;
	
	if (UOSAbilitySystemComponent* ASC = Cast<UOSAbilitySystemComponent>(OSPlayer->GetAbilitySystemComponent()))
	{
		const FOSGameplayTags& GameplayTags = FOSGameplayTags::Get();
		FGameplayTagContainer DodgeTagContainer;
		DodgeTagContainer.AddTag(GameplayTags.Ability_Dodge);
		ASC->TryActivateAbilitiesByTag(DodgeTagContainer);
	}
}


void AOSPlayerController::OnJump()
{
	AOSPlayer* OSPlayer = GetOSPlayer();
	if (!OSPlayer) return;
	
	if (UOSAbilitySystemComponent* ASC = Cast<UOSAbilitySystemComponent>(OSPlayer->GetAbilitySystemComponent()))
	{
		const FOSGameplayTags& GameplayTags = FOSGameplayTags::Get();

		// If we're attacking, only allow "jump cancel" when a montage-authored cancel window grants permission.
		// (Jump ability may be Blueprint; this ensures cancel works even without BP tag wiring.)
		
		//if (ASC->HasMatchingGameplayTag(GameplayTags.IsAttacking))
		//{
		//	if (!ASC->HasMatchingGameplayTag(GameplayTags.CanCancel_Jump))
		//	{
		//		return;
		//	}
		//	FGameplayTagContainer CancelTags;
		//	CancelTags.AddTag(GameplayTags.Ability_ComboAttack);
		//	CancelTags.AddTag(GameplayTags.Ability_ComboAttackHeavy);
		//	CancelTags.AddTag(GameplayTags.Ability_ChargedAttack);
		//	CancelTags.AddTag(GameplayTags.Attack_Light);
		//	CancelTags.AddTag(GameplayTags.Attack_Heavy);
		//	ASC->CancelAbilities(&CancelTags, nullptr, nullptr);
		//}
		
		// Prefer mantle on jump input if it can activate; otherwise fall back to jump.
		// TryActivateAbilitiesByTag returns true if the ability was ACTIVATED — even if
		// ActivateAbility immediately calls EndAbility (e.g., fresh re-trace fails).
		// Without the IsMantling check, a stale CanActivateAbility cache causes mantle
		// to "succeed" (swallowing the jump input) then immediately end with no effect.
		{
			FGameplayTagContainer MantleTagContainer;
			MantleTagContainer.AddTag(GameplayTags.Ability_Mantle);
			if (ASC->TryActivateAbilitiesByTag(MantleTagContainer)
				&& ASC->HasMatchingGameplayTag(GameplayTags.IsMantling))
			{
				return;
			}
		}
		
		FGameplayTagContainer JumpTagContainer;
		JumpTagContainer.AddTag(GameplayTags.Ability_Jump);
		ASC->TryActivateAbilitiesByTag(JumpTagContainer);
	}
}

void AOSPlayerController::OnTargetLock()
{
	AOSPlayer* OSPlayer = GetOSPlayer();
	if (!OSPlayer) return;
	// LockOnComponent removed. If needed, implement lock-on as a GAS ability/state.
}

void AOSPlayerController::OnAbility01()
{
	ActivateAbility(FOSGameplayTags::Get().Ability_Slot1);
}

void AOSPlayerController::OnAbility02()
{
	ActivateAbility(FOSGameplayTags::Get().Ability_Slot2);
}

void AOSPlayerController::OnAbility03()
{
	ActivateAbility(FOSGameplayTags::Get().Ability_Slot3);
}

void AOSPlayerController::OnAbility04()
{
	ActivateAbility(FOSGameplayTags::Get().Ability_Slot4);
}

void AOSPlayerController::ActivateAbility(const FGameplayTag& AbilityTag) const
{
	AOSPlayer* OSPlayer = GetOSPlayer();
	if (!OSPlayer) return;

	if (UOSAbilitySystemComponent* ASC = Cast<UOSAbilitySystemComponent>(OSPlayer->GetAbilitySystemComponent()))
	{
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(AbilityTag);
		ASC->TryActivateAbilitiesByTag(TagContainer);
	}
}

void AOSPlayerController::HandleComboInput(const FGameplayTag& AbilityTag)
{
	AOSPlayer* OSPlayer = GetOSPlayer();
	if (!OSPlayer) return;

	UOSAbilitySystemComponent* ASC = Cast<UOSAbilitySystemComponent>(OSPlayer->GetAbilitySystemComponent());
	if (!IsValid(ASC)) return;

	const FOSGameplayTags& OSTags = FOSGameplayTags::Get();

	// 1. ALWAYS send combo input event — feeds the active ability's WaitGameplayEvent listener.
	//    EventTag carries the requested attack type so the ability can detect cross-type switches.
	//    If no ability is active, event goes into the void (harmless).
	if (OSTags.Event_ComboInputPressed.IsValid())
	{
		FGameplayEventData EventData;
		
		// LOOK AT THIS: Instigator and Target are both set to the player controller's pawn is redundant
		// This is a bit hacky but allows WaitGameplayEvent in the ASC to receive the event without needing special casing for PlayerControllers. 
		// The active ability can still read the requested attack type from InstigatorTags.
		{
		EventData.Instigator = OSPlayer;
		EventData.Target = OSPlayer;
		}
		// Pass requested ability type in InstigatorTags — NOT EventTag.
		// WaitGameplayEvent overwrites Payload.EventTag with the routing tag
		// (GameplayEvent.ComboInputPressed), destroying any custom value we set.
		EventData.InstigatorTags.AddTag(AbilityTag);
		ASC->HandleGameplayEvent(OSTags.Event_ComboInputPressed, &EventData);

		// Server RPC: feeds the server's authoritative ability instance.
		// Skip on authority (listen server host) — local delivery already reached the ASC.
		if (!HasAuthority())
		{
			ServerComboInputPressed(AbilityTag);
		}
	}

	// 2. Activate if NOT already attacking. If a cross-type switch just caused the active ability
	//    to EndAbility (via OnComboInputReceived detecting Recovery + different type), IsAttacking
	//    is already removed — TryActivate picks up the new type cleanly.
	if (!ASC->HasMatchingGameplayTag(OSTags.IsAttacking))
	{
		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(AbilityTag);
		ASC->TryActivateAbilitiesByTag(TagContainer);
	}
}

void AOSPlayerController::CancelAbilitiesWithTag(const FGameplayTag& AbilityTag) const
{
	AOSPlayer* OSPlayer = GetOSPlayer();
	if (!OSPlayer) return;
	
	FGameplayTagContainer TagContainer;
	TagContainer.AddTag(AbilityTag);
	
	if (UOSAbilitySystemComponent* ASC = Cast<UOSAbilitySystemComponent>(OSPlayer->GetAbilitySystemComponent()))
	{
		ASC->CancelAbilitiesWithTags(TagContainer);
	}
}

bool AOSPlayerController::IsAlive() const
{
	AOSPlayer* OSPlayerPawn = GetOSPlayer();
	return OSPlayerPawn && !OSPlayerPawn->IsDead();
}

void AOSPlayerController::OnTargetCycleUp()
{
	AOSPlayer* OSPlayer = GetOSPlayer();
	if (!OSPlayer) return;
	// LockOnComponent removed.
}

void AOSPlayerController::OnTargetCycleDown()
{
	AOSPlayer* OSPlayer = GetOSPlayer();
	if (!OSPlayer) return;
	// LockOnComponent removed.
}

void AOSPlayerController::OnPrimaryUtility()
{
	ActivateAbility(FOSGameplayTags::Get().Ability_Magic_FireCone);
}

void AOSPlayerController::OnSecondaryUtility()
{
	ActivateAbility(FOSGameplayTags::Get().Ability_Magic_FrostBolt);
}

void AOSPlayerController::OnWildcardUtility()
{
	AOSPlayer* OSPlayer = GetOSPlayer();
	if (!OSPlayer) return;
	// MagicComponent removed. Utility slots should be implemented as GAS abilities.
}

void AOSPlayerController::OnScoreboardPressed()
{
	if (!HUDWidget) return;
	
	HUDWidget->ShowScoreboard();
}

void AOSPlayerController::OnScoreboardReleased()
{
	if (!HUDWidget) return;
	
	HUDWidget->HideScoreboard();
}

void AOSPlayerController::OnPauseMenuPressed()
{
	if (!HUDWidget) return;

	//const auto Root = HUDWidget->FindLayerClassByBase(UOSPauseLayerWidget::StaticClass());

	// close domain
	CloseMenuDomain();
	OpenMenuDomain();
}

void AOSPlayerController::OpenMenuDomain()
{
	if (!HUDWidget) return;
	
	HUDWidget->ShowPause();
	
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

	SetInputMode(Mode);
	bShowMouseCursor = true;

	ToggleBetweenMappingContexts(DefaultMappingContext, UIMappingContext, true);
}

void AOSPlayerController::CloseMenuDomain()
{
	if (!HUDWidget) return;

	HUDWidget->HidePause();

	FInputModeGameOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = false;

	ToggleBetweenMappingContexts(DefaultMappingContext, UIMappingContext, false);
}

void AOSPlayerController::ServerComboInputPressed_Implementation(const FGameplayTag& RequestedType)
{
	// Server: receive combo input from client and broadcast so server's combo ability WaitGameplayEvent fires.
	// RequestedType lets the server's ability detect cross-type switches (light→heavy, heavy→light).
	APawn* ControlledPawn = GetPawn();
	if (!IsValid(ControlledPawn)) return;
	const FOSGameplayTags& GameplayTags = FOSGameplayTags::Get();
	if (!GameplayTags.Event_ComboInputPressed.IsValid()) return;
	FGameplayEventData EventData;
	EventData.Instigator = ControlledPawn;
	EventData.Target = ControlledPawn;
	EventData.InstigatorTags.AddTag(RequestedType);
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->HandleGameplayEvent(GameplayTags.Event_ComboInputPressed, &EventData);
	}
}

void AOSPlayerController::OnChargedAttackPressed()
{
	if (!IsAlive()) return;
	
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!IsValid(ASC))
	{
		return;
	}

	// Try to activate charged attack (will start charging)
	const FOSGameplayTags& GameplayTags = FOSGameplayTags::Get();
	FGameplayTagContainer TagContainer;
	TagContainer.AddTag(GameplayTags.Ability_ChargedAttack);
	ASC->TryActivateAbilitiesByTag(TagContainer);
}

void AOSPlayerController::OnChargedAttackReleased()
{
	if (!IsValid(GetPawn())) return;
	
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!IsValid(ASC))
	{
		return;
	}

	// Release input on charged attack abilities
	const FOSGameplayTags& GameplayTags = FOSGameplayTags::Get();
	const TArray<FGameplayAbilitySpec>& AbilitySpecs = ASC->GetActivatableAbilities();
	
	for (const FGameplayAbilitySpec& Spec : AbilitySpecs)
	{
		if (Spec.Ability && Spec.Ability->GetAssetTags().HasTag(GameplayTags.Ability_ChargedAttack))
		{
			if (Spec.IsActive())
			{
				if (FGameplayAbilitySpec* NonConstSpec = ASC->FindAbilitySpecFromHandle(Spec.Handle))
				{
					ASC->AbilitySpecInputReleased(*NonConstSpec);
				}
			}
		}
	}
}

void AOSPlayerController::OnGrab()
{
	if (!IsAlive()) return;
	ActivateAbility(FOSGameplayTags::Get().Ability_Grab);
}

void AOSPlayerController::OnDebugMenu()
{
#if !UE_BUILD_SHIPPING
	// In packaged builds, CheatManager isn't auto-created (unlike editor).
	// EnableCheats instantiates it for both host and client.
	if (!CheatManager)
	{
		EnableCheats();
	}
	if (UOSCheatManager* CM = Cast<UOSCheatManager>(CheatManager))
	{
		CM->ToggleDebugMenu();
	}
#endif
}

// Debug RPC validation: shipping builds reject the call entirely (these are dev-only commands
// and should not be reachable from production clients — any attempt is treated as malicious).
// Dev builds let the call through; the implementation already has its own null/range guards.
bool AOSPlayerController::Server_ApplyDebugEffect_Validate(TSubclassOf<UGameplayEffect> EffectClass, bool bEnable)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return true;
#endif
}

void AOSPlayerController::Server_ApplyDebugEffect_Implementation(TSubclassOf<UGameplayEffect> EffectClass, bool bEnable)
{
#if !UE_BUILD_SHIPPING
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || !EffectClass) return;

	if (bEnable)
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = EffectClass;
		if (ASC->GetActiveEffects(Query).Num() > 0) return;

		FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
		Ctx.AddSourceObject(ASC->GetOwner());
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, Ctx);
		if (Spec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
	else
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = EffectClass;
		ASC->RemoveActiveEffects(Query);
	}
#endif
}

// See Server_ApplyDebugEffect_Validate header comment. Dev builds also sanity-bound Index to a
// small range so a malicious client can't pass INT32_MIN and underflow the clamp arithmetic.
bool AOSPlayerController::Server_SwapCharacter_Validate(int32 Index)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return Index >= 0 && Index <= 100;
#endif
}

void AOSPlayerController::Server_SwapCharacter_Implementation(int32 Index)
{
#if !UE_BUILD_SHIPPING
	AOSPlayerState* PS = GetPlayerState<AOSPlayerState>();
	if (!PS) return;

	const EOSCharacterType NewType = static_cast<EOSCharacterType>(FMath::Clamp(Index - 1, 0, 2));
	if (PS->GetSelectedCharacterType() == NewType) return;

	PS->SetSelectedCharacterType(NewType);

	APawn* PreviousPawn = GetPawn();
	const FVector PrevLocation = IsValid(PreviousPawn) ? PreviousPawn->GetActorLocation() : FVector::ZeroVector;
	const FRotator PrevRotation = IsValid(PreviousPawn) ? PreviousPawn->GetActorRotation() : FRotator::ZeroRotator;
	const FRotator PrevCameraRotation = GetControlRotation();

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->CancelAllAbilities();
	}

	/*if (AOSCharacter* OldChar = Cast<AOSCharacter>(PreviousPawn))
	{
		OldChar->ResetAttributes();
	}*/

	if (IsValid(PreviousPawn))
	{
		PreviousPawn->DetachFromControllerPendingDestroy();
		PreviousPawn->Destroy();
	}

	if (UWorld* World = GetWorld())
	{
		if (AOSGameMode* GM = Cast<AOSGameMode>(World->GetAuthGameMode()))
		{
			GM->RestartPlayer(this);
			GM->OnPlayerStateReady.Broadcast(PS);

			if (APawn* NewPawn = GetPawn())
			{
				NewPawn->SetActorLocationAndRotation(PrevLocation, PrevRotation);
				SetControlRotation(PrevCameraRotation);
			}
		}
	}

	if (UOSCheatManager* CM = Cast<UOSCheatManager>(CheatManager))
	{
		CM->ReapplyDebugState();
	}
#endif
}

// See Server_ApplyDebugEffect_Validate header comment. Dev builds additionally reject NaN / inf
// and extreme magnitudes on SpawnLoc/SpawnRot so a crafted RPC can't spawn bags at impossible
// positions that would trip engine asserts downstream.
bool AOSPlayerController::Server_TogglePunchingBag_Validate(bool bEnable, const FVector& SpawnLoc, const FRotator& SpawnRot, bool bBagEnemyTeam)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (SpawnLoc.ContainsNaN() || SpawnRot.ContainsNaN())
	{
		return false;
	}
	// World bounds sanity: UE's default WORLD_MAX is ~2e6 cm. Reject anything beyond that.
	constexpr float kMaxWorldCoord = 2'000'000.f;
	if (FMath::Abs(SpawnLoc.X) > kMaxWorldCoord ||
		FMath::Abs(SpawnLoc.Y) > kMaxWorldCoord ||
		FMath::Abs(SpawnLoc.Z) > kMaxWorldCoord)
	{
		return false;
	}
	return true;
#endif
}

void AOSPlayerController::Server_TogglePunchingBag_Implementation(bool bEnable, const FVector& SpawnLoc, const FRotator& SpawnRot,
	bool bBagEnemyTeam)
{
#if !UE_BUILD_SHIPPING
	UOSCheatManager* CM = Cast<UOSCheatManager>(this->CheatManager);
	if (!CM) return;

	if (bEnable)
	{
		// Prefer the BP class configured on the CheatManager (authored mesh + AnimBP + collision).
		// Fall back to the raw C++ class if the soft reference can't resolve — keeps the cheat
		// menu functional even if the BP is missing or moved.
		UClass* BagClass = CM->PunchingBagClass.LoadSynchronous();
		if (!BagClass)
		{
			BagClass = AOSPunchingBag::StaticClass();
		}
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (AActor* Spawned = GetWorld()->SpawnActor(BagClass, &SpawnLoc, &SpawnRot, SpawnParams))
		{
			CM->PunchingBagActor = Spawned;
			if (AOSPunchingBag* Bag = Cast<AOSPunchingBag>(Spawned))
			{
				Bag->SetDebugTdmTeamFromSpawner(this, bBagEnemyTeam);
			}
			Client_SetDebugPunchingBag(Spawned);
		}
	}
	else
	{
		// Destroy before nulling the client-mirrored ref. On a listen-server host the owning
		// client is local, so Client_ RPCs execute inline — calling the RPC first would null
		// our own CheatManager ref and the IsValid() check below would skip Destroy(), leaking
		// the bag actor and flipping the button state.
		if (CM->PunchingBagActor.IsValid())
		{
			CM->PunchingBagActor->Destroy();
		}
		CM->PunchingBagActor = nullptr;
		Client_SetDebugPunchingBag(nullptr);
	}
#endif
}

void AOSPlayerController::Client_SetDebugPunchingBag_Implementation(AActor* Bag)
{
#if !UE_BUILD_SHIPPING
	if (UOSCheatManager* CM = Cast<UOSCheatManager>(CheatManager))
	{
		CM->PunchingBagActor = Bag;
	}
#endif
}

void AOSPlayerController::ClientReturnToMainMenuWithTextReason_Implementation(const FText& ReturnReason)
{
	UOSSessionsSubsystem* SessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOSSessionsSubsystem>()
		: nullptr;

	if (!SessionsSubsystem)
	{
		Super::ClientReturnToMainMenuWithTextReason_Implementation(ReturnReason);
		return;
	}

	if (IsLocalController() && HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("[OSPlayerController] ReturnToMainMenu — host path: destroying session before travel"));
		// Host: wait for session destroy to complete before traveling so clients
		// receive a clean disconnect rather than a mid-session drop.
		SessionsSubsystem->OnDestroySessionComplete.RemoveAll(this);
		SessionsSubsystem->OnDestroySessionComplete.AddDynamic(this, &AOSPlayerController::OnHostReadyToLeave);
		SessionsSubsystem->DestroySession();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[OSPlayerController] ReturnToMainMenu — client path: traveling immediately"));
		// Client: fire-and-forget destroy (we don't own the session), travel immediately.
		SessionsSubsystem->DestroySession();
		Super::ClientReturnToMainMenuWithTextReason_Implementation(ReturnReason);
	}
}

void AOSPlayerController::OnHostReadyToLeave(bool bWasSuccessful)
{
	UE_LOG(LogTemp, Log, TEXT("[OSPlayerController] OnHostReadyToLeave (%s) — ServerTravel all clients to main menu"), bWasSuccessful ? TEXT("✓") : TEXT("✗"));

	if (UOSSessionsSubsystem* SessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOSSessionsSubsystem>()
		: nullptr)
	{
		SessionsSubsystem->OnDestroySessionComplete.RemoveAll(this);
	}
    // Super::ClientReturnToMainMenuWithTextReason_Implementation(FText::GetEmpty());
	// Session is already destroyed — EndMatchAndReturnToMenu detects no active session
	// and goes straight to ServerTravel, sending all connected clients to the main menu.
	if (UOSGameInstance* GI = GetGameInstance<UOSGameInstance>())
	{
		GI->EndMatchAndReturnToMenu();
	}
}

void AOSPlayerController::Server_PunchingBagAction_Implementation(FGameplayTag AbilityTag)
{
#if !UE_BUILD_SHIPPING
	UOSCheatManager* CM = Cast<UOSCheatManager>(this->CheatManager);
	if (!CM || !CM->PunchingBagActor.IsValid()) return;

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(CM->PunchingBagActor.Get()))
	{
		if (UAbilitySystemComponent* BagASC = ASI->GetAbilitySystemComponent())
		{
			const FOSGameplayTags& OSTags = FOSGameplayTags::Get();

			// COMBO SUPPORT: match HandleComboInput — pass type tag in InstigatorTags for cross-type detection.
			if (BagASC->HasMatchingGameplayTag(OSTags.IsAttacking))
			{
				FGameplayEventData ComboEvent;
				ComboEvent.InstigatorTags.AddTag(AbilityTag);
				BagASC->HandleGameplayEvent(OSTags.Event_ComboInputPressed, &ComboEvent);
				return;
			}

			// WORKAROUND: BPGE_OSGuardBreak's Guard.IsBroken tag persists indefinitely on the bag
			// rather than expiring after its authored duration (1.5s per OSAttributeSet.cpp:137
			// comment). Until the GE asset is audited — DurationPolicy, Magnitude, stacking
			// policy — strip the GE here before re-activating Block so the debug bag responds
			// to cheat-menu presses. A sibling bandaid lives at OSAttributeSet.cpp:137-142;
			// both should be removed together once the BP asset is fixed.
			if (AbilityTag == OSTags.Ability_Block)
			{
				static const FGameplayTag GuardBrokenTag =
					FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.Guard.IsBroken"), false);
				if (GuardBrokenTag.IsValid())
				{
					FGameplayTagContainer GuardBrokenTags;
					GuardBrokenTags.AddTag(GuardBrokenTag);
					BagASC->RemoveActiveEffectsWithGrantedTags(GuardBrokenTags);
				}
			}

			// Not comboing - start fresh
			FGameplayTagContainer AbilityTags;
			AbilityTags.AddTag(AbilityTag);
			const bool bActivated = BagASC->TryActivateAbilitiesByTag(AbilityTags);

			if (!bActivated && GDebugPunchingBag)
			{
				FGameplayTagContainer OwnedTags;
				BagASC->GetOwnedGameplayTags(OwnedTags);
				UE_LOG(LogOSDebug, Warning,
					TEXT("[BAG] TryActivateAbilitiesByTag(%s) returned false. Owned tags: %s"),
					*AbilityTag.ToString(),
					*OwnedTags.ToStringSimple());
			}
		}
	}
#endif
}

void AOSPlayerController::Server_PunchingBagCancelAbility_Implementation(FGameplayTag AbilityTag)
{
#if !UE_BUILD_SHIPPING
	UOSCheatManager* CM = Cast<UOSCheatManager>(this->CheatManager);
	if (!CM || !CM->PunchingBagActor.IsValid()) return;

	if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(CM->PunchingBagActor.Get()))
	{
		if (UAbilitySystemComponent* BagASC = ASI->GetAbilitySystemComponent())
		{
			FGameplayTagContainer CancelTags;
			CancelTags.AddTag(AbilityTag);
			BagASC->CancelAbilities(&CancelTags);
		}
	}
#endif
}

