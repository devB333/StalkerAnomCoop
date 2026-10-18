"""Block Claude file edits to the read-only original install and the preserved Ghidra database."""
import json, re, sys

data = json.load(sys.stdin)
inp = data.get("tool_input", {})
path = inp.get("file_path") or inp.get("notebook_path") or ""
# Normalize Windows paths (C:\...) to WSL form, lowercase for comparison.
n = path.replace("\\", "/")
n = re.sub(r"^([A-Za-z]):/", lambda m: f"/mnt/{m.group(1).lower()}/", n).lower()

reason = None
if n == "/mnt/c/users/djb20/stalkeranom" or n.startswith("/mnt/c/users/djb20/stalkeranom/"):
    reason = "The original Anomaly install is strictly read-only (AGENTS.md)."
elif "/reverse-engineering/ghidra/anomaly153-c857b6f49d32" in n:
    reason = "The preserved Ghidra database must never be overwritten (AGENTS.md)."

if reason:
    print(json.dumps({"hookSpecificOutput": {
        "hookEventName": "PreToolUse",
        "permissionDecision": "deny",
        "permissionDecisionReason": f"{reason} Blocked write to: {path}",
    }}))
