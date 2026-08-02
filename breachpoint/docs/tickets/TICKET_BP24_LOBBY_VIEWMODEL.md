# TICKET — BP24: There is no lobby ViewModel, so the entire front end has nothing to bind to

> STATUS: open — cut by the UI design pass, 2 Aug 2026. Gap 4 of 4 in
> `docs/UI-DESIGN-SYSTEM.md` §6, and by volume the largest of the four: it blocks **every**
> front-end screen. Owner BP10 + BP11.

Founder directive: the online layer is built and it is talking to nobody.
`UBRSessionsSubsystem` already broadcasts ten delegates covering host, search, join, invite,
travel, failure, disconnect and membership — and **not one line of UI subscribes to any of
them.** This ticket adds no netcode and no new replicated surface. It is one ViewModel class
that listens to what already fires.

**Ordering law:** step 1 (the VM) gates everything. Steps 2 and 3 are parallel after it.
Nothing here may add a session API — if the VM needs state the subsystem does not expose, that
is a `contract_gap` against BP11, not an edit (law 5).

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: engine-installed
- `Source/Breachpoint/Online/BRSessionsSubsystem.h` compiles and its ten delegates are public
  (verified 2 Aug — see the Log; re-check before claiming, because it decides whether this is a
  one-owner or two-owner packet)
- Ticket BP10 is DONE through step 1 — `BRUIManagerSubsystem` (CommonUI layers) and
  `BRActivatableWidget` exist, and `UBRVM_Combat`/`UBRVM_Match` prove the FieldNotify idiom
- CommonUI + ModelViewViewModel plugins enabled in `DefaultEngine.ini` with rung 1 green
- owner_path: `Source/Breachpoint/UI/`

## Steps (in order)

1. `UBRVM_Lobby : UMVVMViewModelBase` in `UI/BRViewModels.h` — the third ViewModel beside
   `UBRVM_Combat` and `UBRVM_Match`, built the same way: a `BindToSessions(UBRSessionsSubsystem*)`
   / `UnbindFromSessions()` pair mirroring `BindToAbilitySystem`, `ClearToUnknown()`, and
   `EBRUIDataState` on every group so a screen can render honestly before anything arrives.
   FieldNotify surface, one field per thing a screen draws: session state
   (`EBRSessionState`), `bIsHost`, the roster (`TArray<FBRLobbyRosterRow>` — display name, team,
   ready, host flag, local-player flag), search results, and the last failure/disconnect as
   player-facing `FText` derived from `FBRSessionOpResult` / `FBRDisconnectNotice`.
   Zero polling, zero `NativeTick`, zero property bindings. Owner: **ui-builder**.
2. Subscribe: all ten subsystem delegates —`OnSessionStateChanged`, `OnHostSessionCreated`,
   `OnSessionSearchCompleted`, `OnJoinTargetResolved`, `OnInviteAccepted`, `OnSessionJoined`,
   `OnClientTravelStarting`, `OnSessionFailure`, `OnDisconnected`, `OnSessionMembershipChanged`
   — plus `GameState->PlayerArray` change for the in-lobby roster. Every handler is unbound in
   `BeginDestroy`, matching `UnbindFromAbilitySystem`'s discipline. Owner: **ui-builder**.
3. Error text is data, not literals: failure and disconnect reasons resolve to `FText` through a
   table, not a `switch` full of string literals in C++ (law 3). If no table exists, file it as a
   `contract_gap` against BP13 rather than hard-coding. Owner: **ui-builder**.
4. Verify: rung 2 spec drives the VM through the full state machine with a fake subsystem —
   including **every failure path**, because a lobby is mostly failure paths. Rung 4 exercises
   host → search → join → travel with real clients. Owner: **verifier**.
5. **Critic REFUTER:** a delegate firing after the VM is destroyed; a search completing after the
   player left the screen; the roster during seamless travel (actors re-created, `PlayerArray`
   momentarily empty); a null `PlayerState` on the first frame; double-subscribe on re-entering
   the menu. Owner: **critic**.

## Done when

- [ ] `UBRVM_Lobby` exists and rung 1 is green on all three targets
- [ ] Every one of the ten subsystem delegates has a bound handler and a matching unbind —
      grep-proven, both directions (a missing unbind is a crash, not a leak)
- [ ] Rung 4 in threes on a real host/join: the **host** sees the roster grow, the **joining
      client** sees itself and the host, and an **observing client** already in the lobby sees the
      third member appear. A lobby claim that only tested the joiner is not a lobby claim.
- [ ] Every screen state has an honest empty rendering — `EBRUIDataState::Unknown` before data,
      never a stale or garbage frame (`netcode.md` law 7)
- [ ] Zero polling and zero property bindings (rung-2 grep gates, per BP10's precedent)
- [ ] Critic findings addressed or waived in the Log
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: ui-builder owns · verifier proves · critic refutes · netcode-builder consults only if
  step 1 discovers missing session state
- Contracts: `netcode.md` (**law 7 join/travel honesty is the governing one** — the lobby *is*
  the late-arriving-state surface: PlayerState null on first frame, arrays arriving
  incrementally, seamless travel re-creating actors; law 3 — the VM reacts, it never decides) ·
  `online-services.md` (the session lifecycle this VM mirrors; the VM must not become a second
  state machine racing the subsystem's) · `data-and-assets.md` (R18 — WBP holds layout only, no
  gameplay branch in a widget graph) · `testing.md` (rungs 2 + 4)
- **Blocks:** every front-end screen. `REFERENCE-EXTRACTION.md` §1 measures the reference file at
  **78 screens @1280×720 across 7 pages** (the design pass counts 75 as front-end — the small
  discrepancy is unreconciled, see the Log). `UI-DESIGN-SYSTEM.md` §4's inventory lists twelve
  components of which **nine are front-end** — `UBRNavBar`, `UBRMenuRow`, `UBRFeatureCard`,
  `UBRDescriptionStrip`, `UBRRosterPanel`, `UBRRosterRow`, `UBRProfileBar`, `UBRButtonPrompt`,
  `UBRProgressBar` — **all nine unbuilt, and none of them bindable, because there is no source.**
- **Bindable today without this:** *nothing on the front end.* This is the one gap of the four
  with no partial workaround — the in-match HUD binds almost completely (see BP22), but a lobby
  screen built today would have to fake every value. Static layout, tokens and geometry can be
  authored against the measured numbers without it; anything that displays state cannot.
- Binary files owned: none yet (`Content/UI/` holds `WBP_RootLayout`, `WBP_HUDLayout`,
  `WBP_KillfeedEntry`; the front-end WBPs are BP10's when they land)
- Out of scope: the WBP screens themselves (BP10 step 3), matchmaking policy, party/friends
  services, settings persistence, the Steam layer (BP11)

## Log

(append findings here, dated, newest last — this is what the next session reads)

**2 Aug 2026 — filed. The producer side is already finished; that is the surprise here.**

*Verified on disk:* `UI/BRViewModels.h` (279 lines) declares **exactly two** ViewModels —
`UBRVM_Combat` and `UBRVM_Match`. There is no third and no lobby type anywhere in `Source/`.
Meanwhile `Online/BRSessionsSubsystem.h` already exposes a full lifecycle: the operations
(`HostListenSession`, `FindAndJoinBestSession`, `JoinResolvedTarget`,
`LeaveSessionAndReturnToMainMenu`, `HostQuitToMainMenu`, `RequestMatchTeardown`,
`ValidateIncomingJoin`), the readable state (`GetSessionState`, `IsInSession`, `IsHosting`,
`GetActiveJoinTarget`, `GetLastDisconnectNotice`), the structs (`FBRSessionOpResult`,
`FBRDisconnectNotice`, `FBRHostSessionParams`, `FBRSessionQuery`, `FBRJoinTarget`) and **ten
public multicast delegates**. **This ticket is a consumer, not a feature** — which is why it is
scoped to one owner path and one crew member despite blocking the largest surface in the project.

*Open questions — stated, not guessed:*

1. **Where does the VM live?** `LocalPlayerSubsystem`, `GameInstanceSubsystem`, or created and
   owned by `BRUIManagerSubsystem`? The session subsystem is a `GameInstance` subsystem, so its
   lifetime outlives any map — a VM owned by a widget will be re-created per screen and must not
   double-subscribe. Nothing in `ue5-ui-architecture` was read for this filing; **load that skill
   before step 1** — it may already settle it.
2. **Is there a party concept before a session exists?** The subsystem's roster signal is
   `OnSessionMembershipChanged(int32, int32)` — two counts, no identities. A roster panel needs
   names and teams, which today can only come from `GameState->PlayerArray` **after** travel.
   So the pre-travel lobby roster may have no data source at all. **This is the most likely place
   this ticket discovers a real BP11 gap** — file it, do not add the API.
3. **75 vs 78 screens.** `REFERENCE-EXTRACTION.md` §1 measures 78 screens across 7 pages in file
   `Kn87U5sy2VD0lP8K7h4LcQ`; the design pass says 75 front-end. Probably 3 non-front-end frames,
   possibly a different count date. Not worth resolving to build the VM; worth resolving before
   anyone plans the screen work off the number.
4. **Settings screens have no backing at all** and are listed out of scope by BP10. If the front
   end's nav includes them, `UBRVM_Lobby` is not the whole answer and a settings VM is a fifth
   gap nobody has filed. **Flagged, not claimed** — the design pass named four gaps and this
   would be a new one.
