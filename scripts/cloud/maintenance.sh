#!/usr/bin/env bash
set -euo pipefail

for tool in cmake ninja g++; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "Error: required tool '$tool' was not found in PATH." >&2
    exit 1
  fi
done

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../.." && pwd)"
cd "$repo_root"

cmake --preset linux-gcc-core-debug
cmake --build --preset linux-gcc-core-debug
ctest --preset linux-gcc-core-debug --no-tests=error
