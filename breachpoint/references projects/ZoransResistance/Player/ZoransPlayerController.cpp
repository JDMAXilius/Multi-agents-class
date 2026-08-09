// Copyright Zollpa LLC.


#include "ZoransPlayerController.h"
#include "ZoransPlayerState.h"
#include "PlayFabHelper/Subsystem/PlayfabHelperLocalUserSubsystem.h"
#include "ZoransResistance/ZoransResistance.h"
#include "ZoransResistance/Widgets/ZoransUserInterfaceWidget.h"
#include "ZoransResistance/Widgets/DamageWidgetComponent.h"


AZoransPlayerController::AZoransPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AZoransPlayerController::ClientTravelInternal_Implementation(const FString& URL, const ETravelType TravelType, const bool bSeamless, FGuid MapPackageGuid)
{
	UE_LOG(ZoransLog, Warning, TEXT("ClientTravelInternal"));
	
	FString OutputURL = URL;
	
	if (UPlayfabHelperLocalUserSubsystem* PlayfabSystem = GetWorld()->GetGameInstance()->GetSubsystem<UPlayfabHelperLocalUserSubsystem>())
	{
		UE_LOG(ZoransLog, Warning, TEXT("ClientTravelInternal -> PlayfabSystem Valid"));
		if (PlayfabSystem->GetIsPlayerAuthenticated())
		{
			OutputURL = OutputURL + FString("?InPlayfabId=") + *PlayfabSystem->GetPlayfabID();
		}
		else
		{
			UE_LOG(ZoransLog, Error, TEXT("ClientTravelInternal -> PlayfabSystem IS NOT VALID"));
		}
	}

	UWorld* World = GetWorld();

	// Warn the client.
	PreClientTravel(OutputURL, TravelType, bSeamless);

	if (bSeamless && TravelType == TRAVEL_Relative)
	{
		UE_LOG(ZoransLog, Warning, TEXT("ClientTravelInternal -> SEAMLESS TRAVEL !!! %s"), *OutputURL);
		World->SeamlessTravel(OutputURL);
	}
	else
	{
		if (bSeamless)
		{
			UE_LOG(LogPlayerController, Warning, TEXT("Unable to perform seamless travel because TravelType was %i, not TRAVEL_Relative"), int32(TravelType));
		}
		// Do the travel.

		UE_LOG(ZoransLog, Warning, TEXT("ClientTravelInternal -> DO THE TRAVEL !!! %s"), *OutputURL);
		GEngine->SetClientTravel(World, *OutputURL, TravelType);
	}
}

void AZoransPlayerController::CreateDamageNumberWidget_Implementation(AActor* DamagedActor, const FName HitBone, const float Amount, const EHitSeverity HitSeverity, const bool bOverrideHitType, const ETargetHitType HitType, const FVector HitLocationOverride)
{
	if (Amount <= 0.f || !DamagedActor || !DamageNumberWidgetClass)
	{
		return;
	}

	UDamageWidgetComponent* DamageNumberWidgetComponent = NewObject<UDamageWidgetComponent>(DamagedActor, DamageNumberWidgetClass);

	FVector HitLocation = FVector::ZeroVector;
	
	if (!HitLocationOverride.IsNearlyZero(0.01f))
	{
		HitLocation = HitLocationOverride;
	}
	else if (HitBone != NAME_None)
	{
		if (const ACharacter* HitChar = Cast<ACharacter>(DamagedActor))
		{
			if (const UPrimitiveComponent* HitComp = HitChar->GetMesh())
			{
				HitLocation = HitComp->GetSocketLocation(HitBone);
			}
		}
	}
	else
	{
		HitLocation = DamagedActor->GetActorLocation();
	}
		
	DamageNumberWidgetComponent->RegisterComponent();
	DamageNumberWidgetComponent->SetWorldLocation(HitLocation);
	DamageNumberWidgetComponent->DisplayDamageText(DamagedActor, Amount, bOverrideHitType ? ETargetHitType::Dynamic : HitType, HitSeverity);
	
	if (DamageDealtEvent.IsBound() && HitType != ETargetHitType::Dynamic)
	{
		DamageDealtEvent.Broadcast(HitSeverity);
	}
}

void AZoransPlayerController::CreateUserInterfaceWidget()
{
	if (!IsLocalController())
	{
		return;
	}
		
	UE_LOG(ZoransLog, Warning, TEXT("Zorans Player Controller: Attempting to CreateUserInterfaceWidget."));

	if (!IsValid(UserInterfaceWidget))
	{
		if (!UserInterfaceWidgetClass)
		{
			UE_LOG(ZoransLog, Warning, TEXT("Zorans Player Controller: No User Interface class specified. Please specify a User Interface class in the Player Controller settings."));

			return;
		}

		UE_LOG(ZoransLog, Warning, TEXT("Zorans Player Controller: Creating User Interface Widget."));
		UserInterfaceWidget = CreateWidget<UZoransUserInterfaceWidget>(this, UserInterfaceWidgetClass, TEXT("UserInterfaceWidget"));
		UserInterfaceWidget->AddToViewport();

		return;
	}
	
	if (IsValid(UserInterfaceWidget) && !UserInterfaceWidget->IsInViewport())
	{
		UE_LOG(ZoransLog, Error, TEXT("Zorans Player Controller: User Interface Widget ALREADY EXISTS."));

		UserInterfaceWidget->AddToViewport();
		UserInterfaceWidget->OnRefreshWidget();
	}
}

UZoransUserInterfaceWidget* AZoransPlayerController::GetUserInterfaceWidget() const
{
	return UserInterfaceWidget;
}

void AZoransPlayerController::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();

	if (GetNetMode() < NM_Client)
	{
		if (AZoransPlayerState* ZoransPlayerState = GetPlayerState<AZoransPlayerState>())
		{
			if (ZoransPlayerState->PlayfabId.IsEmpty())
			{
				const FString TravelURL = GetWorld()->URL.ToString();

				TArray<FString> OptionsArray;
	
				TravelURL.ParseIntoArray(OptionsArray, TEXT("?"), true);

				for (auto String : OptionsArray)
				{
					if (String.StartsWith(TEXT("InPlayfabId=")))
					{
						UE_LOG(ZoransLog, Warning, TEXT("ZoransPlayerController->ZoransPlayerState->InPlayfabId = %s"), *String.RightChop(12));
						ZoransPlayerState->PlayfabId = String.RightChop(12);
						ZoransPlayerState->OnPlayfabIdSet(ZoransPlayerState->PlayfabId);
					}
				}
			}
		}
	}

	if (OnSeamlessServerTravelFinished.IsBound())
	{
		OnSeamlessServerTravelFinished.Broadcast();
	}
}