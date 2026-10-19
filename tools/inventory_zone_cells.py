#!/usr/bin/env python3
"""Inventory interior terrain modes from an existing reconstructed height-grid.json.

Uses the verified 0x0046bed8 sample comparison. No interpolation or edge modes.
See docs/terrain-diagonal-selection.md. Outputs belong under ignored imported/.
"""
import argparse
import hashlib
import json
from pathlib import Path

from reconstruct_zone_heightmap import ROOT, heightmap_png, signed32

DEFAULT_OUTPUT = ROOT / 'imported/terrain-validation/zone111-cell-modes'
ROW_ORDER = 'grid[y][x] = source_bitmap[height - 1 - y][x]'
# Offsets are established by the 0x0046e07c branches, not inferred from sign.
MODE_DEFINITIONS = (
    {'mode': 0, 'condition': 'delta > 0', 'sign': 'positive',
     'diagonal_offsets': [[1, 0], [0, 1]], 'preview_gray': 0},
    {'mode': 2, 'condition': 'delta < 0', 'sign': 'negative',
     'diagonal_offsets': [[0, 0], [1, 1]], 'preview_gray': 255},
    {'mode': 1, 'condition': 'delta == 0', 'sign': 'zero',
     'diagonal_offsets': None, 'preview_gray': 127},
)


def recover_samples(grid):
    """Invert the existing signed32(sample * multiplier + base) without guessing."""
    if not isinstance(grid, dict) or type(grid.get('schema_version')) is not int or grid['schema_version'] != 1:
        raise ValueError('Expected reconstructed height-grid schema_version 1')
    if grid.get('row_order') != ROW_ORDER:
        raise ValueError('Unrecognized row_order; expected the already row-reversed height grid')
    width, height = grid.get('width'), grid.get('height')
    for name, value in (('width', width), ('height', height)):
        if type(value) is not int or not 2 <= value <= 4096:
            raise ValueError(f'Invalid {name}: {value!r}; need 2..4096 samples for interior cells')
    for name in ('multiplier_i32', 'base_i32'):
        value = grid.get(name)
        if type(value) is not int or not -2147483648 <= value <= 2147483647:
            raise ValueError(f'{name} must be a signed 32-bit integer')
    inverse = {signed32(s * grid['multiplier_i32'] + grid['base_i32']): s for s in range(256)}
    if len(inverse) != 256:
        raise ValueError('Ambiguous sample recovery: multiplier/base do not uniquely encode unsigned bytes')
    rows = grid.get('heights')
    if not isinstance(rows, list) or len(rows) != height:
        raise ValueError('heights row count does not match height')
    samples = []
    for y, row in enumerate(rows):
        if not isinstance(row, list) or len(row) != width:
            raise ValueError(f'heights row {y} does not match width')
        decoded = []
        for x, value in enumerate(row):
            if type(value) is not int or value not in inverse:
                raise ValueError(f'Height at ({x},{y}) cannot recover an unsigned byte: {value!r}')
            decoded.append(inverse[value])
        samples.append(decoded)
    return samples


def inventory(grid):
    samples = recover_samples(grid)
    width, height = grid['width'] - 1, grid['height'] - 1
    modes, counts = [], [0, 0, 0]
    for y in range(height):
        row = []
        for x in range(width):
            s00, s10 = samples[y][x:x + 2]
            s01, s11 = samples[y + 1][x:x + 2]
            delta = (s00 + s11) - (s01 + s10)
            # 0x0046c02f: zero -> mode 1; 0x0046c069: negative -> 2, positive -> 0.
            mode = 1 if delta == 0 else 2 if delta < 0 else 0
            row.append(mode)
            counts[mode] += 1
        modes.append(row)
    total = width * height
    return {
        'schema_version': 1,
        'sample_width': grid['width'], 'sample_height': grid['height'],
        'cell_width': width, 'cell_height': height, 'interior_cell_count': total,
        'comparison': 'delta = (s00 + s11) - (s01 + s10), unsigned sample bytes',
        'corner_offsets': {'s00': [0, 0], 's10': [1, 0], 's01': [0, 1], 's11': [1, 1]},
        'orientation': 'cell_modes[y][x]; x increases right, y increases down in preview; '
                       'same row order as input; no second reversal or geographic direction implied',
        'scope': 'Interior cells only; no final-row/column modes, upper flag bits or interpolation; '
                 'bare reconstructed samples, not mission preprocessing',
        'sample_recovery': 'Unique inverse of signed32(sample_u8 * multiplier_i32 + base_i32)',
        'modes': [dict(entry, count=counts[entry['mode']],
                       percentage=counts[entry['mode']] * 100 / total) for entry in MODE_DEFINITIONS],
        'cell_modes': modes,
    }


def write_inventory(result, output, preview=False):
    artifacts = {'cell-modes.json': (json.dumps(result, indent=2) + '\n').encode()}
    if preview:
        # Reuse the existing lossless grayscale writer with a fixed categorical scale.
        artifacts['cell-modes.png'] = heightmap_png({
            'width': result['cell_width'], 'height': result['cell_height'],
            'heights': result['cell_modes'], 'height_min': 0, 'height_max': 2})
    output.mkdir(parents=True, exist_ok=False)
    for name, data in artifacts.items():
        (output / name).write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('height_grid', type=Path, help='Existing reconstructed height-grid.json')
    parser.add_argument('--output', type=Path, default=DEFAULT_OUTPUT, help='New output directory')
    parser.add_argument('--preview', action='store_true', help='Also write a grayscale cell-mode PNG')
    args = parser.parse_args()
    try:
        raw = args.height_grid.read_bytes()
        result = inventory(json.loads(raw))
        result['source_grid'] = {'path': str(args.height_grid), 'sha256': hashlib.sha256(raw).hexdigest()}
        write_inventory(result, args.output, args.preview)
    except (OSError, ValueError) as error:
        parser.exit(1, f'Cell-mode inventory failed: {error}\n')
    print(f'{result["cell_width"]}x{result["cell_height"]}: {result["interior_cell_count"]} interior cells')
    for entry in result['modes']:
        print(f'Mode {entry["mode"]} ({entry["sign"]}): {entry["count"]} ({entry["percentage"]:.4f}%)')
    print(f'Inventory artifacts: {args.output}')


if __name__ == '__main__':
    main()
