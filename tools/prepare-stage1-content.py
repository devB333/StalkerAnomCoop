"""Copy and hash an independent Stage 1 fixture; never launch the original game."""
import hashlib
import json
import ntpath
from pathlib import Path
import shutil
import subprocess

workspace = Path(__file__).resolve().parents[1]
original = workspace.parent / 'StalkerAnom'
destination = workspace / 'builds/test-install/baseline'
manifest_path = workspace / 'docs/content-manifest.json'
if destination.exists() or manifest_path.exists():
    raise SystemExit('Fixture or manifest already exists; preserve it and inspect before retrying.')
if original.resolve() == destination.resolve() or not destination.resolve().is_relative_to(workspace):
    raise SystemExit('Unsafe destination.')

files = list((original / 'db').rglob('*'))
files += list((original / 'gamedata').rglob('*'))
files += [p for p in (original / 'bin').iterdir() if p.suffix.lower() in ('.dll', '.ini') or p.name == 'AnomalyDX11.exe']
files += list(original.glob('*.mhr')) + list(original.glob('changes*.txt'))
files += [original / 'fsgame.ltx']
for path in files:
    if path.is_symlink() or not path.resolve().is_relative_to(original.resolve()):
        raise SystemExit(f'Linked source path requires inspection: {path}')
files = sorted(p for p in files if p.is_file())
total = sum(p.stat().st_size for p in files)
if shutil.disk_usage(workspace).free < total + 10 * 1024**3:
    raise SystemExit('Insufficient free space for independent content and build reserve.')

destination.mkdir(parents=True)
records = []
for index, source in enumerate(files, 1):
    relative = source.relative_to(original)
    target = destination / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    before = source.stat()
    source_hash = hashlib.sha256()
    with source.open('rb') as reader, target.open('xb') as writer:
        for chunk in iter(lambda: reader.read(4 * 1024**2), b''):
            source_hash.update(chunk)
            writer.write(chunk)
    after = source.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise RuntimeError(f'Source changed during copy: {relative}')
    with target.open('rb') as reader:
        target_hash = hashlib.file_digest(reader, 'sha256').hexdigest()
    if source_hash.hexdigest() != target_hash or source.samefile(target):
        raise RuntimeError(f'Copy verification failed: {relative}')
    records.append({'path': relative.as_posix(), 'bytes': before.st_size,
                    'source_sha256': source_hash.hexdigest(), 'copy_sha256': target_hash})
    if index % 10 == 0 or index == len(files):
        print(f'Copied and verified {index}/{len(files)} files', flush=True)

windows_root = subprocess.check_output(['wslpath', '-w', str(destination)], text=True).strip()
config = destination / 'fsgame.ltx'
lines = config.read_text().splitlines()
for index, line in enumerate(lines):
    if line.strip().startswith('$app_data_root$'):
        lines[index] = f'$app_data_root$ = true | false | {windows_root}\\appdata\\'
config.write_text('\n'.join(lines) + '\n')
aliases = {'$fs_root$': windows_root}
for line in lines:
    line = line.split(';', 1)[0].strip()
    if not line:
        continue
    name, value = line.split('=', 1)
    fields = [field.strip() for field in value.split('|')]
    root = aliases.get(fields[2], fields[2])
    if root.startswith('$'):
        raise RuntimeError(f'Unknown filesystem alias: {root}')
    resolved = ntpath.normpath(ntpath.join(root, fields[3] if len(fields) > 3 else ''))
    if ntpath.commonpath([windows_root, resolved]).lower() != windows_root.lower():
        raise RuntimeError(f'Filesystem alias escapes fixture: {name}')
    aliases[name.strip()] = resolved
for name in ('appdata', 'gamedata'):
    (destination / name).mkdir(exist_ok=True)

manifest = {'schema_version': 1, 'original': str(original), 'fixture': str(destination),
            'fixture_windows': windows_root, 'copied_files': len(files), 'copied_bytes': total,
            'files': records, 'filesystem_aliases': aliases,
            'modified_fsgame_sha256': hashlib.sha256(config.read_bytes()).hexdigest(),
            'verification': 'Independent copies; each source/copy SHA-256 matched before fsgame routing edit.',
            'runtime_validation': 'Not run'}
manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
print(f'Fixture prepared: {destination}; {total} bytes; all aliases stay inside fixture.', flush=True)
