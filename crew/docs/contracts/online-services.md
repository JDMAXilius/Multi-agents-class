# Contract — Online Services (seams are abstract; platform trust ends at the match)

Status: v1 · Owner: services-builder · Binds every packet touching `Source/Breachpoint/Online`:
sessions, lobby, host/invite flow, server lifecycle.
The boundary in one line: **services deliver players into a match and manage the server
around it; authority inside the match is netcode's.**

## Laws

1. **Migration seams are interfaces, today.** Sessions hide behind
   `BRSessionsSubsystem::FindAndJoinBestSession` (Steam OSS now, FlexMatch later); server
   lifecycle hides behind `IBRServerLifecycle` (listen server now, GameLift later). Callers
   never learn the backend; a caller branching on the vendor is a finding. The seam is only
   real if the swap needs zero caller changes.
2. **Platform trust is scoped and named.** Steam is trusted for **identity and session
   membership** — nothing else. No platform datum (ID, ticket, invite, lobby metadata) grants
   gameplay authority or skips in-game validation. The Spotter Agent's API key is host-side
   config; it never appears in replicated state, logs shipped to clients, or crash reports.
3. **Join-in-progress is a first-class path.** Every services API and every consumer handles
   late-arriving state: PlayerState null on first frames, incremental array arrival, seamless
   travel re-creating actors, backfill joining mid-match. Each packet's acceptance criteria
   include at least one join-in-progress case. "Worked from map start" is half a test.
4. **Listen-server flows are tested twice.** Host and remote client are different code paths
   (local player present, no travel for host, host advantage). Every flow — create, invite,
   join, leave, host-quit, backfill — states its host behavior and its remote behavior
   separately. **Host-quit behavior is defined and tested, never discovered in production.**
5. **Lifecycle is event-driven and documented.** Consumers subscribe to delegates (session
   created/joined/failed, lobby roster changed, match handoff, shutdown); firing order and
   failure paths are documented HERE as they land. A consumer forced to poll a subsystem is a
   services bug.
6. **The lobby/sessions boundary is named.** In the slice, pre-match assembly happens
   in-match during the Warmup phase; `BRSessionsSubsystem` owns platform session state.
   When a real lobby arrives (Phase 2, `BRLobbySubsystem`) it owns roster/loadout/ready —
   a feature blurring the two files a contract_gap.
7. **Failure is a state, not an exception.** Session create/join failures, invite rejections,
   and mid-join disconnects produce defined UI-visible states (via the normal replication/
   ViewModel path) — never a silent hang in a lobby that will "probably" resolve.

## Breachpoint specifics

- Topology at ship: **Steam listen server, invite-first** (host + invite; quickmatch cut).
  Bots backfill empty slots (ai-builder's domain) — services' job is delivering the humans.
- OSS: Steam Online Subsystem; PIE uses the Null subsystem — claims about invites/joins name
  the rung: "works (Steam, 2 machines)" or they are opinions (`testing.md` applies).
- Session flow at ship: MainMenu → Host (create session + travel to `BR_Arena01` as listen
  server) / Join (accept invite → `FindAndJoinBestSession` → travel) → roster assembles in-match
  during Warmup phase (no separate lobby in the slice; `BRLobbySubsystem` is Phase 2) →
  match start via `IBRServerLifecycle`.
- Dedicated-server future: GameLift arrives behind `IBRServerLifecycle` as a Phase-2 swap;
  nothing at this scope may assume the host has a local player *except* explicitly-marked
  listen-server-only code behind the interface.
