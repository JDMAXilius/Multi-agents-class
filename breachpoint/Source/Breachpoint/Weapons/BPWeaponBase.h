#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "BPWeaponBase.generated.h"

/**
 * C++ base for the FPS template's weapon actors. `BP_FPST_BaseWeapon` reparents onto this,
 * and its four children (Pistol, Rifle, Shotgun, Knife) inherit it.
 *
 * WHY IT EXISTS, AND WHY IT IS EMPTY.
 *
 * `AMyCharacter` used to type its whole weapon API as `AActor*`, because C++ may not hard
 * reference a Blueprint class (law 3). The Blueprint side types the same things as
 * `BP_FPST_BaseWeapon`, so every pin between them was incompatible:
 *
 *     Can't connect pins 'Return Value' and 'Target':
 *     Actor Object Reference is not compatible with BP FPST Base Weapon Object Reference.
 *
 * One shared C++ base makes those pins agree with no cast nodes anywhere.
 *
 * It declares NOTHING on purpose. `BP_FPST_BaseWeapon` already owns functions named
 * `GetAttachSocketName`, `GetCrosshairType`, `GetFireAnimMontage`, `GetReloadAnimMontage`,
 * `GetLinkAnimLayerClass` and friends. A Blueprint function whose name matches a UFUNCTION on
 * its parent is a COMPILE ERROR, not an override — the same shadowing rule that silently
 * renamed this project's camera components. So every getter stays on the Blueprint until it
 * is deliberately moved down here in the same pass that deletes it from the graph.
 *
 * Adding a member to this class without checking `BP_FPST_BaseWeapon` for that name first
 * will break all five weapon assets at once.
 */
UCLASS()
class BREACHPOINT_API ABPWeaponBase : public AActor
{
	GENERATED_BODY()
};
