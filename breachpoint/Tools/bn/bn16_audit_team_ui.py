#!/usr/bin/env python3
"""BN16 — editor-side READ-BACK AUDIT of the team UI surface. INSPECTION ONLY.

Writes nothing. Saves nothing. Starts no PIE. Every call in this file is a read.

WHAT IT PROVES, AND WHAT IT CANNOT
----------------------------------
BN16's team surface is C++ (law 7 / R18): relation tints, the two-block partition and the
relative band are all written by BNScoreRow.cpp / BNKillfeedEntry.cpp / BNMatchBand.cpp /
BNScreen_Scoreboard.cpp at runtime, onto leaves the WBP placed. So the WBP asset carries
exactly ONE kind of BN16 obligation: it must PLACE, under the exact expected name, every
leaf the C++ binds — because a `BindWidgetOptional` that finds no match is silently null
and its feature simply never renders, with no compile error and no log line.

  A) MODULE — is the BN16 C++ actually inside the running editor's loaded binary?
     Probed on the UBNVM_Match CDO: the five FieldNotify team properties either exist on
     the loaded UClass or they do not.

  B) ASSET — does each WBP's widget tree contain the names the C++ binds? Required
     (`BindWidget`) names are a compile gate the editor already enforces; the OPTIONAL
     ones are the real finding surface, because their absence is invisible.

It does NOT prove any tint is on screen.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "mcp-ui" / "gen_ui"))
from mcp import MCP, UMG, OBJ  # noqa: E402

INTENT = {
    "/Game/BN/UI/WBP_BNScoreRow": {
        "cpp": "UBNScoreRow",
        "required": ["NameText", "KillsText", "DeathsText"],
        "optional": {},
        "bn16": "row tint by Relation is SetColorAndOpacity on the ROW (BNScoreRow.cpp:38) "
                "— multiplies down the whole tree, needs no new leaf.",
    },
    "/Game/BN/UI/WBP_BNMatchBand": {
        "cpp": "UBNMatchBand",
        "required": ["MyKillsText", "ClockText"],
        "optional": {
            "TopKillsText": "BN16 enemy-team score readout (and DefaultTopKillsTint capture)",
            "ScoreLimitText": "shared limit text, correct in teams mode",
            "SelfScoreBar": "my team's score bar (MyTeamScoreFraction)",
            "TopScoreBar": "enemy team's score bar (EnemyTeamScoreFraction)",
        },
        "bn16": "relative band reuses the FFA readouts; no BN16-new leaf.",
    },
    "/Game/BN/UI/WBP_BNKillfeedEntry": {
        "cpp": "UBNKillfeedEntry",
        "required": ["LineText"],
        "optional": {
            "KillerText": "BN16 killer part-tint by KillerRelation — REQUIRED for BN16 ON",
            "VictimText": "BN16 victim part-tint by VictimRelation — REQUIRED for BN16 ON",
            "WeaponIcon": "glyph; dimmed when the row is relation-tinted",
        },
        "bn16": "part tints need BOTH KillerText and VictimText bound (bParts gate, "
                "BNKillfeedEntry.cpp:29). Without them the row falls back to the composed "
                "LineText and NO relation tint can ever render.",
    },
    "/Game/BN/UI/WBP_BNScreen_Scoreboard": {
        "cpp": "UBNScreen_Scoreboard",
        "required": ["RowContainer"],
        "optional": {
            "BannerText": "winner/warmup line",
            "OutcomeText": "per-player Victory/Defeat",
            "OutcomeAccent": "outcome stripe",
            "MyTeamScoreText": "BN16 team header, my side — REQUIRED for BN16 ON",
            "EnemyTeamScoreText": "BN16 team header, their side — REQUIRED for BN16 ON",
        },
        "bn16": "the ONLY BN16-new asset obligation in the whole packet: two header texts. "
                "Two-block order is claim ORDER in C++ (Refresh), not a tree change.",
    },
}

VM_TEAM_FIELDS = ["bTeamsMode", "MyTeamScore", "EnemyTeamScore",
                  "MyTeamScoreFraction", "EnemyTeamScoreFraction"]
VM_CDO = "/Script/BreachpointNext.Default__BNVM_Match"


def ref(p):
    return {"refPath": p}


def main() -> int:
    m = MCP()
    m.init()
    findings: list[tuple[str, str]] = []

    print("# BN16 TEAM-UI EDITOR AUDIT — read-only\n")

    print("## A. Module probe — is BN16 C++ in the running editor's binary?\n")
    props, raw = m.call(OBJ, "list_properties", {"instance": ref(VM_CDO)})
    names = raw if isinstance(raw, str) else json.dumps(raw)
    print(f"`list_properties({VM_CDO})` ->")
    print(f"```\n{names[:2000]}\n```\n")
    for f in VM_TEAM_FIELDS:
        hit = f.lower() in names.lower()
        print(f"- {'PASS' if hit else '**MISSING**'} `UBNVM_Match::{f}`")
        if not hit:
            findings.append(("high", f"UBNVM_Match::{f} absent from the loaded CDO — the "
                                     f"editor is running a PRE-BN16 binary"))
    print()

    print("## B. Asset probe — does each WBP place the leaves the C++ binds?\n")
    for path, spec in INTENT.items():
        print(f"### `{path}` (C++ `{spec['cpp']}`)\n")
        tree, raw = m.call(UMG, "GetWidgetDescription",
                           {"widgetBlueprint": ref(f"{path}.{path.rsplit('/', 1)[1]}"),
                            "startWidget": None, "maxDepth": -1})
        if tree is None:
            print(f"**FAILED** — {raw[:400]}\n")
            findings.append(("high", f"{path}: GetWidgetDescription failed — {raw[:160]}"))
            continue
        desc = tree.get("description", "") if isinstance(tree, dict) else str(tree)
        present = {w.get("widgetName") for w in (tree.get("widgets") or [])} \
            if isinstance(tree, dict) else set()
        print("```\n" + desc.strip() + "\n```\n")
        print(f"widget names in tree: `{sorted(n for n in present if n)}`\n")

        for n in spec["required"]:
            ok = n in present
            print(f"- {'PASS' if ok else '**MISSING**'} BindWidget `{n}` (required)")
            if not ok:
                findings.append(("high", f"{path}: required BindWidget `{n}` not in tree — "
                                         f"the WBP cannot be compiling"))
        for n, why in spec["optional"].items():
            ok = n in present
            sev = "REQUIRED for BN16 ON" in why
            tag = "PASS" if ok else ("**MISSING**" if sev else "absent")
            print(f"- {tag} BindWidgetOptional `{n}` — {why}")
            if not ok and sev:
                findings.append(("high", f"{path}: `{n}` not placed — the BN16 feature it "
                                         f"carries CANNOT render ({why})"))
        print(f"\n> {spec['bn16']}\n")

    print("## Findings\n")
    if findings:
        for sev, f in findings:
            print(f"- **{sev}** — {f}")
    else:
        print("- none")
    print("\n## Rung honesty — what this audit does NOT mean\n")
    print("Editor-loaded module + placed leaves. NOT: compiled-and-linked for Server/Client "
          "targets, NOT PIE, NOT multiplayer, NOT that any pixel is the right color. Every "
          "tint here is written at runtime by C++ over the WBP's stored default, so no "
          "stored color in these assets is evidence about the rendered frame.")
    return 1 if findings else 0


if __name__ == "__main__":
    raise SystemExit(main())
