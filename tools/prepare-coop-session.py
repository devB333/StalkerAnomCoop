"""Prepare an isolated co-op session: one campaign server and N clients (S05).

Usage: python3 tools/prepare-coop-session.py <run> [--task S05-T01] [--clients 2]
           [--port 27515] [--save "player - autosave"] [--fixture cordon|marsh] [--inject c1,...]
           [--script c1,c2] [--movement c1,c2] [--churn 10] [--drop 6]
           [--net-loss 5] [--net-lag 40] [--no-predict c1,c2] [--combat c1,c2]
           [--encounter c1,c2] [--weather c1,c2 [--psi-storm]]
           [--protocol-build proto-build-32] [--keys-from <earlier run>]

Each process gets its own runtime from prepare-stage2-smoke.py (fresh appdata,
logs, saves and temp; archives read in place from the accepted content copy):
  builds/stage2/test-install/<task>/<run>-server, <run>-c1, <run>-c2, ...
and a [coop] profile (coop.ltx) in that runtime. Clients also get a bin folder:
the accepted runtime's DLLs, the built AnomalyDX11.exe and
GameNetworkingSockets.dll, and a small windowed user.ltx.

Players: coop-keytool (from the protocol build) makes one DPAPI key file per
client (it opens only for this Windows user) and the server's roster lists the
public keys in client order, so client 1 is roster player 1 and holds the
saved actor. Nothing secret is written to session.json or the roster.

Session record: builds/stage5/<task>/<run>/session.json (paths, ids, hashes,
stop files). Tools/Run-CoopSession.ps1 starts the processes.
"""

import argparse
import hashlib
import json
import re
import secrets
import shutil
import subprocess
import sys
from pathlib import Path

workspace = Path(__file__).resolve().parents[1]
accepted = workspace / "builds/build_2/test-install/source-client"
built_client = workspace / "builds/build_1/engine-work/_build/_game/bin_dbg"


def winpath(path):
    return subprocess.check_output(["wslpath", "-w", str(path)], text=True).strip()


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("run")
    parser.add_argument("--task", default="S05-T01")
    parser.add_argument("--clients", type=int, default=2)
    parser.add_argument("--port", type=int, default=27515)
    parser.add_argument("--fixture", default="cordon", choices=("marsh", "cordon"),
                        help="the served save: cordon (default since S06-T06; docs/cordon-fixture.json, "
                             "l01_escape) or marsh (the headless fixture, k00_marsh)")
    parser.add_argument("--save", default="player - autosave",
                        help="saved game in the server runtime to serve; copied as coop-campaign "
                             "(no spaces: the engine's ini reader drops them)")
    parser.add_argument("--inject", default="", help="comma-separated client names (c1, c2) running the refusal test")
    parser.add_argument("--script", default="",
                        help="comma-separated client names running the S05-T03 look/walk/screenshot driver")
    parser.add_argument("--movement", default="",
                        help="comma-separated client names running the S06-T01 movement validation program "
                             "(profile script = 2)")
    parser.add_argument("--churn", type=int, default=0,
                        help="seconds between server-side medkit spawns, each released half a period later "
                             "(S05-T02 lifecycle fixture; 0 off)")
    parser.add_argument("--drop", type=int, default=0,
                        help="seconds between physics objects the server drops from 1.5 m beside player 1 "
                             "(S06-T02 moving-object fixture; 0 off)")
    parser.add_argument("--net-loss", type=float, default=0.0,
                        help="percent of packets every process drops on send (S06-T02 declared conditions)")
    parser.add_argument("--net-lag", type=int, default=0,
                        help="ms every process adds to what it sends (S06-T02 declared conditions)")
    parser.add_argument("--combat", default="",
                        help="comma-separated client names running the S06-T04 combat program (profile script = 3); "
                             "the server arms the participant actors and heals them below 40 %% (fixtures)")
    parser.add_argument("--encounter", default="",
                        help="comma-separated client names running the S06-T05 encounter program (profile script = 4); "
                             "the server arms the actors, spawns a boar beside participant 2 20 s after both "
                             "send input and kills participant 2's actor 8 s after the boar died, or 60 s "
                             "after the spawn while it lives (fixtures)")
    parser.add_argument("--weather", default="",
                        help="comma-separated client names running the S06-T08 weather program (profile script = 5: "
                             "stand and take screenshots); the server forces w_storm1 10 s after both players are "
                             "in the world and starts the installed surge_manager's emission 70 s after (fixtures)")
    parser.add_argument("--psi-storm", action="store_true",
                        help="with --weather: a psi-storm instead of the emission (psi_storm_manager)")
    parser.add_argument("--no-predict", default="",
                        help="comma-separated client names that do not predict their own actor (S06-T03 baseline; "
                             "profile predict = 0)")
    parser.add_argument("--protocol-build", default="proto-build-32")
    parser.add_argument("--keys-from", default="", help="reuse the keys and roster of an earlier session run")
    args = parser.parse_args()

    if not re.fullmatch(r"[a-zA-Z0-9-]+", args.run):
        raise SystemExit("Run name must be alphanumeric with optional hyphens")
    if not re.fullmatch(r"S\d\d-T\d\d[a-z]?", args.task):
        raise SystemExit("Task id must look like S05-T01")
    if not 1 <= args.clients <= 2:
        raise SystemExit("One or two clients")
    session_dir = workspace / "builds/stage5" / args.task / args.run
    if session_dir.exists():
        raise SystemExit(f"Preserving existing session: {session_dir}")
    keytool = workspace / "builds/protocol/build" / args.protocol_build / "protocol/coop-keytool.exe"
    if not keytool.exists():
        raise SystemExit(f"No coop-keytool in {args.protocol_build}")
    for name in ("AnomalyDX11.exe", "GameNetworkingSockets.dll"):
        if not (built_client / name).exists():
            raise SystemExit(f"Build the client first: {built_client / name} is missing")
    session_dir.mkdir(parents=True)
    (session_dir / "keys").mkdir()

    names = ["server"] + [f"c{i + 1}" for i in range(args.clients)]
    inject = {n for n in args.inject.split(",") if n}
    script = {n for n in args.script.split(",") if n}
    movement = {n for n in args.movement.split(",") if n}
    no_predict = {n for n in args.no_predict.split(",") if n}
    combat = {n for n in args.combat.split(",") if n}
    encounter = {n for n in args.encounter.split(",") if n}
    weather = {n for n in args.weather.split(",") if n}
    if (inject | script | movement | weather) - set(names[1:]):
        raise SystemExit("--inject/--script/--movement name clients that do not exist")
    if script & movement:
        raise SystemExit("a client runs one driver: --script or --movement")

    # One isolated runtime per process.
    runtimes = {}
    for name in names:
        run_name = f"{args.run}-{name}"
        subprocess.run([sys.executable, str(workspace / "tools/prepare-stage2-smoke.py"), run_name, args.task],
                       check=True, stdout=subprocess.DEVNULL)
        runtimes[name] = workspace / "builds/stage2/test-install" / args.task / run_name

    # The served save under a space-free name.
    saves = runtimes["server"] / "appdata/savedgames"
    if args.fixture == "cordon":
        # S06-T06: the Cordon pair, hash-checked against its record
        record = json.loads((workspace / "docs/cordon-fixture.json").read_text())
        for item in record["inputs"]:
            source = workspace / item["path"]
            if hashlib.sha256(source.read_bytes()).hexdigest() != item["sha256"]:
                raise SystemExit(f"Cordon fixture changed: {item['path']}")
            shutil.copyfile(source, saves / source.name)
        args.save = record["save_basename"]
    for extension in (".scop", ".scoc"):
        source = saves / (args.save + extension)
        if not source.exists():
            raise SystemExit(f"No saved game {source}")
        shutil.copyfile(source, saves / ("coop-campaign" + extension))

    # Players: keys (DPAPI, this Windows user) and the public roster.
    roster_lines = []
    players = {}
    if args.keys_from:
        earlier = json.loads((workspace / "builds/stage5" / args.task / args.keys_from / "session.json").read_text())
        for name in names[1:]:
            source = workspace / earlier["players"][name]["key"]
            shutil.copyfile(source, session_dir / "keys" / f"{name}.key")
            players[name] = dict(earlier["players"][name])
            players[name]["key"] = str((session_dir / "keys" / f"{name}.key").relative_to(workspace))
            roster_lines.append(players[name]["roster_line"])
    else:
        for index, name in enumerate(names[1:], start=1):
            key = session_dir / "keys" / f"{name}.key"
            display = f"Player {index}"
            out = subprocess.run([str(keytool), "new-player", "--name", display, "--key", winpath(key)],
                                 check=True, capture_output=True, text=True).stdout.strip().splitlines()
            line = [x for x in out if x.startswith("player ")]
            if len(line) != 1:
                raise SystemExit(f"coop-keytool printed no roster line for {name}")
            fields = line[0].split(" ", 3)
            players[name] = {"display_name": display, "player_id": fields[1], "public_key": fields[2],
                             "roster_line": line[0], "key": str(key.relative_to(workspace))}
            roster_lines.append(line[0])
    roster = session_dir / "roster.txt"
    roster.write_text("# S05 co-op roster: public keys only (docs/session-authentication.md)\n"
                      + "\n".join(roster_lines) + "\n")

    campaign_id = secrets.token_hex(16)
    party_id = secrets.token_hex(16)
    stop_files = {name: session_dir / f"stop-{name}" for name in names}
    profiles = {}
    for name in names:
        runtime = runtimes[name]
        logs = runtime / "appdata/logs"
        lines = ["[coop]", f"campaign_id = {campaign_id}", f"log = {winpath(logs)}\\coop-{name}.log",
                 f"stop_file = {winpath(session_dir)}\\stop-{name}"]
        if args.net_loss or args.net_lag:
            lines += [f"net_loss = {args.net_loss}", f"net_lag = {args.net_lag}"]
        if name == "server":
            lines += ["role = server", f"listen = 0.0.0.0:{args.port}", f"roster = {winpath(roster)}",
                      f"party_id = {party_id}", "save = coop-campaign", "serve_seconds = 3600",
                      f"churn = {args.churn}", f"drop = {args.drop}",
                      f"arm = {1 if combat or encounter else 0}", f"combat_heal = {1 if combat else 0}",
                      f"encounter = {20 if encounter else 0}", f"lethal = {8 if encounter else 0}",
                      f"storm = {10 if weather else 0}",
                      f"surge = {70 if weather and not args.psi_storm else 0}",
                      f"psi_storm = {70 if weather and args.psi_storm else 0}"]
        else:
            program = (5 if name in weather else 4 if name in encounter else 3 if name in combat
                       else 2 if name in movement else 1 if name in script else 0)
            lines += ["role = client", f"server = 127.0.0.1:{args.port}",
                      f"key = {winpath(session_dir / 'keys' / (name + '.key'))}",
                      f"name = {players[name]['display_name']}", "exit_after_seconds = 1800",
                      f"inject = {1 if name in inject else 0}",
                      f"script = {program}",
                      f"predict = {0 if name in no_predict else 1}"]
        profile = runtime / "coop.ltx"
        profile.write_text("\n".join(lines) + "\n")
        profiles[name] = profile

    # Client bins: the accepted runtime's DLLs, the built client and GNS.
    client_bins = {}
    for name in names[1:]:
        runtime = runtimes[name]
        bin_dir = runtime / "bin"
        bin_dir.mkdir()
        for item in (accepted / "bin").iterdir():
            if item.is_file() and item.name not in ("AnomalyDX11.exe", "AnomalyDX11.pdb"):
                shutil.copyfile(item, bin_dir / item.name)
        for item in ("AnomalyDX11.exe", "GameNetworkingSockets.dll"):
            shutil.copyfile(built_client / item, bin_dir / item)
        for item in accepted.glob("*.mhr"):
            shutil.copyfile(item, runtime / item.name)
        settings = []
        for line in (accepted / "appdata/user.ltx").read_text(errors="replace").splitlines():
            if line.startswith("rs_screenmode "):
                line = "rs_screenmode windowed"
            elif line.startswith("vid_mode "):
                line = "vid_mode 800x600"
            elif line.startswith("keypress_on_start "):
                line = "keypress_on_start off"  # unattended clients: no "press any key" after loading
            elif line.startswith("g_always_active "):
                line = "g_always_active on"  # both windows keep rendering when the other has the focus
            settings.append(line)
        (runtime / "appdata/user.ltx").write_text("\n".join(settings) + "\n")
        shutil.copytree(accepted / "appdata/shaders_cache", runtime / "appdata/shaders_cache")
        client_bins[name] = {"bin": str(bin_dir.relative_to(workspace)),
                             "exe_sha256": sha256(bin_dir / "AnomalyDX11.exe"),
                             "gns_sha256": sha256(bin_dir / "GameNetworkingSockets.dll")}

    record = {
        "run": args.run,
        "task": args.task,
        "campaign_id": campaign_id,
        "party_id": party_id,
        "port": args.port,
        "save": args.save,
        "fixture": args.fixture,
        "protocol_build": args.protocol_build,
        "drop": args.drop,
        "net_loss_pct": args.net_loss,
        "net_lag_ms": args.net_lag,
        "roster": str(roster.relative_to(workspace)),
        "roster_sha256": sha256(roster),
        "players": {name: {k: v for k, v in data.items()} for name, data in players.items()},
        "processes": {name: {"runtime": str(runtimes[name].relative_to(workspace)),
                             "fsgame": winpath(runtimes[name] / "fsgame.ltx"),
                             "profile": winpath(profiles[name]),
                             "profile_sha256": sha256(profiles[name]),
                             "stop_file": winpath(stop_files[name]),
                             "inject": name in inject}
                      for name in names},
        "client_bins": client_bins,
    }
    (session_dir / "session.json").write_text(json.dumps(record, indent=2) + "\n")
    print(session_dir / "session.json")


if __name__ == "__main__":
    main()
