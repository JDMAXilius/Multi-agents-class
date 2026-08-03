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

- [x] Declaring one new `UI/` unit in `BREACHPOINT-ARCHITECTURE.md` §3.9 leaves
      `architect.py --all` at **exit 0** — demonstrated with a real declaration, not a dry run
      → **BP61 declared TWO** (`BRRootLayout.h/.cpp`, `BRUISettings.h`), §3.9 4 → 6, budget
      44 → 46. `--all` exit 0, `UI/ 6 6 ok`, `§4 states 46`. **No Python changed to accept it** —
      which is the whole claim of this ticket, proven by the edit rather than asserted
- [x] The self-check still **exits 2** when the §3 header count and its own unit table disagree
      (the regression this ticket must not introduce) — `test_selfcheck.py` **60/60, exit 0**,
      including two NEW cases guarding the doc-parsed budget branch
- [x] The two-deep question is answered in this Log — **one level only, `UNIT_RE` and
      `SECTION_RE` unchanged**, on corrected grounds (§3 declares relative to the discipline
      folder; the ticket's "nothing is two-deep" premise is false — eight sub-folders exist)
- [x] `Tools/architect/state/perception.json` regenerates and `docs/BUILD-STATE.md` is still
      derivable from it — `--all` exit 0 rewrote perception + ranking + blackboard;
      `build_state.py --stdout` exit 0, 64 rows. **Bare invocation is broken on Python 3.9.6
      (`newline=` needs 3.10+) and is also a law-5 write; use `--stdout`.**
- [x] Findings + decisions written to this ticket's Log

## Notes

- Crew: builder · verifier
- Binary files this ticket OWNS (lock before editing): none
- Out of scope: **declaring any unit** (that is BP61) · editing `BREACHPOINT-ARCHITECTURE.md`
  (BP61's owner path) · any change to what the scanner scans on disk
- Verified against the file, 2 Aug 2026: both claims hold exactly as stated —
  `:469` hard-codes `43`/`44`, `:139`'s `UNIT_RE` takes at most one path segment, `:135`'s
  `SECTION_RE` forbids a slash in the folder name.

## Log

**3 Aug 2026 — landed (lead, Mac terminal). Budget now parsed from the doc; suite 60/60.**

**Kickoff line 1 was FALSE and had been since 1 Aug 15:24.** The ticket says the baseline is
`architect.py --all` at exit 0. It exited **2**: `SELF-CHECK FAILED: expected 7 GE classes,
found 8`. `UBRGE_GrenadeCost` landed in `40af3b5` and `GE_CLASS_COUNT_EXPECTED` was never
bumped, so the run died at that assertion **before** the budget check, before the disk scan,
before `perception.json` was written. `test_selfcheck.py` was red for the same reason, and the
failing case was the **control** ("a faithful copy must PASS") — the suite was reporting a
failure of the tree when the failure was its own.

Fixed inside owner_path (`architect.py`). The doc half — §3.3's "seven"/"all seven" prose and
§4's exclusion 1 — is `BREACHPOINT-ARCHITECTURE.md`, which is **BP61's** owner_path; routed
there rather than reached across. See the contract_gap below.

**Step 1 — the 43/44 literals are gone.** `stated_budget()` parses §4's
`| **Total budget** | **N** |` row (line 336). Declaring a unit is now a doc edit, not a code
edit, which was the whole ticket. An **absent** row is a hard failure, never a pass: a check
that disables itself the moment the doc stops stating the number is the desync it exists to
catch. Verified: `--all` → **exit 0**, printing `= §4 composition budget ... 44` /
`§4 states ... 44` / `SELF-CHECK PASSED`.

**Step 2 — RULING: ONE level of nesting. `UNIT_RE` and `SECTION_RE` unchanged. Zero code.**

The ticket's stated grounds are wrong against disk and are replaced. *"`AbilitySystem/` is
one-deep and nothing in the module is two-deep"* is false — the module is two-deep in **eight**
places: `AbilitySystem/{Abilities,Cues,Effects}/` and `UI/{Components,HUD,Screens,Styles,
ViewModels}/`.

The grounds that actually hold: **§3 declares paths RELATIVE to the discipline folder.** §3.3
declares `` `Abilities/BRGA_WeaponFire.h/.cpp` ``, never
`` `AbilitySystem/Abilities/BRGA_WeaponFire.h/.cpp` `` — verified by grep, which returns zero
declarations carrying a discipline-folder prefix. So `UNIT_RE`'s one optional segment already
covers every sub-folder that exists, and `SECTION_RE`'s `[A-Za-z]+` already covers every §3
header, because a sub-folder's units live under their PARENT folder's section. The shape
`UNIT_RE` cannot express is `Components/Core/BRMenuRow.h` — **two** sub-folder segments — and
`find -mindepth 3 -type d` returns nothing. Widening both regexes to satisfy a path nobody has
written is YAGNI. If a future packet needs it, widen `UNIT_RE` and `SECTION_RE` **together**;
splitting them is the trap the ticket correctly names.

**Step 3 — regression.** Two cases added for the new branch, because without them a later edit
could delete §4's row or desync it and nothing would notice: *"§4's total-budget row deleted"*
and *"§3.9 declares one more, §4's total left behind"*. Both REJECT with exit 2.
`test_selfcheck.py` → **60/60, exit 0**, control green.

**Box 4 — `BUILD-STATE.md` is derivable, via `--stdout` only.** `build_state.py --stdout` →
exit 0, 64 table rows. **Do not run it bare:** `:334` calls
`OUT.write_text(text, encoding="utf-8", newline="\n")` and `newline=` is Python 3.10+ while this
machine is **3.9.6**, so the file-write path raises `TypeError`. That path also writes
`docs/BUILD-STATE.md`, which is outside this packet's owner_path — through `pathlib`, where the
hook is blind. The compliant path and the working path are the same one, which is lucky rather
than designed. The committed `BUILD-STATE.md` is **69 diff lines stale** against a fresh
generation; not fixed here (outside owner_path), and box 4 says "derivable", not "regenerated".

**contract_gap G3 → BP61 (which owns `BREACHPOINT-ARCHITECTURE.md`).** BP05 landed an 8th
generic GE class. The doc still says "seven"/"all seven" in §3.3's library paragraph and §4's
exclusion 1, and §3.3's GE table has 7 rows with no `GE_GrenadeCost`. BP60 bumped the code
constant to 8 because `--all` cannot reach exit 0 otherwise; until BP61 lands the prose, the
scanner's GE assertion is checked against a CODE constant the doc contradicts — exactly the
desync class this scanner exists to catch, recorded rather than hidden.

**Also found, not fixed (outside owner_path):** `curried-booping-waffle.md`, cited by this
ticket's step 2 and by BP26 and BP65, is **not in the repo** — it lives at
`~/.claude/plans/`. This ruling rests on disk, not on it.

**Box 1 is verified by BP61**, which declares `BRRootLayout` + `BRUISettings` — a real
declaration, not a dry run, exactly as the box demands. Checked when that lands.
