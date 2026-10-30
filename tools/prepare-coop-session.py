"""Prepare an isolated co-op session: one campaign server and N clients (S05).

Usage: python3 tools/prepare-coop-session.py <run> [--task S05-T01] [--clients 2]
           [--port 27515] [--save "player - autosave"] [--fixture cordon|marsh] [--inject c1,...]
           [--script c1,c2] [--movement c1,c2] [--churn 10] [--drop 6]
           [--net-loss 5] [--net-lag 40] [--no-predict c1,c2] [--combat c1,c2]
           [--encounter c1,c2] [--weather c1,c2 [--psi-storm]]
           [--ledger c1,c2] [--checkpoint-seconds 30] [--failpoint commit:after-apply:3[:fail]]
           [--pickup c1,c2 [--pickup-rounds 8] [--pickup-start 20]]
           [--transfer c1,c2 [--transfer-start 20]] [--quest c1,c2 [--quest-start 20]]
           [--use c1,c2 [--use-start 20]] [--stack c1,c2 [--stack-start 20]]
           [--travel c1,c2 [--travel-start 20] [--travel-variant 0|1|2|3|4] [--travel-barrier-hold 5]
            [--travel-ready-delay c2:15] [--travel-ready-wrong c2] [--travel-rejoin-delay c2:75]
            [--travel-load-fail c2]] [--talk c1,c2 [--talk-start 20]] [--info c1,c2 [--info-start 20]]
           [--party-task c1,c2 [--party-task-start 20]] [--trade c1,c2 [--trade-start 20]]
           [--protocol-build proto-build-42] [--keys-from <earlier run>]

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
    parser.add_argument("--ledger", default="",
                        help="comma-separated client names running the S07-T03 ledger program (profile script = 6: "
                             "a ledger intent every 2 s, unacknowledged ones retried every 3 s with the same id)")
    parser.add_argument("--pickup", default="",
                        help="comma-separated client names running the S08-T01 pickup race driver "
                             "(profile script = 7); the server drops an item between the two actors every 8 s "
                             "and names the tick at which both claim it, then three refusal rounds "
                             "(fixture pickup_race)")
    parser.add_argument("--pickup-rounds", type=int, default=8, help="with --pickup: race rounds")
    parser.add_argument("--pickup-start", type=int, default=20,
                        help="with --pickup: seconds after both players are in the world until the first round")
    parser.add_argument("--transfer", default="",
                        help="comma-separated client names running the S08-T02 transfer driver (profile script = 8); "
                             "the server sets up a box, a far box, a corpse and items and runs nine rounds of drops "
                             "and transfers through the clients' own inventory events (fixture transfer_race)")
    parser.add_argument("--transfer-start", type=int, default=20,
                        help="with --transfer: seconds after both players are in the world until the set-up")
    parser.add_argument("--use", default="",
                        help="comma-separated client names running the S08-T07 equip and use rounds "
                             "(profile script = 8, the transfer driver's steps; fixture use_race)")
    parser.add_argument("--use-start", type=int, default=20,
                        help="with --use: seconds after both players are in the world until the items appear")
    parser.add_argument("--stack", default="",
                        help="comma-separated client names running the S08-T08 container and stack rounds "
                             "(profile script = 8, the transfer driver's steps; fixture stack_race)")
    parser.add_argument("--stack-start", type=int, default=20,
                        help="with --stack: seconds after both players are in the world until the objects appear")
    parser.add_argument("--quest", default="",
                        help="comma-separated client names running the S08-T04 party quest rounds (profile script = 8, "
                             "the transfer driver's steps); the server places both actors beside Sidorovich "
                             "(fixture quest_party)")
    parser.add_argument("--quest-start", type=int, default=20,
                        help="with --quest: seconds after both players are in the world until the actors are placed")
    parser.add_argument("--quest-holder", type=int, default=2,
                        help="with --quest: the slot whose accept comes first (0 or 1; 2: both at the same tick)")
    parser.add_argument("--quest-variant", type=int, default=0,
                        help="with --quest: round 6: 0 both turn in at once, 1 the contributor disconnects first")
    parser.add_argument("--travel", default="",
                        help="comma-separated client names running the S09-T01 party travel rounds (profile "
                             "script = 8, the transfer driver's steps); the server places both actors inside a level "
                             "changer (fixture travel_party)")
    parser.add_argument("--travel-start", type=int, default=20,
                        help="with --travel: seconds after both players are in the world until the actors are placed")
    parser.add_argument("--travel-variant", type=int, default=0,
                        help="with --travel: 0 every round; 1 the crash run (rounds 0 and 1; with --failpoint "
                             "travel:after-propose the server exits itself after round 0's proposal); 2 (S09-T02) "
                             "round 0 only and the agreed travel is carried out (the party arrives on the "
                             "destination level; --failpoint travel:before-take / travel:after-commit for the "
                             "crash windows); 3 (S09-T04) the partner accepts and drops its connection inside the "
                             "barrier (with --travel-barrier-hold), then the party asks again and travels; 4 "
                             "(S09-T04) two agreed travels, the first's publication failed from outside "
                             "(--failpoint checkpoint:after-manifest:3:fail), the second carried out")
    parser.add_argument("--talk", default="",
                        help="comma-separated client names running the S10-T04a talk-window rounds at Sidorovich "
                             "(profile script = 8; fixture talk_party)")
    parser.add_argument("--talk-start", type=int, default=20,
                        help="with --talk: seconds after both players are in the world until the actors are placed")
    parser.add_argument("--talk-variant", type=int, default=0,
                        help="with --talk: round 4's turn-in by talking: 0 participant 1 (the accepter), 1 the other")
    parser.add_argument("--info", default="",
                        help="comma-separated client names present for the S10-T04b info-portion and actor-state "
                             "fixture (server-driven; fixture info_party; use with --auto-respawn 5)")
    parser.add_argument("--info-start", type=int, default=20,
                        help="with --info: seconds after both players are in the world until the checks start")
    parser.add_argument("--party-task", default="",
                        help="comma-separated client names present for the S10-T05b general party-task fixture "
                             "(server-driven: p2 accepts Sidorovich's bounty, a checkpoint; after an external restart "
                             "the target is killed by p2 and the task turned in; fixture task_party)")
    parser.add_argument("--party-task-start", type=int, default=20,
                        help="with --party-task: seconds after both players are in the world until the fixture starts")
    parser.add_argument("--trade", default="",
                        help="comma-separated client names present for the S10-T04c door and trade fixture "
                             "(server-driven: p2 uses the bunker door, p1 opens the trade, buys and sells; an external "
                             "restart replays the trade records; fixture trade_party)")
    parser.add_argument("--trade-start", type=int, default=20,
                        help="with --trade: seconds after both players are in the world until the fixture starts")
    parser.add_argument("--upgrade", default="",
                        help="comma-separated client names present for the S10-T04f upgrade and repair fixture "
                             "(server-driven: both placed at the nearest mechanic, p1 opens the upgrade session, "
                             "installs an upgrade and repairs a weapon, p2 refused busy; an external restart replays "
                             "the records; fixture upgrade_party)")
    parser.add_argument("--upgrade-start", type=int, default=20,
                        help="with --upgrade: seconds after both players are in the world until the fixture starts")
    parser.add_argument("--relations", default="",
                        help="comma-separated client names present for the S10-T04d relations fixture (server-driven: "
                             "p2's stalker goodwill changed through the Lua export with character 0, a stalker/bandit "
                             "relation set, p1's reputation raised, a checkpoint published; fixture relations_party)")
    parser.add_argument("--relations-start", type=int, default=20,
                        help="with --relations: seconds after both players are in the world until the fixture starts")
    parser.add_argument("--stash", default="",
                        help="comma-separated client names present for the S10-T04e stash fixture (server-driven: the "
                             "nearest box made a stash in p1's context, p1 uses it and takes one item; a checkpoint "
                             "follows the spawn; fixture stash_party)")
    parser.add_argument("--stash-start", type=int, default=20,
                        help="with --stash: seconds after both players are in the world until the fixture starts")
    parser.add_argument("--npc", default="",
                        help="comma-separated client names present for the S10-T02a NPC logic fixture (server-driven: "
                             "the stalker nearest p2 evaluates db.actor, AC_ID and a distance condlist inside and "
                             "outside its evaluation scope; fixture npc_party)")
    parser.add_argument("--npc-start", type=int, default=20,
                        help="with --npc: seconds after both players are in the world until the fixture starts")
    parser.add_argument("--loot", default="",
                        help="comma-separated client names present for the S10-T02b NPC loot fixture (server-driven: "
                             "two stalkers spawned beside p2, p2 kills one, the other loots the body through the "
                             "installed corpse scheme, the body's release is asked for; fixture loot_party)")
    parser.add_argument("--loot-start", type=int, default=20,
                        help="with --loot: seconds after both players are in the world until the fixture starts")
    parser.add_argument("--psy", default="",
                        help="comma-separated client names present for the S10-T02c psy fixture (server-driven: p1 "
                             "moved away, a controller spawned near p2, its psy attack on p2 awaited; fixture "
                             "psy_party)")
    parser.add_argument("--psy-start", type=int, default=20,
                        help="with --psy: seconds after both players are in the world until the fixture starts")
    parser.add_argument("--mutant", default="",
                        help="comma-separated client names present for the S10-T02d mutant fixture (server-driven: p1 "
                             "moved away, a bloodsucker, a burer and a poltergeist in turn near p2; fixture "
                             "mutant_party)")
    parser.add_argument("--mutant-start", type=int, default=20,
                        help="with --mutant: seconds after both players are in the world until the fixture starts")
    parser.add_argument("--fauna", default="",
                        help="comma-separated client names present for the S10-T02e fauna fixture (server-driven: p1 "
                             "moved away, each generic mutant class in turn near p2; fixture fauna_party)")
    parser.add_argument("--fauna-start", type=int, default=20,
                        help="with --fauna: seconds after both players are in the world until the fixture starts")
    parser.add_argument("--companion", default="",
                        help="comma-separated client names present for the S10-T02f companion fixture (server-driven: "
                             "p2 recruits the nearest friendly stalker, or a restored companion is checked; fixture "
                             "companion_party)")
    parser.add_argument("--companion-start", type=int, default=20,
                        help="with --companion: seconds after both players are in the world until the fixture starts")
    parser.add_argument("--alife", default="",
                        help="comma-separated client names present for the S10-T03a ALife fixture (server-driven: "
                             "p1 moves away, the respawn guard for p2, a forced respawn, a kill, a checkpoint; after a "
                             "restart the dead stay dead and the respawned squad exists once; fixture alife_party)")
    parser.add_argument("--alife-start", type=int, default=20,
                        help="with --alife: seconds after both players are in the world until the fixture starts")
    parser.add_argument("--squad", default="",
                        help="comma-separated client names present for the S10-T03b squad-target fixture "
                             "(server-driven: p1 moves away, a squad sent at the actor near p2 must reach p2, then p1 "
                             "beside it takes the target; fixture squad_party)")
    parser.add_argument("--squad-start", type=int, default=20,
                        help="with --squad: seconds after both players are in the world until the fixture starts")
    parser.add_argument("--place-giver", type=int, default=0,
                        help="hands-on aid: seconds after both players are in the world until both actors are placed "
                             "in front of Sidorovich, once (profile place_giver; 0 off)")
    parser.add_argument("--travel-barrier-hold", type=int, default=0,
                        help="S09-T04 harness: seconds the server keeps the barrier up before the take (0 off)")
    parser.add_argument("--travel-ready-delay", default="",
                        help="S09-T03 driver: <client>:<seconds>[,...]: that client sends its ready N s after "
                             "arriving on the destination level (999: never)")
    parser.add_argument("--travel-ready-wrong", default="",
                        help="S09-T03 driver: comma-separated clients that first send a ready with a wrong "
                             "transition id, then a wrong level, then the right one")
    parser.add_argument("--travel-rejoin-delay", default="",
                        help="S09-T04 driver: <client>:<seconds> the client waits after a committed travel before "
                             "it reconnects (default 2 s)")
    parser.add_argument("--travel-load-fail", default="",
                        help="S09-T04 driver: comma-separated clients that terminate themselves when the destination "
                             "world is received (a crash during the load; restart them with Run-CoopSession -Restart)")
    parser.add_argument("--restart-no-fixture", action="store_true",
                        help="with --failpoint: the restarted server runs no fixture (the clients' pending "
                             "requests are retried against the recovered world) (S08-T06)")
    parser.add_argument("--checkpoint-seconds", type=int, default=0,
                        help="seconds between the server's periodic checkpoint generations (S07-T02; 0 off; "
                             "joins and a clean stop always publish one)")
    parser.add_argument("--failpoint", default="",
                        help="S07-T06 harness: <point>[:<count>], a coop::store failpoint at which the server "
                             "terminates itself on its count-th hit after recovery (commit:before-append, "
                             "commit:after-flush, commit:after-apply, commit:after-ack, checkpoint:after-temp, "
                             "checkpoint:after-rename, checkpoint:after-manifest, checkpoint:after-rotation, "
                             "checkpoint:after-pointer, append:after-write ...); a trailing :fail makes the hit "
                             "fail the store call instead (the fail-closed path). The restart profile "
                             "(coop-restart.ltx) has no failpoint.")
    parser.add_argument("--auto-respawn", type=int, default=0,
                        help="S07-T07 driver: seconds after its own death until a client confirms the respawn by "
                             "itself (profile auto_respawn; also one early request while alive, expected refused)")
    parser.add_argument("--no-predict", default="",
                        help="comma-separated client names that do not predict their own actor (S06-T03 baseline; "
                             "profile predict = 0)")
    parser.add_argument("--protocol-build", default="proto-build-42")
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
    ledger = {n for n in args.ledger.split(",") if n}
    pickup = {n for n in args.pickup.split(",") if n}
    transfer = {n for n in args.transfer.split(",") if n}
    quest = {n for n in args.quest.split(",") if n}
    use = {n for n in args.use.split(",") if n}
    stack = {n for n in args.stack.split(",") if n}
    travel = {n for n in args.travel.split(",") if n}
    talk = {n for n in args.talk.split(",") if n}
    info = {n for n in args.info.split(",") if n}
    party_task = {n for n in args.party_task.split(",") if n}
    trade_fix = {n for n in args.trade.split(",") if n}
    upgrade_fix = {n for n in args.upgrade.split(",") if n}
    relations_fix = {n for n in args.relations.split(",") if n}
    stash_fix = {n for n in args.stash.split(",") if n}
    npc_fix = {n for n in args.npc.split(",") if n}
    loot_fix = {n for n in args.loot.split(",") if n}
    psy_fix = {n for n in args.psy.split(",") if n}
    mutant_fix = {n for n in args.mutant.split(",") if n}
    fauna_fix = {n for n in args.fauna.split(",") if n}
    companion_fix = {n for n in args.companion.split(",") if n}
    alife_fix = {n for n in args.alife.split(",") if n}
    squad_fix = {n for n in args.squad.split(",") if n}
    ready_delay = {e.split(':')[0]: int(e.split(':')[1]) for e in args.travel_ready_delay.split(',') if e}
    ready_wrong = {n for n in args.travel_ready_wrong.split(',') if n}
    rejoin_delay = {e.split(':')[0]: int(e.split(':')[1]) for e in args.travel_rejoin_delay.split(',') if e}
    load_fail = {n for n in args.travel_load_fail.split(',') if n}
    drivers = (inject | script | movement | weather | ledger | pickup | transfer | quest | use | stack | travel | talk
               | info | party_task | trade_fix | upgrade_fix | relations_fix | stash_fix | npc_fix | loot_fix | psy_fix
               | mutant_fix | fauna_fix | companion_fix | alife_fix | squad_fix)
    if drivers - set(names[1:]):
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
                      f"psi_storm = {70 if weather and args.psi_storm else 0}",
                      f"checkpoint_seconds = {args.checkpoint_seconds}",
                      f"pickup_race = {args.pickup_start if pickup else 0}", f"pickup_rounds = {args.pickup_rounds}",
                      f"transfer_race = {args.transfer_start if transfer else 0}",
                      f"use_race = {args.use_start if use else 0}",
                      f"stack_race = {args.stack_start if stack else 0}",
                      f"quest_party = {args.quest_start if quest else 0}", f"quest_holder = {args.quest_holder}",
                      f"quest_variant = {args.quest_variant}",
                      f"travel_party = {args.travel_start if travel else 0}",
                      f"travel_variant = {args.travel_variant}",
                      f"travel_barrier_hold = {args.travel_barrier_hold}",
                      f"talk_party = {args.talk_start if talk else 0}", f"talk_variant = {args.talk_variant}",
                      f"info_party = {args.info_start if info else 0}",
                      f"task_party = {args.party_task_start if party_task else 0}",
                      f"trade_party = {args.trade_start if trade_fix else 0}",
                      f"upgrade_party = {args.upgrade_start if upgrade_fix else 0}",
                      f"relations_party = {args.relations_start if relations_fix else 0}",
                      f"stash_party = {args.stash_start if stash_fix else 0}",
                      f"npc_party = {args.npc_start if npc_fix else 0}",
                      f"loot_party = {args.loot_start if loot_fix else 0}",
                      f"psy_party = {args.psy_start if psy_fix else 0}",
                      f"mutant_party = {args.mutant_start if mutant_fix else 0}",
                      f"fauna_party = {args.fauna_start if fauna_fix else 0}",
                      f"companion_party = {args.companion_start if companion_fix else 0}",
                      f"alife_party = {args.alife_start if alife_fix else 0}",
                      f"squad_party = {args.squad_start if squad_fix else 0}",
                      f"place_giver = {args.place_giver}"]
            if args.failpoint:
                # <operation>:<phase>[:<count>][:fail]; the point itself has one colon
                parts = args.failpoint.split(":")
                fail = parts[-1] == "fail"
                if fail:
                    parts = parts[:-1]
                count = int(parts[2]) if len(parts) > 2 and parts[2].isdigit() else 1
                point = ":".join(parts[:2])
                restart_lines = list(lines)
                if args.restart_no_fixture:
                    fixtures = ("pickup_race", "transfer_race", "quest_party", "use_race", "stack_race", "travel_party",
                                "talk_party", "info_party", "task_party", "trade_party", "upgrade_party",
                                "relations_party", "stash_party", "npc_party", "loot_party", "psy_party",
                                "mutant_party", "fauna_party", "companion_party", "alife_party", "squad_party")
                    restart_lines = [ln if ln.split(" = ")[0] not in fixtures else ln.split(" = ")[0] + " = 0"
                                     for ln in restart_lines]
                lines += [f"failpoint = {point}", f"failpoint_count = {count}", f"failpoint_fail = {1 if fail else 0}"]
                restart = runtime / "coop-restart.ltx"
                restart.write_text("\n".join(restart_lines) + "\n")
                profiles["server-restart"] = restart
            elif args.restart_no_fixture:
                # an external kill (Run-CoopSession -ServerKill <s>): the restart runs no fixture
                fixtures = ("pickup_race", "transfer_race", "quest_party", "use_race", "stack_race", "travel_party",
                            "talk_party", "info_party", "task_party", "trade_party", "upgrade_party",
                            "relations_party", "stash_party", "npc_party", "loot_party", "psy_party",
                            "mutant_party", "fauna_party", "companion_party", "alife_party", "squad_party")
                restart = runtime / "coop-restart.ltx"
                restart.write_text("\n".join(ln if ln.split(" = ")[0] not in fixtures else ln.split(" = ")[0] + " = 0"
                                             for ln in lines) + "\n")
                profiles["server-restart"] = restart
        else:
            stepped = transfer | quest | use | stack | travel | talk
            program = (8 if name in stepped else 7 if name in pickup else 6 if name in ledger
                       else 5 if name in weather
                       else 4 if name in encounter
                       else 3 if name in combat else 2 if name in movement else 1 if name in script else 0)
            lines += ["role = client", f"server = 127.0.0.1:{args.port}",
                      f"key = {winpath(session_dir / 'keys' / (name + '.key'))}",
                      f"name = {players[name]['display_name']}", "exit_after_seconds = 1800",
                      f"inject = {1 if name in inject else 0}",
                      f"script = {program}",
                      f"predict = {0 if name in no_predict else 1}",
                      f"auto_respawn = {args.auto_respawn}",
                      f"travel_ready_delay = {ready_delay.get(name, 0)}",
                      f"travel_ready_wrong = {1 if name in ready_wrong else 0}",
                      f"travel_rejoin_delay = {rejoin_delay.get(name, 0)}",
                      f"travel_load_fail = {1 if name in load_fail else 0}"]
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
        "checkpoint_seconds": args.checkpoint_seconds,
        "failpoint": args.failpoint,
        "pickup": sorted(pickup),
        "pickup_rounds": args.pickup_rounds if pickup else 0,
        "transfer": sorted(transfer),
        "quest": sorted(quest),
        "use": sorted(use),
        "stack": sorted(stack),
        "travel": sorted(travel),
        "travel_variant": args.travel_variant if travel else 0,
        "travel_ready_delay": ready_delay,
        "travel_ready_wrong": sorted(ready_wrong),
        "travel_barrier_hold": args.travel_barrier_hold,
        "travel_rejoin_delay": rejoin_delay,
        "travel_load_fail": sorted(load_fail),
        "talk": sorted(talk),
        "talk_variant": args.talk_variant if talk else 0,
        "info": sorted(info),
        "roster": str(roster.relative_to(workspace)),
        "roster_sha256": sha256(roster),
        "players": {name: {k: v for k, v in data.items()} for name, data in players.items()},
        "processes": {name: {"runtime": str(runtimes[name].relative_to(workspace)),
                             "fsgame": winpath(runtimes[name] / "fsgame.ltx"),
                             "profile": winpath(profiles[name]),
                             "profile_sha256": sha256(profiles[name]),
                             "profile_restart": winpath(profiles["server-restart"])
                             if name == "server" and "server-restart" in profiles else "",
                             "stop_file": winpath(stop_files[name]),
                             "inject": name in inject}
                      for name in names},
        "client_bins": client_bins,
    }
    (session_dir / "session.json").write_text(json.dumps(record, indent=2) + "\n")
    print(session_dir / "session.json")


if __name__ == "__main__":
    main()
