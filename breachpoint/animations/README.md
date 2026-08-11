# `animations/` — offline Blueprint reading

Tooling and evidence for reading animation Blueprints **from the checked-out files**, with no
editor and no MCP server running.

## Why this is separate from `mcp-bp/`

`mcp-bp/read_graphs.py` and `bp_extract.py` talk to a live editor on `:8000` and take a path per
asset. That works, and it has one structural weakness that `docs/ANIM-PORT-LEDGER.md` already
paid for: a curated target list is a claim about what matters, and every conclusion drawn from it
inherits that claim silently. "14/14 found" read as completeness and only ever meant "14/14 of
what I was told to look at."

The reader here walks the tree instead, so the files answer the coverage question rather than the
list. It is a **complement** to `mcp-bp/`, not a replacement — see the limit below.

## Contents

| Path | What |
|---|---|
| `abp_offline_extract.py` | the reader |
| `FINDINGS.md` | what the first run found, and how accurate it was measured to be |
| `reports/ABP_Mannequin_*.{json,md}` | per-asset declaration inventories |
| `reports/sweep.{json,md}` | every graph-bearing asset under `Content/FPSTemplate`, and whether the inventory knows it |

## Running it

Per asset, with the accuracy cross-check enabled:

```bash
python animations/abp_offline_extract.py \
    "Content/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base.uasset" \
    --inventory mcp-bp/bp_inventory.json \
    --out animations/reports
```

Coverage sweep over a content folder:

```bash
python animations/abp_offline_extract.py --sweep Content/FPSTemplate \
    --inventory mcp-bp/bp_inventory.json --out animations/reports
```

Read-only on every asset it touches. Standard library only; no engine, no plugins.

## What it reads, and what it refuses to guess

It parses the package **name table**: every node class, declared property, referenced package,
function and graph name the asset introduces. Accuracy is measured, not asserted — passing
`--inventory` cross-checks the result against the live-editor extraction already committed in
`mcp-bp/bp_inventory.json` and prints the delta. On `ABP_Mannequin_Base` that is 88 of 96, with
all 8 misses being stock `UAnimInstance` members the Blueprint inherits rather than declares.

**It does not read topology** — pin links, execution order, state transitions, blend weights. That
data lives in the export table's tagged property streams, and this project's packages come from a
source engine build whose `FPackageFileSummary` does not match the documented layout (`SavedHash`
in place of the package GUID, legacy file version −9). Export walks were attempted and rejected by
the validator rather than reported at low confidence.

So: **what is in a graph, never how it is wired.** A verdict that needs topology needs the editor,
and should say so.

## Status

Written 11 Aug 2026. Not claimed under a ticket, and no ledger verdict is changed by it — the
`sweep.md` coverage number (99 graph-bearing assets on disk vs 27 in the inventory) is offered as
input to `ANIM-PORT-LEDGER.md`, which remains the place verdicts are decided.
