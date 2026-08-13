#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "BNCheatManager.generated.h"

/**
 * The testing levers for the death loop, and nothing else.
 *
 * A console command runs on the machine that TYPED it, and damage is the authority's alone — so
 * every lever here forwards itself to the server through APlayerController::ServerCheat before it
 * touches anything, and lands back in the same function with HasAuthority() finally true. Without
 * that, a PIE-client typing a cheat gets a silent no-op, which is the worst possible test result.
 *
 * Numbers are not logged here: they are logged once, where they change, in
 * UBNAttributeSet::PostGameplayEffectExecute. Every lever below goes through the one damage door.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	/** Damage yourself by N through BNDamage — the same door a bullet uses. */
	UFUNCTION(Exec)
	void BNDamageSelf(float Amount = 25.f);

	/** Exactly lethal: shield + health, computed and paid through the same door. */
	UFUNCTION(Exec)
	void BNKillSelf();

	/** Health and shields back to full through the INIT GE — the one respawn uses. Never a set. */
	UFUNCTION(Exec)
	void BNRefill();

	/** The debug key's entry point, so the key and the console command share ONE route and one
	 *  authority hop. Static because a client's CheatManager may not exist while its PC does. */
	static void RouteDamageSelf(APlayerController* PC, float Amount = 25.f);
};
