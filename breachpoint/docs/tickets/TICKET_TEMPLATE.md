# TICKET — <short imperative title>

> STATUS: open — cut by <who/where> <date>. <One line: what unblocks it / what it needs.>

<Founder/lead directive, 1–3 lines: the WHY in plain words, including any founder laws that
bind this ticket (e.g. "server-authoritative only", "numbers in DataTables").>

**Ordering law:** <what gates what, if anything — e.g. "Step 1's contract fill-in gates all
code steps.">

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: <files-only | engine-installed | editor-live>
  <!-- The execution context this packet needs (AUTHORING-MATRIX §5). Checked FIRST, before
       any other condition — it is the cheapest gate and the most expensive to discover late.
       files-only = any session incl. cloud · engine-installed = local, headless, source-built
       UE 5.8 · editor-live = a UE editor must be OPEN before the claim (MCP / in-editor asset
       surface; R21: one editor, one driver, and no overlapping build). -->


- <condition a script can check: "`Content/Data/DT_Weapons.csv` exists and re-validates",
  "ticket BP0X is DONE (all boxes)", "`BRDataRows.h` compiles". An output existing is not
  enough — name the validator that must pass. No conditions? Write "- none (root ticket)".>
- owner_path: <the exact folders this ticket's builders may write — copied into
  `.claude/active-packet.json` on claim; the guard hook enforces it on every write>

## Steps (in order)

1. <Concrete step. Name exact files/modules/commands. Every step a fresh session could
   execute without asking questions.>
2. <…>
3. <For each build step: which crew agent owns it — netcode-builder / sim-builder /
   ui-builder / builder — and which contracts bind it.>
4. <The verification step: which ladder rungs (contracts/testing.md), run by the verifier.>
5. <The adversarial step, if a dangerous domain: critic REFUTER with the specific attack
   surface named.>

## Done when

- [ ] <Observable, binary outcomes only. "Rung 4 green on scenario X", not "netcode works">
- [ ] <…>
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: <who does what — one line>
- Binary files this ticket OWNS (lock before editing): <list or "none">
- Out of scope: <what a well-meaning session must NOT do here>

## Log

(append findings here, dated, newest last — this is what the next session reads)

---
<!-- EXAMPLE first ticket for a fresh adoption — delete after reading:
# TICKET — Bootstrap the validation ladder (rungs 1, 2, 4)
Steps: 1) fill testing.md fill-ins (engine paths, project names) [lead];
2) one pinned Automation Spec suite for an existing sim rule [sim-builder];
3) Gauntlet skeleton + ONE smoke scenario: dedicated server + 2 clients join,
   move, see each other [builder, netcode-builder consults];
4) verifier runs all three rungs and reports; critic prompt-hole review of
   the wiring. Done when: rungs 1/2/4 each produce a real pass/fail artifact
   in CI or locally, recorded in the Log. -->
