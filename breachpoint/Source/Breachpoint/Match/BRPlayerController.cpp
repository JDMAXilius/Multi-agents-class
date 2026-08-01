// Breachpoint. The input -> ASC relay. Stubs today; BP02 routes them.

#include "Match/BRPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "InputMappingContext.h"

#include "Core/BRCore.h"
#include "Input/BRInputConfig.h"

ABRPlayerController::ABRPlayerController()
{
	// Law 4: no gameplay Tick. The relay is delegate-driven end to end — Enhanced Input calls
	// the handlers, and nothing here polls anything.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

// ---------------------------------------------------------------------------
// The relay stubs. Signature fixed by BindAbilityActions — see the header.
// ---------------------------------------------------------------------------

void ABRPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	// Bound to ETriggerEvent::Triggered, so this fires every frame the key is held. The edge is
	// logged at Log; the repeats at Verbose. See LogBRInput's verbosity policy in BRCore.h.
	if (LoggedHeldInputTags.Contains(InputTag))
	{
		UE_LOG(LogBRInput, Verbose, TEXT("BRPlayerController '%s': AbilityInputTag HELD %s"),
			*GetName(), *InputTag.ToString());
		return;
	}

	LoggedHeldInputTags.Add(InputTag);

	UE_LOG(LogBRInput, Log, TEXT("BRPlayerController '%s': AbilityInputTagPressed %s (stub — BP02 relays this to the ASC)"),
		*GetName(), *InputTag.ToString());
}

void ABRPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	LoggedHeldInputTags.Remove(InputTag);

	UE_LOG(LogBRInput, Log, TEXT("BRPlayerController '%s': AbilityInputTagReleased %s (stub — BP02 relays this to the ASC)"),
		*GetName(), *InputTag.ToString());
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

	// The pawn's input component (and every binding on it) dies with the pawn; drop the
	// diagnostic held set with it so a respawn does not inherit a stale "held" view.
	LoggedHeldInputTags.Reset();

	Super::OnUnPossess();
}
