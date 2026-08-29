"""EVERY INPUT TAG, AND WHETHER ANY KEY PRESS CAN REACH IT.

    python Tools/bn/81_input_audit.py       # files only; no editor needed

WHY THIS EXISTS. The dash shipped with its key bound, its InputAction asset built, its
Input.Dash pairing in DA_BNInput and its ability granted — and pressing 2 did nothing.
ABNPlayerController binds tags to handlers from an EXPLICIT LIST, so a tag nobody adds to
that list reaches no handler and the press dies between Enhanced Input and the ASC.

Nothing logs it. From the input system's side the key worked perfectly: the action fired,
the trigger fired, and then the value went nowhere. The only visible symptom is a player
saying "this key does nothing", which is exactly the report that arrived.

The audit is a set difference between two files, so it needs no editor and no build.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
GEN = REPO / "Tools/bn/10_input_assets.py"
CTRL = REPO / "Source/BreachpointNext/Match/BNPlayerController.cpp"


def main() -> int:
    gen = GEN.read_text(encoding="utf-8")
    ctrl = CTRL.read_text(encoding="utf-8")

    # The generator's ACTIONS table is the set of tags a key can produce.
    tags = sorted(set(re.findall(r'"tag":\s*"Input\.([\w.]+)"', gen)))
    # The controller's Bind() calls are the set a press can actually reach.
    bound = set(re.findall(r"Bind\(BNTags::(\w+),", ctrl))

    def as_symbol(tag: str) -> str:
        return "Input_" + tag.replace(".", "_")

    dead = [t for t in tags if as_symbol(t) not in bound]

    print(f"{len(tags)} input tags produced by keys · {len(bound)} tags bound to handlers\n")
    for t in tags:
        print(f"  {'OK  ' if as_symbol(t) in bound else 'DEAD'} Input.{t}")

    if dead:
        print(f"\n{len(dead)} TAG(S) NO PRESS CAN REACH:")
        for t in dead:
            print(f"  - Input.{t}: a key produces it and ABNPlayerController binds nothing to it.")
            print(f"    Add: Bind(BNTags::{as_symbol(t)}, ETriggerEvent::Started, &ABNPlayerController::Handle...);")
        return 1

    print("\nNo dead keys — every tag a key can produce reaches a handler.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
