// Breachpoint. The input -> ASC relay. Stubs today; BP02 routes them.

#include "Match/BRPlayerController.h"

#include "CommonGameViewportClient.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
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
//
// ---------------------------------------------------------------------------
// AND THE STAGE-3 GATE IS NOT HERE EITHER — DELIBERATELY. Recorded at the site where the next
// reader will look for it, so nobody re-derives the decision or "fixes" the omission.
//
// `BRCharacter.cpp` names this file's two handlers as the `InputRouted` hook
// (docs/GAS-INTEGRATION-ROADMAP.md, stage 3). The gate that hook asks for LANDED ONE LEVEL DOWN,
// as the first statement of `UBRAbilitySystemComponent::AbilityInputTagPressed/Released`, where
// the full argument is written out. The three-line version:
//
//   - A refusal logged HERE proves only that the controller was reached. A refusal logged at the
//     ASC proves the whole relay — PlayerState replicated, `GetBRAbilitySystemComponent()`
//     resolved, the component alive — and that untested half of the chain IS what stage 3 exists
//     to observe. Gating here would hide the press at exactly the point the roadmap needs it
//     visible, and "the tag never left the controller" reads identically to a dead binding.
//   - The ASC is the choke point. `ABRBotController::PressInputTag` routes to the same two
//     functions and bypasses this file entirely, so a gate here is a gate with a hole in it.
//   - That hole is not theoretical: at stage `Granting` the roadmap promises *"abilities exist;
//     no input reaches them"*, and grants land on a bot's ASC exactly as on a human's. A
//     controller-only gate lets a bot activate stage 2's grants during stage 2's test run.
//
// So the relay below is UNCHANGED and stays a relay. Its own `no ASC yet` Verbose line is not a
// stage refusal and must not be read as one — it reports a PlayerState that has not replicated,
// which is a different failure with a different fix, and it keeps that meaning at every stage.
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

const UInputAction* ABRPlayerController::GetMouseLookAction() const
{
	if (MouseLookAction.IsNull())
	{
		return nullptr;
	}

	if (const UInputAction* Resident = MouseLookAction.Get())
	{
		return Resident;
	}

	return MouseLookAction.LoadSynchronous();
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

	// CommonUI sits in FRONT of the player controller. Wrong viewport client => every binding
	// logs success and every keypress dies before Enhanced Input — the exact "cannot move"
	// symptom. Config/DefaultEngine.ini pins CommonGameViewportClient; this check catches a
	// stale editor session that never picked the ini up (needs a full editor restart).
	// Read GEngine->GameViewport directly: in PIE GEngine is the editor engine, not UGameEngine.
	if (GEngine && GEngine->GameViewport
		&& !GEngine->GameViewport->IsA(UCommonGameViewportClient::StaticClass()))
	{
		UE_LOG(LogBRInput, Error,
			TEXT("BRPlayerController '%s': GameViewportClient is '%s', not CommonGameViewportClient. CommonUI will swallow gameplay input — keys appear dead while bindings succeed. Restart the editor after Config/DefaultEngine.ini sets GameViewportClientClassName."),
			*GetName(), *GetNameSafe(GEngine->GameViewport->GetClass()));
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogBRInput, Error, TEXT("BRPlayerController '%s': local player has no EnhancedInputLocalPlayerSubsystem; the entire input chain is dead."),
			*GetName());
		return;
	}

	// Every context that actually reached the subsystem, for the key census below. The census asks
	// a question no other log line answers: do these contexts contain a KEY for the actions the
	// pawn is about to bind? See LogMappingKeyCensus.
	TArray<const UInputMappingContext*> AddedContexts;

	// ContextRole, not Role: AActor::Role is a member and C4458 (shadowing) is an error here.
	auto AddOneContext = [this, Subsystem, &AddedContexts](const TSoftObjectPtr<UInputMappingContext>& SoftContext, const TCHAR* ContextRole) -> bool
	{
		if (SoftContext.IsNull())
		{
			UE_LOG(LogBRInput, Warning, TEXT("BRPlayerController '%s': no %s assigned; that mapping context will not be added."),
				*GetName(), ContextRole);
			return false;
		}

		UInputMappingContext* Context = SoftContext.LoadSynchronous();
		if (!Context)
		{
			UE_LOG(LogBRInput, Error, TEXT("BRPlayerController '%s': %s '%s' failed to load."),
				*GetName(), ContextRole, *SoftContext.ToSoftObjectPath().ToString());
			return false;
		}

		Subsystem->AddMappingContext(Context, DefaultMappingContextPriority);
		AddedContexts.Add(Context);
		UE_LOG(LogBRInput, Log, TEXT("BRPlayerController '%s': added mapping context '%s' (%s) at priority %d, %d key mapping(s)."),
			*GetName(), *Context->GetName(), ContextRole, DefaultMappingContextPriority, Context->GetMappings().Num());
		return true;
	};

	AddOneContext(DefaultMappingContext, TEXT("DefaultMappingContext"));

	// Shooter template pattern: Default IMCs always, then the mouse-look IMC that touch builds
	// omit. AdditionalMappingContexts is that second list — today IMC_MouseLook.
	//
	// AN EMPTY LIST HERE IS NOT NECESSARILY FINE, and the loop cannot say so, so this does: on a
	// keyboard/mouse build IMC_Default carries no Mouse2D/MouseX mapping at all (verified against
	// the asset's name table), so with no second context the player cannot TURN. "Cannot turn"
	// reads to a playtester as "cannot move", which is why this is an explicit line and not a
	// silent zero-iteration loop.
	if (AdditionalMappingContexts.IsEmpty())
	{
		UE_LOG(LogBRInput, Warning,
			TEXT("BRPlayerController '%s': AdditionalMappingContexts is EMPTY. If IMC_Default carries no mouse mapping, mouse look is dead — pin IMC_MouseLook via [/Script/Breachpoint.BRPlayerController] +AdditionalMappingContexts in Config/DefaultGame.ini (this class is config=Game, so that section is read) or on the PC Blueprint's defaults."),
			*GetName());
	}

	for (const TSoftObjectPtr<UInputMappingContext>& Extra : AdditionalMappingContexts)
	{
		AddOneContext(Extra, TEXT("AdditionalMappingContext"));
	}

	// HARD contexts, set on the Blueprint — the template's shape, and the reason is in the header:
	// every link in the soft/ini chain was verified correct on 1 Aug and input STILL differed
	// between this controller and `BP_ShooterPlayerController`, which holds its contexts as hard
	// pointers. So the remaining difference is the mechanism, and this is the mechanism that is
	// known to work on this project, on this map, today.
	//
	// Deliberately last: soft/ini first keeps the ini authoritative for anyone driving from
	// config, and a context named in both lists is simply added twice at the same priority, which
	// Enhanced Input treats as one.
	for (const TObjectPtr<UInputMappingContext>& Hard : HardMappingContexts)
	{
		if (!Hard)
		{
			UE_LOG(LogBRInput, Warning,
				TEXT("BRPlayerController '%s': HardMappingContexts holds an EMPTY entry — an array slot was added on the Blueprint and never filled."),
				*GetName());
			continue;
		}

		Subsystem->AddMappingContext(Hard, DefaultMappingContextPriority);
		AddedContexts.Add(Hard);
		UE_LOG(LogBRInput, Log,
			TEXT("BRPlayerController '%s': added mapping context '%s' (HardMappingContexts, set on the Blueprint) at priority %d, %d key mapping(s)."),
			*GetName(), *Hard->GetName(), DefaultMappingContextPriority, Hard->GetMappings().Num());
	}

	// The whole point of the census: it is the ONE line that answers "do the contexts that landed
	// actually contain a KEY for the verbs the pawn is about to bind" — which is the question
	// every input failure on this project has turned out to be.
	if (AddedContexts.IsEmpty())
	{
		UE_LOG(LogBRInput, Error,
			TEXT("BRPlayerController '%s': ZERO mapping contexts were added. No key can reach any action. Pin them in [/Script/Breachpoint.BRPlayerController] in Config/DefaultGame.ini, or set HardMappingContexts on PC_BR in the editor — the template's BP_ShooterPlayerController does the latter and works."),
			*GetName());
	}

	LogMappingKeyCensus(AddedContexts);
}

void ABRPlayerController::LogMappingKeyCensus(const TArray<const UInputMappingContext*>& AddedContexts) const
{
	const UBRInputConfig* Config = GetInputConfig();
	if (!Config)
	{
		// GetInputConfig already said so, loudly. Without a config there is nothing to cross-check
		// the contexts against — and nothing the pawn will bind either.
		return;
	}

	// Count, per InputAction asset, how many keys the ADDED contexts give it. An action with zero
	// is the silent-dead-key case: the row exists, the bind succeeds, no hardware reaches it.
	TMap<const UInputAction*, int32> KeysPerAction;
	for (const UInputMappingContext* Context : AddedContexts)
	{
		if (!Context)
		{
			continue;
		}

		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (const UInputAction* Action = Mapping.Action.Get())
			{
				KeysPerAction.FindOrAdd(Action) += 1;
			}
		}
	}

	// Report both lists, because the two halves fail for different reasons and the reader needs to
	// know which one they are looking at: a dead native verb is a broken movement/camera control,
	// a dead ability verb is one unreachable ability.
	auto CensusOneList = [this, &KeysPerAction](const TArray<FBRInputAction>& Rows, const TCHAR* ListName)
	{
		TArray<FString> Entries;
		Entries.Reserve(Rows.Num());
		int32 Unmapped = 0;

		for (const FBRInputAction& Row : Rows)
		{
			const UInputAction* Action = Row.GetInputAction();
			if (!Action)
			{
				Entries.Add(FString::Printf(TEXT("%s=NO-ASSET"), *Row.InputTag.ToString()));
				++Unmapped;
				continue;
			}

			const int32 Keys = KeysPerAction.FindRef(Action);
			Entries.Add(FString::Printf(TEXT("%s(%s)=%dkey"), *Row.InputTag.ToString(), *Action->GetName(), Keys));
			if (Keys == 0)
			{
				++Unmapped;
			}
		}

		const FString Report = FString::Join(Entries, TEXT(", "));

		if (Unmapped > 0)
		{
			UE_LOG(LogBRInput, Error,
				TEXT("BRPlayerController '%s': %d %s row(s) have NO KEY in the mapping contexts that were added. Those verbs will bind successfully and never fire. Census: %s"),
				*GetName(), Unmapped, ListName, *Report);
		}
		else
		{
			UE_LOG(LogBRInput, Log, TEXT("BRPlayerController '%s': %s key census — %s"),
				*GetName(), ListName, *Report);
		}
	};

	CensusOneList(Config->GetNativeInputActions(), TEXT("native"));
	CensusOneList(Config->GetAbilityInputActions(), TEXT("ability"));

	// MouseLookAction is not a config row (the eleven-action contract forbids a second
	// InputTag.Look), so it needs its own line or its absence is invisible.
	if (const UInputAction* MouseLook = GetMouseLookAction())
	{
		UE_LOG(LogBRInput, Log, TEXT("BRPlayerController '%s': MouseLookAction '%s' has %d key(s) in the added contexts."),
			*GetName(), *MouseLook->GetName(), KeysPerAction.FindRef(MouseLook));
	}
	else
	{
		UE_LOG(LogBRInput, Warning,
			TEXT("BRPlayerController '%s': no MouseLookAction assigned — mouse look will not be bound. Expected pin: [/Script/Breachpoint.BRPlayerController] MouseLookAction=/Game/Input/Actions/IA_MouseLook.IA_MouseLook in Config/DefaultGame.ini."),
			*GetName());
	}
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
