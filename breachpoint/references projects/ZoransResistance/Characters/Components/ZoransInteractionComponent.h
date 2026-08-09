// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZoransInteractionComponent.generated.h"

class UZoransWorldUserWidget;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), Blueprintable)
class ZORANSRESISTANCE_API UZoransInteractionComponent : public UActorComponent 
{
	GENERATED_BODY()

public:
	
	UZoransInteractionComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	void Interact();

	APawn* GetOwningPawn();

	APlayerController* GetOwningPlayerController();

	UFUNCTION(BlueprintCallable, Category = "Trace")
	void DisableInteractionComponent();

protected:
	
	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* InFocus);

	UFUNCTION()
	void OwningPawnDestroyed(AActor* Actor);

	void FindBestInteractable();
	
	UPROPERTY()
	AActor* FocusedActor;

	UPROPERTY(EditDefaultsOnly, Category = "Trace")
	float TraceDistance;

	UPROPERTY(EditDefaultsOnly, Category = "Trace")
	float TraceRadius;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly, Category = "UI")
	TSubclassOf<UZoransWorldUserWidget> DefaultWidgetClass;

	UPROPERTY(BlueprintReadOnly)
	UZoransWorldUserWidget* DefaultWidgetInstance;

	virtual void DestroyComponent(bool bPromoteChildren) override;

	TWeakObjectPtr<APawn> OwningPawn;
	TWeakObjectPtr<APlayerController> OwningPlayerController;
	
public:

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};