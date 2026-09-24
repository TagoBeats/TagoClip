// Fit the fixed design canvas to whatever viewport the host hands us.
//
// The UI is drawn on a fixed 620x380 canvas (see #plugin in App.css). Hosts do
// not reliably give the editor that exact size: with display scaling the WebView
// viewport ends up smaller in CSS pixels (620 / 1.25 = 496), and some hosts
// simply hand over a different window. Without this the page keeps its 560x360
// and the right and bottom edges are cut off behind `overflow: hidden`.
//
// The same clipping was reported for TagoPitch on 24.09.2026 (Cakewalk Sonar,
// Fender Studio Pro). TagoClip has not been reported yet, it shares the fixed
// canvas and reproduces identically with `node scripts/ui_size_check.mjs 496 304`.
//
// Scaling the whole canvas keeps the approved layout intact at any size, and it
// is what makes the editor resizable: the host may pick any window on the design
// aspect ratio and the UI follows.

export const DESIGN = { width: 620, height: 380 } as const;

export function fitToViewport(design: { width: number; height: number } = DESIGN): () => void {
  const apply = () => {
    const vw = window.innerWidth;
    const vh = window.innerHeight;
    const scale = Math.min(vw / design.width, vh / design.height);
    // A zero viewport happens while a host is still sizing its window; keeping
    // the last good values avoids a visible collapse to nothing.
    if (!(scale > 0)) return;

    // Centring is computed here rather than left to the layout: a grid or flex
    // container start-aligns an item that is larger than itself, which puts the
    // canvas off centre at exactly the small sizes this is meant to fix.
    const style = document.documentElement.style;
    style.setProperty("--ui-scale", String(scale));
    style.setProperty("--ui-x", `${(vw - design.width * scale) / 2}px`);
    style.setProperty("--ui-y", `${(vh - design.height * scale) / 2}px`);
  };

  apply();

  // The resize event alone misses host-driven resizes that do not go through
  // the window, which is exactly the case a WebView in a plugin window hits.
  const observer = new ResizeObserver(apply);
  observer.observe(document.documentElement);
  window.addEventListener("resize", apply);

  return () => {
    observer.disconnect();
    window.removeEventListener("resize", apply);
  };
}
