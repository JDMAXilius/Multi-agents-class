#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "MVVMViewModelBase.h"
#include "UI/BNUITypes.h"
#include "BNViewModels.generated.h"

class AGameStateBase;
class UAbilitySystemComponent;
class UTexture2D;
struct FOnAttributeChangeData;

/** The killfeed VIEW ring changed (push or expiry). A native multicast, NOT a FieldNotify on
 *  the array — the compiled reference notifies its killfeed exactly this way, and §API's rule
 *  is transcription over invention. One broadcast per change; readers walk GetKillfeedEntries. */
DECLARE_MULTICAST_DELEGATE(FBNKillfeedViewChangedSignature);

/** The roster view changed — same shape, same reasoning, for the scoreboard's rows. */
DECLARE_MULTICAST_DELEGATE(FBNRosterViewChangedSignature);

/** Which attributes feed the combat ViewModel — INJECTED by the director, never named here.
 *  This struct is the decoupling that let the old module's combat VM port with zero gameplay
 *  includes: the VM knows "a health-like number and its max", not UBNAttributeSet. */
USTRUCT()
struct FBNCombatAttributeBindings
{
	GENERATED_BODY()

	FGameplayAttribute Health;
	FGameplayAttribute MaxHealth;
	FGameplayAttribute Shield;
	FGameplayAttribute MaxShield;
};

/**
 * The owning player's fight, as presentation state: vitals, the hand, death. Fed exclusively by
 * UBNHUDDirector; read exclusively through FieldNotify. Never touches a gameplay header.
 *
 * HONEST-UNKNOWN: VitalsState stays Unknown until BOTH a value arrived AND the denominators are
 * known — the old module shipped an empty health bar on a living player because Health landed
 * one bunch before MaxHealth, and this gate is that lesson (`bAnyFound && bDenominatorsKnown`).
 */
UCLASS(BlueprintType, DisplayName = "BN Combat Viewmodel")
class BREACHPOINTNEXT_API UBNVM_Combat : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	/** Director only. Binds attribute-change delegates and reads every value ONCE — the
	 *  subscribe-then-read rule; changes before this call are not missed, they are the read. */
	void BindToAbilitySystem(UAbilitySystemComponent* InASC, const FBNCombatAttributeBindings& InBindings);
	void UnbindFromAbilitySystem();

	/** Director only: the hand changed. Empty name = unarmed; ammo INDEX_NONE = no magazine
	 *  (the knife, the empty hand) — the widget renders a dash, never a confident zero. The icon
	 *  is the design's silhouette slot and may be null; it notifies like every other field. */
	void SetEquippedWeapon(const FText& InName, int32 InMagAmmo, int32 InReserveAmmo, bool bKnown,
		const TSoftObjectPtr<UTexture2D>& InIcon = TSoftObjectPtr<UTexture2D>());
	void SetAmmo(int32 InMagAmmo, int32 InReserveAmmo);

	/** Director only: the dead state and its clock. RespawnAt <= 0 clears the countdown. The VM
	 *  owns the per-second update — one timer, aligned to the stamp, never a tick. */
	void SetDead(bool bInDead);
	void SetKilledByLine(const FText& InLine);
	void SetRespawnStamp(double InRespawnAtServerTime, AGameStateBase* InTimeSource);

	/** Travel and rebind reset: everything back to Unknown, timers down. */
	void ClearToUnknown();

	float GetHealthPercent() const { return HealthPercent; }
	float GetShieldPercent() const { return ShieldPercent; }
	int32 GetHealthValue() const { return HealthValue; }
	int32 GetShieldValue() const { return ShieldValue; }
	EBNUIDataState GetVitalsState() const { return VitalsState; }
	FText GetWeaponName() const { return WeaponName; }
	int32 GetMagAmmo() const { return MagAmmo; }
	int32 GetReserveAmmo() const { return ReserveAmmo; }
	EBNUIDataState GetEquipmentState() const { return EquipmentState; }
	/** By value, matching the compiled reference's soft-pointer getter shape. */
	TSoftObjectPtr<UTexture2D> GetWeaponIcon() const { return WeaponIcon; }
	bool IsDead() const { return bIsDead; }
	FText GetKilledByLine() const { return KilledByLine; }
	int32 GetRespawnSecondsRemaining() const { return RespawnSecondsRemaining; }

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void RefreshVitals();
	void UpdateRespawnClock();
	void ScheduleNextRespawnUpdate();
	void StopRespawnClock();
	UWorld* GetTimerWorld() const;

	// ---- vitals ----
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetHealthPercent", Category = "BN|Combat", meta = (AllowPrivateAccess))
	float HealthPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetShieldPercent", Category = "BN|Combat", meta = (AllowPrivateAccess))
	float ShieldPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetHealthValue", Category = "BN|Combat", meta = (AllowPrivateAccess))
	int32 HealthValue = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetShieldValue", Category = "BN|Combat", meta = (AllowPrivateAccess))
	int32 ShieldValue = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetVitalsState", Category = "BN|Combat", meta = (AllowPrivateAccess))
	EBNUIDataState VitalsState = EBNUIDataState::Unknown;

	// ---- the hand ----
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetWeaponName", Category = "BN|Combat", meta = (AllowPrivateAccess))
	FText WeaponName;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMagAmmo", Category = "BN|Combat", meta = (AllowPrivateAccess))
	int32 MagAmmo = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetReserveAmmo", Category = "BN|Combat", meta = (AllowPrivateAccess))
	int32 ReserveAmmo = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetEquipmentState", Category = "BN|Combat", meta = (AllowPrivateAccess))
	EBNUIDataState EquipmentState = EBNUIDataState::Unknown;

	/** A FieldNotify soft pointer — the compiled reference's own shape (its roster emblem is
	 *  declared and set exactly this way), so no counter is needed to force a notify. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetWeaponIcon", Category = "BN|Combat", meta = (AllowPrivateAccess))
	TSoftObjectPtr<UTexture2D> WeaponIcon;

	// ---- death ----
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "IsDead", Category = "BN|Combat", meta = (AllowPrivateAccess))
	bool bIsDead = false;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetKilledByLine", Category = "BN|Combat", meta = (AllowPrivateAccess))
	FText KilledByLine;

	/** INDEX_NONE = no respawn pending; 0 = imminent. */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetRespawnSecondsRemaining", Category = "BN|Combat", meta = (AllowPrivateAccess))
	int32 RespawnSecondsRemaining = INDEX_NONE;

	// ---- plumbing (no UPROPERTY: handles and weak refs, cleared on unbind) ----
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FBNCombatAttributeBindings Bindings;
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeHandles;

	/** Raw last-seen attribute values; the gate needs to distinguish "0.0" from "never arrived". */
	float RawHealth = 0.f;
	float RawMaxHealth = 0.f;
	float RawShield = 0.f;
	float RawMaxShield = 0.f;
	bool bAnyVitalsSeen = false;

	TWeakObjectPtr<AGameStateBase> RespawnTimeSource;
	double RespawnAtServerTime = 0.0;
	FTimerHandle RespawnTimerHandle;
};

/**
 * The match, as presentation state: phase, clock, scores, winner, and the killfeed view ring.
 * Fed exclusively by UBNHUDDirector. The clock is COMPUTED, phase-locked to the whole server
 * second so every readout shows the same digit — one replicated stamp, zero ticking properties.
 */
UCLASS(BlueprintType, DisplayName = "BN Match Viewmodel")
class BREACHPOINTNEXT_API UBNVM_Match : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	/** Director only: the phase FName (the engine's own MatchState names) and, when ended, the
	 *  winner line. Composes the banner here so the widget renders text and decides nothing. */
	void SetMatchPhase(FName InMatchState, const FText& InWinnerLine);

	/** Director only: my kills, the current leader's, and the limit — read back by widgets. */
	void SetScores(int32 InMyKills, int32 InTopKills, int32 InScoreLimit);

	/** Director only: the end stamp and the time authority. Restarts the clock updates. */
	void SetMatchClock(double InMatchEndServerTime, AGameStateBase* InTimeSource);

	/** Director only, once per NEW ring entry (dedupe by Sequence is the CALLER's job via
	 *  GetLastKillfeedSequence). Stamps the LOCAL expiry on the entry and trims the view pool. */
	void PushKillfeedEntry(const FText& InLine, int32 InSequence, bool bInvolvesSelf);
	int32 GetLastKillfeedSequence() const { return LastKillfeedSequence; }

	FBNKillfeedViewChangedSignature OnKillfeedViewChanged;

	/** Director only: the whole sorted roster, replaced. Broadcasts only when something the
	 *  scoreboard renders actually changed — a rebuild that produces the same rows is silent. */
	void SetRoster(TArray<FBNScoreRowView>&& InRoster);
	const TArray<FBNScoreRowView>& GetRoster() const { return Roster; }

	FBNRosterViewChangedSignature OnRosterViewChanged;

	void ClearToUnknown();

	FName GetMatchStateName() const { return MatchStateName; }
	FText GetPhaseBannerText() const { return PhaseBannerText; }
	FText GetMatchClockText() const { return MatchClockText; }
	int32 GetMyKills() const { return MyKills; }
	int32 GetTopKills() const { return TopKills; }
	int32 GetScoreLimit() const { return ScoreLimit; }
	EBNUIDataState GetMatchDataState() const { return MatchDataState; }
	const TArray<FBNKillfeedViewEntry>& GetKillfeedEntries() const { return KillfeedEntries; }

private:
	void UpdateClocks();
	void ScheduleNextClockUpdate();
	void StopClockUpdates();
	void ExpireKillfeedEntries();
	void ScheduleKillfeedExpiry();
	UWorld* GetTimerWorld() const;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMatchStateName", Category = "BN|Match", meta = (AllowPrivateAccess))
	FName MatchStateName;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetPhaseBannerText", Category = "BN|Match", meta = (AllowPrivateAccess))
	FText PhaseBannerText;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMatchClockText", Category = "BN|Match", meta = (AllowPrivateAccess))
	FText MatchClockText;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMyKills", Category = "BN|Match", meta = (AllowPrivateAccess))
	int32 MyKills = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetTopKills", Category = "BN|Match", meta = (AllowPrivateAccess))
	int32 TopKills = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetScoreLimit", Category = "BN|Match", meta = (AllowPrivateAccess))
	int32 ScoreLimit = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMatchDataState", Category = "BN|Match", meta = (AllowPrivateAccess))
	EBNUIDataState MatchDataState = EBNUIDataState::Unknown;

	/** Plain member, notified by OnKillfeedViewChanged above — see the delegate's comment. */
	UPROPERTY(Transient)
	TArray<FBNKillfeedViewEntry> KillfeedEntries;

	UPROPERTY(Transient)
	TArray<FBNScoreRowView> Roster;

	/** How many lines the view keeps. Presentation, not replication (the GameState ring is the
	 *  record; this is what's on screen). Linger time is BNUITiming's — shared with the
	 *  director's join-age filter so "too old to show" means one thing. */
	static constexpr int32 KillfeedMaxVisibleEntries = 5;

	int32 LastKillfeedSequence = INDEX_NONE;
	double MatchEndServerTime = 0.0;
	TWeakObjectPtr<AGameStateBase> TimeSource;
	FTimerHandle ClockTimerHandle;
	FTimerHandle KillfeedExpiryTimerHandle;
};
