// Breachpoint. DataTable row structs — the ONE header for every DT_ row type.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "BRDataRows.generated.h"

class UStaticMesh;

UENUM(BlueprintType)
enum class EBRWeaponFireMode : uint8
{
	Automatic,
	SemiAuto
};

UENUM(BlueprintType)
enum class EBRDamageDelivery : uint8
{
	Hitscan,
	Projectile
};

USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRWeaponRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EBRWeaponFireMode FireMode = EBRWeaponFireMode::SemiAuto;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EBRDamageDelivery DamageDelivery = EBRDamageDelivery::Hitscan;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float DamagePerShot = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float RPM = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	int32 MagSize = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	int32 ReserveMags = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float ReloadTime_s = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float HeadshotMult = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float ProjectileSpeed = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float SplashRadius_m = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float SplashDamage = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float EquipTime_s = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float Range_m = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float Spread_deg = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSoftObjectPtr<class UBRAbilitySet> AbilitySet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSoftObjectPtr<UStaticMesh> MeshSoftPath;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FGameplayTag FireCueTag;

	static constexpr float CmPerMetre = 100.f;

	static constexpr float SecondsPerMinute = 60.f;

	float GetSplashRadiusCm() const { return SplashRadius_m * CmPerMetre; }

	float GetProjectileSpeedCmPerSecond() const { return ProjectileSpeed * CmPerMetre; }

	float GetMinShotIntervalSeconds() const
	{
		return (RPM > 0.f) ? (SecondsPerMinute / RPM) : 0.f;
	}

	int32 GetStartingReserveAmmo() const { return MagSize * ReserveMags; }

	bool ValidateSchema(FString& OutError) const
	{
		const bool bIsHitscan = (DamageDelivery == EBRDamageDelivery::Hitscan);
		if (bIsHitscan && ProjectileSpeed != 0.f)
		{
			OutError = TEXT("Hitscan row carries a non-zero ProjectileSpeed");
			return false;
		}
		if (!bIsHitscan && ProjectileSpeed <= 0.f)
		{
			OutError = TEXT("Projectile row carries no ProjectileSpeed");
			return false;
		}

		if (MagSize <= 0)
		{
			OutError = TEXT("MagSize must be positive");
			return false;
		}
		if (ReserveMags < 0)
		{
			OutError = TEXT("ReserveMags cannot be negative");
			return false;
		}

		if (RPM <= 0.f)
		{
			OutError = TEXT("RPM must be positive (it is the server's rate gate)");
			return false;
		}

		if (DamagePerShot < 0.f || ReloadTime_s < 0.f || EquipTime_s < 0.f || SplashDamage < 0.f
			|| SplashRadius_m < 0.f)
		{
			OutError = TEXT("A damage/time/radius column is negative");
			return false;
		}

		if (HeadshotMult < 1.f)
		{
			OutError = TEXT("HeadshotMult below 1.0 would make a headshot weaker than a body shot");
			return false;
		}

		if ((SplashRadius_m > 0.f) != (SplashDamage > 0.f))
		{
			OutError = TEXT("SplashRadius_m and SplashDamage must both be set or both be zero");
			return false;
		}

		return true;
	}
};

class UStateTree;

UENUM(BlueprintType)
enum class EBRSpotterAudience : uint8
{
	Self,
	Team,
	All
};

USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRSpotterLineRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotter")
	FName TriggerId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotter")
	FText Text;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotter")
	EBRSpotterAudience Audience = EBRSpotterAudience::All;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotter")
	float Weight = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spotter")
	float RepeatCooldown_s = 0.f;

	bool ValidateSchema(FString& OutError) const
	{
		if (TriggerId.IsNone())
		{
			OutError = TEXT("TriggerId is empty: this line can never be selected");
			return false;
		}
		if (Text.IsEmpty())
		{
			OutError = TEXT("Text is empty: a silent line is a missing line, not a design position");
			return false;
		}
		if (Weight < 0.f)
		{
			OutError = TEXT("Weight is negative (0 is the legal way to mute a line)");
			return false;
		}
		if (RepeatCooldown_s < 0.f)
		{
			OutError = TEXT("RepeatCooldown_s is negative");
			return false;
		}
		return true;
	}
};

USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRMedalRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Medal")
	FText MedalName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Medal")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Medal")
	FName TriggerId;

	bool ValidateSchema(FString& OutError) const
	{
		if (TriggerId.IsNone())
		{
			OutError = TEXT("TriggerId is empty: this medal can never be awarded");
			return false;
		}
		if (MedalName.IsEmpty())
		{
			OutError = TEXT("MedalName is empty");
			return false;
		}
		return true;
	}
};

USTRUCT()
struct BREACHPOINT_API FBRBotConsiderationRow
{
	GENERATED_BODY()

	UPROPERTY()
	FName Consideration;

	UPROPERTY()
	float Weight = 0.f;

	UPROPERTY()
	bool bInvert = false;
};

USTRUCT()
struct BREACHPOINT_API FBRBotAmbitionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY()
	float base_utility = 0.f;

	UPROPERTY()
	TArray<FBRBotConsiderationRow> considerations;

	UPROPERTY()
	FName personality_scalar;

	UPROPERTY()
	FString Requires;

	UPROPERTY()
	FString forbids;

	bool ValidateSchema(FString& OutError) const
	{
		if (base_utility < 0.f)
		{
			OutError = TEXT("base_utility is negative: an ambition cannot be worth less than nothing");
			return false;
		}
		for (const FBRBotConsiderationRow& Consideration : considerations)
		{
			if (Consideration.Consideration.IsNone())
			{
				OutError = TEXT("A consideration names no fact");
				return false;
			}
			if (Consideration.Weight < 0.f || Consideration.Weight > 1.f)
			{
				OutError = TEXT("A consideration weight is outside [0, 1] (weight is INFLUENCE, not a multiplier)");
				return false;
			}
		}
		return true;
	}
};

USTRUCT()
struct BREACHPOINT_API FBRBotTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	static constexpr int32 ReactionFloorMs = 200;

	int32 GetReactionMsClamped(int32 Jitter = 0) const
	{
		const int32 Raw = FMath::Max(reaction_ms, ReactionFloorMs) + FMath::Max(Jitter, 0);
		return QuantizeMs(Raw, reaction_quantum_ms);
	}

	static int32 QuantizeMs(int32 Value, int32 Quantum)
	{
		if (Quantum <= 0)
		{
			return Value;
		}
		return ((Value + Quantum - 1) / Quantum) * Quantum;
	}

	UPROPERTY()
	int32 reaction_quantum_ms = 0;

	UPROPERTY()
	int32 reaction_jitter_ms = 0;

	UPROPERTY()
	float accuracy_pct = 0.f;

	UPROPERTY()
	float aim_error_deg = 0.f;

	UPROPERTY()
	float switch_margin = 0.f;

	UPROPERTY()
	int32 commit_window_ms = 0;

	UPROPERTY()
	int32 commit_jitter_ms = 0;

	UPROPERTY()
	float rocket_contest = 0.f;

	UPROPERTY()
	float push_threshold = 0.f;

	UPROPERTY()
	float cover_preference = 0.f;

	UPROPERTY()
	float sight_radius_m = 0.f;

	UPROPERTY()
	float sight_fov_deg = 0.f;

	UPROPERTY()
	float target_memory_s = 0.f;

	UPROPERTY()
	int32 engage_update_ms = 0;

	UPROPERTY()
	TSoftObjectPtr<UStateTree> StateTreeSoftPath;

	bool ValidateSchema(FString& OutError) const
	{
		if (reaction_quantum_ms < 0 || reaction_jitter_ms < 0 || commit_window_ms < 0
			|| commit_jitter_ms < 0 || engage_update_ms < 0)
		{
			OutError = TEXT("A millisecond column is negative");
			return false;
		}
		if (accuracy_pct < 0.f || accuracy_pct > 1.f)
		{
			OutError = TEXT("accuracy_pct is outside [0, 1] (it is a fraction, not a percentage)");
			return false;
		}
		if (aim_error_deg < 0.f || aim_error_deg > 180.f)
		{
			OutError = TEXT("aim_error_deg is outside [0, 180]");
			return false;
		}
		if (switch_margin < 0.f)
		{
			OutError = TEXT("switch_margin is negative: a challenger would win by losing");
			return false;
		}
		if (sight_radius_m < 0.f || sight_fov_deg < 0.f || sight_fov_deg > 360.f)
		{
			OutError = TEXT("sight_radius_m / sight_fov_deg are outside their legal range");
			return false;
		}
		if (target_memory_s < 0.f)
		{
			OutError = TEXT("target_memory_s is negative");
			return false;
		}
		return true;
	}

private:
	UPROPERTY()
	int32 reaction_ms = 0;
};

USTRUCT(BlueprintType)
struct BREACHPOINT_API FBRMatchRulesRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	int32 ScoreLimit = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float MatchDuration_s = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float SuddenDeathCap_s = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float Warmup_s = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float RespawnDelay_s = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	float KillCreditWindow_s = 0.f;

	bool ValidateSchema(FString& OutError) const
	{
		if (ScoreLimit <= 0)
		{
			OutError = TEXT("ScoreLimit must be positive (0 would end the match at kickoff)");
			return false;
		}
		if (MatchDuration_s <= 0.f)
		{
			OutError = TEXT("MatchDuration_s must be positive");
			return false;
		}
		if (SuddenDeathCap_s <= 0.f)
		{
			OutError = TEXT("SuddenDeathCap_s must be positive ('no overtime' is a ruling, not a zero)");
			return false;
		}
		if (KillCreditWindow_s <= 0.f)
		{
			OutError = TEXT("KillCreditWindow_s must be positive (0 makes every kill instigator-less)");
			return false;
		}
		if (Warmup_s < 0.f || RespawnDelay_s < 0.f)
		{
			OutError = TEXT("Warmup_s / RespawnDelay_s cannot be negative");
			return false;
		}
		return true;
	}
};
