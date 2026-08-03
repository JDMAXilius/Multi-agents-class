#!/usr/bin/env python3
"""The R26 expectation for every Widget Blueprint — pure CPython, no engine, no MCP.

    python3 Tools/audit_ui/wbp_expect.py          # print what the audit will demand

This file answers "what must a WBP look like" and "given what the editor reported, what
is wrong". `audit_wbp.py` supplies the editor's answers; nothing here talks to anything.

NOTHING IS HARDCODED. The expectation is derived, so it cannot go stale when the WBP
count multiplies:

  * the asset list comes from `Content/**/WBP_*.uasset` on disk;
  * the required C++ parent comes from the naming law — strip `WBP_`, prefix `UBR`, and
    find the header that declares it;
  * the required `BindWidget` names come from PARSING THAT HEADER.

R26 conditions, mechanically (`docs/DESIGN-RULINGS.md`):
  1. parent chain      → the WBP's parent class is the `BR` C++ class the name implies
  2. node count        → ZERO graph nodes
  3. added members     → ZERO new variables, functions, events

`Tools/gen_ui/wbp_plan.py` carries a twin of `BIND_RE`. That duplication is deliberate:
an audit that imports the generator it audits can be broken — or fooled — by the thing it
is supposed to check, and a gate with a false-red dependency on another lane's file is a
gate people learn to ignore. `selftest_no_editor.py` compares the two copies on the real
headers when gen_ui is present, so drift is caught without the coupling.
"""
from __future__ import annotations

import re
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

# Twin of Tools/gen_ui/wbp_plan.py::_BIND_RE — see the module docstring.
BIND_RE = re.compile(
    r"UPROPERTY\([^)]*\bmeta\s*=\s*\([^)]*\bBindWidget(?P<opt>Optional)?\b[^)]*\)[^)]*\)"
    r"\s*(?:TObjectPtr<\s*(?P<ptr>\w+)\s*>|(?P<raw>\w+)\s*\*)\s*(?P<name>\w+)\s*;",
    re.S,
)

# UMG stamps every fresh Widget Blueprint's EventGraph with three unconnected default
# event stubs (`FKismetEditorUtilities::AddDefaultEventNode`). They carry no logic and the
# compiler drops them. A gate that called them "nodes" would fail every conforming asset,
# and a gate that cries wolf is switched off — which is the same death as no gate at all.
# So the measure is: zero nodes BEYOND these three, and only while their bodies are empty.
DEFAULT_EVENT_STUBS = frozenset({
    "UserInterface|EventPreConstruct",
    "UserInterface|EventConstruct",
    "UserInterface|EventTick",
})

STATEMENT_KEYWORDS = frozenset({
    "bind", "if", "elif", "else", "for", "while", "switch", "break", "return", "range",
})


# --------------------------------------------------------------------------- discovery
_CLASS_DECL = re.compile(r"^class\s+(?:\w+_API\s+)?(?P<name>U\w+)\s*:", re.M)


def class_body(header_text: str, cpp_class: str) -> str:
    """The slice of a header belonging to ONE class.

    Not cosmetic. `BRHUDLayout.h` declares BOTH `UBRKillfeedEntryWidget` and `UBRHUDLayout`;
    parsing the whole file handed the killfeed row a `KillfeedContainer` bind that belongs to
    the HUD. It passed only because that bind happens to be Optional — the same mistake on a
    non-optional member is a confident, wrong `high` finding against an innocent asset.
    """
    starts = [(m.start(), m.group("name")) for m in _CLASS_DECL.finditer(header_text)]
    for i, (pos, name) in enumerate(starts):
        if name == cpp_class:
            end = starts[i + 1][0] if i + 1 < len(starts) else len(header_text)
            return header_text[pos:end]
    return ""


def binds_from_text(header_text: str, cpp_class: str | None = None) -> dict[str, dict]:
    """{member: {"class": "UPanelWidget", "optional": bool}} parsed out of a header."""
    if cpp_class:
        header_text = class_body(header_text, cpp_class)
    return {m.group("name"): {"class": m.group("ptr") or m.group("raw"),
                             "optional": bool(m.group("opt"))}
            for m in BIND_RE.finditer(header_text)}


def find_header(cpp_class: str, source_root: Path | None = None) -> Path | None:
    """The header that DECLARES `cpp_class` — not one that merely forward-declares it."""
    root = source_root or (REPO / "Source")
    decl = re.compile(rf"\bclass\s+(?:\w+_API\s+)?{re.escape(cpp_class)}\s*:", re.S)
    for h in sorted(root.rglob("*.h")):
        if decl.search(h.read_text(errors="replace")):
            return h
    return None


def game_path(uasset: Path, content_root: Path) -> str:
    """Content/UI/WBP_X.uasset -> /Game/UI/WBP_X.WBP_X (the ONLY form the server accepts).

    The short form makes the MCP server drop the whole argument object and report
    "input params Json is empty", which names the wrong cause entirely.
    """
    rel = uasset.relative_to(content_root).with_suffix("").as_posix()
    return f"/Game/{rel}.{uasset.stem}"


def discover(content_root: Path | None = None, source_root: Path | None = None) -> list[dict]:
    content_root = content_root or (REPO / "Content")
    out = []
    for ua in sorted(content_root.rglob("WBP_*.uasset")):
        asset = ua.stem
        cpp = "UBR" + asset[len("WBP_"):]
        hdr = find_header(cpp, source_root)
        out.append({
            "asset": asset,
            "game_path": game_path(ua, content_root),
            "cpp_class": cpp,
            # `BR`-prefixed classes all live in the one runtime module (CLAUDE.md).
            "expected_parent": f"/Script/Breachpoint.{cpp[1:]}",
            "header": hdr.relative_to(REPO).as_posix() if hdr else None,
            "binds": binds_from_text(hdr.read_text(errors="replace"), cpp) if hdr else {},
        })
    return out


# --------------------------------------------------------------------------- the DSL
def parse_sexprs(text: str) -> list:
    """S-expressions -> nested lists of str. Unbalanced input raises."""
    toks = re.findall(r'\(|\)|"(?:[^"\\]|\\.)*"|;[^\n]*|[^\s()]+', text)
    stack, cur = [], []
    for t in toks:
        if t.startswith(";"):
            continue
        if t == "(":
            stack.append(cur)
            cur = []
        elif t == ")":
            if not stack:
                raise ValueError("unbalanced ')' in graph DSL")
            parent = stack.pop()
            parent.append(cur)
            cur = parent
        else:
            cur.append(t)
    if stack:
        raise ValueError("unbalanced '(' in graph DSL")
    return cur


def _is_param_list(form) -> bool:
    """`(IsDesignTime)` after an event name is a PARAMETER list, not a statement.

    The DSL is ambiguous here — `(return)` is also a list of one bare word. Every
    tie-break below errs toward calling it a statement, because a gate that
    under-reports is worse than one that over-reports.
    """
    return (isinstance(form, list) and form
            and all(isinstance(x, str) for x in form)
            and not any(x in STATEMENT_KEYWORDS or "|" in x or x[0] in '"-.0123456789'
                        for x in form))


def graph_nodes(dsl_text: str) -> dict:
    """{"nodes": int, "detail": [str]} — R26 condition 2, measured.

    "nodes" counts DSL forms, not literally UE `UEdGraphNode`s: `(+ a b)` is one form and
    one node, but a node with no DSL representation (a comment box, a reroute) is invisible
    here. The gate only ever asks zero-vs-nonzero, and on that question the count is exact.
    """
    detail: list[str] = []
    n = 0
    try:
        forms = parse_sexprs(dsl_text or "")
    except ValueError as e:
        return {"nodes": -1, "detail": [f"graph DSL did not parse ({e}) — treat as unknown"]}

    for form in forms:
        if not isinstance(form, list) or not form:
            n += 1
            detail.append(f"stray top-level token {form!r}")
            continue
        head = form[0]
        if head == "event" and len(form) >= 2:
            name = form[1]
            body = form[2:]
            if body and _is_param_list(body[0]):
                body = body[1:]
            body_n = sum(_count(f) for f in body)
            if name not in DEFAULT_EVENT_STUBS:
                n += 1 + body_n
                detail.append(f"event `{name}` is not an engine default stub "
                              f"(+1 added member, {body_n} node(s) of body)")
            elif body_n:
                n += body_n
                detail.append(f"default stub `{name}` has {body_n} node(s) of logic in it")
        else:
            c = _count(form)
            n += c
            detail.append(f"top-level `{head}` form — {c} node(s)")
    return {"nodes": n, "detail": detail}


def _count(form) -> int:
    if not isinstance(form, list):
        return 0
    return 1 + sum(_count(f) for f in form)


# --------------------------------------------------------------------------- judgement
def judge(exp: dict, obs: dict, compatible) -> list[tuple[str, str]]:
    """[(severity, message)] for ONE asset. Pure: `obs` is whatever the editor said.

    `compatible(declared_leaf, actual_refpath) -> bool | None` — None means "the class
    could not be resolved", which is reported rather than silently passed.
    """
    f: list[tuple[str, str]] = []
    a = exp["asset"]

    if exp["header"] is None:
        f.append(("high", f"{a}: no header declares `{exp['cpp_class']}` — the asset is "
                          f"either misnamed (R26 cond. 5: WBP_<CppClassWithoutPrefix>) or "
                          f"parented to something that is not a BR C++ class"))

    # --- condition 1: parent chain
    parent = obs.get("parent")
    if parent != exp["expected_parent"]:
        f.append(("high", f"{a}: parent is `{parent}`, R26 cond. 1 requires "
                          f"`{exp['expected_parent']}`"))

    # --- condition 3: added members
    for v in obs.get("variables") or []:
        f.append(("high", f"{a}: declares Blueprint variable `{v}` — R26 cond. 3 allows none"))

    for g in obs.get("graphs", {}):
        if g != "EventGraph":
            f.append(("high", f"{a}: has graph `{g}` — a function/macro graph is a new "
                              f"member (R26 cond. 3) and logic (cond. 2)"))

    # --- condition 2: node count
    for gname, dsl in (obs.get("graphs") or {}).items():
        r = graph_nodes(dsl)
        if r["nodes"] < 0:
            f.append(("medium", f"{a}: {gname} — {r['detail'][0]}"))
        elif r["nodes"]:
            f.append(("high", f"{a}: {gname} holds {r['nodes']} node(s), R26 cond. 2 "
                              f"requires ZERO — {'; '.join(r['detail'])}"))

    # --- the BindWidget contract (not R26; the failure UMG defers to asset LOAD time)
    tree = {w["name"]: w for w in obs.get("widgets") or []}
    for name, info in (exp["binds"] or {}).items():
        w = tree.get(name)
        if w is None:
            if not info["optional"]:
                f.append(("high", f"{a}: `{name}` is a NON-OPTIONAL BindWidget in "
                                  f"{exp['header']} and the tree has no such widget — the "
                                  f"WBP fails to compile at asset load, silently, in PIE"))
            else:
                f.append(("info", f"{a}: optional bind `{name}` is not built (allowed)"))
            continue
        ok = compatible(info["class"], w["class"])
        if ok is None:
            f.append(("medium", f"{a}: could not resolve declared class `{info['class']}` "
                                f"for `{name}` — compatibility UNVERIFIED"))
        elif not ok:
            f.append(("high", f"{a}: `{name}` is `{info['class']}` in {exp['header']} but "
                              f"`{w['class']}` in the tree, and that is not a subclass"))
        elif not w.get("inherited"):
            f.append(("medium", f"{a}: `{name}` exists but the editor does not report it as "
                                f"inherited — the C++ bind is probably not wired"))

    # A widget flagged "Is Variable" that no BindWidget backs adds a generated member to the
    # widget class. Inert, but it is a member R26 cond. 3 did not license, and it is how
    # "just one variable" starts. medium, not high: it holds no logic and blocks nothing.
    for w in obs.get("widgets") or []:
        if w.get("isVariable") and not w.get("inherited") and w["name"] not in (exp["binds"] or {}):
            f.append(("medium", f"{a}: widget `{w['name']}` is marked Is Variable but no "
                                f"BindWidget in {exp['header']} backs it — an added member "
                                f"(R26 cond. 3) with nothing in C++ to reach it"))
    return f


def main() -> int:
    assets = discover()
    for e in assets:
        print(f"{e['asset']}  ->  {e['game_path']}")
        print(f"    parent must be  {e['expected_parent']}")
        print(f"    header          {e['header'] or '**NONE FOUND**'}")
        for n, i in (e["binds"] or {}).items():
            print(f"    bind {'(optional) ' if i['optional'] else '            '}{n}: {i['class']}")
        if not e["binds"]:
            print("    bind            (none declared)")
    print(f"\n{len(assets)} Widget Blueprint(s) discovered under Content/.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
