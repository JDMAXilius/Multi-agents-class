# TICKET — Submit Assignment #4 (Dynamic Content Pipeline)

> STATUS: **ready to submit.** Due was *before session 7 — 30 July 11:59 ET*; this lands
> late and the note below says so plainly. Kept in `assignments/` (never on the crew
> board) so course work never ships into the game repo.

Everything the assignment asks for is built, run live, and pushed. What remains is the act
of submitting, plus the checks that protect the criteria most easily lost to a packaging
mistake rather than to the work: **RAG Implementation /2.0** (does `rag_trace.md` render
so the grader can see query/chunk/output side by side?) and the blanket rule that **code
which does not run scores 0 across every criterion**.

## What is already done

- [x] Pipeline — `run_pipeline.py` + `rag.py` + `gaps.py` + `crew.py`, stdlib only
- [x] Knowledge base is the **live** GDD and shipped tables, not a copy
- [x] Three gaps **proven from disk** before any model call (`gaps.py`), a fourth proven
      and deliberately not filled
- [x] Three outputs landed in `output/`, none written by hand
- [x] `rag_trace.md` — query, retrieved chunk, output, side by side, all three jobs
- [x] `judge_log.md` — the pool, the ranking, and what the judge discarded
- [x] `critic_log.md` — findings with pipeline-computed before/after diffs
- [x] Live run completed and recorded; replay verified with no API key, exit 0
- [x] Drives the project's **real** crew definitions (`spotter.md`, `critic.md`), loaded
      from `breachpoint/.claude/agents/` rather than copied
- [x] Zip path verified: `make_submission.sh` mirrors the KB **and** the two agent
      definitions, then refuses to build a package that fails its own replay
- [x] Committed and pushed to `main`

## Steps (in order)

1. **Pick the delivery format.** Prefer the **repo link** — GitHub renders the Mermaid
   architecture diagram and the `<details>` blocks in `rag_trace.md`, and the grader can
   browse `output/` without downloading anything. Link straight to the assignment folder:
   `https://github.com/JDMAXilius/Multi-agents-class/tree/main/assignments/04-content-pipeline`
   Use the zip **only** if the submission form demands a file upload.
2. **If uploading a zip:** run `./make_submission.sh` and attach
   `BREACHPOINT-content-pipeline.zip`. Do not hand-zip the folder — a hand-zip omits
   `kb/` and `agents/`, and the pipeline then exits with "knowledge-base source missing"
   or "crew agent definitions not found". That is the whole submission. The script mirrors
   both and self-tests the replay from the staged copy before writing the archive.
3. **Verify the run on the grader's path**, from a clean checkout:
   ```
   rm -f output/DT_*.csv && python3 run_pipeline.py && ls -l output/DT_*.csv
   ```
   All three CSVs must come back. Replay re-executes retrieval, both gates, the canon
   lint, the telemetry-field check and the judge's selection arithmetic — only the model
   responses are served from `recording.json`.
4. **Confirm the trace renders.** Open `output/rag_trace.md` on GitHub in a browser. The
   `<details>` blocks carry the verbatim retrieved chunks; if they render as raw text the
   RAG criterion is being judged on something unreadable, and the failure is silent.
5. **Write the late note.** Short, factual, no excuses. Suggested:
   *"Submitting Assignment #4 past the deadline — apologies for the delay. The pipeline
   retrieves from BREACHPOINT's own GDD and shipped DataTables, generates a pool of
   candidates, ranks them with a critic in JUDGE mode, and only then runs the adversarial
   pass. It runs with no API key via the included replay mode. The ReadMe's §6 documents a
   defect the pipeline caught in its own first run."*

## What a grader should be pointed at

| Criterion | Open this |
|---|---|
| Game-Anchored Source | `rag.py` → `KB_SOURCES`, and README §1 |
| Content Fit | `output/gap_report.md` — the gaps are proven, not asserted |
| RAG Implementation | `output/rag_trace.md` |
| Consistency Checking | `output/critic_log.md` + README §6 |
| Voice Judgment | README §7 (measured) and §9 (the retrieval tweak, with `output/naive/`) |

## Notes

- **Nothing here is owed to the game repo.** The three CSVs live in this assignment's
  `output/` and are deliberately NOT copied into `breachpoint/Content/Data/`. Importing
  them is a separate ticket with a binary-asset lock, and the README says so rather than
  implying the content is live.
- The pipeline reads the crew's definitions and never writes them. If `spotter.md`
  changes, this pipeline inherits the change on the next run — including its character
  caps, which `crew.py` parses out of the file rather than hard-coding.
