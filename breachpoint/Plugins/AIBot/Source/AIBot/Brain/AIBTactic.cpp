#include "Brain/AIBTactic.h"

#include "Core/AIBTypes.h"

namespace AIBTags
{
	UE_DEFINE_GAMEPLAY_TAG(Tactic_Push, "AIBot.Tactic.Push");
	UE_DEFINE_GAMEPLAY_TAG(Tactic_Flank, "AIBot.Tactic.Flank");
	UE_DEFINE_GAMEPLAY_TAG(Tactic_Hold, "AIBot.Tactic.Hold");
}

namespace
{
	/** A two-key curve with an explicit FLOOR at one end: every tactic term but Flank's
	 *  latched point is built with this, so no per-think fact can zero a tactic. */
	FAIBConsideration& AddTerm(FAIBAmbitionSpec& Spec, EAIBFactSelector Selector,
		float InputMin, float InputMax, float AtMin, float AtMax, float WhenUnknown)
	{
		FAIBConsideration& C = Spec.Considerations.AddDefaulted_GetRef();
		C.Selector = Selector;
		C.InputMin = InputMin;
		C.InputMax = InputMax;
		FRichCurve* Curve = C.Curve.GetRichCurve();
		Curve->Reset();
		Curve->AddKey(0.f, AtMin);
		Curve->AddKey(1.f, AtMax);
		C.ValueWhenUnknown = WhenUnknown;
		return C;
	}
}

void AIBTactic::BuildDefaultTacticSpecs(TArray<FAIBAmbitionSpec>& OutSpecs, float FlankCommitSeconds)
{
	OutSpecs.Reset();

	// PUSH — the floor. Direct pressure, scaled by nerve, vitality and how hurt the target
	// reads; none of the three can reach 0, and no commit, so a latched flank point or a
	// dry magazine is answered on the next Think (the Roam rule, applied to tactics).
	{
		FAIBAmbitionSpec& Push = OutSpecs.AddDefaulted_GetRef();
		Push.Tag = AIBTags::Tactic_Push;
		Push.BaseUtility = 0.6f;
		Push.CommitSeconds = 0.f;
		AddTerm(Push, EAIBFactSelector::ConfidenceNorm, 0.f, 1.f, 0.5f, 1.f, 1.f);
		AddTerm(Push, EAIBFactSelector::VitalityNorm, 0.f, 1.f, 0.6f, 1.f, 1.f);
		AddTerm(Push, EAIBFactSelector::TargetHealthNorm, 0.f, 1.f, 1.f, 0.6f, 1.f);
	}

	// FLANK — mid-band range, being shot, teammates on the target (Phase 12's crowd fact;
	// unknown today scores its default), and THE POINT: the controller's latched flank
	// point arrives as the objective fact joined to this tag, urgency 1. Without it the
	// term is 0 and Flank is silent — the ONLY zero this tactic can produce.
	{
		FAIBAmbitionSpec& Flank = OutSpecs.AddDefaulted_GetRef();
		Flank.Tag = AIBTags::Tactic_Flank;
		Flank.BaseUtility = 1.4f;
		Flank.CommitSeconds = FlankCommitSeconds;
		FAIBConsideration& Band = AddTerm(Flank, EAIBFactSelector::DistToTargetUU,
			0.f, AIB::EngageFadeEndUU, 0.2f, 0.4f, 0.5f);
		{
			// Knife range: push, do not walk away. Mid range: the flank's home. The edge
			// of sight: a long walk around for a target that may not be there.
			FRichCurve* C = Band.Curve.GetRichCurve();
			C->AddKey(0.35f, 1.f);
			C->AddKey(0.75f, 1.f);
		}
		AddTerm(Flank, EAIBFactSelector::RecentDamageTakenNorm, 0.f, 0.5f, 0.5f, 1.f, 0.5f);
		AddTerm(Flank, EAIBFactSelector::NearbyAllies, 0.f, 2.f, 0.6f, 1.f, 0.7f);
		AddTerm(Flank, EAIBFactSelector::ObjectiveUrgency, 0.f, 1.f, 0.f, 1.f, 0.f);
	}

	// HOLD — a thin magazine, a station at fight range, the high ground. All floored:
	// Hold is bounded by the controller's HoldMaxSeconds clock, not by a term dying.
	{
		FAIBAmbitionSpec& Hold = OutSpecs.AddDefaulted_GetRef();
		Hold.Tag = AIBTags::Tactic_Hold;
		Hold.BaseUtility = 1.0f;
		Hold.CommitSeconds = 2.f;
		AddTerm(Hold, EAIBFactSelector::AmmoNorm, 0.f, 1.f, 1.f, 0.25f, 0.5f);
		AddTerm(Hold, EAIBFactSelector::DistToTargetUU, 300.f, 900.f, 0.2f, 1.f, 0.5f);
		AddTerm(Hold, EAIBFactSelector::HeightAdvantageUU, -200.f, 300.f, 0.4f, 1.f, 0.6f);
	}
}
