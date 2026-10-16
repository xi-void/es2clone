#!/usr/bin/env python3
"""Fail publication if original resources or user media are in the Git index."""
import subprocess
from pathlib import PurePosixPath
files = subprocess.check_output(['git', 'ls-files', '-z']).decode().split('\0')
media = {'.iso', '.bin', '.cue', '.vol', '.rbx', '.wax', '.rmf', '.wav', '.mp3', '.ogg', '.flac'}
private_roots = {'original', 'imported'}
bad = []
for name in files:
    if not name:
        continue
    p = PurePosixPath(name)
    if (p.parts[0].lower() in private_roots or p.parts[0].startswith('Earthsiege2RemasteredSoundtrack')
            or p.suffix.lower() in media or (name.startswith('assets/audio/local') and p.suffix == '.ini')
            or (name.startswith(('assets/audio/sfx/', 'assets/audio/music/', 'assets/audio/fallback/')) and p.name != 'README.md')):
        bad.append(name)
if bad:
    raise SystemExit('Refusing publication: private assets are tracked/staged:\n' + '\n'.join(bad))
print('Public tree check: no original resources or audio recordings tracked/staged.')
