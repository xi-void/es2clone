#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel "$(getconf _NPROCESSORS_ONLN)"
exec ./build/earthsiege "$@"
