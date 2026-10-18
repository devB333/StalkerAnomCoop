"""Restore the pinned distribution overlay in build_2, preserving diagnostics."""
from pathlib import Path
import datetime
import hashlib
import json
import ntpath
import shutil
import subprocess

workspace = Path(__file__).resolve().parents[1]
source = workspace / 'builds/build_1/engine-work'
runtime = workspace / 'builds/build_2/test-install/source-client'
revision = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
assert revision == 'c57fe5b9fab4fe0506659db03137c52d2500eb2f'
assert not subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'], text=True).strip()

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()

# Refuse to write during gameplay or silently replace changed distribution files.
command = "@(Get-CimInstance Win32_Process -Filter \"Name LIKE 'Anomaly%exe'\").Count"
count = subprocess.check_output(['powershell.exe', '-NoLogo', '-NoProfile', '-NonInteractive', '-Command', command], text=True).strip()
if count != '0':
    raise RuntimeError('Close Anomaly before applying the runtime repair.')
assets = []
for original in sorted((source / 'gamedata').rglob('*')):
    if original.is_file():
        target = runtime / 'gamedata' / original.relative_to(source / 'gamedata')
        if original.is_symlink() or target.is_symlink():
            raise RuntimeError(f'Linked file requires inspection: {target}')
        expected = digest(original)
        if target.exists() and digest(target) != expected:
            raise RuntimeError(f'Preserving modified asset: {target}')
        assets.append((original, target, expected))
config = runtime / 'fsgame.ltx'
lines = [f'$fs_root$ = false | false | {win(runtime)}\\']
lines += [line for line in config.read_text().splitlines() if line.split('=', 1)[0].strip() != '$fs_root$']
aliases = {}
workspace_windows = win(workspace)
for line in lines:
    line = line.split(';', 1)[0].strip()
    if not line:
        continue
    name, value = line.split('=', 1)
    fields = [part.strip() for part in value.split('|')]
    base = aliases.get(fields[2], fields[2])
    if base.startswith('$'):
        raise RuntimeError(f'Undefined alias: {base}')
    resolved = ntpath.normpath(ntpath.join(base, fields[3] if len(fields) > 3 else ''))
    if ntpath.commonpath([workspace_windows, resolved]).lower() != workspace_windows.lower():
        raise RuntimeError(f'Alias outside workspace: {resolved}')
    aliases[name.strip()] = resolved

stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
backup = workspace / 'builds/build_2/repair-backups' / stamp
backup.mkdir(parents=True, exist_ok=False)
for relative in ['fsgame.ltx', 'Launch-Anomaly.cmd', 'appdata/user.ltx', 'appdata/logs/xray_djb20.log']:
    path = runtime / relative
    if path.is_file():
        dest = backup / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, dest)
saves = {str(p.relative_to(runtime)): digest(p) for p in (runtime / 'appdata/savedgames').rglob('*') if p.is_file()}
exe_hash = digest(runtime / 'bin/AnomalyDX11.exe')
records = []
for original, target, expected in assets:
    copied = not target.exists()
    if copied:
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)
    assert digest(target) == expected
    records.append({'path': str(target.relative_to(runtime)), 'sha256': expected, 'added': copied})
config.write_text('\n'.join(lines) + '\n')
cache = runtime / 'appdata/shaders_cache'
if cache.exists():
    cache.rename(backup / 'shaders_cache')
(runtime / 'tmp').mkdir(exist_ok=True)
launcher = runtime / 'Launch-Anomaly.cmd'
launcher.write_bytes(b'@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\nset "TEMP=%CD%\\tmp"\r\nset "TMP=%TEMP%"\r\n"bin\\AnomalyDX11.exe" -fsltx fsgame.ltx\r\nexit /b %errorlevel%\r\n')
assert all(digest(runtime / p) == h for p, h in saves.items())
assert digest(runtime / 'bin/AnomalyDX11.exe') == exe_hash
manifest = {'recorded_utc': stamp, 'source_revision': revision,
    'runtime': str(runtime.relative_to(workspace)), 'backup': str(backup.relative_to(workspace)),
    'executable_sha256': exe_hash, 'source_provenance_note': 'Overlay from pinned candidate; user-created executable source provenance remains unverified.',
    'files': records, 'filesystem_aliases': aliases, 'saves_preserved': saves,
    'configuration_sha256': digest(config), 'launcher_sha256': digest(launcher),
    'runtime_retest': 'Pending'}
(backup / 'repair-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps({'backup': str(backup), 'added': sum(r['added'] for r in records), 'verified': len(records), 'saves_preserved': len(saves), 'aliases_verified': len(aliases)}, indent=2))
