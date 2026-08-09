#pragma once

#include "CoreMinimal.h"
#include "AkAudioEvent.h"
#include "AkRtpc.h"
#include "AkStateValue.h"
#include "AkSwitchValue.h"
#include "GameplayTagContainer.h"
#include "Data/OSHitDamageContext.h"
#include "FX/ParamSet/OSNiagaraParamSet.h"
#include "FX/ParamSet/Wrapper/OSNiagaraParamSetAsset.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "OSFXTypes.generated.h"

class UOSWwisePreset;
class UOSWwisePresetAsset;
struct FOSAuraInfo;

UENUM(BlueprintType)
enum class EOSFXSpawnSpace : uint8
{
	World,
	AttachToInstigator,
	AttachToTarget
};

UENUM(BlueprintType)
enum class EOSFXPlayerContext : uint8
{
	LOCAL,
	REMOTE,
	AUTO
};

USTRUCT(BlueprintType)
struct FOSNiagaraParamPack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, float> FloatParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, FVector> VectorParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, FLinearColor> ColorParams;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TObjectPtr<UMaterialInterface>> Materials;
};

USTRUCT(BlueprintType)
struct FOSWwiseEventPair
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UAkAudioEvent> Local = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UAkAudioEvent> Remote = nullptr;

	UAkAudioEvent* Resolve(bool bIsLocalControlled) const
	{
		return bIsLocalControlled ? Local : Remote;
	}
	
	bool IsValid() const
	{
		if (Local || Remote) return true;
		return false;
	}
	
	bool HasLocal() const
	{
		return Local ? true : false;
	}
	
	bool HasRemote() const
	{
		return Remote ? true : false;
	}
};

USTRUCT(BlueprintType)
struct FOSWwiseParamPack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer AudioTags;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FOSWwiseEventPair EventPair;
	
	// rtpc | value
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<TObjectPtr<UAkRtpc>, float> RTPC;
	// switch group
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TObjectPtr<UAkSwitchValue>> Switches;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TObjectPtr<UAkStateValue>> States;
	
	// override for audio event selection
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UAkAudioEvent> EventOverride = nullptr;
	
	bool IsValid() const
	{
		return EventPair.Local || EventPair.Remote || EventOverride;
	}
	
	bool HasPair() const
	{
		return EventPair.Local || EventPair.Remote;
	}
	
	void MergeFrom(const FOSWwiseParamPack& Other)
	{
		if (Other.HasPair())
		{
			EventPair = Other.EventPair;
		}

		// last-wins
		for (const auto& It : Other.RTPC)
		{
			if (!It.Key) continue;
			RTPC.Add(It.Key, It.Value);
		}

		// append unique
		for (UAkSwitchValue* S : Other.Switches)
		{
			if (!S) continue;
			Switches.AddUnique(S);
		}
		for (UAkStateValue* S : Other.States)
		{
			if (!S) continue;
			States.AddUnique(S);
		}

		if (Other.EventOverride) EventOverride = Other.EventOverride;
		//if (Other.RTPCInterpolationMs != 0) RTPCInterpolationMs = Other.RTPCInterpolationMs;
	}
};

USTRUCT(BlueprintType)
struct FOSAuraTierAudioSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EOSAuraTier Tier = EOSAuraTier::NONE;

	// optional: baseline params to merge into ctx.AudioParam
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UOSWwisePreset> Preset = nullptr;
};

USTRUCT(BlueprintType)
struct FOSFXContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	TWeakObjectPtr<AActor> Instigator;

	UPROPERTY(BlueprintReadWrite)
	TWeakObjectPtr<AActor> Target;

	UPROPERTY(BlueprintReadWrite)
	FTransform WorldTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite)
	FVector Normal = FVector::UpVector;

	UPROPERTY(BlueprintReadWrite)
	TWeakObjectPtr<USceneComponent> AttachComponentOverride;

	UPROPERTY(BlueprintReadWrite)
	FName AttachSocketOverride = NAME_None;
	
	UPROPERTY(BlueprintReadWrite)
	FOSWwiseParamPack AudioParam;
	
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UOSNiagaraParamSet> NiagaraParam;

	UPROPERTY(BlueprintReadWrite)
	FGameplayTagContainer Tags;
};

USTRUCT()
struct FOSFXContextLite
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag EventTag;

	UPROPERTY()
	FGameplayTagContainer Tags;

	UPROPERTY()
	FVector_NetQuantize10 Location = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantizeNormal Normal = FVector::UpVector;

	UPROPERTY()
	float Magnitude = 0.f;

	UPROPERTY()
	TObjectPtr<AActor> Instigator = nullptr;

	UPROPERTY()
	TObjectPtr<AActor> Target = nullptr;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		bOutSuccess = true;

		Ar << EventTag;
		Tags.NetSerialize(Ar, Map, bOutSuccess);

		Ar << Location;
		Ar << Normal;
		Ar << Magnitude;
		//Ar << InstigatorSocket;
		//Ar << TargetSocket;
//
		Ar << Instigator;
		Ar << Target;

		return bOutSuccess;
	}
};

template<>
struct TStructOpsTypeTraits<FOSFXContextLite> : public TStructOpsTypeTraitsBase2<FOSFXContextLite>
{
	enum { WithNetSerializer = true };
};

inline FOSFXContext MakeContextFromLite(const FOSFXContextLite& lite)
{
	FOSFXContext ctx;
	ctx.Instigator = lite.Instigator;
	ctx.Target = lite.Target;
	ctx.Tags = lite.Tags;
	ctx.WorldTransform = FTransform(FRotator::ZeroRotator, FVector(lite.Location), FVector::OneVector);
	ctx.Normal = FVector(lite.Normal);
	//ctx.Magnitude = lite.Magnitude;
	//ctx.InstigatorSocket = lite.InstigatorSocket;
	//ctx.TargetSocket = lite.TargetSocket;
	return ctx;
}

USTRUCT(BlueprintType)
struct FOSAuraTierParamSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EOSAuraTier Tier = EOSAuraTier::NONE;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UOSNiagaraParamSetAsset> DefaultParamsAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, meta=(EditCondition="DefaultParamsAsset==nullptr", EditConditionHides))
	TObjectPtr<UOSNiagaraParamSet> AuraParams = nullptr;
};

UENUM(BlueprintType)
enum class EOSFXNiagaraStopMode : uint8
{
	DEACTIVATE,
	DESTROY
};

USTRUCT()
struct FOSActiveNiagaraSlot
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Slot;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> Comp = nullptr;
};

UENUM(BlueprintType)
enum class EOSFXNiagaraReuseMode : uint8
{
	SPAWN_NEW,
	REPLACE_EXISTING,
	UPDATE_EXISTING
};

USTRUCT(BlueprintType)
struct FOSFXChooserParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer Tags;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FOSAuraInfo AuraInfo;
};