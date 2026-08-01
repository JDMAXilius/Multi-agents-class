// Breachpoint. The cue handlers, and the registration that arms them.

#include "AbilitySystem/Cues/BRGameplayCues.h"

#include "AbilitySystemGlobals.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectHash.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

// ================================================================================================
// File-local knobs and the log throttle
// ================================================================================================

namespace
{
#if ENABLE_DRAW_DEBUG
	/**
	 * How long a placeholder marker stays on screen, and how big it is. STRUCTURAL, not tuning:
	 * these describe a debug drawing, not a weapon. Nobody balances them, and they are compiled
	 * out entirely of a shipping build — which is why they live inside the guard rather than
	 * beside it, where they would be an unused-constant warning nobody can silence locally.
	 */
	constexpr float PlaceholderDrawSeconds = 0.35f;
	constexpr float PlaceholderRadius = 10.f;
#endif

	/**
	 * The "this cue played nothing" report is ONCE PER (tag, slot) for the life of the process.
	 * An AR at 600 RPM would otherwise emit ten Warnings a second and bury whatever the log was
	 * opened to find — and law 4's spirit is that nothing this project writes runs per frame.
	 *
	 * A file-static set, not a member: `UGameplayCueNotify_Static` handlers run on the shared CDO
	 * and are `const`. This is a log throttle, not gameplay state, and it is the ONLY mutable
	 * thing in this file. Game thread only, like every cue route.
	 */
	TSet<FName>& GetSilentCueReportLedger()
	{
		static TSet<FName> Ledger;
		return Ledger;
	}
}

static int32 GBRCueDrawPlaceholders = 1;
static FAutoConsoleVariableRef CVarBRCueDrawPlaceholders(
	TEXT("BR.Cues.DrawPlaceholders"),
	GBRCueDrawPlaceholders,
	TEXT("Draw a debug marker where a Breachpoint GameplayCue fired but had no FX asset to play. "
		 "1 (default, non-shipping) proves the cue is routing; 0 for a clean capture. The "
		 "once-per-tag log warning is not affected."),
	ECVF_Cheat);

// ================================================================================================
// UBRGameplayCue_Base
// ================================================================================================

UBRGameplayCue_Base::UBRGameplayCue_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// `IsOverride` is inherited as true from UGameplayCueNotify_Static and is left true. It means
	// "when I handle a tag, do NOT also run the parent tag's notify". Breachpoint registers exact
	// leaves and owns no parent-cue chain, so a false here would only ever find a handler nobody
	// meant to run.
	//
	// GameplayCueTag is deliberately NOT set. See the header: for a native class it is read by
	// nothing, and this family answers to more tags than the one field can hold.
}

FVector UBRGameplayCue_Base::ResolveCueLocation(const AActor* Target, const FGameplayCueParameters& Parameters)
{
	if (!Parameters.Location.IsNearlyZero())
	{
		return Parameters.Location;
	}
	return Target ? Target->GetActorLocation() : FVector::ZeroVector;
}

bool UBRGameplayCue_Base::PlaySoundSoft(const UObject* WorldContext, const TSoftObjectPtr<USoundBase>& SoftSound, const FVector& Location) const
{
	if (SoftSound.IsNull())
	{
		return false;
	}

	// Get(), never LoadSynchronous(). A cue runs on the frame the player pulled the trigger; a
	// blocking package load there is a hitch felt exactly when it is least forgivable. The
	// registrar warms these at world begin play so the resident case is the normal one.
	USoundBase* Sound = SoftSound.Get();
	if (!Sound)
	{
		return false;
	}

	UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location);
	return true;
}

bool UBRGameplayCue_Base::SpawnFXSoft(const UObject* WorldContext, const TSoftObjectPtr<UFXSystemAsset>& SoftFX, const FVector& Location, const FRotator& Rotation) const
{
	if (SoftFX.IsNull())
	{
		return false;
	}

	UFXSystemAsset* FX = SoftFX.Get();
	if (!FX)
	{
		return false;
	}

	if (UParticleSystem* Cascade = Cast<UParticleSystem>(FX))
	{
		UGameplayStatics::SpawnEmitterAtLocation(WorldContext, Cascade, Location, Rotation);
		return true;
	}

	// A Niagara system landed here and this module cannot spawn it: `UNiagaraFunctionLibrary`
	// lives in the `Niagara` module, which is not in `Breachpoint.Build.cs` and cannot be added
	// from this packet's owner path. Named loudly, once, rather than returning false and letting
	// it read as "no asset authored" — those are different problems with different fixes.
	static TSet<FName> ReportedNiagara;
	const FName AssetName = FX->GetFName();
	if (!ReportedNiagara.Contains(AssetName))
	{
		ReportedNiagara.Add(AssetName);
		UE_LOG(LogBRCombat, Warning,
			TEXT("BRGameplayCues: '%s' is a non-Cascade FX system and cannot be spawned — the "
				 "`Niagara` module is not a dependency of Breachpoint.Build.cs. contract_gap, not "
				 "a missing asset."),
			*AssetName.ToString());
	}
	return false;
}

void UBRGameplayCue_Base::ReportSilentCue(const UObject* WorldContext, const FGameplayCueParameters& Parameters, const FVector& Location, const TCHAR* Slot) const
{
	const FGameplayTag& MatchedTag = Parameters.MatchedTagName;
	const FName LedgerKey(*FString::Printf(TEXT("%s|%s"), *MatchedTag.ToString(), Slot));

	TSet<FName>& Ledger = GetSilentCueReportLedger();
	if (!Ledger.Contains(LedgerKey))
	{
		Ledger.Add(LedgerKey);
		UE_LOG(LogBRCombat, Warning,
			TEXT("GameplayCue '%s' fired and its '%s' slot had nothing to play. The cue is BOUND "
				 "and ROUTING correctly — no FX asset is authored for it yet. This is logged once "
				 "per tag+slot; see BR.Cues.DrawPlaceholders for the in-world marker."),
			*MatchedTag.ToString(), Slot);
	}

#if ENABLE_DRAW_DEBUG
	if (GBRCueDrawPlaceholders != 0)
	{
		if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr)
		{
			// The point of the marker is to make "the cue fired" observable to a human with no FX
			// in the project. Without it, a correctly-bound cue and a completely unbound one look
			// identical from the chair, and the packet's own claim would be unfalsifiable.
			DrawDebugSphere(World, Location, PlaceholderRadius, 8, FColor::Yellow, false, PlaceholderDrawSeconds);
		}
	}
#endif
}

// ================================================================================================
// UBRGameplayCue_WeaponFire
// ================================================================================================

UBRGameplayCue_WeaponFire::UBRGameplayCue_WeaponFire(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Nothing to configure. The tags come from GetHandledCueTags(), the FX from config, and there
	// is no third thing a fire cue needs — which is the shape a cue handler should have.
}

void UBRGameplayCue_WeaponFire::GetHandledCueTags(FGameplayTagContainer& OutTags) const
{
	// The three leaves that exist. They are named by `DT_Weapons.csv`'s `FireCueTag` column and
	// declared in `BRGameplayTags.h` (R23's open GameplayCue.* family); this packet declares no
	// tag of its own and adds none. A fourth weapon adds a fourth leaf THERE and one line HERE.
	OutTags.AddTag(BRGameplayTags::GameplayCue_Weapon_AR_Fire);
	OutTags.AddTag(BRGameplayTags::GameplayCue_Weapon_Magnum_Fire);
	OutTags.AddTag(BRGameplayTags::GameplayCue_Weapon_Rocket_Fire);
}

void UBRGameplayCue_WeaponFire::GetFXAssetPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	for (const TPair<FGameplayTag, FBRWeaponFireCueFX>& Entry : FXByCueTag)
	{
		if (!Entry.Value.MuzzleFlash.IsNull()) { OutPaths.Add(Entry.Value.MuzzleFlash.ToSoftObjectPath()); }
		if (!Entry.Value.Tracer.IsNull())      { OutPaths.Add(Entry.Value.Tracer.ToSoftObjectPath()); }
		if (!Entry.Value.Report.IsNull())      { OutPaths.Add(Entry.Value.Report.ToSoftObjectPath()); }
	}
}

bool UBRGameplayCue_WeaponFire::HandlesEvent(EGameplayCueEvent::Type EventType) const
{
	// ONE-SHOT. `gas-purity` §6: Executed for muzzle/impact/hitmarker, Added/Removed for looping
	// state FX. The base class answers true to everything; saying so explicitly here is what
	// stops a future duration effect carrying this tag from producing a second muzzle flash on
	// OnActive and a third on WhileActive for a client that joined after the shot.
	return EventType == EGameplayCueEvent::Executed;
}

bool UBRGameplayCue_WeaponFire::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// `MyTarget` is the world context, not `GetWorld()`: UGameplayCueNotify_Static::GetWorld()
	// returns the cue manager's CACHED world, which is correct during a route and stale outside
	// one. The target actor is unambiguous.
	const UObject* WorldContext = MyTarget;
	const FVector MuzzleLocation = ResolveCueLocation(MyTarget, Parameters);

	// The tag we were MATCHED on, not the tag as raised — a translator or a parent fallback makes
	// those different, and the FX row is keyed by what actually matched.
	const FGameplayTag& CueTag = Parameters.MatchedTagName;
	const FBRWeaponFireCueFX* FX = FXByCueTag.Find(CueTag);

	// Aim the muzzle flash along the shot where we can. The instigator's control rotation is the
	// shooter's facing on every machine that received this cue (it is replicated), so this is not
	// a client-local guess. Purely cosmetic: nothing below reads or changes gameplay state.
	FRotator MuzzleRotation = FRotator::ZeroRotator;
	if (const AActor* Shooter = Parameters.Instigator.Get())
	{
		FVector EyeLocation;
		Shooter->GetActorEyesViewPoint(EyeLocation, MuzzleRotation);
	}

	// -- Muzzle flash ---------------------------------------------------------------------------
	if (!FX || !SpawnFXSoft(WorldContext, FX->MuzzleFlash, MuzzleLocation, MuzzleRotation))
	{
		ReportSilentCue(WorldContext, Parameters, MuzzleLocation, TEXT("MuzzleFlash"));
	}

	// -- Report ---------------------------------------------------------------------------------
	if (!FX || !PlaySoundSoft(WorldContext, FX->Report, MuzzleLocation))
	{
		ReportSilentCue(WorldContext, Parameters, MuzzleLocation, TEXT("Report"));
	}

	// -- Tracer ---------------------------------------------------------------------------------
	//
	// A beam needs BOTH ends and only one travels today. The impact point would ride in the cue's
	// effect context; `BRGA_WeaponFire` builds its FGameplayCueParameters with Location and
	// Instigator and no context at all, so there is nothing to read. Reported as silent rather
	// than extrapolated from MuzzleRotation: a tracer drawn along where the shooter is looking
	// NOW is a lie about where the bullet went, and a convincing one.
	const FHitResult* ShotHit = Parameters.EffectContext.GetHitResult();
	const bool bTracerPlayed = ShotHit && FX
		&& SpawnFXSoft(WorldContext, FX->Tracer, MuzzleLocation, (ShotHit->ImpactPoint - MuzzleLocation).Rotation());

	if (!bTracerPlayed)
	{
		ReportSilentCue(WorldContext, Parameters, MuzzleLocation, TEXT("Tracer"));

#if ENABLE_DRAW_DEBUG
		if (GBRCueDrawPlaceholders != 0 && ShotHit)
		{
			if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr)
			{
				DrawDebugLine(World, MuzzleLocation, ShotHit->ImpactPoint, FColor::Yellow, false, PlaceholderDrawSeconds);
			}
		}
#endif
	}

	// The return value of OnExecute is unused by UGameplayCueNotify_Static::HandleGameplayCue —
	// it is a BlueprintNativeEvent whose result the engine discards. Returning true anyway so a
	// direct caller (an automation spec calling the CDO) reads "handled", never as a signal that
	// FX actually played. Whether FX played is what the log above answers.
	return true;
}

// ================================================================================================
// UBRGameplayCueRegistrar — the binding
// ================================================================================================

bool UBRGameplayCueRegistrar::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UBRGameplayCueRegistrar::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const int32 Bound = RegisterNativeCueHandlers();
	WarmCueFXAssets();

	if (Bound == 0)
	{
		// Zero is never correct while any handler exists, and it is the state in which every cue
		// tag in the project plays nothing — which is precisely the bug this file was written to
		// end. It must not pass quietly.
		UE_LOG(LogBRCombat, Error,
			TEXT("BRGameplayCueRegistrar bound ZERO cue handlers in world '%s'. Every "
				 "GameplayCue.* tag will route to nothing. Check that the GameplayCueManager "
				 "exists and that a UBRGameplayCue_Base subclass declares tags."),
			*InWorld.GetName());
	}
}

int32 UBRGameplayCueRegistrar::RegisterNativeCueHandlers()
{
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!CueManager)
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRGameplayCueRegistrar: no GameplayCueManager. No cue can play."));
		return 0;
	}

	// GetRuntimeCueSet() can legitimately be null before the manager's object library has been
	// initialised. Asking for the manager above is what triggers that initialisation, so by here
	// it should exist — a null is a real problem, not a timing quirk to retry past.
	UGameplayCueSet* RuntimeSet = CueManager->GetRuntimeCueSet();
	if (!RuntimeSet)
	{
		UE_LOG(LogBRCombat, Error, TEXT("BRGameplayCueRegistrar: the GameplayCueManager has no runtime cue set."));
		return 0;
	}

	// Discovery, not a hand-kept list. A new handler class registers itself by existing and
	// declaring its tags; a list here would eventually miss one, and a missed handler is silent.
	TArray<UClass*> HandlerClasses;
	GetDerivedClasses(UBRGameplayCue_Base::StaticClass(), HandlerClasses, /*bRecursive=*/true);

	TArray<FGameplayCueReferencePair> CuesToAdd;
	TArray<TPair<FGameplayTag, UClass*>> Expected;

	for (UClass* HandlerClass : HandlerClasses)
	{
		if (!HandlerClass || HandlerClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		const UBRGameplayCue_Base* CDO = HandlerClass->GetDefaultObject<UBRGameplayCue_Base>();
		if (!CDO)
		{
			continue;
		}

		FGameplayTagContainer HandledTags;
		CDO->GetHandledCueTags(HandledTags);

		if (HandledTags.IsEmpty())
		{
			UE_LOG(LogBRCombat, Warning,
				TEXT("BRGameplayCueRegistrar: '%s' declares no cue tags, so it can never run. A "
					 "handler that answers to nothing is dead code, not a disabled feature."),
				*HandlerClass->GetName());
			continue;
		}

		// The soft path to a NATIVE class is `/Script/Breachpoint.<ClassName>`. The cue set
		// resolves it with ResolveObject(), which always succeeds for a native UClass — so this
		// binding never takes the async-load path a Blueprint notify would.
		const FSoftObjectPath ClassPath(HandlerClass);

		for (const FGameplayTag& Tag : HandledTags)
		{
			if (!Tag.IsValid())
			{
				UE_LOG(LogBRCombat, Warning,
					TEXT("BRGameplayCueRegistrar: '%s' returned an INVALID tag. A cue bound to an "
						 "invalid tag looks armed and is not."),
					*HandlerClass->GetName());
				continue;
			}
			CuesToAdd.Emplace(Tag, ClassPath);
			Expected.Emplace(Tag, HandlerClass);
		}
	}

	// AddCues skips a tag the set already holds, so re-asserting every world is free and this
	// whole function is idempotent. It warns on a CONFLICT (same tag, different class), which is
	// the one case that must never pass silently.
	RuntimeSet->AddCues(CuesToAdd);

	// VERIFY, then report. Registering and assuming is how the original bug happened: the point
	// of this pass is that the count below is read back out of the engine's own map, not counted
	// from what we intended.
	int32 Verified = 0;
	for (const TPair<FGameplayTag, UClass*>& Entry : Expected)
	{
		const int32* DataIdx = RuntimeSet->GameplayCueDataMap.Find(Entry.Key);
		const bool bExact = DataIdx && RuntimeSet->GameplayCueData.IsValidIndex(*DataIdx)
			&& RuntimeSet->GameplayCueData[*DataIdx].GameplayCueTag == Entry.Key;

		if (bExact)
		{
			++Verified;
			UE_LOG(LogBRCombat, Log, TEXT("BRGameplayCueRegistrar: '%s' -> %s"),
				*Entry.Key.ToString(), *Entry.Value->GetName());
		}
		else
		{
			UE_LOG(LogBRCombat, Error,
				TEXT("BRGameplayCueRegistrar: '%s' did NOT land in the runtime cue set (wanted "
					 "%s). That tag will play nothing."),
				*Entry.Key.ToString(), *Entry.Value->GetName());
		}
	}

	return Verified;
}

void UBRGameplayCueRegistrar::WarmCueFXAssets()
{
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!CueManager)
	{
		return;
	}

	TArray<UClass*> HandlerClasses;
	GetDerivedClasses(UBRGameplayCue_Base::StaticClass(), HandlerClasses, /*bRecursive=*/true);

	TArray<FSoftObjectPath> Paths;
	for (UClass* HandlerClass : HandlerClasses)
	{
		if (!HandlerClass || HandlerClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}
		if (const UBRGameplayCue_Base* CDO = HandlerClass->GetDefaultObject<UBRGameplayCue_Base>())
		{
			CDO->GetFXAssetPaths(Paths);
		}
	}

	if (Paths.IsEmpty())
	{
		// The honest state today: zero FX assets exist in the project, so zero soft refs are
		// configured and there is nothing to warm. Said at Log rather than left as an absence,
		// because "no FX warmed" and "warming never ran" are different and only one is expected.
		UE_LOG(LogBRCombat, Log,
			TEXT("BRGameplayCueRegistrar: no cue FX soft references configured — cues will route "
				 "and report their empty slots. Populate [/Script/Breachpoint.BRGameplayCue_WeaponFire] "
				 "FXByCueTag once FX assets exist."));
		return;
	}

	// The cue manager's own streamable manager, not a second one: it is what the engine already
	// uses to keep cue assets resident, so warmed refs stay warm for the same reasons.
	CueManager->StreamableManager.RequestAsyncLoad(Paths);
	UE_LOG(LogBRCombat, Log, TEXT("BRGameplayCueRegistrar: warming %d cue FX asset(s)."), Paths.Num());
}
