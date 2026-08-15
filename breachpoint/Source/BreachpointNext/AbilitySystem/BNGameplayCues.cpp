#include "AbilitySystem/BNGameplayCues.h"

#include "BreachpointNext.h"
#include "Characters/BNCharacter.h"
#include "Core/BNCollision.h"
#include "Core/BNGameplayTags.h"
#include "Weapons/BNWeapon.h"
#include "Data/BNDataRows.h"
#include "AbilitySystemGlobals.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
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

FGameplayTag UBNGameplayCue_Death::GetHandledCueTag() const
{
	return BNTags::GameplayCue_Character_Death;
}

bool UBNGameplayCue_Death::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	ACharacter* Character = Cast<ACharacter>(MyTarget);
	USkeletalMeshComponent* MeshComp = Character ? Character->GetMesh() : nullptr;
	if (!MeshComp)
	{
		return true;
	}

	if (USoundBase* Loaded = Sound.IsNull() ? nullptr : Sound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(MyTarget, Loaded, Character->GetActorLocation());
	}

	if (!bRagdoll)
	{
		return true;
	}

	// The owner has been looking at the 1P mesh. A ragdoll on the hidden 3P body would be
	// invisible to them — drop the 1P follower and let them see the world mesh fall.
	if (ABNCharacter* BNChar = Cast<ABNCharacter>(Character))
	{
		if (USkeletalMeshComponent* FPMesh = BNChar->GetFirstPersonMesh())
		{
			FPMesh->SetLeaderPoseComponent(nullptr);
			FPMesh->SetHiddenInGame(true);
		}
		MeshComp->SetOwnerNoSee(false);
	}

	// The CAPSULE stops colliding and the MESH starts. Both halves are needed: a corpse whose
	// capsule still blocks is an invisible wall in the middle of a firefight, and a mesh left on
	// its query-only profile falls through the floor it is supposed to land on.
	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Detached first. The mesh is welded to the capsule, and simulating a welded body drags the
	// actor's root with it — the corpse would tow its own capsule around the map.
	MeshComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	MeshComp->SetCollisionProfileName(RagdollCollisionProfile);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// A corpse falls and slides, but it does not soak up gunfire. The Ragdoll profile answers the
	// weapon channels with the channel default (Block), so leaving this alone would give a body
	// three seconds as a bullet sponge in front of whoever is still alive behind it. Stated here
	// as a decision, not inherited from a response table nobody chose.
	MeshComp->SetCollisionResponseToChannel(BNCollision::WeaponTrace, ECR_Ignore);
	MeshComp->SetCollisionResponseToChannel(BNCollision::MeleeTrace, ECR_Ignore);
	MeshComp->SetAllBodiesSimulatePhysics(true);
	MeshComp->WakeAllRigidBodies();

	// Runs on every machine off one multicast, so each simulates its own corpse locally. They will
	// not settle identically — physics is not deterministic across machines — and that is accepted:
	// a corpse is cosmetic, it lives ~3 seconds, and replicating ragdoll bones would cost more
	// bandwidth than every weapon in the game combined.
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

	// Load the Blueprint children FIRST. GetDerivedClasses walks LOADED classes only, and the cue
	// Blueprints have no referencer — on a cold boot they never load, the C++ CDO silently wins
	// every tag, and the asset references set in the editor never apply (the ini fallback plays
	// instead, which is invisible until an FX edit "does nothing"). The asset registry knows the
	// inheritance without loading; anything under it that is not a /Script/ native class is a
	// Blueprint-generated cue class and gets loaded here, so the most-derived filter below can see it.
	if (const IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		TSet<FTopLevelAssetPath> DerivedClassPaths;
		AssetRegistry->GetDerivedClassNames(
			{ UBNGameplayCue_Base::StaticClass()->GetClassPathName() }, /*ExcludedClassNames=*/{}, DerivedClassPaths);
		for (const FTopLevelAssetPath& ClassPath : DerivedClassPaths)
		{
			if (!ClassPath.GetPackageName().ToString().StartsWith(TEXT("/Script/")))
			{
				FSoftClassPath(ClassPath.ToString()).TryLoadClass<UBNGameplayCue_Base>();
			}
		}
	}

	TArray<UClass*> HandlerClasses;
	GetDerivedClasses(UBNGameplayCue_Base::StaticClass(), HandlerClasses, /*bRecursive=*/true);

	// Editor-only compilation artifacts (SKEL_/REINST_) also show up as derived classes and used
	// to log a phantom registration per tag before the real one — the CUE-BLUEPRINTS ticket's
	// standing finding. They are never the handler; drop them before the most-derived filter so
	// they cannot shadow the class they are scaffolding for.
	HandlerClasses.RemoveAll([](const UClass* Candidate)
	{
		const FString Name = Candidate ? Candidate->GetName() : FString();
		return Name.StartsWith(TEXT("SKEL_")) || Name.StartsWith(TEXT("REINST_"));
	});

	// MOST-DERIVED WINS, and this must land BEFORE any Blueprint child of a cue class exists.
	//
	// A BP child IS a derived class: it inherits GetHandledCueTag() and so registers under its C++
	// parent's tag. UGameplayCueSet keys by tag and REPLACES on a duplicate, so without this the
	// surviving handler depends on whatever order GetDerivedClasses happened to return — the muzzle
	// flash would come from the C++ CDO on one run and the Blueprint on the next, with nothing in
	// the log naming the winner.
	//
	// A class that another registered handler derives FROM is a base, not the handler: the BP child
	// holding the editor's asset references is what should answer. With no BP children this selects
	// exactly the same leaf classes as before, which is what makes it safe to land ahead of them.
	HandlerClasses.RemoveAll([&HandlerClasses](const UClass* Candidate)
	{
		return HandlerClasses.ContainsByPredicate([Candidate](const UClass* Other)
		{
			return Other != Candidate && Other->IsChildOf(Candidate);
		});
	});

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

		// Named out loud, because "which class answered this tag" is otherwise invisible and is
		// exactly what goes wrong when a Blueprint child is added and its assets do not appear.
		UE_LOG(LogBN, Log, TEXT("BNCues: %s -> %s"), *Handled.ToString(), *HandlerClass->GetName());
	}

	if (!CuesToAdd.IsEmpty())
	{
		RuntimeSet->AddCues(CuesToAdd);
	}
}
