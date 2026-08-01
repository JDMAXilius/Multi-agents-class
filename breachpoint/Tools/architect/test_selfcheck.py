#!/usr/bin/env python3
"""Red-then-green for architect.py's step-1 self-check.

BP15's Done-when says the scanner "exits nonzero on any mismatch." A self-check that has only
ever been observed PASSING is not evidence -- it is the same false comfort that let a two-folder
guard_laws hook, a self-gating BP14 Kickoff, and a Linux-only BP14 box all read as enforced.
**An enforcement mechanism proves nothing until it is tested with a case it should REJECT.**

So this drives the real parser over synthetic manifests: one faithful, four corrupted. Each
corruption is a defect that has actually occurred on this project or is one edit away.

    python3 test_selfcheck.py        -> prints a table; exit 0 if all cases behave
"""

import re
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
ARCH = REPO / "BREACHPOINT-ARCHITECTURE.md"

# Each case: (name, transform, must_exit_nonzero, why it matters)
CASES = [
    ("faithful copy", lambda t: t, False,
     "the control -- an unmodified manifest must still PASS, or every red below is meaningless"),
    ("header/table disagree", lambda t: t.replace("### 3.9 `UI/` — 4", "### 3.9 `UI/` — 7"), True,
     "§3.9's header claims 7 while its table declares 4 -- exactly the stale-tree defect that "
     "printed `AI/ (4)` against six units on 31 Jul"),
    ("a unit silently dropped",
     lambda t: t.replace("`BRShieldSpec.cpp`", "BRShieldSpec.cpp-removed"), True,
     "Tests/ header says 3, table now declares 2 -- a unit deleted from the doc without the "
     "count following it"),
    ("budget no longer reaches 44",
     lambda t: t.replace("### 3.1 `Core/` — 2", "### 3.1 `Core/` — 3")
                .replace("`BRCore.h/.cpp`", "`BRCore.h/.cpp` and `BRExtra.h/.cpp`"), True,
     "counts stay self-consistent but the total becomes 44 in-slice + 1 = 45, breaking §4's "
     "printed composition"),
    ("§3's shape changed entirely",
     lambda t: re.sub(r"^### 3\.\d+ ", "### X ", t, flags=re.M), True,
     "no section header parses -- the scanner must fail loudly rather than report zero units"),
]


def run_against(manifest_text):
    """Run the REAL architect.py with ARCH pointed at a synthetic manifest."""
    with tempfile.TemporaryDirectory() as td:
        fake = Path(td) / "BREACHPOINT-ARCHITECTURE.md"
        fake.write_text(manifest_text, encoding="utf-8")
        shim = Path(td) / "shim.py"
        shim.write_text(
            "import sys, pathlib\n"
            f"sys.path.insert(0, r'{HERE}')\n"
            "import architect\n"
            f"architect.ARCH = pathlib.Path(r'{fake}')\n"
            f"architect.STATE_DIR = pathlib.Path(r'{td}') / 'state'\n"
            "sys.exit(0 if architect.scan() else 0)\n", encoding="utf-8")
        p = subprocess.run([sys.executable, str(shim)], capture_output=True, text=True)
        return p.returncode


def main():
    if not ARCH.exists():
        print(f"FATAL: {ARCH} not found")
        return 1
    original = ARCH.read_text(encoding="utf-8")

    print("=" * 78)
    print("architect.py step-1 self-check -- RED-THEN-GREEN")
    print("=" * 78)
    print(f"  {'case':<30}{'expect':<10}{'exit':<7}result")

    failures = 0
    for name, transform, must_fail, why in CASES:
        text = transform(original)
        if text == original and must_fail:
            print(f"  {name:<30}{'--':<10}{'--':<7}SETUP BROKEN: transform changed nothing")
            failures += 1
            continue
        code = run_against(text)
        rejected = code != 0
        ok = rejected == must_fail
        failures += 0 if ok else 1
        print(f"  {name:<30}{'REJECT' if must_fail else 'ACCEPT':<10}{code:<7}"
              f"{'ok' if ok else 'FAILED'}")
        if not ok:
            print(f"      why this case exists: {why}")

    print()
    if failures:
        print(f"{failures} case(s) FAILED -- the self-check does not reject what it claims to.")
        return 1
    print(f"All {len(CASES)} cases behaved. The self-check has now been proven against cases it")
    print("must reject, not only against the one it passes.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
