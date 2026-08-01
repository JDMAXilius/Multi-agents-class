// Breachpoint. The pawn: a body, not a brain.

#include "Character/BRCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

#include "Character/BRCharacterMovementComponent.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Input/BRInputComponent.h"
#include "Input/BRInputConfig.h"
#include "Match/BRPlayerController.h"

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
	// No mesh asset is assigned here and none may be: hard asset references in C++ are banned
	// (law 3). Meshes and ABPs arrive as soft references from data, assigned by the packets that
	// own those assets (anim-builder's ABPs, Weapons/ for the socketed weapon mesh).

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
	// THE SEAM, LEFT OPEN ON PURPOSE. BP01 owns no ASC, no attribute set and no PlayerState; the
	// ASC lives on ABRPlayerState (§3.6) and arrives with BP02. Returning null is honest —
	// inventing a pawn-owned ASC here would be a second, unreplicated ASC that BP02 would then
	// have to delete, and every ability granted to it would be lost on respawn.
	// The exact four steps BP02 must take are listed on this function's declaration.
	return nullptr;
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

	UBRInputComponent* BRInput = Cast<UBRInputComponent>(PlayerInputComponent);
	if (!BRInput)
	{
		// The component class comes from Config/DefaultInput.ini's DefaultInputComponentClass,
		// which BP01 step 4 points at UBRInputComponent. If that line is lost, EVERY key on
		// every pawn dies at once and nothing else reports it.
		UE_LOG(LogBRInput, Error, TEXT("BRCharacter '%s': input component is '%s', not a UBRInputComponent — check DefaultInputComponentClass in Config/DefaultInput.ini. No input is bound."),
			*GetName(), *GetNameSafe(PlayerInputComponent ? PlayerInputComponent->GetClass() : nullptr));
		return;
	}

	ABRPlayerController* BRController = Cast<ABRPlayerController>(GetController());
	if (!BRController)
	{
		// Also the honest state for a bot pawn: an AI controller presses tags on its own ASC
		// (§3.7) and has no input config at all. Not an error, but it does mean no hardware
		// binding exists on this pawn.
		UE_LOG(LogBRInput, Warning, TEXT("BRCharacter '%s': controller '%s' is not an ABRPlayerController; no input bound (expected for AI-controlled pawns)."),
			*GetName(), *GetNameSafe(GetController()));
		return;
	}

	const UBRInputConfig* Config = BRController->GetInputConfig();
	if (!Config)
	{
		// GetInputConfig already logged why. Stop rather than bind half a control scheme.
		return;
	}

	// --- Native verbs: named here, because the pawn genuinely owns them. ---
	// Move/Look on Triggered (continuous); Jump/Crouch as a press/release pair, which is what the
	// default trigger set produces (Started on press, Completed on release).
	BRInput->BindNativeAction(Config, BRGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ABRCharacter::Input_Move);
	BRInput->BindNativeAction(Config, BRGameplayTags::InputTag_Look, ETriggerEvent::Triggered, this, &ABRCharacter::Input_Look);
	BRInput->BindNativeAction(Config, BRGameplayTags::InputTag_Jump, ETriggerEvent::Started, this, &ABRCharacter::Input_JumpStarted);
	BRInput->BindNativeAction(Config, BRGameplayTags::InputTag_Jump, ETriggerEvent::Completed, this, &ABRCharacter::Input_JumpCompleted);
	BRInput->BindNativeAction(Config, BRGameplayTags::InputTag_Crouch, ETriggerEvent::Started, this, &ABRCharacter::Input_CrouchStarted);
	BRInput->BindNativeAction(Config, BRGameplayTags::InputTag_Crouch, ETriggerEvent::Completed, this, &ABRCharacter::Input_CrouchCompleted);

	// --- Ability verbs: NOT named here, and never will be. ---
	// Every row in the config's ability list goes to the same two handlers ON THE CONTROLLER,
	// each carrying its own tag as a bound payload. The controller is the target rather than the
	// pawn because §3.2 routes the tag to the ASC via the controller, and because the controller
	// outlives the pawn across respawns.
	BRInput->BindAbilityActions(Config, BRController, &ABRPlayerController::AbilityInputTagPressed, &ABRPlayerController::AbilityInputTagReleased, &AbilityInputBindHandles);

	UE_LOG(LogBRInput, Log, TEXT("BRCharacter '%s': input bound via UBRInputComponent — 4 native verbs (Move/Look/Jump/Crouch), %d ability rows from config '%s' -> controller '%s' (%d bind handles)."),
		*GetName(), Config->GetAbilityInputActions().Num(), *Config->GetName(), *BRController->GetName(), AbilityInputBindHandles.Num());
}

void ABRCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();

	const AController* OwningController = GetController();
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

	if (!bLoggedFirstMoveInput)
	{
		bLoggedFirstMoveInput = true;
		UE_LOG(LogBRInput, Log, TEXT("BRCharacter '%s': FIRST Move input (%.2f, %.2f) — IMC -> UInputAction -> BRInputComponent -> InputTag.Move -> pawn. The native half of the chain is live."),
			*GetName(), Axis.X, Axis.Y);
	}

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
