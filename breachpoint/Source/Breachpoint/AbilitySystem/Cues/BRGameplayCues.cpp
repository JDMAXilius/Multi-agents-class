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
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectHash.h"

#if ENABLE_DRAW_DEBUG
#include "DrawDebugHelpers.h"
#endif

namespace
{
#if ENABLE_DRAW_DEBUG
	constexpr float PlaceholderDrawSeconds = 0.35f;
	constexpr float PlaceholderRadius = 10.f;
#endif

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

UBRGameplayCue_Base::UBRGameplayCue_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
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

	if (UNiagaraSystem* Niagara = Cast<UNiagaraSystem>(FX))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext, Niagara, Location, Rotation);
		return true;
	}

	static TSet<FName> ReportedUnknownFX;
	const FName AssetName = FX->GetFName();
	if (!ReportedUnknownFX.Contains(AssetName))
	{
		ReportedUnknownFX.Add(AssetName);
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
	}

#if ENABLE_DRAW_DEBUG
	if (GBRCueDrawPlaceholders != 0)
	{
		if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr)
		{
			DrawDebugSphere(World, Location, PlaceholderRadius, 8, FColor::Yellow, false, PlaceholderDrawSeconds);
		}
	}
#endif
}

UBRGameplayCue_WeaponFire::UBRGameplayCue_WeaponFire(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBRGameplayCue_WeaponFire::GetHandledCueTags(FGameplayTagContainer& OutTags) const
{
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
	return EventType == EGameplayCueEvent::Executed;
}

bool UBRGameplayCue_WeaponFire::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	const UObject* WorldContext = MyTarget;
	const FVector MuzzleLocation = ResolveCueLocation(MyTarget, Parameters);

	const FGameplayTag& CueTag = Parameters.MatchedTagName;
	const FBRWeaponFireCueFX* FX = FXByCueTag.Find(CueTag);

	FRotator MuzzleRotation = FRotator::ZeroRotator;
	if (const AActor* Shooter = Parameters.Instigator.Get())
	{
		FVector EyeLocation;
		Shooter->GetActorEyesViewPoint(EyeLocation, MuzzleRotation);
	}

	if (!FX || !SpawnFXSoft(WorldContext, FX->MuzzleFlash, MuzzleLocation, MuzzleRotation))
	{
		ReportSilentCue(WorldContext, Parameters, MuzzleLocation, TEXT("MuzzleFlash"));
	}

	if (!FX || !PlaySoundSoft(WorldContext, FX->Report, MuzzleLocation))
	{
		ReportSilentCue(WorldContext, Parameters, MuzzleLocation, TEXT("Report"));
	}

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

	return true;
}

bool UBRGameplayCueRegistrar::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UBRGameplayCueRegistrar::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const int32 Bound = RegisterNativeCueHandlers();
	WarmCueFXAssets();

}

int32 UBRGameplayCueRegistrar::RegisterNativeCueHandlers()
{
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!CueManager)
	{
		return 0;
	}

	UGameplayCueSet* RuntimeSet = CueManager->GetRuntimeCueSet();
	if (!RuntimeSet)
	{
		return 0;
	}

	TArray<UClass*> HandlerClasses;
	GetDerivedClasses(UBRGameplayCue_Base::StaticClass(), HandlerClasses, true);

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
			continue;
		}

		const FSoftObjectPath ClassPath(HandlerClass);

		for (const FGameplayTag& Tag : HandledTags)
		{
			if (!Tag.IsValid())
			{
				continue;
			}
			CuesToAdd.Emplace(Tag, ClassPath);
			Expected.Emplace(Tag, HandlerClass);
		}
	}

	RuntimeSet->AddCues(CuesToAdd);

	int32 Verified = 0;
	for (const TPair<FGameplayTag, UClass*>& Entry : Expected)
	{
		const int32* DataIdx = RuntimeSet->GameplayCueDataMap.Find(Entry.Key);
		const bool bExact = DataIdx && RuntimeSet->GameplayCueData.IsValidIndex(*DataIdx)
			&& RuntimeSet->GameplayCueData[*DataIdx].GameplayCueTag == Entry.Key;

		if (bExact)
		{
			++Verified;
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
	GetDerivedClasses(UBRGameplayCue_Base::StaticClass(), HandlerClasses, true);

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
		return;
	}

	CueManager->StreamableManager.RequestAsyncLoad(Paths);
}
