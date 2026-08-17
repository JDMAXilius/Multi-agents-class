#pragma once

#include "CoreMinimal.h"

#include "BRBotFacts.generated.h"

UENUM()
enum class EBRBotPrecondition : uint8
{
	TargetVisible = 0,
	TargetKnown,
	MeleeInRange,

	HoldingPowerWeapon,
	PowerWeaponAvailable,

	MyShieldsBroken,
	TargetShieldsBroken,

	GrenadeReady,
	GrappleReady,
	HasAmmo,
	ReloadNeeded,

	InCover,
	TeammateNearby,
	Outnumbered,

	RocketWindowOpen,
	Warmup,
	SelfDead,

	Count UMETA(Hidden)
};

inline constexpr uint32 BRBotBit(EBRBotPrecondition Precondition)
{
	return 1u << static_cast<uint32>(Precondition);
}

inline constexpr uint32 BRBotNoPreconditions = 0u;

UENUM()
enum class EBRBotEventType : uint8
{
	Spawned,

	TargetPerceived,
	TargetLost,

	DamageTaken,
	SelfShieldsBroken,
	TargetShieldsBroken,
	CombatantDied,

	RocketWindowOpened,
	RocketTaken,
	ScoreChanged,
	MatchPhaseChanged,

	StepCompleted,
	StepFailed
};

USTRUCT()
struct BREACHPOINT_API FBRBotEvent
{
	GENERATED_BODY()

	UPROPERTY()
	EBRBotEventType Type = EBRBotEventType::Spawned;

	UPROPERTY()
	int32 SubjectId = INDEX_NONE;
};

USTRUCT()
struct BREACHPOINT_API FBRBotFacts
{
	GENERATED_BODY()

	UPROPERTY() float MyShieldNorm = 1.f;
	UPROPERTY() float MyHealthNorm = 1.f;
	UPROPERTY() float MyAmmoNorm = 1.f;

	UPROPERTY() float TargetShieldNorm = 0.f;

	UPROPERTY() float DistToTargetNorm = 0.f;

	UPROPERTY() float ThreatExposureNorm = 0.f;

	UPROPERTY() float RocketTimerNorm = 0.f;
	UPROPERTY() float DistToRocketNorm = 0.f;

	UPROPERTY() float TeammatesAliveNorm = 1.f;
	UPROPERTY() float EnemiesAliveNorm = 1.f;
	UPROPERTY() float CoverQualityNorm = 0.f;

	UPROPERTY() float ScoreDeficitNorm = 0.f;
	UPROPERTY() float TimeRemainingNorm = 1.f;

	UPROPERTY() uint32 PreconditionMask = 0;

	UPROPERTY() int32 TargetId = INDEX_NONE;

	UPROPERTY() float NowSeconds = 0.f;

	bool Has(EBRBotPrecondition Precondition) const
	{
		return (PreconditionMask & BRBotBit(Precondition)) != 0;
	}

	bool HasAll(uint32 Mask) const { return (PreconditionMask & Mask) == Mask; }
	bool HasNone(uint32 Mask) const { return (PreconditionMask & Mask) == 0; }

	void Set(EBRBotPrecondition Precondition, bool bValue)
	{
		const uint32 BitValue = BRBotBit(Precondition);
		PreconditionMask = bValue ? (PreconditionMask | BitValue) : (PreconditionMask & ~BitValue);
	}

	bool ResolveScalar(FName ConsiderationName, float& OutValue) const
	{
		if (const float FBRBotFacts::* Member = ScalarRegistry().FindRef(ConsiderationName))
		{
			OutValue = this->*Member;
			return true;
		}
		return false;
	}

	static const TMap<FName, float FBRBotFacts::*>& ScalarRegistry()
	{
		static const TMap<FName, float FBRBotFacts::*> Registry = {
			{ FName("my_shield_norm"),       &FBRBotFacts::MyShieldNorm },
			{ FName("my_health_norm"),       &FBRBotFacts::MyHealthNorm },
			{ FName("my_ammo_norm"),         &FBRBotFacts::MyAmmoNorm },
			{ FName("enemy_shield_norm"),    &FBRBotFacts::TargetShieldNorm },
			{ FName("dist_to_target_norm"),  &FBRBotFacts::DistToTargetNorm },
			{ FName("target_exposure_norm"), &FBRBotFacts::ThreatExposureNorm },
			{ FName("rocket_timer_norm"),    &FBRBotFacts::RocketTimerNorm },
			{ FName("dist_to_rocket_norm"),  &FBRBotFacts::DistToRocketNorm },
			{ FName("teammates_alive_norm"), &FBRBotFacts::TeammatesAliveNorm },
			{ FName("enemies_alive_norm"),   &FBRBotFacts::EnemiesAliveNorm },
			{ FName("cover_quality_norm"),   &FBRBotFacts::CoverQualityNorm },
			{ FName("score_deficit_norm"),   &FBRBotFacts::ScoreDeficitNorm },
			{ FName("time_remaining_norm"),  &FBRBotFacts::TimeRemainingNorm }
		};
		return Registry;
	}

	bool ValidateNormalized(FString& OutError) const
	{
		for (const TPair<FName, float FBRBotFacts::*>& Entry : ScalarRegistry())
		{
			const float Value = this->*(Entry.Value);
			if (!FMath::IsFinite(Value) || Value < 0.f || Value > 1.f)
			{
				OutError = FString::Printf(TEXT("Fact '%s' = %f is outside [0,1]"),
					*Entry.Key.ToString(), Value);
				return false;
			}
		}
		return true;
	}
};
