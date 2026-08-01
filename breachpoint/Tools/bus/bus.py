#!/usr/bin/env python3
"""BP17 — the session bus: two live terminals handing each other work.

    bus.py post --to T2 --mode OPEN --subject "..." --body "..." [--ticket BP16] [--pri high]
    bus.py inbox [--for T1] [--mode FILES]     what I can actually run, most relevant first
    bus.py claim <id>  |  bus.py done <id> --result "..."  |  bus.py drop <id> --why "..."
    bus.py board                                everything, both directions
    bus.py whoami

WHY A BUS AND NOT A FILE. Both terminals share ONE working tree, so a file written by T1 is
visible to T2 instantly -- no push, no pull, no latency. What is NOT shared is attention: a
Claude Code session cannot be woken by a file appearing. So this is a MAILBOX plus a discipline,
and the `Stop` hook in .claude/hooks/bus_notify.py is what turns the mailbox into "never stop
working" -- it fires when a session tries to finish and hands it the next relevant message.

ONE MESSAGE PER FILE, never a shared log. Two sessions appending to one file is the law-7
tension R23 had to argue its way around; here it is simply avoided. No two writers ever touch
the same path, so there is no conflict to resolve.

ROUTING IS BY MODE, NOT BY NAME (docs/WORK-ROUTING.md §1). A message declares the lock mode it
needs -- OPEN (live editor), CLOSED (commandlet/build, editor must be gone), FILES (no engine),
ANY -- and `inbox` shows a terminal only what its current mode can actually run. That is the
"relevancy" half; priority and age order the rest. A terminal never picks up work it would have
to block on.

Exit codes: 0 ok · 1 usage/not-found · 2 nothing to do (useful for the Stop hook).
"""

import argparse
import json
import os
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
BUS = REPO / "docs" / "bus"
MODES = ("OPEN", "CLOSED", "FILES", "ANY")
PRIORITIES = {"high": 0, "normal": 1, "low": 2}


def now():
    return datetime.now(timezone.utc)


def whoami():
    """Terminal identity is per-SESSION, so it cannot live in the shared tree -- env var only."""
    return os.environ.get("BR_TERMINAL", "").strip() or "UNSET"


def current_mode():
    """What this terminal can run right now. Declared, not guessed -- a terminal that lies here
    picks up work it must immediately block on, which is worse than picking up nothing."""
    return (os.environ.get("BR_MODE", "").strip() or "FILES").upper()


def slug(s, n=48):
    return re.sub(r"-+", "-", re.sub(r"[^a-z0-9]+", "-", s.lower())).strip("-")[:n] or "msg"


def parse(path):
    text = path.read_text(encoding="utf-8")
    head, _, body = text.partition("\n---\n")
    meta = {}
    for line in head.splitlines():
        if ":" in line and not line.startswith("#"):
            k, _, v = line.partition(":")
            meta[k.strip()] = v.strip()
    meta["_path"] = path
    meta["_body"] = body.strip()
    meta["_id"] = path.stem
    return meta


def load_all():
    if not BUS.exists():
        return []
    return sorted((parse(p) for p in BUS.glob("*.md")), key=lambda m: m["_id"])


def write(meta, body):
    lines = [f"{k}: {v}" for k, v in meta.items() if not k.startswith("_")]
    meta["_path"].write_text("\n".join(lines) + "\n---\n" + body.rstrip() + "\n", encoding="utf-8")


def find(mid):
    for m in load_all():
        if m["_id"] == mid or m["_id"].startswith(mid):
            return m
    print(f"no message matching '{mid}'")
    sys.exit(1)


def cmd_post(a):
    sender = a.sender or whoami()
    if sender == "UNSET":
        print("refusing to post from an unidentified terminal. Set BR_TERMINAL=T1 (or T2).")
        return 1
    stamp = now().strftime("%Y%m%dT%H%M%SZ")
    name = f"{stamp}--{sender}-to-{a.to}--{slug(a.subject)}"
    BUS.mkdir(parents=True, exist_ok=True)
    meta = {"from": sender, "to": a.to, "mode": a.mode, "priority": a.pri,
            "ticket": a.ticket or "-", "status": "open", "subject": a.subject,
            "posted": stamp, "_path": BUS / f"{name}.md"}
    body = a.body or ""
    if a.body_file:
        body = Path(a.body_file).read_text(encoding="utf-8")
    write(meta, body)
    print(f"posted {name}\n  {sender} -> {a.to} | mode {a.mode} | pri {a.pri} | {a.subject}")
    return 0


# What a terminal in a given mode can ACTUALLY run. Reported by T2 on the bus within minutes
# of the bus existing: the first version compared modes for equality, so an OPEN terminal was
# blind to FILES work and its inbox printed "empty" with work waiting -- the exact failure the
# bus exists to prevent, silent on the passing side. FILES needs no engine and therefore runs
# everywhere; OPEN and CLOSED are exclusive with each other, not with FILES.
CAN_RUN = {
    "OPEN":   {"OPEN", "FILES", "ANY"},
    "CLOSED": {"CLOSED", "FILES", "ANY"},
    "FILES":  {"FILES", "ANY"},
    "ANY":    set(MODES),
}


def relevant(m, me, mode):
    if m.get("status") != "open":
        return False
    if m.get("to") not in (me, "ANY"):
        return False
    return m.get("mode") in CAN_RUN.get(mode, {"ANY"})


def cmd_inbox(a):
    me = a.sender or whoami()
    mode = (a.mode or current_mode()).upper()
    msgs = [m for m in load_all() if relevant(m, me, mode)]
    msgs.sort(key=lambda m: (PRIORITIES.get(m.get("priority", "normal"), 1), m["_id"]))
    if not msgs:
        blocked = [m for m in load_all()
                   if m.get("status") == "open" and m.get("to") in (me, "ANY")]
        print(f"inbox empty for {me} in mode {mode}.")
        if blocked:
            print(f"  ({len(blocked)} message(s) addressed to you need another mode: "
                  f"{', '.join(sorted({m.get('mode', '?') for m in blocked}))})")
        return 2
    print(f"INBOX {me} — mode {mode} — {len(msgs)} actionable\n")
    for m in msgs:
        print(f"  [{m.get('priority')}] {m['_id']}")
        print(f"      from {m.get('from')} | mode {m.get('mode')} | ticket {m.get('ticket')}")
        print(f"      {m.get('subject')}")
        if a.full and m["_body"]:
            for line in m["_body"].splitlines():
                print(f"        {line}")
        print()
    return 0


def _set_status(mid, status, note_key=None, note=None):
    m = find(mid)
    m["status"] = status
    if note_key and note:
        m[note_key] = note
    m[f"{status}_at"] = now().strftime("%Y%m%dT%H%M%SZ")
    write(m, m["_body"])
    print(f"{m['_id']} -> {status}")
    return 0


def cmd_claim(a):
    return _set_status(a.id, "claimed", "claimed_by", a.sender or whoami())


def cmd_done(a):
    return _set_status(a.id, "done", "result", a.result)


def cmd_drop(a):
    return _set_status(a.id, "dropped", "why", a.why)


def cmd_board(a):
    msgs = load_all()
    if not msgs:
        print("bus is empty")
        return 0
    print(f"{'id':<52}{'from':<5}{'to':<5}{'mode':<8}{'pri':<8}{'status':<9}subject")
    for m in msgs:
        if not a.all and m.get("status") in ("done", "dropped"):
            continue
        print(f"{m['_id']:<52}{m.get('from',''):<5}{m.get('to',''):<5}"
              f"{m.get('mode',''):<8}{m.get('priority',''):<8}{m.get('status',''):<9}"
              f"{m.get('subject','')[:60]}")
    return 0


def cmd_whoami(a):
    print(f"BR_TERMINAL = {whoami()}\nBR_MODE     = {current_mode()}")
    if whoami() == "UNSET":
        print("\nSet it per shell (it is per-session, so it must NOT live in the shared tree):")
        print("  PowerShell:  $env:BR_TERMINAL='T1'; $env:BR_MODE='FILES'")
        print("  bash:        export BR_TERMINAL=T1 BR_MODE=FILES")
    return 0


def main():
    ap = argparse.ArgumentParser(description="BP17 session bus")
    ap.add_argument("--sender", help="override BR_TERMINAL")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("post"); p.set_defaults(fn=cmd_post)
    p.add_argument("--to", required=True)
    p.add_argument("--mode", default="ANY", choices=MODES)
    p.add_argument("--pri", default="normal", choices=list(PRIORITIES))
    p.add_argument("--ticket")
    p.add_argument("--subject", required=True)
    p.add_argument("--body", default="")
    p.add_argument("--body-file")

    p = sub.add_parser("inbox"); p.set_defaults(fn=cmd_inbox)
    p.add_argument("--for", dest="sender_for")
    p.add_argument("--mode", choices=MODES)
    p.add_argument("--full", action="store_true")

    for name, fn in (("claim", cmd_claim), ("done", cmd_done), ("drop", cmd_drop)):
        p = sub.add_parser(name); p.set_defaults(fn=fn)
        p.add_argument("id")
        if name == "done":
            p.add_argument("--result", default="")
        if name == "drop":
            p.add_argument("--why", default="")

    p = sub.add_parser("board"); p.set_defaults(fn=cmd_board)
    p.add_argument("--all", action="store_true", help="include done/dropped")

    sub.add_parser("whoami").set_defaults(fn=cmd_whoami)

    a = ap.parse_args()
    if getattr(a, "sender_for", None):
        a.sender = a.sender_for
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
