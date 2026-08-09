// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "Abilities/GameplayAbility.h"
#include "ZoransResistance/AbilitySystem/TargetData/ZoransTargetActor.h"
#include "ZoransResistance/InventorySystem/Weapons/ZoransWeaponActor.h"
#include "ZoransGameplayAbility.generated.h"

class AZoransPlayerState;
class UZoransAbilitySystemComponent;
class UInputAction;
class AZoransCharacterBase;
class UZoransAbilityCooldownWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAbilityInputPressedDelegate);

UCLASS()
class ZORANSRESISTANCE_API UZoransGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData) override;

	virtual void PreFirstActivation();
	
	UZoransGameplayAbility();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	AZoransCharacterBase* GetOwningCharacter();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	UZoransAbilitySystemComponent* GetZoransAbilitySystemComponent();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	AZoransWeaponActor* GetOwningWeapon();

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability Details")
	FName AbilityName = NAME_None;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability Details")
	bool bActivateWhenGranted = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability Details")
	bool bRemoveAbilityOnEnd = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability Details")
	float AbilityCost = 0.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability Details")
	float AbilityCooldown = 0.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability Details")
	FGameplayTag AbilityCooldownTag = FGameplayTag();

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability Details")
	FGameplayTag CostIgnoreTag = FGameplayTag();

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability Details|Input")
	UInputAction* ActivationInputAction = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability Details|Input")
	ETriggerEvent ActivationTriggerType = ETriggerEvent::Started;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability Details|Input")
	UInputAction* CancelInputAction = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability Details|Input")
	ETriggerEvent CancelTriggerType = ETriggerEvent::Started;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability Details|User Interface")
	TObjectPtr<UTexture2D> KillfeedIcon = nullptr;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability Details|User Interface")
	TObjectPtr<UTexture2D> UserInterfaceIcon = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability Details|User Interface")
	TObjectPtr<UMaterialInstance> CooldownMaterial = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Ability Details|User Interface")
	TSubclassOf<UZoransAbilityCooldownWidget> ReticleWidgetOverride = nullptr;

	UPROPERTY(BlueprintAssignable)
	FAbilityInputPressedDelegate InputPressedDelegate;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability|Tracing", meta = (HidePin = "Target", DefaultToSelf = "Target"))
	bool IsValidTarget(AActor* InActor);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability|Tracing", meta = (HidePin = "Target", DefaultToSelf = "Target"))
	static ETargetHitType GetHitTypeFromComponent(const UPrimitiveComponent* InComponent);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Target Data|Hitscan")
	static TArray<FHitscanData> ConvertHitResultsToHitscanData(const TArray<FHitResult>& InHits, AActor* InSourceActor, const bool bUsedForTargetData);
	
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	void SetupEnhancedInputBindings(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec);
	
	void CallActivationEvent(const FGameplayAbilityActorInfo* const ActorInfo, const FGameplayAbilitySpec Spec);

	void CallCancelledEvent(const FGameplayAbilityActorInfo* const ActorInfo, const FGameplayAbilitySpec Spec);

	int32 ActivationInputHandle = -1;
	int32 CancelInputHandle = -1;

	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const override;

	float GetWeaponCooldown() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability Details")
	FName GetAbilityName() const { return AbilityName; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ability Metrics")
	void BeginTrackingTimeMetric();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ability Metrics")
	float EndTrackingTimeMetric();

protected:
	TWeakObjectPtr<AZoransCharacterBase> OwningCharacter = nullptr;
	TWeakObjectPtr<UZoransAbilitySystemComponent> ZoransAbilitySystemComponent = nullptr;
	TWeakObjectPtr<AZoransWeaponActor> ZoransWeaponActor = nullptr;

	float MetricTimeStart = 0.f;

#if WITH_EDITOR
	FGameplayTag OldAbilityCooldownTag = FGameplayTag();
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
	UFUNCTION(BlueprintCallable, Category = "Target Data|Hitscan")
	void InternalHandleHitscanData(FGameplayAbilityTargetDataHandle InTargetData);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Target Data|Hitscan")
	void HandleHitscanData(const TArray<FHitscanData>& HitscanData);

	UFUNCTION(BlueprintCallable, Category = "Target Data|Projectile")
	void InternalHandleBallisticProjectileSpawnParams(FGameplayAbilityTargetDataHandle InTargetData);

	UFUNCTION(BlueprintImplementableEvent, Category = "Target Data|Projectile")
	void HandleBallisticProjectileSpawnParams(FBaseProjectileSpawnParams SpawnParams);

	UFUNCTION(BlueprintCallable, Category = "Target Data|Projectile")
	void InternalHandleBouncingProjectileSpawnParams(FGameplayAbilityTargetDataHandle InTargetData);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Target Data|Projectile")
	void HandleBouncingProjectileSpawnParams(FBouncingProjectileSpawnParams SpawnParams);

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability|Tracing")
	bool bTraceEnvironment = true;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability|Tracing")
	bool bTraceEnemy = true;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability|Tracing")
	bool bTraceFriendly = false;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Ability|Tracing")
	int32 PawnHitsPerTrace = 1;

	bool bFirstActivation = true;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability Metrics")
	FDamageMetricValue MergeDamageMetrics(const TArray<FDamageMetricValue>& InDamageMetrics);

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Damage", meta = (HidePin = "Target", DefaultToSelf = "Target"))
	FDamageMetricValue DealDamageToTargetActor(AActor* TargetActor, AActor* InstigatorActor, AActor* OptionalEffectSource, const FHitResult& HitResult, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount, const FGameplayTag DamageTag, bool& bValidHit, const float ImpactForce = 1000.f, const bool bWeakPoint = false, const FZoransWeaponData OptionalWeaponData = FZoransWeaponData());

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Damage", meta = (HidePin = "Target", DefaultToSelf = "Target"))
	void DealDamageToActorUsingHitResult(const FHitResult& HitResult, AActor* InstigatorActor, AActor* OptionalEffectSource, TSubclassOf<UGameplayEffect> DamageEffectClass, float DamageAmount, const FGameplayTag DamageTag);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Damage", meta = (HidePin = "Target", DefaultToSelf = "Target"))
	FDamageMetricValue DealDamageToActorUsingHitscanData(const FHitscanData& HitscanData, const FZoransWeaponData& WeaponData, const TSubclassOf<UGameplayEffect> DamageEffectClass, bool& bValidHit, const float DamageModifier = 1.f);
};

