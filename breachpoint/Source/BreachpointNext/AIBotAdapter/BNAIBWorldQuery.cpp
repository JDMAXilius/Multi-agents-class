#include "AIBotAdapter/BNAIBWorldQuery.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIBotAdapter/BNAIBModeTags.h"
#include "Core/BNGameplayTags.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Match/BNHillPoint.h"
#include "Match/BNPlayerState.h"
#include "Match/BNTeams.h"

bool UBNAIBWorldQuery::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UBNAIBWorldQuery::RegisterHill(ABNHillPoint* Hill)
{
	if (Hill)
	{
		Hills.AddUnique(Hill);
	}
}

void UBNAIBWorldQuery::QueryPointsOfInterest(const AActor* Asker, float MaxDistance,
	TArray<FAIBPointOfInterest>& OutPoints) const
{
	for (const TWeakObjectPtr<ABNHillPoint>& HillPtr : Hills)
	{
		const ABNHillPoint* Hill = HillPtr.Get();
		if (!Hill)
		{
			continue;
		}
		FAIBPointOfInterest& Point = OutPoints.AddDefaulted_GetRef();
		Point.Location = Hill->GetActorLocation();
		Point.Kind = BNAIBTags::POI_Hill;
		Point.Worth = 1.f;
		// The hill's OWN radius is what "on the hill" means — the same number HillTick
		// scores with. Without it the mover invents its own, smaller, arrival test and
		// a bot banking points is told it has not arrived.
		Point.ReachRadiusUU = Hill->Radius;
		Point.Actor = const_cast<ABNHillPoint*>(Hill);
	}
}

void UBNAIBWorldQuery::QueryVisibleEnemies(const AActor* Asker, float Radius,
	TArray<AActor*>& OutEnemies) const
{
	// Empty by design — the header says why. Do not "improve" this with GetAllActorsOfClass.
	OutEnemies.Reset();
}

int32 UBNAIBWorldQuery::CountNearbyAllies(const AActor* Asker, float Radius) const
{
	// TEAMS (BN15): same-team living pawns within Radius, Asker excluded. The team datum
	// lives on the PlayerState (pawns do not carry the interface), and a NoTeam asker
	// counts ZERO — in FFA nobody has allies, which is the pre-teams answer, preserved.
	const APawn* AskerPawn = Cast<APawn>(Asker);
	const ABNPlayerState* AskerPS = AskerPawn ? AskerPawn->GetPlayerState<ABNPlayerState>() : nullptr;
	const FGenericTeamId AskerTeam = AskerPS ? AskerPS->GetGenericTeamId() : FGenericTeamId::NoTeam;
	UWorld* World = GetWorld();
	if (!AskerPawn || !World || AskerTeam == FGenericTeamId::NoTeam)
	{
		return 0;
	}

	const FVector From = AskerPawn->GetActorLocation();
	const double RadiusSq = static_cast<double>(Radius) * static_cast<double>(Radius);
	int32 Count = 0;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Candidate = *It;
		if (!Candidate || Candidate == AskerPawn)
		{
			continue;
		}
		if (FVector::DistSquared(From, Candidate->GetActorLocation()) > RadiusSq)
		{
			continue;
		}
		const ABNPlayerState* PS = Candidate->GetPlayerState<ABNPlayerState>();
		const FGenericTeamId Team = PS ? PS->GetGenericTeamId() : FGenericTeamId::NoTeam;
		if (!BNTeams::AreFriendly(AskerTeam, Team))
		{
			continue;
		}
		// The same alive door AreEnemies uses below — a corpse is nobody's ally either.
		const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate);
		if (!ASC || ASC->HasMatchingGameplayTag(BNTags::State_Dead))
		{
			continue;
		}
		++Count;
	}
	return Count;
}

bool UBNAIBWorldQuery::AreEnemies(const AActor* A, const AActor* B) const
{
	if (A == B || !A || !B)
	{
		return false;
	}
	if (!A->IsA<APawn>() || !B->IsA<APawn>())
	{
		return false;
	}
	// The adapter's audited no-cast door plus the one tag that means dead — the same
	// pair of lines UBNAIBAvatarAdapter::IsAliveTarget uses. A corpse is nobody's enemy.
	const UAbilitySystemComponent* ASCA = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(A));
	const UAbilitySystemComponent* ASCB = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(B));
	const bool bAliveA = ASCA && !ASCA->HasMatchingGameplayTag(BNTags::State_Dead);
	const bool bAliveB = ASCB && !ASCB->HasMatchingGameplayTag(BNTags::State_Dead);
	if (!bAliveA || !bAliveB)
	{
		return false;
	}
	// TEAMS (BN15): enemies = alive-both AND not friendly. Pawns do not carry the team
	// interface — the PlayerState is the one team datum — so each side resolves through
	// its ABNPlayerState; a null PlayerState reads NoTeam. AreFriendly's NoTeam guard
	// makes NoTeam-vs-anything NOT-friendly, so FFA (everyone NoTeam) is BYTE-IDENTICAL:
	// every living pair is still enemies.
	const APawn* PawnA = Cast<APawn>(A);
	const APawn* PawnB = Cast<APawn>(B);
	const ABNPlayerState* PSA = PawnA ? PawnA->GetPlayerState<ABNPlayerState>() : nullptr;
	const ABNPlayerState* PSB = PawnB ? PawnB->GetPlayerState<ABNPlayerState>() : nullptr;
	const FGenericTeamId TeamA = PSA ? PSA->GetGenericTeamId() : FGenericTeamId::NoTeam;
	const FGenericTeamId TeamB = PSB ? PSB->GetGenericTeamId() : FGenericTeamId::NoTeam;
	return !BNTeams::AreFriendly(TeamA, TeamB);
}

bool UBNAIBWorldQuery::AreAllies(const AActor* A, const AActor* B) const
{
	// The header says why there is no alive door here. Team resolution is the same
	// ladder AreEnemies uses: pawn → PlayerState → TeamId, null reads NoTeam, and
	// AreFriendly's NoTeam guard makes FFA answer false for every pair.
	if (!A || !B || !A->IsA<APawn>() || !B->IsA<APawn>())
	{
		return false;
	}
	const APawn* PawnA = Cast<APawn>(A);
	const APawn* PawnB = Cast<APawn>(B);
	const ABNPlayerState* PSA = PawnA ? PawnA->GetPlayerState<ABNPlayerState>() : nullptr;
	const ABNPlayerState* PSB = PawnB ? PawnB->GetPlayerState<ABNPlayerState>() : nullptr;
	const FGenericTeamId TeamA = PSA ? PSA->GetGenericTeamId() : FGenericTeamId::NoTeam;
	const FGenericTeamId TeamB = PSB ? PSB->GetGenericTeamId() : FGenericTeamId::NoTeam;
	return BNTeams::AreFriendly(TeamA, TeamB);
}
