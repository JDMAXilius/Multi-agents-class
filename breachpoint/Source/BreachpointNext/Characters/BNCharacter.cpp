#include "Characters/BNCharacter.h"
#include "BreachpointNext.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/BNGA_Death.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "Characters/BNHealthComponent.h"
#include "Core/BNGameplayTags.h"
#include "Match/BNPlayerState.h"
#include "Weapons/BNEquipmentComponent.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

namespace
{
	// The graph's E_FPST_PoseType ordinals and speed, from MyCharacter's verified port
	// (OnAimStarted/OnAimCompleted): the pose pairs are hip/aim × stand/crouch, scope 0 is
	// the default sight, and 18 is the template's AimPoseChangeSpeed.
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

	/** MyCharacter's ChangePose seam in miniature: the PoseOffsets component is a Blueprint
	 *  class with no C++ base, so the call goes through the UFunction's OWN parameter layout —
	 *  a hand-written mirror struct passed to ProcessEvent is silent memory corruption the day
	 *  the Blueprint's signature changes, which is the exact trap MyCharacter's FBPCall
	 *  documents. Pin names verified against the graph: (InPoseType, InScopeType, InChangeSpeed). */
	bool BNCallChangePose(UActorComponent* Comp, uint8 PoseType, uint8 ScopeType, float ChangeSpeed)
	{
		UFunction* Fn = Comp ? Comp->FindFunction(FName(TEXT("ChangePose"))) : nullptr;
		if (!Fn)
		{
			return false;
		}

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
		return bAllSet;
	}

	/** The Blueprint-owned PoseOffsets component, by name fragment — the SCS node the terminal
	 *  added is named after its class (BPC_FPST_Procedural_PoseOffsets), and instance names may
	 *  carry suffixes, so a Contains match is the robust lookup. */
	UActorComponent* BNFindPoseOffsetsComp(const AActor* Owner)
	{
		for (UActorComponent* Comp : Owner->GetComponents())
		{
			if (Comp && Comp->GetName().Contains(TEXT("PoseOffsets")))
			{
				return Comp;
			}
		}
		return nullptr;
	}
}

ABNCharacter::ABNCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// The camera rides the MESH, not the capsule — the animation set is full-body first
	// person, so the animated body carries the view. Rotation still comes from the controller.
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(GetMesh(), CameraAttachSocket);
	CameraComponent->bUsePawnControlRotation = true;

	EquipmentComponent = CreateDefaultSubobject<UBNEquipmentComponent>(TEXT("EquipmentComponent"));
	HealthComponent = CreateDefaultSubobject<UBNHealthComponent>(TEXT("HealthComponent"));

	bUseControllerRotationYaw = true;

	// True first person: the founder's animation set is full-body — the owner sees their
	// own mannequin (default visibility, no owner-no-see, no separate arms mesh).

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCharacterMovement()->bCanWalkOffLedgesWhenCrouching = true;
}

void ABNCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Layer linking is local-cosmetic and runs on EVERY machine — sim proxies never see a
	// possession event, so BeginPlay is their hook; the mesh's anim instance exists by now.
	InitializeAnimLayer();

	// Bound once per pawn, on every machine; the handler is what gates on authority. The delegate
	// lives on a component this character owns, so it dies with the body and leaks nothing onto
	// the persistent ASC.
	HealthComponent->OnDeath.AddUObject(this, &ABNCharacter::HandleDeath);
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

// The crouch tag's ONE owner: the engine's crouch events, authority-side. Engine crouch
// replicates via compressed flags and these events fire on every machine; the authority
// gate makes the tag server-truth, replicated to everyone by the ASC.
void ABNCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	// No camera offset here: Super moves the mesh by HalfHeightAdjust and the camera is the
	// mesh's child, so the view follows — and the crouch animation lowers the head itself.
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
}

void ABNCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The ASC outlives the pawn (it is the PlayerState's); an unregistered binding per respawn
	// accumulates on it forever. Through the CACHED ASC, never GetAbilitySystemComponent():
	// APawn::UnPossessed() nulls PlayerState before the corpse is destroyed, so the fresh
	// lookup answers null here and the removal this code thought it did never happened.
	if (MoveSpeedChangedHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = CachedAbilitySystem.Get())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBNAttributeSet::GetMoveSpeedAttribute())
				.Remove(MoveSpeedChangedHandle);
		}
		MoveSpeedChangedHandle.Reset();
	}

	// Same cached-ASC route as the delegate above, same reason: the fresh lookup is already null.
	if (ADSPoseTagHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = CachedAbilitySystem.Get())
		{
			ASC->RegisterGameplayTagEvent(BNTags::State_Weapon_ADS, EGameplayTagEventType::NewOrRemoved)
				.Remove(ADSPoseTagHandle);
		}
		ADSPoseTagHandle.Reset();
	}

	// DEBT A2 (crouch critic, 9b59d79): OnEndCrouch never fires when the pawn is DESTROYED, so
	// this GE would stay on the persistent ASC and the next body would spawn permanently tagged
	// Crouching — and a fresh crouch would then stack a second one. Same cached-ASC route as the
	// delegate above, and for the same reason: the PlayerState is already null by now.
	if (CrouchStateHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = CachedAbilitySystem.Get())
		{
			ASC->RemoveActiveGameplayEffect(CrouchStateHandle);
		}
		CrouchStateHandle = FActiveGameplayEffectHandle();
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

	// After GrantDefaults: the weapon's ability set is granted onto the same ASC, so the
	// PlayerState's own grant must already have happened. Authority-gated inside.
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

// Current weapon drives the layer — none today, so the unarmed layer links. Runs wherever
// called; skips silently until the mesh has an anim instance, then links exactly once per class.
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

	// LinkAnimClassLayers never unlinks the outgoing set, so a resolve that yields nothing
	// would leave the previous weapon's layer posing the character on every machine.
	if (LinkedAnimLayerClass)
	{
		MeshComp->UnlinkAnimClassLayers(LinkedAnimLayerClass.Get());
	}
	LinkedAnimLayerClass = LayerClass;

	if (LayerClass)
	{
		MeshComp->LinkAnimClassLayers(LayerClass);

		// The LINKING side of the founder's standing logging order — the anim instance announces
		// what it FOUND linked, this announces what was DELIBERATELY linked and from where. Two
		// independent reports: if they ever disagree, the linking failed silently.
		const FString LayerPath = LayerClass->GetPathName();
		UE_LOG(LogBN, Log, TEXT("BNLink: %s linked layer %s [%s]"),
			*GetName(), *LayerPath,
			LayerPath.Contains(TEXT("/Game/BN/")) ? TEXT("BN DUPLICATE") : TEXT("template original"));
		if (LayerPath.Contains(TEXT("/Game/BN/")))
		{
			UE_LOG(LogBN, Warning,
				TEXT("BNLink: that layer is a BN-owned DUPLICATE. Duplicated layers keep property-access bindings "
					 "compiled against the main ABP's OLD layout and silently read zero after a reparent — the "
					 "14 Aug frozen-aim root cause. Point the weapon row's AnimLayerClass at the FPSTemplate original."));
		}
	}
	else
	{
		UE_LOG(LogBN, Warning,
			TEXT("BNLink: %s resolved NO anim layer class — the weapon row's AnimLayerClass is empty or failed to "
				 "load, and UnarmedAnimLayer is unset. The character will pose from the base ABP alone."),
			*GetName());
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

	// Every machine: Health replicates, so each one reaches zero on its own and the component
	// reports it there — death is never a flag one machine sends to the others.
	HealthComponent->InitializeWithAbilitySystem(ASC);

	if (!MoveSpeedChangedHandle.IsValid())
	{
		MoveSpeedChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBNAttributeSet::GetMoveSpeedAttribute())
			.AddUObject(this, &ABNCharacter::OnMoveSpeedChanged);
	}

	// Every machine, like the health binding above: the ADS tag replicates (Mixed carries
	// GE-granted tags to simulated proxies), so each machine's own listener poses its own view
	// of this character — cosmetics stay per-machine, the template's model exactly.
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
	const bool bADS = NewCount > 0;
	const uint8 PoseType = bADS
		? (bIsCrouched ? BN_Pose_AimCrouch : BN_Pose_AimStand)
		: (bIsCrouched ? BN_Pose_HipCrouch : BN_Pose_HipStand);

	// Scope 0 (default sight) until the weapon row carries a ScopeType column — MyCharacter read
	// it off the weapon (GetCurrentWeaponScopeType); that column is a one-line DT addition later.
	// Logged both ways, on the tag edge: "ADS did nothing" and "ADS was never told" look identical
	// from outside the game, and they are completely different investigations.
	if (BNCallChangePose(BNFindPoseOffsetsComp(this), PoseType, BN_Scope_Default, BN_AimPoseChangeSpeed))
	{
		UE_LOG(LogBN, Log, TEXT("BNPose: %s -> ChangePose(%d) SENT (ADS %s)"),
			*GetName(), PoseType, bADS ? TEXT("on") : TEXT("off"));
	}
	else if (!bPoseCompWarned)
	{
		bPoseCompWarned = true;
		UE_LOG(LogBN, Warning,
			TEXT("BNPose: no callable ChangePose on a PoseOffsets component — ADS narrows the FOV "
				 "but the weapon never rises to the eye. The BPC_FPST_Procedural_PoseOffsets component "
				 "(terminal's R3 pivot) is missing from BP_BNCharacter or its signature changed."));
	}
}

// The current-weapon seam. Null (no weapon, no layer on the row) still means unarmed, and the
// unarmed fallback below stands unchanged — which is why no animation code moved this wave.
UClass* ABNCharacter::GetCurrentWeaponAnimLayer() const
{
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponAnimLayer() : nullptr;
}

UClass* ABNCharacter::ResolveAnimLayerClass()
{
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
