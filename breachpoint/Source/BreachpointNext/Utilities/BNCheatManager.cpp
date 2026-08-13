#include "Utilities/BNCheatManager.h"

#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystem/Effects/BNDamage.h"
#include "Match/BNPlayerState.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	/** True when the call was forwarded and this machine must do nothing else. */
	bool BNForwardToAuthority(APlayerController* PC, const FString& Command)
	{
		if (!PC || PC->HasAuthority())
		{
			return false;
		}

		// The engine's own console-to-server channel: the server runs the identical command on
		// its own cheat manager, which is the machine allowed to move an attribute.
#if !UE_BUILD_SHIPPING
		PC->ServerCheat(Command);
#endif
		return true;
	}
}

void UBNCheatManager::BNDamageSelf(float Amount)
{
	RouteDamageSelf(GetOuterAPlayerController(), Amount);
}

void UBNCheatManager::RouteDamageSelf(APlayerController* PC, float Amount)
{
	if (!PC || BNForwardToAuthority(PC, FString::Printf(TEXT("BNDamageSelf %f"), Amount)))
	{
		return;
	}

	APawn* Pawn = PC->GetPawn();
	BNDamage::ApplyDamage(Pawn, Pawn, Amount, FHitResult());
}

void UBNCheatManager::BNKillSelf()
{
	APlayerController* PC = GetOuterAPlayerController();
	if (!PC || BNForwardToAuthority(PC, TEXT("BNKillSelf")))
	{
		return;
	}

	const ABNPlayerState* PS = PC->GetPlayerState<ABNPlayerState>();
	const UAbilitySystemComponent* ASC = PS ? PS->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return;
	}

	// Lethal is COMPUTED and then paid through the door, rather than asserted by writing zero:
	// death must arrive the way a bullet's would, or this lever tests a path nothing else uses.
	APawn* Pawn = PC->GetPawn();
	BNDamage::ApplyDamage(Pawn, Pawn,
		ASC->GetNumericAttribute(UBNAttributeSet::GetHealthAttribute())
			+ ASC->GetNumericAttribute(UBNAttributeSet::GetShieldAttribute()),
		FHitResult());
}

void UBNCheatManager::BNRefill()
{
	APlayerController* PC = GetOuterAPlayerController();
	if (!PC || BNForwardToAuthority(PC, TEXT("BNRefill")))
	{
		return;
	}

	if (ABNPlayerState* PS = PC->GetPlayerState<ABNPlayerState>())
	{
		PS->ApplyInitAttributes();
	}
}
