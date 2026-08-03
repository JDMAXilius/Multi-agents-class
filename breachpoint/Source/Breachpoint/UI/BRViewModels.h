#pragma once

#include "AttributeSet.h"
#include "MVVMViewModelBase.h"
#include "Types/MVVMEventField.h"
#include "UI/BRUITypes.h"

#include "BRViewModels.generated.h"

class AGameStateBase;
class UAbilitySystemComponent;
class UWorld;
struct FOnAttributeChangeData;

DECLARE_MULTICAST_DELEGATE_OneParam(FBROnHitMarker, EBRHitMarkerKind);
DECLARE_MULTICAST_DELEGATE(FBROnKillfeedChanged);

/**
 * The four attributes the combat ViewModel watches. Lives HERE, not in BRUITypes.h, on purpose
 * (HUD-CPP-AUDIT): this file's two classes are its only consumers, and this header is included
 * by zero other headers — so AttributeSet.h stops riding into every UI translation unit.
 */
USTRUCT(BlueprintType)
struct FBRCombatAttributeBindings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FGameplayAttribute Health;

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FGameplayAttribute MaxHealth;

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FGameplayAttribute Shields;

	UPROPERTY(BlueprintReadWrite, Category = "Breachpoint|UI")
	FGameplayAttribute MaxShields;

	bool HasAny() const
	{
		return Health.IsValid() || MaxHealth.IsValid() || Shields.IsValid() || MaxShields.IsValid();
	}
};

UCLASS(BlueprintType, DisplayName = "BR Combat Viewmodel")
class BREACHPOINT_API UBRVM_Combat : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:

	void BindToAbilitySystem(UAbilitySystemComponent* InASC, const FBRCombatAttributeBindings& InBindings);

	void UnbindFromAbilitySystem();

	void SetAmmo(int32 InMagazine, int32 InReserve);
	void SetWeaponNames(const FText& InActiveWeapon, const FText& InStowedWeapon);
	void SetGrenadeCount(int32 InCount);

	void SetGrappleCooldownStarted(float InDurationSeconds);
	void SetGrappleReady();

	void ReportHitMarker(EBRHitMarkerKind InKind);

	FBROnHitMarker& OnHitMarker() { return OnHitMarkerEvent; }

	void ClearToUnknown();

	virtual void BeginDestroy() override;

	EBRUIDataState GetVitalsState() const { return VitalsState; }
	EBRUIDataState GetEquipmentState() const { return EquipmentState; }

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	float GetHealthPercent() const { return HealthPercent; }
	float GetShields() const { return Shields; }
	float GetMaxShields() const { return MaxShields; }
	float GetShieldPercent() const { return ShieldPercent; }
	bool AreShieldsBroken() const { return bShieldsBroken; }

	int32 GetMagazineAmmo() const { return MagazineAmmo; }
	int32 GetReserveAmmo() const { return ReserveAmmo; }
	FText GetActiveWeaponName() const { return ActiveWeaponName; }
	FText GetStowedWeaponName() const { return StowedWeaponName; }
	int32 GetGrenadeCount() const { return GrenadeCount; }

	float GetGrappleCooldownDuration() const { return GrappleCooldownDuration; }
	bool IsGrappleReady() const { return bGrappleReady; }

public:

	/**
	 * The two grapple event fields survive because BREquipmentTray consumes both. The seven
	 * sibling event fields (four hit-marker kinds, shields broken/restored, killfeed-changed)
	 * were cut (HUD-CPP-AUDIT): every widget consumes the native delegates / FieldNotify
	 * properties for those signals, and a second channel nobody reads is where the next
	 * consumer subscribes and hears nothing.
	 */
	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Category = "Breachpoint|Combat")
	FMVVMEventField GrappleCooldownStarted;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Category = "Breachpoint|Combat", meta = (ScriptName = "GrappleReadyEvent"))
	FMVVMEventField GrappleReady;

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void HandleShieldsBrokenTagChanged(const FGameplayTag Tag, int32 NewCount);

	void SetVitalsState(EBRUIDataState InState);
	void SetEquipmentState(EBRUIDataState InState);
	void RecomputeVitalRatios();
	void PublishCurrentAttributeValues();

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetVitalsState", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	EBRUIDataState VitalsState = EBRUIDataState::Unknown;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetHealth", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	float Health = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMaxHealth", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	float MaxHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetHealthPercent", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	float HealthPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetShields", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	float Shields = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMaxShields", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	float MaxShields = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetShieldPercent", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	float ShieldPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "AreShieldsBroken", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	bool bShieldsBroken = false;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetEquipmentState", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	EBRUIDataState EquipmentState = EBRUIDataState::Unknown;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMagazineAmmo", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	int32 MagazineAmmo = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetReserveAmmo", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	int32 ReserveAmmo = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetActiveWeaponName", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	FText ActiveWeaponName;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetStowedWeaponName", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	FText StowedWeaponName;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetGrenadeCount", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	int32 GrenadeCount = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetGrappleCooldownDuration", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	float GrappleCooldownDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "IsGrappleReady", Category = "Breachpoint|Combat", meta = (AllowPrivateAccess))
	bool bGrappleReady = true;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	UPROPERTY(Transient)
	FBRCombatAttributeBindings Bindings;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle ShieldsChangedHandle;
	FDelegateHandle MaxShieldsChangedHandle;
	FDelegateHandle ShieldsBrokenTagHandle;

	FBROnHitMarker OnHitMarkerEvent;
};

UCLASS(BlueprintType, DisplayName = "BR Match Viewmodel")
class BREACHPOINT_API UBRVM_Match : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:

	void SetTimeSource(AGameStateBase* InGameState);

	void SetMatchEndServerTime(float InServerTimeSeconds);

	void SetRocketSpawnServerTime(float InServerTimeSeconds);

	void SetRocketAvailable(bool bInAvailable);

	void SetTeamScores(int32 InTeam0Score, int32 InTeam1Score);

	void SetLocalTeamId(uint8 InTeamId);

	void SetMatchPhaseTag(FGameplayTag InPhaseTag);

	void PushKillfeedEntry(const FBRKillfeedViewEntry& InEntry);

	void AppendSpotterLine(int32 InSequenceId, const FText& InLine);

	void ClearKillfeed();

	const TArray<FBRKillfeedViewEntry>& GetKillfeedEntries() const { return KillfeedEntries; }

	/**
	 * The killfeed's ONE notification channel. Its per-entry sibling (`OnKillfeedEntryAdded`)
	 * was cut with zero subscribers and zero prospect of one — the pooled feed deliberately
	 * refreshes the whole projection instead of appending (HUD-CPP-AUDIT).
	 */
	FBROnKillfeedChanged& OnKillfeedChanged() { return OnKillfeedChangedEvent; }

	void ClearToUnknown();

	virtual void BeginDestroy() override;

	EBRUIDataState GetMatchState() const { return MatchState; }
	int32 GetSecondsRemaining() const { return SecondsRemaining; }
	FText GetMatchClockText() const { return MatchClockText; }
	bool IsClockRunning() const { return bClockRunning; }
	int32 GetTeam0Score() const { return Team0Score; }
	int32 GetTeam1Score() const { return Team1Score; }
	uint8 GetLocalTeamId() const { return LocalTeamId; }
	FGameplayTag GetMatchPhaseTag() const { return MatchPhaseTag; }
	int32 GetRocketSecondsRemaining() const { return RocketSecondsRemaining; }
	FText GetRocketCountdownText() const { return RocketCountdownText; }
	bool IsRocketAvailable() const { return bRocketAvailable; }

private:
	void UpdateClocks();

	void ScheduleNextClockUpdate();
	void StopClockUpdates();

	void ExpireKillfeedEntries();
	void ScheduleKillfeedExpiry();
	void PublishKillfeedChanged();

	UWorld* GetTimerWorld() const;
	static FText FormatClock(int32 InSeconds);

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMatchState", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	EBRUIDataState MatchState = EBRUIDataState::Unknown;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetSecondsRemaining", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	int32 SecondsRemaining = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMatchClockText", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	FText MatchClockText;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "IsClockRunning", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	bool bClockRunning = false;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetTeam0Score", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	int32 Team0Score = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetTeam1Score", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	int32 Team1Score = 0;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetLocalTeamId", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	uint8 LocalTeamId = 255;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetMatchPhaseTag", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	FGameplayTag MatchPhaseTag;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetRocketSecondsRemaining", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	int32 RocketSecondsRemaining = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetRocketCountdownText", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	FText RocketCountdownText;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "IsRocketAvailable", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	bool bRocketAvailable = false;

	UPROPERTY(BlueprintReadOnly, Transient, FieldNotify, Getter = "GetKillfeedEntries", Category = "Breachpoint|Match", meta = (AllowPrivateAccess))
	TArray<FBRKillfeedViewEntry> KillfeedEntries;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGameStateBase> TimeSource;

	float MatchEndServerTime = 0.0f;

	float RocketSpawnServerTime = 0.0f;

	FTimerHandle ClockTimerHandle;
	FTimerHandle KillfeedExpiryTimerHandle;

	FBROnKillfeedChanged OnKillfeedChangedEvent;
};
