// Breachpoint. The pawn: a body, not a brain.

#include "Character/BRCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"

#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "Character/BRCharacterMovementComponent.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Input/BRInputComponent.h"
#include "Input/BRInputConfig.h"
#include "Match/BRPlayerController.h"
#include "Match/BRPlayerState.h"

ABRCharacter::ABRCharacter(const FObjectInitializer& ObjectInitializer)
	// THE registration line for deliverable 2: ACharacter creates its movement component as the
	// named default subobject CharacterMovementComponentName, and this is the only hook that
	// changes WHICH class that subobject is. Get it wrong and every later sprint/grapple saved
	// move is written against a component the pawn does not have.
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBRCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Law 4: no gameplay Tick, on the one class where the temptation is strongest. Movement,
	// animation and input all arrive by their own paths; nothing here polls.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// FPS aiming: the pawn faces where the controller looks, yaw only. Pitch stays on the camera
	// (a pitched body is what RemoteViewPitch and the aim-offset ABP are for).
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// Crouch is a *capability* flag, not a tuning number — without it, Crouch() is a silent no-op.
	// Speeds, heights, air control and every other Halo-feel number are deliberately NOT set here
	// (§3.4: "config on CMC defaults, not code"; law 3: numbers live in data).
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
	}

	UCapsuleComponent* Capsule = GetCapsuleComponent();

	// ---------------------------------------------------------------------
	// The view
	// ---------------------------------------------------------------------
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(Capsule);
	FirstPersonCamera->bUsePawnControlRotation = true;
	// Eye height comes from APawn's own BaseEyeHeight rather than a literal, so the camera and
	// GetPawnViewLocation() (which the AI/trace code will use) cannot drift apart.
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));
	// UE 5 first-person rendering path (from the template pawn). Presentation only — FOV/scale
	// numbers are view feel, not sim. They matter once Mesh1P has an arms mesh; harmless while empty.
	FirstPersonCamera->bEnableFirstPersonFieldOfView = true;
	FirstPersonCamera->bEnableFirstPersonScale = true;
	FirstPersonCamera->FirstPersonFieldOfView = 70.f;
	FirstPersonCamera->FirstPersonScale = 0.6f;

	// ---------------------------------------------------------------------
	// Dual mesh (§3.4). The two halves are exact opposites on purpose: exactly one of them is
	// visible to any given viewer, and the pair is what makes a first-person view and a
	// third-person silhouette the same actor.
	// ---------------------------------------------------------------------

	// Mesh1P — arms + weapon, for the owning client only, and casting no shadow: a first-person
	// arms mesh sits inside the camera, so its shadow would be a pair of forearms thrown across
	// the world from nowhere.
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
	Mesh1P->SetupAttachment(FirstPersonCamera);
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh1P->SetGenerateOverlapEvents(false);
	Mesh1P->bReceivesDecals = false;
	// Template parity for the UE5 first-person primitive path. Mesh asset stays unassigned
	// (law 3 / BP19: Mesh1P is arms art we do not own yet — leave empty, do not put a full body here).
	Mesh1P->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);

	// Mesh3P — the inherited ACharacter mesh, configured as the full body. See GetMesh3P()'s
	// comment for why this is the inherited component and not a second one.
	if (USkeletalMeshComponent* Mesh3P = GetMesh())
	{
		Mesh3P->SetOwnerNoSee(true);
		Mesh3P->CastShadow = true;
		Mesh3P->bCastDynamicShadow = true;
		// The owner cannot see this mesh but MUST see its shadow — losing your own shadow is the
		// classic tell of a naive OwnerNoSee setup, and it is also what the death cam inherits.
		Mesh3P->bCastHiddenShadow = true;
		Mesh3P->bReceivesDecals = false;
		// World-space body for everyone else / death cam (template Shooter / FP pawn parity).
		Mesh3P->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);

		// Stand the body on the bottom of the capsule and face it down +X. The offset is derived
		// from the capsule rather than typed, so resizing the capsule cannot leave the body
		// floating; -90 yaw is the UE skeleton authoring convention (mannequin forward is -Y).
		Mesh3P->SetRelativeLocation(FVector(0.f, 0.f, -Capsule->GetUnscaledCapsuleHalfHeight()));
		Mesh3P->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}

	bLoggedFirstMoveInput = false;
	bLoggedFirstLookInput = false;
}

UAbilitySystemComponent* ABRCharacter::GetAbilitySystemComponent() const
{
	// Forward, own nothing. The pawn holds no ASC pointer of its own — not even a cached one —
	// because a cache would go stale the moment the PlayerState is swapped (seamless travel,
	// rejoin) and would then be a second, wrong answer to this question.
	const ABRPlayerState* BRPlayerState = GetPlayerState<ABRPlayerState>();
	return BRPlayerState ? BRPlayerState->GetAbilitySystemComponent() : nullptr;
}

// ---------------------------------------------------------------------------
// The GAS init dance (§3.4) — BOTH halves, and the reason there are two
// ---------------------------------------------------------------------------

void ABRCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// SERVER PATH. On the authority, PossessedBy runs after the controller (and therefore the
	// PlayerState) is attached, so the ASC is reachable right here.
	InitializeAbilitySystem(TEXT("PossessedBy (server)"));
}

void ABRCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// CLIENT PATH, AND IT IS NOT OPTIONAL. PossessedBy does not run on a remote client at all — the
	// client learns about its pawn and its PlayerState as two independent replication events, in
	// either order. Without this override the client's ASC has an owner and NO AVATAR: abilities
	// activate server-side and nothing predicts, no cue plays locally, no montage fires, and the
	// player feels a game running entirely on the round trip.
	//
	// THE REASON THIS BUG IS WORTH SIX LINES OF COMMENT: it is invisible in single-process PIE,
	// where the "client" shares a process with the server and finds a populated ASC by accident of
	// timing. It appears only against a real client — honesty-ladder rung 3, the rung nobody runs
	// by accident. BP01's builder flagged it in this ticket's Log for exactly that reason.
	InitializeAbilitySystem(TEXT("OnRep_PlayerState (client)"));
}

void ABRCharacter::InitializeAbilitySystem(const TCHAR* CallSite)
{
	ABRPlayerState* BRPlayerState = GetPlayerState<ABRPlayerState>();
	if (!BRPlayerState)
	{
		// Normal, not an error: on a client the pawn frequently arrives before its PlayerState. The
		// OTHER call site will run when the missing half lands — that redundancy IS the design.
		UE_LOG(LogBRCombat, Verbose, TEXT("BRCharacter '%s': %s — no ABRPlayerState yet; the other init path will cover it."),
			*GetName(), CallSite);
		return;
	}

	UBRAbilitySystemComponent* ASC = BRPlayerState->GetBRAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRCharacter '%s': %s — ABRPlayerState '%s' has no ASC. No ability on this pawn will ever activate."),
			*GetName(), CallSite, *BRPlayerState->GetName());
		return;
	}

	// Owner = the PlayerState (it outlives the pawn), Avatar = this pawn (it is what the world
	// sees). Re-pointing the avatar is the whole of respawn as far as GAS is concerned: attributes,
	// granted abilities and cooldowns were never on the pawn to lose.
	ASC->InitAbilityActorInfo(BRPlayerState, this);

	UE_LOG(LogBRCombat, Log, TEXT("BRCharacter '%s': %s — InitAbilityActorInfo(owner='%s', avatar='%s')."),
		*GetName(), CallSite, *BRPlayerState->GetName(), *GetName());
}

UBRCharacterMovementComponent* ABRCharacter::GetBRCharacterMovement() const
{
	return Cast<UBRCharacterMovementComponent>(GetCharacterMovement());
}

// ---------------------------------------------------------------------------
// Input wiring — §3.2 arrows three and four
// ---------------------------------------------------------------------------

void ABRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// CAST TO THE STOCK TYPE, NOT TO OURS, AND THAT CHANGE IS THE POINT.
	//
	// Until 1 Aug 2026 this cast demanded a UBRInputComponent and returned early when it failed —
	// so a single lost line in Config/DefaultInput.ini (DefaultInputComponentClass) killed every
	// key on every pawn while the pawn itself looked perfectly healthy. The binder functions only
	// ever needed UEnhancedInputComponent::BindAction, so the demand bought nothing and risked
	// everything. The surviving template pawns cast to the stock type and were immune; that
	// asymmetry is precisely what let "the template moves, ours doesn't" happen, and it is closed
	// here rather than documented.
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogBRInput, Error, TEXT("BRCharacter '%s': input component is '%s', not a UEnhancedInputComponent — check DefaultInputComponentClass in Config/DefaultInput.ini and DefaultPlayerInputClass. No input is bound."),
			*GetName(), *GetNameSafe(PlayerInputComponent ? PlayerInputComponent->GetClass() : nullptr));
		return;
	}

	// Still worth saying when it is not ours: UBRInputComponent is what Config/DefaultInput.ini
	// asks for, so anything else means that line was lost or overridden. Not fatal any more, and
	// no longer silent either.
	if (!Cast<UBRInputComponent>(PlayerInputComponent))
	{
		UE_LOG(LogBRInput, Warning, TEXT("BRCharacter '%s': input component is '%s', not a UBRInputComponent — DefaultInputComponentClass in Config/DefaultInput.ini is not being honoured. Binding proceeds via the stock Enhanced Input component."),
			*GetName(), *GetNameSafe(PlayerInputComponent->GetClass()));
	}

	// THE CONTROLLER IS NO LONGER A PRECONDITION FOR MOVING, and that is a Phase 1 requirement
	// rather than a preference. Both early returns that used to live here — "not an
	// ABRPlayerController" and "no InputConfig" — killed WASD outright, and the second one is
	// reachable from a single unassigned soft ref on a Blueprint details panel. The four native
	// verbs are the pawn's own; they now bind from the pawn's own action refs and need neither.
	// A missing controller still costs the ability relay and mouse-look fallback, and still says so.
	ABRPlayerController* BRController = Cast<ABRPlayerController>(GetController());
	if (!BRController)
	{
		// Also the honest state for a bot pawn: an AI controller presses tags on its own ASC
		// (§3.7) and has no input config at all.
		UE_LOG(LogBRInput, Warning, TEXT("BRCharacter '%s': controller '%s' is not an ABRPlayerController; native verbs still bind, no ability relay (expected for AI-controlled pawns)."),
			*GetName(), *GetNameSafe(GetController()));
	}

	const UBRInputConfig* Config = BRController ? BRController->GetInputConfig() : nullptr;

	// Mouse look: this pawn's own pin first, the controller's as a fallback. Resolved ONCE and
	// passed into whichever binder runs, so the two pins can never both bind and double the
	// player's turn rate.
	const UInputAction* ResolvedMouseLook = ResolveAction(MouseLookAction);
	if (!ResolvedMouseLook && BRController)
	{
		ResolvedMouseLook = BRController->GetMouseLookAction();
	}

	int32 MoveBinds = 0;
	int32 LookBinds = 0;
	int32 JumpBinds = 0;
	int32 CrouchBinds = 0;

	// PHASE 1 BASELINE: the switch. See ABRCharacter::bBindNativeVerbsFromInputConfig for what
	// each branch bends and what Phase 2 does with it (docs/PHASE2-RELAYER.md step 2).
	const bool bUseConfigForNatives = bBindNativeVerbsFromInputConfig && Config != nullptr;
	if (bBindNativeVerbsFromInputConfig && !Config)
	{
		UE_LOG(LogBRInput, Error,
			TEXT("BRCharacter '%s': bBindNativeVerbsFromInputConfig is true but no InputConfig resolved — falling back to the Phase 1 direct action refs so the pawn is still controllable."),
			*GetName());
	}

	if (bUseConfigForNatives)
	{
		BindNativeVerbsFromConfig(EnhancedInput, Config, ResolvedMouseLook, MoveBinds, LookBinds, JumpBinds, CrouchBinds);
	}
	else
	{
		BindNativeVerbsDirect(EnhancedInput, ResolvedMouseLook, MoveBinds, LookBinds, JumpBinds, CrouchBinds);
	}

	// --- Ability verbs: NOT named here, and never will be. ---
	// Every row in the config's ability list goes to the same two handlers ON THE CONTROLLER,
	// each carrying its own tag as a bound payload. The controller is the target rather than the
	// pawn because §3.2 routes the tag to the ASC via the controller, and because the controller
	// outlives the pawn across respawns.
	//
	// NOT PHASE 1. These bind, but nothing they route to exists yet: the startup ability set and
	// ApplyInitStats are unbuilt (ticket BP19 step A0), so every press reaches an ASC with no
	// granted ability and does nothing. Bound anyway because binding is free and removing it would
	// be deleting architecture to get movement, which Phase 1 forbids.
	if (Config && BRController)
	{
		UBRInputComponent::BindAbilityActions(EnhancedInput, Config, BRController, &ABRPlayerController::AbilityInputTagPressed, &ABRPlayerController::AbilityInputTagReleased, &AbilityInputBindHandles);
	}

	UE_LOG(LogBRInput, Log,
		TEXT("BRCharacter '%s': input bound on '%s' via %s — Move=%d Look=%d Mouse=%d Jump=%d Crouch=%d binding(s); ability relay %s (%d handles, config '%s', controller '%s')."),
		*GetName(), *GetNameSafe(PlayerInputComponent->GetClass()),
		bUseConfigForNatives ? TEXT("DA_InputConfig tags (Phase 2 shape)") : TEXT("direct action refs (PHASE 1 BASELINE)"),
		MoveBinds, LookBinds, ResolvedMouseLook ? 1 : 0, JumpBinds, CrouchBinds,
		(Config && BRController) ? TEXT("ON") : TEXT("OFF"),
		AbilityInputBindHandles.Num(), *GetNameSafe(Config), *GetNameSafe(BRController));

	// A native verb with zero bindings is a dead control, and it must not be reachable only by
	// counting fields in the line above. Named individually so the reader does not have to, and
	// naming the two possible causes so the reader does not have to guess either.
	if (MoveBinds == 0 || LookBinds == 0 || JumpBinds == 0 || CrouchBinds == 0 || !ResolvedMouseLook)
	{
		UE_LOG(LogBRInput, Error,
			TEXT("BRCharacter '%s': native verb(s) UNBOUND (Move=%d Look=%d Mouse=%d Jump=%d Crouch=%d). %s"),
			*GetName(), MoveBinds, LookBinds, ResolvedMouseLook ? 1 : 0, JumpBinds, CrouchBinds,
			bUseConfigForNatives
				? TEXT("Config path: DA_InputConfig has no NativeInputActions row carrying that InputTag, or the row's InputAction will not load.")
				: TEXT("Direct path: the pin is unset or will not load. Expected [/Script/Breachpoint.BRCharacter] MoveAction/LookAction/MouseLookAction/JumpAction/CrouchAction in Config/DefaultGame.ini."));
	}

	LogMovementSnapshot();
}

const UInputAction* ABRCharacter::ResolveAction(const TSoftObjectPtr<UInputAction>& SoftAction)
{
	if (SoftAction.IsNull())
	{
		return nullptr;
	}

	if (const UInputAction* Resident = SoftAction.Get())
	{
		return Resident;
	}

	// Binding happens once per possession; the blocking load is survivable exactly here, and
	// nowhere else in this class.
	return SoftAction.LoadSynchronous();
}

int32 ABRCharacter::BindNativeVerbsDirect(UEnhancedInputComponent* EnhancedInput, const UInputAction* ResolvedMouseLook, int32& OutMove, int32& OutLook, int32& OutJump, int32& OutCrouch)
{
	// PHASE 1 BASELINE: the template's binding SHAPE (pawn names its own actions,
	// breachpointCharacter.cpp:47-67), with none of the template's law-breaking — soft refs and an
	// ini pin instead of hard `UInputAction*` UPROPERTYs filled in from a Blueprint details panel.
	// Bends ARCHITECTURE.md §3.2 only. Phase 2 flips bBindNativeVerbsFromInputConfig; see
	// docs/PHASE2-RELAYER.md step 2.
	OutMove = 0;
	OutLook = 0;
	OutJump = 0;
	OutCrouch = 0;

	auto BindOne = [EnhancedInput, this](const UInputAction* Action, ETriggerEvent Event, auto Func) -> int32
	{
		if (!Action)
		{
			return 0;
		}

		EnhancedInput->BindAction(Action, Event, this, Func);
		return 1;
	};

	// Move/Look on Triggered (continuous); Jump/Crouch as a press/release pair, which is what the
	// default (empty) trigger set produces: Started on press, Completed on release.
	OutMove = BindOne(ResolveAction(MoveAction), ETriggerEvent::Triggered, &ABRCharacter::Input_Move);
	OutLook = BindOne(ResolveAction(LookAction), ETriggerEvent::Triggered, &ABRCharacter::Input_Look);
	OutJump = BindOne(ResolveAction(JumpAction), ETriggerEvent::Started, &ABRCharacter::Input_JumpStarted)
		+ BindOne(ResolveAction(JumpAction), ETriggerEvent::Completed, &ABRCharacter::Input_JumpCompleted);
	OutCrouch = BindOne(ResolveAction(CrouchAction), ETriggerEvent::Started, &ABRCharacter::Input_CrouchStarted)
		+ BindOne(ResolveAction(CrouchAction), ETriggerEvent::Completed, &ABRCharacter::Input_CrouchCompleted);

	const int32 MouseBinds = BindOne(ResolvedMouseLook, ETriggerEvent::Triggered, &ABRCharacter::Input_Look);

	return OutMove + OutLook + OutJump + OutCrouch + MouseBinds;
}

int32 ABRCharacter::BindNativeVerbsFromConfig(UEnhancedInputComponent* EnhancedInput, const UBRInputConfig* Config, const UInputAction* ResolvedMouseLook, int32& OutMove, int32& OutLook, int32& OutJump, int32& OutCrouch)
{
	// §3.2's permanent shape: the pawn names TAGS, the config names assets, and adding a verb is a
	// data edit. EVERY RETURN VALUE IS KEPT — the old code discarded all six and then logged the
	// literal string "4 native verbs", so a config missing the InputTag.Move row printed exactly
	// the same triumphant line as a fully wired one. A log that cannot fail is not evidence, and
	// that one was the first thing every reader trusted while WASD was dead.
	OutMove = UBRInputComponent::BindNativeAction(EnhancedInput, Config, BRGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ABRCharacter::Input_Move);
	OutLook = UBRInputComponent::BindNativeAction(EnhancedInput, Config, BRGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ABRCharacter::Input_Look);
	OutJump = UBRInputComponent::BindNativeAction(EnhancedInput, Config, BRGameplayTags::InputTag_Jump, ETriggerEvent::Started, this, &ABRCharacter::Input_JumpStarted)
		+ UBRInputComponent::BindNativeAction(EnhancedInput, Config, BRGameplayTags::InputTag_Jump, ETriggerEvent::Completed, this, &ABRCharacter::Input_JumpCompleted);
	OutCrouch = UBRInputComponent::BindNativeAction(EnhancedInput, Config, BRGameplayTags::InputTag_Crouch, ETriggerEvent::Started, this, &ABRCharacter::Input_CrouchStarted)
		+ UBRInputComponent::BindNativeAction(EnhancedInput, Config, BRGameplayTags::InputTag_Crouch, ETriggerEvent::Completed, this, &ABRCharacter::Input_CrouchCompleted);

	// Mouse look stays a SECOND action outside the config: the eleven-action contract forbids a
	// duplicate InputTag.Look row, so IA_MouseLook is pinned rather than tagged.
	int32 MouseBinds = 0;
	if (ResolvedMouseLook)
	{
		EnhancedInput->BindAction(ResolvedMouseLook, ETriggerEvent::Triggered, this, &ABRCharacter::Input_Look);
		MouseBinds = 1;
	}

	return OutMove + OutLook + OutJump + OutCrouch + MouseBinds;
}

void ABRCharacter::LogMovementSnapshot() const
{
	// WHY THIS EXISTS: "input arrives and the character does not move" and "input never arrives"
	// look identical from the outside, and the difference decides whether the next person reads
	// input code or movement config. A CMC whose MaxWalkSpeed is 0 accepts every input vector and
	// produces no displacement — indistinguishable from a dead key without this line.
	//
	// It READS numbers, it does not set them: speeds stay §3.4 "config on CMC defaults, not code"
	// (law 3). Once, at possession, never per frame (law 4).
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		UE_LOG(LogBRInput, Error, TEXT("BRCharacter '%s': no CharacterMovementComponent. Nothing this pawn is told to do can move it."), *GetName());
		return;
	}

	if (Movement->MaxWalkSpeed <= 0.f)
	{
		// NOT fixable from an ini, and saying so saves an hour: UCharacterMovementComponent's
		// MaxWalkSpeed and JumpZVelocity are declared EditAnywhere/BlueprintReadWrite with NO
		// `config` specifier (CharacterMovementComponent.h:274-275 and :163-164), so a
		// [/Script/Engine.CharacterMovementComponent] section is read by nobody. The pawn
		// Blueprint's CharacterMovement details panel is the only lawful home for these numbers.
		UE_LOG(LogBRInput, Error,
			TEXT("BRCharacter '%s': MaxWalkSpeed is %.1f on '%s'. Input will bind and arrive and the character will not move one centimetre. Fix it on BP_BRcharacter's CharacterMovement details panel (engine default is 600); an ini cannot set it, and law 3 keeps it out of C++."),
			*GetName(), Movement->MaxWalkSpeed, *GetNameSafe(Movement->GetClass()));
		return;
	}

	UE_LOG(LogBRInput, Log,
		TEXT("BRCharacter '%s': movement ready on '%s' — MaxWalkSpeed=%.1f MaxWalkSpeedCrouched=%.1f JumpZVelocity=%.1f GravityScale=%.2f AirControl=%.2f mode=%d canCrouch=%d."),
		*GetName(), *GetNameSafe(Movement->GetClass()),
		Movement->MaxWalkSpeed, Movement->MaxWalkSpeedCrouched, Movement->JumpZVelocity,
		Movement->GravityScale, Movement->AirControl,
		static_cast<int32>(Movement->MovementMode.GetValue()),
		Movement->GetNavAgentPropertiesRef().bCanCrouch ? 1 : 0);
}

void ABRCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	const AController* OwningController = GetController();

	// THE LATCH FIRES BEFORE THE EARLY RETURN, and the order is the diagnostic. It used to sit
	// after, so a Triggered event carrying a zero vector — a gamepad inside its deadzone, a
	// modifier zeroing the axis, a Down trigger firing on release — was indistinguishable from the
	// key never arriving at all. Those are opposite bugs: one is data, one is wiring. This line now
	// means "the delegate ran", exactly that, and reports what it was handed.
	if (!bLoggedFirstMoveInput)
	{
		bLoggedFirstMoveInput = true;
		UE_LOG(LogBRInput, Log,
			TEXT("BRCharacter '%s': FIRST Move input (%.2f, %.2f), controller '%s' — IMC -> UInputAction -> pawn. The native half of the chain is live. A (0.00, 0.00) here means the binding is correct and the VALUE is dead: check the IMC's modifiers and the action's Value Type (expect Axis2D)."),
			*GetName(), Axis.X, Axis.Y, *GetNameSafe(OwningController));
	}

	if (!OwningController || Axis.IsNearlyZero())
	{
		return;
	}

	// Control-rotation space, yaw only: "forward" is where the player is looking, flattened, so
	// looking down does not walk you into the floor. Correct independently of
	// bUseControllerRotationYaw, which is why this does not read the actor's own rotation.
	const FRotationMatrix YawFrame(FRotator(0.f, OwningController->GetControlRotation().Yaw, 0.f));
	AddMovementInput(YawFrame.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(YawFrame.GetUnitAxis(EAxis::Y), Axis.X);

	UE_LOG(LogBRInput, VeryVerbose, TEXT("BRCharacter '%s': Move (%.2f, %.2f)"), *GetName(), Axis.X, Axis.Y);
}

void ABRCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();

	// Sign convention: passed through as authored. Inversion and sensitivity are IMC modifiers
	// (input data), never arithmetic in the pawn.
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);

	if (!bLoggedFirstLookInput)
	{
		bLoggedFirstLookInput = true;
		UE_LOG(LogBRInput, Log, TEXT("BRCharacter '%s': FIRST Look input (%.2f, %.2f) — IMC -> UInputAction -> BRInputComponent -> InputTag.Look -> pawn."),
			*GetName(), Axis.X, Axis.Y);
	}

	UE_LOG(LogBRInput, VeryVerbose, TEXT("BRCharacter '%s': Look (%.2f, %.2f)"), *GetName(), Axis.X, Axis.Y);
}

void ABRCharacter::Input_JumpStarted()
{
	// ACharacter::Jump sets bPressedJump; the CMC owns the rest, including the prediction.
	Jump();
	UE_LOG(LogBRInput, Log, TEXT("BRCharacter '%s': InputTag.Jump pressed -> Jump()"), *GetName());
}

void ABRCharacter::Input_JumpCompleted()
{
	StopJumping();
	UE_LOG(LogBRInput, Log, TEXT("BRCharacter '%s': InputTag.Jump released -> StopJumping()"), *GetName());
}

void ABRCharacter::Input_CrouchStarted()
{
	// Hold semantics. Crouch() is a no-op unless NavAgentProps.bCanCrouch is set — see the
	// constructor.
	Crouch();
	UE_LOG(LogBRInput, Log, TEXT("BRCharacter '%s': InputTag.Crouch pressed -> Crouch()"), *GetName());
}

void ABRCharacter::Input_CrouchCompleted()
{
	UnCrouch();
	UE_LOG(LogBRInput, Log, TEXT("BRCharacter '%s': InputTag.Crouch released -> UnCrouch()"), *GetName());
}
