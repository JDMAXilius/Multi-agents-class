// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ZoransWeaponData.h"
#include "ZoransResistance/AbilitySystem/Data/AbilitySystemData.h"
#include "ZoransResistance/Projectiles/ImpaleActorBase.h"
#include "ZoransResistance/Widgets/ReticleWidgetBase.h"
#include "ZoransWeaponActor.generated.h"

class UDataTable;
class AZoransCharacterBase;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadedAmmoChangeDelegate, int32, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLocalAmmoChangeDelegate, int32, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadedAmmoEmpty);

UCLASS()
class ZORANSRESISTANCE_API AZoransWeaponActor : public AActor
{
	GENERATED_BODY()

public:
	
	AZoransWeaponActor();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	USkeletalMeshComponent* GetWeaponMesh() const;
	
	// This will broadcast the hitscan data to all connected players EXCEPT the local shooter unless they are the listen server host. Local shooter calls WeaponCue_WithHitscanData directly with the same Hitscan data they send to the server through target data
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Weapon")
	void TriggerWeaponCue_WithHitscanData(const TArray<FHitscanData>& Hits);

	// This is for cosmetics, like muzzle flash, shell ejection and sound. Does not require other data, as it can be used in many ways
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Weapon")
	void WeaponEffect_Local();

	void InternalWeaponCue_WithHitscanData(const TArray<FHitscanData>& Hits);

	// Implementing this in a weapon is used for receiving other player events -or- the local shooter hit data
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, BlueprintCosmetic, Category = "Weapon")
	void WeaponCue_WithHitscanData(const TArray<FHitscanData>& Hits);

	// Used for enabling toggled effects, like fully automatic fire. This function calls WeaponCue_Activated(true) and WeaponCue_Deactivated(false)
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ToggleActiveWeaponCue(const bool bActive = false);

	UPROPERTY(ReplicatedUsing = "OnRep_bCueActive")
	bool bCueActive = false;

	// Used for state based events, like fully automatic weapon fire so we do not have to broadcast repeated events. Also allows for smoother results despite connection/latency
	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void WeaponCue_Activated();

	// This function should typically disable anything from WeaponCue_Activated. Use this to stop looping timers, continuous particles, sounds etc.
	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void WeaponCue_Deactivated();

	void SetOwningCharacter(AZoransCharacterBase* InOwningCharacter);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	AZoransCharacterBase* GetOwningCharacter();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Weapon")
	FTransform GetWeaponMuzzleTransform() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Weapon")
	FTransform GetWeaponHandPlacementTransform() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Weapon")
	FVector GetWeaponTracePoint() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	TSubclassOf<AImpaleActorBase> GetImpaleActorClass() const { return ImpaleActorClass; }

	UFUNCTION(BlueprintCallable, meta = (HidePin = "Target", DefaultToSelf = "Target"), Category = "Weapon")
	void ApplyRecoil(const float Modifier = 1.f, const bool bCheckAmmo = true);

	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (HidePin = "Target", DefaultToSelf = "Target"), Category = "Weapon")
	float GetCurrentRecoil() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (HidePin = "Target", DefaultToSelf = "Target"), Category = "Weapon")
	float GetCurrentSpread() const;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ToggleWeaponAiming(const FGameplayTag InTag, int32 TagCount);

	void ToggleCrouching(const FGameplayTag InTag, int32 TagCount);

	void InternalReadyWeapon();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Weapon")
	void ReadyWeapon();

	void InternalStowWeapon(const bool bForceStow = false);
	
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Weapon")
	void StowWeapon();

	UFUNCTION()
	void OnRep_LoadedAmmo();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	bool TryUseLoadedAmmo(const int32 Amount = 1);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ConsumeLocalAmmo(const int32 Amount = 1);

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnLoadedAmmoChangeDelegate OnLoadedAmmoChangeDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnLocalAmmoChangeDelegate OnLocalAmmoChangeDelegate;
	
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnLoadedAmmoEmpty OnLoadedAmmoEmpty;

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void LoadedAmmoChanged(const int32 NewValue);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	void SetLoadedAmmo(const int32 InAmmo);
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	int32 GetLoadedAmmo() { return LoadedAmmo; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	int32 GetLocalAmmo() { return LocalAmmo; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetReticleIndex(const int32 InIndex);

	// Adjusts the current reticle index by +1 or -1, depending on the value of bIncrease
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ModifyReticleIndex(const bool bIncrease);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	UReticleWidgetBase* GetReticleWidget() const { return ReticleWidget.Get(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	bool IsWeaponAiming() { return bIsAiming; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	FZoransWeaponData GetWeaponDefaults() const { return WeaponDefaults; }

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon")
	bool bReceivesServerHitEffect = false;

protected:

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon")
	USkeletalMeshComponent* WeaponMesh = nullptr;

	void BeginPlay() override;

	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool bWeaponReady = false;

	float CurrentSpread = 0.f;
	float CurrentRecoil = 0.f;
	float AimingModifier = 1.f;
	float TargetAimingModifier = 1.f;
	bool bIsAiming = false;
	bool bIsCrouching = false;
	float SpreadFromMovement = 0.f;

	UPROPERTY(ReplicatedUsing = "OnRep_LoadedAmmo")
	int32 LoadedAmmo = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Weapon")
	int32 LocalAmmo = 0;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon")
	USceneComponent* WeaponRoot = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Weapon")
	FDataTableRowHandle WeaponData = FDataTableRowHandle();

	UPROPERTY()
	FZoransWeaponData WeaponDefaults;

	UPROPERTY(Replicated)
	TWeakObjectPtr<AZoransCharacterBase> OwningCharacter;

	TWeakObjectPtr<UReticleWidgetBase> ReticleWidget;
 
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<AImpaleActorBase> ImpaleActorClass;

	UFUNCTION()
	void OnRep_bCueActive();

	FDelegateHandle AimingTagChangedDelegate;
	FDelegateHandle CrouchTagChangedDelegate;
};