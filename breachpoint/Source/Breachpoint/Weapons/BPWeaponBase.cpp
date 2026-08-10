#include "Weapons/BPWeaponBase.h"

// Intentionally empty. See BPWeaponBase.h for why this class declares nothing:
// every getter it could plausibly own already exists by name on BP_FPST_BaseWeapon,
// and a Blueprint function that shadows a parent UFUNCTION is a compile error.
