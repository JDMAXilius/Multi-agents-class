# TICKET — Submit Assignment #5 (Goal-Oriented Coding Agent)

> STATUS: in-progress — mac terminal 18 Aug 2026. Verification is green and the zip is
> built; what remains is the act of submitting. Kept in `assignments/` (never on the crew
> board in `breachpoint/docs/tickets/`) so course work never ships into the game repo.

Everything the assignment asks for is built, run, and pushed. What remains is submitting,
plus the checks that protect the criteria most easily lost to a packaging mistake rather
than to the work: the blanket rule that **code which does not run scores 0 across every
criterion**, and the README's claim that a grader needs nothing but **Python 3.8+**.

## Kickoff (machine-checkable)

- requires: files-only — no engine, no API key, no network
- `./verify.sh` exits 0 from a clean state (it deletes the generated C++ and re-runs first)
- `./make_submission.sh` exits 0 — it refuses to write the archive unless the staged copy
  passes `./verify.sh` on its own
- working tree clean and pushed to `origin/main`

## What is already done

- [x] Agent — `agent.py`, stdlib only, five stages: read GDD → scan source → detect gaps →
      rank → generate C++
- [x] Reasoning layer makes **no model call** — `python3 agent.py --rank` reaches the
      selection with the model unreachable
- [x] Target is a **frozen** copy of BREACHPOINT (`project/`, pinned at `13a3882`), and
      `guard_path()` refuses to write outside it
- [x] Generated feature landed: `UBRSpotterSubsystem` (16,051 bytes), verify clean, gap
      closed on the agent's own re-scan
- [x] Replay works with no API key (`recording.json`); `--live` does the real call
- [x] `verify.sh` — every requirement re-derived from that run, evidence printed per check
- [x] `TICKET_VERIFY.md` — the grader-facing explanation of each check
- [x] README answers the three required questions by heading
- [x] `verify.sh` runs on **Python 3.9** (D1's stdlib check no longer needs 3.10+ —
      see Log, 18 Aug 2026)
- [x] `./make_submission.sh` builds `BREACHPOINT-goal-oriented-agent.zip` (364K, 167 files)
      and self-tests the replay from the staged copy
- [x] Committed and pushed to `main` — `126ba7e`
- [ ] Submitted on the course form

## Steps (in order)

1. **Prefer the repo link.** GitHub renders the README's tables and the ticket files, and
   the grader can browse `output/` without downloading anything:
   `https://github.com/JDMAXilius/Multi-agents-class/tree/main/assignments/05-goal-oriented-agent`
   Use the zip **only** if the form demands a file upload.
2. **If uploading a zip:** run `./make_submission.sh` and attach
   `BREACHPOINT-goal-oriented-agent.zip`. Do not hand-zip — the script proves the package
   runs from the staged copy before writing it, and the zip is gitignored on purpose so a
   stale archive can never be the thing submitted.
3. **Point the grader at one command**, not at prose:
   ```
   cd BREACHPOINT-goal-oriented-agent && ./verify.sh
   ```
   Exit 0 = every assignment requirement satisfied by that run.
4. **Write the note.** Short, factual:
   *"Assignment #5 — a goal-oriented agent that reads BREACHPOINT's GDD, scans its source,
   finds what the design promises and the code lacks, ranks the gaps with printed terms and
   no model call, and only then writes the C++. `./verify.sh` checks every requirement from
   a clean re-run; it needs Python 3.8+ and nothing else — no API key, no network, no
   engine."*

## What a grader should be pointed at

| Criterion | Where the evidence is |
|---|---|
| It runs | `./verify.sh` — deletes the output, re-runs the agent, checks each requirement |
| Reads the GDD | `output/perception.json` — 13 features, verbatim from GDD.md §5.1 |
| Scans the codebase | `output/perception.json` — 114 files, 303 declarations, 7 data tables |
| Reasoning is auditable | `output/ranking.json` — every term for every candidate; the winner recomputes by hand |
| Generated code | `project/Source/Breachpoint/Telemetry/BRSpotterSubsystem.{h,cpp}` |
| Honest limits | README §"Were you able to run it in your game?" — never compiled, never in a running build |

## Log

### 18 Aug 2026 — pre-submission verification

`./verify.sh` failed one check on this machine: **D1 (a complete, runnable agent)** raised
`AttributeError: module 'sys' has no attribute 'stdlib_module_names'`. That attribute is
Python **3.10+**; the local interpreter is 3.9.6, and `TICKET_VERIFY.md` promises graders
**3.8+**. So the failure was the verifier's, not the agent's — but it was live on the exact
claim the ticket makes to the grader, and `make_submission.sh` gates the zip on `verify.sh`,
so it would have blocked packaging too.

Fixed at the root: D1 now resolves each import through `importlib.util.find_spec` and
classifies by spec origin (built-in/frozen, or under `sysconfig` stdlib and not a
`-packages` path) instead of consulting a 3.10-only set. Same verdict on 3.10+, and it now
holds on 3.9.

After the fix: **ALL CHECKS PASSED**, and `./make_submission.sh` built the archive with the
staged copy passing `verify.sh` on its own — 364K, 167 files.
