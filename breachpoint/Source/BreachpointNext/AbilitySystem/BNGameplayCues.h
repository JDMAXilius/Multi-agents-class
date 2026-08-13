#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "BNGameplayCues.generated.h"

class UFXSystemAsset;

/**
 * Weapon FX, law 2: every effect this game shows comes through a cue, and every cue handler is a
 * C++ class. Each one degrades SILENTLY when its Effect is unset — the FX assets are ANNOUNCED,
 * not created by this packet, and an unset one must cost nothing at runtime and log nothing.
 *
 * TSoftObjectPtr<UFXSystemAsset> rather than a Niagara type: UFXSystemAsset is the Engine-module
 * base of both systems, so no public header names a Niagara type and Niagara stays a PRIVATE
 * module dependency.
 */
UCLASS(Abstract)
class BREACHPOINTNEXT_API UBNGameplayCue_Base : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	/** The tag this handler answers to. Assigned here rather than in the constructor because
	 *  native tags are not guaranteed registered while CDOs are being built. */
	virtual void PostInitProperties() override;

	/** The one native tag this class handles. Public because the registrar asks the CDO for it at
	 *  world begin-play — long after the tag tree exists — rather than trusting what PostInitProperties
	 *  managed to resolve during CDO construction. A cue that registered under an invalid tag is a
	 *  cue nothing ever calls, and it fails silently. */
	virtual FGameplayTag GetHandledCueTag() const { return FGameplayTag(); }

protected:
	/** Null when the asset is unset or fails to load — callers just do nothing. */
	static UFXSystemAsset* Resolve(const TSoftObjectPtr<UFXSystemAsset>& Soft);

	/** The WEAPON's own muzzle socket, never the character's (MyCharacter.h:120-124,
	 *  .cpp:1557-1580). Falls back to the target's transform when SourceObject is not a weapon. */
	static FTransform ResolveMuzzle(const AActor* Target, const FGameplayCueParameters& Parameters);

	static void SpawnAt(const UObject* WorldContext, UFXSystemAsset* Asset, const FVector& Location, const FRotator& Rotation, FName VectorParameterName, const FVector& VectorParameterValue);
};

UCLASS(Config = Game, meta = (DisplayName = "GC_BN_Weapon_MuzzleFlash"))
class BREACHPOINTNEXT_API UBNGameplayCue_MuzzleFlash : public UBNGameplayCue_Base
{
	GENERATED_BODY()

public:
	virtual FGameplayTag GetHandledCueTag() const override;
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<UFXSystemAsset> Effect;
};

UCLASS(Config = Game, meta = (DisplayName = "GC_BN_Weapon_Impact"))
class BREACHPOINTNEXT_API UBNGameplayCue_Impact : public UBNGameplayCue_Base
{
	GENERATED_BODY()

public:
	virtual FGameplayTag GetHandledCueTag() const override;
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<UFXSystemAsset> Effect;
};

UCLASS(Config = Game, meta = (DisplayName = "GC_BN_Weapon_Tracer"))
class BREACHPOINTNEXT_API UBNGameplayCue_Tracer : public UBNGameplayCue_Base
{
	GENERATED_BODY()

public:
	virtual FGameplayTag GetHandledCueTag() const override;
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:

	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	TSoftObjectPtr<UFXSystemAsset> Effect;

	/** The contract with the FX author: the tracer system must expose a user Vector parameter of
	 *  this name and draw its beam from the emitter to it. ANNOUNCED alongside the asset. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "BN|Cue")
	FName BeamEndParameter = TEXT("BeamEnd");
};

/**
 * Native cue handlers are NOT discovered on their own: UGameplayCueManager builds its runtime set
 * by scanning CONTENT paths for cue assets, and a C++ class lives in no content path. Without this
 * every cue above would be a class nothing ever calls. One pass at world begin-play registers them.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGameplayCueRegistrar : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
