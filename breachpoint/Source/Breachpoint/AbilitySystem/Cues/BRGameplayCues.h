// Breachpoint. THE GameplayCue handler library — the classes that make a cue tag play something.
#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"

#include "BRGameplayCues.generated.h"

class UFXSystemAsset;
class USoundBase;

UCLASS(Abstract, meta = (DisplayName = "BR Gameplay Cue"))
class BREACHPOINT_API UBRGameplayCue_Base : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UBRGameplayCue_Base(const FObjectInitializer& ObjectInitializer);

	virtual void GetHandledCueTags(FGameplayTagContainer& OutTags) const {}

	virtual void GetFXAssetPaths(TArray<FSoftObjectPath>& OutPaths) const {}

protected:
	static FVector ResolveCueLocation(const AActor* Target, const FGameplayCueParameters& Parameters);

	bool PlaySoundSoft(const UObject* WorldContext, const TSoftObjectPtr<USoundBase>& SoftSound, const FVector& Location) const;

	bool SpawnFXSoft(const UObject* WorldContext, const TSoftObjectPtr<UFXSystemAsset>& SoftFX, const FVector& Location, const FRotator& Rotation) const;

	void ReportSilentCue(const UObject* WorldContext, const FGameplayCueParameters& Parameters, const FVector& Location, const TCHAR* Slot) const;
};

USTRUCT()
struct FBRWeaponFireCueFX
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Cue")
	TSoftObjectPtr<UFXSystemAsset> MuzzleFlash;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Cue")
	TSoftObjectPtr<UFXSystemAsset> Tracer;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Cue")
	TSoftObjectPtr<USoundBase> Report;
};

UCLASS(Config = Game, meta = (DisplayName = "GC_Weapon_Fire"))
class BREACHPOINT_API UBRGameplayCue_WeaponFire : public UBRGameplayCue_Base
{
	GENERATED_BODY()

public:
	UBRGameplayCue_WeaponFire(const FObjectInitializer& ObjectInitializer);

	virtual void GetHandledCueTags(FGameplayTagContainer& OutTags) const override;
	virtual void GetFXAssetPaths(TArray<FSoftObjectPath>& OutPaths) const override;

	virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const override;

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	UPROPERTY(EditDefaultsOnly, Config, Category = "Breachpoint|Cue")
	TMap<FGameplayTag, FBRWeaponFireCueFX> FXByCueTag;
};

UCLASS()
class BREACHPOINT_API UBRGameplayCueRegistrar : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	static int32 RegisterNativeCueHandlers();

	static void WarmCueFXAssets();
};
