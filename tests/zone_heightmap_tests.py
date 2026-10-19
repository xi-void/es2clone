#!/usr/bin/env python3
"""Synthetic offline fixtures; no original resources or analysis tools required."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import reconstruct_zone_heightmap as terrain

PROBE = terrain.DEFAULT_PROBE


def bitmap(width, height, samples):
    # Independent packed DBM fixture, matching the existing original graphics tests.
    raw = struct.pack('<IIHHBBBIH', 0x28000e, 13 + len(samples), height, width,
                      8, 0, 0, len(samples), 0) + bytes(samples)
    return raw + bytes(len(raw) & 1)


def array(*frames):
    return struct.pack('<III', 0x280001, 4 + sum(map(len, frames)), len(frames)) + b''.join(frames)


def png_pixels(data):
    """Independently check PNG framing, CRCs and the unfiltered grayscale rows."""
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise AssertionError('Not a PNG')
    pos, compressed, dimensions = 8, b'', None
    while pos < len(data):
        size, kind = struct.unpack_from('>I4s', data, pos)
        body = data[pos + 8:pos + 8 + size]
        crc, = struct.unpack_from('>I', data, pos + 8 + size)
        if crc != zlib.crc32(kind + body):
            raise AssertionError('Bad PNG checksum')
        if kind == b'IHDR':
            width, height, *format_values = struct.unpack('>IIBBBBB', body)
            if format_values != [8, 0, 0, 0, 0]:
                raise AssertionError('Expected unfiltered 8-bit grayscale PNG')
            dimensions = width, height
        elif kind == b'IDAT':
            compressed += body
        pos += 12 + size
        if kind == b'IEND':
            if pos != len(data):
                raise AssertionError('Trailing PNG bytes')
            return dimensions, zlib.decompress(compressed)
    raise AssertionError('Missing IEND')


class ZoneHeightmap(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / 'original'
        self.source.mkdir()
        self.dat = self.source / 'ZONE111.DAT'
        self.dba = self.source / 'ZONE111.DBA'
        self.dat.write_bytes(struct.pack('<4i', 7, 7, 14, 200))
        self.dba.write_bytes(array(bitmap(2, 2, [1, 2, 3, 4])))

    def load(self):
        return terrain.reconstruct_files(self.dat, self.dba, PROBE)

    def cli(self, output):
        return subprocess.run([sys.executable, str(ROOT / 'tools/reconstruct_zone_heightmap.py'),
                               str(self.dat), str(self.dba), '--probe', str(PROBE),
                               '--output', str(output)], text=True, capture_output=True)

    def test_unsigned_samples_and_verified_multiplier(self):
        self.dba.write_bytes(array(bitmap(8, 1, [0, 127, 128, 159, 255, 1, 2, 254])))
        result = self.load()
        self.assertEqual(result['heights'], [[0, 25400, 25600, 31800, 51000, 200, 400, 50800]])
        self.assertEqual((result['sample_min'], result['sample_max']), (0, 255))
        self.assertEqual(result['base_i32'], 0)

    def test_row_reversal_preserves_column_order(self):
        result = self.load()
        self.assertEqual(result['heights'], [[600, 800], [200, 400]])
        self.assertEqual(result['selected_samples'][0],
                         {'grid_x': 0, 'grid_y': 0, 'source_x': 0, 'source_row': 1,
                          'sample_u8': 3, 'height_i32': 600})

    def test_non_square_dimensions_come_from_bitmap_not_unknown_dat_words(self):
        self.dat.write_bytes(struct.pack('<4i', 123, -7, -2147483648, 1))
        self.dba.write_bytes(array(bitmap(4, 2, range(8))))
        result = self.load()
        self.assertEqual((result['width'], result['height']), (4, 2))
        self.assertEqual(result['heights'], [[4, 5, 6, 7], [0, 1, 2, 3]])
        self.assertEqual(result['dat_words_i32'], [123, -7, -2147483648, 1])
        self.assertEqual(result['dat_words_raw_u32'], [123, 4294967289, 2147483648, 1])
        self.assertEqual(result['uninterpreted_dat_byte_offsets'], [0, 4])
        self.assertEqual(result['horizontal_spacing']['status'], 'unsupported_shift')

    def test_zone111_spacing_and_first_to_last_span(self):
        self.dba.write_bytes(array(bitmap(128, 128, [159] * (128 * 128))))
        result = self.load()
        spacing = result['horizontal_spacing']
        self.assertEqual(spacing['spacing_internal'], 16384)
        self.assertEqual(spacing['spacing_metres_nominal'], 98.304)
        self.assertEqual(spacing['span_internal'], {'x': 2080768, 'y': 2080768})
        self.assertEqual(spacing['span_metres_nominal'], {'x': 12484.608, 'y': 12484.608})
        self.assertEqual(result['schema_version'], 1)
        self.assertEqual(result['heights'], [[31800] * 128 for _ in range(128)])
        self.assertFalse(spacing['vertical_metres_independently_verified'])

    def test_spacing_uses_third_dat_word_and_rectangular_bitmap(self):
        self.dat.write_bytes(struct.pack('<4i', 99, -1, 10, 200))
        self.dba.write_bytes(array(bitmap(4, 2, range(8))))
        result = self.load()
        spacing = result['horizontal_spacing']
        self.assertEqual(spacing['shift_i32'], 10)
        self.assertEqual(spacing['spacing_internal'], 1024)
        self.assertEqual(spacing['span_internal'], {'x': 3072, 'y': 1024})
        self.assertEqual(spacing['span_metres_nominal'], {'x': 18.432, 'y': 6.144})
        self.assertEqual(result['heights'], [[800, 1000, 1200, 1400], [0, 200, 400, 600]])

    def test_single_sample_has_zero_span_and_positive_shift_boundaries(self):
        for shift, expected_step in [(0, 1), (30, 1073741824)]:
            with self.subTest(shift=shift):
                spacing = terrain.horizontal_spacing(shift, 1, 1)
                self.assertEqual(spacing['spacing_internal'], expected_step)
                self.assertEqual(spacing['span_internal'], {'x': 0, 'y': 0})
                self.assertEqual(spacing['span_metres_nominal'], {'x': 0.0, 'y': 0.0})

    def test_display_conversion_is_quantized_and_divides_before_multiplying(self):
        for shift, expected_metres in [(9, 0), (10, 6), (14, 96)]:
            with self.subTest(shift=shift):
                spacing = terrain.horizontal_spacing(shift, 2, 2)
                display = spacing['display_conversion']
                self.assertTrue(display['quantized'])
                self.assertEqual(display['quantum_metres'], 6)
                self.assertEqual(display['one_axis_step_metres'], expected_metres)
                self.assertEqual(spacing['nominal_metres_per_internal_unit'],
                                 {'numerator': 6, 'denominator': 1000})
                self.assertNotEqual(spacing['spacing_metres_nominal'], expected_metres)

    def test_unverified_shifts_preserve_heights_without_guessing_spacing(self):
        for shift in [-2147483648, -1, 31, 32, 2147483647]:
            with self.subTest(shift=shift):
                self.dat.write_bytes(struct.pack('<4i', 7, 7, shift, 200))
                result = self.load()
                self.assertEqual(result['heights'], [[600, 800], [200, 400]])
                spacing = result['horizontal_spacing']
                self.assertEqual(spacing['status'], 'unsupported_shift')
                self.assertEqual(spacing['shift_i32'], shift)
                self.assertNotIn('spacing_internal', spacing)
                self.assertNotIn('spacing_metres_nominal', spacing)

    def test_signed_multiplier_and_32_bit_wrap(self):
        self.dba.write_bytes(array(bitmap(2, 1, [1, 2])))
        for multiplier, expected in [(2147483647, [2147483647, -2]),
                                     (-2147483648, [-2147483648, 0]), (-200, [-200, -400])]:
            with self.subTest(multiplier=multiplier):
                self.dat.write_bytes(struct.pack('<4i', 0, 0, 0, multiplier))
                self.assertEqual(self.load()['heights'], [expected])

    def test_flat_grid_preview(self):
        self.dat.write_bytes(struct.pack('<4i', 7, 7, 14, 0))
        grid = self.load()
        self.assertEqual(png_pixels(terrain.heightmap_png(grid)), ((2, 2), bytes(6)))

    def test_truncated_or_extra_dat_bytes(self):
        good = self.dat.read_bytes()
        for raw in [good[:length] for length in range(16)] + [good + b'\0', good + bytes(16)]:
            with self.subTest(length=len(raw)):
                self.dat.write_bytes(raw)
                with self.assertRaisesRegex(ValueError, 'exactly 16 bytes'):
                    self.load()

    def test_every_dba_truncation_is_rejected(self):
        good = self.dba.read_bytes()
        for length in range(len(good)):
            with self.subTest(length=length):
                self.dba.write_bytes(good[:length])
                with self.assertRaisesRegex(ValueError, 'DBA decode failed'):
                    self.load()

    def test_malformed_dba_headers_and_unsupported_encoding(self):
        good = self.dba.read_bytes()
        for offset, fmt, value in [(0, 'I', 0), (4, 'I', 0), (8, 'I', 0), (8, 'I', 4097),
                                   (12, 'I', 0), (16, 'I', 0), (20, 'H', 0), (22, 'H', 65535),
                                   (24, 'B', 16), (25, 'H', 1), (27, 'I', 999), (31, 'H', 1)]:
            with self.subTest(offset=offset, value=value):
                bad = bytearray(good)
                struct.pack_into('<' + fmt, bad, offset, value)
                self.dba.write_bytes(bad)
                with self.assertRaisesRegex(ValueError, 'DBA decode failed'):
                    self.load()

    def test_multiple_frames_rejected_instead_of_guessing(self):
        self.dba.write_bytes(array(bitmap(2, 2, [1] * 4), bitmap(2, 2, [2] * 4)))
        with self.assertRaisesRegex(ValueError, 'exactly one bitmap frame'):
            self.load()

    def test_non_power_of_two_not_resized(self):
        self.dba.write_bytes(array(bitmap(3, 2, range(6))))
        with self.assertRaisesRegex(ValueError, 'Non-power-of-two'):
            self.load()

    def test_invalid_index_json_is_rejected(self):
        good = {'frame_count': 1, 'width': 2, 'height': 2, 'indices': [1, 2, 3, 4]}
        for field, value in [('indices', [1, 2, 3]), ('indices', [-1, 1, 2, 3]),
                             ('indices', [256, 1, 2, 3]), ('indices', [1.0, 1, 2, 3]),
                             ('indices', [True, 1, 2, 3]), ('width', 0), ('height', '2')]:
            with self.subTest(field=field, value=value):
                with self.assertRaises(ValueError):
                    terrain.reconstruct(self.dat.read_bytes(), dict(good, **{field: value}))

    def test_missing_decoder_has_actionable_error(self):
        with self.assertRaisesRegex(ValueError, 'cmake --build build --target graphics_probe'):
            terrain.reconstruct_files(self.dat, self.dba, self.root / 'missing-probe')

    def test_cli_outputs_exact_data_valid_preview_and_source_hashes(self):
        before = {p: p.read_bytes() for p in (self.dat, self.dba)}
        output = self.root / 'results'
        process = self.cli(output)
        self.assertEqual(process.returncode, 0, process.stderr)
        grid = json.loads((output / 'height-grid.json').read_text())
        report = json.loads((output / 'report.json').read_text())
        self.assertEqual(grid['heights'], [[600, 800], [200, 400]])
        self.assertEqual((report['height_min'], report['height_max']), (200, 800))
        self.assertNotIn('heights', report)
        self.assertEqual(report['horizontal_spacing'], grid['horizontal_spacing'])
        self.assertEqual(report['horizontal_spacing']['spacing_internal'], 16384)
        self.assertEqual(png_pixels((output / 'heightmap.png').read_bytes()),
                         ((2, 2), bytes([0, 170, 255, 0, 0, 85])))
        for key, path in (('dat', self.dat), ('dba', self.dba)):
            self.assertEqual(path.read_bytes(), before[path])
            self.assertEqual(report['sources'][key]['sha256'], hashlib.sha256(before[path]).hexdigest())

    def test_cli_bad_resource_is_nonzero_without_artifacts(self):
        self.dba.write_bytes(b'broken')
        output = self.root / 'results'
        process = self.cli(output)
        self.assertNotEqual(process.returncode, 0)
        self.assertIn('DBA decode failed', process.stderr)
        self.assertFalse(output.exists())

    def test_outputs_cannot_overwrite_previous_run_or_source_directory(self):
        output = self.root / 'results'
        output.mkdir()
        sentinel = output / 'height-grid.json'
        sentinel.write_bytes(b'keep previous result')
        self.assertNotEqual(self.cli(output).returncode, 0)
        self.assertEqual(sentinel.read_bytes(), b'keep previous result')
        self.assertEqual(list(output.iterdir()), [sentinel])
        self.assertNotEqual(self.cli(self.source / 'results').returncode, 0)
        self.assertFalse((self.source / 'results').exists())

    def test_existing_palette_preview_interface_remains_compatible(self):
        palette = bytearray(struct.pack('<III', 0x28000f, 1032, 256) + bytes(1028))
        palette[12 + 7 * 4:15 + 7 * 4] = bytes([63, 24, 1])
        path = self.source / 'SYNTHETIC.DPL'
        path.write_bytes(palette)
        self.dba.write_bytes(array(bitmap(2, 2, [7] * 4)))
        output = self.root / 'preview.ppm'
        process = subprocess.run([str(PROBE), str(self.dba), str(path), str(output)],
                                 text=True, capture_output=True)
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertEqual(process.stdout, '1 frames; first 2x2\n')
        self.assertEqual(output.read_bytes(), b'P6\n2 2\n255\n' + bytes([252, 96, 4]) * 4)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--probe', type=Path, default=PROBE)
    args, remaining = parser.parse_known_args()
    PROBE = args.probe.resolve()
    unittest.main(argv=[sys.argv[0], *remaining])
