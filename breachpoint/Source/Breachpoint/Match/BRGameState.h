// Breachpoint. The replicated match view: phase, clock deadline, team scores, killfeed.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "GameplayTagContainer.h"

#include "BRGameState.generated.h"

class APlayerState;

namespace BRMatch
{
	inline constexpr uint8 NumTeams = 2;
	inline constexpr uint8 InvalidTeamId = 255;

	inline constexpr int32 KillFeedCapacity = 8;
}

UENUM()
enum class EBRMatchPhase : uint8
{
	None		UMETA(DisplayName = "None"),
	WarmUp		UMETA(DisplayName = "WarmUp"),
	Live		UMETA(DisplayName = "Live"),
	SuddenDeath	UMETA(DisplayName = "SuddenDeath"),
	PostMatch	UMETA(DisplayName = "PostMatch")
};

USTRUCT()
struct FBRKillFeedEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<APlayerState> Killer = nullptr;

	UPROPERTY()
	TObjectPtr<APlayerState> Victim = nullptr;

	UPROPERTY()
	uint8 KillerTeamId = BRMatch::InvalidTeamId;

	UPROPERTY()
	uint8 VictimTeamId = BRMatch::InvalidTeamId;

	UPROPERTY()
	FGameplayTag CauseTag;

	UPROPERTY()
	float ServerTime = 0.f;

	UPROPERTY()
	int32 Sequence = 0;

	UPROPERTY()
	uint8 bSelfInflicted : 1;

	UPROPERTY()
	uint8 bFriendlyFire : 1;

	FBRKillFeedEntry()
		: bSelfInflicted(0)
		, bFriendlyFire(0)
	{
	}
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FBRMatchPhaseChangedSignature, EBRMatchPhase, EBRMatchPhase);
DECLARE_MULTICAST_DELEGATE_TwoParams(FBRTeamScoreChangedSignature, uint8, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FBRKillFeedEntryAddedSignature, const FBRKillFeedEntry&);
DECLARE_MULTICAST_DELEGATE_OneParam(FBRMatchClockChangedSignature, float);

UCLASS()
class BREACHPOINT_API ABRGameState : public AGameState
{
	GENERATED_BODY()

public:
	ABRGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	EBRMatchPhase GetMatchPhase() const { return MatchPhase; }

	bool IsScoringOpen() const { return MatchPhase == EBRMatchPhase::Live || MatchPhase == EBRMatchPhase::SuddenDeath; }

	float GetMatchEndServerTime() const { return MatchEndServerTime; }

	bool IsClockRunning() const { return MatchEndServerTime > 0.f; }

	float GetSecondsRemaining() const;

	int32 GetTeamScore(uint8 TeamId) const;

	int32 GetScoreLimit() const { return ScoreLimit; }

	uint8 GetWinningTeamId() const { return WinningTeamId; }

	const TArray<FBRKillFeedEntry>& GetKillFeed() const { return KillFeed; }

	FBRMatchPhaseChangedSignature OnMatchPhaseChanged;
	FBRTeamScoreChangedSignature OnTeamScoreChanged;
	FBRKillFeedEntryAddedSignature OnKillFeedEntryAdded;
	FBRMatchClockChangedSignature OnMatchClockChanged;

	void ServerSetMatchPhase(EBRMatchPhase NewPhase);

	void ServerSetMatchEndServerTime(float NewEndServerTime);

	int32 ServerAddTeamScore(uint8 TeamId, int32 Delta);

	void ServerSetScoreLimit(int32 NewScoreLimit);

	void ServerSetWinningTeamId(uint8 TeamId);

	void ServerPushKillFeedEntry(FBRKillFeedEntry Entry);

protected:

	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase)
	EBRMatchPhase MatchPhase = EBRMatchPhase::None;

	UPROPERTY(ReplicatedUsing = OnRep_MatchEndServerTime)
	float MatchEndServerTime = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_TeamScores)
	TArray<int32> TeamScores;

	UPROPERTY(Replicated)
	int32 ScoreLimit = 0;

	UPROPERTY(Replicated)
	uint8 WinningTeamId = BRMatch::InvalidTeamId;

	UPROPERTY(ReplicatedUsing = OnRep_KillFeed)
	TArray<FBRKillFeedEntry> KillFeed;

	UFUNCTION()
	void OnRep_MatchPhase(EBRMatchPhase OldPhase);

	UFUNCTION()
	void OnRep_MatchEndServerTime();

	UFUNCTION()
	void OnRep_TeamScores(const TArray<int32>& OldScores);

	UFUNCTION()
	void OnRep_KillFeed();

private:
	int32 NextKillFeedSequence = 1;

	int32 LastAnnouncedKillFeedSequence = 0;
};
