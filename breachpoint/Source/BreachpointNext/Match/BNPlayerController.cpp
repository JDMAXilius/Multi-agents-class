#include "Match/BNPlayerController.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "BreachpointNext.h"
#include "Core/BNGameplayTags.h"
#include "Input/BNInputComponent.h"
#include "Input/BNInputConfig.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Effects/BNDamage.h"
#include "Match/BNPlayerState.h"
#include "Animation/BNAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "GameFramework/Pawn.h"

#if !UE_BUILD_SHIPPING
namespace
{
	/** True when the call was forwarded and this machine must do nothing else. */
	bool BNForwardToAuthority(APlayerController* PC, const FString& Command)
	{
		if (!PC || PC->HasAuthority())
		{
			return false;
		}

		// The engine's own console-to-server channel: the server runs the identical command on
		// its own copy of this controller, which is the machine allowed to move an attribute.
		// (ServerExec, not ServerCheat — UE 5.8 removed the latter; same channel, 128-char cap.)
		PC->ServerExec(Command);
		return true;
	}
}
#endif

void ABNPlayerController::BeginPlay()
{
	Super::BeginPlay();

#if !UE_BUILD_SHIPPING
	// The forwarded ServerExec runs ConsoleCommand on the server's copy of this controller, and
	// this controller's own exec levers reach ProcessConsoleExec without a cheat manager — but
	// the engine's stock cheats (God, etc.) still need one, and AGameModeBase::AllowCheats says
	// no outside the editor, so a packaged development listen-server would eat those. Forced
	// here, authority side only; the class is the engine's plain UCheatManager.
	if (HasAuthority() && !CheatManager)
	{
		AddCheats(true);
	}
#endif
}

void ABNPlayerController::BNDamageSelf(float Amount)
{
#if !UE_BUILD_SHIPPING
	if (BNForwardToAuthority(this, FString::Printf(TEXT("BNDamageSelf %f"), Amount)))
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();
	BNDamage::ApplyDamage(ControlledPawn, ControlledPawn, Amount, FHitResult());
#endif
}

void ABNPlayerController::BNKillSelf()
{
#if !UE_BUILD_SHIPPING
	if (BNForwardToAuthority(this, TEXT("BNKillSelf")))
	{
		return;
	}

	const ABNPlayerState* PS = GetPlayerState<ABNPlayerState>();
	const UAbilitySystemComponent* ASC = PS ? PS->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	// Lethal is COMPUTED and paid through the door, never asserted by writing zero: death has to
	// arrive the way a bullet's would, or this lever tests a path nothing else uses.
	APawn* ControlledPawn = GetPawn();
	BNDamage::ApplyDamage(ControlledPawn, ControlledPawn,
		ASC->GetNumericAttribute(UBNAttributeSet::GetHealthAttribute())
			+ ASC->GetNumericAttribute(UBNAttributeSet::GetShieldAttribute()),
		FHitResult());
#endif
}

void ABNPlayerController::BNRefill()
{
#if !UE_BUILD_SHIPPING
	if (BNForwardToAuthority(this, TEXT("BNRefill")))
	{
		return;
	}

	// Through the init GE, the same one respawn uses. Nothing here hand-sets an attribute.
	if (ABNPlayerState* PS = GetPlayerState<ABNPlayerState>())
	{
		PS->ApplyInitAttributes();
	}
#endif
}

#if !UE_BUILD_SHIPPING
namespace
{
	/** The anim instance this machine renders for the local pawn. Local by design: an anim
	 *  instance is per-machine, so there is nothing here for the server to own. */
	UBNAnimInstance* BNLocalAnimInstance(const APlayerController* PC)
	{
		const ACharacter* Char = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		const USkeletalMeshComponent* MeshComp = Char ? Char->GetMesh() : nullptr;
		return MeshComp ? Cast<UBNAnimInstance>(MeshComp->GetAnimInstance()) : nullptr;
	}

	EBNSpineAxis BNAxisFromIndex(int32 Axis)
	{
		switch (Axis)
		{
		case 1:  return EBNSpineAxis::Pitch;
		case 2:  return EBNSpineAxis::Yaw;
		default: return EBNSpineAxis::Roll;
		}
	}
}
#endif

void ABNPlayerController::BNAimDebug()
{
#if !UE_BUILD_SHIPPING
	UBNAnimInstance* Anim = BNLocalAnimInstance(this);
	if (!Anim)
	{
		UE_LOG(LogBreachpointNext, Warning,
			TEXT("BNAimDebug: no UBNAnimInstance on the local pawn's mesh — either the pawn is not "
				 "possessed yet or the mesh's ABP does not inherit UBNAnimInstance."));
		return;
	}
	UE_LOG(LogBreachpointNext, Log, TEXT("%s"), *Anim->DescribeAimState());
#endif
}

void ABNPlayerController::BNAimAxis(int32 Axis)
{
#if !UE_BUILD_SHIPPING
	if (UBNAnimInstance* Anim = BNLocalAnimInstance(this))
	{
		Anim->SetAimPitchAxis(BNAxisFromIndex(Axis));
		UE_LOG(LogBreachpointNext, Log, TEXT("%s"), *Anim->DescribeAimState());
	}
#endif
}

void ABNPlayerController::BNLeanAxis(int32 Axis)
{
#if !UE_BUILD_SHIPPING
	if (UBNAnimInstance* Anim = BNLocalAnimInstance(this))
	{
		Anim->SetLeanAxis(BNAxisFromIndex(Axis));
		UE_LOG(LogBreachpointNext, Log, TEXT("%s"), *Anim->DescribeAimState());
	}
#endif
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

void ABNPlayerController::SetupInputComponent()
{
	// NEXT owns its input component outright. The project-wide DefaultInputComponentClass
	// names the OLD module's class, and this module depends on nothing there. Creating it
	// before Super is the engine's own sanctioned override point.
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
				UE_LOG(LogBN, Error, TEXT("BNPlayerController: MappingContexts is empty — set +MappingContexts under [/Script/BreachpointNext.BNPlayerController] in DefaultGame.ini. No key reaches any action."));
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
		UE_LOG(LogBN, Error, TEXT("BNPlayerController: input not bound (component=%s, InputConfig='%s'). Run Tools/bn/10_input_assets.py and check DefaultGame.ini."),
			BNInput ? TEXT("ok") : TEXT("wrong class"), *InputConfig.ToString());
		return;
	}

	const auto Bind = [this, BNInput, Config](FGameplayTag Tag, ETriggerEvent Event, auto Func)
	{
		if (!BNInput->BindActionByTag(Config, Tag, Event, this, Func))
		{
			UE_LOG(LogBN, Error, TEXT("BNPlayerController: %s has no InputAction in '%s' — that control is dead."), *Tag.ToString(), *InputConfig.ToString());
		}
	};

	Bind(BNTags::Input_Move, ETriggerEvent::Triggered, &ABNPlayerController::HandleMove);
	Bind(BNTags::Input_Look, ETriggerEvent::Triggered, &ABNPlayerController::HandleLook);
	Bind(BNTags::Input_Jump, ETriggerEvent::Started, &ABNPlayerController::HandleJumpPressed);
	Bind(BNTags::Input_Jump, ETriggerEvent::Completed, &ABNPlayerController::HandleJumpReleased);
	Bind(BNTags::Input_Crouch, ETriggerEvent::Started, &ABNPlayerController::HandleCrouchPressed);
	Bind(BNTags::Input_Crouch, ETriggerEvent::Completed, &ABNPlayerController::HandleCrouchReleased);
	// Press only: the swap verbs are one-shot, so there is no release event to forward.
	Bind(BNTags::Input_Weapon_Next, ETriggerEvent::Started, &ABNPlayerController::HandleWeaponNextPressed);
	Bind(BNTags::Input_Weapon_Previous, ETriggerEvent::Started, &ABNPlayerController::HandleWeaponPreviousPressed);
	// Fire needs the release: Auto holds the ability open for as long as the trigger is held.
	// Reload is one-shot, so press only, like the swap verbs.
	Bind(BNTags::Input_Weapon_Fire, ETriggerEvent::Started, &ABNPlayerController::HandleFirePressed);
	Bind(BNTags::Input_Weapon_Fire, ETriggerEvent::Completed, &ABNPlayerController::HandleFireReleased);
	Bind(BNTags::Input_Weapon_Reload, ETriggerEvent::Started, &ABNPlayerController::HandleReloadPressed);
	Bind(BNTags::Input_Sprint, ETriggerEvent::Started, &ABNPlayerController::HandleSprintPressed);
	Bind(BNTags::Input_Sprint, ETriggerEvent::Completed, &ABNPlayerController::HandleSprintReleased);
	Bind(BNTags::Input_Lean_Left, ETriggerEvent::Started, &ABNPlayerController::HandleLeanLeftPressed);
	Bind(BNTags::Input_Lean_Left, ETriggerEvent::Completed, &ABNPlayerController::HandleLeanLeftReleased);
	Bind(BNTags::Input_Lean_Right, ETriggerEvent::Started, &ABNPlayerController::HandleLeanRightPressed);
	Bind(BNTags::Input_Lean_Right, ETriggerEvent::Completed, &ABNPlayerController::HandleLeanRightReleased);
	// ANNOUNCED, not created: IA_BN_Melee and IA_BN_Grenade plus their DA_BNInput rows and
	// IMC_BNNext mappings are the editor half of R3 W3. Until they exist Bind logs its "that
	// control is dead" line and the abilities are unreachable — loudly unreachable, which is the
	// point. The C++ half is complete: both abilities activate off these tags.
	Bind(BNTags::Input_Melee, ETriggerEvent::Started, &ABNPlayerController::HandleMeleePressed);
	Bind(BNTags::Input_Grenade, ETriggerEvent::Started, &ABNPlayerController::HandleGrenadePressed);
#if !UE_BUILD_SHIPPING
	// ANNOUNCED, not created: IA_BNDebugDamageSelf + its DA_BNInput row + its IMC_BNNext mapping
	// do not exist yet, so until the founder adds them this binding logs its "that control is
	// dead" line every run — which is the announcement being loud rather than silent. The BINDING
	// is guarded, not just the handler: a shipping listen-server host is an authority, so an
	// unguarded key would damage the host for real.
	Bind(BNTags::Input_Debug_DamageSelf, ETriggerEvent::Started, &ABNPlayerController::HandleDebugDamagePressed);
#endif
}

void ABNPlayerController::HandleMove(const FInputActionValue& Value)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
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

void ABNPlayerController::HandleDebugDamagePressed()
{
	// One keypress, the SAME exec the console runs — the key gets no path of its own, and no
	// second authority hop to keep in sync.
	BNDamageSelf();
}

UBNAbilitySystemComponent* ABNPlayerController::GetBNAbilitySystemComponent() const
{
	const ABNPlayerState* PS = GetPlayerState<ABNPlayerState>();
	return PS ? PS->GetBNAbilitySystemComponent() : nullptr;
}
