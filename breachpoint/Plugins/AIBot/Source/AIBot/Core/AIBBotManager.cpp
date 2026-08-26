#include "Core/AIBBotManager.h"

#include "AIBotModule.h"
#include "Engine/World.h"
#include "Interfaces/AIBAmbitionProvider.h"
#include "Interfaces/AIBWorldQuery.h"

bool UAIBBotManager::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Game worlds only — the shape the host's own cue registrar proved: editor preview
	// worlds get no bot machinery.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UAIBBotManager::RegisterProviders(UObject* AmbitionProvider, UObject* WorldQuery)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		// Law 3: providers are authority state. A client registering one is a wiring
		// bug worth a loud line, not a silent acceptance.
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: RegisterProviders refused on a client world."));
		return;
	}

	AmbitionProviderObject = AmbitionProvider;
	WorldQueryObject = WorldQuery;
	UE_LOG(LogAIBot, Log, TEXT("AIBot: providers registered (ambitions: %s, world query: %s)."),
		*GetNameSafe(AmbitionProvider), *GetNameSafe(WorldQuery));
}

IAIBAmbitionProvider* UAIBBotManager::GetAmbitionProvider() const
{
	UObject* Object = AmbitionProviderObject.Get();
	return Object ? Cast<IAIBAmbitionProvider>(Object) : nullptr;
}

IAIBWorldQuery* UAIBBotManager::GetWorldQuery() const
{
	UObject* Object = WorldQueryObject.Get();
	return Object ? Cast<IAIBWorldQuery>(Object) : nullptr;
}
