#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel "$(getconf _NPROCESSORS_ONLN)"
package_name="earthsiege-linux-$(uname -m)"
package_root="build/package/$package_name"
if [[ -d "$package_root" ]]; then rm -rf -- "$package_root"; fi
mkdir -p "$package_root" dist
cp build/earthsiege "$package_root/earthsiege"
cp build/audio_preview "$package_root/audio_preview"
python3 tools/stage_public_assets.py assets "$package_root/assets"
cp README.md "$package_root/README.md"
cp -R docs "$package_root/"
mkdir -p "$package_root/tools"
cp tools/import_original_audio.py tools/import_music.py tools/original-audio-map.csv "$package_root/tools/"
tar -czf "dist/$package_name.tar.gz" -C build/package "$package_name"
printf 'Created dist/%s.tar.gz\n' "$package_name"
