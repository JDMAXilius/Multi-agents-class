# TICKET — BP62: `LogBRUI` does not exist, and four other channels the docs promise do not either

> STATUS: open — `contract_gap` G3, filed by BP26's cut, 2 Aug 2026. Needs an **exact-file**
> `owner_path` grant on `BRCore.h`/`.cpp`. Not blocking BP26, but every UI packet after it is
> writing diagnostics with nowhere to send them.

Founder directive: `BREACHPOINT-ARCHITECTURE.md:114` says `BRCore.h/.cpp` carries
*"Log channels (LogBRCombat/Net/AI/Online/UI)"*. **R24** treats those five as the existing set and
adds `LogBRInput` by a rule it calls *"mechanical, not case-by-case"*. **R38** then binds every
sub-folder to its parent's channel — so `UI/Components/` and `UI/Screens/` inherit `LogBRUI` and
must never mint their own. All of that assumes `LogBRUI` exists. **It does not.**

R24's own warning applies exactly here: a missing log channel does not fail a compile, it fails an
acceptance check months later. BP01 hit this with `Input/` and had to fall back to
development-only `ensureMsgf` for every diagnostic.

**Ordering law:** none internally — one declaration, one definition. This ticket should land
before the first UI packet that needs to log, i.e. alongside or just after BP26.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: engine-installed
- The gap is still open: `grep -rn "LogBRUI" Source/` returns **nothing**, and
  `grep -rn "DECLARE_LOG_CATEGORY" Source/` returns exactly two lines —
  `Source/Breachpoint/Breachpoint.h:5` (`Logbreachpoint`, the Epic template's) and
  `Source/Breachpoint/Core/BRCore.h:8` (`LogBRAbility`).
- `Tools/run-ubt.sh` is green on all three targets with an R19 timestamp proof before the edit.
- The **exact-file** grant below has been written into `.claude/active-packet.json` — this packet
  touches two files inside another discipline's folder and nothing else. Precedent: R23 and R25
  both chose exact files over a folder split, and `guard_laws.py:73` accepts an exact-file entry
  (`rel == o`).
- owner_path: `Source/Breachpoint/Core/BRCore.h`, `Source/Breachpoint/Core/BRCore.cpp`

## Steps (in order)

1. `BREACHPOINT_API DECLARE_LOG_CATEGORY_EXTERN(LogBRUI, Log, All);` in
   `Source/Breachpoint/Core/BRCore.h` beside the existing declaration, and the matching
   `DEFINE_LOG_CATEGORY(LogBRUI);` in `BRCore.cpp`. Owner: **builder**.
2. Decide and record, in this Log, what happens to the **other four** channels §3.1 promises
   (`LogBRCombat`, `LogBRNet`, `LogBRAI`, `LogBROnline`) and to `LogBRInput`, which R24 says is
   *"added now"* and which is not on disk either. **Declare only what a landed packet actually
   uses** — a channel nothing logs to is dead code that makes the doc look satisfied. Whatever is
   left undeclared gets stated here so the next reader is not misled a second time. Owner: **lead**.
3. Verify: rung 1 green on all three targets under an R19 proof; a UI diagnostic emitted on
   `LogBRUI` appears under `-LogCmds="LogBRUI Verbose"` and **does not** appear when the channel is
   filtered out. Owner: **verifier**.

## Done when

- [ ] `LogBRUI` is declared in `BRCore.h` and defined in `BRCore.cpp`, rung 1 green on all three
      targets under an R19 timestamp proof
- [ ] One real UI diagnostic routes to it and is provably filterable (appears with the channel
      enabled, absent with it disabled) — **not** a `UE_LOG` that nothing ever reaches
- [ ] The status of the other four §3.1 channels and of R24's `LogBRInput` is written down in this
      Log: declared, or deliberately not, with the reason
- [ ] The diff touches **exactly two files** — `BRCore.h` and `BRCore.cpp` — and nothing else
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: builder (the two lines) · lead (step 2's call) · verifier
- Binary files this ticket OWNS (lock before editing): none
- Out of scope: a `LogBRUI` sub-channel of any kind — **R38 rules directly against it**
  (`UI/Components/` inherits the parent folder's channel; a second stream is one a filter can
  miss) · converting existing `Logbreachpoint` call sites · any edit inside
  `Source/Breachpoint/UI/`
- Sharper than the proposal claimed, verified 2 Aug 2026: the source says *"`BRCore.h:8` declares
  exactly one channel, `LogBRAbility`"*. True — **and `LogBRAbility` is not one of the five names
  §3.1 promises.** So it is not that four of five channels are missing; it is that **all five are
  missing** and the one that exists is undocumented. R38 already records the softer half of this
  (*"`LogBRCombat` and `LogBRNet` map to no §3 folder"*) and deliberately leaves the naming alone —
  this ticket adds `LogBRUI` and does not relitigate that.

## Log

(append findings here, dated, newest last — this is what the next session reads)
