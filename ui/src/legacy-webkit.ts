// Shims for the oldest WebKit this UI can land in.
//
// The editor renders in the host OS WebView, not in a browser the user can
// update, so the floor is whatever Safari that macOS version tops out at. On
// 10.13 that is Safari 13.1 on a fully updated machine and Safari 11 on one
// that was never updated. Missing syntax is handled by `build.target` in
// vite.config.ts; missing *APIs* are not, and a ReferenceError during mount
// takes the whole UI down to a blank window with no visible error.
//
// Imported first in main.tsx so these exist before React or any component runs.

// queueMicrotask: Safari 12.1. React's scheduler reaches for it.
if (typeof globalThis.queueMicrotask !== "function") {
  const resolved = Promise.resolve();
  globalThis.queueMicrotask = (cb: VoidFunction) => {
    void resolved.then(cb).catch((err) => {
      // Match the spec: a throwing callback must not swallow itself into the
      // promise chain, it reports as an uncaught error on its own tick.
      setTimeout(() => {
        throw err;
      }, 0);
    });
  };
}

// Pointer capture: Safari 13. Without it a drag that leaves the element stops
// getting events from that element. The knobs already listen on the window for
// exactly this reason, so a no-op keeps them working instead of throwing.
if (typeof Element !== "undefined") {
  // Through an untyped alias on purpose: a direct `in` check on
  // Element.prototype narrows the type to `never` and the assignments stop
  // compiling, which is a type-system artefact and not a real constraint.
  const proto = Element.prototype as unknown as Record<string, unknown>;
  if (typeof proto.setPointerCapture !== "function") {
    proto.setPointerCapture = () => {};
    proto.releasePointerCapture = () => {};
    proto.hasPointerCapture = () => false;
  }
}

export {};
