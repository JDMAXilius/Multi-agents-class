// Copyright Optimum Athena. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OSActor.h"
#include "Components/ShapeComponent.h"
#include "Components/OSAudioComponent.h"
#include "OSControlPoint.generated.h"

class AOSPlayerState;
class AOSCharacter;
class AOSGameMode;
class AOSGameMode_Domination;

/**
 * Capture state of a control point. Absolute — same value on all clients.
 * For friend/enemy perspective (audio, colors), use GetLocalClientTeamRelationship() or
 * GetDisplayColorForTeam(). Do NOT bake viewer perspective into this enum.
 */
UENUM(BlueprintType)
enum EControlPointState : uint8
{
	Uninitialized = 0,  // Initial state before registration
	Neutral,            // No team owns or is capturing this point
	Capturing,          // A team is actively gaining capture progress
	Contested,          // Multiple teams present — progress frozen
	Neutralizing,       // A team is draining an owned point toward neutral
	Captured            // A team owns this point
};

// Fires every capture tick and on replication — high-frequency progress updates for UI (progress bar, color)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPointProgressUpdated, AOSControlPoint*, Point, EControlPointState, PreviousState, EControlPointState, CurrentState);
// Fires only when the capture state enum changes (e.g., NEUTRAL->CAPTURING) — use for logic/audio triggers
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnPointStateTransition, AOSControlPoint*, Point, EControlPointState, OldState, EControlPointState, NewState, int32, OwningTeam, int32, CapturingTeam);
// Fires when a team completes full capture (progress reaches MaxCap)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPointCaptured, AOSControlPoint*, Point, int32, CapturingTeamIndex, int32, PreviousOwnerTeamIndex);
// Fires when an owned point is fully neutralized (progress drains to zero, ownership cleared)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPointNeutralized, AOSControlPoint*, Point, int32, PreviousOwnerTeamIndex, int32, NeutralizingTeamIndex);
// Fires on all machines when this point awards a score tick — use for audio/VFX feedback
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnScoreTick, int32, TeamIndex, int32, NewScore);
// Fires on clients when the replicated player count changes (server-authoritative, replication-safe)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerCountChanged, AOSControlPoint*, Point, int32, NewCount);
// Fires on all machines when a player's pawn overlaps the capture volume
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerEnteredPoint, AOSControlPoint*, Point, AOSPlayerState*, Player);
// Fires on all machines when a player's pawn exits the capture volume
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerExitedPoint, AOSControlPoint*, Point, AOSPlayerState*, Player);



/**
 * Domination capture point actor.
 *
 * Manages capture progress, team ownership, scoring, and visual state for a single
 * objective in Domination mode. Server-authoritative: capture logic, timers, and
 * ownership transitions run only on the server. Clients receive replicated state
 * and drive UI/audio via delegates.
 *
 * Lifecycle: placed in level -> BeginPlay auto-registers with AOSGameMode_Domination
 * -> GM calls SetMatchConfig + Register -> point is active. Overlap events route
 * through Server RPCs to track which players are on the point.
 *
 * Capture flow: NEUTRAL -> CAPTURING -> CAPTURED. If the owning team's point is
 * attacked: CAPTURED -> NEUTRALIZING -> NEUTRAL -> CAPTURING (new team) -> CAPTURED.
 * Multiple teams present = CONTESTED (progress frozen).
 */
UCLASS()
class ONSIGHT_API AOSControlPoint : public AOSActor
{
	GENERATED_BODY()

public:
	AOSControlPoint();
	
	/** True if at least one collider volume is assigned. Invalid points cannot capture. */
	UFUNCTION(BlueprintPure, Category="ControlPoint")
	bool            IsPointValid() const;

	/** The player who "owns" current capture progress (persists after they leave). */
	UFUNCTION(BlueprintPure, Category="ControlPoint")
	AOSPlayerState* GetOccupant() const { return CurrentOccupant; }

	/** The player currently driving capture (first valid player on the uncontested point). */
	UFUNCTION(BlueprintPure, Category="ControlPoint")
	AOSPlayerState* GetLeader() const { return CurrentLeader; }



	/** Returns the team index of the first local player controller. Server returns INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category="ControlPoint")
	int32 GetLocalClientTeamIndex() const;

	/** Returns Friendly/Enemy from the local client's perspective based on point ownership/capture state.
	 *  On the server (no local player), defaults to Friendly. Use for client-side audio/UI only. */
	UFUNCTION(BlueprintPure, Category="ControlPoint")
	EAudioTeamRelationship GetLocalClientTeamRelationship() const;

	/** Returns Friendly/Enemy for a specific team index. Use when you already know the viewer's team. */
	UFUNCTION(BlueprintPure, Category="ControlPoint")
	EAudioTeamRelationship GetTeamRelationshipForTeam(int32 ViewerTeamIndex) const;
	/** Strip non-alpha characters and uppercase. Used for display labels ("A", "B", "C"). */
	static FString  NormalizeLabelString( const FText& Text );
	static bool            IsLabelValid( const FString& Label );
	/** Convert 0-based index to alpha label: 0->"A", 1->"B", 25->"Z", 26->"AA". */
	static FString         IndexToAlphaLabel( int32 Index );

	/** Unique identifier assigned by the GameMode during registration. Replicated to clients. */
	UPROPERTY(ReplicatedUsing=OnRep_ControlPointState, EditAnywhere, BlueprintReadOnly, Category="ControlPoint")
	FName PointName = NAME_None;

	UFUNCTION(BlueprintPure, Category="ControlPoint")
	int32 GetCaptureProgress() const;
	UFUNCTION(BlueprintPure, Category="ControlPoint")
	int32 GetMaxCap() const;

	/** True when a team fully owns this point. Authoritative — set by capture events, not recomputed. */
	UFUNCTION(BlueprintPure, Category="ControlPoint")
	bool IsCaptured() const { return bIsCaptured; }

	/** True when the specified team fully owns this point. */
	UFUNCTION(BlueprintPure, Category="ControlPoint")
	bool IsCapturedByTeam(int32 TeamIndex) const { return bIsCaptured && OwningTeamIndex == TeamIndex; }

	/** Re-evaluate who is on the point, resolve leader/occupant, and start/stop capture timer. Server only. */
	void UpdateCaptureProgress();
	
	UFUNCTION(BlueprintPure)
	FText GetPointLabelText() const { return PointLabel; }
	UFUNCTION(BlueprintPure)
	FText GetLabeledPointNameText() const;
	void SetPointLabel(const FText& InLabel);
	
protected:
	UPROPERTY(EditInstanceOnly, ReplicatedUsing=OnRep_ControlPointState)
	FText PointLabel;
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	/** Shape components that define the capture volume. Populated from "ColliderHolder" children at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ControlPoint")
	TArray<UShapeComponent*> Colliders;

	/** Players currently inside the capture volume. Weak refs — stale entries pruned in UpdateCaptureProgress. */
	UPROPERTY()
	TSet<TWeakObjectPtr<AOSPlayerState>> CurrentPlayers;

	/** Base capture progress per tick (before multi-player scaling). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ControlPoint|Capture", meta=(ClampMin="1"))
	int32 CaptureRate = 5;
	/** Seconds between capture ticks. Also used for decay when point is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ControlPoint|Capture", meta=(ClampMin="0.1"))
	float CaptureInterval = .5;
	/** Seconds between score ticks while fully captured. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ControlPoint|Scoring", meta=(ClampMin="0.1"))
	float ScoreInterval = 1;
	

public:
	/** Called by the GameMode to activate this point: assigns name, enables colliders, broadcasts initial state. */
	void Register(FName name);
	/** Request deregistration through the GameMode. Safe to call multiple times. */
	void RequestUnregister();

	UFUNCTION(BlueprintPure, Category="ControlPoint")
	FString GetPointName() const;

	/** Internal cleanup called by GameMode during unregistration. Stops timers, deactivates colliders. */
	void Unregister_Internal();
	
	/** Server RPC: add player to capture tracking. Called from overlap on owning client. */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_PlayerEntered(AOSPlayerController* player);
	/** Server RPC: remove player from capture tracking. Called from overlap-end on owning client. */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_PlayerExited(AOSPlayerController* player);
	
	UFUNCTION(BlueprintPure, Category="ControlPoint")
	bool IsRegistered() const { return bRegistered; }
	
	UFUNCTION(BlueprintCallable)
	EControlPointState GetControlPointState() const;
	
	void InitializePoint();
	
	/** Fires every capture tick and on replication. High-frequency — use for progress bars and color updates. */
	UPROPERTY(BlueprintAssignable, Category="ControlPoint|Events")
	FOnPointProgressUpdated OnPointProgressUpdated;

	/** Fires only when the capture state enum actually changes (e.g., NEUTRAL→CAPTURING). Use for logic/audio. */
	UPROPERTY(BlueprintAssignable, Category="ControlPoint|Events")
	FOnPointStateTransition OnPointStateTransition;

	/** Fires when a team completes capture. Includes the capturing team index. */
	UPROPERTY(BlueprintAssignable, Category="ControlPoint|Events")
	FOnPointCaptured OnPointCaptured;

	/** Fires when an owned point is fully neutralized. Includes the team that lost it. */
	UPROPERTY(BlueprintAssignable, Category="ControlPoint|Events")
	FOnPointNeutralized OnPointNeutralized;

	/** Fires on clients when the server-authoritative player count replicates. Use for sound start/stop. */
	UPROPERTY(BlueprintAssignable, Category="ControlPoint|Events")
	FOnPlayerCountChanged OnPlayerCountChanged;

	UPROPERTY(BlueprintAssignable, Category="ControlPoint|Events")
	FOnPlayerEnteredPoint OnPlayerEnteredPoint;

	UPROPERTY(BlueprintAssignable, Category="ControlPoint|Events")
	FOnPlayerExitedPoint OnPlayerExitedPoint;

	/** Fires on all machines each score tick. Bind for +1 popup / tick sound. */
	UPROPERTY(BlueprintAssignable, Category="ControlPoint|Events")
	FOnScoreTick OnScoreTick;

	/** Multicast RPC: broadcasts score tick to all clients for audio/VFX feedback. */
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_ScoreTick(int32 TeamIndex, int32 NewScore);

	UFUNCTION(BlueprintPure, Category="ControlPoint")
	int32 GetOwningTeamIndex() const { return OwningTeamIndex; }

	UFUNCTION(BlueprintPure, Category="ControlPoint")
	int32 GetCapturingTeamIndex() const { return CapturingTeamIndex; }

	UFUNCTION(BlueprintPure, Category="ControlPoint")
	bool IsContested() const { return bContested; }

	UFUNCTION(BlueprintPure, Category="ControlPoint")
	float GetCapturePercent() const { return MaxCap > 0 ? (float)CaptureProgress / (float)MaxCap : 0.f; }

	UFUNCTION(BlueprintPure, Category="ControlPoint")
	TArray<AOSPlayerState*> GetPlayersOnPoint() const;

	/** Total players on this point (all teams). Replicated via PlayerNumber. */
	UFUNCTION(BlueprintPure, Category="ControlPoint")
	int32 GetTotalPlayerCount() const { return PlayerNumber; }

	/** Count of players from a specific team currently on this point. Server-authoritative. */
	UFUNCTION(BlueprintPure, Category="ControlPoint")
	int32 GetTeamPlayerCount(int32 TeamIndex) const;

	/** Configure capture/scoring tuning from the GameMode. Called once at registration before the point goes active. */
	void SetMatchConfig(int32 InCaptureRate, int32 InDecayRate, int32 InMaxCap,
		float InCaptureInterval, float InScoreInterval,
		float InTeamPresenceCaptureScaling = 0.6f, float InMaxTeamPresenceMultiplier = 2.0f);

	/** Called by GameMode to adjust score tick rate based on team ownership count. Restarts timer if active. */
	void UpdateScoreInterval(float NewInterval);

	/** Force-capture this point for the given team. Sets all state, fires events, starts scoring. Server only. */
	UFUNCTION(BlueprintCallable, Category="ControlPoint")
	void ForceCapture(int32 TeamIndex);

	/** Reset this point to neutral. Clears all state, stops timers, fires events. Server only. */
	UFUNCTION(BlueprintCallable, Category="ControlPoint")
	void ResetToNeutral();

	/** Primitive: maps absolute state + owner-relative relationship to display color.
	 *  Neutralizing inverts: Friendly relationship (I own it, being drained) → enemy-action color. */
	UFUNCTION(BlueprintPure, Category="ControlPoint|Visual")
	FLinearColor GetColorForStateAndRelationship(EControlPointState State, EAudioTeamRelationship Relationship) const;

	/** Display color from a specific viewer's team perspective. */
	UFUNCTION(BlueprintPure, Category="ControlPoint|Visual")
	FLinearColor GetDisplayColorForTeam(int32 ViewerTeamIndex) const;

	/** Convenience: display color using the local client's team. */
	UFUNCTION(BlueprintPure, Category="ControlPoint|Visual")
	FLinearColor GetDisplayColor() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	
	/** Capture progress required for full capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ControlPoint|Capture", meta=(ClampMin="1"))
	int32 MaxCap = 100;

	// --- Replicated capture state (all use OnRep_ControlPointState for batched client refresh) ---

	/** Player currently driving capture. TObjectPtr for replication — TWeakObjectPtr cannot replicate. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ControlPointState)
	TObjectPtr<AOSPlayerState> CurrentLeader;
	/** Player who "owns" the capture progress. Persists after they leave the volume. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ControlPointState)
	TObjectPtr<AOSPlayerState> CurrentOccupant;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ControlPointState)
	int32 CaptureProgress = 0;
	/** Number of players inside the capture volume. Used for UI display. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ControlPointState)
	int32 PlayerNumber = 0;
	/** True when multiple teams are present — progress is frozen. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ControlPointState)
	bool bContested = false;

	/** Team that fully owns this point (INDEX_NONE = neutral). Set only in OnCaptured/OnNeutralized. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ControlPointState)
	int32 OwningTeamIndex = INDEX_NONE;

	/** Team currently accumulating capture progress (INDEX_NONE when contested or empty). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ControlPointState)
	int32 CapturingTeamIndex = INDEX_NONE;

	/** Tracks which team's partial progress is on a neutral point. Enables drain-before-capture. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ControlPointState)
	int32 LastCapturingTeamIndex = INDEX_NONE;

	/** Authoritative capture flag. Set true in OnCaptured/ForceCapture, false in OnNeutralized/ResetToNeutral. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ControlPointState)
	bool bIsCaptured = false;

	/** Progress per tick when the point is empty and decaying toward stable state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ControlPoint", meta=(ClampMin="1"))
	int32 DecayRate = 5;

	/** Logarithmic scaling for teammates on point: rate = base * min(cap, 1 + ln(N) * scaling). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ControlPoint|Capture", meta=(ClampMin="0.0"))
	float TeamPresenceCaptureScaling = 0.6f;

	/** Maximum capture speed multiplier regardless of player count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ControlPoint|Capture", meta=(ClampMin="1.0"))
	float MaxTeamPresenceMultiplier = 2.0f;
	
	// --- Visual state (client-side color feedback on the point mesh) ---

	/** Mesh whose material color reflects the current capture state. Found by "VisualMesh" tag or root. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ControlPointMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="ControlPoint|Visual")
	FName ColorParamName = "Color";

	UPROPERTY(EditDefaultsOnly, Category="ControlPoint|Visual")
	FLinearColor ColorNeutral = FLinearColor(0.5f, 0.5f, 0.5f);

	UPROPERTY(EditDefaultsOnly, Category="ControlPoint|Visual")
	FLinearColor ColorFriendlyCapturing = FLinearColor(0.2f, 0.6f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category="ControlPoint|Visual")
	FLinearColor ColorFriendlyCaptured = FLinearColor(0.0f, 0.4f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category="ControlPoint|Visual")
	FLinearColor ColorEnemyCapturing = FLinearColor(1.0f, 0.4f, 0.2f);

	UPROPERTY(EditDefaultsOnly, Category="ControlPoint|Visual")
	FLinearColor ColorEnemyCaptured = FLinearColor(0.8f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, Category="ControlPoint|Visual")
	FLinearColor ColorContested = FLinearColor(1.0f, 0.8f, 0.0f);
	

	/** Server-side BP hook: fires each capture timer tick. Use for server-only BP logic. */
	UFUNCTION(BlueprintImplementableEvent)
	void OnCaptureTickUpdate();
private:
	/** Client-side replication callback — detects state transitions, broadcasts delegates, updates visuals. */
	UFUNCTION()
	void OnRep_ControlPointState();

	UPROPERTY()
	bool bRegistered = false;
	UPROPERTY()
	bool bUnregisterInProgress = false;

	/** Tracks last broadcast state to detect transitions and avoid duplicate delegate fires. */
	EControlPointState LastBroadcastState = Uninitialized;

	/** Local mirror of OwningTeamIndex from before the last OnRep — used to provide PreviousOwner in client broadcasts. */
	int32 LastKnownOwningTeamIndex = INDEX_NONE;

	/** Tracks replicated player count to detect changes in OnRep. */
	int32 LastReplicatedPlayerNumber = 0;

	// Overlap routing: broadcasts enter/exit on all machines, then calls Server RPC for capture tracking
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	/** Timer callback: advances capture progress, handles decay, triggers OnCaptured/OnNeutralized. */
	void CaptureTick();

	FTimerHandle CaptureTimer;  // Drives CaptureTick at CaptureInterval
	FTimerHandle ScoreTimer;    // Drives ScoreTick at ScoreInterval while captured

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	/** Populates Colliders array from "ColliderHolder" children. Server only, called at BeginPlay. */
	UFUNCTION(Server, Reliable)
	void Server_UpdateColliders();

	void ScoreTick();           // Awards points to the owning team via GameMode
	void OnCaptured();          // Transition to CAPTURED state, start score timer
	void OnNeutralized();       // Transition to NEUTRAL state, stop score timer
	AOSPlayerState* ResolveLeader();   // Pick the player driving capture (first valid on uncontested point)
	void ResolveOccupant();            // Assign occupant — the player who "owns" the progress
	float ResolveCapRate() const;      // Compute effective capture rate with multi-player scaling

	void LogState(const FString& event) const;

	/** Broadcast OnPointProgressUpdated + refresh visuals. Called on state change and replication. */
	void NotifyPointStateChanged();
	/** True if the point is empty and capture progress is not at its stable resting value. */
	bool ShouldDecay() const;
	/** Update mesh material color to reflect current team-relative capture state. */
	void ApplyPointVisuals();
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
