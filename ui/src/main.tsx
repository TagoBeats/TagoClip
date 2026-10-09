// First import on purpose: the shims have to be installed before React or any
// component module is evaluated.
import './legacy-webkit.ts'
import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import App from './App.tsx'
import './fonts/fonts.css'
import { fitToViewport } from './fit.ts'

// Has to run before first paint, otherwise the canvas flashes at 1x in a
// smaller host window.
fitToViewport()

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>,
)
