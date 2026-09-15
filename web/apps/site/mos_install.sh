#!/usr/bin/env sh

set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
VENV_DIR="${MOS_VENV_DIR:-$HOME/.cache/gigadenoiser/mos-venv}"

python3 --version
python3 -m venv "$VENV_DIR"
"$VENV_DIR/bin/pip" install --no-cache-dir --requirement "$SCRIPT_DIR/requirements-mos.txt"
