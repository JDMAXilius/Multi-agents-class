# TICKET — BN18: teams at rung 5 — multiplayer in threes (T6, the DONE bar)

> STATUS: QUEUED — opens when BN15+BN16 pass their terminal proofs (rungs 1–4).
> SERIAL protocol, not a wave: one live listen+2-client session (the editor/live-
> session rule — the aib-editor exemption's reasoning). Per netcode law every claim
> below is made in THREES: server / acting client / observing client. Teams are the
> first feature since the scores whose whole point is replicated state — this ticket
> is why the roadmap called rung 5 non-optional.
>
> Session: listen server + 2 clients, `bTeamsEnabled=True`, humans on OPPOSITE teams
> (force via assignment order or the tagging script's start layout), bots filling.

## The scripted asserts, in order (each row is three checks, log or eyes-on)

1. **Identity**: after join, each machine reads every PlayerState's TeamId identically
   (server log, acting client scoreboard, observing client scoreboard). The two human
   clients see MIRRORED relative colors — my block first and Ally-blue on each own
   screen, the SAME player Threat-red on the opposing screen. One screenshot each.
2. **Late join, honest window**: connect client 2 mid-match. For at most a frame-window
   its own rows read the None palette (today's FFA colors), then re-tint WITHOUT any
   further gameplay event (the deferred OnTeamChanged subscription firing — BN16's
   whole attack surface, proven by the re-tint happening with no kill in between).
3. **FF gate in threes**: acting client shoots a teammate — server logs the Verbose
   refusal (harness `ff_refused` > 0), acting client sees no hit cue, observing client
   sees no health movement. Self-grenade STILL damages on all three.
4. **Team kill credit**: force one team kill (FF off blocks damage, so use the
   environment or toggle `bFriendlyFire=True` for this row and say so in the log):
   server prints `team kill — no credit`, NO team point moves on any machine's band,
   the killfeed line still shows on both clients.
5. **Ledger + winner**: play to the team score limit. All three machines' bands agree
   at every increment (team point lands within one update on both clients); at the
   buzzer the WINNING team's client reads YOUR TEAM WINS / Victory tint, the losing
   client ENEMY TEAM WINS / Defeat — from the replicated ints, with a leaver removed
   before the end to prove the ledger (not a PlayerArray sum) is the record.
6. **The degenerate cheat, as ABSENCE OF SERVER EFFECT** (BN15 ruling — client
   self-correction is not the provable claim): on a client, call the team setter and
   raw-poke the byte in memory. Server: cheater's TeamId, FF outcomes, ledger all
   unchanged. Acting client: the setter no-ops even locally (authority gate).
   Observing client: no OnRep fires, scoreboard unchanged.
7. **PostMatch join**: connect a third client during PostMatch — it renders the
   decided team outcome immediately (WinningTeamId rides the same bunch as
   MatchState; the BN15 ruling this row either proves or refutes).
8. **Travel**: map-restart via the seamless path — TeamIds survive on every machine,
   the ledger honestly resets, spawns still respect the team tags.

## Done when

- [ ] All eight rows logged in threes, screenshots for rows 1/2/5 attached
- [ ] Any refuted ruling goes back to its ticket as a finding, not patched inline
- [ ] The roadmap's T6 row checked — the team framework is DONE at its own bar

## Log

_(outputs verbatim)_
