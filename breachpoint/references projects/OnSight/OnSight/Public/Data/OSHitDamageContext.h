#pragma once
// Hit/death/attack context: FOSDeathEventInfo, FOSHitNetInfo, FOSGameplayEffectContext, attack/aura enums and payloads. Used by damage pipeline and replication.

#include "CoreMinimal.h"
#include "Data/OSCombatTypes.h"
#include "GameplayEffectTypes.h"
#include "UObject/Object.h"
#include "OnlineSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "EngineUtils.h"
#include "Utilities/NetPackingHelper.h"
#include "OSHitDamageContext.generated.h"

class AController;
class UWorld;

// --- Enums: Aura ---
UENUM(BlueprintType)
enum class EOSAuraSchool : uint8
{
	BASIC,
	ANTIMAGIC,
	MAGIC,
	ENERGY
};

UENUM(BlueprintType)
enum class EOSAuraTier : uint8
{
	NONE,
	LOW,
	MEDIUM,
	HIGH,
	FULL
};

// --- FOSAuraInfo ---
USTRUCT(BlueprintType)
struct FOSAuraInfo
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EOSAuraSchool School = EOSAuraSchool::BASIC;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EOSAuraTier Tier = EOSAuraTier::NONE;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Energy = 0.f;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		bool bSuccess = true;
		// Packed layout: low nibble = School (4 bits, up to 16 values), high nibble = Tier (4 bits, up to 16 values).
		// Previous packing gave School 6 bits (0x3F) and Tier 2 bits (<<6), which silently truncated
		// EOSAuraTier::FULL (value 4 = 0b100) to NONE on the wire — 5-value enum does not fit in 2 bits.
		// 4+4 split gives both enums headroom for future growth without widening the packed byte.
		uint8 Packed = 0;
		uint16 qEnergy = 0;
		if (Ar.IsSaving())
		{
			Packed |= (uint8)School & 0x0F;
			Packed |= (uint8(Tier) & 0x0F) << 4;
			qEnergy = QuantizeFloat(Energy);
		}
		Ar << qEnergy;
		Ar << Packed;
		if (Ar.IsLoading())
		{
			School = EOSAuraSchool(Packed & 0x0F);
			Tier = EOSAuraTier((Packed >> 4) & 0x0F);
			Energy = DequantizeFloat(qEnergy);
		}
		bOutSuccess = bSuccess;
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FOSAuraInfo> : public TStructOpsTypeTraitsBase2<FOSAuraInfo>
{
	enum { WithNetSerializer = true, WithCopy = true };
};

// --- FOSClashInfo ---
USTRUCT(BlueprintType)
struct FOSClashInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) EOSAttackType SelfType = EOSAttackType::None;
	UPROPERTY(BlueprintReadOnly) EOSAttackType OtherType = EOSAttackType::None;
	/** @deprecated Use SelfType. Kept for Chooser table compatibility (CT_OSClashes). */
	UPROPERTY(BlueprintReadOnly) EOSAttackType SelfSeverity = EOSAttackType::None;
	/** @deprecated Use OtherType. Kept for Chooser table compatibility (CT_OSClashes). */
	UPROPERTY(BlueprintReadOnly) EOSAttackType OtherSeverity = EOSAttackType::None;
	UPROPERTY(BlueprintReadOnly) FVector IncomingDir = FVector::ZeroVector;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		uint8 SelfTy = (uint8)SelfType;
		uint8 OtherTy = (uint8)OtherType;
		uint8 SelfSev = (uint8)SelfSeverity;
		uint8 OtherSev = (uint8)OtherSeverity;
		Ar << SelfTy;
		Ar << OtherTy;
		Ar << SelfSev;
		Ar << OtherSev;
		Ar << IncomingDir;
		if (Ar.IsLoading())
		{
			SelfType = (EOSAttackType)SelfTy;
			OtherType = (EOSAttackType)OtherTy;
			SelfSeverity = (EOSAttackType)SelfSev;
			OtherSeverity = (EOSAttackType)OtherSev;
		}
		bOutSuccess = true;
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FOSClashInfo> : public TStructOpsTypeTraitsBase2<FOSClashInfo>
{
	enum { WithNetSerializer = true };
};

// --- UOSClashInfoPayload ---
UCLASS()
class ONSIGHT_API UOSClashInfoPayload : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly) FOSClashInfo info;
};

// --- FOSAttackDataPayload ---
USTRUCT(BlueprintType)
struct FOSAttackDataPayload
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly)
	EOSAttackType Type = EOSAttackType::None;
	UPROPERTY(BlueprintReadOnly)
	FOSAuraInfo AuraInfo;
	UPROPERTY(BlueprintReadOnly)
	float Potency = 0.f;
	UPROPERTY(BlueprintReadOnly)
	FUniqueNetIdRepl InstigatorPlayerStateUniqueId;
	UPROPERTY(BlueprintReadOnly)
	FUniqueNetIdRepl VictimPlayerStateUniqueId;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		bool bSuccess = true;
		uint8 Packed = 0;
		uint16 qPotency = 0;
		const uint8 Ty = FMath::Clamp<uint8>(uint8(Type), 0, 15);
		if (Ar.IsSaving())
		{
			Packed = (uint8(Ty) & 0xF);
			qPotency = QuantizeFloat(Potency);
		}
		Ar << qPotency;
		Ar << Packed;
		AuraInfo.NetSerialize(Ar, Map, bSuccess);
		InstigatorPlayerStateUniqueId.NetSerialize(Ar, Map, bSuccess);
		VictimPlayerStateUniqueId.NetSerialize(Ar, Map, bSuccess);
		if (Ar.IsLoading())
		{
			Type = EOSAttackType(Packed & 0xF);
			Potency = DequantizeFloat(qPotency);
		}
		bOutSuccess = bSuccess;
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FOSAttackDataPayload> : public TStructOpsTypeTraitsBase2<FOSAttackDataPayload>
{
	enum { WithNetSerializer = true, WithCopy = true };
};

// --- FOSHitNetInfo ---
USTRUCT(BlueprintType)
struct FOSHitNetInfo
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly)
	FOSAttackDataPayload AttackInfo;
	UPROPERTY(BlueprintReadOnly)
	FVector_NetQuantize HitLocation = FVector::ZeroVector;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		bool bSuccess = true;
		AttackInfo.NetSerialize(Ar, Map, bOutSuccess);
		HitLocation.NetSerialize(Ar, Map, bOutSuccess);
		bOutSuccess = bSuccess;
		return true;
	}

	void SetAuraInfo(const FOSAuraInfo& aura) { AttackInfo.AuraInfo = aura; }
	void SetSchool(EOSAuraSchool school) { AttackInfo.AuraInfo.School = school; }
	FOSAuraInfo GetAuraInfo() const { return AttackInfo.AuraInfo; }
	EOSAuraSchool GetSchool() const { return AttackInfo.AuraInfo.School; }
};

template<>
struct TStructOpsTypeTraits<FOSHitNetInfo> : public TStructOpsTypeTraitsBase2<FOSHitNetInfo>
{
	enum { WithNetSerializer = true, WithCopy = true };
};

// --- FOSDeathEventInfo ---
USTRUCT(BlueprintType)
struct FOSDeathEventInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FOSHitNetInfo HitInfo;
	UPROPERTY(BlueprintReadOnly)
	FUniqueNetIdRepl InstigatorPlayerStateUniqueId;
	UPROPERTY(BlueprintReadOnly)
	FUniqueNetIdRepl VictimPlayerStateUniqueId;
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<APlayerState> InstigatorPS = nullptr;
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<APlayerState> VictimPS = nullptr;
	UPROPERTY(BlueprintReadWrite)
	bool bIsManualRespawn = false;

	FOSDeathEventInfo() {}
	FOSDeathEventInfo(const FOSHitNetInfo& InHitInfo, const FUniqueNetIdRepl& InInstigatorUniqueId, const FUniqueNetIdRepl& InVictimUniqueId, APlayerState* InInstigatorPlayerState = nullptr, APlayerState* InVictimPlayerState = nullptr)
		: HitInfo(InHitInfo)
		, InstigatorPlayerStateUniqueId(InInstigatorUniqueId)
		, VictimPlayerStateUniqueId(InVictimUniqueId)
		, InstigatorPS(InInstigatorPlayerState)
		, VictimPS(InVictimPlayerState)
	{}

	APlayerState* GetInstigatorPlayerState(const UWorld* World) const;
	APlayerState* GetVictimPlayerState(const UWorld* World) const;
	AController* GetInstigatorController(const UWorld* World) const;
};

// --- FOSGameplayEffectContext ---
USTRUCT()
struct FOSGameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

	UPROPERTY()
	FOSClashInfo ClashInfo;
	UPROPERTY()
	FOSHitNetInfo HitInfo;
	UPROPERTY()
	FName CueSocketName = NAME_None;

	FOSClashInfo GetClashInfo() const { return ClashInfo; }
	FOSHitNetInfo GetHitInfo() const { return HitInfo; }
	FName GetCueSocketName() const { return CueSocketName; }

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual FGameplayEffectContext* Duplicate() const override
	{
		FOSGameplayEffectContext* NewCtx = new FOSGameplayEffectContext();
		*NewCtx = *this;
		return NewCtx;
	}
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override
	{
		bOutSuccess = FGameplayEffectContext::NetSerialize(Ar, Map, bOutSuccess);
		ClashInfo.NetSerialize(Ar, Map, bOutSuccess);
		HitInfo.NetSerialize(Ar, Map, bOutSuccess);
		Ar << CueSocketName;
		return bOutSuccess;
	}
};

template<>
struct TStructOpsTypeTraits<FOSGameplayEffectContext> : public TStructOpsTypeTraitsBase2<FOSGameplayEffectContext>
{
	enum { WithNetSerializer = true };
};

/** Returns false if the handle is empty or the context is not FOSGameplayEffectContext. */
FORCEINLINE bool TryGetOSGameplayEffectContext(const FGameplayEffectContextHandle& Handle, const FOSGameplayEffectContext*& OutOS)
{
	const FGameplayEffectContext* const Raw = Handle.Get();
	if (!Raw || Raw->GetScriptStruct() != FOSGameplayEffectContext::StaticStruct())
	{
		OutOS = nullptr;
		return false;
	}
	OutOS = static_cast<const FOSGameplayEffectContext*>(Raw);
	return true;
}

/**
 * Mutable access after type verification. For contexts allocated as FOSGameplayEffectContext (project ASC / globals).
 * Handle.Get() is const-qualified; const_cast is limited to this verified downcast (same as prior static_cast usage).
 */
FORCEINLINE bool TryGetOSGameplayEffectContext(FGameplayEffectContextHandle& Handle, FOSGameplayEffectContext*& OutOS)
{
	const FGameplayEffectContext* const Raw = Handle.Get();
	if (!Raw || Raw->GetScriptStruct() != FOSGameplayEffectContext::StaticStruct())
	{
		OutOS = nullptr;
		return false;
	}
	OutOS = const_cast<FOSGameplayEffectContext*>(static_cast<const FOSGameplayEffectContext*>(Raw));
	return true;
}
