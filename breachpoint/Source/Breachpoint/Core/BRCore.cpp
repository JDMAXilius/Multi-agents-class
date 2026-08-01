// Breachpoint. Log channels, collision channel aliases, and the GAS stage gate.

#include "Core/BRCore.h"

#include "Misc/DelayedAutoRegister.h"

// THE ONE INVERTED INCLUDE IN THIS FILE, and it is deliberate. Core/ is the bottom of the include
// order and BRCharacter.h sits well above it — but only this .cpp depends on it, never BRCore.h, so
// nothing that includes BRCore.h inherits the dependency and there is no header cycle. The
// alternative was reading the ini key with GConfig directly, which would have made the
// UPROPERTY(Config) on ABRCharacter a second, unread copy of the same value; one storage location
// beats a tidy include graph on the one switch the founder reverts under time pressure.
#include "Character/BRCharacter.h"

// One definition per declaration in BRCore.h. The collision aliases are compile-time
// constants and need no definition here; their counterpart lives in Config/DefaultEngine.ini.

DEFINE_LOG_CATEGORY(LogBRCombat);
DEFINE_LOG_CATEGORY(LogBRNet);
DEFINE_LOG_CATEGORY(LogBRAI);
DEFINE_LOG_CATEGORY(LogBROnline);
DEFINE_LOG_CATEGORY(LogBRUI);

// Input/ (§3.2). Added by BP01 step 4 under ruling R24 — see the header comment.
DEFINE_LOG_CATEGORY(LogBRInput);

// ---------------------------------------------------------------------------
// The GAS stage gate — docs/GAS-INTEGRATION-ROADMAP.md
// ---------------------------------------------------------------------------

const TCHAR* BRGas::ToString(EBRGasStage Stage)
{
	// A switch rather than StaticEnum<>()->GetNameStringByValue(): this runs from an engine-init
	// hook whose whole job is to print the line, and a gate diagnostic that depends on the
	// reflection system being warm is a diagnostic that can go quiet exactly when it is needed.
	switch (Stage)
	{
	case EBRGasStage::Off:            return TEXT("Off");
	case EBRGasStage::AttributesOnly: return TEXT("AttributesOnly");
	case EBRGasStage::Granting:       return TEXT("Granting");
	case EBRGasStage::InputRouted:    return TEXT("InputRouted");
	case EBRGasStage::Sprint:         return TEXT("Sprint");
	case EBRGasStage::Weapons:        return TEXT("Weapons");
	case EBRGasStage::FullSandbox:    return TEXT("FullSandbox");
	case EBRGasStage::Cues:           return TEXT("Cues");
	default:                          return TEXT("<unknown>");
	}
}

EBRGasStage BRGas::GetStage()
{
	// Resolved once per process, from ABRCharacter's C++ CDO — see BRCore.h for why the CDO and
	// not an instance. A function-local static is the whole mechanism: it is initialised on first
	// use, exactly once, and thread-safely, so "log the stage once" needs no flag of its own.
	static const EBRGasStage Resolved = []()
	{
		const EBRGasStage Stage = ABRCharacter::GetConfiguredGasStage();

		// WARNING, not Log, and it is not a warning about anything. It is the one line that makes
		// the gate audible: at Warning it survives a default-verbosity log and a founder skimming
		// PIE output, which is the only place this information is any use. A silent gate and a
		// broken feature look identical from the chair.
		UE_LOG(LogBRCombat, Warning, TEXT("BRGas: stage = %s (Config/DefaultGame.ini)"), BRGas::ToString(Stage));

		return Stage;
	}();

	return Resolved;
}

bool BRGas::IsStageEnabled(EBRGasStage Required)
{
	// The whole gate: one ordered comparison. See EBRGasStage for why the order is load-bearing.
	return GetStage() >= Required;
}

namespace
{
	/**
	 * Print the stage line at engine init, whether or not anything ever asks.
	 *
	 * WHY THIS EXISTS AT ALL: `GetStage()` logs lazily, so at `Off` — the default, and the state
	 * the founder plays in — every gated call site returns early and, without this, the line would
	 * never print. "The gate is Off" and "the gate is broken" would then look the same in the log,
	 * which is the precise failure this whole mechanism was built to end.
	 *
	 * `EndOfEngineInit` is chosen because the ini system and the object system are both up by then,
	 * so the CDO carries its config value. If the phase has already passed when this global is
	 * constructed, FDelayedAutoRegisterHelper runs the lambda immediately — either way, once.
	 *
	 * Not a Tick and not a timer (law 4): it is one callback, at one phase, per process.
	 */
	const FDelayedAutoRegisterHelper GBRGasStageAnnouncer(
		EDelayedRegisterRunPhase::EndOfEngineInit,
		[]() { BRGas::GetStage(); });
}
