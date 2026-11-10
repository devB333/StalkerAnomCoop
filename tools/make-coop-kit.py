#!/usr/bin/env python3
"""S11-T00: the co-op kits for playing over Tailscale from the main menu (Host / Join).

Usage:
  python3 tools/make-coop-kit.py server <name> [--protocol-build proto-build-N]
  python3 tools/make-coop-kit.py host <name> [--player "Your name"] [--protocol-build proto-build-N]
  python3 tools/make-coop-kit.py client <name>

server: a dedicated server on this PC (no game played here): <kit folder>/<name>/ with Start-Server.cmd (prints
        the Tailscale address and a join code), Start-Server-Continue.cmd (the last campaign) and Stop-Server.cmd.
        Both players use client kits and New game > Join co-op; the first to join owns the session (New game,
        Load).

host:   on this PC (the workspace): <kit folder>/<name>/ with a server runtime (the current campaign server build)
        and a client runtime whose app data coop-menu.ltx can host (New game > Host co-op). The server's roster
        starts empty of friends: a friend gets in with the join code the menu shows. Start the game with
        <kit folder>/<name>/Start-Coop.cmd.
client: for a friend's PC: <kit folder>/<name>/ (and <name>.zip) with the client build, the runtime's loose files
        and Setup-Coop.ps1. On the friend's PC, unzip anywhere, run Setup-Coop.ps1 once (it asks for the Anomaly
        1.5.3 folder and the player's name, writes fsgame.ltx and coop-menu.ltx), then Start-Coop.cmd; in the
        game: New game > Join co-op, Host = the host's Tailscale address, Password = the join code.
Both sides need the same build (this workspace's current client build) and an unmodified Anomaly 1.5.3: the
server refuses a different build or different game files (the build and content identity).
"""

import argparse
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

workspace = Path(__file__).resolve().parent.parent
accepted = workspace / "builds/build_2/test-install/source-client"
built_client = workspace / "builds/build_1/engine-work/_build/_game/bin_dbg"
server_exe = workspace / "builds/stage2/campaign/_game/bin_dbg/AnomalyCampaignServer.exe"


def win(p: Path) -> str:
    s = str(p)
    m = re.match(r"^/mnt/([a-z])/(.*)$", s)
    return (m.group(1).upper() + ":\\" + m.group(2).replace("/", "\\")) if m else s


def client_files(dest: Path) -> None:
    """The client runtime's own files: bin (the accepted runtime's DLLs, the built client and GNS), the loose
    gamedata, the HRTF files and the accepted user settings (fullscreen as installed)."""
    (dest / "bin").mkdir(parents=True)
    for item in (accepted / "bin").iterdir():
        if item.is_file() and item.name not in ("AnomalyDX11.exe", "AnomalyDX11.pdb"):
            shutil.copyfile(item, dest / "bin" / item.name)
    for item in ("AnomalyDX11.exe", "GameNetworkingSockets.dll"):
        shutil.copyfile(built_client / item, dest / "bin" / item)
    shutil.copytree(accepted / "gamedata", dest / "gamedata")
    for item in accepted.glob("*.mhr"):
        shutil.copyfile(item, dest / item.name)
    for folder in ("appdata/logs", "appdata/savedgames", "tmp"):
        (dest / folder).mkdir(parents=True, exist_ok=True)  # savedgames: the world a client receives is saved there
    settings = [ln for ln in (accepted / "appdata/user.ltx").read_text(errors="replace").splitlines()]
    (dest / "appdata/user.ltx").write_text("\n".join(settings) + "\n")


FSGAME = """$fs_root$ = false | false | {root}\\
$app_data_root$ = true | false | {root}\\appdata\\
$arch_dir$              = false\t| false\t| {db}\\
$arch_dir_configs$\t\t= false\t| false\t| $arch_dir$\t | configs\\
$arch_dir_maps$         = false\t| false\t| $arch_dir$\t | levels\\
$arch_dir_meshes$       = false\t| false\t| $arch_dir$\t | meshes\\
$arch_dir_sounds$\t\t= false\t| false\t| $arch_dir$\t | sounds\\
$arch_dir_textures$\t\t= false\t| false\t| $arch_dir$\t | textures\\
$arch_dir_resource$     = false\t| false\t| $arch_dir$\t | mods\\
$arch_dir_patches$\t\t= false\t| true \t| $arch_dir$\t | patches\\
$arch_dir_addons$\t\t= false\t| true \t| $arch_dir$\t | addons\\
"""


def fsgame_rest() -> str:
    """Everything of the accepted runtime's fsgame.ltx from $game_data$ on ($fs_root$-relative: the kit's folder)."""
    text = (accepted / "fsgame.ltx").read_text(errors="replace")
    return text[text.index("$game_data$"):]


SETUP = r"""# S11-T00: set up this co-op client kit on this PC (run once; again after moving the folder).
#   powershell -ExecutionPolicy Bypass -File Setup-Coop.ps1 [-Anomaly 'D:\Games\Anomaly'] [-Name 'Your name']
param([string]$Anomaly = '', [string]$Name = '')
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
foreach ($folder in @('appdata\logs', 'appdata\savedgames', 'tmp')) {
    New-Item -ItemType Directory (Join-Path $root $folder) -Force | Out-Null  # a zip keeps no empty folders
}
while (-not $Anomaly -or -not (Test-Path (Join-Path $Anomaly 'db\configs\configs.db0'))) {
    $Anomaly = Read-Host 'The Anomaly 1.5.3 folder (the one with AnomalyLauncher.exe and db\)'
}
while (-not $Name -or $Name.Length -gt 32 -or $Name -notmatch '^[ -~]+$') {
    $Name = Read-Host 'Your player name (1-32 letters)'
}
$fs = (Get-Content (Join-Path $root 'fsgame.template.ltx') -Raw).Replace('{root}', $root)
$fs = $fs.Replace('{db}', (Join-Path $Anomaly 'db'))
Set-Content -Path (Join-Path $root 'fsgame.ltx') -Value $fs -Encoding ASCII
$menu = @(
    '[coop_menu]',
    "name = $Name",
    ('key = ' + (Join-Path $root 'appdata\player.key')),
    ('client_log = ' + (Join-Path $root 'appdata\logs\coop-client.log')),
    ('client_stop = ' + (Join-Path $root 'appdata\coop-stop.txt'))
)
$extra = Join-Path $root 'coop-menu.host.ltx'
if (Test-Path $extra) { $menu += (Get-Content $extra) }
Set-Content -Path (Join-Path $root 'appdata\coop-menu.ltx') -Value $menu -Encoding ASCII
$cmd = '@echo off' + "`r`n" + 'cd /d "%~dp0"' + "`r`n"
# no -fsltx: the engine finds fsgame.ltx beside bin by itself (it would take a quoted path's quotes literally)
$cmd += 'start "" "%~dp0bin\AnomalyDX11.exe"' + "`r`n"
Set-Content -Path (Join-Path $root 'Start-Coop.cmd') -Value $cmd -Encoding ASCII
Write-Host "Ready. Start the game with Start-Coop.cmd; in the game: New game > Join co-op (or Host)."
"""

README = """Anomaly co-op ({kind} kit, build {build})

{steps}

You need:
- An unmodified Anomaly 1.5.3 install (no addons): the host's server refuses different game files.
- The same kit build as the other player (made from the same co-op build).
- Tailscale on both PCs, signed in to the same tailnet (https://tailscale.com/download).

In the game: New game asks Single player / Host co-op / Join co-op.
- Host co-op (host only): a new campaign (the standard creation screen) or the last campaign. The server
  starts on the host's PC in the background (about a minute) and a box shows the address and join code to
  give the friend. Quitting the host's game stops the server.
- Join co-op: Host = the host's address (its Tailscale address, like 100.101.102.103), Password = the join
  code (like ABCDE-23456). The first join adds your key to the host's list: later joins need no new code.
"""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("kind", choices=["server", "host", "client"])
    ap.add_argument("name")
    ap.add_argument("--player", default="")
    ap.add_argument("--protocol-build", default="proto-build-63")
    ap.add_argument("--dist", action="store_true",
                    help="the distribution kits in Kits\\ (only after the final test, plan.md); default builds\\kits\\")
    a = ap.parse_args()
    if not re.fullmatch(r"[a-zA-Z0-9-]+", a.name):
        raise SystemExit("Kit name must be alphanumeric with optional hyphens")
    # the user (2026-11-02): Kits\ at the workspace root, out of Git (generated binaries): Kits\clientKit\ the
    # client kit every joining player uses, Kits\client1HostKit\ the kit of a player who hosts,
    # Kits\ServerHostKit\ a dedicated server's
    # plan.md "Note: distribution kits": Kits\ only after the final test (--dist); test kits in builds\kits\
    folder = {"client": "clientKit", "host": "client1HostKit", "server": "ServerHostKit"}[a.kind]
    kit = (workspace / "Kits" if a.dist else workspace / "builds/kits") / folder / a.name
    if kit.exists():
        raise SystemExit(f"Preserving existing kit: {kit}")
    build = subprocess.run(["sha256sum", str(built_client / "AnomalyDX11.exe")], capture_output=True,
                           text=True).stdout.split()[0][:12]
    if a.kind == "client":
        client_files(kit)
        (kit / "fsgame.template.ltx").write_text(FSGAME + fsgame_rest())
        (kit / "Setup-Coop.ps1").write_text(SETUP)
        steps = ("1. Unzip this folder anywhere (not inside the Anomaly folder).\n"
                 "2. Right-click Setup-Coop.ps1 > Run with PowerShell (or: powershell -ExecutionPolicy Bypass -File "
                 "Setup-Coop.ps1); give it your Anomaly folder and your name.\n"
                 "3. Start the game with Start-Coop.cmd.")
        (kit / "README.txt").write_text(README.format(kind="client", build=build, steps=steps))
        archive = kit.parent / (a.name + ".zip")
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
            for f in sorted(kit.rglob("*")):
                if f.is_file():
                    z.write(f, Path(a.name) / f.relative_to(kit))
        print(f"client kit: {kit}\nzip: {archive} ({archive.stat().st_size // (1024 * 1024)} MB)")
        return 0
    if a.kind == "server":
        run = "kit-" + a.name
        subprocess.run([sys.executable, str(workspace / "tools/prepare-coop-menu.py"), run, "--task", "S11-T00",
                        "--protocol-build", a.protocol_build, "--dedicated"], check=True, stdout=subprocess.DEVNULL)
        server = workspace / "builds/stage2/test-install/S11-T00" / f"{run}-server"
        kit.mkdir(parents=True)
        ps = "@echo off\r\npowershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "
        start = ps + f'"{win(workspace / "tools/Host-CoopServer.ps1")}" -Runtime "{win(server)}"'
        (kit / "Start-Server.cmd").write_text(start + "\r\npause\r\n")
        (kit / "Start-Server-Continue.cmd").write_text(start + " -Continue\r\npause\r\n")
        (kit / "Stop-Server.cmd").write_text(
            ps + f'"{win(workspace / "tools/Stop-CoopServer.ps1")}" -Runtime "{win(server)}"\r\npause\r\n')
        steps = ("This PC is the server only. Start-Server.cmd starts it on its starting campaign and prints the "
                 "address and join code; Start-Server-Continue.cmd starts the last campaign played; Stop-Server.cmd "
                 "stops it cleanly (it saves first). Both players: client kit > New game > Join co-op. The first "
                 "player to join owns the session: New game there starts a new campaign for both, Load switches.")
        (kit / "README.txt").write_text(README.format(kind="server", build=build, steps=steps))
        print(f"server kit: {kit}\nstart: {win(kit / 'Start-Server.cmd')}\nruntime: {win(server)}")
        return 0
    # host: a server runtime and a client runtime on this PC, from a prepared menu session
    run = "kit-" + a.name
    subprocess.run([sys.executable, str(workspace / "tools/prepare-coop-menu.py"), run, "--task", "S11-T00",
                    "--protocol-build", a.protocol_build], check=True, stdout=subprocess.DEVNULL)
    session = workspace / "builds/stage5/S11-T00" / run
    server = workspace / "builds/stage2/test-install/S11-T00" / f"{run}-server"
    client = kit / "client"
    client_files(client)
    (client / "fsgame.ltx").write_text((FSGAME + fsgame_rest()).replace("{root}", win(client))
                                       .replace("{db}", win(accepted / "db")))
    player = a.player or "Host"
    menu = ["[coop_menu]", f"name = {player}", f"key = {win(client / 'appdata/player.key')}",
            f"client_log = {win(client / 'appdata/logs/coop-client.log')}",
            f"client_stop = {win(client / 'appdata/coop-stop.txt')}",
            f"server_exe = {win(server_exe)}", f"server_launcher = {win(workspace / 'tools/Start-CoopServer.ps1')}",
            f"server_runtime = {win(server)}"]
    (client / "appdata/coop-menu.ltx").write_text("\n".join(menu) + "\n")
    # the prepared session's player 1 is not this kit's player: the roster starts with no one (the host's own key
    # is added by its menu at the first Host, friends by the join code)
    roster = session / "roster.txt"
    roster.write_text("".join(ln + "\n" for ln in roster.read_text().splitlines() if not ln.startswith("player ")))
    # no -fsltx: the engine finds client\fsgame.ltx beside its bin folder (a quoted -fsltx path keeps its quotes)
    (kit / "Start-Coop.cmd").write_text('@echo off\r\ncd /d "' + win(client) + '"\r\nstart "" "' +
                                        win(client / "bin/AnomalyDX11.exe") + '"\r\n')
    steps = ("This PC hosts. Start the game with Start-Coop.cmd. The server runtime is " + win(server) + ".")
    (kit / "README.txt").write_text(README.format(kind="host", build=build, steps=steps))
    print(f"host kit: {kit}\nstart: {win(kit / 'Start-Coop.cmd')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
