#!/usr/bin/env bash
# Build the Vite UI and bundle it as ui/webui.zip for BinaryData.
set -euo pipefail

cd "$(dirname "$0")/../ui"
npm run build
rm -f webui.zip
# The Windows CI runner's git-bash has no `zip`; 7z is preinstalled there.
if command -v zip >/dev/null 2>&1; then
    (cd dist && zip -q -r ../webui.zip .)
else
    (cd dist && 7z a -tzip ../webui.zip . > /dev/null)
fi
echo "ui/webui.zip updated"
