#include "Animations/Notifies/OSAnimNotify_NiagaraBurst.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

namespace
{
	struct FOSNiagaraRateLimitKey
	{
		TWeakObjectPtr<AActor> Owner;
		TWeakObjectPtr<UNiagaraSystem> System;

		bool operator==(const FOSNiagaraRateLimitKey& Other) const
		{
			return Owner == Other.Owner && System == Other.System;
		}
	};

	uint32 GetTypeHash(const FOSNiagaraRateLimitKey& Key)
	{
		return HashCombine(GetTypeHash(Key.Owner), GetTypeHash(Key.System));
	}

	static bool GetNearestLocalViewpoint_Niagara(UWorld* World, const FVector& Location, FVector& OutViewLocation)
	{
		if (!IsValid(World))
		{
			return false;
		}

		bool bFound = false;
		float BestDistSq = TNumericLimits<float>::Max();

		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* PC = It->Get();
			if (!IsValid(PC) || !PC->IsLocalController())
			{
				continue;
			}

			FVector ViewLoc;
			FRotator ViewRot;
			PC->GetPlayerViewPoint(ViewLoc, ViewRot);

			const float DistSq = FVector::DistSquared(ViewLoc, Location);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				OutViewLocation = ViewLoc;
				bFound = true;
			}
		}

		return bFound;
	}
}

void UOSAnimNotify_NiagaraBurst::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp) || !System)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	UWorld* World = Owner->GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (bSkipOnDedicatedServer && World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (bLocalOnly)
	{
		APawn* PawnOwner = Cast<APawn>(Owner);
		if (!IsValid(PawnOwner) || !PawnOwner->IsLocallyControlled())
		{
			return;
		}
	}

	FVector BaseLoc = Owner->GetActorLocation();
	FRotator BaseRot = Owner->GetActorRotation();
	FName AttachSocket = NAME_None;

	if (SocketName != NAME_None && MeshComp->DoesSocketExist(SocketName))
	{
		BaseLoc = MeshComp->GetSocketLocation(SocketName);
		BaseRot = MeshComp->GetSocketRotation(SocketName);
		AttachSocket = SocketName;
	}

	const FVector WorldLocForCull = BaseLoc + LocationOffset;
	const FRotator WorldRotForWorldSpawn = BaseRot + RotationOffset;

	if (MaxDistance > 0.f)
	{
		FVector ViewLoc;
		if (GetNearestLocalViewpoint_Niagara(World, WorldLocForCull, ViewLoc))
		{
			if (FVector::DistSquared(ViewLoc, WorldLocForCull) > FMath::Square(MaxDistance))
			{
				return;
			}
		}
	}

	if (MinIntervalPerOwner > 0.f)
	{
		static TMap<FOSNiagaraRateLimitKey, float> LastTimeByOwner;

		const float Now = World->GetTimeSeconds();
		const FOSNiagaraRateLimitKey Key{ Owner, System.Get() };

		if (const float* Last = LastTimeByOwner.Find(Key))
		{
			if ((Now - *Last) < MinIntervalPerOwner)
			{
				return;
			}
		}
		LastTimeByOwner.Add(Key, Now);

		if (LastTimeByOwner.Num() > 512)
		{
			for (auto It = LastTimeByOwner.CreateIterator(); It; ++It)
			{
				if (!It.Key().Owner.IsValid() || !It.Key().System.IsValid())
				{
					It.RemoveCurrent();
				}
			}
		}
	}

	const bool bAutoDestroy = true;
	const bool bAutoActivate = true;

	if (bAttachToMesh)
	{
		UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			System,
			MeshComp,
			AttachSocket,
			LocationOffset,
			RotationOffset,
			EAttachLocation::KeepRelativeOffset,
			bAutoDestroy,
			bAutoActivate);
		if (Comp)
		{
			Comp->SetWorldScale3D(FVector(Scale));
		}
	}
	else
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			System,
			WorldLocForCull,
			WorldRotForWorldSpawn,
			FVector(Scale),
			bAutoDestroy,
			bAutoActivate);
	}
}

