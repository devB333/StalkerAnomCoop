"""Prepare independent profiles for Stage 1 manual gameplay comparisons."""
from pathlib import Path
import hashlib
import json
import ntpath
import subprocess

workspace = Path(__file__).resolve().parents[1]

def win(path):
    return subprocess.check_output(['wslpath', '-w', str(path)], text=True).strip()

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def create(path, data):
    if path.exists() and path.read_bytes() != data:
        raise RuntimeError(f'Preserving changed file: {path}')
    if not path.exists():
        path.write_bytes(data)

fixtures = []
for name, relative, binary in (
    ('candidate', 'builds/build_2/test-install/source-client', 'bin/AnomalyDX11.exe'),
    ('installed-copy', 'builds/build_1/test-install/baseline', 'bin/AnomalyDX11.exe'),
):
    root = workspace / relative
    exe = root / binary
    assert exe.is_file()
    profile = root / 'appdata-stage1'
    temp = root / 'tmp-stage1'
    profile.mkdir(exist_ok=True)
    temp.mkdir(exist_ok=True)
    lines = [f'$fs_root$ = false | false | {win(root)}\\']
    for line in (root / 'fsgame.ltx').read_text().splitlines():
        alias = line.split('=', 1)[0].strip()
        if alias == '$fs_root$':
            continue
        if alias == '$app_data_root$':
            line = f'$app_data_root$ = true | false | {win(profile)}\\'
        lines.append(line)
    aliases = {}
    for line in lines:
        line = line.split(';', 1)[0].strip()
        if not line:
            continue
        alias, value = line.split('=', 1)
        fields = [part.strip() for part in value.split('|')]
        base = aliases.get(fields[2], fields[2])
        if base.startswith('$'):
            raise RuntimeError(f'Undefined alias: {base}')
        resolved = ntpath.normpath(ntpath.join(base, fields[3] if len(fields) > 3 else ''))
        if ntpath.commonpath([win(workspace), resolved]).lower() != win(workspace).lower():
            raise RuntimeError(f'Path outside workspace: {resolved}')
        aliases[alias.strip()] = resolved
    config = root / 'fsgame-stage1.ltx'
    create(config, ('\n'.join(lines) + '\n').encode())
    launcher = root / 'Launch-Stage1-Validation.cmd'
    command = binary.replace('/', '\\')
    create(launcher, ('@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\n'
        'set "TEMP=%CD%\\tmp-stage1"\r\nset "TMP=%TEMP%"\r\n'
        f'"{command}" -fsltx fsgame-stage1.ltx\r\nexit /b %errorlevel%\r\n').encode())
    fixtures.append({'name': name, 'root': relative,
        'executable': str(exe.relative_to(workspace)), 'executable_sha256': digest(exe),
        'configuration': str(config.relative_to(workspace)), 'configuration_sha256': digest(config),
        'launcher': str(launcher.relative_to(workspace)), 'launcher_sha256': digest(launcher),
        'profile': str(profile.relative_to(workspace)), 'aliases': aliases,
        'runtime_validation': 'Pending; configuration prepared only'})
output = workspace / 'docs/stage1-fixtures-manifest.json'
output.write_text(json.dumps({'fixtures': fixtures}, indent=2) + '\n')
print('Prepared candidate and installed-copy launchers with independent appdata-stage1 profiles.')
