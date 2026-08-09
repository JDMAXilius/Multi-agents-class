// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "FX/Data/OSFXEventDataAsset.h"
#include "FX/Data/OSFXLibraryDataAsset.h"
#include "FX/Data/OSFXTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "OSFXSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSFXSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Transient)
	TObjectPtr<UOSFXLibraryDataAsset> Library;

	UFUNCTION(BlueprintCallable)
	void PlayFX(FGameplayTag EventTag, const FOSFXContext& ctx)
	{
		if (!Library) return;
		const UOSFXEventDataAsset* event = Library->FindEvent(EventTag);
		if (!event) return;

		for (const UOSFXEmitter* E : event->Emitters)
		{
			if (!E || !E->bEnabled) continue;
			if (!E->CheckTags(ctx.Tags)) continue;
			E->Emit(ctx);
		}
	}
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFX(const FOSFXContextLite& Lite);

	void Multicast_PlayFX_Implementation(const FOSFXContextLite& Lite)
	{
		FOSFXContext ctx = MakeContextFromLite(Lite);
		PlayFX(Lite.EventTag, ctx);
	}
};
