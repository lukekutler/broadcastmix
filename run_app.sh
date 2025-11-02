#!/usr/bin/env bash

set -euo pipefail

build_dir="${BROADCASTMIX_BUILD_DIR:-build}"

cmake -S . -B "${build_dir}" -G Ninja
cmake --build "${build_dir}" --target BroadcastMixApp

app_path="${build_dir}/apps/BroadcastMixApp_artefacts/BroadcastMix.app"

if [[ "${OSTYPE:-}" == darwin* ]]; then
    # Run the executable directly to see console output
    # Use 'open' instead if you don't need to see debug logs
    "${app_path}/Contents/MacOS/BroadcastMix"
else
    echo "BroadcastMixApp built at: ${app_path}"
fi
