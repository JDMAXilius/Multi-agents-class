#!/usr/bin/env python3
"""Regression suite for guard_laws.py — proves the laws actually block.

Run:  python test_guard_laws.py            (tests the sibling guard_laws.py)
      python test_guard_laws.py <path>     (tests some other copy)

Exists because of a real defect found 31 Jul 2026: guard_laws.py compared a
Windows path string ("Source\\Breachpoint\\A.cpp") against the prefix
"Source/", so every check silently returned 0 and the hook enforced NOTHING
on Windows. A guard that has never been proven to block is a guard nobody has
tested — this file is that proof, per CREW_PLAYBOOK's honesty law.
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

HOOK = sys.argv[1] if len(sys.argv) > 1 else str(Path(__file__).parent / "guard_laws.py")

proj = Path(tempfile.mkdtemp(prefix="guardtest_"))
(proj / ".claude").mkdir()
(proj / "Source" / "Breachpoint" / "Weapons").mkdir(parents=True)
(proj / "Source" / "Breachpoint" / "Match").mkdir(parents=True)
(proj / "docs" / "tickets").mkdir(parents=True)
(proj / ".claude" / "active-packet.json").write_text(json.dumps(
    {"ticket": "BP03", "owner_path": ["Source/Breachpoint/Weapons/"]}))


def run(path, content):
    payload = json.dumps({"cwd": str(proj),
                          "tool_input": {"file_path": str(path), "content": content}})
    p = subprocess.run([sys.executable, HOOK], input=payload,
                       capture_output=True, text=True)
    return p.returncode, p.stderr.strip()


CASES = [
    ("law 2/3: engine damage API into Source/",
     proj / "Source/Breachpoint/Weapons/W.cpp", "void f(){ TakeDamage(1.f); }", 2),
    ("law 2/3: hard ConstructorHelpers ref into Source/",
     proj / "Source/Breachpoint/Weapons/W.cpp", "ConstructorHelpers::FObjectFinder<U> f;", 2),
    ("law 2/3: unseeded FMath::RandRange into Source/",
     proj / "Source/Breachpoint/Weapons/W.cpp", "int x = FMath::RandRange(0,3);", 2),
    ("law 5: write OUTSIDE the claimed owner_path",
     proj / "Source/Breachpoint/Match/M.cpp", "// harmless", 2),
    ("law 5: write INSIDE owner_path must pass",
     proj / "Source/Breachpoint/Weapons/W.cpp", "// harmless", 0),
    ("ALWAYS_ALLOWED: ticket Log",
     proj / "docs/tickets/TICKET_BP03_WEAPONS_FIRE.md", "log entry", 0),
]

fails = 0
for name, path, content, want in CASES:
    got, err = run(path, content)
    ok = got == want
    fails += not ok
    print(f"[{'PASS' if ok else 'FAIL'}] want exit {want}, got {got} :: {name}")
    if err:
        print(f"         -> {err[:110]}")

print(f"\n{len(CASES) - fails}/{len(CASES)} passed")
sys.exit(1 if fails else 0)
