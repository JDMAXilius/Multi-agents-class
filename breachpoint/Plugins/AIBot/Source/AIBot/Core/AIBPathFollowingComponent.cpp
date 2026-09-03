#include "Core/AIBPathFollowingComponent.h"

#include "AIBotModule.h"
#include "Core/AIBBotController.h"
#include "Core/AIBNavArea_Jump.h"
#include "Core/AIBTags.h"
#include "Interfaces/AIBAvatarInterface.h"
#include "NavMesh/NavMeshPath.h"
#include "NavMesh/RecastNavMesh.h"

void UAIBPathFollowingComponent::Reset()
{
	Super::Reset();
	JumpedSegmentIndex = INDEX_NONE;
}

NavNodeRef UAIBPathFollowingComponent::SegmentStartPoly(int32 SegmentStartIndex) const
{
	const FNavMeshPath* NavMeshPath = Path.IsValid() ? Path->CastPath<FNavMeshPath>() : nullptr;
	if (!NavMeshPath)
	{
		return INVALID_NAVNODEREF;
	}
	if (NavMeshPath->IsStringPulled())
	{
		const TArray<FNavPathPoint>& Points = NavMeshPath->GetPathPoints();
		return Points.IsValidIndex(SegmentStartIndex) ? Points[SegmentStartIndex].NodeRef : INVALID_NAVNODEREF;
	}
	return NavMeshPath->PathCorridor.IsValidIndex(SegmentStartIndex)
		? NavMeshPath->PathCorridor[SegmentStartIndex] : INVALID_NAVNODEREF;
}

void UAIBPathFollowingComponent::SetMoveSegment(int32 SegmentStartIndex)
{
	Super::SetMoveSegment(SegmentStartIndex);

	if (JumpedSegmentIndex == SegmentStartIndex)
	{
		return;
	}
	const NavNodeRef Poly = SegmentStartPoly(SegmentStartIndex);
	const ARecastNavMesh* NavMesh = Path.IsValid() ? Cast<ARecastNavMesh>(Path->GetNavigationDataUsed()) : nullptr;
	if (Poly == INVALID_NAVNODEREF || !NavMesh)
	{
		return;
	}
	const UClass* Area = NavMesh->GetAreaClass(static_cast<int32>(NavMesh->GetPolyAreaID(Poly)));
	if (!Area || !Area->IsChildOf(UAIBNavArea_Jump::StaticClass()))
	{
		return;
	}
	if (PressJump(TEXT("jump"), GetCurrentTargetLocation()))
	{
		JumpedSegmentIndex = SegmentStartIndex;
	}
}

void UAIBPathFollowingComponent::StartUsingCustomLink(INavLinkCustomInterface* CustomNavLink, const FVector& DestPoint)
{
	if (CustomNavLink)
	{
		PressJump(TEXT("link"), DestPoint);
	}
	Super::StartUsingCustomLink(CustomNavLink, DestPoint);
}

bool UAIBPathFollowingComponent::PressJump(const TCHAR* Via, const FVector& To)
{
	AAIBBotController* Bot = Cast<AAIBBotController>(GetOwner());
	if (!Bot || !Bot->HasAuthority() || !Bot->GetPawn())
	{
		return false;
	}
	IAIBAvatarInterface* Avatar = Bot->GetAvatar();
	if (!Avatar || !Avatar->IsGrounded())
	{
		return false;
	}
	const FVector From = Bot->GetPawn()->GetActorLocation();
	Avatar->PressVerb(AIBTags::Verb_Jump);
	Avatar->ReleaseVerb(AIBTags::Verb_Jump);
	const UWorld* World = GetWorld();
	UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f link traverse — via %s from (%.0f,%.0f,%.0f) to (%.0f,%.0f,%.0f)"),
		*Bot->GetName(), World ? World->GetTimeSeconds() : 0.0, Via,
		From.X, From.Y, From.Z, To.X, To.Y, To.Z);
	return true;
}
