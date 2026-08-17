# Provenance — this is a frozen copy, not the live project

Everything in this folder was copied from the BREACHPOINT game tree by
`../freeze_project.sh`. **Nothing here is authored for the assignment**, and
nothing here is edited by hand.

| | |
|---|---|
| Source | `breachpoint/` in this repo |
| Pinned commit | `13a3882ac8348955026dc979cd0e126127d4b6e7` (`13a3882`) |
| Commit date | 2026-08-12T03:10:11+00:00 |
| Contents | `GDD.md` + 110 headers + 4 subsystem bodies + 7 data tables |
| Size | 1.1M |

## Why a freeze at all

The live tree changes daily. An agent that scans it would give a different
answer every run, and the README's claims would rot within a day of being
written. Pinning the target makes the run reproducible: `agent.py` on this
folder gives the same perception, the same gaps and the same ranking today as
it did when the README was written.

## What this copy deliberately does NOT contain

- **`.cpp` bodies**, except the four `BR*Subsystem` pairs. Whether a unit
  exists is decided by its declaration; bodies would triple the size and change
  no answer.
- **Non-`BR` sources** — engine template leftovers (`Variant_*`,
  `breachpoint*`) are not units the architecture declares.
- **Any write path back to the live tree.** Code the agent generates lands in
  `project/Source/`, here, in the frozen copy. Porting it into the real game is
  a separate, manual step under a ticket — see the README.

## Re-pinning

```
./freeze_project.sh      # re-copies from breachpoint/ and rewrites this file
```

Re-pinning invalidates the committed run: `output/` and `recording.json`
describe the codebase as it was at the pin above.
