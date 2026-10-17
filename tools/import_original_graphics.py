#!/usr/bin/env python3
"""Import only the verified Tomahawk cockpit and palette; never touch audio."""
import argparse
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
from import_original_audio import digest, safe_path, vol_entries, write_once

MEMBERS = {'hb0/tomahawk.hb0': 'TOMAHAWK.HB0', 'dpl/cockpit.dpl': 'COCKPIT.DPL'}


def read_members(source, members=MEMBERS):
    if source.is_file():
        data = source.read_bytes()
        return {p.as_posix().lower(): payload for p, flag, payload, _, _ in vol_entries(data)
                if p.as_posix().lower() in members and flag == 2}, digest(data)
    found = {}
    for p in source.rglob('*'):
        if p.is_file() and p.parent.name.lower() + '/' + p.name.lower() in members:
            if p.is_symlink():
                raise ValueError(f'Refusing symlink source: {p}')
            key = p.parent.name.lower() + '/' + p.name.lower()
            if key in found:
                raise ValueError('Ambiguous resources; pass the specific SIMVOL0.VOL directory')
            found[key] = p.read_bytes()
    if found:
        return found, None
    volumes = [p for p in source.rglob('*') if p.is_file() and p.name.lower() == 'simvol0.vol']
    if len(volumes) != 1:
        raise ValueError('Expected one SIMVOL0.VOL; pass its path or its extracted resource directory')
    return read_members(volumes[0], members)


def validate(members):
    if set(members) != set(MEMBERS):
        raise ValueError('Missing supported cockpit/palette members')
    b = members['hb0/tomahawk.hb0']
    if (len(b) != 307234 or b[:4] != bytes.fromhex('01002800')
            or struct.unpack_from('<II', b, 4) != (307226, 1)
            or b[12:16] != bytes.fromhex('0e002800')
            or struct.unpack_from('<IHHBBB I H'.replace(' ', ''), b, 16)
            != (307213, 480, 640, 8, 0, 0, 307200, 0)):
        raise ValueError('Unsupported Tomahawk bitmap layout')
    p = members['dpl/cockpit.dpl']
    if (len(p) != 1040 or p[:4] != bytes.fromhex('0f002800')
            or struct.unpack_from('<II', p, 4) != (1032, 256)
            or any(p[12+i*4+c] > 63 for i in range(256) for c in range(3))):
        raise ValueError('Unsupported cockpit palette layout')


def import_graphics(source, destination):
    source = source.resolve(strict=True)
    destination = destination.resolve()
    if destination == source or source.is_dir() and source in destination.parents:
        raise ValueError('Destination must be outside the original resources')
    provenance = {'source': str(source)}
    if source.is_file() and source.suffix.lower() == '.iso':
        if not shutil.which('7z'):
            raise ValueError('ISO input needs 7z; alternatively supply extracted SIMVOL0.VOL')
        listing = subprocess.check_output(['7z', 'l', '-slt', str(source)], text=True)
        candidates = [line[7:] for line in listing.split('----------', 1)[-1].splitlines()
                      if line.startswith('Path = ') and safe_path(line[7:]).name.lower() == 'simvol0.vol']
        if len(candidates) != 1:
            raise ValueError('Expected one SIMVOL0.VOL on the disc')
        with tempfile.TemporaryDirectory(prefix='openearthsiege-graphics-') as temporary:
            subprocess.run(['7z', 'x', '-y', '-bd', '-bso0', '-bsp0', f'-o{temporary}',
                            str(source), candidates[0]], check=True)
            members, archive_hash = read_members(Path(temporary) / safe_path(candidates[0]))
    else:
        members, archive_hash = read_members(source)
    validate(members)
    provenance['archive_sha256'] = archive_hash
    provenance['files'] = [{'resource': name, 'file': target, 'sha256': digest(members[name])}
                           for name, target in MEMBERS.items()]
    # Preflight all outputs so a conflicting local file cannot cause partial replacement.
    for name, target in MEMBERS.items():
        path = destination / target
        if path.exists() and path.read_bytes() != members[name]:
            raise ValueError(f'Refusing to overwrite different data: {path}')
    for name, target in MEMBERS.items():
        write_once(destination / target, members[name])
    manifest = destination / 'import.json'
    if not manifest.exists():
        write_once(manifest, (json.dumps(provenance, indent=2) + '\n').encode())
    print(f'Original Tomahawk cockpit ready: {destination} (2 files; audio untouched)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='ISO, installation, SIMVOL0.VOL, or extracted resources')
    parser.add_argument('--output', type=Path, default=Path('assets/original'))
    args = parser.parse_args()
    try:
        import_graphics(args.source, args.output)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Graphics import failed: {error}\n')
