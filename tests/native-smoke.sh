#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
binary="${1:-./build/earthsiege}"
test_dir="$(mktemp -d /tmp/earthsiege-test.XXXXXX)"
trap 'rm -rf -- "$test_dir"' EXIT
export SDL_VIDEODRIVER=offscreen
export SDL_AUDIODRIVER=dummy
for scene in menu hangar startup combat pause controls audio debrief flow; do
    "$binary" --smoke-test --scene "$scene" --frames 40 --save-dir "$test_dir" --screenshot "$test_dir/$scene.ppm"
    test -s "$test_dir/$scene.ppm"
done
printf 'PASS: all native screens, input flow, and pilot persistence\n'
