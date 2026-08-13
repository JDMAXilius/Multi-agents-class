#include "AbilitySystem/BNGameplayCues.h"

#include "Core/BNGameplayTags.h"
#include "Weapons/BNWeapon.h"
#include "Data/BNDataRows.h"
#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectHash.h"

namespace
{
	/** cpp-local so no header names a Niagara type: spawn and hand back the component for
	 *  user-parameter writes. Null when the asset is unset — cues degrade silently by law. */
	UNiagaraComponent* BNSpawnSystem(const UObject* WorldContext, UFXSystemAsset* Asset, const FVector& Location, const FRotator& Rotation)
	{
		UNiagaraSystem* System = Cast<UNiagaraSystem>(Asset);
		if (!WorldContext || !System)
		{
			return nullptr;
		}
		return UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			WorldContext, System, Location, Rotation, FVector(1.f), /*bAutoDestroy=*/true, /*bAutoActivate=*/true);
	}
}

void UBNGameplayCue_Base::PostInitProperties()
{
	Super::PostInitProperties();

	const FGameplayTag Handled = GetHandledCueTag();
	if (Handled.IsValid())
	{
		GameplayCueTag = Handled;
	}
}

UFXSystemAsset* UBNGameplayCue_Base::Resolve(const TSoftObjectPtr<UFXSystemAsset>& Soft)
{
	return Soft.IsNull() ? nullptr : Soft.LoadSynchronous();
}

FTransform UBNGameplayCue_Base::ResolveMuzzle(const AActor* Target, const FGameplayCueParameters& Parameters)
{
	if (const ABNWeapon* Weapon = Cast<ABNWeapon>(Parameters.SourceObject.Get()))
	{
		return Weapon->GetMuzzleTransform();
	}
	return Target ? Target->GetActorTransform() : FTransform::Identity;
}

void UBNGameplayCue_Base::SpawnAt(const UObject* WorldContext, UFXSystemAsset* Asset, const FVector& Location, const FRotator& Rotation)
{
	BNSpawnSystem(WorldContext, Asset, Location, Rotation);
}

FGameplayTag UBNGameplayCue_MuzzleFlash::GetHandledCueTag() const
{
	return BNTags::GameplayCue_Weapon_MuzzleFlash;
}

bool UBNGameplayCue_MuzzleFlash::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const FTransform Muzzle = ResolveMuzzle(MyTarget, Parameters);
	SpawnAt(MyTarget, Resolve(Effect), Muzzle.GetLocation(), Muzzle.Rotator());

	// The shot. Per-weapon first — one cue class serves rifle and pistol, and they are not the
	// same sound — then the cue's own Config line as the fallback.
	TSoftObjectPtr<USoundBase> ShotSound = Sound;
	if (const ABNWeapon* Weapon = Cast<ABNWeapon>(Parameters.SourceObject.Get()))
	{
		if (const FBNWeaponRow* Row = Weapon->GetRow())
		{
			if (!Row->FireSound.IsNull())
			{
				ShotSound = Row->FireSound;
			}
		}
	}
	if (USoundBase* Loaded = ShotSound.IsNull() ? nullptr : ShotSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(MyTarget, Loaded, Muzzle.GetLocation());
	}
	return true;
}

FGameplayTag UBNGameplayCue_Impact::GetHandledCueTag() const
{
	return BNTags::GameplayCue_Weapon_Impact;
}

bool UBNGameplayCue_Impact::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// The surface picks the row, exactly the template's map lookup; no phys mat (or no matching
	// row) falls back to Effect — the template's own fallback is its concrete row.
	const UPhysicalMaterial* PhysMat = Cast<UPhysicalMaterial>(Parameters.PhysicalMaterial.Get());
	const EPhysicalSurface Surface = PhysMat ? PhysMat->SurfaceType.GetValue() : SurfaceType_Default;

	TSoftObjectPtr<UFXSystemAsset> BurstAsset = Effect;
	TSoftObjectPtr<USoundBase> SoundAsset;
	for (const FBNImpactEffectRow& Row : SurfaceRows)
	{
		if (Row.Surface == Surface)
		{
			BurstAsset = Row.Effect;
			SoundAsset = Row.Sound;
			break;
		}
	}

	// The template writes the same hit into BOTH systems; the decal additionally learns the
	// surface (its look selector) and the trigger. Param names and types are the template's
	// own (position array for positions, vector array for normals) — a wrong type here fails
	// silently, which is what buried the first tracer.
	const TArray<FVector> Positions = { Parameters.Location };
	const TArray<FVector> Normals = { FVector(Parameters.Normal) };

	if (UNiagaraComponent* DecalComp = BNSpawnSystem(MyTarget, Resolve(Decal), Parameters.Location, Parameters.Normal.Rotation()))
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(DecalComp, TEXT("ImpactPositions"), Positions);
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(DecalComp, TEXT("ImpactNormals"), Normals);
		const TArray<int32> Surfaces = { static_cast<int32>(Surface) };
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(DecalComp, TEXT("ImpactSurfaces"), Surfaces);
		DecalComp->SetVariableInt(TEXT("NumberOfHits"), 1);
		DecalComp->SetVariableBool(TEXT("Trigger"), true);
	}

	if (UNiagaraComponent* BurstComp = BNSpawnSystem(MyTarget, Resolve(BurstAsset), Parameters.Location, Parameters.Normal.Rotation()))
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(BurstComp, TEXT("ImpactPositions"), Positions);
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(BurstComp, TEXT("ImpactNormals"), Normals);
		BurstComp->SetVariableInt(TEXT("NumberOfHits"), 1);
	}

	if (USoundBase* Sound = SoundAsset.IsNull() ? nullptr : SoundAsset.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(MyTarget, Sound, Parameters.Location);
	}
	return true;
}

FGameplayTag UBNGameplayCue_Explosion::GetHandledCueTag() const
{
	return BNTags::GameplayCue_Grenade_Explode;
}

bool UBNGameplayCue_Explosion::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// NOT ResolveMuzzle: the source object is the projectile, which the authority has already
	// destroyed by the time this reaches a client. Parameters.Location is the blast's own record.
	SpawnAt(MyTarget, Resolve(Effect), Parameters.Location, FRotator::ZeroRotator);

	if (USoundBase* Loaded = Sound.IsNull() ? nullptr : Sound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(MyTarget, Loaded, Parameters.Location);
	}
	return true;
}

FGameplayTag UBNGameplayCue_Tracer::GetHandledCueTag() const
{
	return BNTags::GameplayCue_Weapon_Tracer;
}

bool UBNGameplayCue_Tracer::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// The template's FireTracerEffect, verbatim: spawn at the muzzle, hand the system the hit
	// as a one-element VECTOR array, then pull the trigger. Vector array — not position — is
	// what the template's own graph calls for this system (NiagaraSetVectorArray).
	const FTransform Muzzle = ResolveMuzzle(MyTarget, Parameters);
	if (UNiagaraComponent* Tracer = BNSpawnSystem(MyTarget, Resolve(Effect), Muzzle.GetLocation(), Muzzle.Rotator()))
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(Tracer, TEXT("ImpactPositions"), { Parameters.Location });
		Tracer->SetVariableBool(TEXT("Trigger"), true);
	}
	return true;
}

bool UBNGameplayCueRegistrar::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UBNGameplayCueRegistrar::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	UGameplayCueSet* RuntimeSet = CueManager ? CueManager->GetRuntimeCueSet() : nullptr;
	if (!RuntimeSet)
	{
		return;
	}

	TArray<UClass*> HandlerClasses;
	GetDerivedClasses(UBNGameplayCue_Base::StaticClass(), HandlerClasses, /*bRecursive=*/true);

	TArray<FGameplayCueReferencePair> CuesToAdd;
	for (UClass* HandlerClass : HandlerClasses)
	{
		if (!HandlerClass || HandlerClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		UBNGameplayCue_Base* CDO = HandlerClass->GetDefaultObject<UBNGameplayCue_Base>();
		const FGameplayTag Handled = CDO ? CDO->GetHandledCueTag() : FGameplayTag();
		if (!Handled.IsValid())
		{
			continue;
		}

		// Asked of the CDO now, not read off whatever PostInitProperties resolved during class
		// construction — and written back, so anything else reading GameplayCueTag agrees.
		CDO->GameplayCueTag = Handled;
		CuesToAdd.Emplace(Handled, FSoftObjectPath(HandlerClass));
	}

	if (!CuesToAdd.IsEmpty())
	{
		RuntimeSet->AddCues(CuesToAdd);
	}
}
