// Fill out your copyright notice in the Description page of Project Settings.


#include "FX/Emitters/OSFXEmitter_Wwise.h"

#include "AkComponent.h"
#include "FX/Data/OSWwisePresetAsset.h"
#include "FX/ParamSet/OSWwisePreset.h"

static AActor* ResolveOwnerActor(const FOSFXContext& Ctx, bool bOnTarget)
{
	AActor* A = bOnTarget ? Ctx.Target.Get() : Ctx.Instigator.Get();
	if (!A) A = Ctx.Instigator.Get();
	return A;
}

bool UOSFXEmitter_Wwise::IsActorLocallyControlled(const AActor* Actor)
{
	if (!Actor) return false;

	const APawn* Pawn = Cast<APawn>(Actor);
	if (!Pawn) Pawn = Actor->GetInstigator();

	if (const AController* C = Pawn ? Pawn->GetController() : nullptr)
	{
		if (const APlayerController* PC = Cast<APlayerController>(C))
		{
			return PC->IsLocalController();
		}
	}
	return false;
}

UAkComponent* UOSFXEmitter_Wwise::FindAkComponent(AActor* Actor)
{
	if (!Actor) return nullptr;
	return Actor->FindComponentByClass<UAkComponent>();
}

void UOSFXEmitter_Wwise::ApplyParams(UAkComponent* AkComp, const FOSWwiseParamPack& Pack)
{
	if (!AkComp) return;

	for (const auto& It : Pack.RTPC)
	{
		if (!It.Key) continue;
		AkComp->SetRTPCValue(It.Key, It.Value, 0, FString());
	}

	for (const auto& It : Pack.Switches)
	{
		AkComp->SetSwitch(It);
	}
	
	for (UAkStateValue* It : Pack.States)
	{
		if (!It) continue;
		//AkComp->SetState(It);
	}
}

const UOSWwisePreset* UOSFXEmitter_Wwise::ResolveDefaultPreset() const
{
	if (DefaultPresetAsset && DefaultPresetAsset->Preset)
	{
		return DefaultPresetAsset->Preset;
	}
	return DefaultPreset;
}

void UOSFXEmitter_Wwise::Emit_Implementation( const FOSFXContext& Ctx ) const
{
	Super::Emit_Implementation(Ctx);
	
	if (const UWorld* W = Ctx.Instigator.IsValid() ? Ctx.Instigator->GetWorld() : nullptr)
	{
		if (W->GetNetMode() == NM_DedicatedServer) return;
	}

	AActor* OwnerActor = ResolveOwnerActor(Ctx, bOnTarget);
	if (!OwnerActor) return;
	
	UAkComponent* AkComp = FindAkComponent(OwnerActor);
	if (!AkComp) return;
	
	const bool bLocal = IsActorLocallyControlled(OwnerActor);

	
	FOSWwiseParamPack PackToUse;
	if (Ctx.AudioParam.IsValid())
		PackToUse.MergeFrom(Ctx.AudioParam);
	else
	{
		auto preset = ResolveDefaultPreset();
		if (preset && preset->BasePack.IsValid()) PackToUse.MergeFrom(preset->BasePack);
	}
	
	if (!PackToUse.IsValid()) return;
	ApplyParams(AkComp, PackToUse);
	
	UAkAudioEvent* EventToPlay = nullptr;
	if (PackToUse.EventOverride) EventToPlay = PackToUse.EventOverride;
	else if (PackToUse.HasPair())
	{
		const FOSWwiseEventPair& PairToUse =
		PackToUse.EventPair;

		EventToPlay = PairToUse.Resolve(bLocal);
	}
	if (EventToPlay)
		AkComp->PostAkEvent(EventToPlay);
}
