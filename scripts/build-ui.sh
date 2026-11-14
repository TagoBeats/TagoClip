#!/usr/bin/env bash
# Build the Vite UI and bundle it as ui/webui.zip for BinaryData.
set -euo pipefail

cd "$(dirname "$0")/../ui"

# Skip the rebuild when webui.zip is newer than every UI source. A fresh zip
# carries new timestamps, so the plugin binary relinks even if nothing changed,
# which would invalidate an already PACE-signed AAX. FORCE_UI=1 always rebuilds.
if [[ "${FORCE_UI:-0}" != "1" && -f webui.zip ]]; then
    newer=$(find src public index.html package.json package-lock.json vite.config.* \
        -newer webui.zip 2>/dev/null | head -n 1)
    if [[ -z "$newer" ]]; then
        echo "ui/webui.zip is up to date, skipping UI build"
        exit 0
    fi
fi

npm run build
rm -f webui.zip
# The Windows CI runner's git-bash has no `zip`; 7z is preinstalled there.
if command -v zip >/dev/null 2>&1; then
    (cd dist && zip -q -r ../webui.zip .)
else
    (cd dist && 7z a -tzip ../webui.zip . > /dev/null)
fi
echo "ui/webui.zip updated"
