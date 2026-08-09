#include "Core/OSBootstrapGameMode.h"
#include "Subsystems/OSSessionsSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

AOSBootstrapGameMode::AOSBootstrapGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AOSBootstrapGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Steam authentication is automatic when Steam client is running
	// No manual login required - Steam handles authentication automatically
	// AttemptLogin() is kept for compatibility but does nothing
}

void AOSBootstrapGameMode::AttemptLogin()
{
	// Steam authentication is automatic when Steam client is running
	// No manual login required - Steam handles authentication automatically
	// This function is kept for compatibility but does nothing
}

