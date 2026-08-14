"""crew.py — load BREACHPOINT's real agent definitions.

Assignment #3's pipeline loaded `crew/.claude/agents/*.md` directly rather than
copying them, on the grounds that a copied agent definition is a fork that
silently drifts from the one the project actually runs. Assignment #4 does the
same thing, and it matters more here: the crew already contains the exact agent
for this job.

`breachpoint/.claude/agents/curators/spotter.md` is the project's authored
owner of "every line of flavor text the game speaks", and `CREW_MAP.md` routes
`DT_SpotterLines` + medals to it. Its doctrine carries constraints that no
generic "write announcer lines" prompt would produce:

  - generate a POOL (~10 per slot); one option is not a choice
  - hard character caps: 48 for an event line, 140 for a coach line
  - a line may only name a place the arena manifest named
  - no lore, no fiction, no characters — inventing narrative is a finding
  - fallback lines must stand alone (no live score, no player name)
  - coach lines are M4-gated: without telemetry they are invented advice

Reading it means the pipeline inherits all of that instead of me re-deriving a
worse version of it in a prompt string.
"""

from __future__ import annotations

import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]

# Where the real definitions live, then the fallback a zip carries.
SEARCH_ROOTS = [
    REPO / "breachpoint" / ".claude" / "agents",
    HERE / "agents",
]

FRONTMATTER_RE = re.compile(r"\A---\s*\n(.*?)\n---\s*\n", re.S)


def agents_dir() -> Path:
    for root in SEARCH_ROOTS:
        if root.is_dir():
            return root
    raise SystemExit(
        "error: crew agent definitions not found (looked in "
        + " and ".join(str(r) for r in SEARCH_ROOTS) + ")")


def find_agent(name: str) -> Path:
    """Agents live either at the root or under a discipline subfolder."""
    root = agents_dir()
    for candidate in (root / f"{name}.md", *root.glob(f"*/{name}.md")):
        if candidate.exists():
            return candidate
    raise SystemExit(f"error: agent definition '{name}.md' not found under {root}")


def load_agent(name: str) -> tuple[str, dict]:
    """Return (body, frontmatter). The body is the doctrine the prompt carries."""
    path = find_agent(name)
    raw = path.read_text(encoding="utf-8")
    meta: dict = {}
    m = FRONTMATTER_RE.match(raw)
    if m:
        for line in m.group(1).splitlines():
            if ":" in line:
                key, _, value = line.partition(":")
                meta[key.strip()] = value.strip()
        raw = raw[m.end():]
    return raw.strip(), meta


def provenance() -> str:
    """One line naming where the definitions came from, for the run log."""
    root = agents_dir()
    where = "the live crew" if root == SEARCH_ROOTS[0] else "the bundled copy (zip layout)"
    return f"{root} — {where}"


# ---------------------------------------------------------------------------
# Constraints lifted OUT of spotter.md rather than re-invented here.
# ---------------------------------------------------------------------------
#
# These are parsed from the definition at import time so that editing
# spotter.md changes the pipeline's gates. If the parse ever fails the pipeline
# stops rather than falling back to a guess — a silently-defaulted limit is how
# a gate stops being a gate.

def spotter_char_caps() -> dict[str, int]:
    body, _ = load_agent("spotter")
    event = re.search(r"event line[^.]*?(\d+)\s*characters", body)
    coach = re.search(r"coach line\s*(?:is\s*)?[<≤]=?\s*(\d+)", body)
    if not (event and coach):
        raise SystemExit(
            "error: could not read the character caps from spotter.md — the "
            "definition changed shape. Fix the parse rather than hard-coding a "
            "limit; a gate that guesses its own threshold is not a gate.")
    return {"event": int(event.group(1)), "coach": int(coach.group(1))}


if __name__ == "__main__":
    print(provenance())
    for name in ("spotter", "critic"):
        body, meta = load_agent(name)
        print(f"\n{name}.md — {len(body):,} chars · tools: {meta.get('tools', '?')}")
        print(f"  {meta.get('description', '')[:150]}")
    print(f"\ncharacter caps parsed from spotter.md: {spotter_char_caps()}")
