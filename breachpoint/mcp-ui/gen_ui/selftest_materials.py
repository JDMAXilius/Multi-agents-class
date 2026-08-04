#!/usr/bin/env python3
"""Prove the material plan with no engine, no editor, no MCP.

    python3 mcp-ui/gen_ui/selftest_materials.py

Two jobs, and the second is the one that matters.

1. EVALUATE THE GRAPH. `material_plan.PLAN`'s node graph is re-implemented here in ~30
   lines of float arithmetic and the stated behaviour is asserted numerically: Sweep 0 draws
   nothing, Sweep 1 closes the circle with no seam gap, the sweep runs CLOCKWISE FROM 12,
   and it is a ring rather than a disc. A material graph is otherwise unreviewable — the
   only way to check the trigonometry is to open the binary and stare at it, and staring is
   how a `-P.y` becomes a `P.y` and the ring runs backwards for a month.

2. FIRE EVERY GATE. A gate that has never fired has not been tested. Each negative case
   below mutates a real spec into a specific, plausible mistake and asserts that
   `validate()` produces the specific error — including the three the packet named:
   wrong material domain, a baked colour, and a parameter name that disagrees with its C++
   constant.
"""
from __future__ import annotations

import copy
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import material_plan as P

FAILS: list[str] = []


def check(name, cond, detail=""):
    print(f"  {'ok  ' if cond else 'FAIL'}  {name}"
          + (f" — {detail}" if detail and not cond else ""))
    if not cond:
        FAILS.append(name)


def fires(name, errs, needle):
    """A negative case: `validate()` must produce an error CONTAINING `needle`.

    Matching on a substring, not just on len(errs) > 0, because "some error fired" is how a
    gate passes its own test while catching the wrong thing.
    """
    hit = [e for e in errs if needle in e]
    check(name, bool(hit), f"no error containing {needle!r}; got {errs}")


SWEEP = P.PLAN["M_UI_RadialSweep"]


# ===========================================================================
# 1. THE GRAPH, EVALUATED
# ===========================================================================
def _vec(v):
    return v if isinstance(v, tuple) else (v,)


def _bin(op, a, b):
    a, b = _vec(a), _vec(b)
    n = max(len(a), len(b))
    a = a * n if len(a) == 1 else a
    b = b * n if len(b) == 1 else b
    r = tuple(op(x, y) for x, y in zip(a, b))
    return r[0] if len(r) == 1 else r


def evaluate(graph, node, uv, params):
    """One node's value. Scalars are floats; vectors are tuples. Recursive, memo-free —
    the graph is 30 nodes, and a cache would be more code than the thing it speeds up."""
    # A source may be ("NodeName", "OutputPin"). The named pin that exists today is a
    # VectorParameter's "A", which this evaluator models as the 4th component - the
    # engine refused a ComponentMask 0001 over a float3 because the unnamed output of a
    # VectorParameter carries RGB only.
    if isinstance(node, tuple):
        node, out_pin = node
        val = evaluate(graph, node, uv, params)
        if out_pin == "A":
            return val[3] if isinstance(val, (tuple, list)) and len(val) > 3 else 1.0
        if out_pin in ("R", "G", "B"):
            return val["RGB".index(out_pin)] if isinstance(val, (tuple, list)) else val
        return val
    nd = graph[node]
    t, pr = nd["type"], nd["props"]
    ins = {k: evaluate(graph, v, uv, params) for k, v in nd["inputs"].items()}

    if t == P.TEXCOORD:
        return uv
    if t == P.CONST:
        return pr["r"]
    if t == P.CONST2:
        return (pr["r"], pr["g"])
    if t == P.CONST3:
        return (pr["r"], pr["g"], pr["b"])
    if t == P.CONST4:
        return (pr["r"], pr["g"], pr["b"], pr["a"])
    if t == P.SCALARP:
        return params.get(pr["parameterName"], pr["defaultValue"])
    if t == P.VECTORP:
        d = pr["defaultValue"]
        return params.get(pr["parameterName"], (d["r"], d["g"], d["b"], d["a"]))
    if t == P.SUB:
        return _bin(lambda x, y: x - y, ins["A"], ins["B"])
    if t == P.ADD:
        return _bin(lambda x, y: x + y, ins["A"], ins["B"])
    if t == P.MUL:
        return _bin(lambda x, y: x * y, ins["A"], ins["B"])
    if t == P.DIV:
        return _bin(lambda x, y: x / y, ins["A"], ins["B"])
    if t == P.DIST:
        a, b = _vec(ins["A"]), _vec(ins["B"])
        return math.dist(a, b)
    if t == P.ATAN2:
        return math.atan2(ins["Y"], ins["X"])
    if t == P.FRAC:
        # HLSL frac is x - floor(x): ALWAYS in [0,1), so frac(-0.25) == 0.75. The whole
        # clockwise-from-12 mapping rests on this and on nothing else.
        return ins["Input"] - math.floor(ins["Input"])
    if t == P.CLAMP:
        # Clamp's engine MinDefault/MaxDefault are 0 and 1 and the plan leaves Min/Max
        # unconnected, so this mirrors the unconnected case exactly.
        return min(1.0, max(0.0, ins["Input"]))
    if t == P.MASK:
        v = _vec(ins["Input"])
        sel = [v[i] for i, c in enumerate("rgba") if pr.get(c) and i < len(v)]
        return sel[0] if len(sel) == 1 else tuple(sel)
    raise AssertionError(f"self-test cannot evaluate {t} — the plan grew a node type "
                         "this evaluator does not model, so the graph is NOT verified")


def alpha_at(turn, sweep, radius=None, params=None):
    """Alpha at `turn` turns CLOCKWISE FROM 12 on the ring's centre line."""
    thick = SWEEP["params"]["Thickness"]["default"]
    r = radius if radius is not None else 0.5 - thick / 2.0
    a = turn * 2 * math.pi
    uv = (0.5 + r * math.sin(a), 0.5 - r * math.cos(a))
    p = {"Sweep": sweep}
    p.update(params or {})
    return evaluate(SWEEP["graph"], SWEEP["outputs"][P.OPACITY], uv, p)


print("1. the graph, evaluated (pure Python, no engine)")

check("all 30-odd node types are modelled by this evaluator",
      alpha_at(0.1, 0.5) is not None)

# Sweep 0 -> NOTHING. Not "a hairline at 12".
check("Sweep=0 draws nothing, at every angle",
      all(alpha_at(t / 64, 0.0) == 0.0 for t in range(64)),
      f"max {max(alpha_at(t / 64, 0.0) for t in range(64))}")

# Sweep 1 -> a CLOSED circle. The point of the (1 + EdgeSoftness) widening is exactly the
# sliver just counter-clockwise of 12, so that is where this samples hardest.
soft = SWEEP["params"]["EdgeSoftness"]["default"]
check("Sweep=1 fills the whole ring",
      all(alpha_at(t / 256, 1.0) > 0.99 for t in range(256)),
      f"min {min(alpha_at(t / 256, 1.0) for t in range(256)):.4f}")
check("Sweep=1 leaves NO seam gap immediately counter-clockwise of 12",
      alpha_at(1.0 - soft / 2, 1.0) > 0.99, f"got {alpha_at(1.0 - soft / 2, 1.0):.4f}")

# CLOCKWISE FROM 12, which is the one thing a reader cannot check by looking at the binary.
check("Sweep=0.25 covers 1 and 2 o'clock (clockwise from 12)",
      alpha_at(0.05, 0.25) > 0.99 and alpha_at(0.20, 0.25) > 0.99)
check("Sweep=0.25 does NOT cover 9 o'clock (i.e. it is not anticlockwise)",
      alpha_at(0.75, 0.25) == 0.0, f"got {alpha_at(0.75, 0.25)}")
check("Sweep=0.25 stops just past 3 o'clock",
      alpha_at(0.24, 0.25) > 0.99 and alpha_at(0.30, 0.25) == 0.0)
check("12 o'clock is the START, not the middle",
      alpha_at(0.001, 0.5) > 0.99 and alpha_at(0.999, 0.5) == 0.0)
check("the sweep is monotonic in Sweep at a fixed angle",
      alpha_at(0.4, 0.3) <= alpha_at(0.4, 0.5) <= alpha_at(0.4, 0.9))

# RING, not disc.
thick = SWEEP["params"]["Thickness"]["default"]
check("the centre of the circle is empty (ring, not disc)",
      alpha_at(0.1, 1.0, radius=0.0) == 0.0)
check("inside the inner radius is empty",
      alpha_at(0.1, 1.0, radius=0.5 - thick - 0.01) == 0.0)
check("outside the outer radius is empty",
      alpha_at(0.1, 1.0, radius=0.51) == 0.0)
check("the ring edges are ANTIALIASED, not binary",
      0.0 < alpha_at(0.1, 1.0, radius=0.5 - soft / 2) < 1.0,
      f"got {alpha_at(0.1, 1.0, radius=0.5 - soft / 2):.4f}")
check("Thickness widens the ring", alpha_at(0.1, 1.0, radius=0.5 - thick - 0.01,
                                            params={"Thickness": thick + 0.05}) > 0.99)

# Colour: the material must have no colour opinion.
check("Color defaults to identity white, so the brush tint is what colours the ring",
      SWEEP["params"]["Color"]["default"] == [1.0, 1.0, 1.0, 1.0])
check("Color.A scales the mask",
      abs(alpha_at(0.1, 1.0, params={"Color": (1.0, 1.0, 1.0, 0.5)}) - 0.5) < 1e-6)


# ===========================================================================
# 2. THE REAL PLAN, AGAINST THE REAL REPO
# ===========================================================================
print("\n2. the real plan")

check("the shipped plan validates clean", not P.validate_all(), f"{P.validate_all()}")
check("the C++ constant is readable and says 'Sweep'",
      P.cpp_constant("Source/Breachpoint/UI/Screens/BRScreen_DeathRespawn.cpp",
                     "UBRScreen_DeathRespawn::RespawnRingSweepParameterName") == "Sweep")
check("the material is User Interface domain", SWEEP["domain"] == "MD_UI")
check("the material is Translucent", SWEEP["blend_mode"] == "BLEND_Translucent")
check("M_UI_ArcFill is NOT built", "M_UI_ArcFill" not in P.PLAN)
check("M_UI_ArcFill's absence is justified in writing",
      len(P.NOT_BUILT.get("M_UI_ArcFill", "")) > 200)


# ===========================================================================
# 3. NEGATIVE CASES — every gate must FIRE
# ===========================================================================
print("\n3. negative cases (each must fire)")


def mutated(**over):
    s = copy.deepcopy(SWEEP)
    s.update(over)
    return s


CPP_OK = {"Source/Breachpoint/UI/Screens/BRScreen_DeathRespawn.cpp":
          'const FName UBRScreen_DeathRespawn::RespawnRingSweepParameterName(TEXT("Sweep"));'}


def errs(spec, cpp=None):
    return P.validate("TEST", spec, cpp or CPP_OK)

check("the mutation harness itself is clean before mutating", not errs(mutated()),
      f"{errs(mutated())}")

# --- the packet's three named gates -------------------------------------------------
fires("wrong material domain (MD_Surface) is caught",
      errs(mutated(domain="MD_Surface")), "must be 'MD_UI'")

s = mutated()
s["graph"]["Baked"] = P.n(P.CONST3, r=0.208, g=0.816, b=0.949)      # the Shield token, hexed
s["graph"]["BakedTint"] = P.n(P.MUL, inputs={"A": "Color", "B": "Baked"})
s["outputs"] = dict(s["outputs"], **{P.EMISSIVE: "BakedTint"})
fires("a baked colour reaching EmissiveColor is caught", errs(s), "baked colour reaches")

fires("a parameter name that disagrees with its C++ constant is caught",
      errs(mutated(), {"Source/Breachpoint/UI/Screens/BRScreen_DeathRespawn.cpp":
                       'const FName UBRScreen_DeathRespawn::'
                       'RespawnRingSweepParameterName(TEXT("Progress"));'}),
      "would silently no-op")

# --- and the ones next door, because the same mistake wears several hats ------------
fires("a non-translucent blend mode is caught",
      errs(mutated(blend_mode="BLEND_Opaque")), "BLEND_Translucent")

s = mutated()
s["graph"]["Color"]["props"]["parameterName"] = "Colour"
fires("renaming a parameter node without updating the params table is caught",
      errs(s), "params does not")

s = mutated()
s["params"]["Sweep"]["default"] = 1.0
fires("a params-table default drifting from the node's default is caught",
      errs(s), "default is 1.0")

s = mutated()
s["graph"]["Alpha"]["inputs"]["B"] = "NotANode"
fires("a dangling input reference is caught", errs(s), "references unknown")

s = mutated()
s["graph"] = {"Alpha": s["graph"]["Alpha"], **s["graph"]}      # Alpha first: forward refs
fires("a forward reference (graph order != creation order) is caught",
      errs(s), "graph lists AFTER it")

s = mutated()
s["outputs"] = {P.EMISSIVE: "Color"}
fires("an unconnected Opacity output is caught", errs(s), "Opacity is not connected")

s = mutated()
s["outputs"] = dict(s["outputs"], **{P.EMISSIVE: "Alpha"})
fires("EmissiveColor driven by something that is not a VectorParameter is caught",
      errs(s), "not a MaterialExpressionVectorParameter")

s = mutated()
s["cpp_constants"]["Sweep"] = ("Source/Breachpoint/UI/Screens/NoSuchFile.cpp", "Nope::Nope")
fires("an unreadable C++ constant is a failure, not a silent skip",
      P.validate("TEST", s), "cannot be verified")

s = mutated()
s["cpp_constants"]["Undeclared"] = s["cpp_constants"].pop("Sweep")
fires("cpp_constants naming a parameter the params table lacks is caught",
      errs(s, {"Source/Breachpoint/UI/Screens/BRScreen_DeathRespawn.cpp":
               'const FName UBRScreen_DeathRespawn::'
               'RespawnRingSweepParameterName(TEXT("Undeclared"));'}),
      "params does not declare")

# The NOT_BUILT ruling, guarded: adding the material back means deleting the reasoning,
# and that shows up in a diff.
saved = dict(P.PLAN)
try:
    P.PLAN["M_UI_ArcFill"] = copy.deepcopy(SWEEP)
    P.NOT_BUILT["M_UI_ArcFill"]           # still justified as not-built
    fires("building something NOT_BUILT still documents is caught",
          P.validate_all(CPP_OK), "resolve the contradiction")
finally:
    P.PLAN.clear()
    P.PLAN.update(saved)

print()
if FAILS:
    print(f"SELF-TEST FAILED: {len(FAILS)} check(s) — {FAILS}")
    sys.exit(1)
print("SELF-TEST PASSED — graph math verified numerically, every gate fired.")
