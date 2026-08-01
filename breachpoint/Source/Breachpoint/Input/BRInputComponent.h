// Breachpoint. The Enhanced Input -> InputTag routing component.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "GameplayTagContainer.h"
#include "InputTriggers.h"

#include "Input/BRInputConfig.h"

#include "BRInputComponent.generated.h"

/**
 * UBRInputComponent — the one place hardware input becomes a gameplay tag.
 *
 * Specification: BREACHPOINT-ARCHITECTURE.md §3.2, the five-arrow flow:
 *
 *   IMC_Default -> UInputAction -> BRInputComponent -> InputTag.Fire
 *     -> BRPlayerController::AbilityInputTagPressed(Tag)
 *     -> BRAbilitySystemComponent::AbilityInputTagPressed(Tag)   <- input buffer lives here
 *     -> activates granted abilities whose BRAbilitySet entry carries that InputTag
 *
 * THE LAW THIS CLASS ENCODES (§3.2, verbatim): "abilities are bound by tag, not by binding
 * code — a new ability is a DataAsset row, no input code changes."
 *
 * Concretely: this file names ZERO abilities and ZERO input actions. BindAbilityActions walks
 * whatever rows UBRInputConfig carries and binds each to the same two handlers, so adding
 * Grapple, or a Phase-2 magic slot, is one row in DA_InputConfig plus one row in the ability
 * set — never an edit here. If a change to the game would require editing this file, the change
 * is being made in the wrong place.
 *
 * Two handlers, not one, is also load-bearing: press AND release must both reach the ASC or
 * WaitInputRelease / WhileHeld (sprint, held fire) / toggle abilities cannot exist.
 *
 * NOT this class's job: authority. Nothing here replicates, and nothing here decides whether an
 * ability may run — the handlers hand a tag to the owning actor and stop. The ASC (BP02) owns
 * the buffer, the prediction window and the server's say.
 */
UCLASS(meta = (DisplayName = "BR Input Component"))
class BREACHPOINT_API UBRInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	UBRInputComponent(const FObjectInitializer& ObjectInitializer);

	/**
	 * Bind ONE native (non-ability) action — Move / Look / Jump / Crouch — to a concrete
	 * function on Object. Native actions are looked up by tag but delivered untagged: the
	 * handler already knows what it is, so it takes the usual Enhanced Input signature
	 * (e.g. void Move(const FInputActionValue& Value)).
	 *
	 * @param TriggerEvent  usually ETriggerEvent::Triggered for Move/Look, and
	 *                      Started / Completed for a press/release pair like Crouch.
	 * @param bEnsureIfNotFound  ensure (development builds) when the config has no such row;
	 *                      leave true, because the failure mode is a silently dead key.
	 */
	template<class UserClass, typename FuncType>
	void BindNativeAction(const UBRInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, bool bEnsureIfNotFound = true);

	/**
	 * Bind EVERY ability action in the config to exactly two handlers on Object, each handler
	 * receiving the row's FGameplayTag as a bound payload argument.
	 *
	 * The handlers must have this exact signature (payload is passed by value):
	 *
	 *     void UserClass::AbilityInputTagPressed(FGameplayTag InputTag);
	 *     void UserClass::AbilityInputTagReleased(FGameplayTag InputTag);
	 *
	 * HOW THE TAG TRAVELS — the one detail every later ability depends on: Enhanced Input's
	 * BindAction is variadic in its trailing payload (VarTypes...). We pass Row.InputTag as
	 * that payload, so the tag is captured into the delegate at BIND time, once, and delivered
	 * with every fire. There is no per-ability function, no switch, no map lookup at input time,
	 * and no lambda capturing 'this'.
	 *
	 * TRIGGER EVENTS. Defaults are Triggered (press) and Completed (release), which is what the
	 * default/Down trigger set produces: Triggered while the key is held, Completed when it is
	 * let go. Two consequences the ASC packet (BP02) must know:
	 *   1. the pressed handler fires REPEATEDLY while the key is held, so ASC-side press
	 *      handling must be idempotent (Lyra's AddUnique-into-InputHeld pattern);
	 *   2. an IA_ asset authored with a *Pressed* trigger would report Completed one frame after
	 *      the press, i.e. a release while the key is still down. The trigger contract for
	 *      ability actions is documented on UBRInputConfig::AbilityInputActions.
	 * Both events are parameters precisely so BP02 can change this policy without editing this
	 * file (e.g. Started for a one-shot press) — that is the point of the class.
	 *
	 * @param BindHandles  optional; receives every handle created, for RemoveBinds() on a
	 *                     loadout or pawn change. Pass nullptr for bindings that live as long
	 *                     as the component.
	 */
	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
	void BindAbilityActions(const UBRInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, TArray<uint32>* BindHandles = nullptr, ETriggerEvent PressedEvent = ETriggerEvent::Triggered, ETriggerEvent ReleasedEvent = ETriggerEvent::Completed);

	/** Remove bindings previously collected into BindHandles, and empty the array. */
	void RemoveBinds(TArray<uint32>& BindHandles);
};

// ---------------------------------------------------------------------------
// Templates. Header-inline by necessity: the handler type is the caller's.
// ---------------------------------------------------------------------------

template<class UserClass, typename FuncType>
void UBRInputComponent::BindNativeAction(const UBRInputConfig* InputConfig, const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, bool bEnsureIfNotFound)
{
	if (!ensureMsgf(InputConfig != nullptr, TEXT("BRInputComponent::BindNativeAction called with no input config; every key on this pawn is dead.")))
	{
		return;
	}

	if (const UInputAction* Action = InputConfig->FindNativeInputActionForTag(InputTag, bEnsureIfNotFound))
	{
		BindAction(Action, TriggerEvent, Object, Func);
	}
}

template<class UserClass, typename PressedFuncType, typename ReleasedFuncType>
void UBRInputComponent::BindAbilityActions(const UBRInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, TArray<uint32>* BindHandles, ETriggerEvent PressedEvent, ETriggerEvent ReleasedEvent)
{
	if (!ensureMsgf(InputConfig != nullptr, TEXT("BRInputComponent::BindAbilityActions called with no input config; no ability is reachable from hardware.")))
	{
		return;
	}

	for (const FBRInputAction& Row : InputConfig->GetAbilityInputActions())
	{
		if (!Row.InputTag.IsValid())
		{
			// A row with no tag has no destination. Reported by UBRInputConfig::IsDataValid.
			continue;
		}

		const UInputAction* Action = Row.GetInputAction();
		if (!Action)
		{
			ensureMsgf(false, TEXT("BRInputComponent: ability row '%s' has no loadable InputAction; that ability is unreachable from hardware."), *Row.InputTag.ToString());
			continue;
		}

		if (PressedFunc)
		{
			// Row.InputTag is the trailing payload — see the header comment. This is the whole
			// mechanism by which a nameless config row reaches a named ability.
			FEnhancedInputActionEventBinding& Binding = BindAction(Action, PressedEvent, Object, PressedFunc, Row.InputTag);
			if (BindHandles)
			{
				BindHandles->Add(Binding.GetHandle());
			}
		}

		if (ReleasedFunc)
		{
			FEnhancedInputActionEventBinding& Binding = BindAction(Action, ReleasedEvent, Object, ReleasedFunc, Row.InputTag);
			if (BindHandles)
			{
				BindHandles->Add(Binding.GetHandle());
			}
		}
	}
}
