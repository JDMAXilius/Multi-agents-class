#!/usr/bin/env python3
"""PreToolUse guard: the crew's laws enforced as mechanism, not trust.

Blocks, BEFORE the tool executes (exit 2 = block, stderr shown to the agent):
  1. Edit/Write outside the claimed packet's owner_path (law 5) — active only
     when a claim exists at .claude/active-packet.json (the tickets skill
     writes it on pickup). No claim = planning session = no confinement.
  2. Banned engine APIs entering Source/ (laws 2-3): the engine damage API,
     hard constructor refs, and unseeded randomness in gameplay code.

Always allowed regardless of claim: ticket Logs (docs/tickets/), the claim
file itself, and anything outside the game repo's Source//Content/ trees.
"""
import json
import re
import sys
from pathlib import Path

BANNED = [
    (r"\bTakeDamage\s*\(|\bApplyRadialDamage|\bApplyPointDamage|\bApplyDamage\s*\(",
     "engine damage API is banned — ONE pipeline: GE_Damage + BRDamageExecCalc (gas-purity.md)"),
    (r"\bConstructorHelpers\s*::",
     "hard asset refs in C++ are banned — soft refs via DataTable rows (data-and-assets.md)"),
    (r"\bFMath::RandRange\s*\(|\bFMath::FRand\s*\(",
     "unseeded randomness in gameplay code — randomness enters as a seeded stream passed in"),
]
ALWAYS_ALLOWED = ("docs/tickets/", ".claude/active-packet.json", "docs/DESIGN-RULINGS.md")


def main():
    data = json.load(sys.stdin)
    tool_input = data.get("tool_input", {})
    path = tool_input.get("file_path") or ""
    if not path:
        return 0
    project = Path(data.get("cwd", "."))
    try:
        # as_posix(): on Windows str(PurePath) yields backslashes, which silently
        # defeats every "Source/" prefix test below and makes this guard inert.
        rel = Path(path).resolve().relative_to(project.resolve()).as_posix()
    except ValueError:
        return 0  # outside the project — not this guard's jurisdiction

    if any(rel.startswith(a) or rel.endswith(a) for a in ALWAYS_ALLOWED):
        return 0

    # Law 2/3: banned APIs, checked on the text being written into Source/
    if rel.startswith("Source/") and rel.endswith((".cpp", ".h", ".inl")):
        text = tool_input.get("content") or tool_input.get("new_string") or ""
        for pattern, why in BANNED:
            if re.search(pattern, text):
                print(f"BLOCKED by guard_laws: {why}", file=sys.stderr)
                return 2

    # Law 5: owner-path confinement, active only under a claim
    claim_file = project / ".claude" / "active-packet.json"
    if claim_file.exists():
        claim = json.loads(claim_file.read_text())
        owners = claim.get("owner_path", [])
        if owners and rel.startswith(("Source/", "Content/")):
            if not any(rel.startswith(o.rstrip("/") + "/") or rel == o for o in owners):
                print(
                    f"BLOCKED by guard_laws: '{rel}' is outside this packet's owner_path "
                    f"{owners} (ticket {claim.get('ticket', '?')}). Law 5: never 'just a "
                    f"tiny fix' — file a contract_gap in the ticket and stop.",
                    file=sys.stderr)
                return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
