#!/usr/bin/env python3
"""Read-only ES2 ISO/VOLN importer. See docs/original-audio.md for format sources."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import shutil
import struct
import subprocess
import wave


def digest(data):
    return hashlib.sha256(data).hexdigest()


def safe_path(name):
    p = PurePosixPath(name.replace('\\', '/'))
    if p.is_absolute() or '..' in p.parts or ':' in name:
        raise ValueError(f'Unsafe resource path: {name!r}')
    return Path(*p.parts)


def write_once(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if path.read_bytes() != data:
            raise ValueError(f'Refusing to overwrite different data: {path}')
    else:
        with path.open('xb') as f:
            f.write(data)


def vol_entries(data):
    if data[:4] != b'VOLN' or len(data) < 18:
        raise ValueError('Not a supported VOLN archive')
    folder_bytes = struct.unpack_from('<H', data, 10)[0]
    if 18 + folder_bytes > len(data):
        raise ValueError('Truncated folder table')
    folders = data[12:12 + folder_bytes].decode('ascii').split('\0')
    count, table_bytes = struct.unpack_from('<HI', data, 12 + folder_bytes)
    start = 18 + folder_bytes
    end = start + count * 18
    if table_bytes != count * 18 or end > len(data):
        raise ValueError('Unsupported or truncated VOLN directory')
    for i in range(count):
        raw_name, folder, offset = struct.unpack_from('<13sBI', data, start + i * 18)
        name = raw_name.split(b'\0')[0].decode('ascii')
        if not name or folder >= len(folders) or offset < end or offset + 9 > len(data):
            raise ValueError('Invalid VOLN entry')
        flag, length, stored = struct.unpack_from('<BII', data, offset)
        path = safe_path(folders[folder]) / safe_path(name)
        # Only the uncompressed 0x02 records have been verified with this ISO.
        if flag != 2:
            yield path, flag, None, offset, length
            continue
        if offset + 9 + length > len(data):
            raise ValueError(f'Truncated payload: {path}')
        yield path, flag, data[offset + 9:offset + 9 + length], offset, length


def wav_info(data):
    with wave.open(io.BytesIO(data)) as f:
        if f.getcomptype() != 'NONE':
            raise ValueError('Non-PCM WAV requires separate verification')
        frames = f.getnframes()
        payload = f.readframes(frames)
        if len(payload) != frames * f.getnchannels() * f.getsampwidth():
            raise ValueError('Truncated PCM data')
        return dict(rate=f.getframerate(), channels=f.getnchannels(), bits=f.getsampwidth()*8,
                    frames=frames, seconds=round(frames/f.getframerate(), 6))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='Local ISO or extracted installation directory')
    parser.add_argument('--output', type=Path, default=Path('imported/original-audio'))
    parser.add_argument('--map', type=Path, help='Reviewed mapping CSV (event, resource, evidence)')
    parser.add_argument('--audio-dir', type=Path, default=Path('assets/audio'))
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    out = args.output.resolve()
    if out == source or source.is_dir() and source in out.parents:
        raise ValueError('Output must be outside the source directory')
    out.mkdir(parents=True, exist_ok=True)
    source_info = {'path': str(source), 'sha256': digest(source.read_bytes()) if source.is_file() else None}
    if source.is_file():
        if not shutil.which('7z'):
            raise RuntimeError('Install 7-Zip (7z) to read the ISO, or pass an extracted directory')
        # Validate member names before asking 7z to unpack into an isolated directory.
        listing = subprocess.check_output(['7z', 'l', '-slt', str(source)], text=True)
        for line in listing.split('----------', 1)[-1].splitlines():
            if line.startswith('Path = '):
                safe_path(line[7:])
        staging = out / 'iso'
        if not staging.exists():
            staging.mkdir()
            subprocess.run(['7z', 'x', '-y', '-bd', '-bso0', '-bsp0', f'-o{staging}', str(source),
                            '-x!DEMOS/*', '-x!DIRECTX/*', '-x!AVI/*'], check=True)
            (out / 'iso-source.sha256').write_text(source_info['sha256'] + '\n')
        elif not (out / 'iso-source.sha256').exists() or (out / 'iso-source.sha256').read_text().strip() != source_info['sha256']:
            raise ValueError('Existing extraction belongs to another/incomplete import; choose another --output')
        source = staging
    inventory = []
    for resource in sorted(source.rglob('*')):
        if resource.is_symlink():
            raise ValueError(f'Refusing symlink in original resources: {resource}')
        if not resource.is_file():
            continue
        suffix = resource.suffix.lower()
        if suffix not in ('.vol', '.wav', '.wax', '.rbx', '.rmf'):
            continue
        data = resource.read_bytes()
        archive_hash = digest(data)
        relative = resource.relative_to(source)
        if suffix == '.vol' and data[:4] == b'VOLN':
            entries = vol_entries(data)
        else:
            entries = [(Path(resource.name), 2, data, 0, len(data))]
        for name, flag, payload, offset, length in entries:
            identity = (relative / name).as_posix() if suffix == '.vol' else relative.as_posix()
            row = dict(resource=identity, archive_sha256=archive_hash, offset=offset,
                       size=length, compression=flag, status='preserved')
            if payload is None:
                row['status'] = 'unsupported compression; original archive preserved'
            else:
                row['sha256'] = digest(payload)
                target = out / 'resources' / safe_path(identity)
                write_once(target, payload)
                if payload[:4] == b'RIFF' and payload[8:12] == b'WAVE':
                    try:
                        row.update(wav_info(payload)); row['status'] = 'PCM WAV verified'
                    except (wave.Error, ValueError, EOFError) as error:
                        row['status'] = f'undecoded WAV: {error}'
                elif name.suffix.lower() in ('.rbx', '.wax', '.rmf', '.vol'):
                    row['status'] = 'unsupported container; retained for inspection'
            inventory.append(row)
    (out / 'inventory.json').write_text(json.dumps({'source': source_info, 'files': inventory}, indent=2) + '\n')
    keys = ['resource', 'status', 'sha256', 'rate', 'bits', 'channels', 'frames', 'seconds', 'compression', 'offset', 'size', 'archive_sha256']
    with (out / 'inventory.csv').open('w', newline='') as f:
        writer = csv.DictWriter(f, keys); writer.writeheader(); writer.writerows(inventory)
    if args.map:
        by_name = {r['resource']: r for r in inventory}
        sections = {'events': {}, 'loops': {}}
        with args.map.open(newline='') as f:
            for mapping in csv.DictReader(f):
                event, identity = mapping['event'], mapping['resource']
                row = by_name.get(identity)
                if not row or row['status'] != 'PCM WAV verified' or not mapping['evidence'].strip():
                    raise ValueError(f'Unverified mapping: {event}: {identity}')
                relative = Path('sfx/original') / safe_path(identity)
                write_once(args.audio_dir / relative, (out / 'resources' / safe_path(identity)).read_bytes())
                section, key = ('loops', event[5:]) if event.startswith('loop.') else ('events', event)
                sections[section].setdefault(key, []).append(relative.as_posix())
        content = '# Generated by import_original_audio.py; media and this mapping stay local.\n'
        for section, values in sections.items():
            content += f'\n[{section}]\n' + ''.join(f'{key} = {" | ".join(paths)}\n' for key, paths in values.items())
        # Keep hand edits: a changed import writes a reviewable sibling instead.
        config = args.audio_dir / 'local.ini'
        if config.exists() and config.read_text() != content:
            config = args.audio_dir / 'local.import.ini'
        write_once(config, content.encode())
        print(f'Mappings: {config}')
    sounds = [r for r in inventory if r['status'] == 'PCM WAV verified']
    print(f'{len(inventory)} resources; {len(sounds)} PCM WAV files; {len({r["sha256"] for r in sounds})} unique recordings')
    print(f'Inventory and preserved resources: {out}')

if __name__ == '__main__':
    main()
