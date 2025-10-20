#!/usr/bin/env bash

set -euo pipefail

missing=()

command -v cmake >/dev/null 2>&1 || missing+=("cmake")
command -v ninja >/dev/null 2>&1 || missing+=("ninja (optional, improves build speed)")

if [[ ${#missing[@]} -eq 0 ]]; then
  echo "Toolchain check passed. CMake and optional Ninja detected."
else
  echo "Missing tools detected:"
  for tool in "${missing[@]}"; do
    echo "  - ${tool}"
  done
  cat <<'EOF'

Install on macOS (Homebrew):
  brew install cmake ninja

Install on Debian/Ubuntu:
  sudo apt-get update && sudo apt-get install -y cmake ninja-build

Install on Windows (winget):
  winget install Kitware.CMake Ninja-build.Ninja

EOF
  exit 1
fi
