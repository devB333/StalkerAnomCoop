"""Assemble a runnable source-built fixture using only workspace content."""
import hashlib
import json
import ntpath
from pathlib import Path
import shutil
import subprocess

workspace = Path(__file__).resolve().parents[1]
source = workspace / 'builds/engine-work'
baseline = workspace / 'builds/test-install/baseline'
runtime = source / '_build/_game'
binary_dir = runtime / 'bin_dbg'
executable = binary_dir / 'AnomalyDX11.exe'
manifest_path = workspace / 'docs/runtime-manifest.json'
previous = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
assert executable.is_file() and (baseline / 'db').is_dir()
assert runtime.resolve().is_relative_to(workspace)

def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def windows_path(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()

records = []

def copy_verified(original, target):
    if original.is_symlink() or target.is_symlink():
        raise RuntimeError(f'Linked runtime file requires inspection: {target}')
    expected = sha256(original)
    if target.exists() and sha256(target) != expected:
        raise RuntimeError(f'Preserving different existing file: {target}')
    if not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)
    assert sha256(target) == expected and not original.samefile(target)
    records.append({'source': str(original.relative_to(workspace)),
                    'destination': str(target.relative_to(workspace)), 'sha256': expected})

for dll in sorted((source / 'sdk/binaries').glob('*.dll')):
    copy_verified(dll, binary_dir / dll.name)
for asset in sorted((source / 'gamedata').rglob('*')):
    if asset.is_file():
        copy_verified(asset, runtime / 'gamedata' / asset.relative_to(source / 'gamedata'))
for asset in sorted(baseline.glob('*.mhr')):
    copy_verified(asset, runtime / asset.name)
copy_verified(baseline / 'bin/alsoft.ini', binary_dir / 'alsoft.ini')

# Archives are scanned while the aliases are parsed. Define $fs_root$ first so
# their virtual gamedata paths are absolute before $arch_dir_configs$ is visited.
config = runtime / 'fsgame.ltx'
if config.exists() and sha256(config) != previous.get('configuration_sha256'):
    raise RuntimeError('Runtime fsgame.ltx changed outside packaging; preserve and inspect it.')
lines = [f'$fs_root$ = false | false | {windows_path(runtime)}\\']
for line in (baseline / 'fsgame.ltx').read_text().splitlines():
    alias = line.split('=', 1)[0].strip()
    if alias == '$app_data_root$':
        line = f'$app_data_root$ = true | false | {windows_path(runtime)}\\appdata\\'
    elif alias == '$arch_dir$':
        line = f'$arch_dir$ = false | false | {windows_path(baseline)}\\db\\'
    lines.append(line)
aliases = {}
workspace_windows = windows_path(workspace)
for line in lines:
    line = line.split(';', 1)[0].strip()
    if not line:
        continue
    name, value = line.split('=', 1)
    fields = [field.strip() for field in value.split('|')]
    root = aliases.get(fields[2], fields[2])
    if root.startswith('$'):
        raise RuntimeError(f'Filesystem alias used before definition: {root}')
    resolved = ntpath.normpath(ntpath.join(root, fields[3] if len(fields) > 3 else ''))
    if ntpath.commonpath([workspace_windows, resolved]).lower() != workspace_windows.lower():
        raise RuntimeError(f'Filesystem alias escapes workspace: {name}')
    aliases[name.strip()] = resolved
config.write_text('\n'.join(lines) + '\n')
for directory in ('appdata', 'tmp', 'gamedata'):
    (runtime / directory).mkdir(exist_ok=True)
launcher = runtime / 'Launch-Anomaly.cmd'
launcher.write_bytes(b'@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\nif not exist "tmp" mkdir "tmp"\r\nset "TEMP=%CD%\\tmp"\r\nset "TMP=%CD%\\tmp"\r\n"bin_dbg\\AnomalyDX11.exe" -fsltx fsgame.ltx\r\nexit /b %errorlevel%\r\n')
revision = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
manifest = {'source_revision': revision, 'executable': str(executable.relative_to(workspace)),
            'executable_sha256': sha256(executable), 'runtime_files': records,
            'content_archives': str((baseline / 'db').relative_to(workspace)),
            'configuration': str(config.relative_to(workspace)),
            'configuration_sha256': sha256(config), 'filesystem_aliases': aliases,
            'appdata': str((runtime / 'appdata').relative_to(workspace)),
            'launcher': str(launcher.relative_to(workspace)),
            'content_overlay': 'Pinned source gamedata, as loose files; baseline archives preserved.',
            'startup_test': 'Pending after packaging'}
manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
print(f'Packaged and verified {len(records)} runtime files; all {len(aliases)} aliases stay in workspace.')
print(f'Launch with {launcher}')
