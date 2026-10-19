#!/usr/bin/env python3
"""Reconstruct discrete ZONE heights offline; reuse graphics_probe's DBM/DBA reader.

Only the verified single-frame, power-of-two bitmap path is admitted. Includes
horizontal spacing metadata; no vertical metre conversion or interpolation.
See docs/zone111-heightmap-validation.md for original executable evidence.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import zlib

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PROBE = ROOT / 'build/graphics_probe'
DEFAULT_OUTPUT = ROOT / 'imported/terrain-validation/zone111'


def decode_bitmap(path, probe):
    """Delegate every original bitmap format/boundary check to the existing reader."""
    try:
        result = subprocess.run([str(probe.resolve()), '--indices-json', str(path)],
                                capture_output=True, text=True, check=False)
    except FileNotFoundError as error:
        raise ValueError('graphics_probe is missing; build it with '
                         '`cmake --build build --target graphics_probe` or pass --probe') from error
    if result.returncode:
        raise ValueError(f'DBA decode failed: {result.stderr.strip()}')
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise ValueError('graphics_probe did not return index JSON; rebuild the tool') from error


def signed32(value):
    """Retain the low 32 bits, as in the observed IMUL/ADD instructions."""
    return ((value + 0x80000000) & 0xffffffff) - 0x80000000


def horizontal_spacing(shift, width, height):
    """Add coordinate metadata without changing discrete height reconstruction."""
    result = {'dat_word_index': 2, 'shift_i32': shift}
    # Keep formerly accepted DATs usable for discrete heights. Do not turn an
    # arbitrary signed word into a guessed spacing via x86 shift-count masking.
    if not 0 <= shift <= 30:
        return dict(result, status='unsupported_shift',
                    reason='Spacing requires a shift in 0..30 for a positive signed 32-bit step; '
                           'height decoding is unchanged')
    spacing = 1 << shift
    spans = {'x': (width - 1) * spacing, 'y': (height - 1) * spacing}
    return dict(result, status='decoded', spacing_internal=spacing,
                spacing_metres_nominal=spacing * 6 / 1000,
                span_internal=spans,
                span_metres_nominal={axis: value * 6 / 1000 for axis, value in spans.items()},
                span_definition='first to last sample: (axis_sample_count - 1) * spacing; '
                                'not an outer-boundary or tiling rule',
                nominal_metres_per_internal_unit={'numerator': 6, 'denominator': 1000},
                display_conversion={
                    'formula': '6 * trunc_toward_zero(distance_internal / 1000)',
                    'quantized': True, 'quantum_metres': 6,
                    'distance_metric': 'max(abs(dx), abs(dy)) + (min(abs(dx), abs(dy)) >> 1)',
                    'one_axis_step_metres': (spacing // 1000) * 6},
                vertical_metres_independently_verified=False)


def reconstruct(dat, bitmap):
    if len(dat) != 16:
        raise ValueError(f'ZONE DAT must contain exactly 16 bytes; got {len(dat)}')
    words = struct.unpack('<4i', dat)
    if not isinstance(bitmap, dict) or bitmap.get('frame_count') != 1:
        raise ValueError('Expected exactly one bitmap frame; other terrain layouts are unverified')
    width, height = bitmap.get('width'), bitmap.get('height')
    for name, value in (('width', width), ('height', height)):
        if type(value) is not int or not 1 <= value <= 4096:
            raise ValueError(f'Invalid bitmap {name}: {value!r}')
        if value & (value - 1):
            raise ValueError(f'Non-power-of-two bitmap {name} is unverified; refusing to resize')
    indices = bitmap.get('indices')
    if (not isinstance(indices, list) or len(indices) != width * height
            or any(type(v) is not int or not 0 <= v <= 255 for v in indices)):
        raise ValueError('Bitmap indices must be width * height unsigned 8-bit integers')
    # 0x0046c3c0 reverses rows, preserving column order. The zone path supplies
    # subtraction offset 0. 0x0046e07c zero-extends each sample, IMULs, then ADDs.
    rows = [indices[y * width:(y + 1) * width] for y in range(height - 1, -1, -1)]
    heights = [[signed32(sample * words[3] + 0) for sample in row] for row in rows]
    flat = [v for row in heights for v in row]
    points = [(0, 0), (width - 1, 0), (0, height - 1), (width - 1, height - 1),
              (width // 2, height // 2)]
    for extreme in (min(flat), max(flat)):
        index = flat.index(extreme)
        points.append((index % width, index // width))
    return {
        'schema_version': 1, 'width': width, 'height': height,
        'dat_words_i32': list(words), 'dat_words_raw_u32': list(struct.unpack('<4I', dat)),
        'uninterpreted_dat_byte_offsets': [0, 4],
        'horizontal_spacing': horizontal_spacing(words[2], width, height),
        'multiplier_i32': words[3], 'base_i32': 0, 'units': 'internal signed 32-bit units',
        'row_order': 'grid[y][x] = source_bitmap[height - 1 - y][x]',
        'sample_min': min(indices), 'sample_max': max(indices),
        'height_min': min(flat), 'height_max': max(flat),
        'selected_samples': [
            {'grid_x': x, 'grid_y': y, 'source_x': x, 'source_row': height - 1 - y,
             'sample_u8': rows[y][x], 'height_i32': heights[y][x]}
            for x, y in dict.fromkeys(points)],
        'preview': '8-bit grayscale: min height = black, max = white; flat grid = black; '
                   'top image row is grid y=0; no physical axis convention implied',
        'heights': heights,
    }


def heightmap_png(grid):
    """A small lossless grayscale preview, with no imaging dependency."""
    low, high = grid['height_min'], grid['height_max']
    span = high - low
    pixels = b''.join(b'\0' + bytes((v - low) * 255 // span if span else 0 for v in row)
                      for row in grid['heights'])

    def chunk(kind, payload):
        return (struct.pack('>I', len(payload)) + kind + payload
                + struct.pack('>I', zlib.crc32(kind + payload)))

    return (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', grid['width'], grid['height'], 8, 0, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b''))


def reconstruct_files(dat_path, dba_path, probe=DEFAULT_PROBE):
    # Read only the exact DAT record; fail without launching the decoder on a bad DAT.
    with dat_path.open('rb') as source:
        dat = source.read(17)
    if len(dat) != 16:
        raise ValueError('ZONE DAT must contain exactly 16 bytes')
    grid = reconstruct(dat, decode_bitmap(dba_path, probe))
    sources = {}
    for key, path in (('dat', dat_path), ('dba', dba_path)):
        digest = hashlib.sha256()
        with path.open('rb') as source:
            for block in iter(lambda: source.read(65536), b''):
                digest.update(block)
        sources[key] = {'path': str(path), 'sha256': digest.hexdigest()}
    grid['sources'] = sources
    grid['decoder'] = 'existing original::decodeBitmaps via graphics_probe --indices-json'
    return grid


def write_artifacts(grid, output, sources):
    output = output.resolve()
    if any(path.resolve().parent == output or path.resolve().parent in output.parents
           for path in sources):
        raise ValueError('Output must be outside the original resource directories')
    # Decode and serialize before creating outputs. A fresh directory prevents
    # replacement of previous validation data or accidental source overwrites.
    summary = {k: v for k, v in grid.items() if k != 'heights'}
    artifacts = {'height-grid.json': (json.dumps(grid, indent=2) + '\n').encode(),
                 'report.json': (json.dumps(summary, indent=2) + '\n').encode(),
                 'heightmap.png': heightmap_png(grid)}
    output.mkdir(parents=True, exist_ok=False)
    for name, data in artifacts.items():
        (output / name).write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dat', type=Path, help='Extracted ZONE111.DAT (16 bytes)')
    parser.add_argument('dba', type=Path, help='Matching extracted ZONE111.DBA')
    parser.add_argument('--probe', type=Path, default=DEFAULT_PROBE)
    parser.add_argument('--output', type=Path, default=DEFAULT_OUTPUT,
                        help='New directory; defaults to ignored imported/terrain-validation/zone111')
    args = parser.parse_args()
    try:
        grid = reconstruct_files(args.dat, args.dba, args.probe)
        write_artifacts(grid, args.output, (args.dat, args.dba))
    except (OSError, ValueError) as error:
        parser.exit(1, f'Heightmap reconstruction failed: {error}\n')
    print(f'{grid["width"]}x{grid["height"]}; samples {grid["sample_min"]}..{grid["sample_max"]}; '
          f'heights {grid["height_min"]}..{grid["height_max"]} internal units')
    print(f'Validation artifacts: {args.output}')


if __name__ == '__main__':
    main()
