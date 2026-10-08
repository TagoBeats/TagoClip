import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  // The plugin UI runs in the host OS WebView, not in a browser the user can
  // update. On macOS 10.13 that is WebKit from Safari 13.1, and the default
  // Vite target emits syntax it cannot parse: logical assignment (??=, ||=,
  // &&=, Safari 14.0) and the private class field in JUCE's own index.js
  // (Safari 14.1). Both only fail at parse time, so the whole UI comes up
  // blank with no error the user can see. esbuild lowers both for safari13.
  //
  // Keep this in sync with the CSS fallbacks in src/App.css, which cover the
  // other half of the problem (flex gap and inset are Safari 14.1).
  // safari12 rather than safari13: 10.13 tops out at Safari 13.1, but only if
  // the machine is fully updated. Targeting 12 also lowers optional chaining
  // and costs nothing measurable in bundle size.
  build: {
    target: ['safari12', 'chrome87', 'edge88', 'firefox78'],
  },
})
