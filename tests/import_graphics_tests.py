#!/usr/bin/env python3
"""Synthetic fixtures only; the original disc is not needed for public tests."""
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import import_original_graphics as graphics


class GraphicsImport(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'resources'
        self.output = self.root / 'assets/original'
        image = (bytes.fromhex('01002800') + struct.pack('<II', 307226, 1)
                 + bytes.fromhex('0e002800') + struct.pack('<IHHBBBIH', 307213, 480, 640, 8, 0, 0, 307200, 0)
                 + bytes(307201))
        palette = bytes.fromhex('0f002800') + struct.pack('<II', 1032, 256) + bytes(1028)
        self.members = {'hb0/tomahawk.hb0': image, 'dpl/cockpit.dpl': palette}
        for name, data in self.members.items():
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        self.audio = self.root / 'assets/audio/local.ini'
        self.audio.parent.mkdir(parents=True)
        self.audio.write_text('[events]\nturbo_engage = keep-this.wav\n')

    def test_copy_idempotence_and_isolation(self):
        for _ in range(2):
            graphics.import_graphics(self.source, self.output)
        for name, data in self.members.items():
            self.assertEqual((self.source / name).read_bytes(), data)
            self.assertEqual((self.output / graphics.MEMBERS[name]).read_bytes(), data)
        self.assertEqual(self.audio.read_text(), '[events]\nturbo_engage = keep-this.wav\n')
        self.assertEqual(len(list(self.output.iterdir())), 3)

    def test_conflict_never_overwrites_or_partially_copies(self):
        self.output.mkdir(parents=True)
        (self.output / 'COCKPIT.DPL').write_bytes(b'local data')
        with self.assertRaises(ValueError):
            graphics.import_graphics(self.source, self.output)
        self.assertEqual((self.output / 'COCKPIT.DPL').read_bytes(), b'local data')
        self.assertFalse((self.output / 'TOMAHAWK.HB0').exists())

    def test_source_is_read_only(self):
        with self.assertRaises(ValueError):
            graphics.import_graphics(self.source, self.source / 'new')

    def test_missing_or_bad_member(self):
        (self.source / 'hb0/tomahawk.hb0').write_bytes(b'not a bitmap')
        with self.assertRaises(ValueError):
            graphics.import_graphics(self.source, self.output)
        self.assertFalse(self.output.exists())

    def test_ambiguous_source(self):
        duplicate = self.source / 'duplicate/hb0/tomahawk.hb0'
        duplicate.parent.mkdir(parents=True)
        duplicate.write_bytes(self.members['hb0/tomahawk.hb0'])
        with self.assertRaises(ValueError):
            graphics.import_graphics(self.source, self.output)

    def test_original_graphics_are_never_packaged(self):
        import subprocess
        graphics.import_graphics(self.source, self.output)
        (self.output / 'private.txt').write_text('private source notes')
        destination = self.root / 'public'
        subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / 'tools/stage_public_assets.py'),
                        str(self.output.parent), str(destination)], check=True)
        self.assertFalse((destination / 'original').exists())


if __name__ == '__main__':
    unittest.main()
