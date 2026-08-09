// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "ZoransCharacterBase.h"
#include "ZoransAICharacter.generated.h"

UCLASS(Blueprintable)
class ZORANSRESISTANCE_API AZoransAICharacter : public AZoransCharacterBase
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AZoransAICharacter(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
