#!/usr/bin/env bash

set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "build_mac.sh must be run on macOS." >&2
  exit 1
fi

build_dir="${BUILD_DIR:-build}"
config="${CONFIG:-Release}"

cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE="$config" "$@"
cmake --build "$build_dir" --config "$config" --target NinjamNext_VST3 NinjamNext_AU
