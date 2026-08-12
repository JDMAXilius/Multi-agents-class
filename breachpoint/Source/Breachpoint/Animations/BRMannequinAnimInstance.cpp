#include "Animations/BRMannequinAnimInstance.h"

// Plain writes, matching the asset: SetADS is SetVariableOnPersistentFrame + an
// ubergraph kick, and the ubergraph is not ported yet.

// SetADS: no matching field in the reader output - left unimplemented on purpose.
void UBRMannequinAnimInstance::SetADS(bool InADS) { }

void UBRMannequinAnimInstance::SetADS_Upper(bool InADS_Upper)
{
	IsADS_Upper = InADS_Upper;
}

void UBRMannequinAnimInstance::SetSprinting(bool InSprinting)
{
	bSprinting = InSprinting;
}

void UBRMannequinAnimInstance::SetUnarmed(bool InUnarmed)
{
	bUnarmed = InUnarmed;
}

