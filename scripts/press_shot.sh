#!/usr/bin/env bash
# Render the plugin UI to a sharp, reproducible press screenshot.
#
# The UI is a WebView front end, so ui/dist rendered in Chrome is pixel
# identical to the plugin window. Two things have to be forced, otherwise
# the shot is not reproducible:
#
#   1. Outside JUCE the meters run a random-walk idle animation
#      (Meters.tsx). Math.random is pinned so the level is the same on
#      every run instead of whatever the walk happened to hit.
#   2. INIT has drive 0 and threshold 100, so nothing is being clipped. The
#      script clicks through to the preset named in PRESET_CLICKS.
#
# Nothing in ui/dist is modified: the injection happens in a temp copy.

set -euo pipefail

NAME="TagoClip"
WIDTH=620          # PluginEditor.cpp setSize
HEIGHT=380
SCALE=3            # 3x gives 1860x1140, enough for forum and press use
PRESET_CLICKS=1    # INIT -> 808 GLUE
PORT=8931

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST="$REPO/ui/dist"
OUT="$REPO/press"
CHROME="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

[ -f "$DIST/index.html" ] || { echo "no ui/dist, run scripts/build-ui.sh first" >&2; exit 1; }
[ -x "$CHROME" ] || { echo "Google Chrome not found at $CHROME" >&2; exit 1; }

mkdir -p "$OUT"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"; [ -n "${SRV:-}" ] && kill "$SRV" 2>/dev/null || true' EXIT

cp -R "$DIST/." "$TMP/"

python3 - "$TMP/index.html" "$PRESET_CLICKS" <<'PY'
import sys

path, clicks = sys.argv[1], int(sys.argv[2])
html = open(path, encoding="utf-8").read()

# Runs before the deferred module script, so Math.random is already pinned
# when the app boots and starts the meter animation.
# The clicks must be spaced out. applyPreset reads presetIdx from the
# render closure, so a synchronous click loop makes every click compute
# the same next index and only one step lands.
inject = """<script>
Math.random = function () { return 0.5; };
window.addEventListener('load', function () {
  var tries = 0;
  (function poll() {
    var next = document.getElementById('next');
    if (next) { return step(0); }
    if (tries++ < 200) setTimeout(poll, 25);
  })();
  function step(i) {
    if (i >= %d) return;
    document.getElementById('next').click();
    setTimeout(function () { step(i + 1); }, 150);
  }
});
</script>""" % clicks

marker = "</head>"
if marker not in html:
    raise SystemExit("no </head> in dist/index.html")
open(path, "w", encoding="utf-8").write(html.replace(marker, inject + marker, 1))
PY

cd "$TMP"
python3 -m http.server "$PORT" >/dev/null 2>&1 &
SRV=$!
sleep 1.2

"$CHROME" --headless=new --disable-gpu --hide-scrollbars \
  --force-device-scale-factor="$SCALE" \
  --window-size="$WIDTH,$HEIGHT" \
  --virtual-time-budget=5000 \
  --screenshot="$OUT/${NAME}_ui.png" \
  "http://localhost:$PORT/index.html" >/dev/null 2>&1

echo "$OUT/${NAME}_ui.png"
