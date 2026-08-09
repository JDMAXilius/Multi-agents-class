// Copyright Zollpa LLC.

#include "ZoransCharacterBase.h"
#include "Components/ZoransCharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Data/CharacterData.h"
#include "ZoransResistance/AbilitySystem/GameplayAbilitySystemFunctionLibrary.h"
#include "ZoransResistance/AbilitySystem/Attributes/CharacterAttributes.h"
#include "ZoransResistance/AbilitySystem/Attributes/HealthAttributes.h"
#include "ZoransResistance/AbilitySystem/Attributes/MovementAttributes.h"
#include "ZoransResistance/Game/Data/StaticGameData.h"
#include "ZoransResistance/Characters/Components/ZoransHealthComponent.h"
#include "ZoransResistance/Characters/Components/ZoransHeroComponent.h"
#include "ZoransResistance/Characters/Components/ZoransWeaponComponent.h"
#include "ZoransResistance/Game/ZoransGameMode.h"
#include "ZoransResistance/Game/ZoransGameState.h"
#include "ZoransResistance/Player/ZoransPlayerState.h"

using namespace Zorans_StaticGameData;

AZoransCharacterBase::AZoransCharacterBase(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<UZoransCharacterMovementComponent>(CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotiongWarpingComponent"));
	
	HeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(GetMesh(), PartSocketNames::Head);
	HeadMesh->SetLeaderPoseComponent(GetMesh());
	
	SpringArmComponent = CreateOptionalDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));

	if (SpringArmComponent.Get())
	{
		SpringArmComponent->SetupAttachment(RootComponent);
		SpringArmComponent->bUsePawnControlRotation = true;

		SpringArmComponent->bEnableCameraLag = true;
		SpringArmComponent->CameraLagSpeed = 5.0f;
		
		CameraComponent = CreateOptionalDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
		CameraComponent->SetupAttachment(SpringArmComponent.Get());
		CameraComponent->SetRelativeLocation(FVector(-300.0f, 0.0f, 75.0f));
	}

	BoneSectionMap.Add(BodyPoints::Point_Head, FBoneList(TArray<FName>{"head", "neck_01", "neck_02"}));
	BoneSectionMap.Add(BodyPoints::Point_Torso, FBoneList(TArray<FName>{"pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "clavicle_l", "clavicle_r"}));
	BoneSectionMap.Add(BodyPoints::Point_Arm_Left, FBoneList(TArray<FName>{"upperarm_l", "lowerarm_l", "hand_l"}));
	BoneSectionMap.Add(BodyPoints::Point_Arm_Right, FBoneList(TArray<FName>{"upperarm_r", "lowerarm_r", "hand_r"}));
	BoneSectionMap.Add(BodyPoints::Point_Leg_Left, FBoneList(TArray<FName>{"thigh_l", "calf_l", "foot_l"}));
	BoneSectionMap.Add(BodyPoints::Point_Leg_Right, FBoneList(TArray<FName>{"thigh_r", "calf_r", "foot_r"}));

	BoneDamageSections.Add(BodyPoints::Point_Head, FBoneDamageMods(BodySections::Section_Head, 1.5f));
	BoneDamageSections.Add(BodyPoints::Point_Torso, FBoneDamageMods(BodySections::Section_Torso, 1.f));
	BoneDamageSections.Add(BodyPoints::Point_Arm_Left, FBoneDamageMods(BodySections::Section_Arm_Left, 0.8f));
	BoneDamageSections.Add(BodyPoints::Point_Arm_Right, FBoneDamageMods(BodySections::Section_Arm_Right, 0.8f));
	BoneDamageSections.Add(BodyPoints::Point_Leg_Left, FBoneDamageMods(BodySections::Section_Leg_Left, 0.7f));
	BoneDamageSections.Add(BodyPoints::Point_Leg_Right, FBoneDamageMods(BodySections::Section_Leg_Right, 0.7f));

	CharacterRewindData.Reserve(RewindDataCount);
}

void AZoransCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AZoransCharacterBase, TeamIndex);
	DOREPLIFETIME_CONDITION(AZoransCharacterBase, bIsRespawn, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AZoransCharacterBase, AimVector, COND_SkipOwner);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransCharacterBase, bHologram, COND_None, REPNOTIFY_Always);
}

void AZoransCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (GetWorld() && IsPlayerControlled() && IsLocallyControlled())
	{
		AimVectorDelegate.BindUObject(this, &AZoransCharacterBase::CalculateAimVector);
		GetWorldTimerManager().SetTimer(AimVectorTimer, AimVectorDelegate, AimVectorUpdateRate, true, 1.f);
	}

	if (GetNetMode() < NM_Client)
	{
		CharacterRewindDelegate.BindUObject(this, &AZoransCharacterBase::AddRewindData);
		GetWorldTimerManager().SetTimer(CharacterRewindTimer, CharacterRewindDelegate, RewindDataSaveFrequency, true, 0.f);
	}

	if (GetZoransPlayerState())
	{
		GetZoransPlayerState()->OnPlayerTeamChange.AddUniqueDynamic(this, &AZoransCharacterBase::PlayerStateTeamChange);
	}
}

void AZoransCharacterBase::SetTeamAssignment(const int32 InIndex)
{
	if (ITeamStateInterface* TeamStateInterface = Cast<ITeamStateInterface>(GetPlayerState()))
	{
		TeamStateInterface->SetTeamAssignment(InIndex);
	}

	TeamIndex = InIndex;
}

int32 AZoransCharacterBase::GetTeamAssignment() const
{
	return TeamIndex;

	/*
	if (const ITeamStateInterface* TeamStateInterface = Cast<ITeamStateInterface>(GetPlayerState()))
	{
		return TeamStateInterface->GetTeamAssignment();
	}

	return -1;
	*/
}

void AZoransCharacterBase::PlayerStateTeamChange()
{
	if (IsValid(GetZoransPlayerState()))
	{
		TeamIndex = GetZoransPlayerState()->GetTeamAssignment();
	}
}

void AZoransCharacterBase::CheckComponentRequirements()
{
	if (!bCharacterReadyForPlay)
	{
		if (AbilitySystemComponent.Get() && HealthComponent.Get() && HeroComponent.Get() && WeaponComponent.Get())
		{
			if (HealthComponent->IsComponentInitialized() && HeroComponent->IsComponentInitialized() && WeaponComponent->IsComponentInitialized())
			{
				bCharacterReadyForPlay = true;

				if (UZoransCharacterMovementComponent* MovementComponent = Cast<UZoransCharacterMovementComponent>(GetMovementComponent()))
				{
					MovementComponent->BindMovementAttributes(AbilitySystemComponent.Get());
				}
				
				On_CharacterReady();
			}
		}
	}
}

UZoransCharacterMovementComponent* AZoransCharacterBase::GetZoransCharacterMovementComponent()
{
	return static_cast<UZoransCharacterMovementComponent*>(GetCharacterMovement());
}

FBoneDamageMods AZoransCharacterBase::GetDamageModifierFromBone(const FName InBone, FName& OutSectionName) const
{
	FBoneDamageMods DamageMod;

	if (InBone != NAME_None)
	{
		for (auto &x : BoneSectionMap)
		{
			if (x.Value.BoneList.Contains(InBone))
			{
				DamageMod = *BoneDamageSections.Find(x.Key);
				OutSectionName = x.Key;
				break;
			}
		}
	}
	
	return DamageMod;
}

FBoneDamageMods AZoransCharacterBase::GetDamageModifierFromSection(const FName InSection) const
{
	return *BoneDamageSections.Find(InSection);
}

TArray<FName> AZoransCharacterBase::GetBoneSections() const
{
	TArray<FName> BoneNames;

	BoneNames.Add(BodyPoints::Point_Head);
	BoneNames.Add(BodyPoints::Point_Torso);
	BoneNames.Add(BodyPoints::Point_Arm_Left);
	BoneNames.Add(BodyPoints::Point_Arm_Right);
	BoneNames.Add(BodyPoints::Point_Leg_Left);
	BoneNames.Add(BodyPoints::Point_Leg_Right);
	
	return BoneNames;
}

void AZoransCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (IsValid(GetZoransPlayerState()))
	{
		//FName CharacterName = GetZoransPlayerState()->GetCurrentLoadout().CharacterSelection;
		AbilitySystemComponent = Cast<UZoransAbilitySystemComponent>(GetZoransPlayerState()->GetAbilitySystemComponent());
		AbilitySystemComponent->InitAbilityActorInfo(GetPlayerState(), this);
		
		HealthComponent = GetZoransPlayerState()->GetHealthComponent();
		HeroComponent = GetZoransPlayerState()->GetHeroComponent();
		WeaponComponent = GetZoransPlayerState()->GetWeaponComponent();

		//if (GetZoransPlayerState()->IsABot() && !bIsRespawn)
		//{
			//ensure(CharacterDataTable);

			// if (!CharacterDataTable->GetRowNames().Contains(CharacterName))
			// {
			// 	CharacterName = "Default";
			// }
			
			//if (FCharacterData* CharacterData = CharacterDataTable->FindRow<FCharacterData>("Default", ""))
			//{
				//CharacterData->AbilitySystemData.GrantedAttributes.Add(UHealthAttributes::StaticClass());
				//CharacterData->AbilitySystemData.GrantedAttributes.Add(UCharacterAttributes::StaticClass());
				//CharacterData->AbilitySystemData.GrantedAttributes.Add(UMovementAttributes::StaticClass());
				
				//UGameplayAbilitySystemFunctionLibrary::GrantAbilitySystemData(this, GetPlayerState(), GetPlayerState(), CharacterData->AbilitySystemData);
			//}
			
			// if (const AZoransGameMode* GameMode = Cast<AZoransGameMode>(GetWorld()->GetAuthGameMode()))
			// {
			// 	UGameplayAbilitySystemFunctionLibrary::GrantAbilitySystemData(this, GetPlayerState(), GetPlayerState(), GameMode->AbilitySystemData);
			// }
		//}
		// if (!GetZoransPlayerState()->IsABot() && bIsRespawn)
		// {
		// 	const float MaxHealth = AbilitySystemComponent->GetNumericAttribute(UHealthAttributes::GetMaximumHealthAttribute());
		// 	const float MaxShield = AbilitySystemComponent->GetNumericAttribute(UHealthAttributes::GetMaximumShieldAttribute());
		// 	const float ChargeBaseValue = AbilitySystemComponent->GetNumericAttributeBase(UCharacterAttributes::GetMaximumKineticChargeAttribute());
		//
		// 	AbilitySystemComponent->SetNumericAttributeBase(UHealthAttributes::GetCurrentHealthAttribute(), MaxHealth);
		// 	AbilitySystemComponent->SetNumericAttributeBase(UHealthAttributes::GetCurrentShieldAttribute(), MaxShield);
		// 	AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributes::GetCurrentKineticChargeAttribute(), ChargeBaseValue * 0.5f);
		// }

		// const float MaxHealth = AbilitySystemComponent->GetNumericAttribute(UHealthAttributes::GetMaximumHealthAttribute());
		// const float MaxShield = AbilitySystemComponent->GetNumericAttribute(UHealthAttributes::GetMaximumShieldAttribute());
		// const float ChargeBaseValue = AbilitySystemComponent->GetNumericAttributeBase(UCharacterAttributes::GetMaximumKineticChargeAttribute());
		//
		// AbilitySystemComponent->SetNumericAttributeBase(UHealthAttributes::GetCurrentHealthAttribute(), MaxHealth);
		// AbilitySystemComponent->SetNumericAttributeBase(UHealthAttributes::GetCurrentShieldAttribute(), MaxShield);
		// AbilitySystemComponent->SetNumericAttributeBase(UCharacterAttributes::GetCurrentKineticChargeAttribute(), ChargeBaseValue * 0.5f);
		
		const FTimerDelegate DelayTimerDelegate = FTimerDelegate::CreateUObject(this, &AZoransCharacterBase::InitCharacterComponents);
		GetWorldTimerManager().SetTimerForNextTick(DelayTimerDelegate);
	}
}

void AZoransCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (GetZoransPlayerState())
	{
		AbilitySystemComponent = Cast<UZoransAbilitySystemComponent>(GetZoransPlayerState()->GetAbilitySystemComponent());
		AbilitySystemComponent->InitAbilityActorInfo(GetPlayerState(), this);
		
		HealthComponent = GetZoransPlayerState()->GetHealthComponent();
		HeroComponent = GetZoransPlayerState()->GetHeroComponent();
		WeaponComponent = GetZoransPlayerState()->GetWeaponComponent();
		
		const FTimerDelegate DelayTimerDelegate = FTimerDelegate::CreateUObject(this, &AZoransCharacterBase::InitCharacterComponents);
		GetWorldTimerManager().SetTimerForNextTick(DelayTimerDelegate);
	}
}

void AZoransCharacterBase::InitCharacterComponents()
{
	HealthComponent->InitHealthComponent(this);
	HeroComponent->InitHeroComponent(this);
	WeaponComponent->InitWeaponComponent(this);
}

void AZoransCharacterBase::On_CharacterReady()
{
	if (IsValid(GetZoransPlayerState()))
	{
		GetZoransPlayerState()->OnPlayerTeamChange.AddUniqueDynamic(this, &AZoransCharacterBase::On_TeamChange);
	}

	if (AZoransGameState* ZoransGameState = Cast<AZoransGameState>(GetWorld()->GetGameState()))
	{
		ZoransGameState->OnTeamAssignmentsUpdated.AddUniqueDynamic(this, &AZoransCharacterBase::On_TeamChange);
	}
	
	On_CharacterReadyForPlay();
	
	if (OnCharacterReadyForPlay.IsBound())
	{
		OnCharacterReadyForPlay.Broadcast();
	}
	
	if (GetTeamAssignment() >= 0)
	{
		On_TeamChange();
	}
}

void AZoransCharacterBase::Destroyed()
{
	if (AbilitySystemComponent.Get())
	{
		AbilitySystemComponent->CancelAbilitiesOnDeath();
	}
	
	OnCharacterReadyForPlay.Clear();
	
	Super::Destroyed();
}

void AZoransCharacterBase::CalculateAimVector_Implementation()
{
	// native function. should be overridden in Player using PlayerController and in AI using some other logic (Target vector most likely)
}

void AZoransCharacterBase::AddRewindData()
{
	if (GetWorld() && GetWorld()->GetGameState())
	{
		FCharacterRewindData NewRewindData;
		NewRewindData.Timestamp = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
		NewRewindData.CharacterTransform = GetActorTransform();
		NewRewindData.CharacterVelocity = GetVelocity();
		NewRewindData.CharacterControlRotation = GetControlRotation();
	}

	//UE_LOG(LogTemp, Warning, TEXT("NewRewindData -> Timestamp(%f)  :: Location(%s) ::  RotationZ(%f)  ::  AimPitch(%f)  ::  Velocity(%s)  ::  Speed(%f)"), NewRewindData.Timestamp, *NewRewindData.CharacterTransform.GetLocation().ToString(), NewRewindData.CharacterTransform.GetRotation().Z, NewRewindData.CharacterControlRotation.Pitch, *NewRewindData.CharacterVelocity.GetSafeNormal().ToString(), NewRewindData.CharacterVelocity.Length());
	//DrawDebugCapsule(GetWorld(), NewRewindData.CharacterTransform.GetLocation(), GetSimpleCollisionHalfHeight(), GetSimpleCollisionRadius(), FQuat::Identity, FColor::Green, false, RewindDataSaveFrequency, 0, 3.f);
	//const FVector Direction = NewRewindData.CharacterVelocity.GetSafeNormal();
	//const float Distance = NewRewindData.CharacterVelocity.Length() * RewindDataSaveFrequency;
	//const FVector Offset = Direction * Distance;
	
	//DrawDebugSphere(GetWorld(), NewRewindData.CharacterTransform.GetLocation() + Offset, 32.f, 8, FColor::Red, false, RewindDataSaveFrequency, 0, 3.f);
}

FRotator AZoransCharacterBase::GetRotationFromAimVector() const
{
	FRotator OutRot = GetControlRotation();
	
	if (const USkeletalMeshComponent* SkeletalMesh = GetMesh())
	{
		const FVector SourceLocation = SkeletalMesh->GetSocketLocation(BodyPoints::Point_Torso);
		const FVector TargetDirection = (AimVector - SourceLocation).GetSafeNormal();
		OutRot = TargetDirection.Rotation();
	}
	
	return OutRot;
}

FColor AZoransCharacterBase::GetTeamColor() const
{
	if (AZoransGameState* GameState = Cast<AZoransGameState>(GetWorld()->GetGameState()))
	{
		if (GameState->TeamDefinitions.IsValidIndex(GetTeamAssignment()))
		{
			return GameState->TeamDefinitions[GetTeamAssignment()].TeamColor;
		}
	}

	return FColor::Silver;
}

FLinearColor AZoransCharacterBase::GetTeamColorWithFallback(const FLinearColor FallbackColor) const
{
	if (TeamIndex >= 0)
	{
		if (AZoransGameState* GameState = Cast<AZoransGameState>(GetWorld()->GetGameState()))
		{
			if (!GameState->GetGameModeSettings().bFreeForAll && GameState->TeamDefinitions.IsValidIndex(GetTeamAssignment()))
			{
				return GameState->TeamDefinitions[GetTeamAssignment()].TeamColor.ReinterpretAsLinear();
			}
		}
	}
	
	return FallbackColor;
}

AZoransPlayerState* AZoransCharacterBase::GetZoransPlayerState()
{
	if (!ZoransPlayerState.Get())
	{
		if (APlayerState* PS = GetPlayerState())
		{
			if (AZoransPlayerState* ZPS = Cast<AZoransPlayerState>(PS))
			{
				ZoransPlayerState = ZPS;
			}
		}
	}

	return ZoransPlayerState.Get();
}

bool AZoransCharacterBase::Internal_CharacterHitReaction(const FVector InDirection, const float InForce, const int32 InHitType, const bool bHasPersonalShield, const FVector HitLocation)
{
	if (GetWorld())
	{
		if (const AGameStateBase* GameState = GetWorld()->GetGameState())
		{
			const float CurrentTime = GameState->GetServerWorldTimeSeconds();
			if (CurrentTime >= HitReactionTime)
			{
				HitReactionTime = CurrentTime + HitReactionFrequency;

				if (GetNetMode() < NM_Client)
				{
					MTC_CharacterHitReaction(InDirection, InForce, InHitType, bHasPersonalShield, HitLocation);
					CharacterHitReaction(InDirection, InForce, InHitType, bHasPersonalShield, HitLocation);
				}
				else
				{
					CharacterHitReaction(InDirection, InForce, InHitType, bHasPersonalShield, HitLocation);
				}
				
				return true;
			}
		}
	}
	return false;
}

void AZoransCharacterBase::SetCameraSensitivityOverride(const float InOverrideValue)
{
	CameraSensitivityOverride = InOverrideValue;
}

void AZoransCharacterBase::SetCameraSensitivityDynamicMultiplier(const float InMultiplierValue)
{
	CameraSensitivityDynamicMultiplier = InMultiplierValue;
}

void AZoransCharacterBase::MTC_CharacterHitReaction_Implementation(const FVector InDirection, const float InForce, const int32 InHitType, const bool bHasPersonalShield, const FVector HitLocation)
{
	if (!IsLocallyControlled())
	{
		CharacterHitReaction(InDirection, InForce, InHitType, bHasPersonalShield, HitLocation);
	}
}

void AZoransCharacterBase::PlayFootstepSound(USoundBase* Sound, const FVector& Location, const FRotator& Rotation, float VolumeMultiplier, float PitchMultiplier)
{
	if (!IsValid(Sound))
	{
		return;
	}

	// Play sound locally on the owning client/server
	UGameplayStatics::PlaySoundAtLocation(
		this,
		Sound,
		Location,
		Rotation,
		VolumeMultiplier,
		PitchMultiplier
	);

	// If we want other clients to hear this, multicast it
	// Only multicast if we're on server/authority (dedicated server or listen server)
	if (GetNetMode() < NM_Client)
	{
		MTC_PlayFootstepSound(Sound, Location, Rotation, VolumeMultiplier, PitchMultiplier);
	}
}

void AZoransCharacterBase::MTC_PlayFootstepSound_Implementation(USoundBase* Sound, const FVector& Location, const FRotator& Rotation, float VolumeMultiplier, float PitchMultiplier)
{
	// Only play on remote clients (not on the one that triggered it)
	if (!IsLocallyControlled() && IsValid(Sound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			Sound,
			Location,
			Rotation,
			VolumeMultiplier,
			PitchMultiplier
		);
	}
}

void AZoransCharacterBase::OnRep_bHologram()
{
	ToggleHologramVisuals(bHologram);
}

void AZoransCharacterBase::ToggleHologramState(const bool bActive)
{
	if (bHologram != bActive)
	{
		bHologram = bActive;
		OnRep_bHologram();
	}
}
