"""Drive the LIVE editor's status-bar console from CPython over the Unreal MCP (HTTP).

    from ue_console import run_py, load_level
    lines = run_py("Tools/aib/aib25_lane_volumes.py", "place", tag="AIB25")

The ProgrammaticToolset sandbox forbids `import unreal`, so editor Python runs as
`py "<abs script>" args` typed into the status-bar console (SlateInspector Type, submit)
and is read back through LogsToolset.GetLogEntries(LogPython). A script signals it is
finished with a LogPython line matching `<tag>.*done`; run_py blocks on that (or on a
Python traceback) and returns every LogPython line carrying the tag from this run.
Reuses land_spillway.py's MCP client (Tools/blockout).
"""
import os
import re
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "blockout"))
import land_spillway as mcp  # noqa: E402

SLATE = "SlateInspectorToolset.SlateInspectorToolset"
LOGS = "EditorToolset.LogsToolset"
mcp.connect()          # one MCP session per process; every mcp.call below rides it


def connect():
    pass


def console_ref():
    """The status-bar console textbox: the only textbox at y~1776, x 500..1400."""
    connect()
    snap = mcp.call(SLATE, "Snapshot", {"ref": "", "maxDepth": 40})["returnValue"]
    for m in re.finditer(r"textbox[^\n]*\[pos=(\d+),(\d+) [^\n]*\[ref=(\w+)\]", snap):
        if 500 <= int(m.group(1)) <= 1400 and 1700 <= int(m.group(2)) <= 1850:
            return m.group(3)
    raise SystemExit("status-bar console textbox not found in the Slate snapshot")


def log(pattern, category="LogPython", n=5000):
    connect()
    return mcp.call(LOGS, "GetLogEntries", {"category": category, "pattern": pattern,
                                            "maxEntries": n})["returnValue"] or []


def load_level(path):
    connect()
    mcp.call(mcp.SCENE, "load_level", {"level_path": path})
    cur = mcp.call(mcp.SCENE, "get_current_level", {})["returnValue"]
    if cur != path:
        raise SystemExit("load_level: editor is on %s, wanted %s" % (cur, path))
    return cur


def run_py(script, args="", tag="AIB", done=r"\bdone\b", timeout=300):
    done_re = "%s.*%s" % (tag, done)
    n_tag, n_done, n_err = len(log(tag)), len(log(done_re)), len(log("Traceback"))
    cmd = 'py "%s" %s' % (os.path.abspath(script), args)
    mcp.call(SLATE, "Type", {"ref": console_ref(), "text": cmd, "submit": True})
    t0 = time.time()
    while time.time() - t0 < timeout:
        time.sleep(1)
        if len(log(done_re)) > n_done:
            return log(tag)[n_tag:]
        if len(log("Traceback")) > n_err:
            raise SystemExit("editor Python raised:\n" + "\n".join(log("Traceback|Error|File")[-30:]))
    raise SystemExit("timeout waiting for '%s' from %s" % (done_re, cmd))


if __name__ == "__main__":  # smoke: `python3 Tools/aib/ue_console.py`
    print("console ref:", console_ref())
