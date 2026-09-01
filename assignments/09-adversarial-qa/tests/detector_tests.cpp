// Executable proof of the adversarial agent's rule layer — WITHOUT Unreal Engine.
//
//   g++ -std=c++17 -O0 -Wall -Wextra -o detector_tests detector_tests.cpp && ./detector_tests
//   (verify.sh does exactly this; it also finds the header in either repo or zip layout)
//
// This compiles BNAQADetectors.h — the SAME header ABNAQAController includes and calls in
// the game — and pushes every detector through two cases:
//
//   the FIRING case   the defect the rule exists to catch
//   the EXCUSE case   the legitimate situation that looks identical and must NOT convict
//
// The second half is the point. A detector that only ever fires is a false-positive
// generator, and this project has been bitten three times (assignments #4, #6, #8) by a
// validation layer that was narrower or looser than the truth it validated. Every excuse
// below is a real situation in BREACHPOINT: the match freeze pins a commanded pawn, the
// grapple legally exceeds walk speed in flight, a respawn legally teleports the body.
//
// No test framework — a header-only assert with a counter, so this builds with a stock
// compiler on any machine the grader happens to have.

#include "BNAQADetectors.h"

#include <cstdio>
#include <cmath>
#include <limits>

static int gChecks = 0;
static int gFailures = 0;

static void Check(bool bCondition, const char* Rule, const char* What)
{
    ++gChecks;
    if (bCondition)
    {
        std::printf("  \033[32mok\033[0m   %-26s %s\n", Rule, What);
    }
    else
    {
        std::printf("  \033[31mFAIL\033[0m %-26s %s\n", Rule, What);
        ++gFailures;
    }
}

int main()
{
    using namespace BNAQA;
    std::printf("\033[1mBNAQA detector rules — executed without Unreal Engine\033[0m\n");
    std::printf("Assignment #9 · BREACHPOINT · %s\n\n", __DATE__);

    // -- 1. fell_out_of_world_alive ------------------------------------------------
    std::printf("1. fell_out_of_world_alive\n");
    Check(FellOutOfWorldAlive(true, -5100.0, -5000.0, 2.0),
          "fires", "alive 2.0s below KillZ");
    Check(!FellOutOfWorldAlive(true, -5100.0, -5000.0, 0.4),
          "excuses", "inside the 1.0s grace — the kill volume gets its turn");
    Check(!FellOutOfWorldAlive(false, -9999.0, -5000.0, 30.0),
          "excuses", "dead below KillZ — that is KillZ WORKING");
    Check(!FellOutOfWorldAlive(true, 200.0, -5000.0, 30.0),
          "excuses", "alive above KillZ, however long");

    // -- 2. escaped_playable_space -------------------------------------------------
    std::printf("\n2. escaped_playable_space\n");
    Check(EscapedPlayableSpace(true, true, true, true),
          "fires", "standing on ground outside the hull + margin");
    Check(!EscapedPlayableSpace(true, false, true, true),
          "excuses", "AIRBORNE outside the hull — falling is KillZ's case, not this one");
    Check(!EscapedPlayableSpace(false, true, true, true),
          "excuses", "a corpse outside the hull");
    Check(!EscapedPlayableSpace(true, true, false, true),
          "excuses", "no PlayerStarts — no hull, so no claim about bounds");
    Check(!EscapedPlayableSpace(true, true, true, false),
          "excuses", "inside the expanded hull");

    // -- 3. stuck_state -------------------------------------------------------------
    std::printf("\n3. stuck_state\n");
    Check(StuckState(true, false, true, 1.5, 3.5),
          "fires", "commanded, alive, unfrozen, 1.5 uu/s for 3.5s");
    Check(!StuckState(true, true, true, 0.0, 30.0),
          "excuses", "FROZEN — the match freeze is supposed to pin the pawn");
    Check(!StuckState(true, false, false, 0.0, 30.0),
          "excuses", "no move order — standing still is not a defect");
    Check(!StuckState(false, false, true, 0.0, 30.0),
          "excuses", "dead");
    Check(!StuckState(true, false, true, 1.0, 2.0),
          "excuses", "still, but only 2.0s — under the 3.0s window");
    Check(!StuckState(true, false, true, 240.0, 30.0),
          "excuses", "commanded and actually moving at 240 uu/s");

    // -- 4. speed_violation ---------------------------------------------------------
    std::printf("\n4. speed_violation\n");
    Check(SpeedViolation(true, 1400.0, 600.0),
          "fires", "1400 uu/s walking against a 600 uu/s model (2.33x)");
    Check(!SpeedViolation(false, 4000.0, 600.0),
          "excuses", "AIRBORNE at 4000 uu/s — grapple flight and gravity are legal");
    Check(!SpeedViolation(true, 620.0, 600.0),
          "excuses", "620 vs 600 — inside the 1.75x tolerance, not a cheat");
    Check(!SpeedViolation(true, 900.0, 0.0),
          "excuses", "MaxWalkSpeed unreadable — 'cannot see it' is not 'broken'");
    Check(SpeedViolation(true, 600.0 * 1.75 + 0.1, 600.0),
          "boundary", "just past 1.75x convicts");
    Check(!SpeedViolation(true, 600.0 * 1.75, 600.0),
          "boundary", "exactly 1.75x does not");

    // -- 5. attribute_anomaly -------------------------------------------------------
    std::printf("\n5. attribute_anomaly\n");
    const double NaNv = std::numeric_limits<double>::quiet_NaN();
    const double Infv = std::numeric_limits<double>::infinity();
    Check(AttributeAnomaly(NaNv, 100.0, 50.0, 50.0),
          "fires", "health is NaN");
    Check(AttributeAnomaly(100.0, 100.0, Infv, 50.0),
          "fires", "shield is infinite");
    Check(AttributeAnomaly(-5.0, 100.0, 50.0, 50.0),
          "fires", "negative health");
    Check(AttributeAnomaly(180.0, 100.0, 50.0, 50.0),
          "fires", "health above its own max");
    Check(AttributeAnomaly(100.0, 100.0, 90.0, 50.0),
          "fires", "shield above its own max");
    Check(!AttributeAnomaly(100.0, 100.0, 50.0, 50.0),
          "excuses", "everything at full");
    Check(!AttributeAnomaly(0.0, 100.0, 0.0, 50.0),
          "excuses", "zeroed on death — a legal floor, not an anomaly");
    Check(!AttributeAnomaly(100.0, 0.0, 50.0, 0.0),
          "excuses", "maxes read 0 (unconfigured) — unreadable is not broken");
    Check(!AttributeAnomaly(100.5, 100.0, 50.0, 50.0),
          "excuses", "half a point over max — inside the rounding slack");

    // -- 6. teleport_discontinuity --------------------------------------------------
    std::printf("\n6. teleport_discontinuity\n");
    Check(TeleportDiscontinuity(true, 5000.0),
          "fires", "5000 uu in one 100ms sample (50,000 uu/s)");
    Check(!TeleportDiscontinuity(false, 90000.0),
          "excuses", "NO previous sample — this is what makes a RESPAWN legal");
    Check(!TeleportDiscontinuity(true, 80.0),
          "excuses", "80 uu in a sample — an ordinary sprint step");
    Check(!TeleportDiscontinuity(true, Thresholds::TeleportUUPerSample),
          "boundary", "exactly at the threshold does not convict");

    // -- 7. the state gates ---------------------------------------------------------
    std::printf("\n7. acted_while_dead / input_during_freeze\n");
    Check(GateBreak(true, true, false) == EGateBreak::ActedWhileDead,
          "fires", "an ability activated while State.Dead");
    Check(GateBreak(true, false, true) == EGateBreak::InputDuringFreeze,
          "fires", "an ability activated while State.Match.Frozen");
    Check(GateBreak(true, true, true) == EGateBreak::ActedWhileDead,
          "ordering", "dead outranks frozen — one finding, the graver one");
    Check(GateBreak(true, false, false) == EGateBreak::None,
          "excuses", "a live, unfrozen pawn acting — the whole point of the game");
    Check(GateBreak(false, true, true) == EGateBreak::None,
          "excuses", "no activation at all — pressing into a closed gate is CORRECT");

    // -- the thresholds the report and README quote ---------------------------------
    std::printf("\n8. thresholds are the ones documented\n");
    Check(Thresholds::SpeedTolerance == 1.75, "pinned", "speed tolerance 1.75x");
    Check(Thresholds::StuckAfterS == 3.0, "pinned", "stuck window 3.0s");
    Check(Thresholds::TeleportUUPerSample == 1200.0, "pinned", "teleport step 1200 uu");
    Check(Thresholds::HullMarginUU == 4000.0, "pinned", "hull margin 4000 uu");
    Check(Thresholds::ProbeSeconds == 0.1, "pinned", "probe cadence 100ms");

    std::printf("\n%d checks, %d failure(s)\n", gChecks, gFailures);
    if (gFailures == 0)
    {
        std::printf("\033[32mALL DETECTOR RULES PASS — every rule fires on its defect and "
                    "excuses its legitimate twin.\033[0m\n");
    }
    return gFailures == 0 ? 0 : 1;
}
