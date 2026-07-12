#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

CROSS_PREFIX="${CROSS:-}"

echo "[setup] clean"
make clean

echo "[setup] build"
make CROSS="$CROSS_PREFIX"

echo "[setup] run"
make run CROSS="$CROSS_PREFIX"
