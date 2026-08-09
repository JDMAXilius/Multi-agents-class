// Copyright Zollpa LLC


#include "ZoransAIController.h"
//#include "/Characters/Components/ComponentInterface.h"
#include "BehaviorTree/BehaviorTree.h"
#include "ZoransResistance/Characters/Components/ComponentInterface.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "ZoransResistance/ZoransResistance.h"
#include "ZoransResistance/Game/ZoransGameInstance.h"
#include "ZoransResistance/Game/ZoransGameState.h"
#include "ZoransResistance/Characters/Components/ZoransWeaponComponent.h"
#include "ZoransResistance/Game/ZoransWorldSubsystem.h"

AZoransAIController::AZoransAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bWantsPlayerState = true;
	
	BBC = CreateDefaultSubobject<UBlackboardComponent>(TEXT("Blackboard Component"));
	//BehaviorTree->BlackboardAsset = BBC;

	//AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));
}

void AZoransAIController::SetWeaponDefaults()
{
	if (ZW)
	{
		if (AZoransWeaponActor* ZWA = ZW->GetCurrentEquippedWeapon())
		{
			
			FZoransWeaponData ZWD = ZWA->GetWeaponDefaults();
			BBC->SetValueAsEnum("Engagement Range",(uint8)ZWD.EngagementRange);
		}
	}
}

void AZoransAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetOwningAI(Cast<AZoransCharacterBase>(InPawn));
	// Initialize the Blackboard
	//BBC->InitializeBlackboard(*GetOwningAI()->TreeAsset->BlackboardAsset);
	if (GetOwningAI())
	{
		if (UZoransWeaponComponent* ZWC = GetOwningAI()->GetWeaponComponent())
		{
			ZW = ZWC;
			ZWC->CurrentWeaponSlotChanged.AddUniqueDynamic(this, &AZoransAIController::SetWeaponDefaults);
			//BBC->SetValueAsEnum("Engagement Range",(uint8)AIEngageRangeMap.Find(ZWC->GetCurrentEquippedWeapon()->GetWeaponDefaults().EngagementRange));
			//BBC->SetValueAsEnum("Engagement Range",(uint8)ZWC->GetCurrentEquippedWeapon()->GetWeaponDefaults().EngagementRange);
			
		}
	}
	const UZoransGameInstance* ZoransGameInstance = Cast<UZoransGameInstance>(InPawn->GetGameInstance());
	if (ZoransGameInstance)
	{
		const auto& AIInfo = ZoransGameInstance->AIStatsInfo.Find(ZoransGameInstance->AIDificulty);
		AIStatsInfo = *AIInfo;
		AIDificulty = ZoransGameInstance->AIDificulty;
	}
	// Start The BehaviorTree.
	//BTC->StartTree(*Chr->TreeAsset);

	if (AZoransGameState* ZWS = GetWorld()->GetGameState<AZoransGameState>())
	{
		BBC->SetValueAsEnum("GameMode Override", (uint8)ZWS->GetGameModeOverride());
		//UE_LOG(ZoransLog, Warning, TEXT("GameMode Override: %d"), ZWS->GetGameModeOverride());
		//FString EnumString = UEnum::GetValueAsString(ZWS->GetGameModeOverride());
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("Enum Value: %s"), *EnumString));
		//UE_LOG(ZoransLog, Warning, TEXT("Replication Properties: ROLE = '%s' ,REMOTEROLE = '%s') ") *Role, GetRemoteRole() );
	}
}

void AZoransAIController::GetGameModeOverride(UZoransGameState* ZWState)
{
	/*switch (ZWState->GetZoransGameState()->GetGameModeOverride())
	{
	case EGameModeOverride::Default:BehaviorTree = BehaviorTree_Default; break;
	case EGameModeOverride::Tutorial: BehaviorTree = BehaviorTree_Tutorial; break;
	case EGameModeOverride::TeamDeathMatch: BehaviorTree = BehaviorTree_TeamDeathMatch; break;
	case EGameModeOverride::ControlPoint: BehaviorTree = BehaviorTree_ControlPoint; break;
	case EGameModeOverride::GunGame:BehaviorTree = BehaviorTree_GunGame; break;
	case EGameModeOverride::DataDash: BehaviorTree = BehaviorTree_DataDash; break;
	default: BehaviorTree = BehaviorTree_Default; break;;
	};*/
}

void AZoransAIController::BeginPlay()
{
	Super::BeginPlay();
	UZoransGameInstance* GI = Cast<UZoransGameInstance>(GetGameInstance());
	if(UZoransWorldSubsystem* ZWState = GetWorld()->GetSubsystem<UZoransWorldSubsystem>())
	{
		if (ZWState->GetZoransGameState())
		{
			switch (ZWState->GetZoransGameState()->GetGameModeOverride())
			{
			case EGameModeOverride::Default:BehaviorTree = BehaviorTree_Default; break;
			case EGameModeOverride::Tutorial: BehaviorTree = BehaviorTree_Tutorial; break;
			case EGameModeOverride::TeamDeathMatch: BehaviorTree = BehaviorTree_TeamDeathMatch; break;
			case EGameModeOverride::ControlPoint: BehaviorTree = BehaviorTree_ControlPoint; break;
			case EGameModeOverride::GunGame:BehaviorTree = BehaviorTree_GunGame; break;
			case EGameModeOverride::DataDash: BehaviorTree = BehaviorTree_DataDash; break;
			default: BehaviorTree = BehaviorTree_Default; break;;
			};
		}
		/*else
		{
			ZWState->OnGameStateReadyDelegate.AddUnique(this,&ThisClass::GetGameModeOverride(ZWState));
		}*/
	}
	
	/*switch (AIDificulTyLevel)
	{
	case EAIDificulTyLevel::Easy:
		BehaviorTree = BehaviorTree_Default;
		break;
		
	case EAIDificulTyLevel::Medium:
		BehaviorTree = BehaviorTree_Tutorial;
		break;
	case EAIDificulTyLevel::Hard:
		BehaviorTree = BehaviorTree_TeamDeathMatch;
		break;
	case EAIDificulTyLevel::Insane:
		BehaviorTree = BehaviorTree_ControlPoint;
		break;
	default:
		BehaviorTree = BehaviorTree_Default;
		break;
	}*/
	
	if (ensureMsgf(BehaviorTree, TEXT("Behavior Tree is nullptr! Please assign BehaviorTree in your AI Controller.")))
	{
		RunBehaviorTree(BehaviorTree);
	}
	
	BBC->SetValueAsBool("OutOfAmmo",true);
	//BBC->SetValueAsEnum("Engagement Range", (uint8)OwningAI->GetWeaponComponent()->GetCurrentEquippedWeapon()->GetWeaponDefaults().EngagementRange);

	//AIStatsInfo
	GetBlackboardComponent()->SetValueAsFloat("AccurayLevel", GI->AIStatsInfo.Find(GI->AIDificulty)->AccurayLevel);
	BBC->SetValueAsFloat("AggressionLevel", AIStatsInfo.AggressionLevel);
	BBC->SetValueAsFloat("ReactionTime", GI->AIStatsInfo.Find(GI->AIDificulty)->ReactionTime);
	BBC->SetValueAsFloat("MaxDistanceTrace", GI->AIStatsInfo.Find(GI->AIDificulty)->MaxDistanceTrace);
	BBC->SetValueAsBool("HeadShots",GI->AIStatsInfo.Find(GI->AIDificulty)->HeadShots);
	BBC->SetValueAsBool("OutOfAmmo",GI->AIStatsInfo.Find(GI->AIDificulty)->PickWeaponAtStart);
	BBC->SetValueAsBool("TakesCover",GI->AIStatsInfo.Find(GI->AIDificulty)->TakesCover);
	BBC->SetValueAsBool("Flanks",GI->AIStatsInfo.Find(GI->AIDificulty)->Flanks);

	//UZoransWorldSubsystem* ZWS = GetWorld()->GetSubsystem<UZoransWorldSubsystem>();
	//AZoransGameState* ZWS = GetWorld()->GetGameState<AZoransGameState>();
	if (AZoransGameState* ZWS = GetWorld()->GetGameState<AZoransGameState>())
	{
		BBC->SetValueAsEnum("GameMode Override", (uint8)ZWS->GetGameModeOverride());
		//UE_LOG(ZoransLog, Warning, TEXT("GameMode Override: %d"), ZWS->GetGameModeOverride());
		//FString EnumString = UEnum::GetValueAsString(ZWS->GetGameModeOverride());
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("Enum Value: %s"), *EnumString));
		//UE_LOG(ZoransLog, Warning, TEXT("Replication Properties: ROLE = '%s' ,REMOTEROLE = '%s') ") *Role, GetRemoteRole() );
	}
	
}

void AZoransAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwningAI())
	{
		//UE_LOG(ZoransLog, Error, TEXT("DESTROYING OWNING AI"));
		GetOwningAI()->Destroy();
	}
	
	Super::EndPlay(EndPlayReason);
}
