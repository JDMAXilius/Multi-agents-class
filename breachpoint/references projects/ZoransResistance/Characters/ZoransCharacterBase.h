// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "ZoransResistance/AbilitySystem/ZoransAbilitySystemComponent.h"
#include "Components/ComponentInterface.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"
#include "ZoransResistance/Teams/TeamStateInterface.h"
#include "ZoransCharacterBase.generated.h"

class AZoransPlayerState;
class UZoransHeroComponent;
class UZoransHealthComponent;
class UZoransAbilitySystemComponent;
class USpringArmComponent;
class UCameraComponent;
class UZoransWeaponComponent;
class AZoransWeaponActor;
class UZoransCharacterMovementComponent;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterReadyForPlayDelegate);
DECLARE_DELEGATE(FAimVectorDelegate);
DECLARE_DELEGATE(FCharacterRewindDelegate);

USTRUCT(BlueprintType)
struct FBoneDamageMods
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	FName BoneDetachPoint = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	float DamageModifier = 1.f;

	FBoneDamageMods() {}

	FBoneDamageMods(const FName InBoneDetachPoint, const float InDamageModifier)
	{
		BoneDetachPoint = InBoneDetachPoint;
		DamageModifier = InDamageModifier;
	}
};

USTRUCT(BlueprintType)
struct FBoneList
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TArray<FName> BoneList;

	FBoneList() {}
	
	FBoneList(const TArray<FName>& InBoneNames)
	{
		BoneList = InBoneNames;
	}
};

UCLASS(NotBlueprintable, Abstract)
class ZORANSRESISTANCE_API AZoransCharacterBase : public ACharacter, public IAbilitySystemInterface, public IComponentInterface, public ITeamStateInterface
{
	GENERATED_BODY()

public:

	virtual void BeginPlay() override;
	
	AZoransCharacterBase(const FObjectInitializer& ObjectInitializer);
		
	virtual void SetTeamAssignment(int32 InIndex) override;

	virtual int32 GetTeamAssignment() const override;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Character")
	int32 TeamIndex = -1;

	UFUNCTION()
	void PlayerStateTeamChange();
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override
	{
		return AbilitySystemComponent.Get();
	}
	
	virtual UZoransHealthComponent* GetHealthComponent() const override
	{
		return HealthComponent.Get();
	}

	virtual UZoransWeaponComponent* GetWeaponComponent() const override
	{
		return WeaponComponent.Get();
	}

	virtual UZoransHeroComponent* GetHeroComponent() const override
	{
		return HeroComponent.Get();
	}

	virtual void CheckComponentRequirements() override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	USkeletalMeshComponent* GetHeadMesh() const
	{
		return HeadMesh.Get();
	}
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Camera")
	UCameraComponent* GetCamera() const
	{
		return CameraComponent.Get();
	}

	UPROPERTY(BlueprintAssignable)
	FOnCharacterReadyForPlayDelegate OnCharacterReadyForPlay;
	
	UFUNCTION(BlueprintImplementableEvent)
	void On_CharacterReadyForPlay();

	UPROPERTY(BlueprintReadOnly, Replicated)
	bool bIsRespawn = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Aiming")
	FVector AimVector = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite)
	float MoveInput_X;

	UPROPERTY(BlueprintReadWrite)
	float MoveInput_Y;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character")
	UZoransCharacterMovementComponent* GetZoransCharacterMovementComponent();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character")
	FBoneDamageMods GetDamageModifierFromBone(const FName InBone, FName& OutSectionName) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character")
	FBoneDamageMods GetDamageModifierFromSection(const FName InSection) const;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character")
	TArray<FName> GetBoneSections() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Aiming")
	void CalculateAimVector();

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe), Category = "Aiming")
	FRotator GetRotationFromAimVector() const;

	UFUNCTION()
	void AddRewindData();
	
	UFUNCTION(BlueprintCallable)
	FColor GetTeamColor() const;

	UFUNCTION(BlueprintCallable)
	FLinearColor GetTeamColorWithFallback(const FLinearColor FallbackColor = FLinearColor(0.9f, 0.1f, 0.f, 1.f)) const;

	UFUNCTION(BlueprintImplementableEvent)
	void On_TeamChange();
	
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Death")
	void BeginDeath(const FGameplayEventData& DeathEvent);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character")
	AZoransPlayerState* GetZoransPlayerState();

	UFUNCTION(BlueprintCallable, Category = "Character")
	bool Internal_CharacterHitReaction(const FVector InDirection, const float InForce, const int32 InHitType, const bool bHasPersonalShield, const FVector HitLocation);

	UFUNCTION(NetMulticast, Unreliable)
	void MTC_CharacterHitReaction(const FVector InDirection, const float InForce, const int32 InHitType, const bool bHasPersonalShield, const FVector HitLocation);

	UFUNCTION(BlueprintImplementableEvent, Category = "Character")
	void CharacterHitReaction(const FVector InDirection, const float InForce, const int32 InHitType, const bool bHasPersonalShield, const FVector HitLocation);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, BlueprintCosmetic, Category = "Character")
	void TriggerCharacterDismemberment(const FName JointName, const FVector RelativeImpulseDirection, const float ImpulseForce = 10000.f);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, BlueprintCosmetic, Category = "Character")
	void TriggerCharacterRagdoll(const FVector RelativeImpulseDirection, const FName BoneName, const float ImpulseForce = 50000.f);

	UFUNCTION(BlueprintCallable, Category = "Character")
	void SetCameraSensitivityOverride(const float InOverrideValue = 1.f);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character")
	float GetCameraSensitivityOverride() const { return CameraSensitivityOverride; }

	UFUNCTION(BlueprintCallable, Category = "Character")
	void SetCameraSensitivityDynamicMultiplier(const float InMultiplierValue = 1.f);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character")
	float GetCameraSensitivityDynamicMultiplier() const { return CameraSensitivityDynamicMultiplier; }

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, BlueprintCosmetic, Category = "Character")
	void PlayThrowdownAudio(const FName InThrowdownRow = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Character")
	void PlayFootstepSound(USoundBase* Sound, const FVector& Location, const FRotator& Rotation, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	UFUNCTION(NetMulticast, Unreliable, Category = "Character")
	void MTC_PlayFootstepSound(USoundBase* Sound, const FVector& Location, const FRotator& Rotation, float VolumeMultiplier, float PitchMultiplier);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character")
	UMotionWarpingComponent* GetMotionWarpingComponent() { return MotionWarpingComponent; }

	UFUNCTION()
	void OnRep_bHologram();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Character")
	void ToggleHologramState(const bool bActive);

	UFUNCTION(BlueprintImplementableEvent, Category = "Character")
	void ToggleHologramVisuals(const bool bActive);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Character")
	void ToggleParodyHologram(const FName ParodyRowName, const bool bActive);
	
protected:
	bool bCharacterReadyForPlay = false;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Character")
	UMotionWarpingComponent* MotionWarpingComponent = nullptr;

	TWeakObjectPtr<UZoransAbilitySystemComponent> AbilitySystemComponent = nullptr;
	TWeakObjectPtr<UZoransHealthComponent> HealthComponent = nullptr;
	TWeakObjectPtr<UZoransHeroComponent> HeroComponent = nullptr;
	TWeakObjectPtr<UZoransWeaponComponent> WeaponComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Character")
	UDataTable* CharacterDataTable = nullptr;
	
	virtual void PossessedBy(AController* NewController) override;
	
	virtual void OnRep_PlayerState() override;

	UFUNCTION()
	void InitCharacterComponents();
	
	UFUNCTION()
	virtual void On_CharacterReady();
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Bone Mapping")
	TMap<FName, FBoneList> BoneSectionMap;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Bone Mapping")
	TMap<FName, FBoneDamageMods> BoneDamageSections; 
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Character Mesh")
	TObjectPtr<USkeletalMeshComponent> HeadMesh = nullptr;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArmComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> CameraComponent = nullptr;

	virtual void Destroyed() override;

	FTimerHandle AimVectorTimer;
	FAimVectorDelegate AimVectorDelegate;
	FTimerHandle CharacterRewindTimer;
	FCharacterRewindDelegate CharacterRewindDelegate;

	TWeakObjectPtr<AZoransPlayerState> ZoransPlayerState;

	float HitReactionTime = 0.f;

	TArray<FCharacterRewindData> CharacterRewindData;

	float CameraSensitivityOverride = -1.f;
	float CameraSensitivityDynamicMultiplier = 1.f;

	UPROPERTY(BlueprintReadWrite, ReplicatedUsing="OnRep_bHologram")
	bool bHologram = false;

private:
	static inline float AimVectorUpdateRate = 0.1f;
	static inline float HitReactionFrequency = 0.33f;
	static inline float RewindDataSaveFrequency = 0.25f;
	static inline int8 RewindDataCount = 8;
};