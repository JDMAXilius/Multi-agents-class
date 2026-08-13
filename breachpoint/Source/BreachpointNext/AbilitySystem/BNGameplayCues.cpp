#include "AbilitySystem/BNGameplayCues.h"

#include "Core/BNGameplayTags.h"
#include "Weapons/BNWeapon.h"
#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "UObject/UObjectHash.h"

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

void UBNGameplayCue_Base::SpawnAt(const UObject* WorldContext, UFXSystemAsset* Asset, const FVector& Location, const FRotator& Rotation, FName VectorParameterName, const FVector& VectorParameterValue)
{
	UNiagaraSystem* System = Cast<UNiagaraSystem>(Asset);
	if (!WorldContext || !System)
	{
		return;
	}

	UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		WorldContext, System, Location, Rotation, FVector(1.f), /*bAutoDestroy=*/true, /*bAutoActivate=*/true);

	if (Component && !VectorParameterName.IsNone())
	{
		Component->SetVectorParameter(VectorParameterName, VectorParameterValue);
	}
}

FGameplayTag UBNGameplayCue_MuzzleFlash::GetHandledCueTag() const
{
	return BNTags::GameplayCue_Weapon_MuzzleFlash;
}

bool UBNGameplayCue_MuzzleFlash::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const FTransform Muzzle = ResolveMuzzle(MyTarget, Parameters);
	SpawnAt(MyTarget, Resolve(Effect), Muzzle.GetLocation(), Muzzle.Rotator(), NAME_None, FVector::ZeroVector);
	return true;
}

FGameplayTag UBNGameplayCue_Impact::GetHandledCueTag() const
{
	return BNTags::GameplayCue_Weapon_Impact;
}

bool UBNGameplayCue_Impact::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	SpawnAt(MyTarget, Resolve(Effect), Parameters.Location, Parameters.Normal.Rotation(), NAME_None, FVector::ZeroVector);
	return true;
}

FGameplayTag UBNGameplayCue_Tracer::GetHandledCueTag() const
{
	return BNTags::GameplayCue_Weapon_Tracer;
}

bool UBNGameplayCue_Tracer::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// Muzzle to impact: the start is the weapon's own socket, the end rides the beam parameter.
	const FTransform Muzzle = ResolveMuzzle(MyTarget, Parameters);
	SpawnAt(MyTarget, Resolve(Effect), Muzzle.GetLocation(), Muzzle.Rotator(), BeamEndParameter, Parameters.Location);
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
