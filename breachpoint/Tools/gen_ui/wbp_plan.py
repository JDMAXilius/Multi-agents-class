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
VBOX = "/Script/UMG.VerticalBox"
HBOX = "/Script/UMG.HorizontalBox"
TEXT = "/Script/CommonUI.CommonTextBlock"
IMAGE = "/Script/UMG.Image"
SIZEBOX = "/Script/UMG.SizeBox"

FILL = {"horizontalAlignment": "HAlign_Fill", "verticalAlignment": "VAlign_Fill",
        "padding": {"left": 0.0, "top": 0.0, "right": 0.0, "bottom": 0.0}}


def canvas_slot(left, top, width, height, anchor=(0.0, 0.0), align=(0.0, 0.0)):
    """A CanvasPanelSlot pinned to a point anchor.

    With min == max the anchor is a POINT, and UE then reads Offsets.Right/Bottom as the
    widget's SIZE, not as margins. That asymmetry is the classic source of a widget that
    looks right at 1280 and drifts at every other aspect ratio, so it is written once here
    rather than in every call site.
    """
    return {"layoutData": {
        "offsets": {"left": float(left), "top": float(top),
                    "right": float(width), "bottom": float(height)},
        "anchors": {"minimum": {"x": float(anchor[0]), "y": float(anchor[1])},
                    "maximum": {"x": float(anchor[0]), "y": float(anchor[1])}},
        "alignment": {"x": float(align[0]), "y": float(align[1])}}}


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

    # ------------------------------------------------------------------
    # WBP_HUDLayout — deliberately SMALL, and that is the honest state.
    #
    # UBRHUDLayout declares exactly ONE BindWidget: `KillfeedContainer`
    # (BindWidgetOptional, UPanelWidget — BRHUDLayout.h:60). The other five HUD
    # surfaces in SCREEN-MANIFEST W1 (vitals, ammo, grenades+grapple, match band,
    # reticle) have NO C++ members to bind to and NO component classes yet.
    #
    # Authoring named containers for them now would be dead layout that nothing can
    # reach — the exact thing `ui-presentation` §8.3 forbids: "if a field does not
    # exist, that is a C++ gap — file it, do not work around it in the widget."
    # They arrive with their component classes in P2.
    # ------------------------------------------------------------------
    "WBP_HUDLayout": {
        "folder": "/Game/UI",
        "parent_class": "/Script/Breachpoint.BRHUDLayout",
        "header": "Source/Breachpoint/UI/BRHUDLayout.h",
        "notes": "Root canvas + the killfeed container. Five of W1's six surfaces are "
                 "blocked on their component classes and are NOT stubbed here.",
        "tree": [
            {"name": "RootCanvas", "class": CANVAS, "parent": None},
            # Top-right, right margin 69, width 349 — column 3's rule from
            # SCREEN-MANIFEST §7.3: "Column 3 content anchors RIGHT, not left",
            # which is what makes ultrawide correct with zero extra work.
            {"name": "KillfeedContainer", "class": VBOX, "parent": "RootCanvas",
             "slot": canvas_slot(left=-69, top=45, width=349, height=300,
                                 anchor=(1.0, 0.0), align=(1.0, 0.0)),
             "bind": True},
        ],
    },

    # ------------------------------------------------------------------
    # WBP_KillfeedEntry — layout only, and NOTHING IN IT IS BINDABLE YET.
    #
    # UBRKillfeedEntryWidget declares ZERO BindWidget members. Its only update path
    # is BP_OnEntrySet, a BlueprintImplementableEvent — and implementing that needs
    # an event graph NODE, which R18/R26 forbid in a WBP. That is decision D1
    # (ROADMAP §1), still open, and it blocks this asset from being finished.
    #
    # The row geometry below is real design work and is worth landing. The widget
    # NAMES are chosen as the contract the C++ packet should adopt: adding
    # BindWidget members with these exact names makes this WBP correct with no
    # re-authoring, and `validate()` will then enforce the match automatically.
    # Filed as a contract_gap; do not paper over it with a property binding, which
    # is a per-frame poll wearing a different hat (law 4).
    # ------------------------------------------------------------------
    "WBP_KillfeedEntry": {
        "folder": "/Game/UI",
        "parent_class": "/Script/Breachpoint.BRKillfeedEntryWidget",
        "header": "Source/Breachpoint/UI/BRHUDLayout.h",
        "notes": "One killfeed row, 349x30. Names are a proposed BindWidget contract; "
                 "the C++ parent declares none yet (D1 open).",
        "tree": [
            {"name": "RootSizeBox", "class": SIZEBOX, "parent": None},
            {"name": "Row", "class": HBOX, "parent": "RootSizeBox"},
            {"name": "KillerNameText", "class": TEXT, "parent": "Row"},
            {"name": "WeaponIcon", "class": IMAGE, "parent": "Row"},
            {"name": "VictimNameText", "class": TEXT, "parent": "Row"},
            # The Spotter line reserves its slot and renders EMPTY when the string is
            # empty — it never collapses layout and never waits on the LLM.
            # Offline ⇒ identical HUD minus flavour (ue5-ui-architecture §5).
            {"name": "SpotterLineText", "class": TEXT, "parent": "Row"},
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


# UMG accepts a SUBCLASS for a BindWidget member — `TObjectPtr<UPanelWidget> Foo` is
# satisfied by a UVerticalBox. This validator has no engine and therefore no reflection,
# so the base chain of every class the plan uses is declared here as data. Adding a widget
# class to a plan means adding its chain here; an unknown class falls back to exact match,
# which fails loudly rather than passing something wrong.
_BASES = {
    "UOverlay":        ["UPanelWidget", "UWidget"],
    "UCanvasPanel":    ["UPanelWidget", "UWidget"],
    "UVerticalBox":    ["UPanelWidget", "UWidget"],
    "UHorizontalBox":  ["UPanelWidget", "UWidget"],
    "USizeBox":        ["UContentWidget", "UPanelWidget", "UWidget"],
    "UImage":          ["UWidget"],
    "UCommonTextBlock": ["UTextBlock", "UTextLayoutWidget", "UWidget"],
    "UCommonActivatableWidgetStack":
        ["UCommonActivatableWidgetContainerBase", "UWidget"],
}


def _satisfies(planned: str, declared: str) -> bool:
    return planned == declared or declared in _BASES.get(planned, [])


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
            if not _satisfies(got, want):
                errs.append(f"{asset}: '{name}' is {want} in {spec['header']} "
                            f"but {got} in the plan, and {got} is not a {want}")
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
