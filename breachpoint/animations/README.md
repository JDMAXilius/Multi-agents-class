# `animations/` — reading Blueprints without the editor

Tooling and evidence for reading animation Blueprints **from the checked-out files**: no editor,
no MCP server, no project loaded.

## Why it is separate from `mcp-bp/`

`mcp-bp/read_graphs.py` and `bp_extract.py` talk to a live editor on `:8000` and take a path per
asset. They work, and they have one structural weakness that `docs/ANIM-PORT-LEDGER.md` already
paid for: a curated target list is a claim about what matters, and every conclusion drawn from it
inherits that claim silently. *"14/14 found" read as completeness and only ever meant "14/14 of
what I was told to look at."*

This reader walks the tree, so the files answer the coverage question instead of the list. It
**complements** `mcp-bp/` and does not replace it — see Limits.

## Contents

| Path | What |
|---|---|
| `abp_offline_extract.py` | the reader |
| `FINDINGS.md` | first-run findings, and the measured accuracy behind them |
| `reports/ABP_Mannequin_*.{json,md}` | per-asset declaration inventories |
| `reports/sweep.{json,md}` | every graph-bearing asset under `Content/FPSTemplate`, and whether the inventory knows it |

Run from the project root (`breachpoint/`). Standard library only — no engine, no plugins, no
network. Read-only on every asset it touches.

```bash
# one asset, with the accuracy cross-check on
python animations/abp_offline_extract.py \
    "Content/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base.uasset" \
    --inventory mcp-bp/bp_inventory.json --out animations/reports

# coverage sweep over a content folder
python animations/abp_offline_extract.py --sweep Content/FPSTemplate \
    --inventory mcp-bp/bp_inventory.json --out animations/reports
```

## What it reads

The package **name table** — every node class, declared property, referenced package, function
and graph name the asset introduces.

Accuracy is measured rather than asserted: `--inventory` cross-checks each run against the
live-editor extraction already committed in `mcp-bp/bp_inventory.json` and prints the delta. On
`ABP_Mannequin_Base` that is **88 of 96**, and all 8 misses are stock `UAnimInstance` members the
Blueprint inherits rather than declares. Without `--inventory` the tool reports `props=?` and
declines to name declared properties at all, because nothing distinguishes a Blueprint's
`AimPitch` from an engine node's own property by spelling. The rule that tried — "declared
properties are lowerCamel", 8 correct out of 116 — is recorded in the source with its score so it
does not get reinvented.

## Limits

**No topology.** Pin links, execution order, state transitions, blend weights: not read, and not
guessed. That data lives in the export table's tagged property streams, and this project's
packages come from a source engine build whose `FPackageFileSummary` does not match the
documented layout — legacy file version −9, and a 20-byte `SavedHash` where the package GUID used
to be. Export walks were attempted and rejected by their own validator rather than published at
low confidence. (The same mismatch is why the name table is located by structural probe instead
of by reading `NameOffset` out of the summary.)

So: **what is in a graph, never how it is wired.** A verdict needing topology needs the editor,
and should say so.

## Status

Written 11 Aug 2026. Claimed under no ticket, and it changes no ledger verdict — `sweep.md`'s
coverage count is offered as *input* to `docs/ANIM-PORT-LEDGER.md`, which remains where verdicts
are decided.
