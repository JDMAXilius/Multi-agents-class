# How to test Assignment #9 — a guide for the grader

**Short version:** you do **not** need Unreal Engine to test most of this. Clone the repo,
run one script, and you will *execute* the agent's rule layer and audit its output.

```bash
git clone https://github.com/JDMAXilius/Multi-agents-class.git
cd Multi-agents-class/assignments/09-adversarial-qa
./verify.sh
```

Requirements: **Python 3** and a **C++ compiler** (`g++` or `clang++` — preinstalled on
macOS with Xcode command-line tools, and on every Linux box; on Windows use WSL or Git
Bash + MinGW). No `pip install`, no API key, no network, no GPU, no game engine.

---

## 1. Why this assignment is different from my others

Assignments #4–#8 are standalone Python that runs anywhere. This one **cannot** be, and
that is the assignment's own requirement: *"an adversarial testing agent that runs against
your capstone game… runs continuously inside your game."* An agent that tests BREACHPOINT
has to live inside BREACHPOINT, which is native C++ for Unreal Engine 5.8.

So the deliverable splits into two honest halves:

| Half | What it is | Can you test it without UE? |
|---|---|---|
| **The rule layer** | *What counts as broken* — 7 detector classes, thresholds, excuse policies | **Yes — you compile and run it.** 44 cases. |
| **The integration** | *Driving the live game* — possession, input presses, navigation, timers | No. Needs the engine. Evidence is the committed report. |

I split the code that way deliberately so the half you can check is a real execution, not
a code-reading exercise.

## 2. What `./verify.sh` actually does

It runs in one of two modes automatically.

**Section 0 — the part that matters most.** It invokes your C++ compiler on
`tests/detector_tests.cpp`, which `#include`s **the exact header the game uses**
(`BNAQADetectors.h` — the game's controller calls these same functions; verify.sh proves
there is no second copy by checking that no threshold literal is restated in the
controller). It then executes all 44 rule cases. Each detector is tested twice:

- the **firing case** — the defect it exists to catch
- the **excuse case** — the legitimate situation that looks identical and must *not* be
  reported (a frozen pawn is not "stuck"; a grappling player exceeding walk speed is not
  speed-hacking; a respawn is not a teleport; a corpse below the kill plane is the kill
  plane *working*)

The second half is the point. A QA agent that reports false positives is worse than none.

**Sections 1–4** map one check per rubric criterion — the behavior loop, the definition of
"broken", the report schema, and the README's answers.

## 3. The one thing that needs the engine — and how to read it

The rubric's **Findings** criterion requires results from a real test run. That means one
Play-In-Editor session on a machine with UE 5.8 and this project. Until that run's report
is committed, `verify.sh` prints those items as **PEND**, not FAIL, and says exactly what
is missing. It never fabricates a result.

**If `report/` contains a JSON file when you run it**, the run happened: `verify.sh`
switches to FULL mode and audits the real findings — location, error type, game context,
evidence — and the README's two answers. That report is the agent's own output, written by
the agent during the run, not by hand.

**If you have UE 5.8 and want to reproduce it yourself:** open `breachpoint/Breachpoint.uproject`,
press Play, open the console (`~`) and type:

```
bn.aqa.start 300      # the probe joins the match and attacks it for 5 minutes
bn.aqa.stop           # (optional) end early — the report writes on any stop
```

The report lands in `Saved/AdversarialQA/aqa_report_*.json`. Watch the `LogBNAQA` category:
every finding logs the moment it is detected.

## 4. Where to look, if you would rather read than run

| What | Where |
|---|---|
| The rules — thresholds and excuse policies, engine-free | `breachpoint/Source/BreachpointNext/QA/BNAQADetectors.h` |
| The agent — the loop, the 5 attack behaviors, the report writer | `breachpoint/Source/BreachpointNext/QA/BNAdversarialAgent.cpp` |
| The tests you just ran | `tests/detector_tests.cpp` |
| The findings write-up | `README.md` |
| The run's own output | `report/aqa_report_*.json` |
| How the run was commissioned and verified | `breachpoint/docs/tickets/TICKET_BN24_ADVERSARIAL_QA_RUN.md` |

## 5. What I am *not* claiming

The project I am building has a written honesty rule — *compiles ≠ works · PIE ≠
multiplayer · listen ≠ dedicated · editor ≠ packaged* — and it applies to this
submission too:

- The 44 detector cases **execute**. That proves the rules, not the wiring.
- The report, when present, comes from **Play-In-Editor**. It names its own net mode. It
  is not a claim about a packaged build or a real multiplayer session.
- The `escaped_playable_space` bounds are a **heuristic** (the hull of the level's
  PlayerStarts plus a margin), and every finding it produces says so in its own evidence
  string.

If any part of this does not run for you, the failure output names the file and the reason
in one line — no stack traces to decode.
