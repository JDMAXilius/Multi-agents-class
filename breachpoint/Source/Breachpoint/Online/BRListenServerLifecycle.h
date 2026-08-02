// Breachpoint. The slice's IBRServerLifecycle: one process, one host, no migration.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/TimerHandle.h"
#include "Online/BRServerLifecycle.h"

#include "BRListenServerLifecycle.generated.h"

class UGameInstance;

UCLASS(Config = Game)
class BREACHPOINT_API UBRListenServerLifecycle : public UObject, public IBRServerLifecycle
{
	GENERATED_BODY()

public:
	virtual bool InitializeHosting(UGameInstance* InGameInstance) override;
	virtual void NotifyServerReadyForPlayers() override;
	virtual FBRJoinVerdict ValidateJoin(const FBRJoinRequest& Request) override;
	virtual void NotifyPlayerJoined(const FString& PlatformIdString) override;
	virtual void NotifyPlayerLeft(const FString& PlatformIdString) override;
	virtual void NotifyMatchComplete(const FBRMatchResultSummary& Summary) override;
	virtual void RequestHostingEnd(EBRHostingEndReason Reason) override;

	virtual FBRHostingEndingSignature& OnHostingEnding() override { return HostingEndingEvent; }
	virtual FBRHostingStateChangedSignature& OnHostingStateChanged() override { return HostingStateChangedEvent; }
	virtual FBRHostingHealthPulseSignature& OnHostingHealthPulse() override { return HostingHealthPulseEvent; }

	virtual EBRHostingState GetHostingState() const override { return HostingState; }
	virtual bool IsAcceptingPlayers() const override { return HostingState == EBRHostingState::Hosting; }

	virtual void BeginDestroy() override;

	int32 GetAdmittedPlayerCount() const { return AdmittedPlayerIds.Num(); }

	const FBRMatchResultSummary& GetLastMatchSummary() const { return LastMatchSummary; }

protected:
	UPROPERTY(Config)
	float HostingEndGraceSeconds = 2.f;

	UPROPERTY(Config)
	float DiagnosticHealthPulseIntervalSeconds = 0.f;

	void SetHostingState(EBRHostingState NewState);

	void ReturnRemotePlayersToMainMenu(const FText& ReasonText);

	void CompleteHostingEnd();

	void HandleEnginePreExit();

	void HandleDiagnosticHealthPulse();

	static FText MakeRemoteMessage(EBRHostingEndReason Reason);

	static FString MakeDiagnosticCode(EBRHostingEndReason Reason);

	UWorld* GetHostWorld() const;

private:
	UPROPERTY()
	TWeakObjectPtr<UGameInstance> GameInstance;

	EBRHostingState HostingState = EBRHostingState::Uninitialized;

	bool bHostingEndLatched = false;

	EBRHostingEndReason EndReason = EBRHostingEndReason::Unknown;

	FBRMatchResultSummary LastMatchSummary;

	TSet<FString> AdmittedPlayerIds;

	FTimerHandle GraceTimerHandle;
	FTimerHandle HealthPulseTimerHandle;

	FDelegateHandle EnginePreExitHandle;

	FBRHostingEndingSignature HostingEndingEvent;
	FBRHostingStateChangedSignature HostingStateChangedEvent;
	FBRHostingHealthPulseSignature HostingHealthPulseEvent;
};
