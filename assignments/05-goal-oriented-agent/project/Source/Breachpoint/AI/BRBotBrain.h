#pragma once

#include "AI/BRBotFacts.h"
#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"

#include "BRBotBrain.generated.h"

class UDataTable;
class UStateTree;

UENUM()
enum class EBRBotAmbition : uint8
{
	None,
	ControlRocket,
	WinThisFight,
	Survive,
	TakeGround,
	HuntWeapon,
	Roam,

	Count UMETA(Hidden)
};

BREACHPOINT_API const TCHAR* LexToString(EBRBotAmbition Ambition);

BREACHPOINT_API EBRBotAmbition BRBotAmbitionFromRowName(FName RowName);

UENUM()
enum class EBRBotStepVerb : uint8
{
	Idle,
	MoveTo,
	Hold,
	Engage,
	Flush,
	Reposition,
	Retreat,
	PickupOrContest,
	Regroup,

	Count UMETA(Hidden)
};

BREACHPOINT_API const TCHAR* LexToString(EBRBotStepVerb Verb);

UENUM()
enum class EBRBotAnchor : uint8
{
	None,
	Target,
	Cover,
	RocketPad,
	FiringPoint,
	Perch,
	Teammate,
	Landmark,

	Count UMETA(Hidden)
};

BREACHPOINT_API const TCHAR* LexToString(EBRBotAnchor Anchor);

USTRUCT()
struct BREACHPOINT_API FBRBotConsiderationDef
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
struct BREACHPOINT_API FBRBotAmbitionDef
{
	GENERATED_BODY()

	UPROPERTY()
	FName RowName;

	UPROPERTY()
	EBRBotAmbition Ambition = EBRBotAmbition::None;

	UPROPERTY()
	float BaseUtility = 0.f;

	UPROPERTY()
	TArray<FBRBotConsiderationDef> Considerations;

	UPROPERTY()
	FName PersonalityScalar;

	UPROPERTY()
	uint32 RequiresMask = 0;

	UPROPERTY()
	uint32 ForbidsMask = 0;
};

USTRUCT()
struct BREACHPOINT_API FBRBotTierScalars
{
	GENERATED_BODY()

	UPROPERTY()
	FName TierName;

	UPROPERTY()
	int32 ReactionMs = 0;

	UPROPERTY()
	int32 ReactionQuantumMs = 0;

	UPROPERTY()
	int32 ReactionJitterMs = 0;

	UPROPERTY()
	float AccuracyPct = 0.f;

	UPROPERTY()
	float AimErrorDeg = 0.f;

	UPROPERTY()
	float SwitchMargin = 0.f;

	UPROPERTY()
	int32 CommitWindowMs = 0;

	UPROPERTY()
	int32 CommitJitterMs = 0;

	UPROPERTY()
	float RocketContest = 0.f;

	UPROPERTY()
	float PushThreshold = 0.f;

	UPROPERTY()
	float CoverPreference = 0.f;

	UPROPERTY()
	float SightRadius_m = 0.f;
	UPROPERTY()
	float SightFov_deg = 0.f;

	UPROPERTY()
	float TargetMemory_s = 0.f;

	UPROPERTY()
	int32 EngageUpdateMs = 0;

	UPROPERTY()
	TSoftObjectPtr<UStateTree> StateTreeSoftPath;

	static constexpr int32 ReactionFloorMs = 200;

	int32 GetReactionMsClamped(int32 Jitter = 0) const
	{
		const int32 Raw = FMath::Max(ReactionMs, ReactionFloorMs) + FMath::Max(Jitter, 0);
		return QuantizeMs(Raw, ReactionQuantumMs);
	}

	static int32 QuantizeMs(int32 Value, int32 Quantum)
	{
		if (Quantum <= 0)
		{
			return Value;
		}
		return ((Value + Quantum - 1) / Quantum) * Quantum;
	}

	bool ResolvePersonalityScalar(FName ScalarName, float& OutValue) const;

	bool ValidateSchema(FString& OutError) const;
};

USTRUCT()
struct BREACHPOINT_API FBRBotPlanStep
{
	GENERATED_BODY()

	UPROPERTY()
	EBRBotStepVerb Verb = EBRBotStepVerb::Idle;

	UPROPERTY()
	EBRBotAnchor Anchor = EBRBotAnchor::None;

	UPROPERTY()
	uint32 RequiresMask = 0;

	UPROPERTY()
	uint32 ForbidsMask = 0;

	bool IsValidUnder(const FBRBotFacts& Facts) const
	{
		return Facts.HasAll(RequiresMask) && Facts.HasNone(ForbidsMask);
	}
};

inline constexpr int32 BRBotMaxPlanSteps = 3;

USTRUCT()
struct BREACHPOINT_API FBRBotPlan
{
	GENERATED_BODY()

	UPROPERTY()
	EBRBotAmbition Ambition = EBRBotAmbition::None;

	UPROPERTY()
	TArray<FBRBotPlanStep> Steps;

	UPROPERTY()
	int32 CurrentStep = INDEX_NONE;

	bool IsValid() const { return Steps.Num() > 0 && Steps.IsValidIndex(CurrentStep); }

	const FBRBotPlanStep& GetCurrentStep() const
	{
		static const FBRBotPlanStep IdleStep;
		return Steps.IsValidIndex(CurrentStep) ? Steps[CurrentStep] : IdleStep;
	}

	void Reset()
	{
		Ambition = EBRBotAmbition::None;
		Steps.Reset();
		CurrentStep = INDEX_NONE;
	}
};

USTRUCT()
struct BREACHPOINT_API FBRBotDecision
{
	GENERATED_BODY()

	UPROPERTY()
	EBRBotEventType Cause = EBRBotEventType::Spawned;

	UPROPERTY()
	EBRBotAmbition Ambition = EBRBotAmbition::None;

	UPROPERTY()
	bool bAmbitionChanged = false;

	UPROPERTY()
	bool bReplanned = false;

	UPROPERTY()
	FBRBotPlanStep Step;

	UPROPERTY()
	float Utility = 0.f;

	UPROPERTY()
	float AtSeconds = 0.f;

	FString ToTraceString() const;
};

UCLASS()
class BREACHPOINT_API UBRBotBrain : public UObject
{
	GENERATED_BODY()

public:

	void Initialize(int32 InSeed, const FBRBotTierScalars& InScalars, const TArray<FBRBotAmbitionDef>& InAmbitions);

	bool IsInitialized() const { return bInitialized; }

	FBRBotDecision Think(const FBRBotEvent& Event, const FBRBotFacts& Facts);

	FBRBotDecision NotifyStepCompleted(const FBRBotFacts& Facts);

	FBRBotDecision NotifyStepFailed(const FBRBotFacts& Facts);

	EBRBotAmbition GetActiveAmbition() const { return ActivePlan.Ambition; }
	const FBRBotPlan& GetActivePlan() const { return ActivePlan; }
	const FBRBotPlanStep& GetCurrentStep() const { return ActivePlan.GetCurrentStep(); }
	const FBRBotDecision& GetLastDecision() const { return LastDecision; }
	const FBRBotTierScalars& GetScalars() const { return Scalars; }

	int32 DrawReactionDelayMs();

	int32 DrawCommitWindowMs();

	FRandomStream& GetStream() { return Stream; }

	const TArray<float>& GetLastUtilities() const { return LastUtilities; }

	FString DescribeLastDecision() const;

	static bool ReadAmbitionDefs(const UDataTable* AmbitionsTable, TArray<FBRBotAmbitionDef>& OutDefs, FString& OutError);

	static bool ReadTierScalars(const UDataTable* TuningTable, FName TierRowName, FBRBotTierScalars& OutScalars, FString& OutError);

private:

	float ScoreAmbition(const FBRBotAmbitionDef& Def, const FBRBotFacts& Facts) const;

	EBRBotAmbition SelectAmbition(const FBRBotFacts& Facts, float& OutUtility, bool& bOutChanged);

	FBRBotPlan BuildPlan(EBRBotAmbition Ambition, const FBRBotFacts& Facts);

	bool IsPlanContradicted(const FBRBotFacts& Facts) const;

	UPROPERTY()
	TArray<FBRBotAmbitionDef> AmbitionDefs;

	UPROPERTY()
	FBRBotTierScalars Scalars;

	UPROPERTY()
	FBRBotPlan ActivePlan;

	UPROPERTY()
	FBRBotDecision LastDecision;

	UPROPERTY()
	TArray<float> LastUtilities;

	float AmbitionAdoptedAtSeconds = 0.f;

	float CurrentCommitWindowSeconds = 0.f;

	FRandomStream Stream;

	bool bInitialized = false;
};
