// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ZoransResistance/Characters/ZoransCharacterBase.h"
#include "ZoransPlayerController.generated.h"

class UZoransUserInterfaceWidget;
class UDamageWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FKillFeedEvent, AController*, KillingPlayer, AController*, KilledPlayer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDamageDealtEvent, EHitSeverity, HitSeverity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSeamlessServerTravelFinishedDelegate);

UCLASS()
class ZORANSRESISTANCE_API AZoransPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	AZoransPlayerController();

	virtual void ClientTravelInternal_Implementation(const FString& URL, ETravelType TravelType, bool bSeamless, FGuid MapPackageGuid) override;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "User Interface")
	TSubclassOf<UUserWidget> UserInterfaceWidgetClass = nullptr;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	void CreateUserInterfaceWidget();
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UZoransUserInterfaceWidget* GetUserInterfaceWidget() const;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "User Interface")
	TSubclassOf<UDamageWidgetComponent> DamageNumberWidgetClass = nullptr;
	
	UFUNCTION(BlueprintCallable, Client, Reliable)
	void CreateDamageNumberWidget(AActor* DamagedActor, const FName HitBone, float Amount, const EHitSeverity HitSeverity, const bool bOverrideHitType = false, const ETargetHitType HitType = ETargetHitType::None, FVector HitLocationOverride = FVector::ZeroVector);

	UPROPERTY(BlueprintAssignable)
	FDamageDealtEvent DamageDealtEvent;

	UPROPERTY(BlueprintAssignable)
	FOnSeamlessServerTravelFinishedDelegate OnSeamlessServerTravelFinished;
	
protected:

	UPROPERTY()
	UZoransUserInterfaceWidget* UserInterfaceWidget = nullptr;

	virtual void PostSeamlessTravel() override;
};