#include "Characters/BNCharacter.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/BNGA_Death.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "Characters/BNHealthComponent.h"
#include "Core/BNCollision.h"
#include "Core/BNGameplayTags.h"
#include "Data/BNDataRows.h"
#include "Match/BNPlayerState.h"
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

ABNCharacter::ABNCharacter()
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

	// Bound on every machine; the handler is what gates on authority. The delegate lives on a
	// component this character owns, so it dies with the body and leaks nothing onto the
	// persistent ability system component.
	HealthComponent->OnDeath.AddUObject(this, &ABNCharacter::HandleDeath);
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
}

void ABNCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitializeAbilitySystem();
	InitializeAnimLayer();
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
		UE_LOG(LogBN, Log, TEXT("BNCharacter: linked anim layer %s (Lyra=%s)."),
			*GetNameSafe(LayerClass), UsesLyraAnim() ? TEXT("yes") : TEXT("no"));
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

bool ABNCharacter::UsesLyraAnim() const
{
	const UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	return Anim && Anim->IsA(UBNLAnimInstance::StaticClass());
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
	if (UsesLyraAnim())
	{
		const ABNWeapon* Weapon = EquipmentComponent ? EquipmentComponent->GetCurrentWeapon() : nullptr;
		return ResolveLyraLayerForRow(Weapon ? Weapon->GetRowName() : NAME_None);
	}

	if (UClass* WeaponLayer = GetCurrentWeaponAnimLayer())
	{
		return WeaponLayer;
	}

	if (!bUnarmedAnimLayerResolveAttempted)
	{
		bUnarmedAnimLayerResolveAttempted = true;
		CachedUnarmedAnimLayer = UnarmedAnimLayer.IsNull() ? nullptr : UnarmedAnimLayer.TryLoadClass<UAnimInstance>();
	}
	return CachedUnarmedAnimLayer;
}
