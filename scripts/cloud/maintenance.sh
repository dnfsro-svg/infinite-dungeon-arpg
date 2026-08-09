#!/usr/bin/env bash
set -euo pipefail

# Cached Codex containers only need a cheap health check. Dependency installation
# belongs in setup.sh because setup runs with internet access.
cmake --version >/dev/null
git --version >/dev/null
python3 --version >/dev/null

echo "[cloud] cached environment is healthy"
