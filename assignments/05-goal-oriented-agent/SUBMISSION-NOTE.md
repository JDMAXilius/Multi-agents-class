# Submission note — Assignment #5 (copy/paste)

Due 13 August · submitted 18 August · **five days late**.
Keep it short and factual. Two versions below; use whichever fits the form.

---

## Full version (email / LMS comment box)

> **Assignment #5 — Goal-Oriented Coding Agent · Juan Diego Lugo**
>
> Submitting this five days past the 13 August deadline — apologies for the delay, no excuse
> to offer.
>
> **What it is.** An agent that reads my capstone's GDD (BREACHPOINT, a 4v4 arena FPS in
> Unreal Engine 5.8), scans its C++ source, detects what the design promises that the code
> does not have, ranks the gaps, and writes the C++ for the top one. It selected the
> **Spotter subsystem** and generated `UBRSpotterSubsystem` — 16 KB of Unreal C++.
>
> **Where it is.**
> Repo: https://github.com/JDMAXilius/Multi-agents-class/tree/main/assignments/05-goal-oriented-agent
> (`BREACHPOINT-goal-oriented-agent.zip` attached if you'd rather not clone.)
>
> **How to check it — one command:**
>
> ```
> cd assignments/05-goal-oriented-agent
> ./verify.sh
> ```
>
> Python 3 only — no `pip install`, no API key, no network, no game engine. It deletes the
> agent's output, re-runs the agent, and prints a PASS/FAIL line per assignment requirement
> with the evidence it used. Exit code 0 means all 13 checks passed. `TICKET_VERIFY.md`
> explains each check.
>
> `python3 agent.py --rank` shows the entire reasoning layer and stops before any model call
> — stages 1–4 make no LLM calls at all, so the selection is auditable and recomputable from
> `output/ranking.json`.
>
> **One thing stated plainly:** the generated C++ has **not** been compiled and is **not**
> running in the game. There's no Unreal Engine in the environment this was built in, and the
> code lands in a pinned copy of the project rather than the live tree. The README answers
> "were you able to run it in your game?" with no, and the reasons. `verify.sh` prints the
> same caveat so a passing run can't be mistaken for a working build.
>
> Thanks for your patience with the late submission.

---

## Short version (a form with a small comment field)

> Assignment #5 — Goal-Oriented Coding Agent · Juan Diego Lugo. Submitting five days past the
> 13 August deadline, apologies for the delay.
>
> The agent reads my capstone's GDD (BREACHPOINT, UE 5.8 FPS), scans its C++, detects gaps,
> ranks them deterministically, and generates the missing `UBRSpotterSubsystem`.
>
> https://github.com/JDMAXilius/Multi-agents-class/tree/main/assignments/05-goal-oriented-agent
>
> To verify: `cd assignments/05-goal-oriented-agent && ./verify.sh` — Python 3 only, no API
> key, prints PASS/FAIL per requirement, exit 0 = all passed. `TICKET_VERIFY.md` explains it.
>
> Noting honestly: the generated C++ is not compiled and not running in the game — no engine
> in the build environment. The README and the verify output both say so.

---

## Notes on tone

- **State the lateness first, once, and move on.** Apologise, don't explain — an excuse
  invites scrutiny of the excuse instead of the work.
- **Lead with the one command.** A grader who can confirm it works in ten seconds is a
  grader who reads the rest generously.
- **Volunteer the limitation.** "Not compiled, not running in the game" is going to be
  noticed either way; saying it first makes every other claim more credible, and it is the
  literal answer to one of the three required README questions.
