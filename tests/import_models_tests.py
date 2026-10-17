#!/usr/bin/env python3
"""Importer isolation tests; synthetic headers, no original copyrighted data."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import import_original_models as models


class ModelImport(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'resources'
        self.output = self.root / 'assets/original/models'
        self.members = {}
        for key in models.MEMBERS:
            if key.endswith('.dat'):
                data = bytearray(216)
                struct.pack_into('<H', data, 148, 1 if 'tomahawk' in key else 3)
                data = bytes(data)
            else:
                tag = {'.dts': 0x1e0003, '.dba': 0x280001, '.dpl': 0x28000f}[Path(key).suffix]
                data = struct.pack('<II', tag, 0)
            self.members[key] = data
            p = self.source / key
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(data)
        self.audio = self.root / 'assets/audio/local.ini'
        self.audio.parent.mkdir(parents=True)
        self.audio.write_text('[events]\nenemy_destroyed = keep.wav\n')

    def test_copy_and_package_isolation(self):
        for _ in range(2):
            models.import_models(self.source, self.output)
        for key, name in models.MEMBERS.items():
            self.assertEqual((self.source / key).read_bytes(), self.members[key])
            self.assertEqual((self.output / name).read_bytes(), self.members[key])
        self.assertEqual(self.audio.read_text(), '[events]\nenemy_destroyed = keep.wav\n')
        public = self.root / 'public'
        subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / 'tools/stage_public_assets.py'),
                        str(self.root / 'assets'), str(public)], check=True)
        self.assertFalse((public / 'original').exists())

    def test_conflict_is_atomic(self):
        self.output.mkdir(parents=True)
        (self.output / 'ENEMY.DBA').write_bytes(b'keep')
        with self.assertRaises(ValueError):
            models.import_models(self.source, self.output)
        self.assertEqual((self.output / 'ENEMY.DBA').read_bytes(), b'keep')
        self.assertFalse((self.output / 'TOMAHAWK.DTS').exists())

    def test_missing_and_wrong_skin(self):
        (self.source / 'dat/achilles.dat').write_bytes(bytes(216))
        with self.assertRaises(ValueError):
            models.import_models(self.source, self.output)
        self.assertFalse(self.output.exists())
        (self.source / 'dat/achilles.dat').unlink()
        with self.assertRaises(ValueError):
            models.import_models(self.source, self.output)

    def test_refuse_source_destination(self):
        with self.assertRaises(ValueError):
            models.import_models(self.source, self.source / 'new')


if __name__ == '__main__':
    unittest.main()
