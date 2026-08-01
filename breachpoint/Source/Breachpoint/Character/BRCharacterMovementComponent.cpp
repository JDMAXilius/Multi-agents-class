// Breachpoint. The CMC subclass. Registered and empty — sprint and grapple arrive later.

#include "Character/BRCharacterMovementComponent.h"

UBRCharacterMovementComponent::UBRCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Deliberately empty. No saved moves, no compressed flags, no tuning numbers — see the
	// header. The one thing that must be true of this class in BP01 is that it is the class
	// ABRCharacter actually instantiates, and that is asserted in ABRCharacter's constructor.
}
