#include "AbilitySystem/BNGameplayCues.h"

#include "BreachpointNext.h"
#include "Characters/BNCharacter.h"
#include "Core/BNCollision.h"
#include "Core/BNGameplayTags.h"
#include "Match/BNPlayerState.h"
#include "Match/BNTeams.h"
#include "UI/BNUITypes.h"
#include "Weapons/BNWeapon.h"
#include "Data/BNDataRows.h"
#include "AbilitySystemGlobals.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "GameFramework/PlayerController.h"
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

UNiagaraComponent* UBNGameplayCue_Base::SpawnAt(const UObject* WorldContext, UFXSystemAsset* Asset, const FVector& Location, const FRotator& Rotation)
{
	return BNSpawnSystem(WorldContext, Asset, Location, Rotation);
}

const APawn* UBNGameplayCue_Base::ResolveEffectOwner(const AActor* Target)
{
	if (const APawn* AsPawn = Cast<APawn>(Target))
	{
		return AsPawn;
	}
	// THE GRENADE CASE. The blast cue is handled on the PROJECTILE — an actor with no
	// PlayerState — so the pawn cast returned null every time and no colour was ever resolved.
	// The instigator is the thrower, which is exactly "whose grenade is this".
	return Target ? Target->GetInstigator() : nullptr;
}

bool UBNGameplayCue_Base::ResolveTeamTint(const AActor* Target, FLinearColor& OutTint, bool bTintOwnEffects)
{
	// WHOSE effect is this? A cue's target is usually the pawn that fired, but not always: the
	// grenade blast is handled on the PROJECTILE, an actor with no PlayerState, and this cast
	// returned null for it every single time. The tint was never dropped for want of a colour
	// parameter there — NS_Grenade_Explosion declares one — it was dropped because nothing ever
	// resolved a colour to write. The blast has been neutral since the day it was wired.
	//
	// So: fall back to the actor's INSTIGATOR, which is exactly the "whose is this" answer for
	// any effect spawned by a projectile, a decal or any other ownerless prop. Fixed HERE rather
	// than at the explosion's call site because every future non-pawn cue target has the same
	// bug waiting, and one guard in the shared resolver is smaller than a guard in each caller.
	const APawn* TargetPawn = ResolveEffectOwner(Target);
	const ABNPlayerState* TargetPS = TargetPawn ? TargetPawn->GetPlayerState<ABNPlayerState>() : nullptr;
	if (!TargetPS)
	{
		return false;
	}

	// THE VIEWER, not the instigator. This runs on every machine that can see the shooter, so the
	// only correct question is "who is this to the person looking at the screen" — asked of the
	// TARGET'S OWN WORLD, because in PIE two clients share a process and GEngine's first local
	// controller belongs to whichever window came up first.
	const APlayerController* Viewer = GEngine ? GEngine->GetFirstLocalPlayerController(Target->GetWorld()) : nullptr;
	const ABNPlayerState* ViewerPS = Viewer ? Viewer->GetPlayerState<ABNPlayerState>() : nullptr;
	if (!ViewerPS)
	{
		return false;
	}
	if (ViewerPS == TargetPS)
	{
		// IDENTITY. Your own fire keeps the look it has always had — unless the caller opted in,
		// which the grenade blast does: you are nearly always watching your OWN explosion, so the
		// old rule made the tint look broken rather than deliberate (founder, 28 Aug: "still
		// gray"). Your own side is Ally, exactly as your own body reads to a spectator.
		if (!bTintOwnEffects)
		{
			return false;
		}
		OutTint = BNUIColors::Ally;
		return true;
	}

	// Either side unassigned answers "no tint": FFA, and the joining client's honest-unknown
	// frame. AreFriendly guards the same sentinel, but it has two answers and this needs three
	// — without this test an unassigned shooter would read ENEMY rather than unknown, and the
	// teams-OFF game would go red. Byte-identical to today's FFA by construction.
	const FGenericTeamId ViewerTeam = ViewerPS->GetGenericTeamId();
	const FGenericTeamId TargetTeam = TargetPS->GetGenericTeamId();
	if (ViewerTeam == FGenericTeamId::NoTeam || TargetTeam == FGenericTeamId::NoTeam)
	{
		return false;
	}

	OutTint = BNTeams::AreFriendly(ViewerTeam, TargetTeam) ? BNUIColors::Ally : BNUIColors::Threat;
	return true;
}

void UBNGameplayCue_Base::ApplyTeamTint(UNiagaraComponent* Component, const AActor* Target) const
{
	FLinearColor Tint;
	const bool bResolved = ResolveTeamTint(Target, Tint, bTintOwnEffects);
	if (Component && !TintParameter.IsNone() && bResolved)
	{
		// A name the system does not declare is a SILENT no-op — see TintParameter's comment for
		// which shipped systems declare one (today: only the grenade's).
		Component->SetVariableLinearColor(TintParameter, Tint);
	}

	// THE LOG THIS BUG EARNED. The grenade blast drew neutral for weeks with nothing to show for
	// it: the tint failed to RESOLVE (the cue is handled on a projectile, so the pawn cast missed)
	// and a tint that resolves to "no answer" is indistinguishable from FFA. Two rounds of
	// "it's still gray" is what an unlogged silent path costs, so the path now says which of its
	// three ways out it took. Verbose — off in a normal run, one line per cue when asked for.
	UE_LOG(LogBN, Verbose, TEXT("BNCue: %s tint on %s -> %s (param %s, component %s)"),
		*GetClass()->GetName(),
		*GetNameSafe(Target),
		bResolved ? *Tint.ToString() : TEXT("NOT RESOLVED (no owner pawn, no viewer, or unassigned side)"),
		TintParameter.IsNone() ? TEXT("<none>") : *TintParameter.ToString(),
		Component ? TEXT("spawned") : TEXT("NULL — nothing to write to"));
}

FGameplayTag UBNGameplayCue_MuzzleFlash::GetHandledCueTag() const
{
	return BNTags::GameplayCue_Weapon_MuzzleFlash;
}

bool UBNGameplayCue_MuzzleFlash::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const FTransform Muzzle = ResolveMuzzle(MyTarget, Parameters);
	ApplyTeamTint(SpawnAt(MyTarget, Resolve(Effect), Muzzle.GetLocation(), Muzzle.Rotator()), MyTarget);

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
	//
	// TEAM TINT. This is the one cue whose system CAN take a colour — though for a long time it
	// still did not draw one: the cue is handled on the projectile, so the resolver's pawn cast
	// failed and no colour was ever produced (fixed in ResolveTeamTint, which now falls back to
	// the instigator). The tint is wired into five other cues, but none of THEIR systems
	// declares a colour parameter — read straight
	// out of the shipped .uasset payloads, the tracer exposes User.ImpactPositions /
	// MuzzlePosition / Trigger, the muzzle flash User.Direction / SmokePuffTexture / Trigger,
	// the impact ImpactPositions / Normals / NumberOfHits. NS_Grenade_Explosion is the only
	// shipped system in the project carrying `User.Team_Color`, which is exactly the name
	// TintParameter defaults to. So a grenade blast shows Ally blue or Threat red today,
	// while the rest stay colourless until an FX asset declares the parameter.
	ApplyTeamTint(SpawnAt(MyTarget, Resolve(Effect), Parameters.Location, FRotator::ZeroRotator), MyTarget);

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
		ApplyTeamTint(Tracer, MyTarget);
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


////////////////////////////////////////////////////////////////////
// BN23 — the Grappleshot's presentation

FGameplayTag UBNGameplayCue_GrappleFire::GetHandledCueTag() const
{
	return BNTags::GameplayCue_Grapple_Fire;
}

bool UBNGameplayCue_GrappleFire::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!MyTarget)
	{
		return true;
	}

	const FTransform Muzzle = ResolveMuzzle(MyTarget, Parameters);

	if (USoundBase* Loaded = Sound.IsNull() ? nullptr : Sound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(MyTarget, Loaded, Muzzle.GetLocation());
	}
	if (UFXSystemAsset* Burst = Resolve(Effect))
	{
		ApplyTeamTint(SpawnAt(MyTarget, Burst, Muzzle.GetLocation(), Muzzle.GetRotation().Rotator()), MyTarget);
	}

	// LOCAL PLAYER ONLY. This cue runs on every client that can see the grappler, so an
	// unguarded shake would kick the camera of everyone watching a teammate fire — the
	// bug PIE cannot show you, because in PIE you are the only viewer.
	const APawn* Pawn = Cast<APawn>(MyTarget);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return true;
	}

	if (UClass* ShakeClass = Shake.IsNull() ? nullptr : Shake.LoadSynchronous())
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraShake(ShakeClass);
		}
	}
	if (UForceFeedbackEffect* Rumble = Haptic.IsNull() ? nullptr : Haptic.LoadSynchronous())
	{
		PC->ClientPlayForceFeedback(Rumble);
	}
	return true;
}

FGameplayTag UBNGameplayCue_GrappleRope::GetHandledCueTag() const
{
	return BNTags::GameplayCue_Grapple_Rope;
}

bool UBNGameplayCue_GrappleRope::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!MyTarget)
	{
		return true;
	}
	const FTransform Muzzle = ResolveMuzzle(MyTarget, Parameters);

	// The TRACER's contract, not a new one: spawn at the muzzle and write the far end into
	// the `User.ImpactPositions` vector ARRAY, which is what the shipped beam systems read.
	// A single BeamEnd vector compiles and draws nothing.
	if (UFXSystemAsset* Rope = Resolve(Effect))
	{
		if (UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				MyTarget->GetWorld(), Cast<UNiagaraSystem>(Rope),
				Muzzle.GetLocation(), Muzzle.GetRotation().Rotator(),
				FVector(1.f), /*bAutoDestroy=*/false))
		{
			TArray<FVector> Ends;
			Ends.Add(Parameters.Location);
			UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
				Comp, TEXT("User.ImpactPositions"), Ends);
			ApplyTeamTint(Comp, MyTarget);
			Comp->SetVariableBool(TEXT("User.Trigger"), true);
		}
	}
	if (USoundBase* Loaded = Loop.IsNull() ? nullptr : Loop.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(MyTarget, Loaded, Muzzle.GetLocation());
	}
	return true;
}

bool UBNGameplayCue_GrappleRope::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// The rope's own end. Nothing to tear down while the FX asset is unset, which is the
	// shipped state — recorded rather than left as an empty override someone deletes as
	// dead code. When a rope asset lands it is stopped HERE, and the component must be
	// found by tag rather than remembered: a static cue holds no per-instance state.
	return true;
}

FGameplayTag UBNGameplayCue_GrappleHit::GetHandledCueTag() const
{
	return BNTags::GameplayCue_Grapple_Hit;
}

bool UBNGameplayCue_GrappleHit::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// At the ANCHOR, not the shooter: Parameters.Location is the trace hit, so the bite
	// reads as something that happened over there rather than in the player's hands.
	const FVector Where = Parameters.Location;
	if (UFXSystemAsset* Burst = Resolve(Effect))
	{
		// Tinted from the GRAPPLER, who is MyTarget — the bite happens over there, but whose
		// hook it was is the fact worth colouring.
		ApplyTeamTint(SpawnAt(MyTarget, Burst, Where, Parameters.Normal.Rotation()), MyTarget);
	}
	if (USoundBase* Loaded = Sound.IsNull() ? nullptr : Sound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(MyTarget, Loaded, Where);
	}
	return true;
}


////////////////////////////////////////////////////////////////////
// BN25 — the pools coming back

FGameplayTag UBNGameplayCue_ShieldRegen::GetHandledCueTag() const
{
	return BNTags::GameplayCue_Character_ShieldRegen;
}

FGameplayTag UBNGameplayCue_HealthRegen::GetHandledCueTag() const
{
	return BNTags::GameplayCue_Character_HealthRegen;
}

bool UBNGameplayCue_RegenBase::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	USceneComponent* Root = MyTarget ? MyTarget->GetRootComponent() : nullptr;
	if (!Root)
	{
		return true;
	}

	// ATTACHED, not spawned at a location: a player heals while running, and a burst left at the
	// spot where the window expired would say the wrong thing about where they are.
	const FName Marker = GetHandledCueTag().GetTagName();
	if (UNiagaraSystem* System = Cast<UNiagaraSystem>(Resolve(Effect)))
	{
		if (UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAttached(
				System, Root, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget, /*bAutoDestroy=*/false))
		{
			// Stamped so OnRemove can find it. A static cue keeps no per-instance state, so the
			// component tag IS the memory — the rope's own note, cashed here.
			Comp->ComponentTags.Add(Marker);
		}
	}
	if (USoundBase* Loaded = Loop.IsNull() ? nullptr : Loop.LoadSynchronous())
	{
		if (UAudioComponent* Audio = UGameplayStatics::SpawnSoundAttached(Loaded, Root))
		{
			Audio->ComponentTags.Add(Marker);
		}
	}
	return true;
}

bool UBNGameplayCue_RegenBase::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!MyTarget)
	{
		return true;
	}
	// Only THIS cue's work: the two regens can run at once and the marker is the cue tag, so the
	// shield's teardown cannot take the health loop with it.
	for (UActorComponent* Spawned : MyTarget->GetComponentsByTag(USceneComponent::StaticClass(), GetHandledCueTag().GetTagName()))
	{
		Spawned->DestroyComponent();
	}
	return true;
}
