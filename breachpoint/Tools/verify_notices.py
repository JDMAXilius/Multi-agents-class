#!/usr/bin/env python3
"""Verify the third-party notices obligation is actually discharged.

The Lucide icon set is ISC. ISC requires its copyright notice and permission
notice to appear in ALL COPIES — so the obligation is satisfied by the notice
SHIPPING, not by it existing in the repository. Four things must hold:

  1. THIRD-PARTY-NOTICES.md exists and carries every required token.
  2. Content/Legal/THIRD-PARTY-NOTICES.txt exists and matches it (the staged copy
     is what actually reaches a player; a stale copy is a silent violation).
  3. Config/DefaultGame.ini stages Content/Legal, or nothing reaches the build.
  4. Every icon set present in the tree has a notice (catches a dependency added
     without one).
  5. Content/Reference (internal dev/test assets — extracted game content, competitor
     art) is excluded from cook AND from git. Development may use anything it likes in
     there; what must never happen is it SHIPPING or being PUSHED. A hard reference from
     cooked content defeats DirectoriesToNeverCook, so that is checked too.

Exit 0 = clean. Exit 1 = the game may not ship. Run from the game repo root, in
rung 2 alongside the other grep gates (docs/contracts/testing.md).

No third-party imports: this must run anywhere, including a cloud container.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CANONICAL = ROOT / "THIRD-PARTY-NOTICES.md"
STAGED = ROOT / "Content" / "Legal" / "THIRD-PARTY-NOTICES.txt"
PACKAGING_INI = ROOT / "Config" / "DefaultGame.ini"

# Tokens that must appear verbatim. Each one is a licence obligation, not a
# stylistic preference — losing any of them is what makes a build unshippable.
REQUIRED_TOKENS: list[tuple[str, str]] = [
    ("Lucide", "the icon set is used and ISC requires attribution"),
    ("Cole Bemis", "ISC notice names the Feather copyright holder"),
    ("Lucide Contributors 2022", "ISC notice names the current holders"),
    ("ISC License", "the licence must be named"),
    ("Permission to use, copy, modify", "ISC permission notice must appear in all copies"),
    ("THE SOFTWARE IS PROVIDED \"AS IS\"", "ISC warranty disclaimer must appear in all copies"),
    ("Epic Games", "UE EULA attribution"),
]

# A dependency present in the tree with no notice is the failure mode this catches.
# glob -> token that must be in the notices file if the glob matches anything.
DEPENDENCY_PROBES: list[tuple[str, str, str]] = [
    ("Content/UI/Icons/**/*.uasset", "Lucide", "icons are in Content/ but no Lucide notice"),
]

REFERENCE_DIR = ROOT / "Content" / "Reference"
GITIGNORE = ROOT / ".gitignore"


def check_reference_area() -> int:
    """Content/Reference may hold anything; it may never ship or be committed."""
    problems = 0

    ini = PACKAGING_INI.read_text(encoding="utf-8") if PACKAGING_INI.exists() else ""
    if not re.search(r'DirectoriesToNeverCook=\(Path="Content/Reference"\)', ini):
        fail(
            "Content/Reference is not in DirectoriesToNeverCook. Internal reference assets "
            "could reach a packaged build."
        )
        problems += 1

    gi = GITIGNORE.read_text(encoding="utf-8") if GITIGNORE.exists() else ""
    if "Content/Reference/*" not in gi:
        fail(
            ".gitignore does not exclude Content/Reference/*. Local use is private; pushing "
            "to a remote is DISTRIBUTION, which is the larger exposure."
        )
        problems += 1

    # A hard reference from cooked content drags the asset in regardless of never-cook.
    # .uasset stores referenced package paths as readable ASCII, so a byte scan finds them.
    if REFERENCE_DIR.is_dir():
        for asset in ROOT.glob("Content/**/*.uasset"):
            if REFERENCE_DIR in asset.parents:
                continue
            try:
                blob = asset.read_bytes()
            except OSError:
                continue
            if b"/Game/Reference/" in blob:
                fail(
                    f"{asset.relative_to(ROOT)} references /Game/Reference/. A HARD reference "
                    f"from shipping content defeats DirectoriesToNeverCook and the asset cooks."
                )
                problems += 1

    return problems


def fail(msg: str) -> None:
    print(f"FAIL: {msg}", file=sys.stderr)


def main() -> int:
    problems = 0

    # 1 — canonical file present and complete
    if not CANONICAL.exists():
        fail(f"{CANONICAL.relative_to(ROOT)} does not exist. The ISC notice must be recorded.")
        return 1
    canonical_text = CANONICAL.read_text(encoding="utf-8")

    for token, why in REQUIRED_TOKENS:
        if token not in canonical_text:
            fail(f"{CANONICAL.name} is missing {token!r} — {why}")
            problems += 1

    # 2 — staged copy present and identical in substance
    if not STAGED.exists():
        fail(
            f"{STAGED.relative_to(ROOT)} does not exist. The repo record is NOT the "
            f"obligation; the staged copy is what ships to a player."
        )
        problems += 1
    else:
        staged_text = STAGED.read_text(encoding="utf-8")
        for token, why in REQUIRED_TOKENS:
            if token not in staged_text:
                fail(f"staged copy is missing {token!r} — {why}")
                problems += 1
        if staged_text.strip() != canonical_text.strip():
            fail(
                "staged copy has DRIFTED from THIRD-PARTY-NOTICES.md. The file players "
                "receive no longer matches the record. Re-copy it."
            )
            problems += 1

    # 3 — packaging actually stages the directory
    if not PACKAGING_INI.exists():
        fail(f"{PACKAGING_INI.relative_to(ROOT)} does not exist")
        problems += 1
    else:
        ini = PACKAGING_INI.read_text(encoding="utf-8")
        if not re.search(r'DirectoriesToAlwaysStage(?:AsUFS|AsNonUFS)=\(Path="Content/Legal"\)', ini):
            fail(
                "Config/DefaultGame.ini does not stage Content/Legal. The notice exists "
                "but WILL NOT SHIP, which is the violation this check is for."
            )
            problems += 1

    # 4 — a dependency in the tree with no notice
    for pattern, token, why in DEPENDENCY_PROBES:
        if any(ROOT.glob(pattern)) and token not in canonical_text:
            fail(f"{why} (matched {pattern})")
            problems += 1

    problems += check_reference_area()

    if problems:
        print(
            f"\n{problems} problem(s). The build must not ship until these are clear.",
            file=sys.stderr,
        )
        return 1

    print("notices: OK — recorded, staged, packaged, matching; Content/Reference walled off.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
