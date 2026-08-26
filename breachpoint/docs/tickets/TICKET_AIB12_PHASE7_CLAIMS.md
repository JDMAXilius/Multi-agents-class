# TICKET — AIB12: Phase 7 proof — the claims board (headless + inertness)

> STATUS: open — cut 26 Aug 2026 by the cloud lead with the Phase-7 build ("WRITTEN,
> NOT COMPILED"). TERMINAL WORK: compile, specs, one negative live check. The design,
> the two-audit barrier, and the closed rulings are docs/AIBOT-PHASE7-PACKET.md.

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

1. Rung 1: all targets compile (everything above is WRITTEN, NOT COMPILED).
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

- [ ] Rung 1 + 108/108
- [ ] FFA inertness grep pasted (0 grants)
- [ ] Watch-list APIs confirmed or the failures pasted
- [ ] (optional) raised-tier negative pasted

## Log

_(terminal: outputs verbatim)_
