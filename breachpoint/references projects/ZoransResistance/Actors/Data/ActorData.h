// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "ActorData.generated.h"

UENUM(BlueprintType)
enum class EActorControlState : uint8
{
	// The actor is neither controlled nor contested.
	// This will usually be the default state.
	// Note: A team may control this actor and it still be uncontested if there are no enemies vying for control.
	Uncontested		UMETA(DisplayName = "Uncontested"),

	// One Team has a dominating presence for the actor and will eventually gain "Controlled" status if they continue to hold the actor.
	Contested		UMETA(DisplayName = "Contested"),

	// Both teams have equal presence and the actor will remain in its current state until this status changes.
	Neutralized		UMETA(DisplayName = "Neutralized")
};

static inline FName ActorTag_CosmeticWeapon = "ActorTag_CosmeticWeapon";