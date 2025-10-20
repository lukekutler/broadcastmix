#!/usr/bin/env bash

set -euo pipefail

build_dir="${BROADCASTMIX_BUILD_DIR:-build}"

cmake -S . -B "${build_dir}" -G Ninja
cmake --build "${build_dir}" --target BroadcastMixApp

app_path="${build_dir}/apps/BroadcastMixApp_artefacts/BroadcastMix.app"

if [[ "${OSTYPE:-}" == darwin* ]]; then
    open "${app_path}"
else
    echo "BroadcastMixApp built at: ${app_path}"
fi
