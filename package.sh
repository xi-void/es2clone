#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel "$(getconf _NPROCESSORS_ONLN)"
package_name="earthsiege-linux-$(uname -m)"
package_root="build/package/$package_name"
mkdir -p "$package_root" dist
cp build/earthsiege "$package_root/earthsiege"
cp -R assets "$package_root/"
cp README.md "$package_root/README.md"
cp -R docs "$package_root/"
tar -czf "dist/$package_name.tar.gz" -C build/package "$package_name"
printf 'Created dist/%s.tar.gz\n' "$package_name"
