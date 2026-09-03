#include "Execution/AIBStateTreeTasks.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/PawnMovementComponent.h"

#include "AIBotModule.h"
#include "Brain/AIBAmbitionEngine.h"
#include "Brain/AIBTactic.h"
#include "Core/AIBBotController.h"
#include "Core/AIBTags.h"
#include "Data/AIBDataRows.h"
#include "Interfaces/AIBAvatarInterface.h"
#include "Interfaces/AIBWorldQuery.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Perception/AIBSensorium.h"
#include "Skills/AIBAimPolicy.h"
#include "Skills/AIBGrenadePolicy.h"
#include "Skills/AIBMeleePolicy.h"
#include "Skills/AIBWeaponPolicy.h"
#include "Skills/AIBMovementPolicy.h"
#include "Skills/AIBTraversalPolicy.h"
#include "StateTreeExecutionContext.h"
#include "Team/AIBTeamCoordinator.h"

namespace
{
	/** The controller every node runs on: the bound instance data when the editor set one,
	 *  else the execution context's OWNER — the AIController the StateTreeAIComponent lives
	 *  on. The fallback is what lets a CODE-authored tree run at all: property bindings are
	 *  editor-only graph data (the host's proven pattern, same reasoning verbatim). */
	AAIBBotController* ResolveBot(const FStateTreeExecutionContext& Context, AAIController* BoundController)
	{
		if (AAIBBotController* Bound = Cast<AAIBBotController>(BoundController))
		{
			return Bound;
		}
		return Cast<AAIBBotController>(Context.GetOwner());
	}

	/** Turns the CONTROL rotation toward a point at a bounded rate, and the body with it.
	 *  Exists because the controller has no tick, so the engine's focus-driven turn never
	 *  runs — the host walked into both halves of that bug (bullets into walls, bodies
	 *  sliding sideways) before writing this shape. */
	void SteerControlRotation(AAIBBotController& Controller, const FVector& WorldPoint, float DegreesPerSecond, float DeltaTime)
	{
		APawn* Pawn = Controller.GetPawn();
		if (!Pawn)
		{
			return;
		}
		// CLAIM THE YAW. Every aimer goes through here, so this one line is what tells the
		// movers to stop facing their travel — see AAIBBotController::NoteYawClaimed.
		if (const UWorld* World = Controller.GetWorld())
		{
			Controller.NoteYawClaimed(World->GetTimeSeconds());
		}
		const FVector ToPoint = WorldPoint - Pawn->GetPawnViewLocation();
		if (ToPoint.IsNearlyZero())
		{
			return;
		}
		const FRotator Desired = ToPoint.Rotation();
		// NO SNAP PATH (F4): zero-or-negative is an authoring accident, not a request
		// for instant aim — it falls back to the default rate. F1 has one clamp site
		// for time; this is the one bound site for turn.
		const float Rate = DegreesPerSecond > 0.f ? DegreesPerSecond : 360.f;
		const FRotator Stepped = FMath::RInterpConstantTo(Controller.GetControlRotation(), Desired, DeltaTime, Rate);
		Controller.SetControlRotation(Stepped);
		Pawn->FaceRotation(Stepped, DeltaTime);
	}

	/** True when Point sits close enough to walkable ground to stand on, with the standing
	 *  spot written to OutPoint. The extent is vertically generous on purpose: a goal is far
	 *  more often slightly ABOVE or BELOW a floor than beside one, which is the ordinary case
	 *  on a multi-level arena. Callers that have a better answer than a refused path check
	 *  the bool; callers that do not just let the mover refuse. */
	bool ProjectToNav(UWorld* World, const FVector& Point, FVector& OutPoint)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		FNavLocation Projected;
		if (NavSys && NavSys->ProjectPointToNavigation(Point, Projected, FVector(300.f, 300.f, 400.f)))
		{
			OutPoint = Projected.Location;
			return true;
		}
		return false;
	}

	/** THE FEET (AIB22 F5-3): capsule bottom = actor origin − simple-collision half-height.
	 *  Every storey / off-mesh Z read uses this, never the pawn centre — the centre sits a
	 *  half-height (88uu on this host) above the navmesh, so a goal at nav z=10 under a
	 *  body at z=98 read as a storey (|up| 88 > 45: 5,908 false storeys). Computed from
	 *  the pawn's collision because the avatar door has no feet accessor and its adapter
	 *  lives outside this plugin. */
	FVector FeetOf(const APawn& Pawn)
	{
		return Pawn.GetActorLocation() - FVector(0.f, 0.f, Pawn.GetSimpleCollisionHalfHeight());
	}

	/** `t=` on every AIB22 metric line: world seconds, 0 with no world. */
	double WorldSeconds(const AAIBBotController& Bot)
	{
		const UWorld* World = Bot.GetWorld();
		return World ? World->GetTimeSeconds() : 0.0;
	}

	/** WHY a move was refused, in the three facts that separate causes a bare "refused"
	 *  cannot. Off-mesh and unreachable are different defects with different fixes, and a
	 *  log that says only "refused" sends the reader guessing — which is how a half-fix
	 *  gets shipped and measured as a win.
	 *
	 *    self=NO   the BOT is off the mesh. Nothing it asks for will ever path; this is a
	 *              spawn or placement bug and the goal is innocent.
	 *    goal=NO   an off-mesh goal — the class already fixed once. Seeing it again means a
	 *              site was missed, not that the fix failed.
	 *    both yes  genuine unreachability: separate islands, or a navlink that never
	 *              generated. Distance says which geometry to go and look at.
	 *
	 *  Built only on the failure path, so its cost never touches a bot that is moving. */
	FString DescribeMoveFailure(const AAIBBotController& Bot, const FVector& Goal)
	{
		const APawn* Pawn = Bot.GetPawn();
		const FVector Self = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
		FVector Ignored;
		const bool bSelfOnNav = Pawn && ProjectToNav(Bot.GetWorld(), Self, Ignored);
		const bool bGoalOnNav = ProjectToNav(Bot.GetWorld(), Goal, Ignored);

		// AIB9 step 2/3, the WHERE and the MOMENT — appended only for self=NO, because
		// that is the case whose causes the ticket must separate (fresh spawn, mid-fall,
		// post-knockback, steady state) and the only one where the bot's own position is
		// the evidence. Exact format is load-bearing: the metrics harness transcribes it.
		FString OffMeshMoment;
		if (Pawn && !bSelfOnNav)
		{
			const UWorld* World = Bot.GetWorld();
			const double Now = World ? World->GetTimeSeconds() : 0.0;
			const UPawnMovementComponent* MoveComp = Pawn->GetMovementComponent();
			const double LastHitAt = Bot.GetLastDamageTakenAtSeconds();
			OffMeshMoment = FString::Printf(
				TEXT(" | off-mesh self at (%.0f, %.0f, %.0f) age=%.1fs falling=%s velZ=%.0f lastHit=%s"),
				Self.X, Self.Y, Self.Z,
				Now - Bot.GetPossessedAtSeconds(),
				(MoveComp && MoveComp->IsFalling()) ? TEXT("yes") : TEXT("no"),
				Pawn->GetVelocity().Z,
				LastHitAt > 0.0 ? *FString::Printf(TEXT("%.1fs"), Now - LastHitAt) : TEXT("never"));
		}

		// HEIGHTS, not just distance. "Both on the mesh and still refused" has two very
		// different shapes: two points on ONE floor with a wall between them, and two
		// points on DIFFERENT floors with no way up. Distance alone cannot tell them
		// apart, and the fix for each is nothing like the fix for the other.
		return FString::Printf(TEXT("self=%s goal=%s dist=%.0fuu selfZ=%.0f goalZ=%.0f dz=%.0f%s"),
			bSelfOnNav ? TEXT("yes") : TEXT("NO"),
			bGoalOnNav ? TEXT("yes") : TEXT("NO"),
			Pawn ? FVector::Dist(Self, Goal) : -1.f,
			Self.Z, Goal.Z, Goal.Z - Self.Z,
			*OffMeshMoment);
	}

	/** EVERY goal this file hands the mover is a REMEMBERED or DERIVED world point — a
	 *  sighting recalled from memory, a belief, or plain arithmetic off the pawn's own
	 *  location. None of those is guaranteed to be somewhere a body can stand, and
	 *  MoveToLocation REFUSES an off-navmesh goal outright. Unprojected, the flee goal
	 *  (pawn + away * distance) refused 33,095 times in one four-minute match against 124
	 *  Retreat ambitions: the brain decided correctly and the body never moved.
	 *
	 *  So projection happens HERE, at the single door to the mover, rather than at each
	 *  call site — four of the five sites pass a remembered point, and fixing only the two
	 *  that log loudly would leave the other two refusing in silence.
	 *
	 *  A FAILED projection is deliberately NOT swallowed: the original point goes through
	 *  and the mover refuses it, so a genuinely unreachable goal still fails loudly (F7)
	 *  instead of being quietly redirected somewhere the caller never asked for.
	 *
	 *  The extent is the vertical-generous box the sibling framework settled on: wide
	 *  enough to catch a goal beside a walkable surface, tall enough to catch one above or
	 *  below a floor, which is the common case on a multi-level arena. */
	EPathFollowingRequestResult::Type MoveToNavPoint(AAIBBotController& Bot, const FVector& Goal, float AcceptanceUU)
	{
		FVector Target;
		if (!ProjectToNav(Bot.GetWorld(), Goal, Target))
		{
			Target = Goal;
		}
		const EPathFollowingRequestResult::Type Result = Bot.MoveToLocation(Target, AcceptanceUU);
		if (Result == EPathFollowingRequestResult::Failed)
		{
			// AIB22 `no_path_requests`: ONE site, because every mover's goal comes through
			// this door. Callers keep their own F7 lines; this one is the harness's.
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f move REFUSED goal=(%.0f,%.0f,%.0f) %s"),
				*Bot.GetName(), WorldSeconds(Bot), Goal.X, Goal.Y, Goal.Z, *DescribeMoveFailure(Bot, Goal));
		}
		return Result;
	}

	bool IsWithin(const AAIController& Controller, const FVector& Point, float RadiusUU)
	{
		const APawn* Pawn = Controller.GetPawn();
		return Pawn && FVector::Dist(Pawn->GetActorLocation(), Point) <= RadiusUU;
	}

	/** PHASE 12 — THE APPROACH SPREAD (AIB23 W-AUDIT, adopted deviation): the close-in goal
	 *  is a ring sample around the belief at the claims book's bearing — the first holder's
	 *  seeded phase + Ordinal·π, so two attackers land on opposite sides by construction; a
	 *  non-holder takes its OWN seeded phase + 90° (W-REVIEW M5/L3: LifeSeed-phased, never
	 *  a UniqueID, and denied bots never stack on one slot). Samples base/±40°/±80° through
	 *  the existing projection only — no TestPathSync per sample; nothing projects -> the
	 *  belief itself, as before. The radius is the caller's (fight range minus acceptance,
	 *  so arrival is always inside the range the strafe owns — a ring AT the fight range
	 *  plus the 350 acceptance would park a bot 1250uu out with tactic=none). */
	FVector RingApproachGoal(AAIBBotController& Bot, const FVector& Belief, float RadiusUU)
	{
		const AActor* Target = Bot.GetSensorium().GetVisibleTarget();
		if (!Target || RadiusUU <= 0.f)
		{
			return Belief;
		}
		const UAIBTeamCoordinator* Team = Bot.GetWorld() ? Bot.GetWorld()->GetSubsystem<UAIBTeamCoordinator>() : nullptr;
		const float AngleDeg = Team ? Team->GetTargetRingAngleDeg(Bot, *Target) : Bot.GetRingPhaseDeg() + 90.f;
		static const float Offsets[] = { 0.f, 40.f, -40.f, 80.f, -80.f };
		for (const float Offset : Offsets)
		{
			const FVector Sample = Belief + FVector(RadiusUU, 0.f, 0.f).RotateAngleAxis(AngleDeg + Offset, FVector::UpVector);
			FVector OnNav;
			if (ProjectToNav(Bot.GetWorld(), Sample, OnNav))
			{
				return OnNav;
			}
		}
		return Belief;
	}

	/** THE DISTANCE A TASK MAY ACT ON — this frame's, not the think's.
	 *
	 *  FAIBFacts::DistToTargetUU is rebuilt every 0.1s, which is right for WANTING (a want
	 *  that flickers at frame rate is not a want) and wrong for a LUNGE: at sprint speed a
	 *  bot covers ~90uu inside one stale think, and a melee reach is ~120. So the executor
	 *  side — which is allowed a world, unlike Brain/ — measures it now.
	 *
	 *  It is still the BELIEF, never the live actor: same F2-B rule the aim obeys, so a bot
	 *  cannot stab or grenade a position it has not honestly seen. */
	bool LiveDistanceToBelief(const AAIBBotController& Bot, float& OutDistanceUU)
	{
		const APawn* Pawn = Bot.GetPawn();
		if (!Pawn || !Bot.GetSensorium().HasVisibleTarget())
		{
			return false;
		}
		OutDistanceUU = FVector::Dist(Pawn->GetActorLocation(), Bot.GetSensorium().GetLastSeenLocation());
		return true;
	}

	/** SPRINT is released inside this multiple of a mover's arrival radius. The host's own
	 *  rule, transcribed with its reasoning: a bot that sprints into its firing position
	 *  arrives unable to shoot, because the sprint state holds for as long as the key is
	 *  down. Un-sprinted bots are also a competitive fact, not polish — sprint multiplies
	 *  move speed, so a sprinting human simply outran every bot, always. */
	constexpr float SprintBeyondRadiusFactor = 1.5f;

	/** The lip search's fan (Egress, and F5-2's vertical-gap step-off). Fixed headings,
	 *  not random ones: the same feet must find the same lip, because a re-entry
	 *  re-derives the pick instead of remembering it. The reach only has to exceed the
	 *  longest platform on either map (~2000uu). */
	constexpr int32 IslandLipRays = 16;
	constexpr float IslandLipReachUU = 4000.f;

	/** The wedge watchdog: less ground than this gained for this long, with a goal still
	 *  ahead, and the bot spends ONE jump — a lip, a crate, a step is exactly the shape a
	 *  jump clears, and it costs nothing to find out. */
	constexpr float WedgeProgressUU = 50.f;
	constexpr float WedgeStallSeconds = 1.5f;
	/** AIB22: a stall EPISODE opens this long after progress stopped — a third of the
	 *  watchdog's own patience, so the metric sees the wedge before the jump answers it. */
	constexpr float StallReportSeconds = 0.5f;

	/** AIB19: the traverse aim's turn rate. Faster than combat facing on purpose — the
	 *  anchor is scenery that cannot dodge, and a slow pan here is seconds spent as a
	 *  stationary target at a known doorstep. Still through the ONE bounded steer (F4). */
	constexpr float TraverseAimTurnRate = 540.f;

	/** RELOAD below a quarter magazine, re-tapped no faster than this. The magazine only
	 *  refills on the weapon's own notify, so a refused reload (frozen, dead, no montage)
	 *  would otherwise be re-pressed at tick rate forever. */
	constexpr float ReloadAtMagazineFraction = 0.25f;
	constexpr float ReloadRetrySeconds = 1.0f;
	/** How long a bot may hold the reload crouch before it gives up on the magazine. Longer
	 *  than any real reload in the table (2.2s is the slowest) with room for the retry
	 *  cadence, so a working reload is never interrupted by this. */
	constexpr float ReloadGiveUpSeconds = 6.0f;

	/** MELEE commits inside this fraction of the HELD WEAPON's own reach — the host's rule,
	 *  and below 1 for the host's reason: a swing at the exact edge of the reach turns one
	 *  backward step into a whiff. The reach is never restated here; it comes through the
	 *  door, so retuning the weapon retunes the bot. */
	constexpr float MeleeCommitFraction = 0.8f;
	constexpr float MeleeRetrySeconds = 1.5f;

	/** THE MUZZLE GATE (W-REVIEW P4+5 H4). A burst may only BEGIN with the control
	 *  rotation already near the aim line — a fresh acquisition behind the bot otherwise
	 *  held the trigger through a 175-degree swing, hosing the arena for the half second
	 *  the turn takes. A burst already running BREAKS on a large swing (a mid-burst
	 *  target switch), on the same reasoning. Start tight, break loose: the gap between
	 *  the two is what keeps ordinary tracking from stuttering the trigger. */
	constexpr float BurstStartAlignDegrees = 10.f;
	constexpr float BurstBreakAlignDegrees = 30.f;

	// The GRENADE band lived here as honest defaults until Phase 4's integration: the
	// judgement is now FAIBGrenadePolicy's per-level recognition ladder (its own bands,
	// anchored inside the sight envelope). Only the throttle below stays task-side.

	/** THE COOLDOWN IS NOT OPTIONAL, and it is per BOT for a reason the host learned in
	 *  the world rather than on paper: eight bots that each throw the instant a band opens
	 *  throw eight grenades in one second, and the arena stops being a fight. Longer than
	 *  any host ability cooldown we could ask about, so the press is never a dead one — the
	 *  module must not know the host's cooldown tag, and this makes not knowing it safe. */
	constexpr float GrenadeCooldownSeconds = 8.f;

	/** WEAPON SWAP. The verb is a CYCLE, not a selection: press until the avatar says the
	 *  hand is right, exactly as a human rolls the mouse wheel past a slot they do not want
	 *  — which is also how a bot walks past an empty holster slot without anything in the
	 *  host's equipment needing to change for it. The pause is what lets the equip actually
	 *  take (pressing again next frame cancels it), and the cap is what stops a bot whose
	 *  whole loadout is dry from cycling for the rest of the match. Five presses is one full
	 *  lap of the audited host's five-slot carry: enough to reach any slot, once. */
	constexpr float SwapSeconds = 0.6f;
	constexpr int32 MaxSwapPresses = 5;

	/** The host's name for a route, or "?" when the host names none. AIB19's open finding
	 *  is that ~half of grapple attempts fail to reach the standoff and NOBODY CAN SAY
	 *  WHICH ROUTES — every line named only the bot, so the remedy (a manifest fix for a
	 *  bad authored anchor vs a generator retune for a physics shortfall) had nothing to
	 *  branch on. Every traverse line carries this now. */
	FString RouteLabel(const FName RouteId)
	{
		return RouteId.IsNone() ? FString(TEXT("?")) : RouteId.ToString();
	}

	/** Sprint is a HOLD: press the rising edge, release the falling one, re-press nothing.
	 *  A leaked hold rides the persistent ASC into the next life (the host's own leak). */
	void SetSprint(IAIBAvatarInterface& Avatar, bool& bHeld, bool bWant)
	{
		if (bWant == bHeld)
		{
			return;
		}
		bHeld = bWant;
		if (bWant)
		{
			Avatar.PressVerb(AIBTags::Verb_Sprint);
		}
		else
		{
			Avatar.ReleaseVerb(AIBTags::Verb_Sprint);
		}
	}

	/** Crouch is a TOGGLE — one tap flips it — so this compares against the avatar's REAL
	 *  state through the door, never a private mirror, and never asks for a crouch while
	 *  falling: mid-air the toggle only ever UNcrouches, so the press would do the exact
	 *  opposite of what the caller wanted. Both rules are the host's, both were bugs first. */
	void SetCrouch(IAIBAvatarInterface& Avatar, bool bWant)
	{
		if (bWant && !Avatar.IsGrounded())
		{
			return;
		}
		if (Avatar.IsCrouched() == bWant)
		{
			return;
		}
		Avatar.PressVerb(AIBTags::Verb_Crouch);
		Avatar.ReleaseVerb(AIBTags::Verb_Crouch);
	}

	/** AIB22 `stuck_seconds`, closing half. jumped= (fix #4 R9) reads whether the path
	 *  follower pressed a LINK jump since this stall's clock restarted — the only jump
	 *  there is; the watchdog itself never hops, and the old flag that printed "yes" at
	 *  the 1.5s diagnosis read as one. resolved= says whether the body got moving or the
	 *  mover let go first. Bookkeeping only — the clocks are untouched. */
	void EndStall(AAIBBotController& Bot, FAIBLocomotionState& State, const TCHAR* Resolved)
	{
		State.bStallOpen = false;
		UE_LOG(LogAIBot, Log,
			TEXT("AIBot: %s t=%.1f stall over — %.1fs at (%.0f,%.0f,%.0f) goal=(%.0f,%.0f,%.0f) jumped=%s resolved=%s"),
			*Bot.GetName(), WorldSeconds(Bot), State.StallSeconds - State.StallReportedSeconds,
			State.BestPoint.X, State.BestPoint.Y, State.BestPoint.Z,
			State.Goal.X, State.Goal.Y, State.Goal.Z,
			Bot.GetLastLinkJumpAtSeconds() >= State.StallStartedAtSeconds ? TEXT("yes") : TEXT("no"), Resolved);
		State.StallReportedSeconds = State.StallSeconds;
	}

	/** AIB22 `island_egress_count`: ONE format for every way off an island. Today only
	 *  the AIB19 grapple-route drop calls it (via=grapple); the drop/link/jump callers and
	 *  the island fact's real stranded clock land with the Egress tactic (step 4). */
	void LogIslandEgress(const AAIBBotController& Bot, const TCHAR* Via, const FVector& From, float StrandedSeconds)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f island egress — via %s from (%.0f,%.0f,%.0f) after %.1fs stranded"),
			*Bot.GetName(), WorldSeconds(Bot), Via, From.X, From.Y, From.Z, StrandedSeconds);
	}

	/** FACE THE WALK (founder, 1 Sep: bots "walking and running in reverse instead of
	 *  like a human rotating themselves"). This host is an FPS pawn whose body yaw IS
	 *  the control rotation, and until now NOTHING in a mover wrote it — so a bot
	 *  crossing the map kept the heading of whatever it last aimed at and moonwalked
	 *  the whole way. The sibling framework has faced its walk since R9; this module
	 *  was written without it.
	 *
	 *  Suppressed while an aimer holds the yaw, which is the founder's own exception:
	 *  "that doesn't mean that it cannot be doing evasive actions in backwards,
	 *  especially if it is in combat mode". A bot with a target to face keeps facing
	 *  it and strafes and backpedals exactly as before — the claim, not a guess about
	 *  which branch is running, is what decides. Phase 13 lifted it out of
	 *  TickLocomotion so the hill hold's footwork (no mover, no stall clock) faces its
	 *  legs too. */
	void FaceTravel(AAIBBotController& Bot, APawn& Pawn, const FVector& Goal, float DeltaTime)
	{
		if (Bot.IsYawClaimed(WorldSeconds(Bot)))
		{
			return;
		}
		const FVector Here = Pawn.GetActorLocation();
		// Velocity when there is real motion, the GOAL when there is not: a bot that
		// has stopped, or is about to set off, should turn toward where it is going
		// BEFORE it starts, rather than leaving sideways and correcting.
		FVector Travel = Pawn.GetVelocity();
		Travel.Z = 0.f;
		if (Travel.Size() < AIB::TravelFacingMinSpeedUU)
		{
			Travel = Goal - Here;
			Travel.Z = 0.f;
		}
		if (Travel.IsNearlyZero())
		{
			return;
		}
		// Yaw only, and LEVEL: a walking body does not pitch. Steering at a point
		// would tilt the head at the floor on a downhill and at the sky on a ramp,
		// which is also where the aim would start from if a target appeared.
		//
		// Plus the SWEEP'S PAN (AIB22, F9): a searching bot looks about as it walks.
		// The offset is SweepLook's, applied HERE rather than written by it, because
		// two writers on one yaw fight per tick — this block would have dragged any
		// separately-written pan back to the path at 420 deg/s. Zero for every mover
		// that is not walking beside a sweep.
		const FRotator Current = Bot.GetControlRotation();
		const FRotator Desired(0.f, Travel.Rotation().Yaw + Bot.GetTravelPanDegrees(), 0.f);
		const FRotator Stepped = FMath::RInterpConstantTo(
			FRotator(0.f, Current.Yaw, 0.f), Desired, DeltaTime,
			AIB::TravelFacingTurnRateDeg);
		const FRotator Applied(Current.Pitch, Stepped.Yaw, Current.Roll);
		Bot.SetControlRotation(Applied);
		Pawn.FaceRotation(Applied, DeltaTime);
	}

	/** Phase 13: the teammate yield's end — window lapsed or the body moved. */
	void EndYield(AAIBBotController& Bot, FAIBLocomotionState& State)
	{
		if (State.YieldUntilSeconds > 0.0)
		{
			State.YieldUntilSeconds = 0.0;
			Bot.SetStillTactic(EAIBStillTactic::Yield, false);
		}
	}

	/** One call per mover tick: hold sprint while there is ground to cover, face the walk,
	 *  and keep the stall clocks. NAVLINKS AND JUMPS ARE NOT THIS FUNCTION'S JOB — the
	 *  path follower presses them on link/jump-area segments (UAIBPathFollowingComponent);
	 *  here a stall is only measured, never hopped out of (fix #4 R9).
	 *
	 *  The clocks are the CONTROLLER's (R3): one body, one stall, however many branches
	 *  flap over it. Returns TRUE when the caller must ABANDON its goal — the body has
	 *  gained no ground for WedgeStallSeconds against a goal a STOREY away (more than a
	 *  step above or below) with no link under the follower: nothing jumps that, so the
	 *  give-up window would be a stand. Or the window itself — the ROW's MoveGiveUpSeconds
	 *  (AIB22 F8-3; the GiveUpSeconds argument keeps one meaning: 0 = this mover never
	 *  gives up) — ran out on a same-level wedge. The caller fails its branch and arms
	 *  suppression. */
	bool TickLocomotion(AAIBBotController& Bot, const FVector& Goal, float ArriveRadiusUU, float DeltaTime, float GiveUpSeconds)
	{
		FAIBLocomotionState& State = Bot.GetLocomotion();
		IAIBAvatarInterface* Avatar = Bot.GetAvatar();
		APawn* Pawn = Bot.GetPawn();
		if (!Avatar || !Pawn)
		{
			return false;
		}
		const double Now = WorldSeconds(Bot);
		const FVector Here = Pawn->GetActorLocation();
		const float ToGoal = FVector::Dist(Here, Goal);
		if (!State.Goal.Equals(Goal, WedgeProgressUU))
		{
			State.Goal = Goal;
			State.GoalSetAtSeconds = Now;
		}
		// Phase 13: no sprint into a teammate's back while yielding (see below) — and ONLY
		// then (W-REVIEW H1: an early yield released sprint on every stalled tick).
		const bool bYielding = State.IsYielding(Now);
		SetSprint(*Avatar, State.bSprintHeld, !bYielding && ToGoal > ArriveRadiusUU * SprintBeyondRadiusFactor);

		if (ToGoal > ArriveRadiusUU)
		{
			FaceTravel(Bot, *Pawn, Goal, DeltaTime);
		}
		// F5-1(a): the goal this body just ABANDONED is refused up front for the window —
		// no clock, no line; the caller fails its branch as before. One line per episode.
		if (State.RefusesGoal(Goal, Now, WedgeProgressUU))
		{
			return true;
		}

		if (!State.bHasBestPoint || FVector::Dist(Here, State.BestPoint) > WedgeProgressUU)
		{
			if (State.bStallOpen)
			{
				EndStall(Bot, State, TEXT("moved"));
			}
			// REAL PROGRESS — the only thing that resets the clocks (R3) and re-arms the
			// one-per-wedge yield (Phase 13 H2).
			EndYield(Bot, State);
			State.NoteProgress();
			State.BestPoint = Here;
			State.bHasBestPoint = true;
			State.StallSeconds = 0.f;
			State.StallReportedSeconds = 0.f;
			State.StallStartedAtSeconds = Now;
			State.bDiagnosed = false;
			return false;
		}
		if (ToGoal <= ArriveRadiusUU)
		{
			return false; // standing AT the goal is station-keeping, not being stuck
		}
		// PHASE 13 (W-REVIEW H2): the stall clock KEEPS RUNNING through a yield — only the
		// sprint and the verdict wait for the window — so a doorway pair still reaches its
		// abandon on schedule and `stuck_seconds` reads the whole stand, not 0.03s of it.
		if (!bYielding)
		{
			EndYield(Bot, State);
		}
		State.StallSeconds += DeltaTime;
		if (!State.bStallOpen && State.StallSeconds - State.StallReportedSeconds >= StallReportSeconds)
		{
			State.bStallOpen = true;
		}
		if (State.StallSeconds < WedgeStallSeconds)
		{
			// NO BLIND HOP, NO RE-ISSUE (AIB22 step 4). Traversal verbs fire from the path
			// itself now (UAIBPathFollowingComponent: custom links and jump-area segments),
			// and a re-issued move only reset the corridor the follower was already on. A
			// stall with nothing traversable ahead is either an island (the Egress tactic's,
			// step 5) or a body wedged on geometry; both are the mover's give-up window to
			// end, and ReleaseLocomotion closes the episode as resolved=abandoned when it does.
			const UPathFollowingComponent* Follow = Bot.GetPathFollowingComponent();
			UE_LOG(LogAIBot, Verbose,
				TEXT("AIBot: %s stalled %.0fuu across / %.0fuu up — link=%s; the mover's give-up window decides."),
				*Bot.GetName(), FVector::Dist2D(Here, Goal), Goal.Z - FeetOf(*Pawn).Z,
				(Follow && Follow->IsFollowingNavLink()) ? TEXT("yes") : TEXT("no"));
			return false;
		}
		// PHASE 13 (AIB24, W-REVIEW H1/H2): a REAL wedge — the clock at WedgeStallSeconds,
		// never the first stalled tick — with a TEAMMATE inside the capsule sum is a body,
		// not geometry: ONE bounded yield per wedge (the controller-held latch, cleared only
		// by WedgeProgressUU of progress), sprint released and the verdict deferred for the
		// window while the crowd's separation does the stepping. No verb, no re-issue. The
		// count is HUD-grade (CountNearbyAllies); GetNearbyAgentLocations is the rejected
		// door (enemy positions, no LOS bound).
		if (!bYielding && !State.bYielded)
		{
			const FAIBTierRow& Tier = Bot.GetTierRow();
			const IAIBWorldQuery* Query = Bot.GetWorldQuery();
			const int32 AlliesInside = Query ? Query->CountNearbyAllies(Pawn, Tier.TeammateYieldRadiusUU) : 0;
			if (AlliesInside > 0 && State.TryArmYield(Now, Tier.TeammateYieldSeconds))
			{
				Bot.SetStillTactic(EAIBStillTactic::Yield, true);
				SetSprint(*Avatar, State.bSprintHeld, false);
				UE_LOG(LogAIBot, Log,
					TEXT("AIBot: %s t=%.1f yields to teammate — %d inside %.0fuu, %.1fs window at (%.0f,%.0f,%.0f)"),
					*Bot.GetName(), Now, AlliesInside, Tier.TeammateYieldRadiusUU,
					Tier.TeammateYieldSeconds, Here.X, Here.Y, Here.Z);
				return false; // the verdict waits for the window; the clock above did not
			}
		}
		if (bYielding)
		{
			return false;
		}
		// NO BLIND HOP, NO RE-ISSUE (AIB22 step 4, R9). Traversal verbs fire from the path
		// itself (UAIBPathFollowingComponent: custom links and jump-area segments); a
		// re-issued move only reset the corridor the follower was already on. The read
		// here is a DIAGNOSIS: a storey with no link is abandoned at once (R3 — the mezzanine
		// stalls, 3-46s each), a same-level wedge waits out the give-up window.
		const UPathFollowingComponent* Follow = Bot.GetPathFollowingComponent();
		const bool bOnLink = Follow && Follow->IsFollowingNavLink();
		const float UpUU = Goal.Z - FeetOf(*Pawn).Z; // F5-3: nav goal against the FEET
		const bool bStorey = !bOnLink && FMath::Abs(UpUU) > AIB::StepHeightUU
			&& Now - State.GoalSetAtSeconds >= WedgeStallSeconds;
		if (!State.bDiagnosed)
		{
			State.bDiagnosed = true;
			UE_LOG(LogAIBot, Verbose,
				TEXT("AIBot: %s stalled %.0fuu across / %.0fuu up — link=%s; %s"),
				*Bot.GetName(), FVector::Dist2D(Here, Goal), UpUU, bOnLink ? TEXT("yes") : TEXT("no"),
				bStorey ? TEXT("a storey — abandoning") : TEXT("the mover's give-up window decides."));
		}
		// AIB22 F8-3: the window is the row's, not the task's (every task authored 8 s).
		const float Window = GiveUpSeconds > 0.f ? Bot.GetTierRow().MoveGiveUpSeconds : 0.f;
		const bool bWindowOut = Window > 0.f && State.StallSeconds >= Window;
		if (bStorey || bWindowOut)
		{
			UE_LOG(LogAIBot, Log,
				TEXT("AIBot: %s t=%.1f stall abandoned — %.1fs, %.0fuu across / %.0fuu up, link=%s (%s, F7)"),
				*Bot.GetName(), Now, State.StallSeconds, FVector::Dist2D(Here, Goal), UpUU,
				bOnLink ? TEXT("yes") : TEXT("no"),
				bStorey ? TEXT("a storey with no link") : TEXT("give-up window"));
			// F5-1(a): the verdict CONSUMES the clock. The episode closes here (one
			// `stall over`, resolved=abandoned — ReleaseLocomotion finds it shut), this goal
			// is refused for the ambition's suppression window, and the next goal starts a
			// fresh clock through the progress block above.
			if (State.bStallOpen)
			{
				EndStall(Bot, State, TEXT("abandoned"));
			}
			const UAIBAmbitionEngine* Engine = Bot.GetAmbitionEngine();
			State.NoteAbandoned(Goal, Now, Engine ? Engine->FailureSuppressSeconds : 3.f);
			State.bHasBestPoint = false;
			return true;
		}
		return false;
	}

	/** Every mover's ExitState. A sprint carried out of a branch is the host's own leak:
	 *  the bot arrives in its firing position still holding the speed state. Closes the
	 *  stall EPISODE (the metric line); the clocks stay — the next branch's mover inherits
	 *  the same stuck body (R3). */
	void ReleaseLocomotion(AAIBBotController& Bot)
	{
		FAIBLocomotionState& State = Bot.GetLocomotion();
		if (State.bStallOpen)
		{
			EndStall(Bot, State, TEXT("abandoned"));
		}
		EndYield(Bot, State);
		if (IAIBAvatarInterface* Avatar = Bot.GetAvatar())
		{
			SetSprint(*Avatar, State.bSprintHeld, false);
		}
	}

	/** Fix #4 R6 — GROUNDED (the avatar's movement state) with NO MESH UNDER THE FEET: the
	 *  projection fails, or lands more than a step below the feet or beside them (the
	 *  gantry and core tops stand up to 300uu above their own navmesh; the `self=NO`
	 *  refusals). The feet are the capsule's bottom, not the actor origin. Fills the feet
	 *  and the nearest nav point. False on the mesh, airborne, or with no mesh in reach. */
	bool IsOffMesh(const AAIBBotController& Bot, const APawn& Pawn, FVector& OutFeet, FVector& OutOnNav)
	{
		const IAIBAvatarInterface* Avatar = Bot.GetAvatar();
		if (!Avatar || !Avatar->IsGrounded())
		{
			return false;
		}
		OutFeet = FeetOf(Pawn);
		if (!ProjectToNav(Bot.GetWorld(), OutFeet, OutOnNav))
		{
			return false;
		}
		const bool bOnMesh = OutFeet.Z - OutOnNav.Z <= AIB::StepHeightUU
			&& FVector::Dist2D(OutFeet, OutOnNav) <= AIB::StepHeightUU;
		return !bOnMesh;
	}

	/** A projection this close horizontally is a VERTICAL gap (F5-2): the body stands on
	 *  geometry ABOVE its navmesh, and a walk of 0uu cannot close it. */
	constexpr float VerticalGapUU = 10.f;

	/** THE VERTICAL GAP'S STEP-OFF (F5-2, fix #4 R6's lip machinery run from the feet). No
	 *  navmesh raycast is possible off the mesh, so the fan probes the ground around the
	 *  feet directly: a heading counts when lower navmesh sits under its probe point, by
	 *  more than a step, and the chooser accepts the drop. The best is the probe whose
	 *  navmesh lies most squarely under it. OutBeyond is the step-off's straight-line
	 *  target at the lower ground's height — the fall reaches it on its own. */
	bool FindVerticalGapStepOff(const AAIBBotController& Bot, const FVector& Feet, FVector& OutBeyond)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Bot.GetWorld());
		if (!NavSys)
		{
			return false;
		}
		const FAIBTierRow& Tier = Bot.GetTierRow();
		const float DropLimitUU = AIB::SafeDropUU;
		const FVector ProbeExtent(Tier.IslandLipProbeUU * 0.5f, Tier.IslandLipProbeUU * 0.5f, DropLimitUU * 0.5f);
		float BestOffsetSq = TNumericLimits<float>::Max();
		bool bFound = false;
		for (int32 Ray = 0; Ray < IslandLipRays; ++Ray)
		{
			const FVector Dir = FRotator(0.f, 360.f * Ray / IslandLipRays, 0.f).Vector();
			const FVector Probe = Feet + Dir * Tier.IslandLipProbeUU;
			FNavLocation Below;
			if (!NavSys->ProjectPointToNavigation(Probe - FVector(0.f, 0.f, DropLimitUU * 0.5f), Below, ProbeExtent))
			{
				continue;
			}
			const float DropUU = Feet.Z - Below.Location.Z;
			if (DropUU <= AIB::StepHeightUU)
			{
				continue; // the same deck, or higher: not the gap's floor
			}
			FAIBTraversalRequest Crossing;
			Crossing.HorizontalUU = Tier.IslandLipStandoffUU;
			Crossing.VerticalUU = -DropUU;
			if (FAIBTraversalPolicy::Choose(Crossing, /*bLastResort=*/true) != EAIBTraversal::Drop)
			{
				continue;
			}
			const float OffsetSq = static_cast<float>(FVector::DistSquared2D(Probe, Below.Location));
			if (OffsetSq < BestOffsetSq)
			{
				BestOffsetSq = OffsetSq;
				bFound = true;
				OutBeyond = FVector(Probe.X, Probe.Y, Below.Location.Z);
			}
		}
		return bFound;
	}

	enum class EAIBOffMeshRecovery : uint8
	{
		OnMesh,   // nothing to recover: on the mesh, airborne, or too far from any mesh
		Started,  // a recovery move is running — walk or step-off; TickOffMeshRecovery judges it
		Failed    // a vertical gap with no legal lip: failed ONCE, stranded for the cooldown
	};

	/** THE RECOVERY, decided from the feet. A HORIZONTAL gap inside IslandLipProbeUU is
	 *  the walk (pathfinding OFF, no projection — a pathed request refuses off the mesh);
	 *  farther is not a walk, it is the mover's refusal. A VERTICAL gap (F5-2) goes
	 *  straight to the step-off fan — the 0uu walk it used to issue could never close it
	 *  (856 walks, 95% FAILED, two channel spots on Spillway) — and with no legal lip
	 *  fails ONCE: one line, and the island latch STRANDS the body for EgressCooldownSeconds
	 *  (Wander stands, no draws) instead of the 3s walk / suppression / re-entry loop. */
	EAIBOffMeshRecovery StartOffMeshRecovery(AAIBBotController& Bot, const APawn& Pawn, FVector& OutTarget)
	{
		FVector Feet, OnNav;
		if (!IsOffMesh(Bot, Pawn, Feet, OnNav))
		{
			return EAIBOffMeshRecovery::OnMesh;
		}
		const FAIBTierRow& Tier = Bot.GetTierRow();
		const double Now = WorldSeconds(Bot);
		const float AcrossUU = FVector::Dist2D(Feet, OnNav);
		if (AcrossUU > VerticalGapUU)
		{
			if (AcrossUU > Tier.IslandLipProbeUU)
			{
				return EAIBOffMeshRecovery::OnMesh;
			}
			OutTarget = OnNav;
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f off-mesh recovery — walking %.0fuu to the mesh"),
				*Bot.GetName(), Now, AcrossUU);
			Bot.MoveToLocation(OutTarget, /*AcceptanceRadius=*/30.f, /*bStopOnOverlap=*/true,
				/*bUsePathfinding=*/false, /*bProjectDestinationToNavigation=*/false, /*bCanStrafe=*/true);
			return EAIBOffMeshRecovery::Started;
		}
		const float GapUU = Feet.Z - OnNav.Z;
		if (!FindVerticalGapStepOff(Bot, Feet, OutTarget))
		{
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f off-mesh recovery FAILED — vertical gap %.0fuu, no lip (F7)"),
				*Bot.GetName(), Now, GapUU);
			Bot.GetIslandLatch().Strand(Now, Tier.EgressCooldownSeconds);
			return EAIBOffMeshRecovery::Failed;
		}
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f off-mesh recovery — vertical gap %.0fuu, stepping off %.0fuu to the mesh"),
			*Bot.GetName(), Now, GapUU, FVector::Dist2D(Feet, OutTarget));
		Bot.MoveToLocation(OutTarget, /*AcceptanceRadius=*/80.f, /*bStopOnOverlap=*/true,
			/*bUsePathfinding=*/false, /*bProjectDestinationToNavigation=*/false, /*bCanStrafe=*/true);
		return EAIBOffMeshRecovery::Started;
	}

	/** How long an off-mesh recovery may take before it is a railing, not a walk. */
	constexpr float OffMeshRecoveryTimeoutSeconds = 3.f;

	/** The recovery's tick: +1 the feet are on the mesh (over), 0 still moving or airborne
	 *  off the edge, -1 gave up (timed out, or the mover let go short of it). A give-up
	 *  STRANDS for the cooldown (F5-2): the body could not leave, and the next entry must
	 *  stand rather than issue the same recovery again — the caller logs its own line. */
	int32 TickOffMeshRecovery(AAIBBotController& Bot, const APawn& Pawn, float& Seconds, float DeltaTime)
	{
		Seconds += DeltaTime;
		const IAIBAvatarInterface* Avatar = Bot.GetAvatar();
		if (!Avatar || !Avatar->IsGrounded())
		{
			return 0;
		}
		FVector Feet, OnNav;
		if (!IsOffMesh(Bot, Pawn, Feet, OnNav))
		{
			return 1;
		}
		if (Seconds >= OffMeshRecoveryTimeoutSeconds
			|| (Seconds > 0.5f && Bot.GetMoveStatus() == EPathFollowingStatus::Idle))
		{
			Bot.GetIslandLatch().Strand(WorldSeconds(Bot), Bot.GetTierRow().EgressCooldownSeconds);
			return -1;
		}
		return 0;
	}

	/** The reload crouch's three tenants move together — the toggle, the task's record of
	 *  having rented it, and the F9 tactic flag (AIB22) that says the squat is deliberate. */
	void SetReloadCrouch(AAIBBotController& Bot, IAIBAvatarInterface& Avatar,
		FAIBFireWhenAbleTaskInstanceData& InstanceData, bool bWant)
	{
		SetCrouch(Avatar, bWant);
		InstanceData.bCrouchedToReload = bWant;
		Bot.SetStillTactic(EAIBStillTactic::Reload, bWant);
	}
}

////////////////////////////////////////////////////////////////////

bool FAIBAmbitionGateCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const UAIBAmbitionEngine* Engine = Bot ? Bot->GetAmbitionEngine() : nullptr;
	if (!Engine)
	{
		return false;
	}
	// The IsValid guard stays OUTSIDE Matches: an invalid current want must never match
	// any branch (invalid == invalid is true, and MatchesTag on invalid is undefined
	// comfort) — the Fallback branch is where "no want" lands, by design.
	const FGameplayTag Current = Engine->GetCurrent();
	return Current.IsValid() && Matches(Current);
}

bool FAIBAmbitionGateCondition::Matches(const FGameplayTag& Current) const
{
	// EXACT equality is the default — the 1:1 arbitration mirror.
	const FGameplayTag BranchTag = GetBranchTag();
	return BranchTag.IsValid() && Current == BranchTag;
}

// Each branch's identity is a virtual, not a node parameter — the compiled authoring
// surface sets nothing on the nodes it adds, so per-branch data lives in the TYPE.
FGameplayTag FAIBAmbitionGateCondition::GetBranchTag() const     { return FGameplayTag(); }
FGameplayTag FAIBGateEngageCondition::GetBranchTag() const       { return AIBTags::Ambition_Engage; }
FGameplayTag FAIBGateRetreatCondition::GetBranchTag() const      { return AIBTags::Ambition_Retreat; }
FGameplayTag FAIBGateEvadeCondition::GetBranchTag() const        { return AIBTags::Ambition_Evade; }
FGameplayTag FAIBGateSearchCondition::GetBranchTag() const       { return AIBTags::Ambition_Search; }
FGameplayTag FAIBGateSeekCondition::GetBranchTag() const   { return AIBTags::Ambition_Seek; }
FGameplayTag FAIBGateRoamCondition::GetBranchTag() const         { return AIBTags::Ambition_Roam; }
FGameplayTag FAIBGateModeCondition::GetBranchTag() const         { return AIBTags::Ambition_Mode; }

bool FAIBGateModeCondition::Matches(const FGameplayTag& Current) const
{
	// A host's mode want is a CHILD tag (AIBot.Ambition.Mode.Hold); exact == can never
	// equal it, which was the statue-beside-the-objective defect (W-AUDIT P6 f.5).
	return Current.MatchesTag(AIBTags::Ambition_Mode);
}

bool FAIBTacticGateCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const UAIBAmbitionEngine* Engine = Bot ? Bot->GetTacticEngine() : nullptr;
	if (!Engine)
	{
		return false;
	}
	const FGameplayTag Current = Engine->GetCurrent();
	return Current.IsValid() && Matches(Current);
}

FGameplayTag FAIBGateTacticFlankCondition::GetBranchTag() const { return AIBTags::Tactic_Flank; }
FGameplayTag FAIBGateTacticHoldCondition::GetBranchTag() const  { return AIBTags::Tactic_Hold; }

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBAmbitionSentinelTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const UAIBAmbitionEngine* Engine = Bot ? ResolveEngine(*Bot) : nullptr;
	if (!Engine)
	{
		return EStateTreeRunStatus::Failed;
	}
	InstanceData.AmbitionAtEnter = Engine->GetCurrent();
	return EStateTreeRunStatus::Running;
}

const UAIBAmbitionEngine* FAIBAmbitionSentinelTask::ResolveEngine(const AAIBBotController& Bot) const { return Bot.GetAmbitionEngine(); }
const UAIBAmbitionEngine* FAIBTacticSentinelTask::ResolveEngine(const AAIBBotController& Bot) const   { return Bot.GetTacticEngine(); }

EStateTreeRunStatus FAIBAmbitionSentinelTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const UAIBAmbitionEngine* Engine = Bot ? ResolveEngine(*Bot) : nullptr;
	if (!Engine)
	{
		return EStateTreeRunStatus::Failed;
	}
	// SUCCEEDED, not failed: the want moved on, nothing broke. Root re-selects the
	// branch the brain now wants — this is the executor mirroring arbitration.
	return Engine->GetCurrent() == InstanceData.AmbitionAtEnter
		? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

void FAIBAmbitionSentinelTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->ClearStillTactics();
	}
}

void FAIBTacticSentinelTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->ClearStillTactics(EAIBStillTactic::Reload);
	}
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBFaceBeliefTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (Bot && Bot->GetSensorium().HasVisibleTarget())
	{
		return EStateTreeRunStatus::Running;
	}
	// Nothing to face. In Engage that ends the fight; in Retreat it is a normal moment
	// mid-flight and the flee must go on (see bRequireTarget).
	return InstanceData.bRequireTarget ? EStateTreeRunStatus::Failed : EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBFaceBeliefTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn || !Bot->GetSensorium().HasVisibleTarget())
	{
		// No pawn is a real failure either way. No TARGET is only a failure where the
		// branch is built on holding one.
		const bool bFatal = !Pawn || InstanceData.bRequireTarget;
		return bFatal ? EStateTreeRunStatus::Failed : EStateTreeRunStatus::Running;
	}
	// THE BELIEF, never the live actor: during the juke window this is the frozen
	// last-seen spot — a bot aiming through the pillar is the bug this line bans.
	//
	// PHASE 4 INTEGRATION — F4 finally executes: the aim point is the belief displaced
	// by the level's HELD, DECAYING angular error (drawn in the cone, settled over the
	// correct time, redrawn on cadence, fully reset on a target switch). The bot aims
	// where it believes MINUS how good its hands are — never a perfect solution.
	const AActor* Target = Bot->GetSensorium().GetVisibleTarget();
	const FVector AimPoint = FAIBAimPolicy::StepAimPoint(
		Bot->GetAimState(), Pawn->GetPawnViewLocation(),
		Bot->GetSensorium().GetLastSeenLocation(),
		Target ? Target->GetUniqueID() : 0,
		Bot->GetSkillProfile().Level(EAIBSkill::Aim),
		Bot->GetPolicyRandom(), Bot->GetWorld()->GetTimeSeconds());
	SteerControlRotation(*Bot, AimPoint, InstanceData.TurnDegreesPerSecond, DeltaTime);
	return EStateTreeRunStatus::Running;
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBMoveNearBeliefTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot || !Bot->GetSensorium().HasVisibleTarget())
	{
		return EStateTreeRunStatus::Failed;
	}
	InstanceData.LastGoal = Bot->GetSensorium().GetLastSeenLocation();
	InstanceData.RepathCooldown = 0.f;
	// THE ENTRY MIRRORS THE TICK'S YIELD (BN22 W-REVIEW M3): Engage re-enters on every
	// belief blink, and without this gate each re-entry issued a close-to-350 request
	// that ran at full speed until the current strafe leg expired — the beeline the
	// fight-range hand-off exists to kill, surviving at exactly the moments fights
	// blink most. Inside the fight range, footwork owns the legs from the first frame.
	if (IsWithin(*Bot, InstanceData.LastGoal, InstanceData.FightRangeUU))
	{
		Bot->SetStillTactic(EAIBStillTactic::Hold, true);
		return EStateTreeRunStatus::Running;
	}
	// Already in position: station-keep from here (issuing a move to where we stand
	// would complete instantly and thrash the branch — the never-succeed contract).
	if (!IsWithin(*Bot, InstanceData.LastGoal, InstanceData.AcceptanceRadiusUU))
	{
		const FVector Approach = RingApproachGoal(*Bot, InstanceData.LastGoal,
			InstanceData.FightRangeUU - InstanceData.AcceptanceRadiusUU);
		if (MoveToNavPoint(*Bot, Approach, InstanceData.AcceptanceRadiusUU)
			== EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s could not path to the belief — closing refused (F7). %s"), *Bot->GetName(), *DescribeMoveFailure(*Bot, InstanceData.LastGoal));
			return EStateTreeRunStatus::Failed;
		}
		InstanceData.RepathCooldown = InstanceData.RepathIntervalSeconds;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBMoveNearBeliefTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot || !Bot->GetSensorium().HasVisibleTarget())
	{
		return EStateTreeRunStatus::Failed;
	}

	// FOOTWORK OWNS THE FIGHT RANGE (founder, 27 Aug — see the instance data's comment):
	// target visible inside FightRangeUU, this mover stands down and the strafe task has
	// the legs. The sprint hand-off matters: TickLocomotion below is what RELEASES a
	// held sprint, so yielding must release it explicitly or the bot strafes at sprint
	// speed with the key stuck down (ReleaseLocomotion is state-guarded — free per tick).
	const FVector Belief = Bot->GetSensorium().GetLastSeenLocation();
	const bool bInFightRange = IsWithin(*Bot, Belief, InstanceData.FightRangeUU);
	Bot->SetStillTactic(EAIBStillTactic::Hold, bInFightRange); // F9: the station is a named hold
	if (bInFightRange)
	{
		ReleaseLocomotion(*Bot);
		return EStateTreeRunStatus::Running;
	}

	// Station-keeping: chase the belief's drift, never complete. Firing runs beside
	// this task; the sentinel or a visibility loss is what ends the branch. Out of
	// position for ANY reason — belief drift or the BOT displaced (knockback, a pad) —
	// re-closes on the repath cadence (W-REVIEW P3 M3).
	InstanceData.RepathCooldown -= DeltaTime;
	if (!IsWithin(*Bot, Belief, InstanceData.AcceptanceRadiusUU)
		&& InstanceData.RepathCooldown <= 0.f)
	{
		InstanceData.LastGoal = Belief;
		InstanceData.RepathCooldown = InstanceData.RepathIntervalSeconds;
		const FVector Approach = RingApproachGoal(*Bot, Belief,
			InstanceData.FightRangeUU - InstanceData.AcceptanceRadiusUU);
		if (MoveToNavPoint(*Bot, Approach, InstanceData.AcceptanceRadiusUU)
			== EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s could not path to the belief — closing refused (F7). %s"), *Bot->GetName(), *DescribeMoveFailure(*Bot, Belief));
			return EStateTreeRunStatus::Failed;
		}
	}

	// Close FAST, arrive WALKING. There is no blind case to test here — this task fails
	// without a held belief — so distance is the whole rule. The stall verdict is NOT
	// this task's to act on: a target a storey up with no way there is still a target to
	// shoot at, and the belief tasks beside this one own the state's fate.
	TickLocomotion(*Bot, Belief, InstanceData.AcceptanceRadiusUU, DeltaTime, /*GiveUpSeconds=*/0.f);
	return EStateTreeRunStatus::Running;
}

void FAIBMoveNearBeliefTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->SetStillTactic(EAIBStillTactic::Hold, false);
		ReleaseLocomotion(*Bot);
		Bot->StopMovement();
	}
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBFireWhenAbleTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bHolding = false;
	InstanceData.PhaseSecondsLeft = 0.f;
	// A fresh engagement gets a fresh swap budget: the cap exists to stop an endless spin
	// inside ONE fight, not to make a bot that gave up once give up for the match.
	InstanceData.SwapPresses = 0;
	InstanceData.SwapCooldownLeft = 0.f;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBFireWhenAbleTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	IAIBAvatarInterface* Avatar = Bot ? Bot->GetAvatar() : nullptr;
	if (!Bot || !Avatar)
	{
		// The avatar door closed mid-hold (the adapter died): the trigger cannot be
		// released through a door that no longer exists — say so at Warning, because a
		// silently stranded held verb is undiagnosable (W-REVIEW P3). The controller's
		// unpossess/EndPlay release is the belt on the host side.
		if (InstanceData.bHolding)
		{
			UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s lost its avatar door while holding fire — release could not be delivered."),
				Bot ? *Bot->GetName() : TEXT("<no controller>"));
			InstanceData.bHolding = false;
		}
		return EStateTreeRunStatus::Failed;
	}

	// A CORPSE PRESSES NOTHING. The controller holds the body until respawn (the host's
	// own lifecycle), and every press below this line was gated on facts — ammo, range,
	// visibility — none of which say "alive". A dead bot with a low magazine re-pressed
	// reload every retry window into an ASC whose weapon grants die with the body: the
	// leading suspect for BN20's 308 no-grant warnings, and wrong on its own terms
	// regardless (the fire gate's own State_Dead read shows the host already refuses
	// dead verbs — this stops asking). Held trigger released first, or the corpse keeps
	// firing until the actor is destroyed.
	if (!Avatar->IsAlive())
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = false;
			InstanceData.PhaseSecondsLeft = 0.f;
		}
		if (InstanceData.bAimHeld)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
			InstanceData.bAimHeld = false;
		}
		// AND THE CROUCH. This block already gives back the trigger and the sights; the crouch
		// was the one rented verb it kept, so a corpse stayed squatting and the next life's
		// task instance started believing it was mid-reload (aib-critic L2).
		if (InstanceData.bCrouchedToReload)
		{
			SetReloadCrouch(*Bot, *Avatar, InstanceData, false);
		}
		InstanceData.ReloadWantedSeconds = 0.f;
		return EStateTreeRunStatus::Running;
	}

	// The cached facts are the one info door: matured visibility + the assembled
	// can-fight answer, never raw avatar reads scattered through tasks.
	const FAIBFacts& Facts = Bot->GetLastFacts();

	// RELOAD, AND CROUCH WHILE THE HANDS ARE BUSY. The most dangerous seconds a bot has
	// are the ones it cannot shoot back in, and a reload is the only moment it chooses to
	// spend them — so it spends them small, and legibly: a crouched bot with its hands
	// busy reads as "reloading" from across the arena. No reserve is a different problem
	// (the weapon swap), and it is NOT handled here — see the ticket Log for why.
	// Every countdown throttle decays here, ABOVE the reload's early return: a cooldown
	// that froze while the bot reloaded would fire the instant the magazine landed. (The
	// melee throttle is an ABSOLUTE deadline on controller state now — nothing to decay.)
	InstanceData.ReloadCooldownLeft = FMath::Max(0.f, InstanceData.ReloadCooldownLeft - DeltaTime);
	InstanceData.SwapCooldownLeft = FMath::Max(0.f, InstanceData.SwapCooldownLeft - DeltaTime);
	InstanceData.ReAimCooldownLeft = FMath::Max(0.f, InstanceData.ReAimCooldownLeft - DeltaTime);
	// HOISTED ABOVE THE RELOAD GATE (aib-critic M3, 28 Aug), for two reasons that turned out
	// to be one. A bot mid-reload would not swing at a rusher inside knife range — it crouched
	// and took the beating, where Halo's answer to a rusher mid-reload is ALWAYS the melee. And
	// worse, the reload's early return skipped FAIBMeleePolicy::ShouldMelee entirely, while that
	// policy's own contract says it must be stepped EVERY tick or its continuous-range reset law
	// does not hold. Stepping it here honours the contract and makes the interrupt possible.
	// THIS FRAME'S distance to the belief — the reason the three checks below can exist at
	// all. Everything past here that names a range uses it; nothing past here reads the
	// think-rate DistToTargetUU, which is a want's number, not a swing's. (One deliberate
	// exception below: the grenade CALL reads the fact snapshot, because a throw is a
	// decision, not a swing — the policy's one-info-door law.)
	float DistanceUU = 0.f;
	const bool bHasDistance = LiveDistanceToBelief(*Bot, DistanceUU);
	const double Now = Bot->GetWorld()->GetTimeSeconds();

	// -- MELEE: they are close enough to touch, AND the bot has READ that ---------------
	// Two gates, two owners. The REACH is the held weapon's, through the door — a knife
	// commits from further than a rifle butt, and this code never learns which is which.
	// The RECOGNITION is the level's (Phase 4): the policy's clock starts when the range
	// picture becomes continuously true and answers after the level's delay — an Expert
	// reads the closing fight early and is already swinging on arrival, a Novice only
	// realises at point-blank and takes a beat more (the R11 reasoning, applied to the
	// knife). Stepped EVERY tick so the continuous-range reset law holds.
	//
	// EMPTY-HANDED is read HERE, above the melee, because it changes two decisions and the
	// melee is the first of them. It means the hand cannot fight AND the pouch can — the
	// dead-end state, not a dry loadout. See the swap block below for why those differ.
	const bool bCanFight = Avatar->CanWeaponFight();
	const bool bHasUsableWeapon = Avatar->HasUsableWeapon();
	const bool bEmptyHanded = !bCanFight && bHasUsableWeapon;
	const bool bMeleeRecognised = FAIBMeleePolicy::ShouldMelee(
		Bot->GetMeleeState(), bHasDistance ? DistanceUU : -1.f, Facts.bTargetVisible,
		Bot->GetSkillProfile().Level(EAIBSkill::Melee), Now, bEmptyHanded);
	const float MeleeRangeUU = Avatar->GetMeleeRangeUU();
	// THE INTERRUPT. Reach and recognition both, so the swing still obeys the level's read
	// (an Expert reads the rush early, a Novice a beat later) — the reload does not get to
	// bypass a capability gate, it only stops OUTRANKING one.
	const bool bMeleeWarranted = bMeleeRecognised && bHasDistance && MeleeRangeUU > 0.f
		&& DistanceUU <= MeleeRangeUU * MeleeCommitFraction;

	const bool bWantsReload = Facts.bHasReserveAmmo && Facts.AmmoNorm <= ReloadAtMagazineFraction;
	InstanceData.ReloadWantedSeconds = bWantsReload ? InstanceData.ReloadWantedSeconds + DeltaTime : 0.f;

	// THE CROUCH MUST NOT OUTLIVE ITS REASON. Crouching is rented against hands that are busy
	// changing a magazine; when the magazine never arrives the rent is never paid and the bot
	// simply squats in the open until it dies. One measured cause was an unresolved weapon row
	// (fixed in the adapter, which now reports such a weapon as FULL) — this is the guard for
	// every cause not yet measured. Standing up and fighting badly beats crouching and not
	// fighting at all.
	if (bWantsReload && InstanceData.ReloadWantedSeconds >= ReloadGiveUpSeconds)
	{
		if (InstanceData.bCrouchedToReload)
		{
			SetReloadCrouch(*Bot, *Avatar, InstanceData, false);
			UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s wanted a reload for %.0fs and never got one (ammo %.2f, reserve %s) — standing up and fighting anyway."),
				*Bot->GetName(), InstanceData.ReloadWantedSeconds, Facts.AmmoNorm,
				Facts.bHasReserveAmmo ? TEXT("yes") : TEXT("no"));
		}
	}
	else if (bWantsReload && !bMeleeWarranted)
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = false;
			InstanceData.PhaseSecondsLeft = 0.f;
		}
		// Hands busy = sights down: a human drops out of ADS to reload, and holding the
		// aim through the magazine change is a speed penalty bought for nothing.
		if (InstanceData.bAimHeld)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
			InstanceData.bAimHeld = false;
		}
		SetReloadCrouch(*Bot, *Avatar, InstanceData, true);
		if (InstanceData.ReloadCooldownLeft <= 0.f)
		{
			// THE CROUCH INSTRUMENT (founder, 28 Aug: "they are just staying crouched").
			// Measured 1222 reload REFUSALS against 1 activation in a single match, so the
			// bot asks every second, is told no, and holds the crouch that goes with asking.
			// The refusal happens on the GAME side and its reason is invisible from here, so
			// this prints the inputs the decision was made on — ammo, reserve, and whether the
			// body actually crouched — which is the half this module can see.
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s wants RELOAD — ammo %.2f (gate %.2f), reserve %s, crouched %s."),
				*Bot->GetName(), Facts.AmmoNorm, ReloadAtMagazineFraction,
				Facts.bHasReserveAmmo ? TEXT("yes") : TEXT("no"),
				Avatar->IsCrouched() ? TEXT("yes") : TEXT("no"));
			// One tap, like the human's R: the press activates, the release clears the
			// held flag so the next reload is a fresh press rather than an input the
			// ability system still believes is down.
			Avatar->PressVerb(AIBTags::Verb_Reload);
			Avatar->ReleaseVerb(AIBTags::Verb_Reload);
			InstanceData.ReloadCooldownLeft = ReloadRetrySeconds;
		}
		return EStateTreeRunStatus::Running;
	}
	if (InstanceData.bCrouchedToReload)
	{
		SetReloadCrouch(*Bot, *Avatar, InstanceData, false);
	}

	if (bMeleeRecognised && bHasDistance && MeleeRangeUU > 0.f
		&& Now >= Bot->GetMeleeState().NextSwingAtSeconds
		&& DistanceUU <= MeleeRangeUU * MeleeCommitFraction)
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = false;
			InstanceData.PhaseSecondsLeft = 0.f;
		}
		if (InstanceData.bAimHeld) // nobody knifes through a scope
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
			InstanceData.bAimHeld = false;
		}
		Avatar->PressVerb(AIBTags::Verb_Melee);
		Avatar->ReleaseVerb(AIBTags::Verb_Melee);
		// Controller-owned absolute deadline — a belief blink re-entering this state must
		// find the throttle still standing (W-REVIEW P4+5 H2).
		Bot->GetMeleeState().NextSwingAtSeconds = Now + MeleeRetrySeconds;
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s swung at %.0fuu (reach %.0f)."),
			*Bot->GetName(), DistanceUU, MeleeRangeUU);
		return EStateTreeRunStatus::Running;
	}

	// -- SWAP: hold the right thing, and NEVER hold nothing ---------------------------
	// The avatar answers; this presses. Nothing here knows what a weapon is or what one
	// is worth — pressing until the answer is yes walks past the host's null holster slot
	// the same way a mouse wheel does, which is why the host's equipment code needed no
	// change to make this work.
	//
	// TWO REASONS to spin the wheel, and conflating them is what left bots standing
	// around unarmed (founder, 1 Sep: "they tend to have not a weapon on it"):
	//
	//   EMPTY-HANDED — the hand cannot fight and the pouch can. A DEAD END, not a
	//     preference. The carry contains a deliberate null Unarmed slot; a cycle that
	//     stopped on it left IsBestWeaponForRange answering false forever (Best != Current
	//     AND Current cannot fight), so the "settled" reset below never fired, the press
	//     budget never refilled, and the bot was unarmed for the rest of its life. It
	//     also does not need a TARGET: a bot holding nothing should draw while it walks,
	//     which is why this arm ignores bHasDistance and asks at a default range.
	//   WRONG-RANGED — the hand works, something else works better here. A preference.
	//
	// THE PRESS CAP BELONGS TO THE SECOND CASE ONLY. It exists to stop a bot whose whole
	// loadout is dry from cycling for the rest of the match — and HasUsableWeapon() is
	// that test, properly stated. "I am standing on the empty holster" is not a dry
	// loadout, and budgeting it was the bug.
	// THE RANGE THE QUESTION IS ASKED AT. With a target it is the real distance; without
	// one it is a mid-map default, so a bot patrolling with the wrong tool in its hands
	// still walks toward the general-purpose answer instead of waiting to be shot at
	// before it thinks about its loadout. ONE range in play, asked once.
	const float SwapRangeUU = bHasDistance ? DistanceUU : AIB::NoTargetSwapRangeUU;
	// The two door reads are the ones taken above the melee, not fresh ones: one tick,
	// one answer, so the melee cannot believe the hand is empty while the swap believes
	// it is full.
	const FAIBSwapDecision Swap = FAIBWeaponPolicy::Decide(
		bCanFight, bHasUsableWeapon, Avatar->IsBestWeaponForRange(SwapRangeUU),
		InstanceData.SwapPresses, MaxSwapPresses);
	if (Swap.bCycle)
	{
		if (InstanceData.SwapCooldownLeft <= 0.f)
		{
			if (InstanceData.bHolding)
			{
				Avatar->ReleaseVerb(AIBTags::Verb_Fire);
				InstanceData.bHolding = false;
				InstanceData.PhaseSecondsLeft = 0.f;
			}
			Avatar->PressVerb(AIBTags::Verb_WeaponNext);
			Avatar->ReleaseVerb(AIBTags::Verb_WeaponNext);
			++InstanceData.SwapPresses;
			InstanceData.SwapCooldownLeft = SwapSeconds;
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s cycling weapons for %.0fuu (%s, press %d/%d)."),
				*Bot->GetName(), SwapRangeUU,
				Swap.bEmptyHanded ? TEXT("EMPTY-HANDED, uncapped") : TEXT("wrong-ranged"),
				InstanceData.SwapPresses, MaxSwapPresses);
		}
		// Do NOT fire mid-cycle: the hand may be empty or holding the wrong answer, and a
		// burst pressed into an equip montage is a burst that never leaves the barrel.
		return EStateTreeRunStatus::Running;
	}
	// SETTLED. Two conditions, because the old single one could not be met from the dead
	// end it was supposed to release: a bot on the null slot is never "best", so the reset
	// never fired. Something in hand that can fight is the first half of settled; being
	// the right thing for the range is the second, and with no target there is no range
	// to be right for.
	if (InstanceData.SwapPresses > 0 && Swap.bSettled)
	{
		InstanceData.SwapPresses = 0; // settled: the next range change gets a full budget
	}

	// -- GRENADE: the level's RECOGNITION, the pouch, the cooldown ---------------------
	// PHASE 4 INTEGRATION: the fixed band became the policy's per-level recognition
	// ladder — a Novice never throws deliberately, Trained sees the opener, Skilled the
	// finisher (on damage-DEALT history, never enemy vitals — F3), Expert the area
	// denial. The CALL reads the fact snapshot (a throw is a decision; one info door)
	// on the level's consider cadence; the throw still rides the CONTROLLER's cooldown,
	// because StateTree re-initialises instance data on every state ENTRY and Engage
	// re-enters whenever a belief blinks — a per-task countdown would reset roughly
	// once a second and throttle nothing at all (see AAIBBotController::CanThrowGrenade).
	const EAIBGrenadeCall GrenadeCall = FAIBGrenadePolicy::Consider(
		Bot->GetGrenadeState(), Facts,
		Bot->GetSkillProfile().Level(EAIBSkill::Grenade),
		Bot->GetPolicyRandom(), Now);
	if (GrenadeCall != EAIBGrenadeCall::None && !Bot->CanThrowGrenade())
	{
		// The recognition fired and the throttle ate it — across one 8s cooldown an
		// Expert burns several of these, and a silent burn is undiagnosable (F7,
		// W-REVIEW P4+5 M5). Verbose: it is cadence, not an error.
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s recognised a grenade moment (call %d) inside the throw cooldown."),
			*Bot->GetName(), static_cast<int32>(GrenadeCall));
	}
	if (GrenadeCall != EAIBGrenadeCall::None && Bot->CanThrowGrenade())
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = false;
			InstanceData.PhaseSecondsLeft = 0.f;
		}
		if (InstanceData.bAimHeld) // the throw drops the sights, like the human's
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
			InstanceData.bAimHeld = false;
		}
		Avatar->PressVerb(AIBTags::Verb_Grenade);
		Avatar->ReleaseVerb(AIBTags::Verb_Grenade);
		Bot->NoteGrenadeThrown(GrenadeCooldownSeconds);
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s threw (call %d) at %.0fuu of belief (%d left in the pouch)."),
			*Bot->GetName(), static_cast<int32>(GrenadeCall), Facts.DistToTargetUU, Facts.GrenadeCount);
		return EStateTreeRunStatus::Running;
	}

	// The live sensorium check closes the destroyed-target frame: a corpse whose weak
	// handle just went null must not be burst at for the rest of the fact snapshot's
	// life (W-REVIEW P3 M1 — the facts refresh at think cadence, this task at tick).
	// AIB23 W-REVIEW H1 — THE TRIGGER'S OWN EYES: a held target the bot cannot SEE right
	// now (a damage-eligible shooter, a juke in flight) is fired at only when the eyes
	// have a clear line to the believed point. A callout moves the feet, never the trigger.
	const FAIBSensorium& Senses = Bot->GetSensorium();
	const bool bMayFire = Facts.bTargetVisible && Facts.bWeaponCanFight && Senses.HasVisibleTarget()
		&& (Senses.IsSightCurrent() || Bot->HasLineOfSightToBelief());

	if (!bMayFire)
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = false;
			InstanceData.PhaseSecondsLeft = 0.f;
		}
		if (InstanceData.bAimHeld) // nothing to aim AT — sights down with the trigger
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
			InstanceData.bAimHeld = false;
		}
		return EStateTreeRunStatus::Running; // stay: visibility may return next pump
	}

	// -- ADS: the sights come up for the mid-range fight (founder, 27 Aug) --------------
	// Aim skill gates it (a Novice hip-fires — the ladder's shape everywhere else);
	// the band is [AimRangeUU, AimMaxRangeUU] with hysteresis so the boundary never
	// flaps; the HOST owns everything aiming means (spread, the speed penalty, and the
	// DESCOPE — a landed hit cancels the aim behind this task's back, which is why the
	// read is Avatar->IsAiming() and never our own flag, and why the re-press waits
	// ReAimSeconds: the descope must be FELT, not instantly erased). The press is
	// release-then-press on purpose — after a descope the old press is still logically
	// down, and the ability activates on the press EDGE.
	if (bHasDistance)
	{
		constexpr float AimHysteresisUU = 80.f;
		const bool bSkillAims = Bot->GetSkillProfile().Level(EAIBSkill::Aim) >= EAIBCompetence::Trained;
		const float InEdge = InstanceData.bAimHeld ? InstanceData.AimRangeUU - AimHysteresisUU : InstanceData.AimRangeUU;
		const float OutEdge = InstanceData.bAimHeld ? InstanceData.AimMaxRangeUU + AimHysteresisUU : InstanceData.AimMaxRangeUU;
		const bool bWantAim = bSkillAims && DistanceUU >= InEdge && DistanceUU <= OutEdge;
		if (bWantAim && !Avatar->IsAiming() && InstanceData.ReAimCooldownLeft <= 0.f)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
			Avatar->PressVerb(AIBTags::Verb_Aim);
			InstanceData.bAimHeld = true;
			InstanceData.ReAimCooldownLeft = InstanceData.ReAimSeconds;
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s aimed in at %.0fuu."), *Bot->GetName(), DistanceUU);
		}
		else if (!bWantAim && InstanceData.bAimHeld)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
			InstanceData.bAimHeld = false;
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s let the aim go at %.0fuu."), *Bot->GetName(), DistanceUU);
		}
	}

	// The muzzle gate's measured quantity: degrees between where the control rotation
	// points NOW and the line to the belief. FaceBelief is turning the rotation at its
	// rate limit; this only asks whether it has arrived.
	const APawn* FiringPawn = Bot->GetPawn();
	const FVector ToAimLine = Bot->GetSensorium().GetLastSeenLocation()
		- (FiringPawn ? FiringPawn->GetPawnViewLocation() : FVector::ZeroVector);
	float MuzzleOffDegrees = 0.f;
	if (ToAimLine.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		const float AlignDot = FMath::Clamp(FVector::DotProduct(
			Bot->GetControlRotation().Vector(), ToAimLine.GetSafeNormal()), -1.f, 1.f);
		MuzzleOffDegrees = FMath::RadiansToDegrees(FMath::Acos(AlignDot));
	}

	// A burst in progress breaks on a large swing — the trigger must not ride a turn.
	if (InstanceData.bHolding && MuzzleOffDegrees > BurstBreakAlignDegrees)
	{
		Avatar->ReleaseVerb(AIBTags::Verb_Fire);
		InstanceData.bHolding = false;
		InstanceData.PhaseSecondsLeft = InstanceData.BetweenBurstsSeconds;
		return EStateTreeRunStatus::Running;
	}

	InstanceData.PhaseSecondsLeft -= DeltaTime;
	if (InstanceData.PhaseSecondsLeft <= 0.f)
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = false;
			InstanceData.PhaseSecondsLeft = InstanceData.BetweenBurstsSeconds;
		}
		else if (MuzzleOffDegrees <= BurstStartAlignDegrees)
		{
			// The muzzle has arrived: this burst starts on the aim line, never across it.
			Avatar->PressVerb(AIBTags::Verb_Fire);
			InstanceData.bHolding = true;
			InstanceData.PhaseSecondsLeft = InstanceData.BurstSeconds;
		}
		// else: hold the trigger up this tick — the turn is still in flight, and the
		// phase clock stays expired so the burst begins the tick alignment lands.
	}
	return EStateTreeRunStatus::Running;
}

void FAIBFireWhenAbleTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// ALWAYS release: a held verb on the host's persistent verb sink outlives the body.
	// A skipped release is LOUD (W-REVIEW P3) — clearing bHolding must never silently
	// erase the record that the trigger is still down somewhere.
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (IAIBAvatarInterface* Avatar = Bot ? Bot->GetAvatar() : nullptr)
	{
		if (InstanceData.bHolding)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Fire);
		}
		if (InstanceData.bAimHeld)
		{
			Avatar->ReleaseVerb(AIBTags::Verb_Aim);
		}
		if (InstanceData.bCrouchedToReload)
		{
			// Stand back up on the way out, or the bot carries the reload's crouch — and
			// its speed penalty — into whatever it wanted instead.
			SetCrouch(*Avatar, false);
		}
	}
	else if (InstanceData.bHolding || InstanceData.bCrouchedToReload)
	{
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s exited a fire state holding a verb with no avatar door — release not delivered."),
			Bot ? *Bot->GetName() : TEXT("<no controller>"));
	}
	InstanceData.bHolding = false;
	InstanceData.bCrouchedToReload = false;

	// Leaving the fight breaks the melee CONTINUITY clock: ShouldMelee's "range stayed
	// true" reset only runs on ticks that happen, so a branch gap would otherwise splice
	// two separate approaches into one paid delay (W-REVIEW P4+5 H2's second half). The
	// swing deadline above deliberately survives — that one is the throttle.
	if (Bot)
	{
		Bot->GetMeleeState().InRangeSinceSeconds = -1.0;
		Bot->SetStillTactic(EAIBStillTactic::Reload, false);
	}
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBFleeFromBeliefTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	// THE BLAST OUTRANKS THE ENEMY, and this is what turns the Evade want into a dodge.
	// A grenade at your feet is a closer problem than the rifle across the room, and running
	// "away from the shooter" while standing on the grenade is how a bot dies looking clever.
	// BlastCenterRelative is centre-minus-self, so self + it is the centre in world space —
	// the fact was published for exactly this and had no reader until now.
	//
	// Same node, same mover, same projection and stall handling: the dodge is a flee with a
	// different threat, not a second movement system.
	FVector ThreatPoint;
	bool bHasThreatPoint = false;
	const FAIBFacts& BlastFacts = Bot->GetLastFacts();
	if (BlastFacts.bIncomingBlast)
	{
		ThreatPoint = Pawn->GetActorLocation() + BlastFacts.BlastCenterRelative;
		bHasThreatPoint = true;
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s scattering — blast %.1fs out, %.0fuu away."),
			*Bot->GetName(), BlastFacts.BlastSecondsToDetonation, BlastFacts.BlastCenterRelative.Size2D());

		// AND DASH OUT OF IT. This is what the verb is FOR: a grenade at your feet is the one
		// moment where walking away is not fast enough, and the host's dash covers ~500uu in a
		// quarter second — most of a blast radius, in the window a fuse actually leaves.
		//
		// Pressed HERE rather than in the strafe or on a timer, so it fires once per scatter
		// and only when something is genuinely about to go off. The bot tracks its own window
		// (CanDash) because the host refuses a dash on cooldown, and a refused verb pressed on
		// a timer is exactly the futile-press shape F7 bans — the reload taught that lesson.
		//
		// The MOVER still owns where the body goes: the dash is a shove along the direction
		// the flee already chose, not a second opinion about where safety is.
		if (Bot->CanDash())
		{
			if (IAIBAvatarInterface* DashAvatar = Bot->GetAvatar())
			{
				DashAvatar->PressVerb(AIBTags::Verb_Dash);
				DashAvatar->ReleaseVerb(AIBTags::Verb_Dash);
				Bot->NoteDashed(AIB::DashThrottleSeconds);
				UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s dashed clear of the blast."), *Bot->GetName());
			}
		}
	}
	// Otherwise: away from the freshest threat knowledge we hold — the visible belief, else
	// memory. With NOTHING held — hurt, threat unknown — Retreat still needs an executable exit
	// (W-REVIEW P3 H1): reposition to a random reachable point. A hurt bot that
	// relocates reads as falling back; a hurt bot frozen mid-arena reads as broken, and
	// the hysteresis defends the freeze.
	else if ((bHasThreatPoint = Bot->GetSensorium().HasVisibleTarget()))
	{
		ThreatPoint = Bot->GetSensorium().GetLastSeenLocation();
	}
	else
	{
		const float Window = Bot->GetLastFacts().MemoryFreshWindowSeconds;
		bHasThreatPoint = Bot->GetSensorium().Memory().GetFresh(
			Bot->GetWorld()->GetTimeSeconds(), Window > 0.f ? Window : AIB::DefaultMemoryFreshSeconds, ThreatPoint);
	}

	FVector Goal;
	if (bHasThreatPoint)
	{
		const FVector Away = (Pawn->GetActorLocation() - ThreatPoint).GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			// Standing exactly ON the threat point: direction is meaningless — fall
			// through to the reposition draw rather than failing silently forever.
			bHasThreatPoint = false;
		}
		else
		{
			const FVector Directed = Pawn->GetActorLocation() + Away * InstanceData.FleeDistanceUU;
			// "Straight away from the threat" is a DIRECTION, not a promise that anything is
			// standable there — off a ledge, through a wall, past the arena edge. When the
			// directed point will not project, take the SAME exit the zero-direction case
			// takes rather than handing the mover a goal it must refuse: a reachable point
			// somewhere else still breaks contact, and refusing does not move the bot at all.
			// Measured: unprojected, this refused 267 times per Retreat ambition.
			if (!ProjectToNav(Bot->GetWorld(), Directed, Goal))
			{
				bHasThreatPoint = false;
			}
		}
	}
	if (!bHasThreatPoint)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Bot->GetWorld());
		FNavLocation Reposition;
		if (!NavSys || !NavSys->GetRandomReachablePointInRadius(
			Pawn->GetActorLocation(), InstanceData.FleeDistanceUU, Reposition))
		{
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s wants Retreat with no threat point and no reachable reposition — standing (F7)."), *Bot->GetName());
			Bot->NoteCurrentAmbitionFailed(); // R2: never re-drawn next frame
			return EStateTreeRunStatus::Failed;
		}
		Goal = Reposition.Location;
	}

	InstanceData.FleeGoal = Goal;
	InstanceData.ClosestSoFarUU = FVector::Dist(Pawn->GetActorLocation(), Goal);
	InstanceData.SecondsWithoutProgress = 0.f;
	if (MoveToNavPoint(*Bot, InstanceData.FleeGoal, 150.f) == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s flee path REFUSED — failing loudly, not standing (F7). %s"), *Bot->GetName(), *DescribeMoveFailure(*Bot, InstanceData.FleeGoal));
		Bot->NoteCurrentAmbitionFailed(); // R2: a refused path rests the want, never a per-frame retry
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBFleeFromBeliefTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}
	// DEFEND, rather than run until the want expires. With the threat IN SIGHT and contact
	// already broken to the band's floor, this mover stands down and the footwork beside it
	// takes the legs — the bot evades and keeps shooting instead of jogging away with its
	// back turned (founder, 28 Aug). Out of sight it still flees: you cannot fight what you
	// cannot see, and breaking contact fully is the right answer there.
	// NEVER stand down on a live grenade. The defend band's whole purpose is to stop running
	// and fight — which is exactly the wrong answer while something is about to detonate, and
	// would cancel the scatter one tick after it started.
	if (InstanceData.DefendRangeUU > 0.f && !Bot->GetLastFacts().bIncomingBlast
		&& Bot->GetSensorium().HasVisibleTarget()
		&& !IsWithin(*Bot, Bot->GetSensorium().GetLastSeenLocation(), InstanceData.DefendRangeUU))
	{
		ReleaseLocomotion(*Bot);
		if (!InstanceData.bStoodDownToDefend)
		{
			// ONCE, on the edge. Per-tick this would cancel the strafe step every frame and
			// the bot would stand rooted — the exact "doing nothing" this change exists to end.
			InstanceData.bStoodDownToDefend = true;
			Bot->StopMovement();
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s broke contact — holding to DEFEND (%.0fuu out)."),
				*Bot->GetName(), FVector::Dist(Pawn->GetActorLocation(), Bot->GetSensorium().GetLastSeenLocation()));
		}
		// Deliberately NOT Succeeded: succeeding would re-select the branch and re-pick a
		// new flee goal, which is the running-away loop again. The bot stays in Retreat,
		// fighting, until the brain's own score says the danger has passed.
		return EStateTreeRunStatus::Running;
	}
	if (InstanceData.bStoodDownToDefend)
	{
		// THE WAY BACK OUT, and without it the band was a ONE-WAY DOOR (aib-critic H1, 28 Aug).
		// Standing down issued StopMovement, which kills the path request the enter state made.
		// Dropping back inside the band then cleared the flag and fell straight into
		// TickLocomotion, which holds sprint against a DEAD request — so the bot froze in the
		// open for about a second and a half until the wedge watchdog happened to re-issue it.
		//
		// And re-entry is not incidental, it is structural: the strafe walks a chord, whose
		// midpoint dips inward by ~11% at MaxArcDegrees 55, so two legs from 700-790uu put the
		// bot back under the floor by construction.
		InstanceData.bStoodDownToDefend = false;
		MoveToNavPoint(*Bot, InstanceData.FleeGoal, 150.f);
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s fell back inside the defend band — resuming the break."), *Bot->GetName());
	}

	if (IsWithin(*Bot, InstanceData.FleeGoal, 200.f))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	// A partial path that stalled short of the goal must not read as "fleeing" forever
	// (W-REVIEW P3 H3): no closer approach for the window = the path is dead, say so.
	const float DistNow = FVector::Dist(Pawn->GetActorLocation(), InstanceData.FleeGoal);
	if (DistNow < InstanceData.ClosestSoFarUU - 1.f)
	{
		InstanceData.ClosestSoFarUU = DistNow;
		InstanceData.SecondsWithoutProgress = 0.f;
	}
	else if ((InstanceData.SecondsWithoutProgress += DeltaTime) >= InstanceData.GiveUpAfterNoProgressSeconds)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s flee stalled %.1fs short of its goal — giving up loudly (F7)."),
			*Bot->GetName(), InstanceData.GiveUpAfterNoProgressSeconds);
		Bot->NoteCurrentAmbitionFailed();
		return EStateTreeRunStatus::Failed;
	}
	// A bot that WALKS away is not fleeing. Same helper as every other mover, so the
	// hold is released on the way out; a storey with no link abandons at once (R3).
	if (TickLocomotion(*Bot, InstanceData.FleeGoal, 200.f, DeltaTime, InstanceData.GiveUpAfterNoProgressSeconds))
	{
		Bot->NoteCurrentAmbitionFailed();
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

void FAIBFleeFromBeliefTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		ReleaseLocomotion(*Bot);
		Bot->StopMovement();
	}
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBMoveToLastKnownTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	const float Window = Bot->GetLastFacts().MemoryFreshWindowSeconds;
	FVector LastKnown;
	if (!Bot->GetSensorium().Memory().GetFresh(
		Bot->GetWorld()->GetTimeSeconds(), Window > 0.f ? Window : AIB::DefaultMemoryFreshSeconds, LastKnown))
	{
		return EStateTreeRunStatus::Failed; // stale: Root re-selects
	}
	InstanceData.ClosestSoFarUU = FVector::Dist(Pawn->GetActorLocation(), LastKnown);
	InstanceData.SecondsWithoutProgress = 0.f;
	InstanceData.SecondsSinceEnter = 0.f;
	InstanceData.MoverIdleSeconds = 0.f;
	if (!IsWithin(*Bot, LastKnown, InstanceData.AcceptanceRadiusUU)
		&& MoveToNavPoint(*Bot, LastKnown, InstanceData.AcceptanceRadiusUU)
			== EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s cannot path to the last-known spot — search fails loudly (F7). %s"), *Bot->GetName(), *DescribeMoveFailure(*Bot, LastKnown));
		// A refusal is about the mesh under ME (mid-fall, knockback, a fresh spawn), not
		// about the lead (W-REVIEW H1): keep the memory — a heard enemy would only re-
		// supply it — and rest the WANT for FailureSuppressSeconds, which is what stops
		// the re-select/refuse loop at the 0.1s failure delay.
		Bot->NoteCurrentAmbitionFailed();
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBMoveToLastKnownTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return EStateTreeRunStatus::Failed;
	}
	// Seeing someone ends searching — arbitration will already be swinging to Engage.
	if (Bot->GetSensorium().HasVisibleTarget())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	const float Window = Bot->GetLastFacts().MemoryFreshWindowSeconds;
	FVector LastKnown;
	if (!Bot->GetSensorium().Memory().GetFresh(
		Bot->GetWorld()->GetTimeSeconds(), Window > 0.f ? Window : AIB::DefaultMemoryFreshSeconds, LastKnown))
	{
		return EStateTreeRunStatus::Failed;
	}
	// Crossing ground toward a place someone WAS is the cheapest sprint a bot ever
	// takes: nothing to lose sight of and no burst to interrupt. It drops to a walk on
	// arrival, which is also when SweepLook's hunt starts mattering. A storey with no
	// link (R3) is a lead this body cannot reach: forget it, rest the want.
	if (TickLocomotion(*Bot, LastKnown, InstanceData.AcceptanceRadiusUU, DeltaTime, InstanceData.GiveUpAfterNoProgressSeconds))
	{
		Bot->ForgetSearchMemory(TEXT("stalled against a storey"), InstanceData.SecondsSinceEnter);
		Bot->NoteCurrentAmbitionFailed();
		return EStateTreeRunStatus::Failed;
	}

	// Arrived: stand at the post while SweepLook spends the controller's sweep budget,
	// then END THE WANT (AIB22 H1/F9) — a post that yielded nothing is a finished
	// search, and standing there until the memory ages is exactly the stand F9 bans.
	// Short of the post, no-progress means the spot is unreachable (a catwalk memory,
	// a nav hole) — or the mover has already gone Idle at the end of a PARTIAL path
	// (islands do not refuse, they deliver you to the edge: the audit's corrected
	// premise), which needs no 8s to diagnose. Either way: forget the lead, rest the
	// want, and fail LOUDLY instead of "searching" motionless for the memory window (H3).
	const APawn* Pawn = Bot->GetPawn();
	if (!Pawn)
	{
		return EStateTreeRunStatus::Running;
	}
	InstanceData.SecondsSinceEnter += DeltaTime;
	const float DistNow = FVector::Dist(Pawn->GetActorLocation(), LastKnown);
	const bool bMoverIdle = Bot->GetMoveStatus() == EPathFollowingStatus::Idle;
	InstanceData.MoverIdleSeconds = bMoverIdle ? InstanceData.MoverIdleSeconds + DeltaTime : 0.f;
	// The follower's reach test adds the agent radius to the acceptance radius, so an
	// honest arrival can stand a body-width outside IsWithin: the mover Idle inside 1.5x
	// acceptance IS the post (the sibling module's compiled band), not a short path.
	const bool bAtPost = DistNow <= InstanceData.AcceptanceRadiusUU
		|| (bMoverIdle && DistNow <= InstanceData.AcceptanceRadiusUU * 1.5f);
	if (!bAtPost)
	{
		if (DistNow < InstanceData.ClosestSoFarUU - 1.f)
		{
			InstanceData.ClosestSoFarUU = DistNow;
			InstanceData.SecondsWithoutProgress = 0.f;
		}
		else
		{
			InstanceData.SecondsWithoutProgress += DeltaTime;
		}
		// The short-path read (W-REVIEW M3): 0.5s since ENTER (the request is synchronous,
		// but one tick of engine ordering between issue and follow is not worth a false
		// give-up) AND Idle for 0.3s CONSECUTIVE — a single Idle frame between a detour's
		// path and its re-issue is not a finished path. The old test was a no-progress
		// ratchet, so any Idle frame after any detour fired it.
		const bool bMoverDoneShort = InstanceData.SecondsSinceEnter >= 0.5f && InstanceData.MoverIdleSeconds >= 0.3f;
		if (bMoverDoneShort || InstanceData.SecondsWithoutProgress >= InstanceData.GiveUpAfterNoProgressSeconds)
		{
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s cannot reach the last-known spot (%.0fuu short, mover %s) — giving up the search loudly (F7)."),
				*Bot->GetName(), DistNow, bMoverDoneShort ? TEXT("idle") : TEXT("stalled"));
			Bot->ForgetSearchMemory(bMoverDoneShort ? TEXT("path ended short") : TEXT("no progress"),
				InstanceData.SecondsSinceEnter);
			Bot->NoteCurrentAmbitionFailed();
			return EStateTreeRunStatus::Failed;
		}
		return EStateTreeRunStatus::Running;
	}
	// AT THE POST. The refill is the BODY standing somewhere new (fix #4 R4, the same key
	// Think uses) — asked here as well so the first at-post tick cannot read a budget
	// spent at the LAST post before Think's still sample has refilled it.
	FAIBSweepBudget& Budget = Bot->GetSweepBudget();
	Budget.ArriveAt(Pawn->GetActorLocation(), AIB::SweepRefillRadiusUU);
	if (!Budget.HasBudget(Bot->GetTierRow().SweepMaxSeconds))
	{
		Bot->ForgetSearchMemory(TEXT("post swept, nothing there"), Budget.SpentSeconds);
		Bot->NoteCurrentAmbitionFailed();
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

void FAIBMoveToLastKnownTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		ReleaseLocomotion(*Bot);
		Bot->StopMovement();
	}
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBSweepLookTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	// AIB22 `sweep_seconds` stamps. The active frames are already the new state's here
	// (engine: frames install before any task's EnterState), so the name is this branch.
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (const AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		InstanceData.EnterSeconds = WorldSeconds(*Bot);
		InstanceData.EnterLocation = Bot->GetPawn() ? Bot->GetPawn()->GetActorLocation() : FVector::ZeroVector;
		InstanceData.StateName = Bot->GetActiveStateName();
		// Phase == Arc is the pan's zero crossing: the walk starts looking straight ahead.
		InstanceData.PanPhaseDegrees = Bot->GetTierRow().SweepArcDegrees;
	}
	return EStateTreeRunStatus::Running;
}

void FAIBSweepLookTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return;
	}
	Bot->SetTravelPanDegrees(0.f); // the next state's mover faces its walk, not our pan
	Bot->SetStillTactic(EAIBStillTactic::Sweep, false);
	const APawn* Pawn = Bot->GetPawn();
	// STATIONARY seconds only (W-REVIEW M5): the budget this run spent, not the state's
	// duration — most of which is now walking. The Hold scan is Hold's, not sweep's.
	UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f sweep over — %.1fs, moved %.0fuu, state=%s"),
		*Bot->GetName(), WorldSeconds(*Bot), InstanceData.StationarySeconds,
		Pawn ? FVector::Dist(Pawn->GetActorLocation(), InstanceData.EnterLocation) : 0.f,
		InstanceData.StateName.IsNone() ? TEXT("?") : *InstanceData.StateName.ToString());
}

EStateTreeRunStatus FAIBSweepLookTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	// WALKING: NO YAW CLAIM (AIB22 H2, law F9). Before this the sweep claimed the yaw from
	// tick one, so the mover stood aside and the bot crab-walked to its post spinning. Now
	// the mover faces its travel and the sweep only BENDS that heading — a bounded pan
	// handed to TickLocomotion's facing block as an offset (see there for why it is not
	// written here). "Walking" is the mover's word plus the body's: a request still
	// running, or real ground speed under it.
	const FAIBTierRow& Tier = Bot->GetTierRow();
	const bool bStationary = Bot->GetMoveStatus() == EPathFollowingStatus::Idle
		&& Pawn->GetVelocity().Size2D() < AIB::TravelFacingMinSpeedUU;
	if (!bStationary)
	{
		Bot->SetStillTactic(EAIBStillTactic::Sweep, false);
		InstanceData.PanPhaseDegrees += InstanceData.SweepDegreesPerSecond * DeltaTime;
		Bot->SetTravelPanDegrees(AIBSweep::PanOffsetDegrees(InstanceData.PanPhaseDegrees, Tier.SweepArcDegrees));
		return EStateTreeRunStatus::Running;
	}
	Bot->SetTravelPanDegrees(0.f);

	// STANDING. Under a named Hold (Mode on its objective) the look is the unbudgeted
	// slow scan (W-REVIEW H2): a hill is held by standing on it — the founder's tactical
	// exception — but a guard looks about, and a budget that spends once for the whole
	// hold left the head frozen. Otherwise this is the search post's full-circle look,
	// bounded by the CONTROLLER's budget (H1: instance data is recreated on re-entry, so
	// a budget here refilled itself). Spent: a no-op that stays Running. Not claiming IS
	// releasing (the claim is a stamp that lapses in one hold window), and Failed would
	// end the whole state under Any-completion (H2) — the mover decides what a swept,
	// empty post means. The spend is the NAMED stillness `Sweep` (M5), so the idle gate
	// excludes exactly the seconds `sweep over` reports.
	const bool bHold = Bot->HasStillTactic(EAIBStillTactic::Hold);
	float ScanRate = AIB::HoldScanDegreesPerSecond;
	if (!bHold)
	{
		FAIBSweepBudget& Budget = Bot->GetSweepBudget();
		if (!Budget.HasBudget(Tier.SweepMaxSeconds))
		{
			Bot->SetStillTactic(EAIBStillTactic::Sweep, false);
			if (!InstanceData.bBudgetSpentLogged)
			{
				InstanceData.bBudgetSpentLogged = true;
				UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f sweep budget spent — %.1fs, releasing yaw"),
					*Bot->GetName(), WorldSeconds(*Bot), Budget.SpentSeconds);
			}
			return EStateTreeRunStatus::Running;
		}
		Budget.Spend(DeltaTime);
		InstanceData.StationarySeconds += DeltaTime;
		InstanceData.bBudgetSpentLogged = false; // a refill (new post, motion) earns a new line
		Bot->SetStillTactic(EAIBStillTactic::Sweep, true);
		ScanRate = InstanceData.SweepDegreesPerSecond;
	}

	// THE STATIONARY LOOK OWNS THE YAW, and says so. It writes the control rotation
	// directly rather than through SteerControlRotation (it turns at a rate toward no
	// point at all), so it must claim by hand — otherwise a mover ticking beside it
	// would drag the body back toward the path and the search would read as a bot
	// shaking its head.
	Bot->NoteYawClaimed(WorldSeconds(*Bot));

	// AREA DENIAL'S CALLER (the P4+5 review's dormant-Expert finding, closed): the
	// searching look is exactly where denial lives — target NOT visible, memory fresh —
	// and this task already owns the control rotation here, which is why the consult
	// is folded in rather than shipped as a new node (FireWhenAble's own pinned-node
	// rationale). The policy decides on its cadence through the one info door; this
	// only faces the remembered spot and presses. The throw must not ride the sweep's
	// arbitrary heading — the reviewers' explicit condition — so the press waits for
	// alignment, at the burst gate's own threshold. STANDING ONLY (W-REVIEW L6): the
	// align used to claim the yaw while walking, and the alignment is the sweep budget's
	// time — spent above before this runs.
	if (IAIBAvatarInterface* Avatar = Bot->GetAvatar())
	{
		const double Now = Bot->GetWorld()->GetTimeSeconds();
		const EAIBGrenadeCall DenialCall = FAIBGrenadePolicy::Consider(
			Bot->GetGrenadeState(), Bot->GetLastFacts(),
			Bot->GetSkillProfile().Level(EAIBSkill::Grenade),
			Bot->GetPolicyRandom(), Now);
		if (DenialCall == EAIBGrenadeCall::AreaDenial && Bot->CanThrowGrenade())
		{
			const float Window = Bot->GetLastFacts().MemoryFreshWindowSeconds;
			FVector Remembered;
			if (Bot->GetSensorium().Memory().GetFresh(Now,
				Window > 0.f ? Window : AIB::MaxMemorySeconds, Remembered))
			{
				SteerControlRotation(*Bot, Remembered, 360.f, DeltaTime);
				const FVector ToSpot = Remembered - Pawn->GetPawnViewLocation();
				if (ToSpot.SizeSquared() > KINDA_SMALL_NUMBER)
				{
					const float OffDot = FMath::Clamp(FVector::DotProduct(
						Bot->GetControlRotation().Vector(), ToSpot.GetSafeNormal()), -1.f, 1.f);
					if (FMath::RadiansToDegrees(FMath::Acos(OffDot)) <= BurstStartAlignDegrees)
					{
						Avatar->PressVerb(AIBTags::Verb_Grenade);
						Avatar->ReleaseVerb(AIBTags::Verb_Grenade);
						Bot->NoteGrenadeThrown(GrenadeCooldownSeconds);
						UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s denied the remembered spot with a grenade."),
							*Bot->GetName());
					}
				}
				// Denial owns the look until it resolves — the scan resumes next tick
				// the call goes quiet (thrown, throttled, or memory faded).
				return EStateTreeRunStatus::Running;
			}
		}
	}

	FRotator Swept = Bot->GetControlRotation();
	Swept.Yaw += ScanRate * DeltaTime;
	Swept.Normalize();
	Bot->SetControlRotation(Swept);
	Pawn->FaceRotation(Swept, DeltaTime);
	return EStateTreeRunStatus::Running;
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBMoveToPOITask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.bHasGoal = false;
	InstanceData.bStranded = false;
	InstanceData.bRecovering = false;
	InstanceData.TraversePhase = 0;

	// AIB19 — sometimes an idle leg is the climb (or the drop). Armed BEFORE the
	// ordinary picks so the traverse owns the Goal; every guard that fails just falls
	// through to the wander this leg would have been anyway. The short clock is charged
	// at ARMING, not at success — a route whose path immediately refuses must not
	// re-arm on every branch re-entry.
	if (MayGrappleTraverse())
	{
		IAIBWorldQuery* Query = Bot->GetWorldQuery();
		IAIBAvatarInterface* Avatar = Bot->GetAvatar();
		const double Now = Bot->GetWorld()->GetTimeSeconds();
		FVector Approach, Anchor;
		FName RouteId = NAME_None;
		if (Query && Avatar && Avatar->IsGrounded()
			&& Now >= InstanceData.NextTraverseAllowedSeconds
			&& Bot->GetSkillProfile().Level(EAIBSkill::Movement) >= EAIBCompetence::Trained
			&& Query->GetGrappleRoute(Pawn->GetActorLocation(), Approach, Anchor, RouteId))
		{
			const float SelfZ = Pawn->GetActorLocation().Z;
			FRandomStream& Random = Bot->GetPolicyRandom();
			if (Anchor.Z > SelfZ + InstanceData.MinTraverseRiseUU
				&& Random.FRand() < InstanceData.ClimbChance)
			{
				InstanceData.RouteApproach = Approach;
				InstanceData.RouteAnchor = Anchor;
				InstanceData.RouteId = RouteId;
				InstanceData.Goal = Approach;
				InstanceData.bHasGoal = true;
				InstanceData.TraversePhase = 1;
			}
			else if (SelfZ > Approach.Z + InstanceData.MinTraverseRiseUU
				&& Random.FRand() < InstanceData.DescendChance)
			{
				InstanceData.RouteApproach = Approach;
				InstanceData.RouteAnchor = Anchor;
				InstanceData.RouteId = RouteId;
				// The lip: the anchor's spot at the bot's own height — on this deck's
				// navmesh island by construction, so the walk there is an ordinary walk.
				InstanceData.Goal = FVector(Anchor.X, Anchor.Y, SelfZ);
				InstanceData.bHasGoal = true;
				InstanceData.TraversePhase = 4;
			}
			if (InstanceData.TraversePhase != 0)
			{
				InstanceData.PhaseSeconds = 0.f;
				InstanceData.bAirborneSeen = false;
				InstanceData.TraverseArmedSeconds = Now;
				InstanceData.NextTraverseAllowedSeconds =
					Now + InstanceData.TraverseCooldownSeconds / 3.0;
			}
		}
	}

	// SOMEWHERE TO BE, in preference order. The belief comes from the memory's fresh
	// window — F5-lawful, never a live actor read — so "move toward the current target"
	// means the last place this bot honestly knew of, not where the target is now.
	if (ShouldMoveToBeliefFirst())
	{
		const float Window = Bot->GetLastFacts().MemoryFreshWindowSeconds;
		FVector Believed;
		if (Bot->GetSensorium().Memory().GetFresh(Bot->GetWorld()->GetTimeSeconds(),
			Window > 0.f ? Window : AIB::MaxMemorySeconds, Believed))
		{
			InstanceData.Goal = Believed;
			InstanceData.bHasGoal = true;
		}
	}

	// A world-query provider is Phase 6's registration, and its POIs land here between
	// the belief and the wander. Until one exists, a mover that may wander does so and
	// every other FAILS loudly (F7) rather than improvising.
	TOptional<FVector> MoveTarget; // set only when the mover's goal differs from the arrival goal
	if (!InstanceData.bHasGoal && ShouldWanderWithoutProvider())
	{
		// THE DRAW IS FROM THE PAWN, AND IT IS NAVIGABLE, NOT REACHABLE (AIB22 5(B)).
		// A reachable draw can only ever land on the island the bot stands on — that was
		// the private patrol — and a draw centred on the heard fight (AIB17) was a
		// partial path, an 8s stall and a dead Roam on its own (H3). Now: any navmesh
		// point in the radius, from where the feet are; a full path wins outright; with
		// none, the LONGEST partial is walked to its end (W-REVIEW H1: Wander never fails
		// with a goal in hand — on an island that motion is toward the edge, which is the
		// point). Every draw is a measurement — IslandLatchDraws consecutive draws with no
		// full path latch the island fact on the controller, where a re-entry cannot
		// reset it; the path test carries NO cost limit (H2: a capped search reads "far
		// but reachable" as partial and false-latches open ground). The fight bias
		// survives as a preference between two self-centred draws.
		UWorld* World = Bot->GetWorld();
		const double Now = World->GetTimeSeconds();
		// STRANDED (W-REVIEW #3 H2): a confirmed island with no legal lip. No draw for the
		// cooldown — each would be IslandLatchDraws exhaustive pathfinds toward the same
		// nothing, 4 Hz forever on a micro-island — the bot stands, and the idle line
		// names it. Tick ends the stand when the latch says so.
		if (Bot->GetIslandLatch().IsStranded(Now))
		{
			InstanceData.bStranded = true;
			return EStateTreeRunStatus::Running;
		}
		// GROUNDED, OFF THE MESH (fix #4 R6): no draw measures anything from here, and
		// every move refuses (`self=NO`, 163 of 940 refusals from one spot). Walk to the
		// mesh first — pathfinding off — and let the re-entry draw from there.
		switch (StartOffMeshRecovery(*Bot, *Pawn, InstanceData.RecoveryTarget))
		{
		case EAIBOffMeshRecovery::Started:
			InstanceData.bRecovering = true;
			InstanceData.RecoverySeconds = 0.f;
			return EStateTreeRunStatus::Running;
		case EAIBOffMeshRecovery::Failed:
			InstanceData.bStranded = true; // F5-2: failed once; the latch's cooldown ends the stand
			return EStateTreeRunStatus::Running;
		case EAIBOffMeshRecovery::OnMesh:
			break;
		}
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const ANavigationData* NavData = NavSys ? NavSys->GetDefaultNavDataInstance() : nullptr;
		FVector Feet;
		// Off the mesh (mid-fall, knockback, a fresh spawn) no draw is a measurement of
		// anything: count nothing, fall through to the F7 line below.
		if (NavData && ProjectToNav(World, FeetOf(*Pawn), Feet))
		{
			const FAIBAllyFightMemory& AllyFight = Bot->GetAllyFightMemory();
			const bool bTowardFight = AllyFight.IsFresh(Now);
			const int32 LatchDraws = FMath::Max(Bot->GetTierRow().IslandLatchDraws, 1);
			double LongestPartialUU = -1.0;
			FVector PartialEnd = FVector::ZeroVector;
			for (int32 Draw = 0; Draw < LatchDraws && !InstanceData.bHasGoal; ++Draw)
			{
				FNavLocation Candidate;
				if (!NavSys->GetRandomPointInNavigableRadius(Feet, InstanceData.WanderRadiusUU, Candidate))
				{
					break; // no navmesh in reach at all: the F7 line, not the latch
				}
				// PHASE 12 — THE BEST OF A FEW DRAWS: toward the heard fight while one is
				// fresh (AIB17's bias, unchanged in rank), else the COLDEST cell of the
				// team's visit heat — exploration over re-treading, from the team's own
				// footsteps only (F3: a visit is not an enemy). Still one path test per draw.
				const FAIBTierRow& Row = Bot->GetTierRow();
				const UAIBTeamCoordinator* Team = World->GetSubsystem<UAIBTeamCoordinator>();
				auto HeatAt = [&](const FVector& Where)
				{
					return Team ? Team->VisitHeatAt(*Bot, Where, Row.VisitHeatCellUU, Row.VisitHeatDecaySeconds) : 0.f;
				};
				float CandidateHeat = bTowardFight ? 0.f : HeatAt(Candidate.Location);
				for (int32 Extra = 1; Extra < Row.VisitHeatDrawSamples; ++Extra)
				{
					FNavLocation Other;
					if (!NavSys->GetRandomPointInNavigableRadius(Feet, InstanceData.WanderRadiusUU, Other))
					{
						break;
					}
					if (bTowardFight)
					{
						if (FVector::DistSquared(Other.Location, AllyFight.HeardAt) < FVector::DistSquared(Candidate.Location, AllyFight.HeardAt))
						{
							Candidate = Other;
						}
					}
					else if (const float OtherHeat = HeatAt(Other.Location); OtherHeat < CandidateHeat)
					{
						Candidate = Other;
						CandidateHeat = OtherHeat;
					}
				}
				// AIB25 W-REVIEW M3: the bot's OWN filter ranks the draws' lengths — the
				// island test (the anchor walk) stays unfiltered: reachability, not taste.
				const FPathFindingResult Found = NavSys->FindPathSync(FPathFindingQuery(Bot, *NavData, Feet, Candidate.Location,
					UNavigationQueryFilter::GetQueryFilter(*NavData, Bot, Bot->GetDefaultNavigationFilterClass())));
				const bool bPathed = Found.IsSuccessful() && Found.Path.IsValid();
				const bool bFull = bPathed && !Found.Path->IsPartial();
				if (Bot->GetIslandLatch().NoteDraw(bFull, LatchDraws, Now))
				{
					UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s t=%.1f island latched — %d draws with no full path inside %.0fuu at (%.0f,%.0f,%.0f)"),
						*Bot->GetName(), Now, LatchDraws, InstanceData.WanderRadiusUU, Feet.X, Feet.Y, Feet.Z);
				}
				if (bFull)
				{
					InstanceData.Goal = Candidate.Location;
					InstanceData.bHasGoal = true;
					MoveTarget.Reset(); // a full draw walks itself; forget any earlier partial
					if (bTowardFight)
					{
						UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s wandering toward the team's fight."), *Bot->GetName());
					}
					else
					{
						UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s t=%.1f wander picks the coldest of %d draws — heat %.2f"),
							*Bot->GetName(), Now, FMath::Max(Row.VisitHeatDrawSamples, 1), CandidateHeat);
					}
				}
				else if (bPathed && Found.Path->GetLength() > LongestPartialUU)
				{
					LongestPartialUU = Found.Path->GetLength();
					PartialEnd = Found.Path->GetEndLocation();
					MoveTarget = Candidate.Location;
				}
			}
			if (!InstanceData.bHasGoal && LongestPartialUU >= 0.0)
			{
				// The arrival test is the partial's END; the mover is handed the draw
				// itself so the path it follows stays PARTIAL — a full-path completion
				// clears the latch (M3), and the edge of an island must not.
				InstanceData.Goal = PartialEnd;
				InstanceData.bHasGoal = true;
				UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s t=%.1f wander walks its longest partial draw — %.0fuu to (%.0f,%.0f,%.0f)"),
					*Bot->GetName(), Now, LongestPartialUU, PartialEnd.X, PartialEnd.Y, PartialEnd.Z);
			}
		}
	}

	if (!InstanceData.bHasGoal)
	{
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s has no POI provider for kind %s — branch fails."),
			*Bot->GetName(), *GetPOIKind().ToString());
		Bot->NoteCurrentAmbitionFailed(); // R2: an off-mesh or meshless entry is not retried next frame
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ClosestSoFarUU = FVector::Dist(Pawn->GetActorLocation(), InstanceData.Goal);
	InstanceData.SecondsWithoutProgress = 0.f;
	if (MoveToNavPoint(*Bot, MoveTarget.IsSet() ? MoveTarget.GetValue() : InstanceData.Goal, InstanceData.AcceptanceRadiusUU)
		== EPathFollowingRequestResult::Failed)
	{
		// R2: exactly Search's shape since fix #1 — the refusal rests the want. Without it
		// Roam re-selected at the 0.1s failure delay: 25,182 refusals from one bot, 170/s.
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s POI path refused — branch fails (F7). %s"), *Bot->GetName(), *DescribeMoveFailure(*Bot, InstanceData.Goal));
		Bot->NoteCurrentAmbitionFailed();
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBMoveToPOITask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (InstanceData.bStranded)
	{
		// Succeeded re-selects Roam: Wander re-enters and draws again (#3 H2).
		return Bot->GetIslandLatch().IsStranded(WorldSeconds(*Bot))
			? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
	}
	if (InstanceData.bRecovering)
	{
		// R6: on the mesh = Succeeded, so Roam re-selects and the next entry draws from
		// the mesh; a walk that never gets there is a railing — rest the want (R2).
		const int32 Recovery = TickOffMeshRecovery(*Bot, *Pawn, InstanceData.RecoverySeconds, DeltaTime);
		if (Recovery < 0)
		{
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f off-mesh recovery FAILED — %.1fs and the feet are still off the mesh (F7)"),
				*Bot->GetName(), WorldSeconds(*Bot), InstanceData.RecoverySeconds);
			Bot->NoteCurrentAmbitionFailed();
			return EStateTreeRunStatus::Failed;
		}
		return Recovery > 0 ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
	}
	if (!InstanceData.bHasGoal)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (InstanceData.TraversePhase != 0)
	{
		return TickTraverse(*Bot, *Pawn, InstanceData, DeltaTime);
	}
	if (IsWithin(*Bot, InstanceData.Goal, InstanceData.AcceptanceRadiusUU))
	{
		return EStateTreeRunStatus::Succeeded;
	}
	// An aborted or stalled walk must not read as a healthy roam forever (W-REVIEW P3
	// H3) — the wedge jump above gets its chance first (1.5s) and this is the give-up.
	const float DistNow = FVector::Dist(Pawn->GetActorLocation(), InstanceData.Goal);
	if (DistNow < InstanceData.ClosestSoFarUU - 1.f)
	{
		InstanceData.ClosestSoFarUU = DistNow;
		InstanceData.SecondsWithoutProgress = 0.f;
	}
	else if ((InstanceData.SecondsWithoutProgress += DeltaTime) >= InstanceData.GiveUpAfterNoProgressSeconds)
	{
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s POI walk stalled — giving up (F7)."), *Bot->GetName());
		Bot->NoteCurrentAmbitionFailed(); // R2/R3: a give-up rests the want
		return EStateTreeRunStatus::Failed;
	}
	// Crossing the arena with nothing to fight is the one time speed costs a bot
	// nothing — and it is what stops a roaming bot reading as a patrolling tourist.
	if (TickLocomotion(*Bot, InstanceData.Goal, InstanceData.AcceptanceRadiusUU, DeltaTime, InstanceData.GiveUpAfterNoProgressSeconds))
	{
		Bot->NoteCurrentAmbitionFailed();
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

void FAIBMoveToPOITask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	// A branch ended mid-traverse (the sentinel: the want moved on) resets the machine
	// but NOT a pull in flight — the host's movement component owns a started pull, and
	// a bot yanked into Engage mid-ride finishes arriving exactly like a human would.
	InstanceData.TraversePhase = 0;
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		ReleaseLocomotion(*Bot);
		Bot->StopMovement();
	}
}

/** AIB19's micro-machine. Every exit path either logs an outcome or logs a whiff, sets
 *  the traverse clock, and returns a status the branch can live with — a failed hook is
 *  never a stranded bot (F7): Succeeded re-selects Roam, which wanders as if the climb
 *  had never been offered. */
EStateTreeRunStatus FAIBMoveToPOITask::TickTraverse(AAIBBotController& Bot, APawn& Pawn,
	FInstanceDataType& InstanceData, float DeltaTime) const
{
	IAIBAvatarInterface* Avatar = Bot.GetAvatar();
	if (!Avatar)
	{
		InstanceData.TraversePhase = 0;
		return EStateTreeRunStatus::Failed;
	}
	const double Now = Bot.GetWorld()->GetTimeSeconds();
	const FVector Here = Pawn.GetActorLocation();
	const float RetrySoonSeconds = InstanceData.TraverseCooldownSeconds / 3.f;

	switch (InstanceData.TraversePhase)
	{
	case 1: // ---- walk to the approach --------------------------------------------
	case 4: // ---- walk to the lip (same walk, different doorstep) -----------------
	{
		if (IsWithin(Bot, InstanceData.Goal, InstanceData.ApproachReachUU))
		{
			ReleaseLocomotion(Bot);
			Bot.StopMovement();
			InstanceData.PhaseSeconds = 0.f;
			InstanceData.bAirborneSeen = false;
			if (InstanceData.TraversePhase == 1)
			{
				InstanceData.TraversePhase = 2;
			}
			else
			{
				// THE DROP: pathfinding OFF and no nav projection on purpose — the
				// approach point is a storey below this island, and walking straight
				// off the lip toward it is the whole mechanism. The fall is the move.
				Bot.MoveToLocation(InstanceData.RouteApproach, /*AcceptanceRadius=*/80.f,
					/*bStopOnOverlap=*/true, /*bUsePathfinding=*/false,
					/*bProjectDestinationToNavigation=*/false, /*bCanStrafe=*/true);
				UE_LOG(LogAIBot, Log, TEXT("AIBot: %s steps off the lip."), *Bot.GetName());
				InstanceData.TraversePhase = 5;
			}
			return EStateTreeRunStatus::Running;
		}
		// The ordinary walk machinery: progress bookkeeping, sprint, the wedge jump.
		const float DistNow = FVector::Dist(Here, InstanceData.Goal);
		if (DistNow < InstanceData.ClosestSoFarUU - 1.f)
		{
			InstanceData.ClosestSoFarUU = DistNow;
			InstanceData.SecondsWithoutProgress = 0.f;
		}
		else if ((InstanceData.SecondsWithoutProgress += DeltaTime)
			>= InstanceData.GiveUpAfterNoProgressSeconds)
		{
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s could not reach the grapple %s on %s — back to wandering."),
				*Bot.GetName(), InstanceData.TraversePhase == 1 ? TEXT("approach") : TEXT("lip"),
				*RouteLabel(InstanceData.RouteId));
			InstanceData.TraversePhase = 0;
			InstanceData.NextTraverseAllowedSeconds = Now + RetrySoonSeconds;
			return EStateTreeRunStatus::Failed;
		}
		if (TickLocomotion(Bot, InstanceData.Goal, InstanceData.ApproachReachUU, DeltaTime, InstanceData.GiveUpAfterNoProgressSeconds))
		{
			InstanceData.TraversePhase = 0;
			InstanceData.NextTraverseAllowedSeconds = Now + RetrySoonSeconds;
			Bot.NoteCurrentAmbitionFailed();
			return EStateTreeRunStatus::Failed;
		}
		return EStateTreeRunStatus::Running;
	}

	case 2: // ---- aim at the anchor, then ONE press -------------------------------
	{
		SteerControlRotation(Bot, InstanceData.RouteAnchor, TraverseAimTurnRate, DeltaTime);
		const FVector Desired = (InstanceData.RouteAnchor - Pawn.GetPawnViewLocation()).GetSafeNormal();
		const float CosErr = FVector::DotProduct(Bot.GetControlRotation().Vector(), Desired);
		if (CosErr >= FMath::Cos(FMath::DegreesToRadians(InstanceData.AimToleranceDeg)))
		{
			// The press goes through the ONE door and the host's own authority
			// validation judges it — range, LOS, cooldown — like any player's press.
			Avatar->PressVerb(AIBTags::Verb_Grapple);
			Avatar->ReleaseVerb(AIBTags::Verb_Grapple);
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s grapples for the high ground on %s (rise %.0fuu)."),
				*Bot.GetName(), *RouteLabel(InstanceData.RouteId),
				InstanceData.RouteAnchor.Z - Here.Z);
			InstanceData.TraversePhase = 3;
			InstanceData.PhaseSeconds = 0.f;
			InstanceData.bAirborneSeen = false;
			return EStateTreeRunStatus::Running;
		}
		if ((InstanceData.PhaseSeconds += DeltaTime) >= InstanceData.AimTimeoutSeconds)
		{
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s hook shot never lined up on %s — back to wandering."),
				*Bot.GetName(), *RouteLabel(InstanceData.RouteId));
			InstanceData.TraversePhase = 0;
			InstanceData.NextTraverseAllowedSeconds = Now + RetrySoonSeconds;
			return EStateTreeRunStatus::Succeeded;
		}
		return EStateTreeRunStatus::Running;
	}

	case 3: // ---- the ride --------------------------------------------------------
	case 5: // ---- the drop (same watch: airborne, then ground again) --------------
	{
		InstanceData.PhaseSeconds += DeltaTime;
		if (!Avatar->IsGrounded())
		{
			InstanceData.bAirborneSeen = true;
		}
		const bool bLanded = Avatar->IsGrounded() && InstanceData.bAirborneSeen
			&& InstanceData.PhaseSeconds > 0.25f;
		// Never left the ground after a full second = the press was REFUSED (the host's
		// cooldown or validation said no) — waiting out the whole ride timeout would
		// just be a bot standing at a known doorstep. The refusal was the host's right;
		// the early exit is ours.
		const bool bTimedOut = InstanceData.PhaseSeconds
			>= InstanceData.RideTimeoutSeconds + (InstanceData.TraversePhase == 5 ? 1.f : 0.f)
			|| (!InstanceData.bAirborneSeen && InstanceData.PhaseSeconds > 1.f);
		if (!bLanded && !bTimedOut)
		{
			return EStateTreeRunStatus::Running;
		}
		const bool bClimb = InstanceData.TraversePhase == 3;
		const float RiseUU = Here.Z - InstanceData.RouteApproach.Z;
		const bool bMadeIt = bLanded && (bClimb
			? RiseUU > InstanceData.MinTraverseRiseUU * 0.5f
			: RiseUU < InstanceData.MinTraverseRiseUU * 0.5f);
		if (bMadeIt && bClimb)
		{
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s made the deck on %s (%.0fuu up, wanted %.0fuu)."),
				*Bot.GetName(), *RouteLabel(InstanceData.RouteId), RiseUU,
				InstanceData.RouteAnchor.Z - InstanceData.RouteApproach.Z);
		}
		else if (bMadeIt)
		{
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s dropped back down on %s (%.0fuu)."),
				*Bot.GetName(), *RouteLabel(InstanceData.RouteId), RiseUU);
			// InstanceData.Goal is still the LIP the drop stepped off (phase 5 never
			// overwrites it); the clock is armed-to-landed until step 4's island fact.
			LogIslandEgress(Bot, TEXT("grapple"), InstanceData.Goal,
				static_cast<float>(Now - InstanceData.TraverseArmedSeconds));
		}
		else
		{
			// TWO DIFFERENT FAILURES wore one message, which is the other half of why
			// AIB19's finding could not be acted on. bAirborneSeen separates them and
			// they have OPPOSITE remedies:
			//   REFUSED    - never left the ground. The host's own range/LOS/cooldown
			//                validation said no, so the STANDOFF is wrong (generator).
			//   SHORT      - rode the hook and landed under the lip. The ANCHOR is wrong
			//                (manifest), or the pull cannot carry that rise.
			// Logged at Log, not Verbose: a whiff is the thing being measured, and a
			// diagnostic nobody's default verbosity prints is not an instrument.
			UE_LOG(LogAIBot, Log,
				TEXT("AIBot: %s traverse FAILED on %s (%s, rose %.0fuu of %.0fuu) — back to wandering."),
				*Bot.GetName(), *RouteLabel(InstanceData.RouteId),
				!InstanceData.bAirborneSeen ? TEXT("REFUSED - never left the ground")
					: (bClimb ? TEXT("SHORT - rode the hook, landed under the lip")
							  : TEXT("SHORT - drop never cleared the deck")),
				RiseUU, InstanceData.RouteAnchor.Z - InstanceData.RouteApproach.Z);
		}
		InstanceData.TraversePhase = 0;
		InstanceData.NextTraverseAllowedSeconds = Now
			+ (bMadeIt ? InstanceData.TraverseCooldownSeconds : RetrySoonSeconds);
		return EStateTreeRunStatus::Succeeded;
	}

	default:
		InstanceData.TraversePhase = 0;
		return EStateTreeRunStatus::Failed;
	}
}

FGameplayTag FAIBMoveToPOITask::GetPOIKind() const           { return FGameplayTag(); }
bool FAIBMoveToPOITask::ShouldWanderWithoutProvider() const  { return false; }

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBFlankTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}
	const FAIBFlankLatch& Latch = Bot->GetFlankLatch();
	if (Latch.bDone)
	{
		// W-REVIEW M2: the point was REACHED this fight and Engage re-selected. Stand by
		// silently — no move, no strike — until the next Think reads the zeroed point term
		// and hands to Push; the sentinel beside this ends the child.
		InstanceData.bDone = true;
		return EStateTreeRunStatus::Running;
	}
	InstanceData.bDone = false;
	if (!Latch.bHasPoint)
	{
		// The gate passed on the tactic, the latch is gone: a one-think race. Rest the
		// tactic so the next selection reads the board the latch's absence produces.
		Bot->NoteCurrentTacticFailed(TEXT("no latched flank point"));
		return EStateTreeRunStatus::Failed;
	}
	InstanceData.Goal = Latch.Point;
	InstanceData.ClosestSoFarUU = FVector::Dist(Pawn->GetActorLocation(), InstanceData.Goal);
	InstanceData.SecondsWithoutProgress = 0.f;
	InstanceData.EnteredAtSeconds = WorldSeconds(*Bot);
	if (MoveToNavPoint(*Bot, InstanceData.Goal, InstanceData.AcceptanceRadiusUU) == EPathFollowingRequestResult::Failed)
	{
		Bot->ClearFlankLatch(TEXT("refused"));
		Bot->NoteCurrentTacticFailed(TEXT("flank path refused"));
		return EStateTreeRunStatus::Failed;
	}
	UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f flank starts — point (%.0f,%.0f,%.0f) detour %.0fuu"),
		*Bot->GetName(), InstanceData.EnteredAtSeconds, InstanceData.Goal.X, InstanceData.Goal.Y, InstanceData.Goal.Z, Latch.DetourUU);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBFlankTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (InstanceData.bDone)
	{
		return EStateTreeRunStatus::Running; // M2: standing by for the tactic engine
	}
	const double Now = WorldSeconds(*Bot);
	if (IsWithin(*Bot, InstanceData.Goal, InstanceData.AcceptanceRadiusUU))
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f flank over — arrived after %.1fs"),
			*Bot->GetName(), Now, Now - InstanceData.EnteredAtSeconds);
		Bot->MarkFlankDone(); // M2: a mark, not a failure — re-entry runs silently
		return EStateTreeRunStatus::Succeeded;
	}
	const float DistNow = FVector::Dist(Pawn->GetActorLocation(), InstanceData.Goal);
	if (DistNow < InstanceData.ClosestSoFarUU - 1.f)
	{
		InstanceData.ClosestSoFarUU = DistNow;
		InstanceData.SecondsWithoutProgress = 0.f;
	}
	else if ((InstanceData.SecondsWithoutProgress += DeltaTime) >= InstanceData.GiveUpAfterNoProgressSeconds)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f flank over — stalled after %.1fs (F7)"),
			*Bot->GetName(), Now, Now - InstanceData.EnteredAtSeconds);
		Bot->ClearFlankLatch(TEXT("stalled"));
		Bot->NoteCurrentTacticFailed(TEXT("flank walk stalled"));
		return EStateTreeRunStatus::Failed;
	}
	if (TickLocomotion(*Bot, InstanceData.Goal, InstanceData.AcceptanceRadiusUU, DeltaTime, InstanceData.GiveUpAfterNoProgressSeconds))
	{
		Bot->ClearFlankLatch(TEXT("stalled"));
		Bot->NoteCurrentTacticFailed(TEXT("flank walk abandoned"));
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

void FAIBFlankTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		ReleaseLocomotion(*Bot);
		Bot->StopMovement();
	}
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBHoldStationTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot || !Bot->GetPawn())
	{
		return EStateTreeRunStatus::Failed;
	}
	Bot->NoteHoldEntered(WorldSeconds(*Bot));
	Bot->StopMovement();
	Bot->SetStillTactic(EAIBStillTactic::Hold, true); // F9: the stand is named
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBHoldStationTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot || !Bot->GetPawn())
	{
		return EStateTreeRunStatus::Failed;
	}
	const double Now = WorldSeconds(*Bot);
	const double Since = Bot->GetHoldSinceSeconds();
	if (Since >= 0.0 && Now - Since >= Bot->GetTierRow().HoldMaxSeconds)
	{
		// BOUNDED (F9): the hold is over, not broken. W-REVIEW H1: NOT a completion — a
		// Succeeded here re-selected Engage>Hold per FRAME until the next Think (the
		// cleared clock re-armed on each re-entry: 6-12 `hold over` lines and suppression
		// strikes per stand). The clock clears HERE, once, the tactic rests, and the child
		// keeps running until the tactic engine re-elects and the sentinel ends it.
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f hold over — %.1fs at station"), *Bot->GetName(), Now, Now - Since);
		Bot->NoteHoldOver();
		Bot->NoteCurrentTacticFailed(TEXT("hold reached HoldMaxSeconds"));
	}
	return EStateTreeRunStatus::Running;
}

void FAIBHoldStationTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->SetStillTactic(EAIBStillTactic::Hold, false);
	}
}

////////////////////////////////////////////////////////////////////

bool FAIBOnIslandCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return false;
	}
	const double Now = WorldSeconds(*Bot);
	const FAIBTierRow& Tier = Bot->GetTierRow();
	FAIBIslandLatch& Latch = Bot->GetIslandLatch();
	if (!Latch.ReadLatched(Now, Tier.LatchMaxAgeSeconds))
	{
		return false;
	}
	// ONCE PER LATCH (W-REVIEW #3 M6): the cache answers every evaluation after the first.
	if (Latch.Confirmation != FAIBIslandLatch::EConfirm::Untested)
	{
		return Latch.Confirmation == FAIBIslandLatch::EConfirm::Island;
	}
	// THE HYPOTHESIS, CONFIRMED AGAINST A LIST (W-REVIEW H2; fix #4 R1 over #3 H3's single
	// anchor): cost-unlimited path tests from the feet to each anchor the controller
	// offers — the current want's goal, the last completed full-path move, every
	// PlayerStart — stopping at the first FULL path. Recast reports a partial as a failed
	// test. ISLAND means NONE reached: one anchor on the bot's own island (Arena01's corner
	// spawn pads, 276 of 316 self-refutations) can no longer refute by itself, while a
	// floor bot still refutes through the objective in one test. Any full path is a false
	// read on open ground: cleared with the cooldown so the next draws walk. The verdict
	// is FAIBIslandLatch::Confirm, worldless, spec-driven. Off the mesh nothing can be
	// tested: gate closed, latch kept and still Untested, Wander re-measures once the
	// feet are back on it.
	const APawn* Pawn = Bot->GetPawn();
	UWorld* World = Bot->GetWorld();
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const ANavigationData* NavData = NavSys ? NavSys->GetDefaultNavDataInstance() : nullptr;
	FVector Feet;
	if (!Pawn || !NavData || !ProjectToNav(World, FeetOf(*Pawn), Feet))
	{
		return false;
	}
	TArray<FAIBIslandAnchor> Anchors;
	Bot->GetIslandAnchors(Anchors);
	TArray<const FAIBIslandAnchor*> Tested;
	TArray<bool> FullPaths;
	for (const FAIBIslandAnchor& Anchor : Anchors)
	{
		FVector AnchorOnNav;
		if (!ProjectToNav(World, Anchor.Location, AnchorOnNav))
		{
			continue;
		}
		const bool bFull = NavSys->TestPathSync(FPathFindingQuery(Bot, *NavData, Feet, AnchorOnNav));
		Tested.Add(&Anchor);
		FullPaths.Add(bFull);
		if (bFull)
		{
			break;
		}
	}
	const int32 Refuter = Latch.Confirm(FullPaths, Now, Tier.EgressCooldownSeconds);
	if (Refuter != INDEX_NONE)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f island latch REFUTED — full path to the %s anchor (%d of %d tested, %d offered) from (%.0f,%.0f,%.0f); cleared, %.0fs cooldown."),
			*Bot->GetName(), Now, Tested[Refuter]->Name, Refuter + 1, Tested.Num(), Anchors.Num(),
			Feet.X, Feet.Y, Feet.Z, Tier.EgressCooldownSeconds);
		return false;
	}
	if (Latch.Confirmation == FAIBIslandLatch::EConfirm::Untested)
	{
		// Untested, not Island: acting on the latch alone is not a confirmation, and only a
		// CONFIRMED island strands (#3 H2). Egress's ordinary failure clears the latch.
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f island latch unconfirmed — %d anchors offered, none on the mesh; acting on the latch alone."),
			*Bot->GetName(), Now, Anchors.Num());
		return true;
	}
	UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s t=%.1f island latch CONFIRMED — no full path to any of %d anchors from (%.0f,%.0f,%.0f)."),
		*Bot->GetName(), Now, Tested.Num(), Feet.X, Feet.Y, Feet.Z);
	return true;
}

namespace
{
	/** THE NEAREST EDGE OF MY ISLAND WITH A DROP BEYOND IT. A navmesh raycast walks the
	 *  polygon cluster under the feet and stops at its boundary — walls and lips alike,
	 *  never crossing a link — so every hit is on the surface the bot can walk (critic
	 *  L6: geometry I stand on, not map knowledge). A hit is a LIP when navmesh sits at
	 *  least MinDrop below the probe point just past it. OutLip is the on-mesh walk goal
	 *  (standoff inside the boundary); OutBeyond the step-off's straight-line target, at
	 *  the lower ground's height, which the fall reaches on its own. */
	struct FAIBLipCandidate
	{
		FVector Hit, Dir, Beyond, Landing;
		float DropUU, DistSq;
	};

	/** F7-1: a lip is LEGAL only if its LANDING is off the island — one cost-unlimited
	 *  path test from the projected landing to the nearest anchor the gate already found
	 *  unreachable from the feet. No full path = the landing is still this island (the
	 *  gantry top's pillar cut) or an unreachable ledge. No anchor on the mesh = nothing
	 *  to measure = legal, as before F7. */
	bool LandingLeavesIsland(const AAIBBotController& Bot, UNavigationSystemV1& NavSys, const FVector& Landing)
	{
		TArray<FAIBIslandAnchor> Anchors;
		Bot.GetIslandAnchors(Anchors);
		Anchors.Sort([&Landing](const FAIBIslandAnchor& A, const FAIBIslandAnchor& B)
		{
			return FVector::DistSquared(A.Location, Landing) < FVector::DistSquared(B.Location, Landing);
		});
		const ANavigationData* NavData = NavSys.GetDefaultNavDataInstance();
		for (const FAIBIslandAnchor& Anchor : Anchors)
		{
			FVector AnchorOnNav;
			if (NavData && ProjectToNav(Bot.GetWorld(), Anchor.Location, AnchorOnNav))
			{
				return NavSys.TestPathSync(FPathFindingQuery(&Bot, *NavData, Landing, AnchorOnNav));
			}
		}
		return true;
	}

	bool FindIslandLip(const AAIBBotController& Bot, const FVector& Feet, FVector& OutLip, FVector& OutBeyond, float& OutDropUU, FBox& OutFootprint)
	{
		UWorld* World = Bot.GetWorld();
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		OutFootprint = FBox(ForceInit);
		if (!NavSys)
		{
			return false;
		}
		const FAIBTierRow& Tier = Bot.GetTierRow();
		// The probe box is the SURVIVABLE drop envelope (W-REVIEW M5, limit per #3 M5:
		// Egress is the last resort and asks at SafeDropUU, no commit fraction — the 893uu
		// gantry is a lip, not a wall), and it hangs BELOW the probe point (L7) so only
		// lower navmesh qualifies — a same-level neighbour behind a thin wall is not a lip.
		const float DropLimitUU = AIB::SafeDropUU;
		const FVector ProbeExtent(Tier.IslandLipProbeUU * 0.5f, Tier.IslandLipProbeUU * 0.5f, DropLimitUU * 0.5f);
		TArray<FAIBLipCandidate> Candidates;
		OutFootprint += Feet;
		for (int32 Ray = 0; Ray < IslandLipRays; ++Ray)
		{
			const FVector Dir = FRotator(0.f, 360.f * Ray / IslandLipRays, 0.f).Vector();
			const FVector End = Feet + Dir * IslandLipReachUU;
			FVector Hit;
			const bool bHit = UNavigationSystemV1::NavigationRaycast(World, Feet, End, Hit);
			OutFootprint += bHit ? Hit : End; // F7-3: the surface's extent this way
			if (!bHit)
			{
				continue; // open ground this way
			}
			const FVector Probe = Hit + Dir * Tier.IslandLipProbeUU;
			FNavLocation Below;
			if (!NavSys->ProjectPointToNavigation(Probe - FVector(0.f, 0.f, DropLimitUU * 0.5f), Below, ProbeExtent))
			{
				continue; // a wall, or a pit with no floor in the drop envelope
			}
			const float DropUU = Hit.Z - Below.Location.Z;
			if (DropUU < Tier.IslandMinDropUU)
			{
				continue; // a step, a seam, the same deck
			}
			// The chooser has the last word on every crossing (M5): a lip it refuses —
			// too far down — is a wall to this bot. The horizontal is the step-off's own
			// length, the lip standoff (#3 M5) — the projection's scatter inside the probe
			// box measured the probe, not the crossing.
			FAIBTraversalRequest Crossing;
			Crossing.HorizontalUU = Tier.IslandLipStandoffUU;
			Crossing.VerticalUU = -DropUU;
			if (FAIBTraversalPolicy::Choose(Crossing, /*bLastResort=*/true) != EAIBTraversal::Drop)
			{
				continue;
			}
			Candidates.Add({ Hit, Dir, FVector(Probe.X, Probe.Y, Below.Location.Z), Below.Location, DropUU, static_cast<float>(FVector::DistSquared2D(Feet, Hit)) });
		}
		// NEAREST LEGAL (F7-1): nearest first, one landing path test each until one leaves.
		Candidates.Sort([](const FAIBLipCandidate& A, const FAIBLipCandidate& B) { return A.DistSq < B.DistSq; });
		for (const FAIBLipCandidate& Candidate : Candidates)
		{
			if (!LandingLeavesIsland(Bot, *NavSys, Candidate.Landing))
			{
				UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s lip at (%.0f,%.0f,%.0f) skipped — landing still islanded"),
					*Bot.GetName(), Candidate.Hit.X, Candidate.Hit.Y, Candidate.Hit.Z);
				continue;
			}
			OutLip = Candidate.Hit - Candidate.Dir * Tier.IslandLipStandoffUU;
			OutBeyond = Candidate.Beyond;
			OutDropUU = Candidate.DropUU;
			return true;
		}
		return false;
	}
}

namespace
{
	/** THE LIP FAN AND THE LIP WALK, run once the feet are grounded AND on the mesh (fix #4
	 *  R6): from EnterState directly, or from Tick after the ground came back or the
	 *  off-mesh recovery walked the body onto the mesh. */
	EStateTreeRunStatus BeginEgress(AAIBBotController& Bot, APawn& Pawn, FAIBEgressTaskInstanceData& InstanceData)
	{
		FAIBIslandLatch& Latch = Bot.GetIslandLatch();
		const double Now = WorldSeconds(Bot);
		const float Cooldown = Bot.GetTierRow().EgressCooldownSeconds;
		InstanceData.bBegun = true;
		InstanceData.SecondsSinceEnter = 0.f; // the lip walk's own grace, from HERE
		InstanceData.MoverIdleSeconds = 0.f;
		InstanceData.SecondsWithoutProgress = 0.f;

		FVector Feet;
		float DropUU = 0.f;
		const bool bOnMesh = ProjectToNav(Bot.GetWorld(), FeetOf(Pawn), Feet);
		if (!bOnMesh || !FindIslandLip(Bot, Feet, InstanceData.Lip, InstanceData.Beyond, DropUU, InstanceData.Footprint))
		{
			if (bOnMesh && Latch.Confirmation == FAIBIslandLatch::EConfirm::Island)
			{
				// STRANDED (W-REVIEW #3 H2): a CONFIRMED island with no policy-legal lip is a
				// MAP defect — one Log line per latch for the verifier, then the cooldown with
				// no draws at all (Wander stands, tactic=Stranded on the idle line).
				Latch.Strand(Now, Cooldown);
				UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f stranded — no legal lip within %.0fuu (drops ≤ %.0fuu)"),
					*Bot.GetName(), Now, IslandLipReachUU, AIB::SafeDropUU);
				return EStateTreeRunStatus::Failed;
			}
			// No mesh within reach of the feet even after the recovery, or acting on an
			// unconfirmed latch with no lip in the envelope. This tactic has nothing to
			// offer: clear the latch AND arm the cooldown (W-REVIEW H1) so Wander walks its
			// partial draws instead of re-latching into the same nothing every failure delay.
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f island egress FAILED — no lip with a drop within %.0fuu (grounded=yes, on-mesh=%s) — latch cleared, %.0fs cooldown (F7)."),
				*Bot.GetName(), Now, IslandLipReachUU, bOnMesh ? TEXT("yes") : TEXT("no"), Cooldown);
			Latch.ClearWithCooldown(Now, Cooldown);
			return EStateTreeRunStatus::Failed;
		}
		InstanceData.Footprint += FeetOf(Pawn); // the real feet's height, not the nav projection's
		InstanceData.ClosestSoFarUU = FVector::Dist(Pawn.GetActorLocation(), InstanceData.Lip);
		// F7-2: under the agent radius + standoff HORIZONTALLY from the FEET (IsWithin reads
		// 3D from the centre — a body standing on the lip is 88uu "away"): the walk cannot
		// be made; the body is at the lip already and only the step-off remains.
		InstanceData.bAtLipOnEntry = FVector::Dist2D(Feet, InstanceData.Lip) < AIB::AgentRadiusUU + Bot.GetTierRow().IslandLipStandoffUU;
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s t=%.1f island egress starts — lip (%.0f,%.0f,%.0f) %.0fuu away, drop %.0fuu%s"),
			*Bot.GetName(), Now, InstanceData.Lip.X, InstanceData.Lip.Y, InstanceData.Lip.Z,
			InstanceData.ClosestSoFarUU, DropUU, InstanceData.bAtLipOnEntry ? TEXT(" (at the lip, F7-2)") : TEXT(""));
		// Already at the lip: Tick steps off this frame. Otherwise an ordinary on-mesh walk
		// — EGRESS'S OWN (R8): its completion on the island must not clear the latch.
		if (!InstanceData.bAtLipOnEntry && !IsWithin(Bot, InstanceData.Lip, InstanceData.LipReachUU))
		{
			if (MoveToNavPoint(Bot, InstanceData.Lip, InstanceData.LipReachUU) == EPathFollowingRequestResult::Failed)
			{
				Latch.ClearWithCooldown(Now, Cooldown);
				Bot.NoteCurrentAmbitionFailed(); // R2
				return EStateTreeRunStatus::Failed;
			}
			Bot.MarkEgressMove();
		}
		return EStateTreeRunStatus::Running;
	}
}

EStateTreeRunStatus FAIBEgressTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	IAIBAvatarInterface* Avatar = Bot ? Bot->GetAvatar() : nullptr;
	if (!Pawn || !Avatar)
	{
		return EStateTreeRunStatus::Failed;
	}
	InstanceData.bAirborneSeen = false;
	InstanceData.bSteppedOff = false;
	InstanceData.bAtLipOnEntry = false;
	InstanceData.bBegun = false;
	InstanceData.bRecovering = false;
	InstanceData.RecoverySeconds = 0.f;
	InstanceData.LipSeconds = 0.f;
	InstanceData.SecondsWithoutProgress = 0.f;
	InstanceData.SecondsSinceEnter = 0.f;
	InstanceData.MoverIdleSeconds = 0.f;
	InstanceData.StrandedSinceSeconds = Bot->GetIslandLatch().LatchedAtSeconds;
	// F6-2: up BEFORE any of Egress's moves (recovery, lip walk, step-off) is issued — a lip
	// walk inside acceptance completes inside the request, before MarkEgressMove's id exists.
	Bot->SetEgressMoveInFlight(true);

	// GROUNDED IS THE AVATAR'S MOVEMENT STATE (fix #4 R6), never the nav projection. In
	// the air (a grapple ride, a knock): nothing to measure yet — Tick begins on landing
	// instead of failing into a cooldown on the very platform this tactic exists for.
	if (!Avatar->IsGrounded())
	{
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s island egress waits for the ground."), *Bot->GetName());
		return EStateTreeRunStatus::Running;
	}
	// Grounded but OFF the mesh (geometry standing above its navmesh — the gantry and
	// core tops): walk to the nearest nav point first, pathfinding off; the lip fan then
	// runs from that on-mesh point. Egress's own move (R8).
	switch (StartOffMeshRecovery(*Bot, *Pawn, InstanceData.RecoveryTarget))
	{
	case EAIBOffMeshRecovery::Started:
		Bot->MarkEgressMove();
		InstanceData.bRecovering = true;
		return EStateTreeRunStatus::Running;
	case EAIBOffMeshRecovery::Failed:
		Bot->NoteCurrentAmbitionFailed(); // F5-2: stranded by the recovery; Wander stands it out
		return EStateTreeRunStatus::Failed;
	case EAIBOffMeshRecovery::OnMesh:
		break;
	}
	return BeginEgress(*Bot, *Pawn, InstanceData);
}

EStateTreeRunStatus FAIBEgressTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	IAIBAvatarInterface* Avatar = Bot ? Bot->GetAvatar() : nullptr;
	if (!Pawn || !Avatar)
	{
		return EStateTreeRunStatus::Failed;
	}
	const FVector Here = Pawn->GetActorLocation();
	const double Now = WorldSeconds(*Bot);
	const FAIBTierRow& Tier = Bot->GetTierRow();
	InstanceData.SecondsSinceEnter += DeltaTime;

	// FALLING: the fall is the move. Nothing to press, nothing to steer, nothing to cancel.
	// Counts as the drop only after the step-off (W-REVIEW M4): one ungrounded frame on
	// the walk — a seam, a bump — is not a landing to judge.
	if (!Avatar->IsGrounded())
	{
		InstanceData.bAirborneSeen = InstanceData.bSteppedOff;
		return EStateTreeRunStatus::Running;
	}
	// R6: the recovery walk, then the fan from the mesh; or the ground came back.
	if (InstanceData.bRecovering)
	{
		const int32 Recovery = TickOffMeshRecovery(*Bot, *Pawn, InstanceData.RecoverySeconds, DeltaTime);
		if (Recovery == 0)
		{
			return EStateTreeRunStatus::Running;
		}
		InstanceData.bRecovering = false;
		if (Recovery < 0)
		{
			// The latch is already STRANDED for the cooldown (TickOffMeshRecovery, F5-2).
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f island egress FAILED — off-mesh recovery never reached the mesh (%.1fs) — latch cleared, %.0fs cooldown (F7)."),
				*Bot->GetName(), Now, InstanceData.RecoverySeconds, Tier.EgressCooldownSeconds);
			Bot->NoteCurrentAmbitionFailed();
			return EStateTreeRunStatus::Failed;
		}
		return BeginEgress(*Bot, *Pawn, InstanceData);
	}
	if (!InstanceData.bBegun)
	{
		// The ground came back (a grapple ride ended on the gantry top): the same two
		// doors EnterState uses — recover onto the mesh first, then fan.
		switch (StartOffMeshRecovery(*Bot, *Pawn, InstanceData.RecoveryTarget))
		{
		case EAIBOffMeshRecovery::Started:
			Bot->MarkEgressMove();
			InstanceData.bRecovering = true;
			InstanceData.RecoverySeconds = 0.f;
			return EStateTreeRunStatus::Running;
		case EAIBOffMeshRecovery::Failed:
			Bot->NoteCurrentAmbitionFailed();
			return EStateTreeRunStatus::Failed;
		case EAIBOffMeshRecovery::OnMesh:
			break;
		}
		return BeginEgress(*Bot, *Pawn, InstanceData);
	}

	// LANDED. Below the lip by the drop number = off the island: the parsed egress line
	// and the latch cleared. At the lip's height = the step-off never left this island
	// (a rail, a thin ledge past the boundary): cleared with the cooldown, so Wander
	// walks rather than re-latching here.
	if (InstanceData.bAirborneSeen)
	{
		FAIBIslandLatch& Latch = Bot->GetIslandLatch();
		const float StrandedSeconds = InstanceData.StrandedSinceSeconds >= 0.0 ? static_cast<float>(Now - InstanceData.StrandedSinceSeconds) : 0.f;
		const FVector FeetNow = FeetOf(*Pawn);
		const float DropUU = InstanceData.Lip.Z - FeetNow.Z; // F5-3: lip (nav) vs the FEET
		if (AIB::LandedOnSameIsland(InstanceData.Footprint, FeetNow))
		{
			// F7-3: inside the fan's footprint at the step-off's height — the same island. The
			// cooldown lets Wander re-draw; the next fan skips this lip (F7-1), never re-issued.
			Latch.ClearWithCooldown(Now, Tier.EgressCooldownSeconds);
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f island egress FAILED — landed on the same island (F7) — latch cleared, %.0fs cooldown."),
				*Bot->GetName(), Now, Tier.EgressCooldownSeconds);
			return EStateTreeRunStatus::Failed;
		}
		if (DropUU >= Tier.IslandMinDropUU)
		{
			Latch.Clear();
			LogIslandEgress(*Bot, TEXT("drop"), InstanceData.Lip, StrandedSeconds);
			return EStateTreeRunStatus::Succeeded;
		}
		Latch.ClearWithCooldown(Now, Tier.EgressCooldownSeconds);
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f island egress FAILED — landed %.0fuu below the lip, still on it — latch cleared, %.0fs cooldown (F7)."),
			*Bot->GetName(), Now, DropUU, Tier.EgressCooldownSeconds);
		return EStateTreeRunStatus::Failed;
	}

	// AT THE LIP: AIB19's step-off, verbatim in spirit — pathfinding OFF, no projection,
	// a straight line past the boundary toward the lower ground. Re-issued only when the
	// mover is not already carrying it (a step that ended on the ground gets pushed again
	// until the timeout says the body cannot leave here).
	const bool bMoverIdle = Bot->GetMoveStatus() == EPathFollowingStatus::Idle;
	const bool bAtLip = InstanceData.bAtLipOnEntry // F7-2
		|| IsWithin(*Bot, InstanceData.Lip, InstanceData.LipReachUU)
		|| (bMoverIdle && IsWithin(*Bot, InstanceData.Lip, InstanceData.LipReachUU * 1.5f));
	if (bAtLip)
	{
		if (!InstanceData.bSteppedOff || bMoverIdle)
		{
			ReleaseLocomotion(*Bot); // a walking step, not a sprinting leap
			Bot->MoveToLocation(InstanceData.Beyond, /*AcceptanceRadius=*/80.f,
				/*bStopOnOverlap=*/true, /*bUsePathfinding=*/false,
				/*bProjectDestinationToNavigation=*/false, /*bCanStrafe=*/true);
			Bot->MarkEgressMove(); // R8: the step-off is Egress's own move too
			InstanceData.bSteppedOff = true;
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s steps off the island's lip."), *Bot->GetName());
		}
		if ((InstanceData.LipSeconds += DeltaTime) >= InstanceData.DropTimeoutSeconds)
		{
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f island egress FAILED — %.1fs at the lip, never left the ground — latch cleared, %.0fs cooldown (F7)."),
				*Bot->GetName(), Now, InstanceData.LipSeconds, Tier.EgressCooldownSeconds);
			Bot->GetIslandLatch().ClearWithCooldown(Now, Tier.EgressCooldownSeconds);
			return EStateTreeRunStatus::Failed;
		}
		return EStateTreeRunStatus::Running;
	}

	// WALKING TO THE LIP: the ordinary mover bookkeeping. The short-path read is lane A's
	// M3 shape, not a no-progress ratchet: 0.5s since ENTER and Idle for 0.3s CONSECUTIVE
	// — a single Idle frame between a detour's path and its re-issue is not a finished path.
	InstanceData.MoverIdleSeconds = bMoverIdle ? InstanceData.MoverIdleSeconds + DeltaTime : 0.f;
	const float DistNow = FVector::Dist(Here, InstanceData.Lip);
	if (DistNow < InstanceData.ClosestSoFarUU - 1.f)
	{
		InstanceData.ClosestSoFarUU = DistNow;
		InstanceData.SecondsWithoutProgress = 0.f;
	}
	else
	{
		InstanceData.SecondsWithoutProgress += DeltaTime;
	}
	const bool bMoverDoneShort = InstanceData.SecondsSinceEnter >= 0.5f && InstanceData.MoverIdleSeconds >= 0.3f;
	const bool bStalled = TickLocomotion(*Bot, InstanceData.Lip, InstanceData.LipReachUU, DeltaTime, InstanceData.GiveUpAfterNoProgressSeconds);
	if (bMoverDoneShort || bStalled || InstanceData.SecondsWithoutProgress >= InstanceData.GiveUpAfterNoProgressSeconds)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f island egress FAILED — lip %.0fuu short (mover %s) — latch cleared, %.0fs cooldown (F7)."),
			*Bot->GetName(), Now, DistNow, bMoverDoneShort ? TEXT("idle") : TEXT("stalled"), Tier.EgressCooldownSeconds);
		Bot->GetIslandLatch().ClearWithCooldown(Now, Tier.EgressCooldownSeconds);
		Bot->NoteCurrentAmbitionFailed(); // R2/R3
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

void FAIBEgressTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	if (!Bot)
	{
		return;
	}
	ReleaseLocomotion(*Bot);
	Bot->SetEgressMoveInFlight(false); // F6-2: the id mark (R8) stays as belt for a step-off still falling
	// NEVER CANCELS A FALL (critic M5): a body in the air finishes arriving like a human
	// would; only a grounded walk is stopped. Airborne, the unpathed step-off request is
	// handed to the controller's one-shot (W-REVIEW M6): stopped on the first grounded
	// sample unless a newer move owns the mover by then. The latch stays — a landing, a
	// full draw, or a completed full-path move clears it from the world, not this exit.
	const IAIBAvatarInterface* Avatar = Bot->GetAvatar();
	if (!Avatar || Avatar->IsGrounded())
	{
		Bot->StopMovement();
	}
	else
	{
		Bot->ArmStopOnLanding();
	}
}

////////////////////////////////////////////////////////////////////

namespace
{
	/** ONE STRAFE LEG'S STEP, actuated: the policy's worldless arc geometry
	 *  (FAIBMovementPolicy::ArcStep — range-invariant, arc-capped, re-banded) fed with the
	 *  leg actually in flight and issued as a navmesh-projected move. Shared by the
	 *  Engage/Retreat strafe task (pivot = the belief) and the Mode hill hold (pivot = the
	 *  objective centre, Phase 13 / AIB22 LOW-7).
	 *
	 *  FILL THE LEG. One step is issued per leg, so a constant distance can only ever suit
	 *  one rung: at 600uu/s a leg covers 210..1200uu across the ladder, against the old
	 *  fixed 220 — tuned for Expert's SHORTEST leg, leaving every other rung standing for
	 *  60-80% of its own leg. Derive it from the leg actually in flight instead. The arc is
	 *  capped, not the distance: a long leg at close range would otherwise swing the bot
	 *  most of the way around the pivot, which reads as orbiting, not footwork.
	 *
	 *  THE SPIRAL FIX (founder's strafe review, 26 Aug), rebanded for the fight range.
	 *  Only the ENDPOINTS of an arc step sit on the range circle — the walk between them
	 *  is the CHORD, dipping inward by R(1-cos(arc/2)) at midpoint. Legs are TIME-driven
	 *  and routinely expire mid-chord, so the next leg re-measures range from the dip and
	 *  keeps it. Over the wide fight range the ratchet is kept ON PURPOSE as gradual
	 *  pressure — a jinking bot slowly working closer is the Halo read — and the floor is
	 *  what stops it at stand-off instead of at melee-accident range. Returns the
	 *  destination through OutGoal when a move was issued. */
	bool StrafeArcStep(AAIBBotController& Bot, const APawn& Pawn, const FVector& Pivot,
		EAIBStrafeIntent Intent, const FAIBStrafeTaskInstanceData& Params,
		float MinRangeUU, float MaxRangeUU, FVector& OutGoal)
	{
		const FAIBMovementState& MovementState = Bot.GetMovementState();
		const float LegRemainingSeconds = FMath::Max(0.f,
			static_cast<float>(MovementState.NextDecisionAtSeconds - WorldSeconds(Bot)));
		float StepUU = Params.StepDistanceUU;
		if (const UPawnMovementComponent* MoveComp = Pawn.GetMovementComponent())
		{
			const float Speed = MoveComp->GetMaxSpeed();
			if (Speed > 0.f && LegRemainingSeconds > 0.f)
			{
				StepUU = FMath::Max(Params.StepDistanceUU, Speed * LegRemainingSeconds);
			}
		}
		FAIBArcStep Step;
		if (!FAIBMovementPolicy::ArcStep(Pawn.GetActorLocation(), Pivot, Pawn.GetActorForwardVector(),
				Intent == EAIBStrafeIntent::Right, StepUU, Params.MaxArcDegrees, MinRangeUU, MaxRangeUU, Step))
		{
			return false;
		}
		// Projected onto the navmesh by the move itself: a step into a wall or off a ledge
		// resolves to the nearest legal point instead of failing (the host's proven call).
		if (Bot.MoveToLocation(Step.Destination, /*AcceptanceRadius=*/50.f, /*bStopOnOverlap=*/true,
				/*bUsePathfinding=*/true, /*bProjectDestinationToNavigation=*/true, /*bCanStrafe=*/true)
			== EPathFollowingRequestResult::Failed)
		{
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s strafe step refused — holding this leg."), *Bot.GetName());
			return false;
		}
		// The measurement the founder's report needed and nobody had: how far one leg
		// actually carries the bot, and at what range. Range is printed because the arc
		// is supposed to hold it constant — a drifting range means the geometry is wrong.
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s strafe leg — %.0fuu of arc at range %.0fuu (%.0f deg, %.2fs left)."),
			*Bot.GetName(), Step.ArcRadians * Step.RangeUU, Step.RangeUU,
			FMath::RadiansToDegrees(Step.ArcRadians), LegRemainingSeconds);
		OutGoal = Step.Destination;
		return true;
	}

	/** THE HILL IS HELD WITH FOOTWORK (Phase 13, AIB22 LOW-7: "Hold is a label on standing
	 *  still"). Mode on its objective runs the SAME rhythm as the fight — the policy's
	 *  strafe legs on the controller's per-life state, a planted leg the named StrafeHold
	 *  with the existing StopMovement asymmetry — about the objective centre instead of a
	 *  belief, on a ring inside the objective's reach so no chord dip leaves it. No mover
	 *  and no stall clock: a plant is a plant, not a wedge. The Hold flag stays up beside
	 *  it so SweepLook's unbudgeted guard scan owns the planted legs. */
	void HoldHillWithFootwork(AAIBBotController& Bot, APawn& Pawn,
		FAIBMoveToObjectiveTaskInstanceData& InstanceData, float DeltaTime)
	{
		FAIBMovementState& MovementState = Bot.GetMovementState();
		const EAIBStrafeIntent Intent = FAIBMovementPolicy::StepStrafe(
			MovementState, Bot.GetSkillProfile().Level(EAIBSkill::Movement),
			Bot.GetPolicyRandom(), WorldSeconds(Bot));
		if (MovementState.NextDecisionAtSeconds != MovementState.LastActuatedLegStamp)
		{
			MovementState.LastActuatedLegStamp = MovementState.NextDecisionAtSeconds;
			Bot.SetStillTactic(EAIBStillTactic::StrafeHold, Intent == EAIBStrafeIntent::Hold);
			if (Intent == EAIBStrafeIntent::Hold)
			{
				Bot.StopMovement();
			}
			else
			{
				// The strafe task's own node defaults (step floor, arc cap) — one ladder,
				// not a second one restated for the hill.
				const FAIBStrafeTaskInstanceData Footwork;
				const float RingUU = InstanceData.GoalReachUU * FMath::Clamp(Bot.GetTierRow().HillStrafeRadiusFraction, 0.f, 1.f);
				if (!InstanceData.bRingSlotTaken)
				{
					// W-REVIEW M5: the first leg goes to THIS bot's seeded slot on the ring
					// (the same LifeSeed phase as the approach ring), so two holders arriving
					// on one bearing never orbit in lockstep; the arc steps take over from it.
					InstanceData.bRingSlotTaken = true;
					InstanceData.StrafeLegGoal = InstanceData.Goal
						+ FVector(RingUU, 0.f, 0.f).RotateAngleAxis(Bot.GetRingPhaseDeg(), FVector::UpVector);
					MoveToNavPoint(Bot, InstanceData.StrafeLegGoal, 50.f);
				}
				else
				{
					StrafeArcStep(Bot, Pawn, InstanceData.Goal, Intent, Footwork, RingUU * 0.5f, RingUU, InstanceData.StrafeLegGoal);
				}
			}
		}
		if (!Bot.HasStillTactic(EAIBStillTactic::StrafeHold))
		{
			FaceTravel(Bot, Pawn, InstanceData.StrafeLegGoal, DeltaTime);
		}
	}

	/** The objective pick, callable from Tick as well as EnterState (BN22 W-REVIEW H1:
	 *  a goal snapshotted once was written for a hill that never moves, and Rally POIs
	 *  are PAWNS — a bot "arrived" at a teammate's abandoned spot stood there forever
	 *  while the live urgency kept the want winning). Worth ties break NEAREST (L2:
	 *  uniform-Worth ally POIs picked in iterator order sent a bot across the map past
	 *  a 700uu teammate). Returns false when no POI of the kind survives the filters. */
	bool PickObjectiveGoal(AAIBBotController& Bot, const APawn& Pawn,
		FAIBMoveToObjectiveTaskInstanceData& InstanceData)
	{
		InstanceData.bHasGoal = false;
		const FGameplayTag Kind = Bot.GetObjectiveKindForCurrentAmbition();
		IAIBWorldQuery* Query = Bot.GetWorldQuery();
		if (!Query)
		{
			return false;
		}

		// Phase 7: the scoring seam has a task-side mirror — a claim honoured at the
		// want but not at the pick is a bot that walks to a claimed slot anyway
		// (multi-slot kinds keep the want alive while one slot is spoken for). Self
		// passes through IsClaimedByOtherTeammate by definition; the gate matches the
		// builder's (a Novice's board is nobody's business here either — the
		// coordinator read below is what the builder's Teamwork gate already allowed
		// or refused at scoring time, mirrored on the same profile read).
		const UAIBTeamCoordinator* Coordinator =
			Bot.GetSkillProfile().Level(EAIBSkill::Teamwork) >= EAIBCompetence::Trained
				? (Bot.GetWorld() ? Bot.GetWorld()->GetSubsystem<UAIBTeamCoordinator>() : nullptr)
				: nullptr;

		TArray<FAIBPointOfInterest> Points;
		Query->QueryPointsOfInterest(&Pawn, AIB::ObjectiveQueryRadiusUU, Points);
		float BestWorth = -1.f;
		float BestDistSq = TNumericLimits<float>::Max();
		for (const FAIBPointOfInterest& Point : Points)
		{
			if (Kind.IsValid() && Point.Kind != Kind)
			{
				continue;
			}
			if (Coordinator && Point.bClaimableSlot
				&& Coordinator->IsClaimedByOtherTeammate(Bot, Point))
			{
				continue;
			}
			const float DistSq = static_cast<float>(FVector::DistSquared(Pawn.GetActorLocation(), Point.Location));
			if (Point.Worth > BestWorth || (Point.Worth == BestWorth && DistSq < BestDistSq))
			{
				BestWorth = Point.Worth;
				BestDistSq = DistSq;
				InstanceData.Goal = Point.Location;
				InstanceData.bHasGoal = true;
				// Never SHRINK the arrival test below the mover's own floor — a provider
				// publishing 0 (a point objective) must not make arrival impossible.
				InstanceData.GoalReachUU = FMath::Max(InstanceData.AcceptanceRadiusUU, Point.ReachRadiusUU);
			}
		}
		return InstanceData.bHasGoal;
	}
}

EStateTreeRunStatus FAIBMoveToObjectiveTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	// The kind join: the CURRENT ambition's ObjectiveKind from the controller's cached
	// mode set (data, never a serialized node parameter), then the best matching POI
	// the world query offers. A servable want with no POI is the provider
	// under-delivering — loud (F7), and the 1s failed-delay keeps it cheap.
	PickObjectiveGoal(*Bot, *Pawn, InstanceData);
	InstanceData.RepollCooldown = InstanceData.RepollIntervalSeconds;

	if (!InstanceData.bHasGoal)
	{
		const FGameplayTag Kind = Bot->GetObjectiveKindForCurrentAmbition();
		UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s won a mode want but %s offered no POI of kind %s — branch fails (F7)."),
			*Bot->GetName(), Bot->GetWorldQuery() ? TEXT("the world query") : TEXT("NO world query is registered and it"),
			Kind.IsValid() ? *Kind.ToString() : TEXT("<any>"));
		// The deadlock of record: with the hill registered 2ms after possession, seven
		// bots won this want, landed here, and never made another decision for four
		// minutes. Reporting it is what lets the want go quiet and the bot get on with
		// the match.
		Bot->NoteCurrentAmbitionFailed();
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ClosestSoFarUU = FVector::Dist(Pawn->GetActorLocation(), InstanceData.Goal);
	InstanceData.SecondsWithoutProgress = 0.f;
	if (!IsWithin(*Bot, InstanceData.Goal, InstanceData.GoalReachUU)
		&& MoveToNavPoint(*Bot, InstanceData.Goal, InstanceData.GoalReachUU)
			== EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s cannot path to the objective — failing loudly (F7). %s"),
			*Bot->GetName(), *DescribeMoveFailure(*Bot, InstanceData.Goal));
		Bot->NoteCurrentAmbitionFailed();
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBMoveToObjectiveTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn || !InstanceData.bHasGoal)
	{
		return EStateTreeRunStatus::Failed;
	}

	// THE GOAL FOLLOWS THE POI (BN22 W-REVIEW H1). The snapshot was correct for a hill;
	// a Rally POI is a pawn, and standing at a teammate's ABANDONED spot while the live
	// urgency keeps the want winning was a 10-60s statue — the isolation collapse this
	// want exists to fix, re-created as standing. Re-pick on the repath cadence: the
	// hill re-picks itself (byte-identical goal, nothing resets), a moved ally re-aims
	// the walk. Progress tracking resets ONLY when the goal actually moved — a re-poll
	// that reset it every time would neuter the no-progress give-up law. A pick that
	// comes back empty means the provider stopped offering (the last ally died): fail
	// loudly, the F7 shape, and suppression rests the want.
	InstanceData.RepollCooldown -= DeltaTime;
	if (InstanceData.RepollCooldown <= 0.f)
	{
		InstanceData.RepollCooldown = InstanceData.RepollIntervalSeconds;
		const FVector OldGoal = InstanceData.Goal;
		if (!PickObjectiveGoal(*Bot, *Pawn, InstanceData))
		{
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s's objective POI is gone — the branch fails with it (F7)."),
				*Bot->GetName());
			Bot->NoteCurrentAmbitionFailed();
			return EStateTreeRunStatus::Failed;
		}
		if (FVector::DistSquared(OldGoal, InstanceData.Goal) > FMath::Square(50.f))
		{
			// Re-seed the RATCHET against the new goal — progress toward a post that moved
			// must be measured from where it moved to.
			InstanceData.ClosestSoFarUU = FVector::Dist(Pawn->GetActorLocation(), InstanceData.Goal);

			// But do NOT reset the stall CLOCK here. It used to, and that silently disabled
			// the give-up entirely: the goal is a live teammate, the repoll runs every
			// RepollIntervalSeconds (0.5s), and any teammate who is walking, strafing or
			// fighting moves more than 50uu in that time — so the clock was zeroed ~16
			// times before it could ever reach GiveUpAfterNoProgressSeconds (8s).
			// NoteCurrentAmbitionFailed() below became unreachable, and a bot walled off
			// from its team stood there for the rest of the match with no F7 line to say
			// so. Under teams-by-default that is a silent, permanent deadlock.
			//
			// The clock is now cleared in exactly one place — the genuine-progress branch
			// below — so a bot that is actually closing keeps its reprieve and a bot that
			// is not eventually gives up and lets another want run.
			if (!IsWithin(*Bot, InstanceData.Goal, InstanceData.GoalReachUU))
			{
				MoveToNavPoint(*Bot, InstanceData.Goal, InstanceData.GoalReachUU);
			}
		}
	}

	// ON the objective: STAND — a hill is held by being there, the founder's named
	// tactical exception to F9 (W-REVIEW H2), so it is flagged Hold: the idle gate reads
	// it as a tactic and SweepLook beside it runs the unbudgeted slow scan instead of a
	// budget that spends once and freezes the head. The sentinel or the want's own decay
	// ends the branch, never arrival.
	// PHASE 13 (AIB22 LOW-7): the hill is held with FOOTWORK — the fight's strafe legs
	// about the objective centre, planted legs named StrafeHold; the Hold flag stays up
	// for SweepLook's guard scan. One `hill strafe-hold` line per arrival; a leg whose
	// projection carried the body off the hill walks back through the ordinary mover.
	const bool bOnObjective = IsWithin(*Bot, InstanceData.Goal, InstanceData.GoalReachUU);
	Bot->SetStillTactic(EAIBStillTactic::Hold, bOnObjective);
	if (bOnObjective != InstanceData.bWasOnObjective)
	{
		InstanceData.bWasOnObjective = bOnObjective;
		if (bOnObjective)
		{
			InstanceData.StrafeLegGoal = Pawn->GetActorLocation();
			UE_LOG(LogAIBot, Log, TEXT("AIBot: %s t=%.1f hill strafe-hold — ring %.0fuu of reach %.0fuu at (%.0f,%.0f,%.0f)"),
				*Bot->GetName(), WorldSeconds(*Bot),
				InstanceData.GoalReachUU * Bot->GetTierRow().HillStrafeRadiusFraction, InstanceData.GoalReachUU,
				InstanceData.Goal.X, InstanceData.Goal.Y, InstanceData.Goal.Z);
		}
		else
		{
			Bot->SetStillTactic(EAIBStillTactic::StrafeHold, false);
			InstanceData.bRingSlotTaken = false; // the next arrival takes its slot afresh
			InstanceData.ClosestSoFarUU = FVector::Dist(Pawn->GetActorLocation(), InstanceData.Goal);
			MoveToNavPoint(*Bot, InstanceData.Goal, InstanceData.GoalReachUU);
		}
	}
	if (bOnObjective)
	{
		HoldHillWithFootwork(*Bot, *Bot->GetPawn(), InstanceData, DeltaTime);
		return EStateTreeRunStatus::Running;
	}

	// Short of it: sprint the crossing, give up LOUDLY if the post is unreachable (the
	// movers' shared no-progress law). Fix #4 R5: short of the objective is NOT a hold —
	// an objective a storey away with no link fails here at once (R3) and the want rests.
	if (TickLocomotion(*Bot, InstanceData.Goal, InstanceData.GoalReachUU, DeltaTime, InstanceData.GiveUpAfterNoProgressSeconds))
	{
		Bot->NoteCurrentAmbitionFailed();
		return EStateTreeRunStatus::Failed;
	}
	const float DistNow = FVector::Dist(Pawn->GetActorLocation(), InstanceData.Goal);
	if (DistNow < InstanceData.ClosestSoFarUU - 1.f)
	{
		InstanceData.ClosestSoFarUU = DistNow;
		InstanceData.SecondsWithoutProgress = 0.f;
	}
	else if ((InstanceData.SecondsWithoutProgress += DeltaTime) >= InstanceData.GiveUpAfterNoProgressSeconds)
	{
		UE_LOG(LogAIBot, Log, TEXT("AIBot: %s cannot reach the objective — giving up loudly (F7). %s"),
			*Bot->GetName(), *DescribeMoveFailure(*Bot, InstanceData.Goal));
		Bot->NoteCurrentAmbitionFailed();
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

void FAIBMoveToObjectiveTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->SetStillTactic(EAIBStillTactic::Hold, false);
		Bot->SetStillTactic(EAIBStillTactic::StrafeHold, false);
		ReleaseLocomotion(*Bot);
		Bot->StopMovement();
	}
}

////////////////////////////////////////////////////////////////////

namespace
{
	/** AIB10's spell instrument, closing half: one line with the DURATION when a gate-hold
	 *  spell ends, whatever ends it — re-entering the circle or losing the target. The
	 *  harness sums the seconds; the reason says which door closed the spell. */
	void EndStrafeGateSpell(AAIBBotController& Bot, const TCHAR* Reason)
	{
		FAIBMovementState& MovementState = Bot.GetMovementState();
		if (!MovementState.bStrafeOutsideGate)
		{
			return;
		}
		MovementState.bStrafeOutsideGate = false;
		const double OutsideSeconds =
			Bot.GetWorld()->GetTimeSeconds() - MovementState.StrafeOutsideSinceSeconds;
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s strafe opportunity back — %.1fs outside (%s)."),
			*Bot.GetName(), OutsideSeconds, Reason);
	}
}

EStateTreeRunStatus FAIBStrafeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAIBStrafeTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
	if (!Pawn || !Bot->GetSensorium().HasVisibleTarget())
	{
		// Not this task's failure — the belief tasks beside it own the state's fate.
		// Keep running and simply stop stepping (the host's own strafe rule). A gate-hold
		// spell in flight ends here: "denied opportunity" only means anything while there
		// is a visible target to strafe against.
		if (Bot)
		{
			EndStrafeGateSpell(*Bot, TEXT("target lost"));
			Bot->SetStillTactic(EAIBStillTactic::StrafeHold, false);
		}
		return EStateTreeRunStatus::Running;
	}

	// The whole visible fight range is footwork's (founder, 27 Aug): outside FightRangeUU
	// the mover owns the legs and this task holds. The log wording below is FROZEN — the
	// harness's strafe_hold regex transcribes it, and "the engaged radius" now names this
	// wider gate.
	const FVector Belief = Bot->GetSensorium().GetLastSeenLocation();
	// THE BAND'S FLOOR (Retreat only; 0 in Engage). Inside it the flee mover is still
	// breaking contact and owns the legs — one mover per tick, always.
	if (InstanceData.MinRangeUU > 0.f && IsWithin(*Bot, Belief, InstanceData.MinRangeUU))
	{
		return EStateTreeRunStatus::Running;
	}
	if (!IsWithin(*Bot, Belief, InstanceData.FightRangeUU))
	{
		// THE SPELL EDGE (AIB10 re-instrument): the first cut logged this hold EVERY TICK
		// while the leg line fired once per leg, and the "182:1 gated out" measurement was
		// those two rates divided — frames over legs, meaningless. One line per SPELL at
		// the edge (the closing line below carries the duration) makes holds and legs
		// commensurate: hold spells vs stepped legs, plus denied-seconds the harness sums.
		// A hold is not a failure, but it must be countable, or "the strafe is too short"
		// has no measurement behind it.
		Bot->SetStillTactic(EAIBStillTactic::StrafeHold, false); // the mover has the legs
		FAIBMovementState& MovementState = Bot->GetMovementState();
		if (!MovementState.bStrafeOutsideGate)
		{
			MovementState.bStrafeOutsideGate = true;
			MovementState.StrafeOutsideSinceSeconds = Bot->GetWorld()->GetTimeSeconds();
			UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s strafe held — outside the engaged radius (range %.0fuu > %.0fuu)."),
				*Bot->GetName(), FVector::Dist(Pawn->GetActorLocation(), Belief), InstanceData.FightRangeUU);
		}
		return EStateTreeRunStatus::Running;
	}
	EndStrafeGateSpell(*Bot, TEXT("reentered"));

	// The policy decides the rhythm (per-life state on the controller — a branch blink
	// must not reset the dance); this task actuates ONE step per leg.
	const double Now = Bot->GetWorld()->GetTimeSeconds();
	const EAIBStrafeIntent Intent = FAIBMovementPolicy::StepStrafe(
		Bot->GetMovementState(), Bot->GetSkillProfile().Level(EAIBSkill::Movement),
		Bot->GetPolicyRandom(), Now);
	// The stamp lives on the CONTROLLER's movement state (W-REVIEW P4+5 H1): instance
	// data resets on every Engage re-entry, and a reset stamp re-actuated the same leg
	// once per belief blink — up to five 220uu steps in a leg authored as one.
	FAIBMovementState& MovementState = Bot->GetMovementState();
	if (MovementState.NextDecisionAtSeconds == MovementState.LastActuatedLegStamp)
	{
		return EStateTreeRunStatus::Running; // this leg is already actuated, hold or move
	}
	MovementState.LastActuatedLegStamp = MovementState.NextDecisionAtSeconds;
	// F9 (AIB22): a planted leg is a NAMED stillness for exactly one leg.
	Bot->SetStillTactic(EAIBStillTactic::StrafeHold, Intent == EAIBStrafeIntent::Hold);

	// A HOLD IS AN ACTUATION TOO (founder's strafe review, 26 Aug). The policy's hold
	// windows only ever suppressed NEW steps — the previous leg's move request kept
	// walking right through them, so the "plant" the ladder authors (a Novice stands
	// 19 windows in 20) never actually read on a body mid-chord. One StopMovement per
	// hold leg makes the stand real; safe, because inside the engaged circle this task
	// is the only mover (MoveNearBelief stations, it does not move).
	if (Intent == EAIBStrafeIntent::Hold)
	{
		Bot->StopMovement();
		UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s strafe hold — planted for this leg."), *Bot->GetName());
		return EStateTreeRunStatus::Running;
	}

	// THE COMBAT DASH. The scatter proved the verb works; this is what makes it visible,
	// because grenades are rare and a dash nobody sees may as well not exist.
	//
	// Runs in BOTH branches, unlike the hop: dashing is how a fight stops being two bodies
	// sliding left and right at each other, and a bot that only dashes while retreating
	// reads as a bot that only panics. The direction is the LEG's — the dash launches along
	// the movement input the step already set, so it lengthens the footwork rather than
	// arguing with it.
	//
	// Gated on the bot's own throttle FIRST and the tier roll second: the throttle is what
	// actually bounds the rate (one dash per 3.5s however eager the tier), and asking it
	// first is what keeps this off the futile-press path when the host would refuse.
	if (Bot->CanDash()
		&& Bot->GetPolicyRandom().FRand() < FAIBMovementPolicy::DashChance(
			Bot->GetSkillProfile().Level(EAIBSkill::Movement)))
	{
		if (IAIBAvatarInterface* DashAvatar = Bot->GetAvatar())
		{
			if (DashAvatar->IsGrounded())
			{
				DashAvatar->PressVerb(AIBTags::Verb_Dash);
				DashAvatar->ReleaseVerb(AIBTags::Verb_Dash);
				Bot->NoteDashed(AIB::DashThrottleSeconds);
				UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s dashed on the strafe leg."), *Bot->GetName());
			}
		}
	}

	// THE EVASIVE HOP. A defending bot that only slides left and right is a predictable
	// target on a flat plane; breaking the vertical is what spoils a tracking aim
	// (founder, 28 Aug: "crouch jump, evade, fire").
	//
	// ITS OWN CHANCE, not a rider on the juke. The first cut fired only on juke legs so it
	// would inherit JukeChance's tier gate for free — but that gate is 0.00 below Skilled,
	// so the jump was structurally impossible at Marine, the tier actually being played.
	// Measured: 9 defend stand-downs and 0 hops in a full match. HopChance is nonzero on
	// every rung and still rises with competence, so it is capability-shaped without being
	// capability-BLOCKED (R28 wants tiers to differ, not to silence a behaviour).
	//
	// DEFEND ONLY (MinRangeUU > 0 is Retreat's band, 0 in Engage). Engage's footwork is
	// unchanged, byte for byte, which keeps every strafe measurement in the AIB tickets
	// comparable rather than silently re-baselined.
	if (InstanceData.MinRangeUU > 0.f
		&& Bot->GetPolicyRandom().FRand() < FAIBMovementPolicy::HopChance(
			Bot->GetSkillProfile().Level(EAIBSkill::Movement)))
	{
		if (IAIBAvatarInterface* Avatar = Bot->GetAvatar())
		{
			// Grounded only: pressing jump mid-air is a no-op the ability system still has
			// to refuse, and a bot that spams it reads as a stuck key.
			if (Avatar->IsGrounded())
			{
				Avatar->PressVerb(AIBTags::Verb_Jump);
				Avatar->ReleaseVerb(AIBTags::Verb_Jump);
				UE_LOG(LogAIBot, Verbose, TEXT("AIBot: %s defensive juke — hopping the direction change."), *Bot->GetName());
			}
		}
	}

	// ON AN ARC AROUND THE BELIEF, not perpendicular to it. A perpendicular step always
	// LENGTHENS range — from distance d a lateral L lands at sqrt(d^2 + L^2) — so it
	// walks itself out of this task's own FightRange gate and MoveNearBelief drags it
	// back. Rotating the bot's own bearing about the belief keeps range CONSTANT by
	// construction (StrafeArcStep; the geometry is the policy's, worldless and spec'd),
	// and it is what strafing physically is. Standing ON the belief, the body's forward
	// is the bearing (Phase 13) — the old early return stood there, which F9 forbids.
	FVector LegGoal;
	StrafeArcStep(*Bot, *Pawn, Belief, Intent, InstanceData,
		FMath::Min(InstanceData.StandOffMinUU, InstanceData.FightRangeUU), InstanceData.FightRangeUU, LegGoal);
	return EStateTreeRunStatus::Running;
}

void FAIBStrafeTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller))
	{
		Bot->SetStillTactic(EAIBStillTactic::StrafeHold, false);
	}
}

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FAIBUnservedWantTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const AAIBBotController* Bot = ResolveBot(Context, InstanceData.Controller);
	const UAIBAmbitionEngine* Engine = Bot ? Bot->GetAmbitionEngine() : nullptr;
	// F7 at default verbosity: the one line that turns "mystery statue" into a named
	// gap. The sentinel beside this task ends the branch when the want changes.
	UE_LOG(LogAIBot, Warning, TEXT("AIBot: %s wants '%s' and NO branch serves it — standing until the want changes (F7)."),
		Bot ? *Bot->GetName() : TEXT("<no controller>"),
		Engine && Engine->GetCurrent().IsValid() ? *Engine->GetCurrent().ToString() : TEXT("<none>"));
	return EStateTreeRunStatus::Running;
}
