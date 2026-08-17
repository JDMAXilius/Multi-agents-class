#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UI/Settings/BRSettingsValueObjects.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include "BRSettingsKeyRemap.generated.h"

class UEnhancedInputUserSettings;

/**
 * What happened when the player tried to bind a key. Returned rather than logged, because every
 * one of these is a sentence the rebind screen has to show -- a rebind that silently fails is the
 * single most common defect in this feature.
 */
UENUM(BlueprintType)
enum class EBRKeyRebindResult : uint8
{
	/** Bound. */
	Applied,

	/** The key is already on another action and the caller did not ask to steal it. */
	Conflict,

	/** Not a key we accept for a binding at all (see `IsBindableKey`). */
	Rejected,

	/** No Enhanced Input user settings -- headless, or the plugin's user settings are disabled. */
	Unavailable
};

/**
 * `UBRSettingsValue_KeyRemap` -- one rebindable action, in one slot.
 *
 * ONE OBJECT PER SLOT, NOT PER ACTION. Enhanced Input models "Fire, primary" and "Fire,
 * secondary" as two mappings in one row, and the screen shows them as two columns on one line.
 * Modelling this as one object per ACTION would mean every method taking a slot argument and the
 * list entry owning the decision of which one it is editing. One object per slot keeps the leaf
 * exactly as dumb as every other leaf in the tree.
 *
 * IT OWNS NO KEY STATE. `GetCurrentKey` reads through to the Enhanced Input profile every time.
 * There is no cached `FKey` member to go stale when something else -- a profile switch, a reset,
 * another slot's rebind -- changes the mapping underneath. The cost is a map lookup on a screen
 * that redraws when a human presses a button.
 *
 * CONFLICTS ARE REPORTED, NEVER RESOLVED SILENTLY. `TryAssignKey` refuses a key that is already
 * bound elsewhere and hands back the conflicting action names; the screen asks the player and
 * calls again with `bStealFromConflicts`. Auto-stealing would be a keyboard layout quietly
 * rearranging itself, and auto-refusing with no explanation is worse.
 *
 * CLIENT-LOCAL, LIKE EVERYTHING IN THIS TREE. A rebind changes which key produces an input on
 * this machine. The server sees the same intent it always saw, so there is no authority question
 * here and no `_Validate` is owed (law 1).
 */
UCLASS(BlueprintType)
class BREACHPOINT_API UBRSettingsValue_KeyRemap : public UBRSettingsValue
{
	GENERATED_BODY()

public:
	/**
	 * Bind to one mapping/slot pair. `InSettings` may be null -- the object stays inert and every
	 * query returns an empty key, which is what lets the headless spec construct one.
	 */
	void Initialize(UEnhancedInputUserSettings* InSettings, FName InMappingName, EPlayerMappableKeySlot InSlot);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	FKey GetCurrentKey() const;

	/** The key the game shipped with, for this mapping and slot. Empty if unknown. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	FKey GetDefaultKey() const;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	FName GetMappingName() const { return MappingName; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	EPlayerMappableKeySlot GetSlot() const { return Slot; }

	/**
	 * Actions other than this one that already use `InKey`. Empty means the key is free.
	 * A key bound to THIS mapping in ANOTHER slot counts as a conflict: binding Fire to the same
	 * key twice is not something a player ever means to do.
	 */
	void FindConflicts(const FKey& InKey, TArray<FName>& OutConflictingMappings) const;

	/**
	 * Try to bind `InKey`. With `bStealFromConflicts` false this refuses on any conflict and
	 * fills `OutConflicts`; with it true the conflicting mappings are unbound first.
	 */
	EBRKeyRebindResult TryAssignKey(const FKey& InKey, bool bStealFromConflicts, TArray<FName>& OutConflicts);

	/** Put this one mapping back to its shipped key. */
	EBRKeyRebindResult ResetKeyToDefault();

	/**
	 * Keys we refuse to bind, and the whole policy is here so it is arguable in one place:
	 *   - invalid keys, and `AnyKey`, which is not a key;
	 *   - `Escape`, because it is how the capture screen is cancelled -- binding it makes the
	 *     rebind screen unexitable by the only means a player will try;
	 *   - gesture and non-button axes, which cannot express a discrete press.
	 * Mouse buttons and gamepad face buttons ARE bindable; a shooter that refuses them is broken.
	 */
	static bool IsBindableKey(const FKey& InKey);

	//~ Begin UBRSettingsDataObject interface
	virtual FText GetDisplayValue() const override;
	virtual bool IsDirty() const override;
	virtual void ResetToDefault() override;
	virtual void CaptureBaseline() override;
	virtual void RestoreBaseline() override;
	//~ End UBRSettingsDataObject interface

	//~ Begin UBRSettingsValue interface
	virtual bool IsBound() const override;
	//~ End UBRSettingsValue interface

private:
	/** Locate our mapping in the current profile. Null when absent -- never creates one. */
	const FPlayerKeyMapping* FindMapping() const;

	/** Shared shape for the args every Enhanced Input call here needs. */
	FMapPlayerKeyArgs MakeArgs(const FKey& InKey) const;

	/**
	 * Weak: the user settings object belongs to the local player, and a settings screen can
	 * outlive a seamless travel that rebuilds it. A raw pointer here would be a crash on the
	 * first rebind after a map change.
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputUserSettings> UserSettings;

	UPROPERTY(Transient)
	FName MappingName;

	UPROPERTY(Transient)
	EPlayerMappableKeySlot Slot = EPlayerMappableKeySlot::First;

	UPROPERTY(Transient)
	FKey BaselineKey;
};
