import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import App from './App.tsx'
import './fonts/fonts.css'
import { fitToViewport } from './fit.ts'
import { installFocusRelease } from './bridge.ts'

// Has to run before first paint, otherwise the canvas flashes at 1x in a
// smaller host window.
fitToViewport()
installFocusRelease()

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>,
)
