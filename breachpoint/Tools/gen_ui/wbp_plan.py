#!/usr/bin/env python3
"""The committed plan half of the WBP generator. Pure CPython — imports no engine.

This file is the reviewable artifact R37.1 requires: a WBP's layout expressed as
diffable text. `build_wbp.py` executes it; nothing else decides what a WBP contains.

The load-bearing idea: a widget marked `bind: True` MUST correspond to a
`meta = (BindWidget)` member on the C++ parent, and that correspondence is checked
HERE, against the real header, before an editor is ever opened.

`BindWidget` desync is otherwise UE's nastiest UI failure — the C++ renames a member,
the WBP keeps the old name, the widget fails to compile *at asset load* rather than at
build time, so rung 1 stays green and the HUD is simply empty in PIE. Checking it at
plan time turns that into a text-mode error nobody can miss.
"""
from __future__ import annotations

import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

# Class paths, named once. A typo here is a silent "widget not created".
OVERLAY = "/Script/UMG.Overlay"
CANVAS = "/Script/UMG.CanvasPanel"
STACK = "/Script/CommonUI.CommonActivatableWidgetStack"

FILL = {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill",
        "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}}


# ---------------------------------------------------------------------------
# THE PLAN
# ---------------------------------------------------------------------------
# tree order IS z-order for an Overlay: earlier children render behind later ones.
# The layer order below is the whole point of the root layout — Game at the back,
# Modal in front — and it matches FBRUITags' declaration order in BRUITypes.h.

PLAN = {
    "WBP_RootLayout": {
        "folder": "/Game/UI",
        "parent_class": "/Script/Breachpoint.BRRootLayout",
        "header": "Source/Breachpoint/UI/BRRootLayout.h",
        "notes": "Four CommonActivatableWidgetStacks in an Overlay. Overlay child "
                 "order is z-order: Game behind, Modal in front.",
        "tree": [
            {"name": "RootOverlay", "class": OVERLAY, "parent": None},
            {"name": "GameLayerStack", "class": STACK, "parent": "RootOverlay",
             "slot": FILL, "bind": True},
            {"name": "GameMenuLayerStack", "class": STACK, "parent": "RootOverlay",
             "slot": FILL, "bind": True},
            {"name": "MenuLayerStack", "class": STACK, "parent": "RootOverlay",
             "slot": FILL, "bind": True},
            {"name": "ModalLayerStack", "class": STACK, "parent": "RootOverlay",
             "slot": FILL, "bind": True},
        ],
    },
}


# ---------------------------------------------------------------------------
# Reading the C++ contract
# ---------------------------------------------------------------------------

_BIND_RE = re.compile(
    r"UPROPERTY\([^)]*\bmeta\s*=\s*\([^)]*\bBindWidget(?P<opt>Optional)?\b[^)]*\)[^)]*\)"
    r"\s*(?:TObjectPtr<\s*(?P<ptr>\w+)\s*>|(?P<raw>\w+)\s*\*)\s*(?P<name>\w+)\s*;",
    re.S,
)


def required_bind_widgets(header_rel: str) -> dict[str, dict]:
    """{member name: {"class": "UCommonActivatableWidgetStack", "optional": bool}}

    Parsed from the header so the plan cannot drift from the code silently.
    """
    text = (REPO / header_rel).read_text()
    out = {}
    for m in _BIND_RE.finditer(text):
        out[m.group("name")] = {
            "class": m.group("ptr") or m.group("raw"),
            "optional": bool(m.group("opt")),
        }
    return out


def _leaf_class(class_path: str) -> str:
    """/Script/CommonUI.CommonActivatableWidgetStack -> UCommonActivatableWidgetStack"""
    return "U" + class_path.rsplit(".", 1)[-1]


# ---------------------------------------------------------------------------
# Validation — every failure here is one that would otherwise surface in PIE
# ---------------------------------------------------------------------------

def validate(asset: str, spec: dict) -> list[str]:
    errs: list[str] = []
    tree = spec["tree"]
    names = [n["name"] for n in tree]

    dupes = {n for n in names if names.count(n) > 1}
    if dupes:
        errs.append(f"{asset}: duplicate widget names {sorted(dupes)}")

    roots = [n for n in tree if n["parent"] is None]
    if len(roots) != 1:
        errs.append(f"{asset}: expected exactly 1 root widget, got {len(roots)}")

    seen: set[str] = set()
    for node in tree:
        p = node["parent"]
        if p is None:
            seen.add(node["name"])
            continue
        if p not in names:
            errs.append(f"{asset}: {node['name']} parents to unknown '{p}'")
        elif p not in seen:
            errs.append(f"{asset}: {node['name']} precedes its parent '{p}' — "
                        "tree order must be creation order")
        seen.add(node["name"])

    # THE CHECK THIS FILE EXISTS FOR
    required = required_bind_widgets(spec["header"])
    planned = {n["name"]: n for n in tree if n.get("bind")}

    for name, info in required.items():
        if name in planned:
            want, got = info["class"], _leaf_class(planned[name]["class"])
            if want != got:
                errs.append(f"{asset}: '{name}' is {want} in {spec['header']} "
                            f"but {got} in the plan")
        elif not info["optional"]:
            errs.append(f"{asset}: '{name}' is a NON-OPTIONAL BindWidget in "
                        f"{spec['header']} and the plan does not create it — "
                        "the WBP will fail to compile at asset load")

    for name in planned:
        if name not in required:
            errs.append(f"{asset}: plan marks '{name}' bind:True but "
                        f"{spec['header']} declares no such BindWidget")

    return errs


def validate_all() -> list[str]:
    return [e for asset, spec in PLAN.items() for e in validate(asset, spec)]


if __name__ == "__main__":
    import sys
    problems = validate_all()
    for p in problems:
        print("ERROR:", p)
    if not problems:
        for asset, spec in PLAN.items():
            req = required_bind_widgets(spec["header"])
            print(f"{asset}: {len(spec['tree'])} widgets, parent {spec['parent_class']}")
            print(f"  BindWidget contract from {spec['header']}: "
                  f"{', '.join(f'{k}:{v[chr(99)+chr(108)+chr(97)+chr(115)+chr(115)]}' for k, v in req.items())}")
        print("PLAN OK")
    sys.exit(1 if problems else 0)
