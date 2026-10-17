#!/usr/bin/env python3
"""Copy the verified human Tomahawk / Cybrid Achilles proof pair, never audio."""
import argparse
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
from import_original_audio import digest, safe_path, write_once
from import_original_graphics import read_members

MEMBERS = {name.lower(): Path(name).name for name in (
    'dts/TOMAHAWK.DTS', 'dat/TOMAHAWK.DAT', 'dts/ACHILLES.DTS', 'dat/ACHILLES.DAT',
    'dba/MEDIUM.DBA', 'dba/ENEMY.DBA', 'dpl/WORLD0.DPL')}


def validate(resources):
    if set(resources) != set(MEMBERS):
        raise ValueError('Missing proof-pair resource(s): ' + ', '.join(sorted(set(MEMBERS) - set(resources))))
    for key, data in resources.items():
        if len(data) < 8 or len(data) > 4 * 1024 * 1024:
            raise ValueError(f'Invalid resource size: {key}')
        if key.endswith('.dat'):
            skin = 1 if 'tomahawk' in key else 3
            if len(data) != 216 or struct.unpack_from('<H', data, 148)[0] != skin:
                raise ValueError(f'Unsupported skin binding: {key}')
        else:
            tag, size = struct.unpack_from('<II', data)
            expected = {'.dts': 0x1e0003, '.dba': 0x280001, '.dpl': 0x28000f}[Path(key).suffix]
            if tag != expected or size > len(data) - 8:
                raise ValueError(f'Unsupported resource header: {key}')


def import_models(source, output):
    source = source.resolve(strict=True)
    output = output.resolve()
    if source == output or source.is_dir() and source in output.parents:
        raise ValueError('Output must be outside the source resources')
    if source.is_file() and source.suffix.lower() == '.iso':
        if not shutil.which('7z'):
            raise ValueError('ISO input needs 7z, or supply an extracted SIMVOL0.VOL directory')
        listing = subprocess.check_output(['7z', 'l', '-slt', str(source)], text=True)
        candidates = [line[7:] for line in listing.split('----------', 1)[-1].splitlines()
                      if line.startswith('Path = ') and safe_path(line[7:]).name.lower() == 'simvol0.vol']
        if len(candidates) != 1:
            raise ValueError('Expected exactly one SIMVOL0.VOL on the disc')
        with tempfile.TemporaryDirectory(prefix='openearthsiege-models-') as temporary:
            subprocess.run(['7z', 'x', '-y', '-bd', '-bso0', '-bsp0', f'-o{temporary}', str(source), candidates[0]], check=True)
            resources, archive_hash = read_members(Path(temporary) / safe_path(candidates[0]), MEMBERS)
    else:
        resources, archive_hash = read_members(source, MEMBERS)
    validate(resources)
    # Preflight the complete set before writing anything. No original is modified.
    for key, name in MEMBERS.items():
        target = output / name
        if target.is_symlink() or target.exists() and target.read_bytes() != resources[key]:
            raise ValueError(f'Refusing to replace existing resource: {target}')
    for key, name in MEMBERS.items():
        write_once(output / name, resources[key])
    manifest = {'source': str(source), 'archive_sha256': archive_hash,
                'files': [{'resource': key, 'file': name, 'sha256': digest(resources[key])}
                          for key, name in MEMBERS.items()]}
    if not (output / 'import.json').exists():
        write_once(output / 'import.json', (json.dumps(manifest, indent=2) + '\n').encode())
    print(f'Original HERC proof pair: {output} (7 unchanged resources; audio untouched)')
    print('Verify decoding: build/model_tests ' + str(output))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='ISO, installation, SIMVOL0.VOL, or extracted resources')
    parser.add_argument('--output', type=Path, default=Path('assets/original/models'))
    args = parser.parse_args()
    try:
        import_models(args.source, args.output)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Model import failed: {error}\n')
