# TICKET — AIB12: Phase 7 proof — the claims board (headless + inertness)

> STATUS: open — cut 26 Aug 2026 by the cloud lead with the Phase-7 build
> (~~"WRITTEN, NOT COMPILED"~~ — **corrected 28 Aug 2026: it compiles; all targets build
> clean and the module suite reads 119/119/0**, which supersedes the 108 expected below).
> TERMINAL WORK REMAINING: **the negative live check only** — the FFA inertness grep.
> The design, the two-audit barrier, and the closed rulings are
> docs/AIBOT-PHASE7-PACKET.md.

## What landed (cloud)

- `Team/AIBClaimsBoard.h/.cpp` — NEW headless core (claims array; key = actor else
  Kind+100uu cell; TTL + stale-pawn + ReleaseAll belts; injected hostility predicate).
- `Team/AIBTeamCoordinator.h/.cpp` — the stub became the shell: world subsystem,
  refuses client worlds and pawn-backed targets, GRANT/DENY/RELEASE log lines.
- `Interfaces/AIBWorldQuery.h` — `FAIBPointOfInterest::bClaimableSlot` (slot vs zone,
  provider-declared, default false — hills stay zones).
- `Core/AIBTypes.h` — `FAIBObjectiveFact::bClaimedElsewhere` (present-zero) +
  `AIB::ClaimTtlSeconds = 5`.
- `Brain/` — selector `ObjectiveClaimedElsewhere`; `BuildModeAmbitionSpec` attaches the
  falling claim consideration (claimed ⇒ exactly 0 ⇒ the zero-score veto releases a
  committed loser in one rescore; VWU=1 so the no-fact silence keeps one owner).
- `Core/AIBFactsBuilder.cpp` — honours the board (Teamwork ≥ Trained), skips
  other-claimed slots for distance, sets the flag only when slots existed and ALL are
  spoken for (a zone in the set keeps the want alive).
- `Execution/AIBStateTreeTasks.cpp` — `FAIBMoveToObjectiveTask` pick mirrors the filter.
- `Core/AIBBotController.cpp` — files/renews the claim in Think() right after Rescore
  (same-frame cluster resolves in strict order); ReleaseAll on OnUnPossess + EndPlay.
- `Tests/AIBClaimsSpec.cpp` — NEW, 11 specs. **Module spec total: 108.**

No game-side files changed. No new tree nodes: the probe list stays 20 — but the owed
SeekWeapon→Seek RENAME landed in the same push (FAIBGateSeekCondition,
FAIBSeekDestinationTask; probe list updated to the new /Script paths), so the tree
rebuild AIB11 already owes is now MANDATORY before any PIE run: a stale ST_AIBBot
still names the old structs and will not load against this code.

## Steps (terminal)

1. ~~Rung 1: all targets compile (everything above is WRITTEN, NOT COMPILED).~~ **DONE
   28 Aug — all targets build clean.**
2. Specs: `AIBot.Sim.*` — expect **108/108** (Claims 11 new; totals history: 91 → 95
   Phase 6 → 97 P4+5 barrier → 108).
3. Watch-list (assumed APIs to confirm at compile): `FObjectKey` equality + construction
   from `const UObject*`/`this`; `TFunctionRef` from a free/static function pointer;
   `FIntVector` `==` in the key compare; second `UE_DEFINE_GAMEPLAY_TAG_STATIC` cpp in
   Tests/ (duplicate-tag-name registration across TUs should dedup — if the checker
   objects, rename the spec tag).
4. LIVE (negative instrument, one FFA PIE match, standard config): grep the log —
   - `claim GRANTED` count must be **0** (default Teamwork=Novice: dormant), and
   - zero `claim DENIED` / `claims RELEASED` lines.
   This is a countable absence, not an impression — paste the grep.
5. OPTIONAL second negative: temporarily raise Teamwork to Trained in the defaults row,
   one more FFA match — STILL zero grants (nothing claimable: hills are zones, and the
   all-hostile predicate binds nobody). Revert the row after. Paste the grep.
6. The positive proof (two allied bots, one pickup, contested count 0) is DEFERRED —
   waits for teams + claimable slots in a host mode; the roadmap row carries this.

## Done when

- [x] Rung 1 (all targets clean) + module suite green — **count re-pinned 108 → 119/119/0
      on 28 Aug**; 108 was this phase's number and eight waves of pins have landed since
- [ ] FFA inertness grep pasted (0 grants)
- [ ] Watch-list APIs confirmed or the failures pasted
- [ ] (optional) raised-tier negative pasted

## Log

_(terminal: outputs verbatim)_

### 2026-08-28 — board-hygiene pass: compiled; the countable absence is still owed

Corrected, not measured. The header said "WRITTEN, NOT COMPILED" for a build that Phases
8, 9 and 10 were later stacked on — Phase 10 physically moved this code to
`Plugins/AIBot/` and it still compiles. All targets clean this session, AIBot 119/119/0.
The watch-list APIs in step 3 (`FObjectKey` equality, `TFunctionRef` from a free function,
`FIntVector` `==`, the second `UE_DEFINE_GAMEPLAY_TAG_STATIC` TU) are therefore all
CONFIRMED by the fact of the build — that box could be ticked by inspection, but the
ticket asks for confirmation *or the failures pasted*, so it is recorded here instead.

**Still open, and it is the interesting one:** step 4's FFA inertness grep. Later tickets
have been leaning on this result as if it were banked — BN15's Done-when says "AIB12 FFA-
inert holds OFF", BN17's protocol 6 says "AIB12's result still holds", AIB13's step 3 says
it "must NOT change". None of them can hold what was never measured. **Zero grants in FFA
is still an unrun grep**, and it is one match's work.
