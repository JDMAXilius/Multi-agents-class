// Breachpoint. The input -> ASC relay. Stubs today; BP02 routes them.

#include "Match/BRPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "InputMappingContext.h"

#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "Core/BRCore.h"
#include "Input/BRInputConfig.h"
#include "Match/BRPlayerState.h"

ABRPlayerController::ABRPlayerController()
{
	// Law 4: no gameplay Tick. The relay is delegate-driven end to end — Enhanced Input calls
	// the handlers, and nothing here polls anything.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

// ---------------------------------------------------------------------------
// The relay. Signature fixed by BindAbilityActions — see the header.
//
// No de-duplication here, no state here, no logging of the held stream here. All three moved to
// the ASC when BP02 step 1 landed, and none of them may come back: the controller's whole job in
// §3.2 is to know which ASC the tag belongs to.
// ---------------------------------------------------------------------------

void ABRPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent())
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
	else
	{
		// The PlayerState has not arrived yet. Reachable on a joining client for a frame or two;
		// the input is genuinely lost, and saying so is better than buffering it here and
		// replaying a stale press into a freshly spawned fighter.
		UE_LOG(LogBRInput, Verbose, TEXT("BRPlayerController '%s': %s pressed with no ASC yet (PlayerState not replicated); input dropped."),
			*GetName(), *InputTag.ToString());
	}
}

void ABRPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent())
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

UBRAbilitySystemComponent* ABRPlayerController::GetBRAbilitySystemComponent() const
{
	const ABRPlayerState* BRPlayerState = GetPlayerState<ABRPlayerState>();
	return BRPlayerState ? BRPlayerState->GetBRAbilitySystemComponent() : nullptr;
}

// ---------------------------------------------------------------------------
// Authored data
// ---------------------------------------------------------------------------

const UBRInputConfig* ABRPlayerController::GetInputConfig() const
{
	if (InputConfig.IsNull())
	{
		// Expected until BP01 step 3's generation script authors DA_InputConfig and it is
		// assigned on this class's defaults. Logged, not asserted — but it does mean every key
		// on the possessed pawn is dead, so it is not Verbose either.
		UE_LOG(LogBRInput, Warning, TEXT("BRPlayerController '%s': no InputConfig assigned; no action can be bound (expected until Content/Input/DA_InputConfig exists and is assigned)."),
			*GetName());
		return nullptr;
	}

	if (const UBRInputConfig* Resident = InputConfig.Get())
	{
		return Resident;
	}

	// Binding happens once per possession; the blocking load is survivable exactly here.
	return InputConfig.LoadSynchronous();
}

// ---------------------------------------------------------------------------
// Arrow one: the mapping context
// ---------------------------------------------------------------------------

void ABRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	AddDefaultMappingContext();
}

void ABRPlayerController::AddDefaultMappingContext()
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		// Not a locally controlled player (a server-side proxy for a remote client, or a bot's
		// controller). There is no hardware here and nothing to map. Not an error.
		UE_LOG(LogBRInput, Verbose, TEXT("BRPlayerController '%s': no local player; no mapping context added (remote or AI-controlled)."),
			*GetName());
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogBRInput, Error, TEXT("BRPlayerController '%s': local player has no EnhancedInputLocalPlayerSubsystem; the entire input chain is dead."),
			*GetName());
		return;
	}

	if (DefaultMappingContext.IsNull())
	{
		UE_LOG(LogBRInput, Warning, TEXT("BRPlayerController '%s': no DefaultMappingContext assigned; no key is mapped to any action (expected until Content/Input/IMC_Default exists and is assigned)."),
			*GetName());
		return;
	}

	UInputMappingContext* Context = DefaultMappingContext.LoadSynchronous();
	if (!Context)
	{
		UE_LOG(LogBRInput, Error, TEXT("BRPlayerController '%s': DefaultMappingContext '%s' failed to load."),
			*GetName(), *DefaultMappingContext.ToSoftObjectPath().ToString());
		return;
	}

	Subsystem->AddMappingContext(Context, DefaultMappingContextPriority);

	UE_LOG(LogBRInput, Log, TEXT("BRPlayerController '%s': added mapping context '%s' at priority %d."),
		*GetName(), *Context->GetName(), DefaultMappingContextPriority);
}

// ---------------------------------------------------------------------------
// Possession — logged as a seam, nothing more. The GAS init dance (InitAbilityActorInfo)
// is the PAWN's and BP02's; a controller-side copy of it is how ASCs end up initialised twice.
// ---------------------------------------------------------------------------

void ABRPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Authority-side only: OnPossess does not run on a remote client (its pawn arrives by
	// replication and AcknowledgePossession). Stated so nobody reads this line as proof the
	// client bound anything.
	UE_LOG(LogBRInput, Log, TEXT("BRPlayerController '%s': possessed pawn '%s' (authority side)."),
		*GetName(), *GetNameSafe(InPawn));
}

void ABRPlayerController::OnUnPossess()
{
	UE_LOG(LogBRInput, Log, TEXT("BRPlayerController '%s': unpossessed pawn '%s' (authority side)."),
		*GetName(), *GetNameSafe(GetPawn()));

	// The held-input flush is NOT here — see SetPawn below for why OnUnPossess is the wrong hook.
	Super::OnUnPossess();
}

void ABRPlayerController::SetPawn(APawn* InPawn)
{
	const APawn* PreviousPawn = GetPawn();

	Super::SetPawn(InPawn);

	if (PreviousPawn == InPawn)
	{
		return;
	}

	// FLUSH THE HELD-INPUT BUFFER ON EVERY PAWN CHANGE.
	//
	// The pawn's input component, and every binding on it, dies with the pawn — so no release event
	// will ever arrive for a key that was down at the moment of death. The ASC outlives the pawn, so
	// without this a respawned fighter starts life with a phantom held tag, and once step 3's
	// WhileHeld sprint exists it sprints on a key nobody is pressing.
	//
	// SetPawn AND NOT OnUnPossess, deliberately: OnUnPossess runs on the AUTHORITY ONLY, while the
	// buffer being flushed is the LOCAL client's. A remote client learns its pawn is gone through
	// OnRep_Pawn -> SetPawn and never sees OnUnPossess at all — so the server-only hook would clean
	// the one buffer that was already fine and leave the one that matters stale. Another
	// "correct in PIE, wrong against a real client" shape, which is why it is written down.
	if (UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent())
	{
		ASC->ClearAbilityInput();
	}
}
