#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "BRSettingsDataObject.generated.h"

/**
 * THE SETTINGS DATA MODEL -- read this before adding a setting.
 *
 * A setting is a DATA OBJECT, not a widget. The object owns the value, its legal range, how it
 * reads back as text, whether it is currently editable and why not. The widget owns none of
 * that; it renders whatever the object says and calls back when the player moves something.
 *
 * WHY THAT SPLIT IS THE WHOLE DESIGN. Every settings screen that rots does so the same way:
 * the slider knows it is the sensitivity slider. Clamp logic ends up in a widget, the "apply"
 * path ends up in a button handler, and nothing can be tested without spinning a viewport. Here
 * the entire model is `UObject`s with no widget dependency at all, which is why
 * `Breachpoint.Sim.Settings` can drive the real objects headlessly -- see `BRSettingsSpec.cpp`.
 *
 * ACCESSORS ARE TYPED LAMBDAS, NOT PROPERTY PATHS. A value object does not know the name of the
 * property it edits and never reaches through reflection to find it. The registry binds a
 * getter/setter pair at build time (`BindAccessor`) and the object calls them. Three reasons,
 * in order of how much they matter:
 *   1. A typo in a reflected property path is a RUNTIME failure that silently no-ops a setting.
 *      A typo in a lambda does not compile.
 *   2. The spec can bind a value object to a plain test stub. There is no `UGameUserSettings`
 *      anywhere in the headless suite, and there does not need to be.
 *   3. The setter can be more than a field write -- clamping, unit conversion, or a call into a
 *      subsystem -- without the object growing a special case.
 *
 * NO TICK, NO POLLING. Values change when something calls a setter. `OnValueChanged` fires from
 * there and the list entry updates. Nothing samples anything per frame (law 4).
 *
 * WHAT LIVES WHERE:
 *   `UBRSettingsDataObject`  -- this file. Identity, description, edit condition, dirty state.
 *   `UBRSettingsCollection`  -- this file. A named group with children; the tabs and sections.
 *   `UBRSettingsValue_*`     -- `BRSettingsValueObjects.h`. The leaves that hold a real value.
 *   `UBRSettingsValue_KeyRemap` -- `BRSettingsKeyRemap.h`. Enhanced Input's rebind row.
 *   `UBRSettingsRegistry`    -- `BRSettingsRegistry.h`. Builds the tree and owns apply/save.
 */

class UBRSettingsDataObject;

/** One data object changed. The list entry rendering it re-reads and redraws; nobody else cares. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBRSettingsValueChanged, UBRSettingsDataObject* /* Changed */);

/**
 * Why a setting cannot be edited right now. Kept as an enum rather than a bare bool so the
 * details view can say something true instead of greying a row with no explanation -- "this is
 * fullscreen-only" is a different sentence from "this needs a restart".
 */
UENUM(BlueprintType)
enum class EBRSettingsEditState : uint8
{
	/** Normal. The row is live. */
	Editable,

	/** Another setting's value rules this one out (windowed mode hides the monitor picker). */
	DependencyUnmet,

	/** The platform does not expose it at all. Hidden, not greyed -- a row nobody can ever use is noise. */
	Unsupported
};

/**
 * Base for everything in the settings tree.
 *
 * IT IS A `UObject`, NOT A `UDataAsset` AND NOT A ROW STRUCT. Law 3 governs tuning NUMBERS --
 * weapon damage, bot scalars -- which are authored in CSV because designers change them without
 * a compile. A settings tree is not tuning: it is the SHAPE of a screen, its entries are bound
 * to C++ accessors that must exist at compile time, and a CSV row cannot name a lambda. The
 * numbers a setting does carry (a slider's min/max) are display bounds for a player-owned value,
 * not balance. This is deliberately outside law 3's subject and is recorded as such rather than
 * left to look like an oversight.
 */
UCLASS(Abstract, BlueprintType)
class BREACHPOINT_API UBRSettingsDataObject : public UObject
{
	GENERATED_BODY()

public:
	/** Stable identity. Used by the spec, by save/restore, and by "jump to this setting" links. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	FName GetSettingId() const { return SettingId; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	FText GetDisplayName() const { return DisplayName; }

	/** The paragraph the details view shows while this row has focus. May be empty. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	FText GetDescription() const { return Description; }

	void SetIdentity(FName InId, const FText& InDisplayName, const FText& InDescription);

	/**
	 * How this value reads on the right-hand side of a `UBRMenuRow`. The base returns the em
	 * dash, which is the honest answer for a header or a collection -- see `BRUI::UnknownValueText`
	 * for why blank is never the right blank.
	 */
	virtual FText GetDisplayValue() const;

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	EBRSettingsEditState GetEditState() const { return EditState; }

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	bool IsEditable() const { return EditState == EBRSettingsEditState::Editable; }

	/** Why the row is greyed. Empty when `Editable`. */
	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	FText GetDisabledReason() const { return DisabledReason; }

	void SetEditState(EBRSettingsEditState InState, const FText& InReason);

	/**
	 * True when the live value differs from what it was when `CaptureBaseline` last ran. The
	 * screen uses it for the "you have unsaved changes" confirm on back-out. The base has no
	 * value, so it is never dirty.
	 */
	virtual bool IsDirty() const { return false; }

	/** Put the value back to the game's shipped default. No-op on the base. */
	virtual void ResetToDefault() {}

	/** Snapshot the current value as "unchanged". Called when the settings screen opens. */
	virtual void CaptureBaseline() {}

	/**
	 * Put the value back to the last `CaptureBaseline` snapshot -- the Cancel button.
	 *
	 * DELIBERATELY NOT THE SAME AS `ResetToDefault`, and the pair is the reason both snapshots
	 * exist. Cancel means "forget what I did in this screen"; reset means "forget what I have
	 * ever done". A build that implements one of them twice has a bug the player only finds
	 * after losing a config they spent an hour on.
	 */
	virtual void RestoreBaseline() {}

	/**
	 * Re-read from the backing store and re-evaluate the edit condition. Called after an apply,
	 * and after any sibling changes -- a dependency (`DependencyUnmet`) is only correct if
	 * something recomputes it.
	 */
	virtual void Refresh();

	/** Broadcast when this object's value or edit state changed. */
	FBRSettingsValueChanged OnSettingChanged;

protected:
	/** Fire `OnSettingChanged` for this object. The single place the delegate is raised. */
	void NotifyChanged();

	UPROPERTY(Transient)
	FName SettingId;

	UPROPERTY(Transient)
	FText DisplayName;

	UPROPERTY(Transient)
	FText Description;

	UPROPERTY(Transient)
	EBRSettingsEditState EditState = EBRSettingsEditState::Editable;

	UPROPERTY(Transient)
	FText DisabledReason;
};

/**
 * A named group of settings: a tab ("Video"), or a section header inside one ("Advanced").
 *
 * The tree is deliberately only two levels deep in practice -- tab, then section -- but nothing
 * here enforces a depth, because a limit that is not needed is a limit that will be wrong later.
 * `FlattenVisible` is what the list view actually consumes, and it does the walking.
 */
UCLASS(BlueprintType)
class BREACHPOINT_API UBRSettingsCollection : public UBRSettingsDataObject
{
	GENERATED_BODY()

public:
	void AddChild(UBRSettingsDataObject* Child);

	UFUNCTION(BlueprintCallable, Category = "Breachpoint|Settings")
	const TArray<UBRSettingsDataObject*>& GetChildren() const { return Children; }

	/**
	 * Depth-first walk producing the rows a list view shows: this collection's children in order,
	 * each nested collection contributing a header row followed by its own children.
	 *
	 * `Unsupported` entries are dropped entirely rather than greyed. `DependencyUnmet` entries
	 * are KEPT -- the player needs to see the monitor picker exists and go turn fullscreen on,
	 * and a row that vanishes when you change an unrelated setting reads as a bug.
	 */
	void FlattenVisible(TArray<UBRSettingsDataObject*>& OutRows) const;

	//~ Begin UBRSettingsDataObject interface
	virtual bool IsDirty() const override;
	virtual void ResetToDefault() override;
	virtual void CaptureBaseline() override;
	virtual void RestoreBaseline() override;
	virtual void Refresh() override;
	//~ End UBRSettingsDataObject interface

private:
	/**
	 * Strong refs: the registry builds this tree with `NewObject` and the collection is the only
	 * thing keeping the leaves alive. `TObjectPtr` in a `UPROPERTY` array is what makes that a
	 * GC root rather than a dangling-pointer bug the first time a GC runs mid-menu.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBRSettingsDataObject>> Children;

	/**
	 * Mirrors `Children` for the BlueprintCallable getter, which cannot return a
	 * `TArray<TObjectPtr<>>` by const ref as a reflected type. Rebuilt in `AddChild`, never
	 * written anywhere else.
	 */
	UPROPERTY(Transient)
	TArray<UBRSettingsDataObject*> ChildrenView;
};
