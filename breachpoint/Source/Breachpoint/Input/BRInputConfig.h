// Breachpoint. The hardware -> InputTag map. Soft asset refs only.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StreamableManager.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"

#include "BRInputConfig.generated.h"

class UInputAction;

/**
 * FBRInputAction — one row of the input map: an InputAction asset and the InputTag it means.
 *
 * Specification: BREACHPOINT-ARCHITECTURE.md §3.2.
 *
 * The asset reference is a TSoftObjectPtr and that is law, not preference
 * (docs/contracts/data-and-assets.md: every asset reference in row structs and data assets is
 * TSoftObjectPtr/TSoftClassPtr, resolved via the streamable manager at load points; a hard
 * UPROPERTY asset ref — or a constructor-time asset finder — is a finding. The literal API name
 * is left out of this comment on purpose so a grep audit for it stays clean.) A hard
 * UInputAction* here would drag every input asset into memory the moment this config is
 * referenced, and would make the config uncookable apart from the assets it names.
 *
 * This struct is deliberately NOT in Source/Breachpoint/Data/BRDataRows.h: that header is the
 * home of DataTable row types (FTableRowBase), and this is a member struct of a DataAsset.
 * §3.2 places it here.
 */
USTRUCT()
struct FBRInputAction
{
	GENERATED_BODY()

	/** The Enhanced Input action asset. SOFT — see the struct comment; never a hard pointer. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TSoftObjectPtr<UInputAction> InputAction;

	/**
	 * The tag this hardware action means. Declared natively in Core/BRGameplayTags.h
	 * (InputTag.Move/Look/Jump/Crouch/Sprint/Fire/Reload/Swap/Grenade/Melee/Grapple).
	 * The tag is the whole contract: nothing downstream ever learns which asset produced it.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;

	/** A row is usable when it names an asset AND a tag. Neither half is optional. */
	bool IsValidRow() const
	{
		return !InputAction.IsNull() && InputTag.IsValid();
	}

	/**
	 * Resolve the soft reference.
	 * @param bLoadSynchronousIfNeeded  if the asset is not already resident, block and load it.
	 *        Binding happens once at pawn setup, so the blocking path is survivable — but callers
	 *        that can afford to prewarm should call UBRInputConfig::PreloadInputActionsAsync()
	 *        first and then resolve with bLoadSynchronousIfNeeded = false.
	 * @return the loaded action, or nullptr.
	 */
	const UInputAction* GetInputAction(bool bLoadSynchronousIfNeeded = true) const;
};

/**
 * UBRInputConfig — THE input map: Enhanced Input actions -> InputTags, in two lists.
 *
 * Specification: BREACHPOINT-ARCHITECTURE.md §3.2. The authored instance is Content/Input/
 * DA_InputConfig (authored by a committed generation script per law 7 — never hand-placed).
 *
 * Two lists, because the two halves of the input surface have different destinations:
 *
 *   NativeInputActions  — Move / Look / Jump / Crouch. Bound one-by-one, by tag, to concrete
 *                         pawn functions via UBRInputComponent::BindNativeAction. These are the
 *                         movement/camera verbs the character owns directly; they are NOT
 *                         abilities and never touch the ASC.
 *
 *   AbilityInputActions — Sprint / Fire / Reload / Swap / Grenade / Melee / Grapple. Bound
 *                         wholesale by UBRInputComponent::BindAbilityActions to exactly two
 *                         handlers (pressed / released), each carrying the row's tag. No entry
 *                         in this list is named anywhere in C++.
 *
 * LOADOUT-AGNOSTIC (§3.2, verbatim): "the config maps hardware to tags, never to abilities."
 * There is no ability class, no ability tag, and no loadout reference in this file, and adding
 * one is a finding — the ability side of the pairing lives in BRAbilitySet (§3.3), which pairs
 * {ability class, level, InputTag}. The two meet only at the tag. That is what makes a new
 * ability a DataAsset row instead of an input-code change, and it is what lets a bot inject the
 * same tags into the same ASC path with no input assets at all.
 */
UCLASS(meta = (DisplayName = "BR Input Config"))
class BREACHPOINT_API UBRInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// ---------------------------------------------------------------------
	// Lookup — by tag, always. Nothing in the codebase looks a row up by asset.
	// ---------------------------------------------------------------------

	/**
	 * Find the native (non-ability) action carrying this tag.
	 * @param bEnsureIfNotFound  fire a development-only ensure when the tag is absent. A missing
	 *        native binding is a silently dead key, so setup code should leave this true.
	 */
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bEnsureIfNotFound = true) const;

	/** Find the ability action carrying this tag. See FindNativeInputActionForTag. */
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bEnsureIfNotFound = true) const;

	/** The raw rows. BindAbilityActions walks this list; it never asks for a tag by name. */
	const TArray<FBRInputAction>& GetNativeInputActions() const { return NativeInputActions; }
	const TArray<FBRInputAction>& GetAbilityInputActions() const { return AbilityInputActions; }

	// ---------------------------------------------------------------------
	// Soft-reference resolution (data-and-assets.md: "resolved via the streamable
	// manager at load points").
	// ---------------------------------------------------------------------

	/** Every non-null soft path in both lists, appended to OutPaths. */
	void GetAllInputActionPaths(TArray<FSoftObjectPath>& OutPaths) const;

	/** True when every named action is already resident, i.e. binding will not block. */
	bool AreInputActionsLoaded() const;

	/**
	 * Async-load every action named by this config. Call at a load point (pawn possession,
	 * loadout apply) so the later bind pass resolves without a hitch.
	 * @return the streamable handle — keep it alive to keep the actions resident; null if there
	 *         is nothing to load or no asset manager (e.g. some commandlets).
	 */
	TSharedPtr<FStreamableHandle> PreloadInputActionsAsync(const FStreamableDelegate& OnLoaded = FStreamableDelegate()) const;

#if WITH_EDITOR
	/**
	 * Validates what the compiler cannot: rows are complete, tags are InputTag.*, no tag is
	 * duplicated within a list, and no tag appears in both lists. This is the acceptance test
	 * for the generation script that authors DA_InputConfig — a generated asset that fails here
	 * is a broken control scheme that would otherwise only show up as a dead key in PIE.
	 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	// ---------------------------------------------------------------------
	// The two lists. Public because the committed asset-generation script (law 7: input
	// assets are generated, never hand-placed) must be able to populate them — by editor
	// reflection or from a commandlet — and because that script is the only writer.
	// Runtime code reads through the accessors above; it has no reason to touch these.
	// ---------------------------------------------------------------------

	/** Move / Look / Jump / Crouch — bound to concrete pawn functions, never to the ASC. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Native", meta = (TitleProperty = "InputTag"))
	TArray<FBRInputAction> NativeInputActions;

	/**
	 * Sprint / Fire / Reload / Swap / Grenade / Melee / Grapple — bound wholesale, by tag.
	 *
	 * TRIGGER CONTRACT for the assets in this list (the generation script must honour it):
	 * the IA_ asset must use the DEFAULT trigger set (empty) or an explicit Down trigger, so
	 * that Triggered fires while the key is held and Completed fires on release. An explicit
	 * *Pressed* trigger would end the action one frame after the press and report a release
	 * while the key is still down — which silently breaks WaitInputRelease, hold and toggle
	 * abilities (Sprint, WhileHeld fire). See UBRInputComponent::BindAbilityActions.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Ability", meta = (TitleProperty = "InputTag"))
	TArray<FBRInputAction> AbilityInputActions;

private:
	static const UInputAction* FindActionForTag(const TArray<FBRInputAction>& Rows, const FGameplayTag& InputTag, bool bEnsureIfNotFound, const TCHAR* ListName);
};
