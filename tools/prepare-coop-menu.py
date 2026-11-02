#!/usr/bin/env python3
"""S11-T00: a local hosting setup for the host and join menu, on this PC.

Usage: python3 tools/prepare-coop-menu.py <run> [--task S11-T00] [--protocol-build proto-build-N] [--test]

Prepares a two-client session with tools/prepare-coop-session.py, then turns it into what the menu uses:
  - the server runtime is the host's: no time limit (serve_seconds = 0); its roster lists only player 1 (the
    host's key); player 2 is not listed and gets in with the host's join code;
  - each client's app data gets coop-menu.ltx (docs/validation/S11-T00.md): player 1 can host (the server's
    runtime, its exe and tools/Start-CoopServer.ps1), player 2 only joins; player 2's key does not exist yet
    (the menu makes it on first use);
  - --test adds the menu's test driver: player 1 hosts a new campaign, player 2 joins with the address and code
    player 1's client wrote (its app data coop-host-info.txt).
Start the clients with tools/Run-CoopMenuTest.ps1 (no -coop-config: the menu starts the link).
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

workspace = Path(__file__).resolve().parent.parent


def win(p: Path) -> str:
    """The Windows form of a workspace path (/mnt/c/... -> C:\\...)."""
    s = str(p)
    m = re.match(r"^/mnt/([a-z])/(.*)$", s)
    return (m.group(1).upper() + ":\\" + m.group(2).replace("/", "\\")) if m else s


def unwin(s: str) -> Path:
    m = re.match(r"^([A-Za-z]):\\(.*)$", s)
    return Path("/mnt/" + m.group(1).lower() + "/" + m.group(2).replace("\\", "/")) if m else Path(s)


def ini_value(text: str, key: str) -> str:
    m = re.search(r"^\s*" + re.escape(key) + r"\s*=\s*(.*?)\s*$", text, re.M)
    return m.group(1) if m else ""


def set_value(text: str, key: str, value: str) -> str:
    if re.search(r"^\s*" + re.escape(key) + r"\s*=", text, re.M):
        return re.sub(r"^\s*" + re.escape(key) + r"\s*=.*$", f"{key} = {value}", text, flags=re.M)
    return text.rstrip("\n") + f"\n{key} = {value}\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("run")
    ap.add_argument("--task", default="S11-T00")
    ap.add_argument("--protocol-build", default="proto-build-62")
    ap.add_argument("--test", action="store_true")
    ap.add_argument("--dedicated", action="store_true",
                    help="a dedicated server (tools/Host-CoopServer.ps1): no one on its roster, both players join "
                         "with its code, neither client can host")
    a = ap.parse_args()
    if not re.fullmatch(r"[a-zA-Z0-9-]+", a.run):
        raise SystemExit("Run name must be alphanumeric with optional hyphens")
    subprocess.run([sys.executable, str(workspace / "tools/prepare-coop-session.py"), a.run, "--task", a.task,
                    "--protocol-build", a.protocol_build], check=True, stdout=subprocess.DEVNULL)
    session_dir = workspace / "builds/stage5" / a.task / a.run
    session = json.loads((session_dir / "session.json").read_text())
    procs = session["processes"]
    server_runtime = workspace / procs["server"]["runtime"]
    server_profile = server_runtime / "coop.ltx"
    text = server_profile.read_text()
    text = set_value(text, "serve_seconds", "0")
    server_profile.write_text(text)
    roster = unwin(ini_value(text, "roster"))
    host_id = "" if a.dedicated else session["players"]["c1"]["player_id"]
    kept = [ln for ln in roster.read_text().splitlines()
            if not ln.startswith("player ") or (host_id and f" {host_id} " in ln)]
    roster.write_text("\n".join(kept) + "\n")
    appdata = server_runtime / "appdata"
    for name in ("coop-join-code.txt", "coop-create.ltx", "coop-continue.txt", "coop-relaunch.ltx", "coop-owner.txt"):
        (appdata / name).unlink(missing_ok=True)
    server_exe = workspace / "builds/stage2/campaign/_game/bin_dbg/AnomalyCampaignServer.exe"
    launcher = workspace / "tools/Start-CoopServer.ps1"
    c1_runtime = workspace / procs["c1"]["runtime"]
    for name, host in (("c1", not a.dedicated), ("c2", False)):
        runtime = workspace / procs[name]["runtime"]
        profile = (runtime / "coop.ltx").read_text()
        key = ini_value(profile, "key")
        if not host:
            key_path = session_dir / "keys" / f"{name}-menu.key"
            key_path.unlink(missing_ok=True)
            key = win(key_path)
        menu = ["[coop_menu]", f"name = {ini_value(profile, 'name')}", f"key = {key}",
                f"client_log = {ini_value(profile, 'log')}", f"client_stop = {ini_value(profile, 'stop_file')}"]
        if host:
            menu += [f"server_exe = {win(server_exe)}", f"server_launcher = {win(launcher)}",
                     f"server_runtime = {win(server_runtime)}"]
        if a.test:
            if host:
                menu += ["test = host_new", "test_delay = 3"]
            else:
                info = (server_runtime if a.dedicated else c1_runtime) / "appdata" / "coop-host-info.txt"
                menu += ["test = join", "test_delay = 10", f"test_info = {win(info)}"]
        (runtime / "appdata" / "coop-menu.ltx").write_text("\n".join(menu) + "\n")
        for name2 in ("coop-host-info.txt", "coop-menu-client.ltx"):
            (runtime / "appdata" / name2).unlink(missing_ok=True)
    (appdata / "coop-host-info.txt").unlink(missing_ok=True)
    session["menu"] = {"test": a.test, "host": "server" if a.dedicated else "c1", "joiner": "c2",
                       "dedicated": a.dedicated, "server_exe": win(server_exe), "launcher": win(launcher),
                       "server_runtime": win(server_runtime)}
    (session_dir / "session.json").write_text(json.dumps(session, indent=1) + "\n")
    print(session_dir / "session.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
