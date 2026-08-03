#!/usr/bin/env python3
"""The committed plan half of the UI-material generator. Pure CPython — imports no engine.

    python3 Tools/gen_ui/material_plan.py        # self-checks, prints the graph, exits non-zero on error

Sibling of `wbp_plan.py`, and the same split for the same reason: this file is the
reviewable R37.1 artifact — a material's node graph expressed as diffable text — and
`build_materials.py` merely executes it. A material is a BINARY. Everything a reviewer
needs to judge it therefore has to exist as text before the editor is opened, because
afterwards the only readable thing about it is this file.

THE LOAD-BEARING CHECK, mirroring `wbp_plan`'s BindWidget parse: a material parameter that
C++ drives by name is DECLARED HERE WITH THE C++ SYMBOL THAT NAMES IT, and `validate()`
reads the real .cpp and compares. A material parameter rename is otherwise UE's silent
failure — `SetScalarParameterValue` on a name the material does not have is a no-op, no
warning, no log line, and the ring simply never sweeps.


TIER 4 — WHY EITHER OF THESE IS ALLOWED TO BE AN ASSET AT ALL
=============================================================
`BREACHPOINT-AUTHORING-MATRIX.md` Tier 4: an asset exists ONLY where UE has no C++ path.
The question to answer per material is not "would a material be nice" but "what is the C++
expression of this, and why is there none". Answered once, per material:

M_UI_RadialSweep — BUILT.
    The shape is an ANGULAR mask. UMG's only fill primitive is `UProgressBar`, whose fill
    types are all LINEAR (LeftToRight / RightToLeft / TopToBottom / BottomToTop / FillFrom-
    Center) — none of them sweeps an angle, and `UBRProgressBar` wraps a `UProgressBar` so
    it inherits exactly that limit. `EWidgetClipping::ClipToBounds`, the other C++ way to
    reveal part of a widget, clips to a RECTANGLE and cannot express a wedge. There is no
    arc/pie Slate leaf exposed to UMG. So the angular mask has no C++ expression, and this
    is the narrow Tier-4 case. C++ pushes ONE scalar and draws nothing.
    It also already has a consumer: `UBRScreen_DeathRespawn` sets `Sweep` today (see
    CPP_CONSTANTS below), against a material that does not exist. This closes that.

M_UI_ArcFill — **NOT BUILT. DELIBERATELY. SEE `NOT_BUILT` BELOW.**
    Short version: the reveal-along-a-curve it would do IS expressible in C++ (a
    `ClipToBounds` box at a percent-driven width over a fixed-size `UImage` — the arc's
    2.7px sag is a vertical offset, and a horizontal reveal never needs to know about it),
    AND nothing in Source/ sets a `Percent` parameter on anything, so the material would
    ship with zero consumers. It fails the Tier-4 test twice. Long version below.
"""
from __future__ import annotations

import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

# Feature-first, matching wbp_plan.ASSET_FOLDER's reasoning: a flat /Game/UI stops working
# at thirty assets, and this generator deletes-then-creates so the folder is free NOW and
# nearly impossible later (BP18: MCP cannot rename an asset).
UI_MATERIALS = "/Game/UI/Materials"

TAU = 6.283185307179586

# ---------------------------------------------------------------------------
# Node type names. UE `UMaterialExpression*` class leaf names, spelled once.
# A typo here is a silent "node not created" — same failure class as wbp_plan's class paths.
# ---------------------------------------------------------------------------
TEXCOORD   = "MaterialExpressionTextureCoordinate"
CONST      = "MaterialExpressionConstant"
CONST2     = "MaterialExpressionConstant2Vector"
CONST3     = "MaterialExpressionConstant3Vector"       # BANNED in a colour chain — see below
CONST4     = "MaterialExpressionConstant4Vector"       # BANNED in a colour chain — see below
SUB        = "MaterialExpressionSubtract"
ADD        = "MaterialExpressionAdd"
MUL        = "MaterialExpressionMultiply"
DIV        = "MaterialExpressionDivide"
DIST       = "MaterialExpressionDistance"
ATAN2      = "MaterialExpressionArctangent2"
FRAC       = "MaterialExpressionFrac"
CLAMP      = "MaterialExpressionClamp"
MASK       = "MaterialExpressionComponentMask"
SCALARP    = "MaterialExpressionScalarParameter"
VECTORP    = "MaterialExpressionVectorParameter"

# Any of these reaching EmissiveColor is a baked colour, which is the same finding as a hex
# literal in a WBP and worse, because a binary cannot be diffed. Colour is a VectorParameter.
BAKED_COLOUR_TYPES = {CONST3, CONST4}

# Material outputs this plan is allowed to drive. Translucent UI wants exactly these two:
# UI domain is unlit, so EmissiveColor IS the colour, and Opacity is the mask.
EMISSIVE, OPACITY = "EmissiveColor", "Opacity"


def n(type_, **kw) -> dict:
    """One node. `inputs` names other nodes; every other key is a literal property."""
    inputs = kw.pop("inputs", {})
    return {"type": type_, "inputs": inputs, "props": kw}


# ---------------------------------------------------------------------------
# THE PLAN
# ---------------------------------------------------------------------------
# Graph order IS creation order and every `inputs` reference must name an EARLIER node —
# validate() enforces it, for the same reason wbp_plan enforces host-before-hosted: the
# executor walks this dict in insertion order and a forward reference is a KeyError at
# call 14 with a half-built graph, whose error message names the wrong cause.

PLAN = {
    # =======================================================================
    # M_UI_RadialSweep — the respawn ring's sweep.
    #
    # `figma_screen_layout.json` death_respawn.respawn_ring: track and sweep are two
    # separate 104x104 circles at (588,410), and the JSON says so itself — "the sweep is a
    # radial fill, which UMG cannot do with a ProgressBar. Tier-4 material."
    #
    # THE TRACK IS NOT THIS MATERIAL. It is a static circle, an unbound WBP image, and
    # `BRScreen_DeathRespawn.h` says exactly that at `RespawnRingSweep`. One material,
    # one job.
    #
    # GEOMETRY, IN UV SPACE. The brush quad's UV is 0..1 with **y DOWN**. Every sign below
    # follows from that one fact and nothing else:
    #     P     = UV - (0.5, 0.5)          centre-relative, y still down
    #     R     = |P|                       0.5 at the box edge, 0.707 in the corners
    #     Ang   = atan2(P.x, -P.y)          0 at 12 o'clock, +pi/2 at 3 o'clock
    #     T     = frac(Ang / 2pi)           0 -> 1 CLOCKWISE from 12  (HLSL frac is
    #                                       x - floor(x), so frac(-0.25) = 0.75 — 9 o'clock)
    # Checked at the four cardinals: 12->0.0, 3->0.25, 6->0.5, 9->0.75. Clockwise from 12,
    # which is what `BRScreen_DeathRespawn.h` specifies ("wound clockwise from 12").
    #
    # THE SWEEP TEST, both ends, because both are stated behaviour:
    #   Sweep = 0 -> SweepAdj = 0, and T >= 0 everywhere, so (SweepAdj - T) <= 0 and the
    #                mask is 0 at every pixel. NOTHING DRAWN. Not "a thin line at 12".
    #   Sweep = 1 -> SweepAdj = 1 + EdgeSoftness. Without that widening the antialiased
    #                edge would feather to zero just counter-clockwise of 12 and leave a
    #                HAIRLINE GAP in a circle that is supposed to be closed. The
    #                (1 + EdgeSoftness) factor is that gap and only that gap.
    #
    # WHY A FIXED `EdgeSoftness` AND NOT SCREEN DERIVATIVES. DDX/DDY of T would give a
    # perfectly scaled edge everywhere EXCEPT across the 12 o'clock seam, where T jumps 1->0
    # and the derivative explodes: a bright radial artifact line up the middle of the ring,
    # on every frame, at every sweep value. A constant in T-units has no seam. It is a
    # PARAMETER rather than a constant because the correct value depends on the ring's
    # rendered pixel size, which changes with the DPI curve — 0.004 of a turn is ~1.4 deg,
    # about 1.3 px at the mock's r=52.
    # =======================================================================
    "M_UI_RadialSweep": {
        "folder": UI_MATERIALS,
        "domain": "MD_UI",
        "blend_mode": "BLEND_Translucent",
        "shading_model": "MSM_Unlit",
        "notes": "Angular ring mask, clockwise from 12. Sweep 0 = nothing, 1 = closed circle.",

        # param -> (C++ file, C++ symbol). validate() parses the .cpp and compares the
        # STRING LITERAL. This is the check this file exists for.
        "cpp_constants": {
            "Sweep": ("Source/Breachpoint/UI/Screens/BRScreen_DeathRespawn.cpp",
                      "UBRScreen_DeathRespawn::RespawnRingSweepParameterName"),
        },

        "params": {
            # Driven by C++ every countdown tick. Default 0 = the honest empty state: a
            # ring that has not been told a value draws nothing, exactly like
            # `UBRProgressBar::SetPercentUnknown`.
            "Sweep": {"kind": "scalar", "default": 0.0,
                      "doc": "0 = nothing drawn, 1 = the closed circle. C++-driven."},

            # UNMEASURED — flagged, not invented. `figma_screen_layout.json` gives the ring
            # a 104x104 box and NO stroke width; the mock's ring art was never exported
            # (Export/UI/ has tracker and minimap rings, none for respawn). 0.0769 is 8px of
            # 104 and is PROVISIONAL. It is a parameter precisely so correcting it is an
            # instance tweak and not a rebuild of a binary. Filed as a contract_gap.
            "Thickness": {"kind": "scalar", "default": 0.0769,
                          "doc": "Ring stroke as a fraction of the 104px box. UNMEASURED."},

            "EdgeSoftness": {"kind": "scalar", "default": 0.004,
                             "doc": "Antialias width, in turns and in UV units. ~1.3px at r=52."},

            # WHITE IS NOT A BAKED COLOUR, IT IS THE IDENTITY. `FSlateBrush::TintColor`
            # multiplies the material's output, so with this at (1,1,1,1) the widget's brush
            # tint — set from `BRUI::ResolveColorToken` — is what colours the ring, and the
            # material carries no colour opinion at all.
            # The parameter exists anyway because it is the path a future
            # `SetVectorParameterValue` takes, and today NOTHING in Source/ sets a vector
            # parameter on any UI material (contract_gap, reported). Whoever adds that line
            # must also set the brush tint to White, or the two multiply.
            "Color": {"kind": "vector", "default": [1.0, 1.0, 1.0, 1.0],
                      "doc": "Identity by default; the widget's brush tint carries the token. "
                             "No C++ consumer yet — see the contract gap."},
        },

        "graph": {
            "UV":        n(TEXCOORD, coordinateIndex=0),
            "Centre":    n(CONST2, r=0.5, g=0.5),
            "P":         n(SUB, inputs={"A": "UV", "B": "Centre"}),
            "R":         n(DIST, inputs={"A": "UV", "B": "Centre"}),

            "Px":        n(MASK, inputs={"Input": "P"}, r=True, g=False, b=False, a=False),
            "Py":        n(MASK, inputs={"Input": "P"}, r=False, g=True, b=False, a=False),
            "MinusOne":  n(CONST, r=-1.0),
            "NegPy":     n(MUL, inputs={"A": "Py", "B": "MinusOne"}),
            # atan2(Y = P.x, X = -P.y): zero at 12 o'clock, increasing clockwise on screen.
            "Ang":       n(ATAN2, inputs={"Y": "Px", "X": "NegPy"}),
            "Tau":       n(CONST, r=TAU),
            "Turns":     n(DIV, inputs={"A": "Ang", "B": "Tau"}),
            "T":         n(FRAC, inputs={"Input": "Turns"}),

            "Sweep":     n(SCALARP, parameterName="Sweep", defaultValue=0.0),
            "Thickness": n(SCALARP, parameterName="Thickness", defaultValue=0.0769),
            "Soft":      n(SCALARP, parameterName="EdgeSoftness", defaultValue=0.004),
            "Color":     n(VECTORP, parameterName="Color",
                           defaultValue={"r": 1.0, "g": 1.0, "b": 1.0, "a": 1.0}),

            # Sweep * (1 + EdgeSoftness) — closes the seam at Sweep = 1. See the header note.
            "One":       n(CONST, r=1.0),
            "SoftPlus1": n(ADD, inputs={"A": "One", "B": "Soft"}),
            "SweepAdj":  n(MUL, inputs={"A": "Sweep", "B": "SoftPlus1"}),

            "SweepMinusT": n(SUB, inputs={"A": "SweepAdj", "B": "T"}),
            "AngRamp":     n(DIV, inputs={"A": "SweepMinusT", "B": "Soft"}),
            # Clamp's own MinDefault/MaxDefault are already 0 and 1, so Min/Max stay
            # unconnected. Fewer wires, and the defaults are engine behaviour, not a guess.
            "AngMask":     n(CLAMP, inputs={"Input": "AngRamp"}),

            # The RING, not a disc: two radial edges, same softness, both antialiased.
            "Half":      n(CONST, r=0.5),
            "OuterGap":  n(SUB, inputs={"A": "Half", "B": "R"}),
            "OuterRamp": n(DIV, inputs={"A": "OuterGap", "B": "Soft"}),
            "OuterMask": n(CLAMP, inputs={"Input": "OuterRamp"}),
            "InnerR":    n(SUB, inputs={"A": "Half", "B": "Thickness"}),
            "InnerGap":  n(SUB, inputs={"A": "R", "B": "InnerR"}),
            "InnerRamp": n(DIV, inputs={"A": "InnerGap", "B": "Soft"}),
            "InnerMask": n(CLAMP, inputs={"Input": "InnerRamp"}),
            "Ring":      n(MUL, inputs={"A": "OuterMask", "B": "InnerMask"}),

            "Wedge":     n(MUL, inputs={"A": "Ring", "B": "AngMask"}),
            "ColorA":    n(MASK, inputs={"Input": "Color"}, r=False, g=False, b=False, a=True),
            "Alpha":     n(MUL, inputs={"A": "Wedge", "B": "ColorA"}),
        },

        # Emissive takes the PARAMETER directly (rgb); a UI-domain material is unlit, so this
        # is the colour. Opacity is the mask. Nothing else is connected.
        "outputs": {EMISSIVE: "Color", OPACITY: "Alpha"},
    },
}


# ---------------------------------------------------------------------------
# THE MATERIAL THIS PACKET WAS ASKED FOR AND DID NOT BUILD
# ---------------------------------------------------------------------------
# Recorded as data, in the plan, so the decision is reviewable and so nobody re-opens the
# question in six weeks by noticing an absence. `validate_all()` asserts these stay absent
# from PLAN, which is the cheapest possible guard against someone adding one back silently.

NOT_BUILT = {
    "M_UI_ArcFill": """The vitals shield/health arc does NOT need a material, on two
    independent grounds, either of which alone is disqualifying under Tier 4.

    1. C++ CAN EXPRESS IT, which is the whole Tier-4 test.
       The job is "reveal a fixed-size arc texture left-to-right by a percent". That is a
       rectangular reveal: `USizeBox::SetWidthOverride(Percent * 273.33)` with
       `SetClipping(EWidgetClipping::ClipToBounds)` over a left-aligned `UImage` carrying
       `HUD_Vitals_ShieldArc` at its natural 275x22. Zero assets, ~4 lines, and it is
       exactly as correct as the material would be — the arc's 2.7px sag is a VERTICAL
       displacement, and a horizontal reveal never has to know the curve exists.
       (What does NOT work is the thing already planned: `UBRProgressBar::Fill` is a stock
       `UProgressBar`, and Slate's LeftToRight fill draws the fill brush into a quad of
       width Percent*W — it STRETCHES the brush, so at 50% you get a whole arc squashed
       into the left half rather than the left half of an arc. That is a real bug in the
       current WBP plan and it is reported as a gap; it is not an argument for a material,
       because the clip-box above fixes it with no binary at all.)

    2. IT WOULD SHIP WITH ZERO CONSUMERS.
       grep of Source/Breachpoint/UI for SetScalarParameterValue / SetVectorParameterValue
       returns exactly ONE hit in the whole tree: `Sweep`, in BRScreen_DeathRespawn.cpp.
       Nothing anywhere sets a `Percent`. `UBRVitalsWidget` binds ShieldBar/HealthBar as
       `UBRProgressBar`, whose only fill path is `Fill->SetPercent`. Wiring a material in
       means changing `UBRVitalsWidget` or `UBRProgressBar` — Source/, not this owner path
       — and a material committed ahead of the C++ that drives it is an unmaintained
       binary by definition.

    Either fix (the clip box, or a material) needs the same C++ change in the same files.
    Given that, the one that adds no binary wins. Filed as a contract_gap against the
    vitals owner; this file builds nothing for it.""",
}


# ---------------------------------------------------------------------------
# Reading the C++ contract — the same discipline as wbp_plan.required_bind_widgets
# ---------------------------------------------------------------------------

def _cpp_literal_re(symbol: str) -> re.Pattern:
    """`Foo::Bar` -> a pattern matching `... Foo::Bar(TEXT("value"))` or `= TEXT("value")`.

    Both spellings are matched because both are idiomatic FName initialisation and the
    project uses the constructor form today. Matching only the form currently in the file
    would make this check fail the day someone reformats, which is the worst possible time
    for a drift guard to cry wolf.
    """
    return re.compile(
        re.escape(symbol) + r"\s*(?:\(\s*TEXT\(\s*\"(?P<v>[^\"]*)\"\s*\)\s*\)"
                            r"|=\s*(?:FName\s*\(\s*)?TEXT\(\s*\"(?P<v2>[^\"]*)\"\s*\))")


def cpp_constant(rel_path: str, symbol: str, text: str | None = None) -> str | None:
    """The string literal a C++ FName constant is initialised with, or None.

    `text` overrides the file read — the self-test uses it, so the negative cases run with
    no repo state at all.
    """
    if text is None:
        p = REPO / rel_path
        if not p.exists():
            return None
        text = p.read_text()
    m = _cpp_literal_re(symbol).search(text)
    if not m:
        return None
    return m.group("v") if m.group("v") is not None else m.group("v2")


# ---------------------------------------------------------------------------
# Validation — every failure here is one that would otherwise be invisible in a binary
# ---------------------------------------------------------------------------

def _reaches(graph: dict, start: str, pred) -> list[str]:
    """Every node reachable upstream from `start` that satisfies `pred`. Cycle-safe."""
    out, seen, stack = [], set(), [start]
    while stack:
        cur = stack.pop()
        if cur in seen or cur not in graph:
            continue
        seen.add(cur)
        if pred(graph[cur]):
            out.append(cur)
        stack.extend(graph[cur]["inputs"].values())
    return sorted(out)


def validate(asset: str, spec: dict, cpp_text: dict[str, str] | None = None) -> list[str]:
    errs: list[str] = []
    graph = spec["graph"]

    # --- the two properties that silently break every UMG material ---------------------
    # A Surface-domain material in a widget brush renders wrong or not at all, and reports
    # nothing. It is the single most common way a UI material fails.
    if spec.get("domain") != "MD_UI":
        errs.append(f"{asset}: MaterialDomain is {spec.get('domain')!r}, must be 'MD_UI' — "
                    "a Surface-domain material in a widget brush renders wrong or not at all")
    if spec.get("blend_mode") != "BLEND_Translucent":
        errs.append(f"{asset}: BlendMode is {spec.get('blend_mode')!r}, must be "
                    "'BLEND_Translucent' — this material composites over the scene")

    # --- graph shape -------------------------------------------------------------------
    seen: set[str] = set()
    for name, node in graph.items():
        for pin, src in node["inputs"].items():
            if src not in graph:
                errs.append(f"{asset}: node '{name}' pin {pin} references unknown '{src}'")
            elif src not in seen:
                errs.append(f"{asset}: node '{name}' pin {pin} references '{src}', which the "
                            "graph lists AFTER it — graph order must be creation order")
        seen.add(name)

    outs = spec.get("outputs", {})
    for want in (EMISSIVE, OPACITY):
        if want not in outs:
            errs.append(f"{asset}: material output {want} is not connected")
    for prop, src in outs.items():
        if src not in graph:
            errs.append(f"{asset}: output {prop} references unknown node '{src}'")

    # --- ZERO HEX ----------------------------------------------------------------------
    # A baked colour in a binary is the same fork as a hex literal in a WBP and strictly
    # worse, because you cannot diff it. Colour arrives as a VectorParameter.
    if EMISSIVE in outs and outs[EMISSIVE] in graph:
        baked = _reaches(graph, outs[EMISSIVE],
                         lambda nd: nd["type"] in BAKED_COLOUR_TYPES)
        if baked:
            errs.append(f"{asset}: baked colour reaches {EMISSIVE} via {baked} — colour must "
                        "be a VectorParameter the widget sets from BRUI::ResolveColorToken")
        emissive_src = graph[outs[EMISSIVE]]
        if emissive_src["type"] != VECTORP:
            errs.append(f"{asset}: {EMISSIVE} is driven by a {emissive_src['type']}, not a "
                        f"{VECTORP} — the colour must be the parameter itself")

    # --- params table vs the actual graph ----------------------------------------------
    # Two places name a parameter (the documented table and the node), so the redundancy is
    # checked rather than left free to drift — same reasoning as wbp_plan's class/parent_class.
    declared = spec.get("params", {})
    in_graph = {nd["props"].get("parameterName"): (name, nd)
                for name, nd in graph.items() if nd["type"] in (SCALARP, VECTORP)}
    for pname, info in declared.items():
        if pname not in in_graph:
            errs.append(f"{asset}: params declares '{pname}' but no parameter node in the "
                        "graph carries that name")
            continue
        node_name, node = in_graph[pname]
        want_type = SCALARP if info["kind"] == "scalar" else VECTORP
        if node["type"] != want_type:
            errs.append(f"{asset}: '{pname}' is declared {info['kind']} but node "
                        f"'{node_name}' is a {node['type']}")
        got = node["props"].get("defaultValue")
        want = info["default"]
        if info["kind"] == "vector":
            got = [got.get(k) for k in "rgba"] if isinstance(got, dict) else got
        if got != want:
            errs.append(f"{asset}: '{pname}' default is {want} in params but {got} on node "
                        f"'{node_name}'")
    for pname in in_graph:
        if pname not in declared:
            errs.append(f"{asset}: graph exposes parameter '{pname}' that params does not "
                        "declare — an undocumented parameter is an unowned one")

    # --- THE CHECK THIS FILE EXISTS FOR ------------------------------------------------
    # A `SetScalarParameterValue` against a name the material does not have is a NO-OP.
    # No warning, no log, no crash — the ring just never sweeps. Caught here, in text mode.
    for pname, (rel, symbol) in spec.get("cpp_constants", {}).items():
        override = (cpp_text or {}).get(rel)
        got = cpp_constant(rel, symbol, override)
        if got is None:
            errs.append(f"{asset}: cannot read C++ constant {symbol} from {rel} — the "
                        "parameter-name contract cannot be verified")
        elif got != pname:
            errs.append(f"{asset}: parameter '{pname}' but {symbol} in {rel} is "
                        f"'{got}' — SetScalarParameterValue would silently no-op")
        if pname not in declared:
            errs.append(f"{asset}: cpp_constants names '{pname}', which params does not "
                        "declare")

    return errs


def validate_all(cpp_text: dict[str, str] | None = None) -> list[str]:
    errs: list[str] = []
    for asset, spec in PLAN.items():
        errs += validate(asset, spec, cpp_text)
    # Cheapest possible guard on the "we decided not to build this" ruling: if someone adds
    # it back, they have to delete the reasoning too, and that shows up in a diff.
    for asset in NOT_BUILT:
        if asset in PLAN:
            errs.append(f"{asset}: listed in NOT_BUILT with a written justification and ALSO "
                        "in PLAN — resolve the contradiction before building anything")
    return errs


def describe(asset: str, spec: dict) -> str:
    """The graph as readable text. This is what a reviewer reads instead of the binary."""
    lines = [f"{asset}  ->  {spec['folder']}/{asset}.{asset}",
             f"  domain {spec['domain']} · blend {spec['blend_mode']} · "
             f"shading {spec['shading_model']}",
             f"  {spec['notes']}", "  parameters:"]
    for p, i in spec["params"].items():
        lines.append(f"    {i['kind']:6} {p:12} = {i['default']}   {i['doc']}")
    lines.append("  graph:")
    for name, node in spec["graph"].items():
        wires = " ".join(f"{k}<-{v}" for k, v in node["inputs"].items())
        props = " ".join(f"{k}={v}" for k, v in node["props"].items())
        lines.append(f"    {name:12} {node['type'][len('MaterialExpression'):]:20} "
                     f"{wires}  {props}".rstrip())
    lines.append("  outputs:")
    for prop, src in spec["outputs"].items():
        lines.append(f"    {prop} <- {src}")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Self-check when run directly. The NEGATIVE cases live in selftest_materials.py —
# this is the positive one, run against the real repo and the real .cpp.
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    import sys

    problems = validate_all()
    for p in problems:
        print("ERROR:", p)
    if problems:
        sys.exit(1)

    for asset, spec in PLAN.items():
        print(describe(asset, spec))
        for pname, (rel, symbol) in spec.get("cpp_constants", {}).items():
            print(f"  C++ contract: {symbol} == \"{cpp_constant(rel, symbol)}\"  ({rel})")
        print()
    for asset, why in NOT_BUILT.items():
        print(f"NOT BUILT — {asset}")
        print("  " + " ".join(why.split())[:400] + " ...")
        print()
    print("PLAN OK")
