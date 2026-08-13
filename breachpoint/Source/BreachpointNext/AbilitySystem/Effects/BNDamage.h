#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "BNDamage.generated.h"

struct FBNWeaponRow;

namespace BNSetByCaller
{
	/** UBNGE_Damage's magnitude key. An FName, not a tag, for the same construction-order reason
	 *  BNSetByCaller::FireDelay is one: native tags are not guaranteed registered while CDOs build. */
	const FName Damage(TEXT("Damage"));
}

/**
 * Instant, one SetByCaller modifier into the IncomingDamage META attribute. No execution
 * calculation: UBNAttributeSet::PostGameplayEffectExecute drains shield-then-health, and that is
 * the whole pipeline this wave gets.
 */
UCLASS()
class BREACHPOINTNEXT_API UBNGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBNGE_Damage();
};

/**
 * THE one damage door. Every damage spec in this module is built inside these two functions and
 * nowhere else — the real pipeline (mitigation, damage types, falloff, friendly fire: none of
 * which exist and none of which are this wave's business) replaces their INSIDES, not one caller.
 *
 * Authority only, always. A call from a client is refused and logged rather than silently doing
 * nothing: an attribute a client wrote is never corrected, because replication only sends changes.
 */
namespace BNDamage
{
	BREACHPOINTNEXT_API void ApplyDamage(AActor* Instigator, AActor* Target, float Amount, const FHitResult& Hit);

	/** The weapon-shaped front of the same door: the number comes from the ROW, so the headshot
	 *  rule lives behind the door and no caller ever multiplies a damage value. */
	BREACHPOINTNEXT_API void ApplyWeaponDamage(AActor* Instigator, const FBNWeaponRow& Row, const FHitResult& Hit);
}
