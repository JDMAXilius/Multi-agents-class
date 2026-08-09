// Copyright Zollpa LLC


#include "ZoransControlPointBase.h"
#include "Components/SphereComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "ZoransResistance/Characters/ZoransCharacterBase.h"
#include "ZoransResistance/Characters/Components/ZoransHealthComponent.h"


AZoransControlPointBase::AZoransControlPointBase()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->Mobility = EComponentMobility::Movable;
	RootComponent = SceneRoot;
	
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionObjectType(ECC_Destructible);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CollisionShape = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionShape"));
	CollisionShape->SetupAttachment(SceneRoot);
}

void AZoransControlPointBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION(AZoransControlPointBase, ControlPointIndex, COND_None);
	DOREPLIFETIME_CONDITION(AZoransControlPointBase, OwningTeamIndex, COND_None);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransControlPointBase, ControlState, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransControlPointBase, CaptureProgress, COND_None, REPNOTIFY_Always);
}

void AZoransControlPointBase::SetOwningTeamIndex(const int32 InIndex)
{
	if (OwningTeamIndex != InIndex)
	{
		OwningTeamIndex = InIndex;

		OnRep_OwningTeamIndex();
	}
}

void AZoransControlPointBase::SetControlPointIndex(const int32 InIndex)
{
	if (ControlPointIndex != InIndex)
	{
		ControlPointIndex = InIndex;

		OnRep_ControlPointIndex();
	}
}

void AZoransControlPointBase::SetControlPointControlState(const EActorControlState NewState)
{
	if (NewState != ControlState)
	{
		ControlState = NewState;

		OnRep_ControlState();
	}
}

void AZoransControlPointBase::SetCaptureProgress(const float InProgress)
{
	CaptureProgress = InProgress;
	OnRep_CaptureProgress();
}

TArray<AZoransCharacterBase*> AZoransControlPointBase::GetOverlappedCharacters(const bool bCheckIfAlive) const
{
	TArray<AZoransCharacterBase*> OverlappingCharacterArray = {};
	
	if (IsValid(CollisionShape))
	{
		TArray<AActor*> OverlappingActorArray;
		
		CollisionShape->GetOverlappingActors(OverlappingActorArray, AZoransCharacterBase::StaticClass());

		for (AActor* Actor : OverlappingActorArray)
		{
			if (AZoransCharacterBase* ZoransCharacterBase = Cast<AZoransCharacterBase>(Actor))
			{
				if (bCheckIfAlive)
				{
					if (ZoransCharacterBase->GetHealthComponent()->IsAlive())
					{
						OverlappingCharacterArray.AddUnique(ZoransCharacterBase);
					}
				}
				else
				{
					OverlappingCharacterArray.AddUnique(ZoransCharacterBase);
				}
			}
		}
	}

	return OverlappingCharacterArray;
}

void AZoransControlPointBase::SetDominantTeam(const int32 InTeamIndex)
{
	DominantTeamIndex = InTeamIndex;
}

void AZoransControlPointBase::AdjustCaptureProgress(const bool bGaining)
{
	if (OwningTeamIndex < 0 && bGaining) return;
	
	const float MaxCapTime = UKismetMathLibrary::SafeDivide(CaptureTime, IntervalRate);
	const float CurrentProgress = CaptureProgress * MaxCapTime;
	const float UpdatedProgress = bGaining ? CurrentProgress + 1.f : CurrentProgress - 1.f;
	const float NormalizedProgress = UKismetMathLibrary::SafeDivide(UpdatedProgress, MaxCapTime);

	CaptureProgress = FMath::Clamp<float>(NormalizedProgress, 0.f, 1.f);
	OnRep_CaptureProgress();
}

void AZoransControlPointBase::StartOverlapTimer()
{
	if (GetWorld())
	{
		const FTimerDelegate CheckOverlapDelegate = FTimerDelegate::CreateUObject(this, &AZoransControlPointBase::OnCheckOverlap);
		
		GetWorld()->GetTimerManager().SetTimer(OverlapCheckTimer, CheckOverlapDelegate, IntervalRate, true, 0.0f);
	}
}

void AZoransControlPointBase::OnRep_ControlPointIndex()
{
	if (OnControlPointIndexChanged.IsBound())
	{
		OnControlPointIndexChanged.Broadcast(GetControlPointIndex());
	}
}

void AZoransControlPointBase::OnRep_OwningTeamIndex()
{
	if (OnOwningIndexTeamChanged.IsBound())
	{
		OnOwningIndexTeamChanged.Broadcast(GetOwningTeamIndex());
	}
}

void AZoransControlPointBase::OnRep_ControlState()
{
	if (OnControlPointControlStateChanged.IsBound())
	{
		OnControlPointControlStateChanged.Broadcast(GetControlPointControlState());
	}
}

void AZoransControlPointBase::OnRep_CaptureProgress()
{
	if (OnCaptureProgressChanged.IsBound())
	{
		OnCaptureProgressChanged.Broadcast(GetCaptureProgress());
	}
}