#!/usr/bin/env bash
set -euo pipefail

echo "[cloud] preparing C++ toolchain"

if command -v apt-get >/dev/null 2>&1; then
  APT=(apt-get)
  if command -v sudo >/dev/null 2>&1; then
    APT=(sudo -n apt-get)
  fi

  "${APT[@]}" update
  DEBIAN_FRONTEND=noninteractive "${APT[@]}" install -y --no-install-recommends \
    build-essential \
    clang \
    clang-format \
    clang-tidy \
    cmake \
    git \
    ninja-build \
    pkg-config \
    python3 \
    python3-pip
fi

cmake --version
git --version
python3 --version

echo "[cloud] environment ready"
