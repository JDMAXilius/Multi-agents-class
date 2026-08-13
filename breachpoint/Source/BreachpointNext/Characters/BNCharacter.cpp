#include "Characters/BNCharacter.h"
#include "AbilitySystem/BNAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Effects/BNGameplayEffects.h"
#include "Core/BNGameplayTags.h"
#include "Match/BNPlayerState.h"
#include "Weapons/BNEquipmentComponent.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ABNCharacter::ABNCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// The camera rides the MESH, not the capsule — the animation set is full-body first
	// person, so the animated body carries the view. Rotation still comes from the controller.
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(GetMesh(), CameraAttachSocket);
	CameraComponent->bUsePawnControlRotation = true;

	EquipmentComponent = CreateDefaultSubobject<UBNEquipmentComponent>(TEXT("EquipmentComponent"));

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

	if (!MoveSpeedChangedHandle.IsValid())
	{
		MoveSpeedChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBNAttributeSet::GetMoveSpeedAttribute())
			.AddUObject(this, &ABNCharacter::OnMoveSpeedChanged);
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
