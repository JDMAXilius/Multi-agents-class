#!/usr/bin/env python3
"""Prove the plan's logic with no engine, no editor, no MCP.

    python3 mcp-ui/gen_ui/selftest_no_editor.py

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

print("\n6. BindWidget accepts a SUBCLASS (UMG does; the validator must too)")
check("UVerticalBox satisfies a declared UPanelWidget", P._satisfies("UVerticalBox", "UPanelWidget"))
check("UCanvasPanel satisfies a declared UPanelWidget", P._satisfies("UCanvasPanel", "UPanelWidget"))
check("exact match still satisfies", P._satisfies("UOverlay", "UOverlay"))
check("an UNRELATED class is still rejected", not P._satisfies("UImage", "UPanelWidget"))
check("an unknown class falls back to exact match", not P._satisfies("UMysteryWidget", "UWidget"))

print("\n7. typography — the letter-spacing conversion, and bad font specs are REJECTED")

# THE conversion check. Figma records PERCENT; UMG's LetterSpacing is 1/1000 em, so the
# only correct factor is 10 — and getting it wrong is invisible in every number that
# "matches" while the chrome quietly stops reading as military UI. Checked across ALL 16
# styles rather than one, because the failure that matters is a single row drifting.
bad_ls = [s["name"] for s in P.TOKENS["type"]["styles"] if s["ls_umg"] != s["ls_pct"] * 10]
check("every token style has ls_umg == ls_pct * 10", not bad_ls, f"drifted: {bad_ls}")

fp = P.font_properties("Label/Tab")["font"]
check("Label/Tab (15% in Figma) writes letterSpacing 150", fp["letterSpacing"] == 150, f"got {fp}")
check("letterSpacing is an int, not a float or a percent",
      isinstance(fp["letterSpacing"], int) and not isinstance(fp["letterSpacing"], bool))
check("the Figma weight string IS the TypefaceFontName", fp["typefaceFontName"] == "SemiBold")
check("the font object ref is the full /Game/X.X form",
      fp["fontObject"] == {"refPath": "/Game/UI/Fonts/F_Rajdhani.F_Rajdhani"}, f"got {fp}")

italic = P.font_properties("Body/Flavor")["font"]
check("'Medium Italic' is ONE FName with a space, not a family plus a flag",
      italic["typefaceFontName"] == "Medium Italic", f"got {italic}")

check("no font payload contains an array (the set_properties size+elements refusal)",
      not any(isinstance(v, list)
              for name in P.TYPE_STYLES
              for v in P.font_properties(name)["font"].values()))

check("every style the PLAN names resolves to a typeface its font asset really has",
      all(P.TYPE_STYLES[n["font"]]["weight"]
          in P.FONT_ASSETS[P.TYPE_STYLES[n["font"]]["family"]][1]
          for s in P.PLAN.values() for n in s["tree"] if n.get("font")))

# --- negative cases: a bad font spec must never reach an editor ---
TEXTNODE = {"name": "AllyScoreText", "class": P.TEXT, "parent": "BandCanvas",
            "slot": P.canvas_slot(90, 1, 34, 20), "bind": True, "font": "Display/Title"}

e = P.validate_art("TEST", dict(TEXTNODE, font="Display/Nonexistent"))
check("a style name that is not in figma_tokens.json is caught",
      any("not in figma_tokens.json" in x for x in e), f"got {e}")

e = P.validate_art("TEST", dict(TEXTNODE, font="Display/Item Title"))
check("a 48px style in a 20px slot is caught as unfittable",
      any("cannot fit" in x for x in e), f"got {e}")

e = P.validate_art("TEST", {"name": "CentreTick", "class": P.IMAGE, "parent": "C",
                            "font": "Display/Title"})
check("a font on a UImage is caught (it has no `font` property)",
      any("has no `font` property" in x for x in e), f"got {e}")

# A weight the composite font does not carry renders in the FALLBACK face and reports
# nothing, so it has to be caught here. Injected rather than hoped for: no real token row
# is wrong today, and a negative case that cannot fire is not a test.
P.TYPE_STYLES["TEST/Ghost Weight"] = {"name": "TEST/Ghost Weight", "family": "Rajdhani",
                                      "weight": "Ultralight", "size": 12,
                                      "ls_pct": 0, "ls_umg": 0, "case": "ORIGINAL"}
P.TYPE_STYLES["TEST/Ghost Family"] = {"name": "TEST/Ghost Family", "family": "Comic Sans",
                                      "weight": "Bold", "size": 12,
                                      "ls_pct": 0, "ls_umg": 0, "case": "ORIGINAL"}
e = P.validate_art("TEST", dict(TEXTNODE, font="TEST/Ghost Weight"))
check("a weight the composite font does not contain is caught",
      any("TypefaceFontName must match" in x for x in e), f"got {e}")
e = P.validate_art("TEST", dict(TEXTNODE, font="TEST/Ghost Family"))
check("a family with no imported font asset is caught",
      any("no font asset in UE" in x for x in e), f"got {e}")
del P.TYPE_STYLES["TEST/Ghost Weight"], P.TYPE_STYLES["TEST/Ghost Family"]

check("the real plan is still valid after those mutations", not P.validate_all())

print("\n8. brushes — a missing texture SKIPS, it never fails the asset")

real = {"name": "ReticleImage", "class": P.IMAGE, "parent": "O",
        "brush": P.brush("/Game/UI/HUD/HUD_Reticle_AR", 43, 43)}
check("a texture that exists is not a problem", P.texture_problem(real["brush"]) is None,
      f"got {P.texture_problem(real['brush'])}")
props, notes = P.art_properties(real)
check("...and it produces a brush payload", "brush" in props and not notes, f"got {props}")
check("the brush resource ref is the full /Game/X.X form",
      props["brush"]["resourceObject"]
      == {"refPath": "/Game/UI/HUD/HUD_Reticle_AR.HUD_Reticle_AR"}, f"got {props}")
check("imageSize is the AUTHORED 1280x720 size, never multiplied by 1.5",
      props["brush"]["imageSize"] == {"x": 43.0, "y": 43.0}, f"got {props}")

ghost = {"name": "HitMarkerImage", "class": P.IMAGE, "parent": "O",
         "brush": P.brush("/Game/UI/HUD/HUD_Does_Not_Exist", 40, 42)}
check("a missing texture is reported", P.texture_problem(ghost["brush"]) is not None)
check("a missing texture is NOT a plan error", not P.validate_art("TEST", ghost),
      f"got {P.validate_art('TEST', ghost)}")
props, notes = P.art_properties(ghost)
check("a missing texture yields NO brush write and one loud note",
      props == {} and len(notes) == 1, f"got {props}, {notes}")

e = P.validate_art("TEST", dict(TEXTNODE, brush=P.brush("/Game/UI/HUD/HUD_Reticle_AR", 8, 8)))
check("a brush on a CommonTextBlock is caught",
      any("has no `brush` property" in x for x in e), f"got {e}")

check("every brush the PLAN asks for today resolves", not P.skipped_brushes(),
      f"skipped: {P.skipped_brushes()}")

print()
if FAILS:
    print(f"SELF-TEST FAILED: {len(FAILS)} check(s) — {FAILS}")
    sys.exit(1)
print("SELF-TEST PASSED — plan logic is sound with no engine present.")
