#!/bin/sh
# Golden test: render the tagodsp references (once), then compare the C++ render.
# Usage: scripts/run_golden.sh [path-to-TagoClipRender] [--fresh]
set -e
cd "$(dirname "$0")/.."

PY="$HOME/Documents/tagodsp/.venv/bin/python"
BIN="${1:-build/TagoClipRender_artefacts/Release/TagoClipRender}"

if [ "$2" = "--fresh" ] || [ ! -f golden/refs/manifest.json ]; then
    "$PY" golden/make_reference.py
fi
"$PY" golden/compare.py "$BIN"
