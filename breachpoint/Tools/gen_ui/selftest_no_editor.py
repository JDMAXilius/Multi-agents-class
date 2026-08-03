#!/usr/bin/env python3
"""Prove the plan's logic with no engine, no editor, no MCP.

    python3 Tools/gen_ui/selftest_no_editor.py

The point is the BindWidget cross-check: it must FAIL loudly when the plan and the C++
header disagree. A validator that only ever passes is not a validator, so every negative
case below is constructed by mutating a real spec and asserting the specific error fires.
"""
from __future__ import annotations

import copy, sys, tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import wbp_plan as P

FAILS: list[str] = []


def check(name, cond, detail=""):
    print(f"  {'ok  ' if cond else 'FAIL'}  {name}" + (f" — {detail}" if detail and not cond else ""))
    if not cond:
        FAILS.append(name)


def errors_for(spec, header_text=None):
    """Validate a spec, optionally against a synthetic header."""
    spec = copy.deepcopy(spec)
    if header_text is not None:
        tmp = Path(tempfile.mkdtemp()) / "Fake.h"
        tmp.write_text(header_text)
        spec["header"] = str(tmp.relative_to(P.REPO)) if str(tmp).startswith(str(P.REPO)) else None
        if spec["header"] is None:            # tmp is outside the repo — patch the reader
            orig = P.REPO
            P.REPO = tmp.parent
            spec["header"] = "Fake.h"
            try:
                return P.validate("TEST", spec)
            finally:
                P.REPO = orig
    return P.validate("TEST", spec)


REAL_HEADER = """
UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
TObjectPtr<UCommonActivatableWidgetStack> GameLayerStack;
UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
TObjectPtr<UCommonActivatableWidgetStack> GameMenuLayerStack;
UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
TObjectPtr<UCommonActivatableWidgetStack> MenuLayerStack;
UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess))
TObjectPtr<UCommonActivatableWidgetStack> ModalLayerStack;
"""

print("1. header parsing")
hdr = P.required_bind_widgets("Source/Breachpoint/UI/BRRootLayout.h")
check("finds 4 BindWidget members in BRRootLayout.h", len(hdr) == 4, f"got {sorted(hdr)}")
check("all four are UCommonActivatableWidgetStack",
      all(v["class"] == "UCommonActivatableWidgetStack" for v in hdr.values()))
check("none is marked optional", not any(v["optional"] for v in hdr.values()))
check("does not pick up the non-BindWidget LayerStacks map", "LayerStacks" not in hdr)

print("\n2. BindWidgetOptional is distinguished from BindWidget")
opt = errors_for(P.PLAN["WBP_RootLayout"], REAL_HEADER + """
UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UPanelWidget> KillfeedContainer;
""")
check("an unbuilt OPTIONAL bind is not an error", not opt, f"got {opt}")

print("\n3. the real plan is valid")
check("validate_all() is clean", not P.validate_all(), f"got {P.validate_all()}")

print("\n4. negative cases — the validator must FAIL these")

missing = copy.deepcopy(P.PLAN["WBP_RootLayout"])
missing["tree"] = [n for n in missing["tree"] if n["name"] != "ModalLayerStack"]
e = P.validate("TEST", missing)
check("a missing NON-optional BindWidget is caught",
      any("ModalLayerStack" in x and "NON-OPTIONAL" in x for x in e), f"got {e}")

wrongtype = copy.deepcopy(P.PLAN["WBP_RootLayout"])
for n in wrongtype["tree"]:
    if n["name"] == "MenuLayerStack":
        n["class"] = "/Script/UMG.Overlay"
e = P.validate("TEST", wrongtype)
check("a class mismatch against the header is caught",
      any("MenuLayerStack" in x and "UOverlay" in x for x in e), f"got {e}")

ghost = copy.deepcopy(P.PLAN["WBP_RootLayout"])
ghost["tree"].append({"name": "GhostStack", "class": P.STACK,
                      "parent": "RootOverlay", "bind": True})
e = P.validate("TEST", ghost)
check("bind:True with no matching header member is caught",
      any("GhostStack" in x and "declares no such BindWidget" in x for x in e), f"got {e}")

dupe = copy.deepcopy(P.PLAN["WBP_RootLayout"])
dupe["tree"].append(dict(dupe["tree"][1]))
e = P.validate("TEST", dupe)
check("duplicate widget names are caught", any("duplicate" in x for x in e), f"got {e}")

orphan = copy.deepcopy(P.PLAN["WBP_RootLayout"])
orphan["tree"][2]["parent"] = "NoSuchWidget"
e = P.validate("TEST", orphan)
check("an unknown parent is caught", any("unknown" in x for x in e), f"got {e}")

order = copy.deepcopy(P.PLAN["WBP_RootLayout"])
order["tree"] = [order["tree"][1]] + [order["tree"][0]] + order["tree"][2:]
e = P.validate("TEST", order)
check("a child declared before its parent is caught",
      any("precedes its parent" in x for x in e), f"got {e}")

tworoots = copy.deepcopy(P.PLAN["WBP_RootLayout"])
tworoots["tree"].append({"name": "SecondRoot", "class": P.OVERLAY, "parent": None})
e = P.validate("TEST", tworoots)
check("two root widgets are caught", any("exactly 1 root" in x for x in e), f"got {e}")

print("\n5. z-order intent is expressed by tree order")
names = [n["name"] for n in P.PLAN["WBP_RootLayout"]["tree"] if n.get("bind")]
check("layer order is Game -> GameMenu -> Menu -> Modal (back to front)",
      names == ["GameLayerStack", "GameMenuLayerStack", "MenuLayerStack", "ModalLayerStack"],
      f"got {names}")

print()
if FAILS:
    print(f"SELF-TEST FAILED: {len(FAILS)} check(s) — {FAILS}")
    sys.exit(1)
print("SELF-TEST PASSED — plan logic is sound with no engine present.")
