// Breachpoint. Match telemetry collection: events, not opinions. Authority-only, no PII, no tick.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "Online/BRServerLifecycle.h"

#include "BRTelemetrySubsystem.generated.h"

class AController;
class AGameModeBase;
class APlayerController;
class APlayerState;
enum class EBRMatchPhase : uint8;
struct FBRKillFeedEntry;

UENUM()
enum class EBRMatchTelemetryOutcome : uint8
{
	InProgress,
	Completed,
	AbandonedByHost,
	HostLost,
	PlatformEnded,
	Fault
};

USTRUCT()
struct FBRTelemetryEvent
{
	GENERATED_BODY()

	UPROPERTY()
	FName EventId;

	UPROPERTY()
	float ServerTime = 0.f;

	UPROPERTY()
	int32 SubjectKey = 0;

	UPROPERTY()
	int32 ObjectKey = 0;

	UPROPERTY()
	float Value = 0.f;

	UPROPERTY()
	FName Detail;
};

USTRUCT()
struct FBRPlayerMatchTelemetry
{
	GENERATED_BODY()

	UPROPERTY()
	int32 PlayerKey = 0;

	UPROPERTY()
	uint8 TeamId = 255;

	UPROPERTY()
	bool bIsBot = false;

	UPROPERTY()
	bool bJoinedInProgress = false;

	UPROPERTY()
	int32 Kills = 0;

	UPROPERTY()
	int32 Deaths = 0;

	UPROPERTY()
	int32 Assists = 0;

	UPROPERTY()
	int32 SelfInflictedDeaths = 0;

	UPROPERTY()
	int32 FriendlyFireKills = 0;

	UPROPERTY()
	float TimeInMatchSeconds = 0.f;

	UPROPERTY()
	bool bLeftEarly = false;
};

USTRUCT()
struct FBRMatchTelemetryRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid MatchId;

	UPROPERTY()
	FName MapName;

	UPROPERTY()
	EBRMatchTelemetryOutcome Outcome = EBRMatchTelemetryOutcome::InProgress;

	UPROPERTY()
	float MatchDurationSeconds = 0.f;

	UPROPERTY()
	float WarmupDurationSeconds = 0.f;

	UPROPERTY()
	uint8 WinningTeamId = 255;

	UPROPERTY()
	int32 HumanPlayerCount = 0;

	UPROPERTY()
	int32 BotPlayerCount = 0;

	UPROPERTY()
	int32 JoinInProgressCount = 0;

	UPROPERTY()
	int32 EarlyDepartureCount = 0;

	UPROPERTY()
	TArray<FBRPlayerMatchTelemetry> Players;

	UPROPERTY()
	TArray<FBRTelemetryEvent> Events;

	UPROPERTY()
	int32 DroppedEventCount = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FBRMatchTelemetryFinalizedSignature, const FBRMatchTelemetryRecord&);

DECLARE_MULTICAST_DELEGATE_OneParam(FBRTelemetryEventRecordedSignature, const FBRTelemetryEvent&);

UCLASS(Config = Game)
class BREACHPOINT_API UBRTelemetrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	void RecordEvent(const FBRTelemetryEvent& Event);

	void RecordPlayerEvent(FName EventId, const APlayerState* Subject, const APlayerState* Object,
		float Value, FName Detail);

	int32 GetPlayerKey(const APlayerState* Player);

	const FBRMatchTelemetryRecord& GetMatchRecord() const { return Record; }

	bool IsFinalized() const { return bFinalized; }

	FBRMatchTelemetryFinalizedSignature OnMatchTelemetryFinalized;
	FBRTelemetryEventRecordedSignature OnTelemetryEventRecorded;

protected:
	UPROPERTY(Config)
	int32 MaxRetainedEvents = 2048;

	UPROPERTY(Config)
	bool bTelemetryEnabled = true;

private:
	bool HasTelemetryAuthority() const;

	void TryBindMatchSources();

	void TryBindServerLifecycle();

	void HandlePlayerKilled(APlayerState* Killer, APlayerState* Victim, const TArray<APlayerState*>& Assists);
	void HandleMatchPhaseChanged(EBRMatchPhase OldPhase, EBRMatchPhase NewPhase);
	void HandleKillFeedEntryAdded(const FBRKillFeedEntry& Entry);
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void HandleLogout(AGameModeBase* GameMode, AController* Exiting);
	void HandleHostingEnding(const FBRHostingEndNotice& Notice);

	FBRPlayerMatchTelemetry* FindOrAddPlayerRow(const APlayerState* Player);

	FBRPlayerMatchTelemetry* FindPlayerRowByKey(int32 PlayerKey);

	void FinalizeRecord(EBRMatchTelemetryOutcome Outcome);

	float GetServerTime() const;

	FBRMatchTelemetryRecord Record;

	TMap<FObjectKey, int32> PlayerKeys;

	int32 NextPlayerKey = 1;

	bool bFinalized = false;
	bool bMatchSourcesBound = false;
	bool bLifecycleBound = false;

	float LiveStartServerTime = 0.f;
	float WarmupStartServerTime = 0.f;

	bool bMatchIsLive = false;

	FDelegateHandle PlayerKilledHandle;
	FDelegateHandle PhaseChangedHandle;
	FDelegateHandle KillFeedHandle;
	FDelegateHandle PostLoginHandle;
	FDelegateHandle LogoutHandle;
	FDelegateHandle HostingEndingHandle;

	UPROPERTY()
	TScriptInterface<IBRServerLifecycle> BoundLifecycle;
};
