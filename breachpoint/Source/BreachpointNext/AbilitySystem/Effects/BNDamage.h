#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "BNDamage.generated.h"

struct FBNWeaponRow;

/**
 * WHAT killed you, as one FName — the killfeed's glyph and the death screen's weapon line.
 *
 * A weapon's name IS its row name (the row is a weapon's whole identity in this module), so the
 * UI resolves it against the weapon table and gets DisplayName + Icon for free. The two causes
 * that have no row are named here, and the UI falls back to the NAME ITSELF when the table has
 * no such row — so "Melee" renders as MELEE with no second mapping table to keep in sync, and
 * the day a Grenade row exists in DT_BNWeapons it starts drawing an icon with no code change.
 */
namespace BNDamageSource
{
	const FName Melee(TEXT("Melee"));
	const FName Grenade(TEXT("Grenade"));
}

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
	BREACHPOINTNEXT_API void ApplyDamage(AActor* Instigator, AActor* Target, float Amount, const FHitResult& Hit, FName SourceName = NAME_None);

	/** The weapon-shaped front of the same door: the number comes from the ROW, so the headshot
	 *  rule lives behind the door and no caller ever multiplies a damage value. RowName is passed
	 *  ALONGSIDE the row because a row does not know its own key. */
	BREACHPOINTNEXT_API void ApplyWeaponDamage(AActor* Instigator, FName RowName, const FBNWeaponRow& Row, const FHitResult& Hit);

	/** The cause of the damage being applied RIGHT NOW — meaningful only inside a door call, on
	 *  the authority, and read by exactly one caller: UBNAttributeSet::PostGameplayEffectExecute,
	 *  which captures it into FBNLastDamage.
	 *
	 *  WHY A STASH AND NOT THE SPEC: an FName cannot ride FGameplayEffectContext without a custom
	 *  context subclass (NetSerialize + TStructOpsTypeTraits + an allocation override) — a
	 *  gameplay packet with its own critic pass, which is what R7.1 deferred. The door is
	 *  SYNCHRONOUS on the server (an instant GE runs PostGameplayEffectExecute inside
	 *  ApplyGameplayEffectSpecToSelf, in this call stack), so the value only has to survive four
	 *  frames of stack, and the door saves/restores it so nested damage cannot lose the outer
	 *  cause. Nothing replicates: the killfeed carries the name from the server as a field. */
	BREACHPOINTNEXT_API FName GetApplyingSourceName();
}
