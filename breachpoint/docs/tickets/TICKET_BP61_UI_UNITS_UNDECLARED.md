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

- [ ] D11 is recorded as **decided** in `docs/DECISIONS-OWED.md`, with a date and the reason, and
      it covers **all four** units it names — not only the three in `UI/`
- [ ] `BREACHPOINT-ARCHITECTURE.md` §3.9's header count equals its own unit table **and** equals
      what is on disk under the ruling's terms
- [ ] `architect.py --all` exits **0**, and `state/perception.json`'s `undeclared` list agrees
      with the ruling
- [ ] `docs/BUILD-STATE.md` regenerates without listing a ruled-on unit as undeclared
- [ ] Findings + decisions written to this ticket's Log

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

(append findings here, dated, newest last — this is what the next session reads)
