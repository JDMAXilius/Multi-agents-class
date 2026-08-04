#!/usr/bin/env python3
"""Export Figma art to Content/UI/Icons over the Figma REST API. No MCP, no relay.

    # one-time: put a personal access token in Tools/env.local
    #   FIGMA_TOKEN=figd_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    # (Figma → Settings → Security → Personal access tokens. Read-only scope is enough.)

    python3 mcp-ui/gen_ui/figma_export.py --list-pages
    python3 mcp-ui/gen_ui/figma_export.py --audit 48:2
    python3 mcp-ui/gen_ui/figma_export.py --export 80:2 --family Glyphs --scale 4
    python3 mcp-ui/gen_ui/figma_export.py --export-all

WHY THIS EXISTS
---------------
The Figma MCP works but cannot be delegated: its tools are session-scoped, so a subagent
sees "No such tool available" (verified — two crew lanes failed exactly that way). That
forces every export through one conversation, relaying base64 by hand, which is expensive
and silently corruptible — one transcription slipped a character and produced an invalid
PNG. This script removes the relay entirely: bytes go Figma → disk without passing through
a model, and because it is plain HTTP, the crew can run it in parallel.

WHAT IT REFUSES TO DO
---------------------
Nothing lands in Content/UI/Icons until it passes `preflight_textures.py`. Failures go to
mcp-ui/gen_ui/quarantine/ with the reason. The first export pass produced 41 files with
baked-in backgrounds that looked perfect in every viewer and were wrong only in engine —
the validator is the whole point, not a formality.

RENDERING CAVEAT, STATED BECAUSE IT BIT US ONCE
-----------------------------------------------
The MCP `download_assets` tool composites a node against its page backdrop. The REST
`/v1/images` endpoint is expected to render in isolation like the Plugin API's
`exportAsync` does — but "expected" is not "verified", and it is the exact failure this
project keeps a validator for. So every single output is checked, every run. If the REST
endpoint turns out to composite as well, the validator catches it on file #1 and nothing
reaches the project.
"""
from __future__ import annotations

import argparse, json, os, re, sys, time, urllib.error, urllib.request
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ICONS = REPO / "Content/UI/Icons"
QUARANTINE = Path(__file__).resolve().parent / "quarantine"
RECEIPTS = REPO / "docs/ui/receipts"

API = "https://api.figma.com/v1"
FILE_KEY = "yznvnVdOFDADaugZSeomfP"          # BREACHPOINT — UI/UX System
IMAGE_BATCH = 20                              # /v1/images caps the id list; 20 is comfortable

# Figma page id -> (destination family folder, default export scale).
# Scale is per-family on purpose: a 24px glyph wants 4x for 4K, a 120px difficulty mark at
# 4x would be 480x480, which is far more texture than a UI element needs.
PAGES = {
    "48:2": ("Ranks",      4),
    "68:2": ("Icons",      4),
    "80:2": ("Glyphs",     4),
}


# --------------------------------------------------------------------------- auth
def token() -> str:
    t = os.environ.get("FIGMA_TOKEN", "").strip()
    if t:
        return t
    envf = REPO / "Tools/env.local"
    if envf.is_file():
        for line in envf.read_text().splitlines():
            if line.strip().startswith("FIGMA_TOKEN="):
                return line.split("=", 1)[1].strip().strip('"').strip()
    sys.exit(
        "BLOCKED — no Figma token.\n"
        "  Add to Tools/env.local:   FIGMA_TOKEN=figd_...\n"
        "  Or export FIGMA_TOKEN in the environment.\n"
        "  Create one at Figma → Settings → Security → Personal access tokens.\n"
        "  Read-only scope is sufficient; this script never writes to Figma."
    )


def get(url: str, tries: int = 4) -> dict:
    """GET with backoff. Figma rate-limits, and a 429 mid-batch must not lose the run."""
    req = urllib.request.Request(url, headers={"X-Figma-Token": token()})
    for attempt in range(tries):
        try:
            with urllib.request.urlopen(req, timeout=120) as r:
                return json.loads(r.read().decode())
        except urllib.error.HTTPError as e:
            if e.code in (429, 500, 502, 503) and attempt < tries - 1:
                wait = 2 ** attempt * 3
                print(f"    HTTP {e.code}, retrying in {wait}s")
                time.sleep(wait)
                continue
            body = e.read().decode()[:300]
            sys.exit(f"BLOCKED — Figma API {e.code}: {body}")
    raise RuntimeError("unreachable")


# --------------------------------------------------------------------------- naming
def asset_name(component_name: str, family: str) -> str:
    """'Icon / Mode / Capture the Flag / 40' -> 'T_UI_Icon_Mode_CaptureTheFlag_40'.

    The prefix convention is ASSET-PIPELINE.md §3: T_UI_<Family>_<Name>.
    """
    parts = [p.strip() for p in component_name.split("/") if p.strip()]
    cleaned = []
    for p in parts:
        # collapse multi-word segments: "Capture the Flag" -> "CaptureTheFlag"
        words = re.sub(r"[^A-Za-z0-9 ]", "", p).split()
        cleaned.append("".join(w[:1].upper() + w[1:] for w in words) if len(words) > 1
                       else (words[0] if words else ""))
    stem = "_".join(c for c in cleaned if c)
    return f"T_UI_{stem}" if not stem.startswith("T_UI_") else stem


# --------------------------------------------------------------------------- commands
def list_pages() -> int:
    doc = get(f"{API}/files/{FILE_KEY}?depth=1")["document"]
    print(f"{doc['name']} — {len(doc['children'])} pages\n")
    for p in doc["children"]:
        mark = "  <- configured" if p["id"] in PAGES else ""
        print(f"  {p['id']:<12} {p['name']}{mark}")
    return 0


def _components(page_id: str) -> list[dict]:
    node = get(f"{API}/files/{FILE_KEY}/nodes?ids={page_id}&depth=2")
    doc = node["nodes"][page_id]["document"]
    return [c for c in doc.get("children", []) if c.get("type") == "COMPONENT"]


def audit(page_id: str) -> int:
    """Detect the BP68 defect: a child pinned to (0,0) instead of composed in its frame."""
    node = get(f"{API}/files/{FILE_KEY}/nodes?ids={page_id}&depth=3")
    doc = node["nodes"][page_id]["document"]
    print(f"Auditing '{doc['name']}' ({page_id})\n")
    pinned, ok, childless = [], [], []
    for c in doc.get("children", []):
        if c.get("type") != "COMPONENT":
            continue
        kids = c.get("children") or []
        if not kids:
            childless.append(c["name"])
            continue
        fb, kb = c.get("absoluteBoundingBox"), kids[0].get("absoluteBoundingBox")
        if not fb or not kb:
            continue
        rel_x, rel_y = round(kb["x"] - fb["x"], 2), round(kb["y"] - fb["y"], 2)
        cx = round((fb["width"] - kb["width"]) / 2, 2)
        cy = round((fb["height"] - kb["height"]) / 2, 2)
        if rel_x == 0 and rel_y == 0 and (cx > 0.5 or cy > 0.5):
            pinned.append((c["name"], f"child at (0,0), should be ~({cx},{cy})"))
        else:
            ok.append(c["name"])
    for n, why in pinned:
        print(f"  PINNED  {n}  — {why}")
    for n in childless:
        print(f"  (no children) {n}")
    print(f"\n{len(ok)} composed, {len(pinned)} pinned to top-left, {len(childless)} childless")
    if pinned:
        print("\nThese will export with the mark in a corner and ink on the frame edge.")
        print("Known: rank insignia land misaligned; crop before import.")
        return 1
    print("Clean — every component is composed inside its frame.")
    return 0


def export(page_id: str, family: str | None, scale: int | None, rc) -> tuple[int, int]:
    fam, default_scale = PAGES.get(page_id, (family or "Misc", 4))
    fam, scale = (family or fam), (scale or default_scale)
    comps = _components(page_id)
    if not comps:
        rc(f"- **{page_id}**: no COMPONENT nodes found")
        return 0, 0
    rc(f"\n## page `{page_id}` -> `Content/UI/Icons/{fam}/` @ {scale}x — {len(comps)} components\n")

    dest, quar = ICONS / fam, QUARANTINE / fam
    dest.mkdir(parents=True, exist_ok=True)

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from preflight_textures import preflight            # noqa: E402

    passed = failed = 0
    for i in range(0, len(comps), IMAGE_BATCH):
        chunk = comps[i:i + IMAGE_BATCH]
        ids = ",".join(c["id"] for c in chunk)
        res = get(f"{API}/images/{FILE_KEY}?ids={ids}&format=png&scale={scale}")
        if res.get("err"):
            rc(f"- **FAILED** batch {i//IMAGE_BATCH}: {res['err']}")
            failed += len(chunk)
            continue
        for c in chunk:
            url = (res.get("images") or {}).get(c["id"])
            name = asset_name(c["name"], fam)
            if not url:
                rc(f"- **FAILED** `{name}` — Figma returned no image URL")
                failed += 1
                continue
            tmp = dest / f"{name}.png"
            with urllib.request.urlopen(url, timeout=120) as r, open(tmp, "wb") as f:
                f.write(r.read())

            box = c.get("absoluteBoundingBox") or {}
            expect = ({"w": box["width"], "h": box["height"], "scale": scale}
                      if box.get("width") else None)
            ok, fails, summary = preflight(tmp, expect)
            if ok:
                rc(f"- PASS `{name}` — {summary}")
                passed += 1
            else:
                quar.mkdir(parents=True, exist_ok=True)
                tmp.rename(quar / tmp.name)
                rc(f"- **FAILED** `{name}` — quarantined")
                for f_ in fails:
                    rc(f"    - {f_}")
                failed += 1
    return passed, failed


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--list-pages", action="store_true")
    ap.add_argument("--audit", metavar="PAGE_ID")
    ap.add_argument("--export", metavar="PAGE_ID")
    ap.add_argument("--export-all", action="store_true")
    ap.add_argument("--family")
    ap.add_argument("--scale", type=int)
    a = ap.parse_args()

    if a.list_pages:
        return list_pages()
    if a.audit:
        return audit(a.audit)
    if not (a.export or a.export_all):
        ap.print_help()
        return 2

    RECEIPTS.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    rf = (RECEIPTS / f"figma-export-{stamp}.md").open("w", buffering=1)   # flushed per line

    def rc(s=""):
        print(s)
        rf.write(s + "\n")

    rc(f"# RECEIPT — Figma REST export · {datetime.now(timezone.utc).isoformat()}")
    rc(f"\n**Command:** `{' '.join(sys.argv)}`")
    rc(f"**File:** `{FILE_KEY}` · **Endpoint:** `{API}/images` (REST, not MCP)")
    rc("**Gate:** every file runs `preflight_textures.py`; failures are quarantined, "
       "never written into `Content/UI/Icons/`.\n")

    targets = list(PAGES) if a.export_all else [a.export]
    tp = tf = 0
    for pid in targets:
        p, f = export(pid, a.family, a.scale, rc)
        tp, tf = tp + p, tf + f

    rc(f"\n## Verdict\n{tp} exported and verified, {tf} quarantined.")
    rc("\n## Rung honesty\n"
       "- **Not a rung.** A validated PNG on disk is not a rendered widget. This proves the "
       "file is transparent, correctly sized, unclipped and neutral — not that it looks "
       "right in a screen.\n"
       "- Import into UE is a separate step: `mcp-ui/gen_ui/import_textures.py`, which sets "
       "the four texture settings and reads each one back.")
    rf.close()
    print(f"\nreceipt: docs/ui/receipts/figma-export-{stamp}.md")
    return 0 if tf == 0 else 1


if __name__ == "__main__":
    sys.exit(main())


# --------------------------------------------------------------------------- self-test
def _selftest() -> int:
    """python3 -c "import figma_export as f; f._selftest()"  — no token, no network."""
    cases = [
        ("Icon / Mode / Capture the Flag / 40", "Icons", "T_UI_Icon_Mode_CaptureTheFlag_40"),
        ("Icon / Mode / Slayer / 16",           "Icons", "T_UI_Icon_Mode_Slayer_16"),
        ("Glyph / Save & Quit / 24",            "Glyphs", "T_UI_Glyph_SaveQuit_24"),
        ("Rank / 01",                           "Ranks", "T_UI_Rank_01"),
        ("PARAM Rank / 16 Vanguard",            "Ranks", "T_UI_PARAMRank_16Vanguard"),
        ("Icon / Currency / Credit / 24x40",    "Icons", "T_UI_Icon_Currency_Credit_24x40"),
    ]
    bad = []
    for raw, fam, want in cases:
        got = asset_name(raw, fam)
        print(f"  {'ok  ' if got == want else 'FAIL'} {raw!r} -> {got}")
        if got != want:
            bad.append((raw, got, want))
    if bad:
        for raw, got, want in bad:
            print(f"    {raw!r}: got {got!r}, want {want!r}")
        return 1
    print("naming self-test passed")
    return 0
