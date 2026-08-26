#include "Skills/AIBGrenadePolicy.h"

/**
 * The recognition ladder, worldless. Everything here is a pure read of FAIBFacts plus the
 * level: no handles, no clock, no queries. The conventions block in AIBSkillProfile.h binds
 * this file; FAIRPLAY F3 binds what may be read at all.
 */
namespace
{
	/**
	 * THE FLOOR, one value for every level that throws at all: nobody lobs one at their own
	 * feet. It sits just OUTSIDE AIB::EngageFullAppetiteUU (400) — the distance the module
	 * already calls full appetite, i.e. "this is a gunfight, not a lob". A level-varying
	 * floor would be a tuning knob pretending to be a skill: a Novice does not throw at all
	 * (its band is empty, below), and an Expert is not braver about its own blast.
	 */
	constexpr float ThrowFloorUU = 450.f;

	/**
	 * THE CEILINGS, justified against the sight envelope the same way the Engage band is
	 * (W-REVIEW P2 C3: a band that starts AT the envelope's edge is provably inert, because
	 * no fact that can occur ever lands inside it).
	 *   DistToTargetUU is only ever KNOWN while a target is held. Sight is gained inside the
	 *   default tier's SightRadius (1200uu) and held out to LoseSightRadius (1500uu) ==
	 *   AIB::EngageFadeEndUU, so the reachable range of a known distance is (0, 1500].
	 *   - Trained 1100: entirely inside the GAIN radius, so every uu of a Trained band is
	 *     reachable by a fresh sighting alone — the most conservative reachability proof.
	 *   - Skilled 1300: reaches into the hold-only zone, the ground a Trained bot never
	 *     throws across.
	 *   - Expert: the envelope's own edge, AIB::EngageFadeEndUU — the widest band that is
	 *     still reachable, tied to the named constant so a Phase 8 envelope change moves the
	 *     ceiling with it instead of stranding it outside.
	 * All three are strictly above the floor, so no throwing level carries an empty band.
	 */
	constexpr float TrainedCeilingUU = 1100.f;
	constexpr float SkilledCeilingUU = 1300.f;

	/**
	 * The FINISHER's read. Enemy vitals are NOT perceivable (FAIRPLAY F3, the 26 Aug
	 * amendment): there is no target-health number to threshold. What a human plays by is
	 * the pressure THEY are applying — damage DEALT in the recent window — and that is a
	 * SELF fact off the avatar door. At or above half a health bar's worth of recent output,
	 * the moment is a finish, not an opening.
	 */
	constexpr float FinisherDamageDealtNorm = 0.5f;

	/**
	 * AREA DENIAL's freshness bar, expressed in the module's own normalised memory freshness
	 * (1 - age/window, the MemoryFreshness selector's formula) rather than a new absolute
	 * second count: "just lost sight" must stay tier-relative so Phase 8 retunes it through
	 * the tier row's window, never by editing this file. Half the window is the bar.
	 */
	constexpr float DenialMinMemoryFreshness = 0.5f;

	/** Rungs are ordered novice -> expert, and each level sees everything below it. */
	bool AtLeast(EAIBCompetence Level, EAIBCompetence Rung)
	{
		return static_cast<uint8>(Level) >= static_cast<uint8>(Rung);
	}

	/**
	 * The band test. An UNKNOWN distance (<0, the facts convention) is not in band — the
	 * module's unknown-is-a-state rule: an unknown never counts as satisfied.
	 */
	bool IsInThrowBand(float DistUU, EAIBCompetence Level)
	{
		if (DistUU < 0.f)
		{
			return false;
		}
		return DistUU >= FAIBGrenadePolicy::ThrowBandMinUU(Level)
			&& DistUU <= FAIBGrenadePolicy::ThrowBandMaxUU(Level);
	}

	/** UNSET (returned as <0) means "not knowable", exactly as the MemoryFreshness selector. */
	float MemoryFreshnessOrUnknown(const FAIBFacts& Facts)
	{
		if (!Facts.bHasMemory || Facts.MemoryFreshWindowSeconds <= 0.f)
		{
			return -1.f;
		}
		return FMath::Clamp(1.f - Facts.LastKnownAgeSeconds / Facts.MemoryFreshWindowSeconds, 0.f, 1.f);
	}

	/**
	 * THE UNKNOWN-DISTANCE RULING for area denial, resolved and recorded here because the
	 * contract left it open.
	 *
	 * The facts builder populates DistToTargetUU ONLY while a target is held; when sight is
	 * gone — the exact situation denial is FOR — the scalar is unknown (<0) by construction.
	 * So a denial that demanded a known in-band distance could never fire with any fact set
	 * the builder can produce: an inert Expert capability, which is the same defect class as
	 * the band that sat outside the envelope.
	 *
	 * RULING: denial does not rest on the distance scalar at all. What it requires is a fact
	 * that IS known — a fresh memory — and the band is applied only WHERE THE SCALAR EXISTS.
	 * That keeps unknown-is-a-state intact (nothing unknown is scored as satisfied; the band
	 * simply does not apply), and it arms automatically if a later builder starts publishing
	 * a memory-sourced distance.
	 *
	 * NAMED RESIDUAL RISK: the floor's self-protection (never throw at your own feet) cannot
	 * apply while the scalar is absent, so an Expert may deny a remembered spot that is very
	 * close. Bounded by the fact that a spot that close is usually still visible (denial
	 * requires it not to be); revisit the moment a memory distance exists.
	 */
	bool DenialDistanceAllows(const FAIBFacts& Facts, EAIBCompetence Level)
	{
		return Facts.DistToTargetUU < 0.f ? true : IsInThrowBand(Facts.DistToTargetUU, Level);
	}
}

bool FAIBGrenadePolicy::CanEvadeBlast(EAIBCompetence Level)
{
	// THE CAPABILITY GATE — design, not a number. There is no tuning value, no data row and
	// no fact combination that lets a Novice dodge a matured blast: the answer is the level.
	return Level != EAIBCompetence::Novice;
}

float FAIBGrenadePolicy::ThrowBandMinUU(EAIBCompetence /*Level*/)
{
	// One floor for everyone, Novice included — the floor is not what stops a Novice
	// throwing (its CEILING is 0, so its band is empty whatever the floor says).
	return ThrowFloorUU;
}

float FAIBGrenadePolicy::ThrowBandMaxUU(EAIBCompetence Level)
{
	switch (Level)
	{
	case EAIBCompetence::Novice:
		// Below the floor on purpose: a Novice's band is EMPTY by construction, so even a
		// future caller that consulted the band without consulting the level cannot get a
		// Novice to throw. The capability gate belongs to the level twice over.
		return 0.f;
	case EAIBCompetence::Trained:
		return TrainedCeilingUU;
	case EAIBCompetence::Skilled:
		return SkilledCeilingUU;
	case EAIBCompetence::Expert:
	default:
		// Out-of-range degrades to the row's own baseline elsewhere in the module; here the
		// widest band is still bounded by the named envelope constant, never by infinity.
		return AIB::EngageFadeEndUU;
	}
}

float FAIBGrenadePolicy::ConsiderSeconds(EAIBCompetence Level)
{
	switch (Level)
	{
	case EAIBCompetence::Novice:
		// N/A: a Novice never considers, so there is no cadence to report. 0 is the honest
		// "no cadence" — a plausible-looking number here would be a lie a future caller
		// could act on, and a 0-length window is inert anyway because the Novice branch
		// returns None before any stamp is written.
		return 0.f;
	case EAIBCompetence::Trained:
		return 1.5f;
	case EAIBCompetence::Skilled:
		return 1.2f;
	case EAIBCompetence::Expert:
	default:
		return 0.9f;
	}
}

EAIBGrenadeCall FAIBGrenadePolicy::Consider(FAIBGrenadeState& State, const FAIBFacts& Facts,
	EAIBCompetence Level, FRandomStream& /*Rng*/, double NowSeconds)
{
	// NOTE ON Rng (contract ambiguity, resolved): the signature carries the caller's stream
	// so the policy never reaches for a global one, but this ladder makes NO draw. Every
	// call is a deterministic recognition of the facts — a per-throw dice roll is exactly
	// the shape F4 bans for aim, and it would make the same moment read differently for two
	// bots at the same level. The parameter stays unnamed rather than fed a fabricated
	// roll; when a real draw lands (throw hesitation, arc noise) it draws from HERE.

	// GATE 1 — the cadence. Inside the window nothing is considered, so the stamp is NOT
	// touched: restamping on a refused look would let the think rate push the next glance
	// forever forward, and the bot would never throw at all.
	if (NowSeconds < State.NextConsiderAtSeconds)
	{
		return EAIBGrenadeCall::None;
	}

	// GATE 2 — an empty pocket is not a decision, at any level. No stamp either: nothing was
	// weighed, so a grenade arriving next think may be seen at once.
	if (Facts.GrenadeCount <= 0)
	{
		return EAIBGrenadeCall::None;
	}

	// GATE 3 — the capability gate again, on the throwing side. A Novice never throws
	// deliberately: no band, no cadence, no stamp.
	if (Level == EAIBCompetence::Novice)
	{
		return EAIBGrenadeCall::None;
	}

	// From here a consideration really happens, and it costs a glance whatever the answer
	// is — a pass that did not pay the cadence would re-evaluate at think rate.
	State.NextConsiderAtSeconds = NowSeconds + ConsiderSeconds(Level);

	const bool bInBand = IsInThrowBand(Facts.DistToTargetUU, Level);

	// FINISHER (Skilled+) — checked FIRST because it is the more specific read of the same
	// visible moment; opener-first would swallow every finish. bDamageHistoryKnown false
	// means the read is unknowable, and an unknown never counts as satisfied — it falls
	// through to the opener, which is the honest lesser recognition.
	if (AtLeast(Level, EAIBCompetence::Skilled) && Facts.bTargetVisible && bInBand
		&& Facts.bDamageHistoryKnown && Facts.RecentDamageDealtNorm >= FinisherDamageDealtNorm)
	{
		return EAIBGrenadeCall::Finisher;
	}

	// OPENER (Trained+) — a target you can see, inside the band, grenades in pocket.
	if (AtLeast(Level, EAIBCompetence::Trained) && Facts.bTargetVisible && bInBand)
	{
		return EAIBGrenadeCall::Opener;
	}

	// AREA DENIAL (Expert only) — sight is gone but the memory is fresh: the throw at where
	// they WENT. bHasMemory is the honest flag, NOT bTargetFactsFromMemory: the latter is a
	// held-belief marker that the builder only sets while a target is still held, so it is
	// false in every situation denial exists for (requiring it would make this branch inert).
	// DORMANT, AND SAID SO (W-REVIEW P4+5 F-H4): this branch currently has NO reachable
	// caller. Consider() is invoked only from the Engage branch's fire task, whose gate
	// needs a VISIBLE target to hold the state, while denial requires !bTargetVisible —
	// the precondition and the only call site are mutually exclusive. Until a Search-side
	// caller lands (registered debt: it must also FACE the memory point before pressing,
	// or the throw goes wherever the sweep happens to look), the Expert grenade rung is
	// Skilled-plus-nothing IN THE FIELD, and claiming otherwise is the inert-band defect
	// this file twice cites as blocking. The logic stays, spec'd, because the specs prove
	// the ladder's shape — but no packet may claim the capability landed until the
	// caller exists.
	if (AtLeast(Level, EAIBCompetence::Expert) && !Facts.bTargetVisible)
	{
		const float Freshness = MemoryFreshnessOrUnknown(Facts);
		if (Freshness >= DenialMinMemoryFreshness && DenialDistanceAllows(Facts, Level))
		{
			return EAIBGrenadeCall::AreaDenial;
		}
	}

	return EAIBGrenadeCall::None;
}
