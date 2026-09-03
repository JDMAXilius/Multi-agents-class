#include "Characters/BNCharacter.h"

#include "Characters/BNCharacterMovementComponent.h"

#include "AIBotAdapter/BNAIBAvatarAdapter.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/BNGA_Death.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "Characters/BNHealthComponent.h"
#include "Core/BNCollision.h"
#include "Core/BNGameplayTags.h"
#include "Data/BNDataRows.h"
#include "Match/BNPlayerState.h"
#include "Match/BNTeams.h"
#include "Materials/MaterialInterface.h"
#include "Navigation/CrowdManager.h"
#include "Weapons/BNEquipmentComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/BNLAnimInstance.h"
#include "Weapons/BNWeapon.h"
#include "BreachpointNext.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr uint8 BN_Pose_HipStand = 1;
	constexpr uint8 BN_Pose_AimStand = 2;
	constexpr uint8 BN_Pose_HipCrouch = 6;
	constexpr uint8 BN_Pose_AimCrouch = 7;
	constexpr uint8 BN_Scope_Default = 0;
	constexpr float BN_AimPoseChangeSpeed = 18.f;

	bool BNSetFnByte(UFunction* Fn, void* Buffer, const TCHAR* Name, uint8 Value)
	{
		if (const FByteProperty* AsByte = FindFProperty<FByteProperty>(Fn, FName(Name)))
		{
			AsByte->SetPropertyValue_InContainer(Buffer, Value);
			return true;
		}
		if (const FEnumProperty* AsEnum = FindFProperty<FEnumProperty>(Fn, FName(Name)))
		{
			AsEnum->GetUnderlyingProperty()->SetIntPropertyValue(
				AsEnum->ContainerPtrToValuePtr<void>(Buffer), static_cast<int64>(Value));
			return true;
		}
		return false;
	}

	bool BNSetFnNumber(UFunction* Fn, void* Buffer, const TCHAR* Name, float Value)
	{
		if (const FDoubleProperty* AsDouble = FindFProperty<FDoubleProperty>(Fn, FName(Name)))
		{
			AsDouble->SetPropertyValue_InContainer(Buffer, static_cast<double>(Value));
			return true;
		}
		if (const FFloatProperty* AsFloat = FindFProperty<FFloatProperty>(Fn, FName(Name)))
		{
			AsFloat->SetPropertyValue_InContainer(Buffer, Value);
			return true;
		}
		return false;
	}

	/** Called through the UFunction's OWN parameter layout: a hand-written mirror struct passed to
	 *  ProcessEvent is silent memory corruption the day the Blueprint's signature changes. Pin
	 *  names verified against the graph: (InPoseType, InScopeType, InChangeSpeed). */
	void BNCallChangePose(UObject* Comp, UFunction* Fn, uint8 PoseType, uint8 ScopeType, float ChangeSpeed)
	{
		TArray<uint8> Buffer;
		Buffer.SetNumZeroed(FMath::Max<int32>(Fn->ParmsSize, 1));
		for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Buffer.GetData());
		}

		bool bAllSet = true;
		bAllSet &= BNSetFnByte(Fn, Buffer.GetData(), TEXT("InPoseType"), PoseType);
		bAllSet &= BNSetFnByte(Fn, Buffer.GetData(), TEXT("InScopeType"), ScopeType);
		bAllSet &= BNSetFnNumber(Fn, Buffer.GetData(), TEXT("InChangeSpeed"), ChangeSpeed);
		if (bAllSet)
		{
			Comp->ProcessEvent(Fn, Buffer.GetData());
		}

		for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Buffer.GetData());
		}
	}
}

ABNCharacter::ABNCharacter(const FObjectInitializer& ObjectInitializer)
	// The engine's one sanctioned swap point for a default subobject: the grapple's
	// predicted pull lives in the movement component, so the class changes HERE — the
	// legacy module's own compiled pattern (BRCharacter.cpp:21).
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBNCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 96.f);

	// Epic's FP template, two representations of one skeleton:
	//   FirstPerson  — owner only, rendered in the camera's first-person pass (own FOV + scale)
	//   WorldSpace   — everyone else, hidden from the owner so the two never draw on top of each other
	// The 1P mesh is a FOLLOWER: SetLeaderPoseComponent(GetMesh()) so the native aim proxy poses
	// once and both views agree. A second anim instance would be a second aim brain.
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->bReceivesDecals = false;
	FirstPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);

	// The view rides the 1P head. Position from the socket (lagged); rotation from the controller.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(FirstPersonMesh, CameraAttachSocket);
	CameraBoom->TargetArmLength = 0.f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 15.f;
	CameraBoom->CameraLagMaxDistance = 10.f;
	CameraBoom->bEnableCameraRotationLag = false;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	CameraComponent->bUsePawnControlRotation = false;
	// Manny's head socket is not camera-forward. These are the template's measured eye offset.
	CameraComponent->SetRelativeLocationAndRotation(FVector(-2.8f, 5.89f, 0.0f), FRotator(0.0f, 90.0f, -90.0f));
	// UE 5.5+ first-person pass: 1P primitives render at their own FOV and a scale that keeps
	// the arms in frame without parenting them to the camera.
	CameraComponent->bEnableFirstPersonFieldOfView = true;
	CameraComponent->bEnableFirstPersonScale = true;
	CameraComponent->FirstPersonFieldOfView = 70.f;
	CameraComponent->FirstPersonScale = 0.6f;

	EquipmentComponent = CreateDefaultSubobject<UBNEquipmentComponent>(TEXT("EquipmentComponent"));
	HealthComponent = CreateDefaultSubobject<UBNHealthComponent>(TEXT("HealthComponent"));

	// Weapon and melee traces are judged against the MESH, per bone, which is the only way a
	// headshot can differ from a gut shot. The capsule is deliberately deaf to both: it is a
	// movement volume, and a shot that resolves on a cylinder resolves on the wrong shape.
	// (Neither profile answers ECC_Visibility, by engine default and on purpose — see BNCollision.h.)
	GetMesh()->SetCollisionResponseToChannel(BNCollision::WeaponTrace, ECR_Block);
	GetMesh()->SetCollisionResponseToChannel(BNCollision::MeleeTrace, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(BNCollision::WeaponTrace, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(BNCollision::MeleeTrace, ECR_Ignore);

	// A dedicated server renders nothing, and hit confirmation is judged against the mesh's
	// physics bodies — without RefreshBones those bodies sit in the reference pose and every
	// server-side trace is scored against a T-pose.
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->bReceivesDecals = false;

	// Yaw only. Pitch and roll on the capsule would tip the whole character with the view; the
	// spine bends instead, through the anim instance's aim chain. Side-to-side aim IS this yaw:
	// the body (and the weapon welded to its hands) turns with the look. A separate aim-offset
	// yaw only appears when the body lags the view — interpolation on a proxy, or a future
	// turn-in-place that stops the capsule.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = false;
	MoveComp->bUseControllerDesiredRotation = false;
	MoveComp->NavAgentProps.bCanCrouch = true;
	MoveComp->bCanWalkOffLedgesWhenCrouching = true;
	MoveComp->AirControl = 0.5f;
	MoveComp->BrakingDecelerationFalling = 1500.f;

	// BOTS MOVE THE WAY PLAYERS DO. This one flag is why bots slid instead of walking.
	//
	// Path following has two modes (UPathFollowingComponent::FollowPathSegment):
	//   false (the engine default) -> RequestDirectMove(), which sets CMC::RequestedVelocity and
	//                                 NEVER assigns CMC::Acceleration;
	//   true                       -> RequestPathMove() -> AddInputVector(), the exact same road
	//                                 a player's input takes to ConsumeInputVector.
	//
	// Everything downstream asks GetCurrentAcceleration(): UBNLAnimInstance, and — the part that
	// actually gates the walk cycle — ABP_ItemAnimLayersBase, which binds
	// `GetMovementComponent.GetCurrentAcceleration` DIRECTLY through its own PropertyAccess and
	// never reads our C++ field at all. So with the default, a pathing bot reported zero
	// acceleration to every consumer at once and Lyra's locomotion machine held it in Idle while
	// it travelled at 600 uu/s.
	//
	// Fixing it HERE rather than in either AnimBP is deliberate: it repairs the source, so the
	// two ABPs, the linked layers and the C++ spine all agree without any of them being edited,
	// and it needs no asset change (law 7 — C++ wherever C++ can express it).
	//
	// Inert for players: a player-controlled pawn never runs path following, so this flag is only
	// ever read on a pawn an AIController is moving.
	if (FNavMovementProperties* NavProps = MoveComp->GetNavMovementProperties())
	{
		NavProps->bUseAccelerationForPaths = true;
	}
}

FRotator ABNCharacter::GetAimRotation() const
{
	FRotator Aim = GetBaseAimRotation();
	Aim.Pitch = FRotator::NormalizeAxis(Aim.Pitch);
	Aim.Yaw = FRotator::NormalizeAxis(Aim.Yaw);
	return Aim;
}

void ABNCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// The BP child assigns the mannequin to GetMesh. The 1P mesh is the same skeleton, posed by
	// the 3P instance — copy the asset here, after the child has written it.
	if (FirstPersonMesh)
	{
		if (USkeletalMesh* Body = GetMesh()->GetSkeletalMeshAsset())
		{
			FirstPersonMesh->SetSkeletalMeshAsset(Body);
		}
		FirstPersonMesh->SetLeaderPoseComponent(GetMesh());
	}

	if (CameraBoom && CameraBoom->GetAttachSocketName() != CameraAttachSocket)
	{
		CameraBoom->AttachToComponent(FirstPersonMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, CameraAttachSocket);
	}

	// The Blueprint editor preview never hits BeginPlay. Without this the Lyra main ABP has no
	// linked item layer and the viewport is a T-pose even when PIE would walk.
	InitializeAnimLayer();
}

void ABNCharacter::AttachWeaponMeshes(ABNWeapon* Weapon)
{
	if (!Weapon || !FirstPersonMesh)
	{
		return;
	}

	const FBNWeaponRow* Row = Weapon->GetRow();
	const FName Socket = Row ? Row->AttachSocketName : NAME_None;
	if (USkeletalMeshComponent* FPWeapon = Weapon->GetFirstPersonMesh())
	{
		FPWeapon->AttachToComponent(FirstPersonMesh,
			FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget,
				EAttachmentRule::KeepWorld, false),
			Socket);
	}
}

void ABNCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitializeAnimLayer();
	ResolvePoseOffsets();
	VerifyDamageCollision();

	// Bound on every machine; the handler is what gates on authority. The delegate lives on a
	// component this character owns, so it dies with the body and leaks nothing onto the
	// persistent ability system component.
	HealthComponent->OnDeath.AddUObject(this, &ABNCharacter::HandleDeath);

	// TEAM COLOURS. Runs on every machine including the server's own listen window, because
	// the colourway is a LOCAL read of two replicated ids — never a replicated property of
	// its own. Called here and again at every point either id can land.
	EnsureTeamSubscriptions();
	RefreshTeamColors();
}

// THE HITTABILITY CHECK — every weapon's damage depends on this and nothing else reports it.
//
// The constructor sets the mesh to Block WeaponTrace/MeleeTrace, but a constructor value is only
// a DEFAULT: a Blueprint that serialised its own collision settings out-serialises it, silently.
// That matters right now because the pawn the game mode spawns is BP_FPSCharacter — a Blueprint
// authored long before these channels existed and reparented onto this class afterwards. If its
// mesh does not answer the channels, EVERY bullet and EVERY swing passes straight through every
// player and hits the wall behind them, with a perfectly valid FHitResult and no error anywhere.
// That is precisely the failure this project keeps paying for, so it announces itself instead.
void ABNCharacter::VerifyDamageCollision() const
{
	const USkeletalMeshComponent* MeshComp = GetMesh();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!MeshComp || !Capsule)
	{
		return;
	}

	const bool bMeshBlocksWeapon = MeshComp->GetCollisionResponseToChannel(BNCollision::WeaponTrace) == ECR_Block;
	const bool bMeshBlocksMelee = MeshComp->GetCollisionResponseToChannel(BNCollision::MeleeTrace) == ECR_Block;
	const bool bMeshCollides = MeshComp->GetCollisionEnabled() != ECollisionEnabled::NoCollision;

	if (bMeshBlocksWeapon && bMeshBlocksMelee && bMeshCollides)
	{
		UE_LOG(LogBN, Log, TEXT("BNHit: %s is hittable — mesh blocks WeaponTrace and MeleeTrace (profile '%s')."),
			*GetName(), *MeshComp->GetCollisionProfileName().ToString());
	}
	else
	{
		UE_LOG(LogBN, Error,
			TEXT("BNHit: %s CANNOT BE SHOT. Mesh collision '%s': enabled=%s, WeaponTrace=%s, MeleeTrace=%s. "
				 "Bullets and melee will pass through this character and hit the wall behind it. The pawn's "
				 "Blueprint has out-serialised the C++ collision defaults — set the mesh to Block the BRWeapon "
				 "and BRMelee channels on the Blueprint's mesh component, or clear its collision override."),
			*GetName(), *MeshComp->GetCollisionProfileName().ToString(),
			bMeshCollides ? TEXT("yes") : TEXT("NO"),
			bMeshBlocksWeapon ? TEXT("Block") : TEXT("NOT BLOCKING"),
			bMeshBlocksMelee ? TEXT("Block") : TEXT("NOT BLOCKING"));
	}

	// The capsule must stay deaf, or every shot resolves on a cylinder and no headshot can exist.
	if (Capsule->GetCollisionResponseToChannel(BNCollision::WeaponTrace) != ECR_Ignore)
	{
		UE_LOG(LogBN, Warning,
			TEXT("BNHit: %s's capsule answers WeaponTrace — shots will resolve on the movement cylinder "
				 "instead of the body, so bone-based headshots can never fire."), *GetName());
	}
}

void ABNCharacter::ResolvePoseOffsets()
{
	// By name fragment: the SCS node is named after its class (BPC_FPST_Procedural_PoseOffsets)
	// and instance names may carry suffixes.
	for (UActorComponent* Comp : GetComponents())
	{
		if (Comp && Comp->GetName().Contains(TEXT("PoseOffsets")))
		{
			PoseOffsetsComponent = Comp;
			return;
		}
	}
}

void ABNCharacter::HandleDeath(UBNHealthComponent* /*Component*/)
{
	if (!HasAuthority())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = CachedAbilitySystem.Get())
	{
		ASC->TryActivateAbilityByClass(UBNGA_Death::StaticClass());
	}
}

void ABNCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	if (HasAuthority() && !CrouchStateHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UBNGE_State::StaticClass(), 1.f, ASC->MakeEffectContext());
			if (Spec.IsValid())
			{
				Spec.Data->DynamicGrantedTags.AddTag(BNTags::State_Movement_Crouching);
				CrouchStateHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
			}
		}
	}

	RefreshWeaponPose();
}

void ABNCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	if (HasAuthority() && CrouchStateHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
		{
			ASC->RemoveActiveGameplayEffect(CrouchStateHandle);
		}
		CrouchStateHandle = FActiveGameplayEffectHandle();
	}

	RefreshWeaponPose();
}

void ABNCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCrowdManager* Crowd = UCrowdManager::GetCurrent(this))
	{
		Crowd->UnregisterAgent(this);
	}

	if (UAbilitySystemComponent* ASC = CachedAbilitySystem.Get())
	{
		if (MoveSpeedChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBNAttributeSet::GetMoveSpeedAttribute())
				.Remove(MoveSpeedChangedHandle);
			MoveSpeedChangedHandle.Reset();
		}

		if (ADSPoseTagHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(BNTags::State_Weapon_ADS, EGameplayTagEventType::NewOrRemoved)
				.Remove(ADSPoseTagHandle);
			ADSPoseTagHandle.Reset();
		}

		if (CrouchStateHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(CrouchStateHandle);
			CrouchStateHandle = FActiveGameplayEffectHandle();
		}
	}

	// The OWN handle dies with its PlayerState anyway, but the VIEWER'S does not: that
	// PlayerState outlives every one of this player's pawns, so an unreleased handle would
	// stack one dead delegate per respawn for the whole match.
	if (ABNPlayerState* OwnPS = SubscribedOwnPS.Get(); OwnPS && OwnTeamChangedHandle.IsValid())
	{
		OwnPS->OnTeamChanged.Remove(OwnTeamChangedHandle);
	}
	if (ABNPlayerState* ViewerPS = SubscribedViewerPS.Get(); ViewerPS && ViewerTeamChangedHandle.IsValid())
	{
		ViewerPS->OnTeamChanged.Remove(ViewerTeamChangedHandle);
	}
	OwnTeamChangedHandle.Reset();
	ViewerTeamChangedHandle.Reset();
	SubscribedOwnPS.Reset();
	SubscribedViewerPS.Reset();

	CachedAbilitySystem.Reset();
	Super::EndPlay(EndPlayReason);
}

UAbilitySystemComponent* ABNCharacter::GetAbilitySystemComponent() const
{
	const ABNPlayerState* PS = GetPlayerState<ABNPlayerState>();
	return PS ? PS->GetBNAbilitySystemComponent() : nullptr;
}

void ABNCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// AIBot track: an AIB-possessed body grows its avatar adapter — the module's one
	// door — before the controller's OnPossess goes looking for it. Humans and BN bots
	// never carry it; the call is a no-op for them.
	UBNAIBAvatarAdapter::EnsureOn(this, NewController);

	if (HasAuthority() && Cast<APlayerController>(NewController))
	{
		if (UCrowdManager* Crowd = UCrowdManager::GetCurrent(this))
		{
			Crowd->RegisterAgent(this);
		}
	}

	InitializeAbilitySystem();
	InitializeAnimLayer();

	if (ABNPlayerState* PS = GetPlayerState<ABNPlayerState>())
	{
		PS->GrantDefaults();
	}

	if (EquipmentComponent)
	{
		EquipmentComponent->InitializeCarriedWeapons();
	}

	EnsureTeamSubscriptions();
	RefreshTeamColors();
}

void ABNCharacter::UnPossessed()
{
	if (UCrowdManager* Crowd = UCrowdManager::GetCurrent(this))
	{
		Crowd->UnregisterAgent(this);
	}
	Super::UnPossessed();
}

FVector ABNCharacter::GetCrowdAgentLocation() const
{
	return GetNavAgentLocation();
}

FVector ABNCharacter::GetCrowdAgentVelocity() const
{
	return GetVelocity();
}

void ABNCharacter::GetCrowdAgentCollisions(float& CylinderRadius, float& CylinderHalfHeight) const
{
	GetCapsuleComponent()->CalcBoundingCylinder(CylinderRadius, CylinderHalfHeight);
}

float ABNCharacter::GetCrowdAgentMaxSpeed() const
{
	return GetCharacterMovement()->GetMaxSpeed();
}

void ABNCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializeAbilitySystem();
	InitializeAnimLayer();

	// A pawn routinely replicates BEFORE its PlayerState: until this fires there was no team
	// to read and the body wore the mesh's shipped colours. This is the retry that matters
	// most on a joining client.
	EnsureTeamSubscriptions();
	RefreshTeamColors();
}

void ABNCharacter::InitializeAnimLayer()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp || !MeshComp->GetAnimInstance())
	{
		return;
	}

	UClass* LayerClass = ResolveAnimLayerClass();
	if (LayerClass == LinkedAnimLayerClass)
	{
		return;
	}

	if (LinkedAnimLayerClass)
	{
		MeshComp->UnlinkAnimClassLayers(LinkedAnimLayerClass.Get());
	}
	LinkedAnimLayerClass = LayerClass;

	if (LayerClass)
	{
		MeshComp->LinkAnimClassLayers(LayerClass);
		// The anim CLASS is named too, not just the layer: three ABP_Mannequin_Base assets exist
		// in this project and every previous aim fix edited one the pawn does not run. This line
		// is how you tell, from the log alone, which one is live.
		const UAnimInstance* Anim = MeshComp->GetAnimInstance();
		UE_LOG(LogBN, Log, TEXT("BNCharacter: linked anim layer %s onto %s."),
			*GetNameSafe(LayerClass), *GetNameSafe(Anim ? Anim->GetClass() : nullptr));
	}
}

void ABNCharacter::InitializeAbilitySystem()
{
	ABNPlayerState* PS = GetPlayerState<ABNPlayerState>();
	if (!PS)
	{
		return;
	}

	UBNAbilitySystemComponent* ASC = PS->GetBNAbilitySystemComponent();
	ASC->InitAbilityActorInfo(PS, this);
	CachedAbilitySystem = ASC;

	HealthComponent->InitializeWithAbilitySystem(ASC);

	if (!MoveSpeedChangedHandle.IsValid())
	{
		MoveSpeedChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBNAttributeSet::GetMoveSpeedAttribute())
			.AddUObject(this, &ABNCharacter::OnMoveSpeedChanged);
	}

	if (!ADSPoseTagHandle.IsValid())
	{
		ADSPoseTagHandle = ASC->RegisterGameplayTagEvent(BNTags::State_Weapon_ADS, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ABNCharacter::HandleADSTagChanged);
	}

	const float MoveSpeed = ASC->GetNumericAttribute(UBNAttributeSet::GetMoveSpeedAttribute());
	if (MoveSpeed > 0.f)
	{
		GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	}
}

void ABNCharacter::OnMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
	GetCharacterMovement()->MaxWalkSpeed = Data.NewValue;
}

void ABNCharacter::HandleADSTagChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
	RefreshWeaponPose(NewCount);
}

void ABNCharacter::RefreshWeaponPose(int32 ADSCount)
{
	UFunction* ChangePose = PoseOffsetsComponent ? PoseOffsetsComponent->FindFunction(FName(TEXT("ChangePose"))) : nullptr;
	if (!ChangePose)
	{
		return;
	}

	bool bADS = ADSCount > 0;
	if (ADSCount < 0)
	{
		const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
		bADS = ASC && ASC->HasMatchingGameplayTag(BNTags::State_Weapon_ADS);
	}

	const uint8 PoseType = bADS
		? (bIsCrouched ? BN_Pose_AimCrouch : BN_Pose_AimStand)
		: (bIsCrouched ? BN_Pose_HipCrouch : BN_Pose_HipStand);

	// Scope 0 until the weapon row carries a ScopeType column.
	BNCallChangePose(PoseOffsetsComponent, ChangePose, PoseType, BN_Scope_Default, BN_AimPoseChangeSpeed);
}

UClass* ABNCharacter::GetCurrentWeaponAnimLayer() const
{
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponAnimLayer() : nullptr;
}

UClass* ABNCharacter::ResolveLyraLayerForRow(FName RowName) const
{
	const FSoftClassPath* Path = &LyraUnarmedAnimLayer;
	if (RowName == FName(TEXT("Pistol")))
	{
		Path = &LyraPistolAnimLayer;
	}
	else if (RowName == FName(TEXT("Rifle")))
	{
		Path = &LyraRifleAnimLayer;
	}
	else if (RowName == FName(TEXT("Shotgun")))
	{
		Path = &LyraShotgunAnimLayer;
	}

	if (UClass* Loaded = Path->IsNull() ? nullptr : Path->TryLoadClass<UAnimInstance>())
	{
		return Loaded;
	}

	if (UClass* Unarmed = LyraUnarmedAnimLayer.IsNull() ? nullptr : LyraUnarmedAnimLayer.TryLoadClass<UAnimInstance>())
	{
		return Unarmed;
	}

	return LyraItemAnimLayersBase.IsNull() ? nullptr : LyraItemAnimLayersBase.TryLoadClass<UAnimInstance>();
}

UClass* ABNCharacter::ResolveAnimLayerClass()
{
	// FOUNDER'S RULING, 22 Aug 2026: LYRA LOCOMOTION ONLY. There is no second path any more —
	// the layer is always the Lyra set, chosen by the weapon's row name from the ini. The
	// FPSTemplate branch that used to live below (DT_BNWeapons' AnimLayerClass column, then the
	// unarmed fallback) is deleted with the ruling: a dormant second path through the anim spine
	// is precisely what let three aim fixes land on assets the game never loads.
	const ABNWeapon* Weapon = EquipmentComponent ? EquipmentComponent->GetCurrentWeapon() : nullptr;
	return ResolveLyraLayerForRow(Weapon ? Weapon->GetRowName() : NAME_None);
}


// ---------------------------------------------------------------------------- team colours

void ABNCharacter::EnsureTeamSubscriptions()
{
	// MY side. Rebound rather than skipped when the PlayerState changes identity, which it
	// does on a rejoin — a handle held on the old one would never fire again.
	ABNPlayerState* OwnPS = GetPlayerState<ABNPlayerState>();
	if (OwnPS != SubscribedOwnPS.Get())
	{
		if (ABNPlayerState* Previous = SubscribedOwnPS.Get(); Previous && OwnTeamChangedHandle.IsValid())
		{
			Previous->OnTeamChanged.Remove(OwnTeamChangedHandle);
		}
		OwnTeamChangedHandle.Reset();
		SubscribedOwnPS = OwnPS;
		if (OwnPS)
		{
			OwnTeamChangedHandle = OwnPS->OnTeamChanged.AddUObject(this, &ABNCharacter::HandleOwnTeamChanged);
		}
	}

	// THE VIEWER'S side, and this is the subscription that is easy to forget: when the local
	// player's own team id lands, EVERY body already in the world is wearing the wrong colour,
	// including bodies whose own team never changes again. Asked of this actor's world, not
	// GEngine's first controller globally — in PIE two clients share a process (the same trap
	// ResolveTeamTint documents).
	const APlayerController* Viewer = GEngine ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	ABNPlayerState* ViewerPS = Viewer ? Viewer->GetPlayerState<ABNPlayerState>() : nullptr;
	if (ViewerPS != SubscribedViewerPS.Get())
	{
		if (ABNPlayerState* Previous = SubscribedViewerPS.Get(); Previous && ViewerTeamChangedHandle.IsValid())
		{
			Previous->OnTeamChanged.Remove(ViewerTeamChangedHandle);
		}
		ViewerTeamChangedHandle.Reset();
		SubscribedViewerPS = ViewerPS;
		if (ViewerPS)
		{
			ViewerTeamChangedHandle = ViewerPS->OnTeamChanged.AddUObject(this, &ABNCharacter::HandleViewerTeamChanged);
		}
	}
}

void ABNCharacter::HandleOwnTeamChanged(ABNPlayerState* /*PS*/)
{
	RefreshTeamColors();
}

void ABNCharacter::HandleViewerTeamChanged(ABNPlayerState* /*PS*/)
{
	// Same work, different reason — every body re-reads itself because each holds its own
	// subscription to this one PlayerState. No sweep, no iteration over the world.
	RefreshTeamColors();
}


EBNBodyColorway ABNCharacter::ResolveBodyColorway(FGenericTeamId ViewerTeam, FGenericTeamId OwnTeam, bool bIsViewerSelf)
{
	// YOUR OWN body is your own side, always — checked BEFORE the unassigned guard, because in
	// FFA you are still yourself. ResolveTeamTint answers "no tint" for self on purpose (your
	// own muzzle flash keeps the look it always had); a BODY is the opposite case, the one a
	// kill-cam or a spectator sees, where reading as your own side is what you want.
	if (bIsViewerSelf)
	{
		return EBNBodyColorway::Ally;
	}

	// EITHER side unassigned answers SHIPPED, and this is the guard that keeps FFA from turning
	// red. AreFriendly has two answers and this needs three: without it an unassigned body would
	// read ENEMY rather than unknown, so a teams-OFF match — and every joining client's first
	// frames, before the id replicates — would paint the whole lobby as threats.
	if (ViewerTeam == FGenericTeamId::NoTeam || OwnTeam == FGenericTeamId::NoTeam)
	{
		return EBNBodyColorway::Shipped;
	}

	return BNTeams::AreFriendly(ViewerTeam, OwnTeam) ? EBNBodyColorway::Ally : EBNBodyColorway::Threat;
}

void ABNCharacter::RefreshTeamColors()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Body->GetSkeletalMeshAsset())
	{
		return;
	}

	// THE VIEWER, not the instigator — the same question BNGameplayCues::ResolveTeamTint asks,
	// and deliberately the same ANSWER: BN16's ruling is that red means threat and blue means
	// your side, "one hue, one meaning — held, not bent". Absolute Red-team/Blue-team bodies
	// would bend it, putting red on your own team-mates while the scoreboard called them blue.
	const ABNPlayerState* OwnPS = GetPlayerState<ABNPlayerState>();
	const APlayerController* Viewer = GEngine ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	const ABNPlayerState* ViewerPS = Viewer ? Viewer->GetPlayerState<ABNPlayerState>() : nullptr;
	if (!OwnPS || !ViewerPS)
	{
		return; // nothing known yet; the body keeps the mesh's shipped colours
	}

	const EBNBodyColorway Colorway = ResolveBodyColorway(
		ViewerPS->GetGenericTeamId(), OwnPS->GetGenericTeamId(), /*bIsViewerSelf=*/OwnPS == ViewerPS);
	if (Colorway == EBNBodyColorway::Shipped)
	{
		return; // FFA, or a team id that has not landed yet — the mesh keeps what it shipped with
	}

	const bool bFriendly = (Colorway == EBNBodyColorway::Ally);
	const FSoftObjectPath& TorsoPath    = bFriendly ? AllyTorsoMaterial    : ThreatTorsoMaterial;
	const FSoftObjectPath& HeadLegsPath = bFriendly ? AllyHeadLegsMaterial : ThreatHeadLegsMaterial;

	// BY SLOT NAME. SKM_Manny's torso slot carries MI_Manny_02 and its head/legs slot carries
	// MI_Manny_01 — the numbering runs OPPOSITE to the slot order, so pairing by index swaps
	// the two materials and the result looks like a broken texture rather than a wrong pairing.
	const auto ApplySlot = [Body](FName SlotName, const FSoftObjectPath& Path)
	{
		if (SlotName.IsNone() || !Path.IsValid())
		{
			return; // an unconfigured slot is left exactly as the mesh shipped it
		}
		const int32 Index = Body->GetMaterialIndex(SlotName);
		if (Index == INDEX_NONE)
		{
			UE_LOG(LogBN, Warning, TEXT("BNCharacter: team colours name slot '%s', which %s does not have — that slot keeps its shipped material."),
				*SlotName.ToString(), *GetNameSafe(Body->GetSkeletalMeshAsset()));
			return;
		}
		if (UMaterialInterface* Material = Cast<UMaterialInterface>(Path.TryLoad()))
		{
			Body->SetMaterial(Index, Material);
		}
		else
		{
			UE_LOG(LogBN, Warning, TEXT("BNCharacter: team colour material '%s' would not load — slot '%s' keeps its shipped material."),
				*Path.ToString(), *SlotName.ToString());
		}
	};

	ApplySlot(TorsoSlotName, TorsoPath);
	ApplySlot(HeadLegsSlotName, HeadLegsPath);
}
