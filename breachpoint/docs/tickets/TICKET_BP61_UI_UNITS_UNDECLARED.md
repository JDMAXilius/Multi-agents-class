# TICKET — BP61: Three `UI/` units exist on disk that §3 does not declare

> STATUS: open — `contract_gap` G2, filed by BP26's cut, 2 Aug 2026. This is **D11**, which is
> already on `docs/DECISIONS-OWED.md` as an open **RULING** with a recommendation — it needs
> deciding and recording, not opening. Gated by BP60.

Founder directive: `BREACHPOINT-ARCHITECTURE.md` §3.9 declares **4** `UI/` units; disk holds
**7**. `BRRootLayout`, `BRUISettings` and `BRUITypes` are real, landed, working C++ that the
manifest does not know about. Nothing breaks mechanically — §3 is self-consistent and the
self-check passes — but **§3 is the document the architect parses and the one a new session reads
first**, and the gap gets wider with every front-end packet. D11 is worth clearing this week
precisely because it blocks nothing: it will never be cheaper than it is now.

**Ordering law:** BP60 gates every step. Declaring a unit today makes `architect.py --all` exit 2
on the hard-coded `43`/`44` budget (`architect.py:469`), so the fix cannot even be validated until
BP60 lands.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: files-only
- **BP60 is DONE (all boxes).** Verify by declaring one unit and confirming
  `python3 Tools/architect/architect.py --all` still exits 0.
- The gap is still open: `Tools/architect/state/perception.json` `undeclared` still reads
  `{"AbilitySystem": ["BRCombatCurves"], "UI": ["BRRootLayout", "BRUISettings", "BRUITypes"]}`,
  and `docs/BUILD-STATE.md:74` still lists the same three under `UI/`.
- `BREACHPOINT-ARCHITECTURE.md:266` still reads `### 3.9 `UI/` — 4` while
  `ls Source/Breachpoint/UI/*.h` returns **7** distinct unit names.
- owner_path: `BREACHPOINT-ARCHITECTURE.md`, `docs/DECISIONS-OWED.md`

## Steps (in order)

1. Read `docs/DECISIONS-OWED.md` §D11 (~L441) in full. The question is already framed, the three
   options (a/b/c) are already written, and it already carries a recommendation — **(b), declare
   some and exclude the rest by name with the reason stated**, on the grounds that §4 already has
   that mechanism for the seven `UBRGE_*` library classes. Do not re-derive it. Owner: **lead**.
2. Record the ruling in `docs/DECISIONS-OWED.md` D11 with a date. **D11 covers four units, not
   three** — `BRCombatCurves` (`AbilitySystem/`) is the fourth and is decided by the same ruling;
   declaring only the three UI ones leaves the register half-answered. Owner: **lead**.
3. Apply it to `BREACHPOINT-ARCHITECTURE.md` §3.9 (and §3.3 / §4's exclusion list, as the ruling
   directs), updating the section header count and the §4 composition budget to match. Owner:
   **builder**.
4. Verify: `python3 Tools/architect/architect.py --all` exits 0, the self-check arithmetic reaches
   the new budget, and `undeclared` no longer lists the ruled-on units (or lists them as
   *excluded*, per the ruling). Owner: **verifier**.

## Done when

- [x] D11 is recorded as **decided** in `docs/DECISIONS-OWED.md`, with a date and the reason, and
      it covers **all four** units it names — not only the three in `UI/` — **and landed in the
      rulings ledger as R39**, which the register's own preamble requires and which was missing
- [x] `BREACHPOINT-ARCHITECTURE.md` §3.9's header count equals its own unit table **and** equals
      what is on disk under the ruling's terms — `--all` prints `UI/ 6 6 ok`
- [x] `architect.py --all` exits **0**, and `state/perception.json`'s `undeclared` list agrees
      with the ruling — required a CODE change (`RULED_EXCLUSIONS`), not just §4 prose; all four
      D11 names cleared, 43 → 41 remaining
- [x] `docs/BUILD-STATE.md` regenerates without listing a ruled-on unit as undeclared —
      `BRRootLayout` and `BRUISettings` now appear in the DECLARED table, not the UNDECLARED list
      (use `--stdout`; the bare write path is broken on Python 3.9.6 and is a law-5 write)
- [x] Findings + decisions written to this ticket's Log

## Notes

- Crew: lead (the ruling) · builder (the edit) · verifier
- Binary files this ticket OWNS (lock before editing): none
- Out of scope: touching `Tools/architect/` (BP60) · adding, renaming or deleting any C++ unit ·
  declaring the ~40 units the front-end board will add later — this ticket closes the **existing**
  gap and leaves a mechanism that the next packet can use
- Correction to the proposal, verified 2 Aug 2026: the source text says to *"open as D11"*. **D11
  is already open** — `docs/DECISIONS-OWED.md:46` (register row, status `RULING`) and `:441` (full
  body with options and a recommendation). The work is to close it.

## Log

**3 Aug 2026 — landed (lead, Mac terminal). D11 decided (b), ledgered as R39.**

**Kickoff was stale in three places and the ticket was executed anyway, deliberately — the
ruling did not depend on the stale numbers.** `ls Source/Breachpoint/UI/*.h` returns **8**, not
the 7 the ticket asserts (`BRHUDDirector` landed in `b2eaaf8`), and 43 counting sub-folders.
`undeclared` was **not** the four names the ticket quotes — a fresh scan finds 45. What the
ticket asks is still answerable: D11 named four specific units, and those four are what R39
decides.

**The split (option b).** `BRRootLayout.h/.cpp` and `BRUISettings.h` declared in §3.9 (4 → 6);
`BRUITypes` and `BRCombatCurves` excluded by name in §4's new item 3. Arithmetic: per-folder sum
43 → 45, budget 44 → 46, across §3's header, §3.9, and the four prose sites at the old lines 84,
322, 328-329, 334, 336.

**`BRUISettings` is declared `.h`, NOT `.h/.cpp`.** `Source/Breachpoint/UI/BRUISettings.cpp` does
not exist — checked before writing it. `classify()` reads the declared FORM, so a `.h/.cpp`
declaration would report a finished header-only settings object as a **STUB** and offer it to the
architect as work to build. The form is part of the declaration, not cosmetics.

**This ticket is also BP60's box 1, and that is the real result.** BP60 claimed the budget now
comes from the doc rather than a Python literal. This declaration proves it: §3.9's header, its
body, and §4's total were edited and **no Python changed**, and `--all` went straight to exit 0
with `UI/ 6 6 ok`, `§4 states 46`, SELF-CHECK PASSED. Under the old hard-coded
`if total_declared != 43 or budget != 44` this same edit would have exited 2 until someone
patched the scanner.

**Box 3 was FALSE on the first attempt and needed code, not prose.** With §4's item 3 written,
`perception.json` still listed `BRUITypes` and `BRCombatCurves` as undeclared, because
`undeclared_files()` skipped only `declared | {GE_HEADER.stem}` — a hard-coded constant. §4's
named-exclusion device was **prose that nothing read**, so the ruling would have been "landed"
while the tool went on contradicting it forever. Added `RULED_EXCLUSIONS`, printed with the other
declared exclusions (never silently applied), plus a self-check that FAILS if a ruled exclusion
names a header that is not on disk — an exclusion for a deleted file hides nothing and misleads
everyone. `undeclared` 43 → 41, all four D11 names cleared.

**A false claim was committed and is corrected here, on the record.** §4 item 3 first justified
the no-extension convention by asserting that §4 falls inside §3.12's parse span, and said
*"Verified by running it."* Both halves were wrong: `parse_manifest` ends the last section at
`text.find("\n## 4.", ...)`, which resolves against the real heading, so §4 is **outside** the
span — and that verification had not been run. A crew REFUTER pass caught it. Then actually run:
injecting `BRUITypes.h/.cpp` and `BRCombatCurves.h/.cpp` into §4 leaves UI 6/6, Tests 3/3, total
45 — unchanged. The convention is kept because the **end-of-file fallback** fires if that heading
is ever renamed, which is one edit nobody would think of as a manifest change.

**Ledger gap closed (was `high`).** `BREACHPOINT-ARCHITECTURE.md` cited "ruling D11(b)" three
times and `grep -c D11 docs/DESIGN-RULINGS.md` returned **0** — the architecture citing a ruling
that did not exist, while `DECISIONS-OWED.md`'s own preamble says a decision becomes real only
when it lands in the ledger. Written up as **R39** (R38 was highest) and all three citations now
read `D11(b) → R39`.

**Also corrected:** §4 exclusion 1 said `AbilitySystem/` holds **8** headers; disk has **15**
(8 `Abilities/`, 5 root, 1 `Cues/`, 1 `Effects/`). Third correction to that one sentence — the
file number is decoration and keeps rotting; the class-name rule is the load-bearing half.
BP60's contract_gap G3 discharged in the same pass: the GE library is now "8 total"/"all eight"
with a `GE_GrenadeCost` row added to §3.3's table.

**Left open, deliberately — do not read this ticket as closing the drift.** 41 `BR*` headers
remain undeclared: 37 in `UI/`, plus `BRGA_Jump`, `BRGameplayCues`, `BRExplosion`,
`BRProjectile` (the subject of the still-open **D6**), and `Camera/BRPlayerCameraManager` — in a
folder §3's tree does not list and §9's owner-path map does not own, so it has **no owner under
law 5**. Recorded in R39's "the limit" paragraph. A successor register item is owed before §3 can
be called the manifest again. Not blocked by tooling: BP60 ruled `UNIT_RE` already covers every
sub-folder on disk. Blocked only on someone deciding them.

**Stale elsewhere, outside owner_path, not fixed:** `docs/ASSIGNMENT-5.md` cites "the full list
of 44" and a BUILT/STUB/MISSING split that no longer holds — it is a graded external deliverable,
so it wants a pass before 6 Aug. `docs/BUILD-STATE.md` regenerates from `--all` and self-corrects.
