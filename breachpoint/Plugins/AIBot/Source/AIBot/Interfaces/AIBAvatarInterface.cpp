#include "Interfaces/AIBAvatarInterface.h"

#include "Core/AIBTags.h"

namespace AIB
{
void ReleaseHeldVerbs(IAIBAvatarInterface& Avatar)
{
	for (const FGameplayTag& Verb : AIBTags::HeldVerbs())
	{
		Avatar.ReleaseVerb(Verb);
	}

	// AND THE CROUCH, which a release cannot give back. It is a TOGGLE: the only way up
	// is another tap, and tapping blind would put a standing bot into a squat on the way
	// out. So it asks the avatar's REAL state through the door - the same rule SetCrouch
	// follows, and the same one aib-critic L2 found broken on the corpse path, where a
	// dead bot kept its reload crouch.
	if (Avatar.IsCrouched())
	{
		Avatar.PressVerb(AIBTags::Verb_Crouch);
		Avatar.ReleaseVerb(AIBTags::Verb_Crouch);
	}
}
}
