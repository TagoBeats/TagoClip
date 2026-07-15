#!/usr/bin/env bash
# Build a signed + notarized TagoClip release installer for macOS.
#
# Output: dist/TagoClip-X.Y.Z.dmg — a signed, notarized disk image wrapping
# the distribution pkg (itself signed, notarized and stapled separately, so
# it stays valid if unzipped/copied out of the dmg). The pkg has a format
# choice screen (VST3 / AU), welcome, license and conclusion pages, installing
#   /Library/Audio/Plug-Ins/VST3/TagoClip.vst3
#   /Library/Audio/Plug-Ins/Components/TagoClip.component
# as a universal binary (arm64 + x86_64).
#
# Mounting a dmg isn't a Downloads-folder write, so Installer.app never shows
# the "wants access to your Downloads folder" TCC prompt that a bare pkg
# triggers when opened straight out of Downloads.
#
# Prerequisites:
#   - Developer ID Application + Developer ID Installer certs in Keychain
#   - notarytool keychain profile (default: DubCheck-Notarize), created via
#       xcrun notarytool store-credentials "DubCheck-Notarize" \
#           --apple-id <apple-id> --team-id 3CU95LXM7N --password <app-specific>
#
# Env overrides:
#   SKIP_NOTARIZE=1   sign + pkg/dmg only
#   SKIP_SIGN=1       unsigned local test build (pkg only, no dmg)
set -euo pipefail

cd "$(dirname "$0")/.."
REPO_ROOT="$(pwd)"

VERSION="$(sed -n 's/^project(TagoClip VERSION \([0-9.]*\))/\1/p' CMakeLists.txt)"
[[ -n "$VERSION" ]] || { echo "Could not parse version from CMakeLists.txt" >&2; exit 1; }

APP_SIGNING_ID="${APP_SIGNING_ID:-Developer ID Application: Robin Busse (3CU95LXM7N)}"
PKG_SIGNING_ID="${PKG_SIGNING_ID:-Developer ID Installer: Robin Busse (3CU95LXM7N)}"
NOTARIZE_PROFILE="${NOTARIZE_PROFILE:-DubCheck-Notarize}"

BUILD_DIR="build-release"
PKG_NAME="TagoClip-$VERSION.pkg"
DMG_NAME="TagoClip-$VERSION.dmg"

echo "==> TagoClip $VERSION release build"

echo "==> Building UI bundle (ui/webui.zip)"
bash scripts/build-ui.sh

echo "==> Configuring + building Release (VST3 + AU, universal binary)"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DTAGOCLIP_DEV_UI=OFF \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build "$BUILD_DIR" --target TagoClip_VST3 TagoClip_AU -j "$(sysctl -n hw.ncpu)"

ART="$BUILD_DIR/TagoClip_artefacts/Release"
VST3="$ART/VST3/TagoClip.vst3"
AU="$ART/AU/TagoClip.component"
[[ -d "$VST3" && -d "$AU" ]] || { echo "Build artefacts missing" >&2; exit 1; }

echo "==> Verifying universal binary"
lipo "$VST3/Contents/MacOS/TagoClip" -verify_arch arm64 x86_64
lipo "$AU/Contents/MacOS/TagoClip" -verify_arch arm64 x86_64

echo "==> Embedding third-party attribution in the bundles"
for BUNDLE in "$VST3" "$AU"; do
    mkdir -p "$BUNDLE/Contents/Resources"
    cp THIRD_PARTY_LICENSES.txt "$BUNDLE/Contents/Resources/"
done

if [[ "${SKIP_SIGN:-0}" != "1" ]]; then
    echo "==> Codesigning bundles (hardened runtime)"
    for BUNDLE in "$VST3" "$AU"; do
        codesign --force --options runtime --timestamp \
            --sign "$APP_SIGNING_ID" "$BUNDLE"
        codesign --verify --deep --strict --verbose=2 "$BUNDLE"
    done
fi

echo "==> Building component packages"
PKGS="$BUILD_DIR/pkgs"
rm -rf "$PKGS"
mkdir -p "$PKGS/root-vst3/Library/Audio/Plug-Ins/VST3" \
         "$PKGS/root-au/Library/Audio/Plug-Ins/Components"
cp -R "$VST3" "$PKGS/root-vst3/Library/Audio/Plug-Ins/VST3/"
cp -R "$AU" "$PKGS/root-au/Library/Audio/Plug-Ins/Components/"

pkgbuild --root "$PKGS/root-vst3" --identifier com.tagobeats.tagoclip.vst3 \
    --version "$VERSION" --install-location "/" "$PKGS/TagoClip-VST3.pkg"
pkgbuild --root "$PKGS/root-au" --identifier com.tagobeats.tagoclip.au \
    --version "$VERSION" --install-location "/" "$PKGS/TagoClip-AU.pkg"

echo "==> Writing distribution definition"
cat > "$PKGS/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>TagoClip $VERSION</title>
    <welcome file="welcome.html"/>
    <license file="license.html"/>
    <conclusion file="conclusion.html"/>
    <options customize="always" require-scripts="false" rootVolumeOnly="true"
             hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>
    <choice id="vst3" title="VST3 plugin"
            description="For FL Studio, Cubase, Studio One, Reaper and most other DAWs. Installs to /Library/Audio/Plug-Ins/VST3."
            start_selected="true">
        <pkg-ref id="com.tagobeats.tagoclip.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit (AU) plugin"
            description="For Logic Pro and GarageBand. Installs to /Library/Audio/Plug-Ins/Components."
            start_selected="true">
        <pkg-ref id="com.tagobeats.tagoclip.au"/>
    </choice>
    <pkg-ref id="com.tagobeats.tagoclip.vst3" version="$VERSION">TagoClip-VST3.pkg</pkg-ref>
    <pkg-ref id="com.tagobeats.tagoclip.au" version="$VERSION">TagoClip-AU.pkg</pkg-ref>
</installer-gui-script>
EOF

mkdir -p dist
PRODUCT_ARGS=(
    --distribution "$PKGS/distribution.xml"
    --resources installer/resources
    --package-path "$PKGS"
    "dist/$PKG_NAME"
)
if [[ "${SKIP_SIGN:-0}" != "1" ]]; then
    echo "==> productbuild → dist/$PKG_NAME (signed)"
    productbuild --sign "$PKG_SIGNING_ID" "${PRODUCT_ARGS[@]}"
else
    echo "==> productbuild → dist/$PKG_NAME (UNSIGNED — SKIP_SIGN=1)"
    productbuild "${PRODUCT_ARGS[@]}"
fi

if [[ "${SKIP_NOTARIZE:-0}" != "1" && "${SKIP_SIGN:-0}" != "1" ]]; then
    echo "==> Notarizing pkg (this can take a few minutes)"
    xcrun notarytool submit "dist/$PKG_NAME" \
        --keychain-profile "$NOTARIZE_PROFILE" \
        --wait
    echo "==> Stapling ticket to pkg"
    xcrun stapler staple "dist/$PKG_NAME"
    xcrun stapler validate "dist/$PKG_NAME"
else
    echo "==> Notarization skipped"
fi

if [[ "${SKIP_SIGN:-0}" != "1" ]]; then
    echo "==> Wrapping stapled pkg in a signed dmg"
    DMG_STAGING="$BUILD_DIR/dmg-staging"
    rm -rf "$DMG_STAGING"
    mkdir -p "$DMG_STAGING"
    cp "dist/$PKG_NAME" "$DMG_STAGING/"
    rm -f "dist/$DMG_NAME"
    hdiutil create -volname "TagoClip $VERSION" -srcfolder "$DMG_STAGING" \
        -ov -format UDZO "dist/$DMG_NAME"

    echo "==> Codesigning dmg"
    codesign --force --sign "$APP_SIGNING_ID" "dist/$DMG_NAME"
    codesign --verify --verbose=2 "dist/$DMG_NAME"

    if [[ "${SKIP_NOTARIZE:-0}" != "1" ]]; then
        echo "==> Notarizing dmg (this can take a few minutes)"
        xcrun notarytool submit "dist/$DMG_NAME" \
            --keychain-profile "$NOTARIZE_PROFILE" \
            --wait
        echo "==> Stapling ticket to dmg"
        xcrun stapler staple "dist/$DMG_NAME"
        xcrun stapler validate "dist/$DMG_NAME"
    fi
else
    echo "==> Dmg wrap skipped (SKIP_SIGN=1, pkg is unsigned)"
fi

PKG_SIZE="$(du -h "dist/$PKG_NAME" | cut -f1 | tr -d ' ')"
echo "==> Done. Installer pkg: dist/$PKG_NAME ($PKG_SIZE)"
if [[ -f "dist/$DMG_NAME" ]]; then
    DMG_SIZE="$(du -h "dist/$DMG_NAME" | cut -f1 | tr -d ' ')"
    echo "==> Distribute this one: dist/$DMG_NAME ($DMG_SIZE)"
fi
