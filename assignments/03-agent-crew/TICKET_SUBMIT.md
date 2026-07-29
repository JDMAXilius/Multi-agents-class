# TICKET — Submit Assignment #3 (Build an Agent Crew)

> STATUS: **open — DO THIS TODAY.** Due was 28 Jul 2026 11:59 ET; today is 29 Jul, so this
> lands late and every hour counts. Runs anywhere — no engine, no UE, no game repo.
> **Not a game-build ticket:** this is course submission, deliberately kept off the
> `crew/docs/tickets/` board so it never ships into the game repo.

Everything the assignment asks for is built, run, and pushed. What remains is the act of
submitting it, plus the checks that protect the two scored criteria most likely to be lost
to a packaging mistake rather than to the work itself: **Working Crew /3.0** (does it run on
the grader's machine?) and **Architecture Diagram /1.0** (does the Mermaid actually render?).

## What is already done

- [x] Crew code — `run_crew.py`, four coordinating agents, gated pipeline
- [x] Mermaid diagram — inline in `README.md`
- [x] README naming the game — **BREACHPOINT**, what the crew produces and why
- [x] Live run completed; `output/` holds `DT_Weapons.csv` + `arena_manifest.json`
- [x] Replay mode verified: stdlib only, no API key, exit 0
- [x] Zip path verified: `make_submission.sh` bundles the agent definitions and refuses to
      build a package that fails its own replay
- [x] Committed and pushed to `main`

## Steps (in order)

1. **Pick the delivery format.** Prefer the **repo link** — GitHub renders the Mermaid
   diagram inline (worth 1.0) and the grader can browse `output/` without downloading
   anything. Link straight to the assignment folder, not the repo root:
   `https://github.com/JDMAXilius/Multi-agents-class/tree/main/assignments/03-agent-crew`
   Use the zip **only** if the submission form demands a file upload.
2. **If uploading a zip:** run `./make_submission.sh` and attach
   `BREACHPOINT-agent-crew.zip`. Do not hand-zip the folder — a hand-zip omits the agent
   definitions and the crew exits immediately with "agent definitions not found," which is
   the entire Working Crew score. The script bundles them and self-tests before writing.
3. **Confirm the diagram renders.** Open the README on GitHub in a browser and look at the
   flowchart. A Mermaid block that renders as raw text scores 0 on the diagram criterion,
   and the failure is silent — it only shows in the browser.
4. **Write the late note.** Short, factual, no excuses. Suggested:
   *"Submitting Assignment #3 a day past the deadline — apologies for the delay. The crew is
   a four-agent pipeline that authors BREACHPOINT's weapon table and arena blockout spec
   under adversarial review. It runs with no API key via the included replay mode."*
5. **Submit**, then paste the confirmation into this ticket's Log.

## Done when

- [ ] Submitted, with the format used recorded in the Log
- [ ] Mermaid diagram confirmed rendering in a browser (not assumed)
- [ ] Late note sent to the instructor
- [ ] Confirmation / receipt noted in the Log

## Notes

- **Rubric map** — Working Crew /3.0: the run happened, `recording.json` is the evidence and
  replay reproduces it. Game Connection /3.0: output is Breachpoint's real tuning data, and
  the README names the game in its first line. Role Clarity /2.0: four agents, each
  load-bearing, with the removal test spelled out per agent. Architecture Diagram /1.0:
  inline Mermaid. README /1.0: the README itself.
- **If asked why not CrewAI:** the assignment allows "CrewAI *or raw orchestration code*."
  The README's "Why raw orchestration" section makes the case — and Class 04's own warning
  about what CrewAI hides (context tracking, rate limiting, output parsing) is the argument.
- **The strongest thing to point at** is not the code — it's what the crew *caught*: a Rocket
  headshot multiplier that made it a guaranteed one-shot, an AR headshot bonus that inverted
  the sandbox, and a schema defect the write-disabled verifier proved. All three are in
  `recording.json`, not asserted.
- Out of scope: any game-engine work (that is BP13 step 6 and BP14, on the crew board);
  further tuning of the landed data.

## Log

**29 Jul 2026 — verification run.** `python3 run_crew.py` (replay, stdlib only, no API key)
→ **exit 0**; full pipeline executed (producer → gate A → critic REFUTER → builder → gate B
→ verifier PASS). Both deliverables (`DT_Weapons.csv`, `arena_manifest.json`) regenerated
**byte-for-byte identical** to the committed copies — replay is deterministic. Manifest
validates as well-formed JSON (8 spawns). Architecture diagram rendered via
`@mermaid-js/mermaid-cli` (exit 0, valid SVG/PNG) — confirmed it renders, not assumed.
Working tree clean; `main` in sync with `origin/main`.
