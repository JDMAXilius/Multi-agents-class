// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZoransResistance/HMS/FHMSActorState.h"
#include "ZoransResistance/HMS/HMSSavedState.h"
#include "Net/UnrealNetwork.h"
#include "HMSSavedStateComponent.generated.h"


UCLASS(ClassGroup = (Custom), Blueprintable)
class ZORANSRESISTANCE_API UHMSSavedStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UHMSSavedStateComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;

	const FHMSActorState SaveState() const;

	UFUNCTION(BlueprintCallable,Category = "SavedState")
	void LoadActorState(FHMSActorSave State);

	UFUNCTION(BlueprintCallable, Category = "SavedState")
	void ResetOwner();

	UFUNCTION(BlueprintCallable, Category = "SavedState")
	void OwnerBeginPlayCall();

	UPROPERTY(Category = "Defaults", SaveGame, BlueprintReadOnly, EditAnywhere)
	TArray<FString> ComponentsToSerialize;

	UPROPERTY(Category = "Defaults", SaveGame, BlueprintReadOnly, EditAnywhere)
	TArray<FString> CustomStructuresToSerialize;

	UPROPERTY(Category = "ObjectReference", SaveGame, BlueprintReadOnly, VisibleAnywhere, Replicated)
	int64 Tag;

	UPROPERTY(Category = "Defaults", SaveGame, BlueprintReadWrite, EditAnywhere)
	bool Enabled;

	UPROPERTY(Category = "Defaults", BlueprintReadWrite, EditAnywhere)
	bool SerializeSelf;

};
