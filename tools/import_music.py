#!/usr/bin/env python3
"""Link or copy user-supplied soundtrack files; never fetch or generate music."""
import argparse
import os
from pathlib import Path
import shutil
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('source', type=Path)
p.add_argument('--audio-dir', type=Path, default=Path('assets/audio'))
p.add_argument('--copy', action='store_true')
a = p.parse_args()
source = a.source.resolve(strict=True)
target = (a.audio_dir / 'music').resolve()
target.mkdir(parents=True, exist_ok=True)
count = 0
for file in sorted(source.rglob('*')):
    if not file.is_file() or any(part.startswith('.') or part == '__MACOSX' for part in file.relative_to(source).parts):
        continue
    if file.suffix.lower() not in ('.wav', '.flac', '.ogg', '.mp3'):
        continue
    dest = target / file.name
    if dest.exists() or dest.is_symlink():
        if dest.resolve() == file.resolve() or dest.is_file() and dest.read_bytes() == file.read_bytes():
            continue
        raise SystemExit(f'Refusing to replace existing music: {dest}')
    if a.copy:
        shutil.copyfile(file, dest)
    else:
        dest.symlink_to(os.path.relpath(file, dest.parent))
    count += 1
print(f'Imported {count} tracks into {target}; numbered track 001 is reserved for level startup.')
