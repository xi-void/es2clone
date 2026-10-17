#!/usr/bin/env python3
"""Copy only public UI assets and audio documentation/config, never recordings."""
from pathlib import Path
import shutil
import sys
source, target = map(Path, sys.argv[1:])
# An explicit allowlist also keeps stale private files out of repeated packages.
allowed = {'.ttf', '.otf', '.svg', '.desktop', '.md', '.txt'}
for p in source.rglob('*'):
    if p.is_symlink() or not p.is_file():
        continue
    relative = p.relative_to(source)
    if 'original' in relative.parts:
        continue
    if 'audio' in relative.parts:
        keep = relative == Path('audio/audio.ini') or p.name == 'README.md'
    else:
        keep = p.suffix.lower() in allowed or p.name.startswith('LICENSE')
    if keep:
        out = target / relative
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(p, out)
