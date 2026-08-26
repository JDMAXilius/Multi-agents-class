# TICKET — AIB8: the ~600–770uu unreachability cluster vs the 400uu jump link

> STATUS: open — cut 26 Aug 2026 by the cloud lead from AIB6's own findings. TERMINAL
> WORK: measuring and tuning against the live arena; no module code is expected to change.

AIB6's diagnosis was unanimous: every remaining move refusal (30/30 in one match) is
GENUINE unreachability — both endpoints on-mesh, mover refuses — and the straight-line
distances cluster hard (min 593 / median 615 / max 771, a ~180uu band). Random
unreachability does not cluster; one unspanned arena feature does. The widest generated
nav link is `JumpLength=400` (`Config/DefaultEngine.ini:291`); a gap wider than the
longest link yields no link.

AIB6 deliberately did NOT claim this proven: the logged distance is bot-to-goal, not
gap width. This ticket is the confirmation it asked for.

## Steps

1. In the editor on the arena map, visualise the navmesh + generated links around the
   spots the AIB6 log names (the refusal lines carry positions if verbosity allows —
   else stand bots at the median-615 pairs and eyeball the unconnected islands).
2. Measure the actual gap(s). If ≤ the character's real jump reach: raise `JumpLength`
   (and re-check `BN_Drop`/`BN_Climb` heights) until the link generates; rebuild nav;
   re-run one match and re-paste AIB6's refusal table — the claim is CLOSED when the
   `self=yes goal=yes` count collapses the way flee refusals did (267 → 0.8).
3. If the gap exceeds honest jump reach, the fix is LEVEL geometry (a ramp, a crate) or
   accepting the island split — founder's call; paste the measurement and stop.
4. Record the numbers in this Log either way — the refusal table is the instrument.

## Done when

- [ ] Gap(s) measured and named (location + width)
- [ ] Either the link generates and the refusal table collapses, or the founder ruling
      on geometry is requested with measurements attached
- [ ] AIB6's table re-run and pasted for the after

## Log

_(terminal: outputs verbatim)_
