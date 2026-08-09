#include "Characters/AI/OSAICharacter.h"

#include "GAS/Components/OSAbilitySystemComponent.h"
#include "GameFramework/Controller.h"
#include "GAS/Attributes/OSAttributeSet.h"

AOSAICharacter::AOSAICharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// ========================================
	// AI-SPECIFIC CONFIGURATION
	// ========================================
	// AI characters can have different movement settings
	// Example: Slower movement, different rotation rates, etc.
	AbilitySystemComponent = CreateDefaultSubobject<UOSAbilitySystemComponent>("AbilitySystemComponent");
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetIsReplicated(true);
		AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	}
	AttributeSet = CreateDefaultSubobject<UOSAttributeSet>("AttributeSet");
}

bool AOSAICharacter::IsLocallyControlled() const
{
	// Unpossessed, non-replicated pawns (punching bag, local-only debug dummies)
	// exist only on the spawning machine — they are trivially "local" for all
	// presentation paths (audio, VFX, GameplayCues). This single override
	// replaces per-callsite !GetController() checks across the codebase.
	if (!GetController() && !GetIsReplicated())
	{
		return true;
	}
	return Super::IsLocallyControlled();
}

void AOSAICharacter::BeginPlay()
{
	Super::BeginPlay();

	// ========================================
	// AI INITIALIZATION
	// ========================================
	// AI-specific initialization can be added here
	// Use the unified GAS init + startup loadout pipeline.
	InitializeGAS();
}

void AOSAICharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// ========================================
	// AI CONTROLLER SETUP
	// ========================================
	// AI-specific controller setup can be added here
	// Example: Set Behavior Tree, Blackboard, etc.
	// Note: Add AIModule to Build.cs when using AIController
}

