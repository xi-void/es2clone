#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location('importer', Path(__file__).resolve().parents[1]/'tools/import_original_audio.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class ImportTests(unittest.TestCase):
    def archive(self, flag=2, path=b'FOOT.WAV', declared=None):
        folders = b'hmi\\\0'
        start = 18 + len(folders)
        payload = b'original resource bytes'
        header = b'VOLN' + bytes(6) + struct.pack('<H', len(folders)) + folders + struct.pack('<HI', 1, 18)
        entry = struct.pack('<13sBI', path, 0, start+18)
        return header + entry + struct.pack('<BII', flag, len(payload) if declared is None else declared, len(payload)) + payload
    def test_uncompressed(self):
        entries = list(module.vol_entries(self.archive()))
        self.assertEqual(entries[0][0], Path('hmi/FOOT.WAV'))
        self.assertEqual(entries[0][2], b'original resource bytes')
    def test_truncated(self):
        with self.assertRaises(ValueError):list(module.vol_entries(self.archive()[:-1]))
    def test_unknown_compression_preserved(self):
        self.assertIsNone(list(module.vol_entries(self.archive(9)))[0][2])
    def test_hostile_paths(self):
        for path in ('../outside', '/absolute', 'C:\\outside', '..\\outside'):
            with self.assertRaises(ValueError):module.safe_path(path)
    def test_bad_count(self):
        data = bytearray(self.archive());struct.pack_into('<H', data, 17, 65535)
        with self.assertRaises(ValueError):list(module.vol_entries(data))
    def test_bad_offset(self):
        data = bytearray(self.archive());struct.pack_into('<I', data, 37, 2)
        with self.assertRaises(ValueError):list(module.vol_entries(data))
    def test_unknown_signature(self):
        with self.assertRaises(ValueError):list(module.vol_entries(b'PVOL'+self.archive()[4:]))

if __name__ == '__main__':unittest.main()
