# TICKET — BP60: `architect.py` cannot represent a growing UI folder

> STATUS: open — `contract_gap` G1, filed by BP26's cut, 2 Aug 2026. Blocks BP61 and every packet
> that adds a `BR*` unit under `Source/Breachpoint/UI/`. `Tools/architect/` is in no UI ticket's
> owner path, so it is filed, not fixed inline (law 5).

Founder directive: the architect is the mechanism that proves the codebase and
`BREACHPOINT-ARCHITECTURE.md` §3 still describe the same program. The front-end board adds ~40 UI
units. **Today the architect refuses the first one**, on two independent counts, and a scanner
that fails on correct growth teaches the crew to stop running it — which is the whole failure mode
it exists to prevent.

**Ordering law:** the two counts are independent; fix either first. Both must land before BP61
can declare a unit, and BP61's declaration is the acceptance test for this ticket.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: files-only
- `python3 Tools/architect/architect.py --all` exits **0** on the tree as it stands today (the
  baseline this ticket must not break — the self-check currently passes at 43 + 1 = 44).
- The hard-coded budget is still there: `Tools/architect/architect.py:469` reads
  `if total_declared != 43 or budget != 44:` and `:470-472` exits 2 on mismatch.
- `UNIT_RE` at `Tools/architect/architect.py:139` is still
  `` r"`((?:[A-Za-z]+/)?BR[A-Za-z0-9_]+)\.(h/\.cpp|h|cpp)`" `` — **one optional path segment**, so
  `Components/Core/BRMenuRow.h` does not match. `SECTION_RE` at `:135` requires `[A-Za-z]+` for the
  folder name, so a section header naming a two-deep folder cannot be written either.
- owner_path: `Tools/architect/`

## Steps (in order)

1. Replace the hard-coded `43`/`44` at `:469` with a budget the doc itself states, so declaring a
   unit is a one-place edit that the self-check then verifies — not a code change. Whatever shape
   is chosen, **it must still fail when the doc and the scanner disagree**; the check exists
   because a silent desync is the bug. Owner: **builder**.
2. Decide, and write down in this ticket's Log, whether two-deep source paths are supported at all.
   Plan `curried-booping-waffle.md` §1.1 argues **one level of nesting only** for
   `Source/Breachpoint/UI/`, on the grounds that `AbilitySystem/` (the biggest folder, 30 files) is
   one-deep and nothing in the module is two-deep. **If that holds, the cheapest fix is no code
   change at all** — record the ruling and leave `UNIT_RE` alone. If the board wants two-deep,
   widen `UNIT_RE` **and** `SECTION_RE` together, or a declarable unit still cannot get a section
   header to live under. Owner: **builder**, ruling by the board.
3. Re-run `python3 Tools/architect/architect.py --all` and confirm `state/perception.json` still
   regenerates. Owner: **verifier**.

## Done when

- [ ] Declaring one new `UI/` unit in `BREACHPOINT-ARCHITECTURE.md` §3.9 leaves
      `architect.py --all` at **exit 0** — demonstrated with a real declaration, not a dry run
- [ ] The self-check still **exits 2** when the §3 header count and its own unit table disagree
      (the regression this ticket must not introduce)
- [ ] The two-deep question is answered in this Log — either "one level only, `UNIT_RE` unchanged"
      or "widened, with `SECTION_RE` widened alongside it"
- [ ] `Tools/architect/state/perception.json` regenerates and `docs/BUILD-STATE.md` is still
      derivable from it
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: builder · verifier
- Binary files this ticket OWNS (lock before editing): none
- Out of scope: **declaring any unit** (that is BP61) · editing `BREACHPOINT-ARCHITECTURE.md`
  (BP61's owner path) · any change to what the scanner scans on disk
- Verified against the file, 2 Aug 2026: both claims hold exactly as stated —
  `:469` hard-codes `43`/`44`, `:139`'s `UNIT_RE` takes at most one path segment, `:135`'s
  `SECTION_RE` forbids a slash in the folder name.

## Log

(append findings here, dated, newest last — this is what the next session reads)
