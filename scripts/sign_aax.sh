#!/usr/bin/env bash
# Wrap and sign the TagoClip AAX bundle with PACE wraptool (needs the iLok stick).
#
# Input:  the unsigned bundle from the Release build (default below, or pass a path)
# Output: build/aax-signed/TagoClip.aaxplugin
#
# build_release.sh expects exactly that output. Run this after the Release build
# and before the pkg is built. Never run codesign on the result afterwards, that
# would break the PACE signature.
#
# Usage:  scripts/sign_aax.sh [path/to/unsigned/TagoClip.aaxplugin]
#
# The iLok password is asked for interactively (no echo). wraptool has no prompt
# of its own, so it is passed with --password and is briefly visible in the
# process list on this machine. It is never written to a file or printed.
set -euo pipefail

cd "$(dirname "$0")/.."

PRODUCT="TagoClip"
WCGUID="${TAGOCLIP_WCGUID:-833F2400-DF5A-11F1-86B0-005056920FF7}"
# SHA1 of "Developer ID Application: Robin Busse (3CU95LXM7N)". Always the hash,
# never a name: a second, unrelated Developer ID sits in the same keychain.
SIGN_ID="${AAX_SIGN_ID:-177EA2FD8EF8C1BDE0E4F0692BA448F57B748065}"
ILOK_ACCOUNT="${ILOK_ACCOUNT:-ice_rob}"
WRAPTOOL="${WRAPTOOL:-/Applications/PACEAntiPiracy/Eden/Fusion/Versions/6/bin/wraptool}"

IN="${1:-build-release/${PRODUCT}_artefacts/Release/AAX/${PRODUCT}.aaxplugin}"
OUT_DIR="build/aax-signed"
OUT="$OUT_DIR/${PRODUCT}.aaxplugin"

[[ -x "$WRAPTOOL" ]] || { echo "wraptool not found at $WRAPTOOL" >&2; exit 1; }
[[ -d "$IN" ]] || { echo "Unsigned bundle not found: $IN (run the Release build first)" >&2; exit 1; }

echo "==> Unsigned bundle: $IN"
lipo -archs "$IN/Contents/MacOS/${PRODUCT}"

# Silent password prompt that works in bash and zsh (read -s differs between them).
printf 'iLok password for %s (input hidden): ' "$ILOK_ACCOUNT" >&2
trap 'stty echo 2>/dev/null || true' EXIT INT TERM
stty -echo
IFS= read -r ILOK_PW || true
stty echo
printf '\n' >&2
[[ -n "$ILOK_PW" ]] || { echo "Empty password, aborting" >&2; exit 1; }

mkdir -p "$OUT_DIR"
rm -rf "$OUT"

echo "==> Signing with wraptool (iLok stick must be plugged in)"
set +e
WRAP_OUT="$("$WRAPTOOL" sign --verbose \
    --account "$ILOK_ACCOUNT" --password "$ILOK_PW" \
    --wcguid "$WCGUID" --signid "$SIGN_ID" \
    --in "$IN" --out "$OUT" --autoinstall on 2>&1)"
WRAP_RC=$?
set -e
# Print wraptool output with the password masked, then drop the variable.
printf '%s\n' "${WRAP_OUT//"$ILOK_PW"/********}"
unset ILOK_PW

if [[ $WRAP_RC -ne 0 || ! -d "$OUT" ]]; then
    echo "" >&2
    echo "ERROR: wraptool failed (exit $WRAP_RC)." >&2
    echo "Hint: Run Synchronize on the iLok in iLok License Manager, check that the stick is plugged in and the password is right." >&2
    exit 1
fi

echo "==> Verifying signature"
codesign --verify --deep --strict --verbose=2 "$OUT"
codesign -dvv "$OUT" 2>&1 | grep '^Authority' || { echo "No Authority line found" >&2; exit 1; }

echo "==> Signed bundle: $OUT"
