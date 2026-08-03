# TICKET — BP74: The six signals the HUD renders as "unknown" forever

> STATUS: open — cut 3 Aug 2026 by `UBRHUDDirector` (`b2eaaf8`). **files-only to write,
> BP71 to verify, and gameplay-adjacent — it needs a critic pass, not a UI-lane free hand.**

Founder directive is law 1 and the honesty ladder together: the HUD director wires every signal
the game already publishes, and **files a gap for every signal the game does not**. Six ViewModel
feeds have no game-side event to subscribe to. Each one is a HUD element that will render its
honest unknown state forever until a producer exists — which is correct behaviour and a
permanently wrong screen.

**Ordering law:** each of the six is independent. Two already have tickets and are NOT
re-litigated here (see Notes). The four that remain are ordered by what they unblock, not by
size — team id first, because two HUD surfaces gate their entire display on it.

## Kickoff (machine-checkable)

- requires: **files-only** to author; **engine-installed** (BP71 green) to verify
- `Source/Breachpoint/UI/BRHUDDirector.cpp` exists and its class comment lists six gaps
- owner_path: per sub-item — see each step. **This ticket writes OUTSIDE `Source/Breachpoint/UI/`
  and that is the whole reason it is a separate packet.** A UI packet that reaches into
  `Match/` to add a delegate is exactly the law-5 violation the guard hook blocks.

## The six

| # | Signal | ViewModel feed waiting for it | Game side today |
|---|---|---|---|
| 1 | **local team id** | `UBRVM_Match::SetLocalTeamId` | `ABRPlayerState` exposes no team accessor at all |
| 2 | **match phase tag** | `SetMatchPhaseTag` | `EBRMatchPhase` enum exists; no `Match.Phase.*` tag vocabulary |
| 3 | **grapple cooldown / ready** | `SetGrappleCooldownStarted` / `SetGrappleReady` | the GE + cooldown tag exist; nothing publishes the transition to UI |
| 4 | **rocket spawn / available** | `SetRocketSpawnServerTime` / `SetRocketAvailable` | BP09's scope; no replicated spawn time |
| 5 | **hit-marker confirmation** | `ReportHitMarker` | `UBRAttributeSet::OnDeath` exists; no per-hit server confirm to the instigator |
| 6 | **spotter line** | `AppendSpotterLine` | the LLM path is unbuilt |

## Steps (in order)

1. **Team id (#1) — the highest-value one and the smallest.** `UBRMatchBand::Refresh` gates
   BOTH score slots on `LocalTeamId ∈ {0,1}`, and `UBRKillfeed::ResolveEntryTint` dims every row
   whose team it cannot resolve. So with no producer, **the band shows dashes and the whole feed
   renders `ink-dim` for the entire match** — two visible surfaces, one missing accessor.
   `FBRKillFeedEntry` already carries `KillerTeamId`/`VictimTeamId`, so the concept exists on
   the wire; what is missing is *the local player's own*. Owner: `Source/Breachpoint/Match/`,
   **netcode-builder** — a replicated team id on `ABRPlayerState` is law-1 surface and needs the
   critic REFUTER pass.
2. **Match phase tag (#2).** Decide first whether the VM should take the tag or the enum. It
   takes `FGameplayTag` today, and `BRGameplayTags.h` has no `Match.Phase.*` family — so either
   the tag family lands (owner: `Source/Breachpoint/Core/`) or the VM's field changes to the
   enum (owner: UI, and cheaper). **This is a design call disguised as a wiring gap**; make it
   explicitly rather than adding four tags to satisfy a signature.
3. **Hit-marker confirmation (#5).** The most player-visible of the six — the reticle has a
   full four-kind marker path, a timer, priority resolution and art slots, and **nothing has
   ever fired it**. Needs a server→instigator confirm on damage application carrying
   shield/flesh/headshot/kill. Owner: `Source/Breachpoint/AbilitySystem/`, and it is
   `gas-purity` contract surface: the confirm rides the existing damage pipeline as a cue or a
   client RPC to the instigator, and **does not** become a second damage path.
4. **Grapple (#3).** BP06 owns the grapple. This is a two-line publish from wherever the
   cooldown GE is applied and removed — file it against BP06 rather than building a parallel
   path.
5. **Rocket (#4)** is BP09's, **spotter (#6)** is the LLM packet's. Neither is this ticket's to
   build; both are recorded here so the ViewModel's unfed fields are explained rather than
   mistaken for dead code by the next audit.
6. **Verify** after BP71: PIE and confirm each landed signal moves its HUD element off the
   unknown state. Rung 2 at best; the team-id change is replicated, so it needs rung 4 in
   threes (server, acting client, observing client) before "works" is said out loud.

## Done when

- [ ] #1 team id: `ABRPlayerState` publishes it, the director pushes it, the band shows real
      scores and the feed shows real team colours — **proven in threes at rung 4** (R30)
- [ ] #2 phase: the tag-vs-enum call is made and recorded, and the chosen side is wired
- [ ] #5 hit marker: a server-confirmed hit lights the reticle in PIE, and the confirm is a cue
      or an instigator RPC — **not** a second damage pipeline (critic REFUTER signs this off)
- [ ] #3 filed against BP06, #4 against BP09, #6 against the LLM packet — with the exact feed
      name each one owes
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: **netcode-builder** owns #1 (replicated state + REFUTER); **sim-builder** owns #5 with
  the `gas-purity` contract binding; **ui-builder** owns only the director-side push and #2's
  UI half. The critic reviews #1 and #5 before either lands.
- Binary files this ticket OWNS: none.
- Out of scope: **building #3, #4 or #6 here.** They belong to tickets that already exist, and
  re-implementing them in a UI packet is how a second grapple cooldown path gets written.
- Already-ticketed cousins NOT restated: **BP22** (reticle target state), **BP23** (respawn
  countdown), **BP25** (weapon icon path), **BP65** (motion tracker feed). Four more HUD feeds
  with their own tickets — check them before filing anything new.

## Log

(append findings here, dated, newest last)
