#!/usr/bin/env python3
"""Assignment #5 — a goal-oriented coding agent for BREACHPOINT.

    read the GDD -> scan the codebase -> detect gaps -> prioritise -> generate

Run it:

    python3 agent.py              # replay the committed run — stdlib only, no API key
    python3 agent.py --live       # real model call for the code generation step
    python3 agent.py --scan       # perception only
    python3 agent.py --rank       # the ranked table, every scoring term printed

## The one design decision worth stating up front

**The agent decides deterministically and writes with a model.** Stages 1-4 —
reading the GDD, scanning source, detecting gaps, ranking them — are plain
Python with no model call anywhere. Only stage 5, writing the C++, calls a
model. The assignment says the objective is *"the reasoning layer; how the agent
decides what to build and in what order"*, and a reasoning layer you cannot
audit is not one you understand. Every scoring term is printed for every
candidate, so the selection can be recomputed by hand from `output/ranking.json`.

## The target is frozen, on purpose

The agent reads and writes `./project/` — a pinned copy of BREACHPOINT (see
`project/PROVENANCE.md`), never the live game tree, which changes daily. An
agent whose inputs move under it cannot be reproduced or graded. Generated code
lands in the frozen copy; porting it into the real game is a separate manual
step, described in the README.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field, asdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE / "project"
SOURCE = PROJECT / "Source" / "Breachpoint"
DATA = PROJECT / "Content" / "Data"
GDD = PROJECT / "GDD.md"
OUT = HERE / "output"
RECORDING = HERE / "recording.json"
MODEL = "claude-sonnet-5"


def log(msg: str = ""):
    print(msg, flush=True)


def guard_path(p: Path):
    """Refuse to touch anything outside the frozen copy.

    The whole point of the freeze is that the live tree is off-limits. A comment
    saying so would be a promise; this is a check.
    """
    resolved = Path(p).resolve()
    if PROJECT.resolve() not in resolved.parents and resolved != PROJECT.resolve():
        sys.exit(f"error: refusing to touch {resolved} — outside {PROJECT}")
    return resolved


# ===========================================================================
# 1. READ THE GDD — extract the features and systems it describes
# ===========================================================================
#
# The feature list is not inferred, and it is not mine: it is the GDD's own
# §5.1 "Shipped Scope", which the document ends with the word "Nothing else."
# That sentence is the closest thing the project has to a definitive statement
# of what the game is, and it is semicolon-delimited, so it parses exactly
# rather than approximately. Headings would have been easier and worse — §2.7
# "Information Without Radar" is a design consequence, not a feature.

STOP = {"the", "and", "with", "for", "any", "all", "one", "two", "three", "plus",
        "from", "into", "each", "per", "its", "not", "but", "that", "this",
        "minimal", "front", "end", "including", "filling", "slot", "slots",
        "settings", "timer", "level", "map"}


@dataclass
class Feature:
    id: str
    text: str                       # verbatim from §5.1
    keys: list[str]                 # what a symbol must contain to implement it
    gdd_mentions: int = 0           # how often the GDD talks about it overall
    built: bool = False
    evidence: list[str] = field(default_factory=list)
    inputs: list[str] = field(default_factory=list)
    integration: int = 0
    score: float = 0.0
    terms: dict = field(default_factory=dict)


def _keys_from(text: str) -> list[str]:
    """Distinctive lowercase stems a matching symbol would contain."""
    words = re.findall(r"[A-Za-z][A-Za-z-]+", text.lower())
    out = []
    for w in words:
        for part in w.split("-"):
            part = part.rstrip("s") if len(part) > 5 and part.endswith("s") else part
            if len(part) >= 4 and part not in STOP and part not in out:
                out.append(part)
    return out


def read_gdd_features() -> tuple[list[Feature], str]:
    text = GDD.read_text(encoding="utf-8")
    m = re.search(r"### 5\.1 Shipped Scope\s*\n(.+?)\n\s*###", text, re.S)
    if not m:
        sys.exit("error: could not find '### 5.1 Shipped Scope' in the GDD — the "
                 "document changed shape; fix the parse rather than guessing")
    scope = " ".join(m.group(1).split())
    scope = re.sub(r"\*\*|\*", "", scope).split("Nothing else")[0]

    features = []
    for frag in (f.strip(" .;") for f in scope.split(";")):
        if not frag:
            continue
        keys = _keys_from(frag)
        if not keys:
            continue
        fid = re.sub(r"[^a-z0-9]+", "-", frag.lower()).strip("-")[:40]
        f = Feature(id=fid, text=frag, keys=keys)
        # How central is this to the design? Count how often the whole document
        # returns to it. A feature the GDD mentions once is a mention; one it
        # returns to twenty times is load-bearing.
        low = text.lower()
        f.gdd_mentions = sum(low.count(k) for k in keys)
        features.append(f)
    return features, text


# ===========================================================================
# 2. SCAN THE CODEBASE — read the source to see what is already built
# ===========================================================================

DECL_RE = re.compile(r"^\s*(?:UCLASS|USTRUCT|UINTERFACE)\s*\(.*?\)\s*$|"
                     r"^\s*(?:class|struct)\s+(?:\w+_API\s+)?(\w+)", re.M)


def matches(key: str, name: str) -> bool:
    """Does `name` implement something called `key`?

    Plain containment is not enough. The GDD calls a feature "Grappleshot" and
    the code calls it `BRGA_Grapple`, so `"grappleshot" in "brga_grapple"` is
    False and the feature reads as missing when it is built. So: containment, or
    a shared run of >= 6 characters. Six, not four — "core", "menu" and "team"
    are four-character runs that appear inside half the codebase and would make
    everything look built.
    """
    k = re.sub(r"[^a-z]", "", key.lower())
    n = re.sub(r"[^a-z]", "", name.lower())
    if len(k) >= 4 and k in n:
        return True
    for length in range(len(k), 5, -1):
        for i in range(len(k) - length + 1):
            if k[i:i + length] in n:
                return True
    return False


# A feature is BUILT when something *does* it, not when something *describes*
# it. UE's prefixes make that checkable: U/A/I are runtime classes; F and E are
# plain structs and enums, which in this project are overwhelmingly DataTable
# row types — inputs to a feature, never the feature. Ignoring this reported the
# Spotter as built because `FBRSpotterLineRow` exists, when the subsystem that
# would read that row does not.
IMPL_PREFIX = re.compile(r"^(U|A|I)[A-Z]")
DATA_PREFIX = re.compile(r"^(F|E)[A-Z]")


@dataclass
class Index:
    symbols: dict           # symbol name -> repo-relative file
    files: list[str]
    tables: list[str]

    def find(self, key: str, kind: str = "impl") -> list[str]:
        """kind='impl' -> runtime classes and source files that implement it.
           kind='data' -> row structs and CSV tables that feed it."""
        hits = []
        for s, p in sorted(self.symbols.items()):
            if not matches(key, s):
                continue
            is_impl = bool(IMPL_PREFIX.match(s))
            is_data = bool(DATA_PREFIX.match(s))
            if (kind == "impl" and is_impl) or (kind == "data" and is_data):
                hits.append(f"{s} ({p})")
        if kind == "impl":
            hits += [p for p in self.files if matches(key, Path(p).stem)
                     and not any(f"({p})" in h for h in hits)]
        else:
            hits += [t for t in self.tables if matches(key, t)]
        return hits


def scan_source() -> Index:
    symbols, files = {}, []
    for path in sorted(SOURCE.rglob("*.h")) + sorted(SOURCE.rglob("*.cpp")):
        rel = str(path.relative_to(PROJECT))
        files.append(rel)
        body = path.read_text(encoding="utf-8", errors="ignore")
        for m in DECL_RE.finditer(body):
            name = m.group(1)
            if name and name not in symbols:
                symbols[name] = rel
    tables = sorted(p.name for p in DATA.glob("*.csv")) if DATA.is_dir() else []
    return Index(symbols=symbols, files=files, tables=tables)


# ===========================================================================
# 3. DETECT GAPS — what the GDD requires that the codebase does not contain
# ===========================================================================

def detect_gaps(features: list[Feature], index: Index) -> None:
    for f in features:
        for key in f.keys:
            impl = index.find(key, "impl")
            if impl:
                f.built = True
                f.evidence.append(f"{key} -> {impl[0]}")
        # Inputs are counted whether or not the feature is built: for a gap they
        # are the reason it can be started today, and printing them for a built
        # feature keeps the two columns comparable.
        for key in f.keys:
            f.inputs += [d for d in index.find(key, "data") if d not in f.inputs]
        if not f.built:
            why = f"no U/A/I class matches any of {f.keys}"
            if f.inputs:
                why += f" — but its data exists ({', '.join(f.inputs[:2])})"
            f.evidence.insert(0, why)
        f.integration = sum(len(index.find(k, "impl")) for k in f.keys)


# ===========================================================================
# 4. PRIORITISE — which missing feature to build first, and why
# ===========================================================================
#
# Four terms, all printed, no model involved. The weights are here in the open
# so the ranking can be argued with; that is the point of putting them in code
# rather than in a prompt.

W_CENTRAL, W_INPUTS, W_INTEGRATION, W_BREADTH = 1.0, 25.0, 2.0, 4.0


def rank(features: list[Feature]) -> list[Feature]:
    gaps = [f for f in features if not f.built]
    for f in gaps:
        terms = {
            # How much the design document leans on this feature.
            "central": round(f.gdd_mentions * W_CENTRAL, 1),
            # Can it be built NOW? A unit with no inputs on disk is a unit that
            # stalls at first contact — the exact way the previous attempt at
            # this assignment failed, so it is weighted hardest.
            "inputs_ready": round(len(f.inputs) * W_INPUTS, 1),
            # How much existing code it would plug into.
            "integration": round(f.integration * W_INTEGRATION, 1),
            # A feature described by several distinct nouns is a system; one
            # described by a single noun is usually an asset or a setting.
            "breadth": round(len(f.keys) * W_BREADTH, 1),
        }
        f.terms = terms
        f.score = round(sum(terms.values()), 1)
    gaps.sort(key=lambda f: -f.score)
    return gaps


# ===========================================================================
# 5. GENERATE CODE — write the top-ranked missing feature
# ===========================================================================

BUILD_PROMPT = """You are writing one new C++ unit for BREACHPOINT, a 4v4 arena FPS
in Unreal Engine 5.8. Return JSON only.

## THE FEATURE, from the game's own GDD §5.1 shipped scope
{feature}

## WHAT THE GDD SAYS ABOUT IT
{gdd_context}

## PROJECT LAWS — violations are defects, not style notes
- Pure native C++. No Blueprint classes.
- **No gameplay Tick.** Use timers, delegates and events.
- Server-authoritative: clients send intent, the server decides.
- Data is not code: tuning numbers live in `Content/Data/*.csv`, and asset
  references in C++ are SOFT (`TSoftObjectPtr`). No `ConstructorHelpers`.
- Class prefix `BR`. One runtime module, folder per discipline.

## STYLE EXEMPLAR — an existing subsystem from this same codebase
Match its conventions: include order, `UCLASS()` macros, logging, comment
density, naming. Do not import conventions from other Unreal projects.

### {exemplar_h_path}
```cpp
{exemplar_h}
```

### {exemplar_cpp_path}
```cpp
{exemplar_cpp}
```

## WHAT TO WRITE
Class `{class_name}`.

Choose the folder it belongs in from the ones this module already has —
`{folder_list}` — and return it as `folder`. This is the one choice the agent
delegates to you: WHICH feature to build was decided deterministically before
you were called, but where a new file sits is a convention question and the
codebase is the only thing that answers it.

{brief}

Keep it to the declaration surface plus a working, compilable implementation of
the parts that do not need the rest of the engine. Where a real integration
point does not exist yet, leave a clearly-named TODO rather than inventing an
API — an invented call site is worse than an honest gap.

## OUTPUT
{{"folder": "<one of the folders listed above>",
  "header": "<full contents of the .h>", "source": "<full contents of the .cpp>",
  "rationale": "<2-3 sentences: what you built and what you deliberately left out>"}}
"""


def call_model(prompt: str, live: bool, stage: str) -> dict:
    """Live call via the anthropic SDK or the `claude` CLI; else replay."""
    if not live:
        if not RECORDING.exists():
            sys.exit("error: no recording.json — run once with --live first")
        rec = json.loads(RECORDING.read_text(encoding="utf-8"))
        if rec.get("stage") != stage:
            sys.exit(f"error: recording is for stage {rec.get('stage')!r}, not {stage!r}")
        return rec["response"]

    text, usage = None, {}
    if os.environ.get("ANTHROPIC_API_KEY"):
        try:
            import anthropic
            resp = anthropic.Anthropic().messages.create(
                model=MODEL, max_tokens=16000,
                messages=[{"role": "user", "content": prompt}])
            text = "".join(b.text for b in resp.content if b.type == "text")
        except ImportError:
            pass
    if text is None:
        from shutil import which
        if not which("claude"):
            sys.exit("error: --live needs ANTHROPIC_API_KEY + `pip install anthropic`, "
                     "or the `claude` CLI on PATH")
        # A nested `claude` inherits the parent session's BUN_OPTIONS and dies on
        # a bare ENOENT. Strip it.
        env = {k: v for k, v in os.environ.items() if k != "BUN_OPTIONS"}
        proc = subprocess.run(["claude", "-p", prompt, "--model", MODEL,
                               "--output-format", "json"],
                              capture_output=True, text=True, timeout=1200, env=env)
        if proc.returncode != 0:
            sys.exit(f"error: claude CLI failed: {proc.stderr.strip()[:300]}")
        payload = json.loads(proc.stdout)
        text = (payload.get("result") or "").strip()
        u = payload.get("usage") or {}
        usage = {"input_tokens": u.get("input_tokens", 0),
                 "output_tokens": u.get("output_tokens", 0),
                 "cost_usd": payload.get("total_cost_usd")}

    data = extract_json(text)
    RECORDING.write_text(json.dumps(
        {"model": MODEL, "stage": stage, "prompt": prompt,
         "response": data, "usage": usage}, indent=2), encoding="utf-8")
    if usage:
        log(f"    tokens in {usage['input_tokens']:,} · out {usage['output_tokens']:,}"
            + (f" · ${usage['cost_usd']:.4f}" if usage.get("cost_usd") else ""))
    return data


def extract_json(text: str):
    text = text.strip()
    fence = re.search(r"```(?:json)?\s*(.+?)```", text, re.S)
    if fence:
        text = fence.group(1).strip()
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        start, end = text.find("{"), text.rfind("}")
        if start != -1 and end > start:
            return json.loads(text[start:end + 1])
    raise SystemExit(f"error: no JSON in model reply (first 200: {text[:200]!r})")


def gdd_context_for(feature: Feature, gdd_text: str, limit: int = 4) -> str:
    """The GDD sections that actually discuss this feature — the agent's own
    little retrieval step, so the generator writes from the design document
    rather than from what it remembers about shooters."""
    sections = re.split(r"\n(?=#{2,3} )", gdd_text)
    scored = []
    for s in sections:
        low = s.lower()
        hits = sum(low.count(k) for k in feature.keys)
        if hits:
            scored.append((hits, s))
    scored.sort(key=lambda p: -p[0])
    return "\n\n---\n\n".join(s.strip()[:2200] for _, s in scored[:limit])


def class_name(feature: Feature) -> str:
    """`UBR<Noun>Subsystem`, from the feature's leading distinctive noun."""
    noun = feature.keys[0].capitalize()
    return f"UBR{noun}Subsystem"


def folders(index: Index) -> list[str]:
    return sorted({m.group(1) for p in index.files
                   if (m := re.search(r"Source/Breachpoint/([^/]+)/", p))})


def paths_for(cls: str, folder: str) -> tuple[str, str]:
    base = f"Source/Breachpoint/{folder}/{cls[1:]}"     # drop the U prefix
    return f"{base}.h", f"{base}.cpp"


def pick_exemplar(index: Index) -> tuple[Path, Path]:
    pairs = [p for p in SOURCE.rglob("BR*Subsystem.cpp")
             if p.with_suffix(".h").exists()]
    if not pairs:
        sys.exit("error: no BR*Subsystem pair in the frozen project to use as an exemplar")
    # Smallest complete pair: enough to show the conventions, short enough to
    # leave the generator room to think.
    best = min(pairs, key=lambda p: p.stat().st_size + p.with_suffix(".h").stat().st_size)
    return best.with_suffix(".h"), best


BR_SYMBOL_RE = re.compile(r"\b([UAIFE]BR[A-Za-z_]\w*)\b")


def undefined_symbols(header: Path, source: Path, index: Index) -> list[str]:
    """Project types the generated code names that nothing in the codebase defines.

    This is the check that separates working code from plausible code. A model
    writing against an unfamiliar codebase invents a helper that "should" exist
    far more readily than it gets a brace wrong, and an invented type is a
    compile error the other structural checks sail straight past.
    """
    written = {header.name, source.name}
    known = set(index.symbols)
    body = header.read_text(encoding="utf-8") + source.read_text(encoding="utf-8")
    # Types the new files legitimately define themselves — including the ones
    # UE declares through macros. Delegates are the common case: a signature
    # name only ever appears as the first argument of DECLARE_*DELEGATE*, so a
    # checker that only reads `class`/`struct` reports the file's own delegate
    # as undefined. That was this check's first finding, and it was wrong.
    # `class BREACHPOINT_API UBRSpotterSubsystem` — the export macro sits between
    # the keyword and the name, so it must be skipped or the checker reports the
    # file's own class as undefined. (It did.)
    for m in re.finditer(r"(?:enum class|struct|class)\s+(?:\w+_API\s+)?(\w+)", body):
        known.add(m.group(1))
    for m in re.finditer(r"DECLARE_\w*DELEGATE\w*\s*\(\s*(\w+)", body):
        known.add(m.group(1))
    missing = []
    for name in sorted(set(BR_SYMBOL_RE.findall(body))):
        if name in known:
            continue
        # Fall back to a text search: a type may be defined without matching
        # DECL_RE (typedefs, macro-generated names).
        found = any(re.search(rf"\b{re.escape(name)}\b", (PROJECT / f).read_text(
            encoding="utf-8", errors="ignore"))
            for f in index.files if Path(f).name not in written)
        if not found:
            missing.append(name)
    return missing


def verify(header: Path, source: Path, cls: str) -> list[str]:
    """Deterministic checks on what the model wrote. Not a compile — say so."""
    problems = []
    h = header.read_text(encoding="utf-8")
    c = source.read_text(encoding="utf-8")
    if "#pragma once" not in h:
        problems.append("header has no #pragma once")
    if ".generated.h" not in h:
        problems.append("header does not include its .generated.h")
    if cls not in h:
        problems.append(f"header does not declare {cls}")
    if "UCLASS" not in h:
        problems.append("header has no UCLASS()")
    if header.stem not in c.split("\n")[0] and header.name not in c[:400]:
        problems.append("source does not include its own header first")
    for banned, why in ((r"\bvoid\s+Tick\s*\(", "law 4 — no gameplay Tick"),
                        (r"ConstructorHelpers", "law 3 — no hard asset refs in C++")):
        if re.search(banned, h + c):
            problems.append(why)
    return problems


# ===========================================================================
# Reporting
# ===========================================================================

def write_outputs(features, gaps, index, selected=None, build=None):
    OUT.mkdir(exist_ok=True)
    (OUT / "perception.json").write_text(json.dumps({
        "pinned_project": str(PROJECT.relative_to(HERE)),
        "files_scanned": len(index.files),
        "symbols_found": len(index.symbols),
        "data_tables": index.tables,
        "features": [asdict(f) for f in features],
    }, indent=2), encoding="utf-8")
    (OUT / "ranking.json").write_text(json.dumps({
        "weights": {"central": W_CENTRAL, "inputs_ready": W_INPUTS,
                    "integration": W_INTEGRATION, "breadth": W_BREADTH},
        "selected": selected.id if selected else None,
        "candidates": [{"id": f.id, "text": f.text, "score": f.score,
                        "terms": f.terms, "inputs": f.inputs,
                        "evidence": f.evidence} for f in gaps],
    }, indent=2), encoding="utf-8")
    if build:
        (OUT / "build_report.json").write_text(json.dumps(build, indent=2),
                                               encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--scan", action="store_true", help="perception only")
    ap.add_argument("--rank", action="store_true", help="stop after the ranked table")
    ap.add_argument("--live", action="store_true", help="real model call for stage 5")
    args = ap.parse_args()

    if not GDD.exists():
        sys.exit(f"error: no frozen project at {PROJECT} — run ./freeze_project.sh")

    # ---- 1 ----------------------------------------------------------------
    features, gdd_text = read_gdd_features()
    log(f"1. GDD  — {GDD.relative_to(HERE)} §5.1 Shipped Scope")
    log(f"   {len(features)} features the game is supposed to have")

    # ---- 2 ----------------------------------------------------------------
    index = scan_source()
    log(f"\n2. SCAN — {len(index.files)} source files, {len(index.symbols)} declarations, "
        f"{len(index.tables)} data tables")

    # ---- 3 ----------------------------------------------------------------
    detect_gaps(features, index)
    built = [f for f in features if f.built]
    gaps_all = [f for f in features if not f.built]
    log(f"\n3. GAPS — {len(built)} built · {len(gaps_all)} missing")
    for f in features:
        mark = "built  " if f.built else "MISSING"
        log(f"   {mark} {f.text[:52]:52} {f.evidence[0][:60]}")
    if args.scan:
        write_outputs(features, rank(features), index)
        log(f"\nwrote {OUT.relative_to(HERE)}/perception.json")
        return

    # ---- 4 ----------------------------------------------------------------
    gaps = rank(features)
    log(f"\n4. RANK — every term printed; no model call reached this point")
    log(f"   {'score':>7}  {'central':>7} {'inputs':>7} {'integ':>6} {'breadth':>7}  feature")
    for f in gaps:
        t = f.terms
        log(f"   {f.score:7.1f}  {t['central']:7.1f} {t['inputs_ready']:7.1f} "
            f"{t['integration']:6.1f} {t['breadth']:7.1f}  {f.text[:44]}")
    if not gaps:
        log("\nno gaps — nothing to build")
        return
    selected = gaps[0]
    runner_up = gaps[1] if len(gaps) > 1 else None
    log(f"\n   SELECTED: {selected.text}")
    log(f"   because  : score {selected.score}"
        + (f" vs {runner_up.score} for the runner-up ({runner_up.text[:34]})"
           if runner_up else ""))
    if selected.inputs:
        log(f"   inputs already on disk: {', '.join(selected.inputs)}")
    if args.rank:
        write_outputs(features, gaps, index, selected)
        log(f"\nwrote {OUT.relative_to(HERE)}/ranking.json")
        return

    # ---- 5 ----------------------------------------------------------------
    cls = class_name(selected)
    choices = folders(index)
    ex_h, ex_cpp = pick_exemplar(index)
    log(f"\n5. BUILD — {cls}")
    log(f"   exemplar: {ex_cpp.relative_to(PROJECT)}")

    prompt = BUILD_PROMPT.format(
        feature=selected.text,
        gdd_context=gdd_context_for(selected, gdd_text),
        exemplar_h_path=ex_h.relative_to(PROJECT),
        exemplar_h=ex_h.read_text(encoding="utf-8")[:6000],
        exemplar_cpp_path=ex_cpp.relative_to(PROJECT),
        exemplar_cpp=ex_cpp.read_text(encoding="utf-8")[:6000],
        class_name=cls, folder_list=" · ".join(choices),
        brief=f"The GDD context above is the specification. Inputs already on disk: "
              f"{', '.join(selected.inputs) or 'none'}.")
    result = call_model(prompt, args.live, stage="build")

    folder = result.get("folder", "")
    if folder not in choices:
        sys.exit(f"error: model chose folder {folder!r}, which is not one of {choices}")
    header_rel, source_rel = paths_for(cls, folder)
    log(f"   -> {header_rel}")
    log(f"   -> {source_rel}")

    hpath = guard_path(PROJECT / header_rel)
    cpath = guard_path(PROJECT / source_rel)
    hpath.parent.mkdir(parents=True, exist_ok=True)
    hpath.write_text(result["header"], encoding="utf-8")
    cpath.write_text(result["source"], encoding="utf-8")
    log(f"   wrote {len(result['header']):,} + {len(result['source']):,} chars")

    problems = verify(hpath, cpath, cls)
    invented = undefined_symbols(hpath, cpath, index)
    log(f"\n6. VERIFY — structural checks (NOT a compile; no engine here)")
    if problems:
        for p in problems:
            log(f"   FAIL {p}")
    else:
        log(f"   pass: pragma once · generated.h · UCLASS · {cls} declared · "
            f"self-include · no Tick · no ConstructorHelpers")
    refs = sorted(set(BR_SYMBOL_RE.findall(
        hpath.read_text(encoding='utf-8') + cpath.read_text(encoding='utf-8'))))
    if invented:
        problems += [f"references undefined project type {n}" for n in invented]
        for n in invented:
            log(f"   FAIL references {n}, which nothing in the codebase defines")
    else:
        log(f"   pass: all {len(refs)} project types it references are real "
            f"({', '.join(refs[:4])}{'…' if len(refs) > 4 else ''})")

    # Prove the loop closed: re-scan and confirm the gap is gone.
    after = scan_source()
    closed = bool(after.find(selected.keys[0]))
    log(f"\n7. RE-SCAN — gap '{selected.text[:40]}' now "
        + ("CLOSED" if closed else "STILL OPEN"))

    write_outputs(features, gaps, index, selected, build={
        "selected": selected.id, "class": cls,
        "header": header_rel, "source": source_rel,
        "rationale": result.get("rationale", ""),
        "verify_problems": problems, "gap_closed_on_rescan": closed,
    })
    log(f"\nwrote {OUT.relative_to(HERE)}/ — perception · ranking · build_report")


if __name__ == "__main__":
    main()
