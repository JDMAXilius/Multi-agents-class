#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AIBBotController.h"
#include "Core/AIBTypes.h"
#include "Data/AIBDataRows.h"
#include "Skills/AIBMovementPolicy.h"

/**
 * Phase 13 (AIB24), proven headless. The strafe step's geometry is the policy's and
 * worldless: an arc step keeps range (1), caps the arc (2), re-bands into the stand-off
 * band (3), mirrors left/right (4), and standing ON the pivot steps off along the body's
 * forward — never stands (5). The hill ring at the row's fraction keeps both endpoints and
 * the chord midpoint inside the objective's reach (6). The overlap episode opens on the
 * first sample with an ally inside, keeps the peak, closes on the first without, and a
 * body-gone close reports what was open (7-8). The row defaults are what the table must
 * mirror (9).
 */
BEGIN_DEFINE_SPEC(FAIBSeparationSpec, "AIBot.Sim.Separation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

	static float Range2D(const FVector& A, const FVector& B) { return static_cast<float>(FVector::Dist2D(A, B)); }

END_DEFINE_SPEC(FAIBSeparationSpec)

void FAIBSeparationSpec::Define()
{
	const FVector Pivot(1000.f, 2000.f, 50.f);
	const FVector Forward(1.f, 0.f, 0.f);

	It("keeps range under an arc step — the endpoint sits on the same circle", [this, Pivot, Forward]()
	{
		const FVector From = Pivot + FVector(500.f, 0.f, 0.f);
		FAIBArcStep Step;
		TestTrue(TEXT("stepped"), FAIBMovementPolicy::ArcStep(From, Pivot, Forward, true, 220.f, 55.f, 280.f, 900.f, Step));
		TestEqual(TEXT("range invariant"), (double)Range2D(Step.Destination, Pivot), 500.0, 0.5);
		TestEqual(TEXT("arc = step / range"), Step.ArcRadians, 220.f / 500.f, 1e-4f);
		TestEqual(TEXT("z is the pivot's"), (double)Step.Destination.Z, (double)Pivot.Z, 1e-3);
		// The chord this step walks is StepUU long to first order.
		TestEqual(TEXT("chord ~ step"), Range2D(Step.Destination, From), 2.f * 500.f * FMath::Sin(0.22f), 1.f);
	});

	It("caps the arc, not the distance — a long leg at close range does not orbit", [this, Pivot, Forward]()
	{
		const FVector From = Pivot + FVector(0.f, 300.f, 0.f);
		FAIBArcStep Step;
		TestTrue(TEXT("stepped"), FAIBMovementPolicy::ArcStep(From, Pivot, Forward, false, 2000.f, 55.f, 280.f, 900.f, Step));
		TestEqual(TEXT("arc capped at 55 deg"), Step.ArcRadians, FMath::DegreesToRadians(55.f), 1e-4f);
		TestEqual(TEXT("range still invariant"), Range2D(Step.Destination, Pivot), 300.f, 0.5f);
	});

	It("re-bands the range into [min, max] — the spiral fix and the fight-range ceiling", [this, Pivot, Forward]()
	{
		FAIBArcStep Inside, Outside;
		TestTrue(TEXT("inside stepped"), FAIBMovementPolicy::ArcStep(Pivot + FVector(100.f, 0.f, 0.f), Pivot, Forward, true, 220.f, 55.f, 280.f, 900.f, Inside));
		TestEqual(TEXT("a dip renormalises OUT to the floor"), Range2D(Inside.Destination, Pivot), 280.f, 0.5f);
		TestTrue(TEXT("outside stepped"), FAIBMovementPolicy::ArcStep(Pivot + FVector(1200.f, 0.f, 0.f), Pivot, Forward, true, 220.f, 55.f, 280.f, 900.f, Outside));
		TestEqual(TEXT("beyond the ceiling clamps to it"), Range2D(Outside.Destination, Pivot), 900.f, 0.5f);
		TestEqual(TEXT("measured range reported raw"), Outside.RangeUU, 1200.f, 0.5f);
	});

	It("mirrors left and right about the pivot", [this, Pivot, Forward]()
	{
		const FVector From = Pivot + FVector(400.f, 0.f, 0.f);
		FAIBArcStep Right, Left;
		FAIBMovementPolicy::ArcStep(From, Pivot, Forward, true, 220.f, 55.f, 0.f, 900.f, Right);
		FAIBMovementPolicy::ArcStep(From, Pivot, Forward, false, 220.f, 55.f, 0.f, 900.f, Left);
		TestEqual(TEXT("same X"), (double)Right.Destination.X, (double)Left.Destination.X, 0.5);
		TestEqual(TEXT("opposite Y"), (double)(Right.Destination.Y - Pivot.Y), (double)(-(Left.Destination.Y - Pivot.Y)), 0.5);
		TestTrue(TEXT("right is +Y about +X (up-axis rotation)"), Right.Destination.Y > Pivot.Y);
	});

	It("standing ON the pivot steps off along the body's forward — never stands (F9)", [this, Pivot]()
	{
		FAIBArcStep Step;
		TestTrue(TEXT("stepped from the pivot"), FAIBMovementPolicy::ArcStep(Pivot, Pivot, FVector(0.f, 1.f, 0.3f), true, 220.f, 55.f, 150.f, 300.f, Step));
		TestEqual(TEXT("lands on the band floor"), Range2D(Step.Destination, Pivot), 150.f, 0.5f);
		TestEqual(TEXT("full arc off the forward"), Step.ArcRadians, FMath::DegreesToRadians(55.f), 1e-4f);
		const FVector Bearing = (Step.Destination - Pivot).GetSafeNormal2D();
		TestEqual(TEXT("55 deg off +Y"), (double)FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Bearing, FVector(0.f, 1.f, 0.f)))), 55.0, 0.1);
		FAIBArcStep Refused;
		TestFalse(TEXT("no bearing at all is refused"), FAIBMovementPolicy::ArcStep(Pivot, Pivot, FVector(0.f, 0.f, 1.f), true, 220.f, 55.f, 150.f, 300.f, Refused));
	});

	It("holds the hill inside its reach — ring endpoints and the chord midpoint at the row's fraction", [this, Pivot, Forward]()
	{
		const FAIBTierRow Row;
		const float ReachUU = 600.f;
		const float RingUU = ReachUU * Row.HillStrafeRadiusFraction;
		const FAIBStrafeTaskInstanceData Footwork; // the strafe task's own node defaults
		// Worst case: a leg long enough to hit the arc cap, from the ring's edge.
		FVector From = Pivot + FVector(RingUU, 0.f, 0.f);
		for (int32 Leg = 0; Leg < 12; ++Leg)
		{
			FAIBArcStep Step;
			TestTrue(TEXT("stepped"), FAIBMovementPolicy::ArcStep(From, Pivot, Forward, (Leg % 3) != 0, 1200.f,
				Footwork.MaxArcDegrees, RingUU * 0.5f, RingUU, Step));
			TestTrue(TEXT("endpoint within the ring"), Range2D(Step.Destination, Pivot) <= RingUU + 0.5f);
			TestTrue(TEXT("endpoint within reach"), Range2D(Step.Destination, Pivot) <= ReachUU);
			const FVector Mid = (From + Step.Destination) * 0.5f;
			TestTrue(TEXT("chord midpoint within reach"), Range2D(Mid, Pivot) <= ReachUU);
			// A leg that expires mid-chord re-measures from the dip: still inside the band.
			From = Mid;
		}
	});

	It("counts an overlap as one episode — opens on the first ally inside, keeps the peak, closes on none", [this]()
	{
		FAIBOverlapEpisode Episode;
		float Seconds = 0.f;
		int32 Peak = 0;
		TestFalse(TEXT("nobody: nothing opens"), Episode.Note(0, 10.0, Seconds, Peak));
		TestFalse(TEXT("one inside: opens, not closed"), Episode.Note(1, 10.1, Seconds, Peak));
		TestFalse(TEXT("two inside: peak"), Episode.Note(2, 10.2, Seconds, Peak));
		TestFalse(TEXT("one again: still open"), Episode.Note(1, 10.3, Seconds, Peak));
		TestTrue(TEXT("none: closes"), Episode.Note(0, 10.5, Seconds, Peak));
		TestEqual(TEXT("duration from the opening sample"), Seconds, 0.4f, 1e-3f);
		TestEqual(TEXT("peak kept"), Peak, 2);
		TestFalse(TEXT("closed twice is nothing"), Episode.Note(0, 10.6, Seconds, Peak));
		// A brush shorter than the report threshold is the CALLER's filter, not the episode's.
		TestFalse(TEXT("brush opens"), Episode.Note(1, 20.0, Seconds, Peak));
		TestTrue(TEXT("brush closes"), Episode.Note(0, 20.1, Seconds, Peak));
		TestTrue(TEXT("brush is under the report threshold"), Seconds < AIB::TeammateOverlapReportSeconds);
	});

	It("closes with the body — a spell open at unpossession reports what was open", [this]()
	{
		FAIBOverlapEpisode Episode;
		float Seconds = 0.f;
		int32 Peak = 0;
		TestFalse(TEXT("nothing to close"), Episode.Close(5.0, Seconds, Peak));
		Episode.Note(3, 5.0, Seconds, Peak);
		TestTrue(TEXT("closed by the body"), Episode.Close(6.0, Seconds, Peak));
		TestEqual(TEXT("one second"), Seconds, 1.f, 1e-3f);
		TestEqual(TEXT("peak 3"), Peak, 3);
		TestTrue(TEXT("reset"), Episode.SinceSeconds < 0.0 && Episode.PeakCount == 0);
	});

	It("yields ONCE per wedge — the latch clears only on progress, the window is bounded (AIB24 H1/H2)", [this]()
	{
		FAIBLocomotionState State;
		TestFalse(TEXT("not yielding at rest"), State.IsYielding(10.0));
		TestTrue(TEXT("the first wedge yields"), State.TryArmYield(10.0, 1.f));
		TestTrue(TEXT("inside the window"), State.IsYielding(10.5));
		TestFalse(TEXT("the window lapses"), State.IsYielding(11.0));
		TestFalse(TEXT("the same wedge never yields again — the stall clock's abandon fires on schedule"), State.TryArmYield(11.1, 1.f));
		TestFalse(TEXT("still not yielding"), State.IsYielding(11.2));
		State.NoteProgress();
		TestTrue(TEXT("ground gained: the next wedge may yield"), State.TryArmYield(20.0, 1.f));
		State = FAIBLocomotionState();
		TestTrue(TEXT("a fresh body yields"), State.TryArmYield(0.0, 0.f));
		TestFalse(TEXT("a zero window is no window"), State.IsYielding(0.0));
	});

	It("pins the row defaults the tier table mirrors", [this]()
	{
		const FAIBTierRow Row;
		TestEqual(TEXT("CrowdSeparationWeight"), Row.CrowdSeparationWeight, 1.5f);
		TestEqual(TEXT("TeammateYieldRadiusUU"), Row.TeammateYieldRadiusUU, 80.f);
		TestEqual(TEXT("TeammateYieldSeconds"), Row.TeammateYieldSeconds, 1.f);
		TestEqual(TEXT("HillStrafeRadiusFraction"), Row.HillStrafeRadiusFraction, 0.6f);
		TestEqual(TEXT("overlap report threshold"), AIB::TeammateOverlapReportSeconds, 0.25f);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
