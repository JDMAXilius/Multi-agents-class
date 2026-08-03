#!/usr/bin/env python3
"""Prove the R26 gate's logic with no engine, no editor, no MCP.

    python3 Tools/audit_ui/selftest_no_editor.py
    python3 Tools/audit_ui/audit_wbp.py --selftest

The editor is replaced by FAKE observations — plain dicts shaped exactly like what
`GetWidgets` / `list_variables` / `list_graphs` / `read_graph_dsl` really returned on
2026-08-02 (captured live, so the fakes are not wishful thinking).

**Every negative case below must actually fire.** A validator that only ever passes is not
a validator, and this file exists because `audit_r26.py` — the sibling gate — was written,
never run, and never wired in. Each check here names the specific finding it demands, so a
check cannot go green because *some* finding happened to appear.
"""
from __future__ import annotations

import copy
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import wbp_expect as E  # noqa: E402

FAILS: list[str] = []


def check(name, cond, detail=""):
    print(f"  {'ok  ' if cond else 'FAIL'}  {name}" + (f" — {detail}" if detail and not cond else ""))
    if not cond:
        FAILS.append(name)


def sev(found, severity, needle):
    """Did a finding of exactly this severity mentioning this text fire?"""
    return any(s == severity and needle in m for s, m in found)


# --------------------------------------------------------------------------- fakes
# A tiny stand-in for the engine's reflection. The real one asks the editor.
FAKE_BASES = {
    "UVerticalBox": {"UPanelWidget", "UWidget"},
    "UCanvasPanel": {"UPanelWidget", "UWidget"},
    "UImage": {"UWidget"},
    "UCommonActivatableWidgetStack": {"UCommonActivatableWidgetContainerBase", "UWidget"},
}


def fake_compatible(declared_leaf, actual_refpath):
    actual = "U" + (actual_refpath or "").rsplit(".", 1)[-1]
    if declared_leaf == "UMysteryWidget":
        return None                       # the "could not resolve" path
    return declared_leaf == actual or declared_leaf in FAKE_BASES.get(actual, set())


EMPTY_DSL = ("(event UserInterface|EventPreConstruct (IsDesignTime))\n\n"
             "(event UserInterface|EventConstruct)\n\n"
             "(event UserInterface|EventTick (MyGeometry InDeltaTime))\n")

ROOT_EXP = {
    "asset": "WBP_RootLayout",
    "game_path": "/Game/UI/WBP_RootLayout.WBP_RootLayout",
    "cpp_class": "UBRRootLayout",
    "expected_parent": "/Script/Breachpoint.BRRootLayout",
    "header": "Source/Breachpoint/UI/BRRootLayout.h",
    "binds": {n: {"class": "UCommonActivatableWidgetStack", "optional": False}
              for n in ("GameLayerStack", "GameMenuLayerStack", "MenuLayerStack",
                        "ModalLayerStack")},
}

ROOT_OBS = {
    "parent": "/Script/Breachpoint.BRRootLayout",
    "variables": [],
    "graphs": {"EventGraph": EMPTY_DSL},
    "widgets": [{"name": "RootOverlay", "class": "/Script/UMG.Overlay",
                 "isVariable": False, "inherited": False}] +
               [{"name": n, "class": "/Script/CommonUI.CommonActivatableWidgetStack",
                 "isVariable": True, "inherited": True}
                for n in ("GameLayerStack", "GameMenuLayerStack", "MenuLayerStack",
                          "ModalLayerStack")],
}


def judged(exp_mut=None, obs_mut=None):
    exp, obs = copy.deepcopy(ROOT_EXP), copy.deepcopy(ROOT_OBS)
    if exp_mut:
        exp_mut(exp)
    if obs_mut:
        obs_mut(obs)
    return E.judge(exp, obs, fake_compatible)


# --------------------------------------------------------------------------- 1. header
print("1. header parsing — the expectation is DERIVED, not typed in")
HDR = """
class BREACHPOINT_API UBRKillfeedEntryWidget : public UCommonUserWidget
{
    GENERATED_BODY()
};

class BREACHPOINT_API UBRHUDLayout : public UBRActivatableWidget
{
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Breachpoint|HUD")
    TObjectPtr<UPanelWidget> KillfeedContainer;

    UPROPERTY(meta = (BindWidget))
    UImage* Reticle;
};
"""
hud = E.binds_from_text(HDR, "UBRHUDLayout")
check("finds both binds on UBRHUDLayout", set(hud) == {"KillfeedContainer", "Reticle"}, hud)
check("BindWidgetOptional is distinguished", hud["KillfeedContainer"]["optional"] is True)
check("plain BindWidget is not optional", hud["Reticle"]["optional"] is False)
check("raw-pointer form is parsed too", hud["Reticle"]["class"] == "UImage")

# The bug this file caught on its first run: BRHUDLayout.h declares TWO classes, and
# parsing the whole file handed the killfeed row a bind that belongs to the HUD.
kfe = E.binds_from_text(HDR, "UBRKillfeedEntryWidget")
check("a sibling class in the SAME header does NOT inherit its neighbour's binds",
      kfe == {}, kfe)
check("class_body slices the right class",
      "KillfeedContainer" in E.class_body(HDR, "UBRHUDLayout")
      and "KillfeedContainer" not in E.class_body(HDR, "UBRKillfeedEntryWidget"))
check("an unknown class yields an empty body, not the whole file",
      E.class_body(HDR, "UBRNoSuchThing") == "")

print("\n1b. against the REAL headers on disk")
real = E.discover()
check("discovery finds the three committed WBPs",
      {a["asset"] for a in real} >= {"WBP_RootLayout", "WBP_HUDLayout",
                                     "WBP_KillfeedEntryWidget"},
      [a["asset"] for a in real])
byname = {a["asset"]: a for a in real}
check("WBP_RootLayout needs 4 non-optional binds",
      len(byname["WBP_RootLayout"]["binds"]) == 4
      and not any(v["optional"] for v in byname["WBP_RootLayout"]["binds"].values()))
check("WBP_HUDLayout needs exactly one, and it is OPTIONAL",
      list(byname["WBP_HUDLayout"]["binds"]) == ["KillfeedContainer"]
      and byname["WBP_HUDLayout"]["binds"]["KillfeedContainer"]["optional"])
check("WBP_KillfeedEntryWidget needs none", byname["WBP_KillfeedEntryWidget"]["binds"] == {})
check("game paths are the FULL /Game/X/Y.Y form the server requires",
      all(a["game_path"].startswith("/Game/") and a["game_path"].count(".") == 1
          and a["game_path"].endswith("." + a["asset"]) for a in real))

# Soft drift check against the generator's twin regex — no hard dependency (see wbp_expect).
try:
    sys.path.insert(0, str(E.REPO / "Tools" / "gen_ui"))
    import wbp_plan as G  # noqa: E402
    same = all(E.binds_from_text((E.REPO / a["header"]).read_text(), a["cpp_class"])
               == {k: v for k, v in G.required_bind_widgets(a["header"]).items()
                   if k in E.binds_from_text((E.REPO / a["header"]).read_text(), a["cpp_class"])}
               for a in real if a["header"])
    check("agrees with Tools/gen_ui/wbp_plan.py's twin regex", same)
except Exception as e:
    print(f"  skip  gen_ui twin-regex comparison — {type(e).__name__}: {e}")

# --------------------------------------------------------------------------- 2. DSL
print("\n2. graph node counting")
check("the three engine default stubs count as ZERO",
      E.graph_nodes(EMPTY_DSL)["nodes"] == 0, E.graph_nodes(EMPTY_DSL))
check("an empty graph is zero", E.graph_nodes("")["nodes"] == 0)

one_line = EMPTY_DSL.replace("(event UserInterface|EventConstruct)",
                             '(event UserInterface|EventConstruct\n'
                             '  (Development|PrintString "hi"))')
check("ONE statement inside a default stub is caught",
      E.graph_nodes(one_line)["nodes"] > 0, E.graph_nodes(one_line))

custom = EMPTY_DSL + "\n(event MyCustomEvent)\n"
check("a custom event with an EMPTY body still counts (added member)",
      E.graph_nodes(custom)["nodes"] >= 1, E.graph_nodes(custom))

func = EMPTY_DSL + "\n(fn DoubleIt (Value)\n  (return (* Value 2)))\n"
check("a function graph counts", E.graph_nodes(func)["nodes"] > 0, E.graph_nodes(func))

branch = ('(event UserInterface|EventTick (MyGeometry InDeltaTime)\n'
          '  (if (> InDeltaTime 0.0)\n'
          '    (Development|PrintString "slow")))\n')
check("a branch inside Tick is caught", E.graph_nodes(branch)["nodes"] >= 3,
      E.graph_nodes(branch))
check("a parameter list is NOT mistaken for a statement",
      E.graph_nodes("(event UserInterface|EventTick (MyGeometry InDeltaTime))")["nodes"] == 0)
check("`(return)` IS treated as a statement, not a parameter",
      E.graph_nodes("(event UserInterface|EventConstruct (return))")["nodes"] > 0)
check("unbalanced DSL reports UNKNOWN (-1), never silently zero",
      E.graph_nodes("(event Foo")["nodes"] == -1)

# --------------------------------------------------------------------------- 3. positive
print("\n3. the real, conforming shape passes")
clean = judged()
check("a conforming WBP produces no findings", clean == [], clean)

hud_exp = {"asset": "WBP_HUDLayout", "game_path": "/Game/UI/WBP_HUDLayout.WBP_HUDLayout",
           "cpp_class": "UBRHUDLayout", "expected_parent": "/Script/Breachpoint.BRHUDLayout",
           "header": "Source/Breachpoint/UI/BRHUDLayout.h",
           "binds": {"KillfeedContainer": {"class": "UPanelWidget", "optional": True}}}
hud_obs = {"parent": "/Script/Breachpoint.BRHUDLayout", "variables": [],
           "graphs": {"EventGraph": EMPTY_DSL},
           "widgets": [{"name": "RootCanvas", "class": "/Script/UMG.CanvasPanel",
                        "isVariable": False, "inherited": False},
                       {"name": "KillfeedContainer", "class": "/Script/UMG.VerticalBox",
                        "isVariable": True, "inherited": True}]}
check("a SUBCLASS satisfies a declared base (UVerticalBox for UPanelWidget)",
      E.judge(hud_exp, hud_obs, fake_compatible) == [],
      E.judge(hud_exp, hud_obs, fake_compatible))

gone = copy.deepcopy(hud_obs)
gone["widgets"] = gone["widgets"][:1]
check("an unbuilt OPTIONAL bind is info, never high",
      sev(E.judge(hud_exp, gone, fake_compatible), "info", "KillfeedContainer")
      and not [f for f in E.judge(hud_exp, gone, fake_compatible) if f[0] == "high"])

# --------------------------------------------------------------------------- 4. negatives
print("\n4. negative cases — each MUST fire, or the gate is decorative")

f = judged(obs_mut=lambda o: o["widgets"].pop())          # drop ModalLayerStack
check("R26/bind: a MISSING non-optional BindWidget is high",
      sev(f, "high", "ModalLayerStack"), f)

f = judged(obs_mut=lambda o: o.__setitem__("parent", "/Script/UMG.UserWidget"))
check("R26 cond. 1: a WRONG PARENT is high", sev(f, "high", "parent is"), f)

f = judged(obs_mut=lambda o: o.__setitem__("variables", ["bIsHovered", "CachedScore"]))
check("R26 cond. 3: a NON-EMPTY variable list is high",
      sev(f, "high", "bIsHovered") and sev(f, "high", "CachedScore"), f)

f = judged(obs_mut=lambda o: o["graphs"].__setitem__("EventGraph", one_line))
check("R26 cond. 2: a NON-ZERO node count is high", sev(f, "high", "requires ZERO"), f)

f = judged(obs_mut=lambda o: o["graphs"].__setitem__("GetHealthPercent", EMPTY_DSL))
check("R26 cond. 3: an extra FUNCTION GRAPH is high",
      sev(f, "high", "GetHealthPercent"), f)


def _wrong_class(o):
    o["widgets"][1]["class"] = "/Script/UMG.Image"


f = judged(obs_mut=_wrong_class)
check("bind: an INCOMPATIBLE widget class is high", sev(f, "high", "not a subclass"), f)


def _not_inherited(o):
    o["widgets"][1]["inherited"] = False


f = judged(obs_mut=_not_inherited)
check("bind: present but NOT INHERITED is flagged (desync)",
      sev(f, "medium", "not wired"), f)

f = judged(obs_mut=lambda o: o["widgets"].append(
    {"name": "StrayIcon", "class": "/Script/UMG.Image", "isVariable": True, "inherited": False}))
check("R26 cond. 3: an Is Variable widget with no BindWidget is flagged",
      sev(f, "medium", "StrayIcon"), f)

f = judged(exp_mut=lambda e: e.__setitem__("header", None))
check("an asset with NO C++ class of the right name is high",
      sev(f, "high", "no header declares"), f)


def _mystery(e):
    e["binds"]["GameLayerStack"]["class"] = "UMysteryWidget"


f = judged(exp_mut=_mystery)
check("an UNRESOLVABLE class is reported UNVERIFIED, not passed",
      sev(f, "medium", "UNVERIFIED"), f)

f = judged(obs_mut=lambda o: o["graphs"].__setitem__("EventGraph", "(event Foo"))
check("a graph that will not parse is reported, not passed as clean",
      sev(f, "medium", "did not parse"), f)

print()
if FAILS:
    print(f"SELF-TEST FAILED: {len(FAILS)} check(s) — {FAILS}")
    sys.exit(1)
print("SELF-TEST PASSED — the R26 gate fires on every violation it claims to catch, "
      "with no engine present.")
