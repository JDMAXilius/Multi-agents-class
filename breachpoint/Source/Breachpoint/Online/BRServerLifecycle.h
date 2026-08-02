// Breachpoint. THE SEAM: everything that changes when the server stops being a listen host.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "BRServerLifecycle.generated.h"

class APlayerController;
class UGameInstance;
class UWorld;

UENUM()
enum class EBRHostingEndReason : uint8
{
	Unknown,

	MatchComplete,

	HostQuit,

	HostConnectionLost,

	PlatformRequested,

	Fault
};

UENUM()
enum class EBRHostingState : uint8
{
	Uninitialized,
	Initializing,
	Hosting,
	Ending,
	Ended
};

UENUM()
enum class EBRJoinAdmission : uint8
{
	Accepted,
	Rejected
};

UENUM()
enum class EBRJoinTargetKind : uint8
{
	Invalid,

	SessionHandle,

	Address
};

USTRUCT()
struct FBRJoinCredential
{
	GENERATED_BODY()

	UPROPERTY()
	FString Token;

	bool IsEmpty() const { return Token.IsEmpty(); }

	FString ToUrlOption() const
	{
		return Token.IsEmpty() ? FString() : FString::Printf(TEXT("%s=%s"), UrlOptionKey, *Token);
	}

	static constexpr const TCHAR* UrlOptionKey = TEXT("BRJoin");
};

USTRUCT()
struct FBRJoinTarget
{
	GENERATED_BODY()

	UPROPERTY()
	EBRJoinTargetKind Kind = EBRJoinTargetKind::Invalid;

	UPROPERTY()
	int32 SessionHandleId = INDEX_NONE;

	UPROPERTY()
	FString ConnectAddress;

	UPROPERTY()
	FBRJoinCredential Credential;

	UPROPERTY()
	FText DisplayLabel;

	UPROPERTY()
	int32 OpenSlots = -1;

	UPROPERTY()
	int32 PingMs = -1;

	bool IsValid() const
	{
		switch (Kind)
		{
		case EBRJoinTargetKind::SessionHandle:	return SessionHandleId != INDEX_NONE;
		case EBRJoinTargetKind::Address:		return !ConnectAddress.IsEmpty();
		default:								return false;
		}
	}

	FString ToLogString() const
	{
		switch (Kind)
		{
		case EBRJoinTargetKind::SessionHandle:
			return FString::Printf(TEXT("JoinTarget(handle #%d, slots=%d, ping=%d, cred=%s)"),
				SessionHandleId, OpenSlots, PingMs, Credential.IsEmpty() ? TEXT("none") : TEXT("present"));
		case EBRJoinTargetKind::Address:
			return FString::Printf(TEXT("JoinTarget(address %s, slots=%d, ping=%d, cred=%s)"),
				*ConnectAddress, OpenSlots, PingMs, Credential.IsEmpty() ? TEXT("none") : TEXT("present"));
		default:
			return TEXT("JoinTarget(invalid)");
		}
	}
};

USTRUCT()
struct FBRJoinRequest
{
	GENERATED_BODY()

	UPROPERTY()
	FString Options;

	UPROPERTY()
	FString RemoteAddress;

	UPROPERTY()
	FString PlatformIdString;

	UPROPERTY()
	FBRJoinCredential Credential;

	UPROPERTY()
	bool bJoinInProgress = false;
};

USTRUCT()
struct FBRJoinVerdict
{
	GENERATED_BODY()

	UPROPERTY()
	EBRJoinAdmission Admission = EBRJoinAdmission::Accepted;

	UPROPERTY()
	FText DenialReason;

	UPROPERTY()
	FString DenialCode;

	bool IsAccepted() const { return Admission == EBRJoinAdmission::Accepted; }

	static FBRJoinVerdict Accept()
	{
		return FBRJoinVerdict();
	}

	static FBRJoinVerdict Reject(const FText& Reason, const FString& Code)
	{
		FBRJoinVerdict Verdict;
		Verdict.Admission = EBRJoinAdmission::Rejected;
		Verdict.DenialReason = Reason;
		Verdict.DenialCode = Code;
		return Verdict;
	}
};

USTRUCT()
struct FBRHostingEndNotice
{
	GENERATED_BODY()

	UPROPERTY()
	EBRHostingEndReason Reason = EBRHostingEndReason::Unknown;

	UPROPERTY()
	float GraceSeconds = 0.f;

	UPROPERTY()
	FText RemoteMessage;

	UPROPERTY()
	FString DiagnosticCode;
};

USTRUCT()
struct FBRMatchResultSummary
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 WinningTeamId = 255;

	UPROPERTY()
	float MatchDurationSeconds = 0.f;

	UPROPERTY()
	int32 HumanPlayerCount = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FBRHostingEndingSignature, const FBRHostingEndNotice&);

DECLARE_MULTICAST_DELEGATE_TwoParams(FBRHostingStateChangedSignature, EBRHostingState, EBRHostingState);

DECLARE_MULTICAST_DELEGATE_OneParam(FBRHostingHealthPulseSignature, double);

UINTERFACE(MinimalAPI, NotBlueprintable, meta = (CannotImplementInterfaceInBlueprint))
class UBRServerLifecycle : public UInterface
{
	GENERATED_BODY()
};

class IBRServerLifecycle
{
	GENERATED_BODY()

public:

	virtual bool InitializeHosting(UGameInstance* GameInstance) = 0;

	virtual void NotifyServerReadyForPlayers() = 0;

	virtual FBRJoinVerdict ValidateJoin(const FBRJoinRequest& Request) = 0;

	virtual void NotifyPlayerJoined(const FString& PlatformIdString) = 0;

	virtual void NotifyPlayerLeft(const FString& PlatformIdString) = 0;

	virtual void NotifyMatchComplete(const FBRMatchResultSummary& Summary) = 0;

	virtual void RequestHostingEnd(EBRHostingEndReason Reason) = 0;

	virtual FBRHostingEndingSignature& OnHostingEnding() = 0;
	virtual FBRHostingStateChangedSignature& OnHostingStateChanged() = 0;
	virtual FBRHostingHealthPulseSignature& OnHostingHealthPulse() = 0;

	virtual EBRHostingState GetHostingState() const = 0;

	virtual bool IsAcceptingPlayers() const = 0;
};
