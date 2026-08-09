// Copyright Zollpa LLC.


#include "ZoransGameInstance.h"
#if PLATFORM_LINUX
#include "Unix/UnixPlatformMisc.h"
#else
#include "Windows/WindowsPlatformMisc.h" // Include Windows header for non-Linux platforms
#endif
#include "ShaderCompiler.h"
#include "AbilitySystemGlobals.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "ZoransResistance/Audio/ZoransInstanceAudioSubsystem.h"

UZoransGameInstance::UZoransGameInstance()
{
	
}

void UZoransGameInstance::Init()
{
	Super::Init();

	GetSubsystem<UZoransInstanceAudioSubsystem>()->Config = ZoransInstanceAudioSubsystemConfig;
	
	UAbilitySystemGlobals::Get().InitGlobalData();

#if PLATFORM_LINUX
	RegionEnvVar = FUnixPlatformMisc::GetEnvironmentVariable(TEXT("SERVER_REGION"));
	IsMatchmakingServer = FUnixPlatformMisc::GetEnvironmentVariable(TEXT("IS_MATCHMAKING_SERVER"));
	IsDevOnlyServer = FUnixPlatformMisc::GetEnvironmentVariable(TEXT("DEVS_ONLY"));
	MapRouletteEnabled = FUnixPlatformMisc::GetEnvironmentVariable(TEXT("MAP_ROULETTE_ENABLED"));
	GameModeRouletteEnabled = FUnixPlatformMisc::GetEnvironmentVariable(TEXT("GAMEMODE_ROULETTE_ENABLED"));
	BotsEnabled = FUnixPlatformMisc::GetEnvironmentVariable(TEXT("BOTS_ENABLED"));
	VersionCode = FUnixPlatformMisc::GetEnvironmentVariable(TEXT("VERSION_CODE"));
	MatchTimeOverrideString = FUnixPlatformMisc::GetEnvironmentVariable(TEXT("MATCHTIME_OVERRIDE"));
	MatchTimeOverrideFloat = FCString::Atof(*MatchTimeOverrideString);
	Temp_ShouldDestroyPlayerState = FUnixPlatformMisc::GetEnvironmentVariable(TEXT("DESTROY_PS"));
	ServerName = FUnixPlatformMisc::GetEnvironmentVariable(TEXT("SERVER_NAME"));
	PortBeacon = FUnixPlatformMisc::GetEnvironmentVariable(TEXT("PORT_BEACON"));
	IsSandbox = FUnixPlatformMisc::GetEnvironmentVariable(TEXT("IS_SANDBOX"));
	ServerIP = FUnixPlatformMisc::GetEnvironmentVariable(TEXT("SERVER_IP"));

#else
	RegionEnvVar = FWindowsPlatformMisc::GetEnvironmentVariable(TEXT("SERVER_REGION"));
	IsMatchmakingServer = FWindowsPlatformMisc::GetEnvironmentVariable(TEXT("IS_MATCHMAKING_SERVER"));
	IsDevOnlyServer = FWindowsPlatformMisc::GetEnvironmentVariable(TEXT("DEVS_ONLY"));
	MapRouletteEnabled = FWindowsPlatformMisc::GetEnvironmentVariable(TEXT("MAP_ROULETTE_ENABLED"));
	GameModeRouletteEnabled = FWindowsPlatformMisc::GetEnvironmentVariable(TEXT("GAMEMODE_ROULETTE_ENABLED"));
	BotsEnabled = FWindowsPlatformMisc::GetEnvironmentVariable(TEXT("BOTS_ENABLED"));
	VersionCode = FWindowsPlatformMisc::GetEnvironmentVariable(TEXT("VERSION_CODE"));
	MatchTimeOverrideString = FWindowsPlatformMisc::GetEnvironmentVariable(TEXT("MATCHTIME_OVERRIDE"));
	MatchTimeOverrideFloat = FCString::Atof(*MatchTimeOverrideString);
	ServerName = FWindowsPlatformMisc::GetEnvironmentVariable(TEXT("SERVER_NAME"));
	PortBeacon = FWindowsPlatformMisc::GetEnvironmentVariable(TEXT("PORT_BEACON"));
	IsSandbox = FWindowsPlatformMisc::GetEnvironmentVariable(TEXT("IS_SANDBOX"));
	ServerIP = FWindowsPlatformMisc::GetEnvironmentVariable(TEXT("SERVER_IP"));

#endif
	
}

void UZoransGameInstance::FinishCompilingShaders()
{
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
		UE_LOG(LogTemp, Warning, TEXT("tezt Finished All Shader Compilation!"));
	}
}

bool UZoransGameInstance::GetIsAsyncCompilingShaders()
{
	if (GShaderCompilingManager)
	{
		bool bShadersStillCompiling;
		bShadersStillCompiling = GShaderCompilingManager->IsCompiling();
		FString AreShadersStillCompiling = bShadersStillCompiling ? TEXT("true") : TEXT("false");
		UE_LOG(LogTemp, Warning, TEXT("tezt ShadersStillCompiling = %s"), *AreShadersStillCompiling);
		return bShadersStillCompiling;
	}
	else
	{
		return false;
	}
}

void UZoransGameInstance::ShowNetworkErrorWidget()
{
	if (NetworkErrorWidget)
	{
		NetworkErrorWidget->AddToViewport(10000);
		return;
	}
	NetworkErrorWidget = CreateWidget<UUserWidget>(this, NetworkErrorWidgetClass);
	if (NetworkErrorWidget)
	{
		NetworkErrorWidget->AddToViewport(10000);
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("Failed to create network error widget! Widget was null."));
}

void UZoransGameInstance::ShowMigrationWidget()
{
	if (MigrationWidget)
	{
		MigrationWidget->AddToViewport(10000);
		return;
	}
	MigrationWidget = CreateWidget<UUserWidget>(this, MigrationWidgetClass);
	if (MigrationWidget)
	{
		MigrationWidget->AddToViewport(10000);
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("Failed to create host migration widget! Widget was null."));
}

void UZoransGameInstance::HideNetworkErrorWidget()
{
	if (NetworkErrorWidget)
	{
		NetworkErrorWidget->RemoveFromParent();
		NetworkErrorWidget = nullptr;
	}
}

void UZoransGameInstance::HideMigrationWidget()
{
	if (MigrationWidget)
	{
		MigrationWidget->RemoveFromParent();
		MigrationWidget = nullptr;
	}
}
