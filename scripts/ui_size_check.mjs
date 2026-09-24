#!/usr/bin/env node
// Render ui/dist at an exact host viewport size and report whether the UI fits.
//
// Why this exists: a host does not always give the editor the size it asked
// for. With display scaling the WebView viewport ends up smaller in CSS pixels
// (620 / 1.25 = 496), and a fixed-size page gets cut off instead of adapting.
// This reproduces that from the outside, without a DAW.
//
// Chrome's --window-size cannot express this: it clamps the width at 500px and
// subtracts window chrome from the height, so a request for 448x360 silently
// becomes a 500x288 viewport. The size therefore goes through CDP's
// Emulation.setDeviceMetricsOverride, which is exact. Node 22+ has a native
// WebSocket, so driving CDP needs no dependencies.
//
// Usage: node scripts/ui_size_check.mjs WIDTH HEIGHT [--scale N] [--out FILE]
//   node scripts/ui_size_check.mjs 496 304        # the reported bug (620 @ 125%)
//   node scripts/ui_size_check.mjs 620 380        # the design size

import { spawn } from 'node:child_process'
import { createServer } from 'node:http'
import { mkdtemp, readFile, rm, writeFile, mkdir } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const REPO = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..')
const DIST = path.join(REPO, 'ui', 'dist')
const CHROME = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'

const args = process.argv.slice(2)
const width = Number(args[0])
const height = Number(args[1])
if (!width || !height) {
  console.error('usage: ui_size_check.mjs WIDTH HEIGHT [--scale N] [--out FILE]')
  process.exit(1)
}
const flag = (name, fallback) => {
  const i = args.indexOf(name)
  return i === -1 ? fallback : args[i + 1]
}
const scale = Number(flag('--scale', 2))
const out = path.resolve(flag('--out', path.join(REPO, 'press', `size_check_${width}x${height}.png`)))

const TYPES = {
  '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css',
  '.svg': 'image/svg+xml', '.woff2': 'font/woff2', '.png': 'image/png',
  '.json': 'application/json',
}

// Pin the meter idle walk (same as press_shot.sh) so two runs are comparable.
const PIN = '<script>Math.random = function () { return 0.5; };</script>'

const server = createServer(async (req, res) => {
  const rel = decodeURIComponent(new URL(req.url, 'http://x').pathname)
  const file = path.join(DIST, rel === '/' ? 'index.html' : rel)
  if (!file.startsWith(DIST)) return res.writeHead(403).end()
  try {
    let body = await readFile(file)
    if (file.endsWith('index.html')) body = Buffer.from(String(body).replace('</head>', PIN + '</head>'))
    res.writeHead(200, { 'content-type': TYPES[path.extname(file)] ?? 'application/octet-stream' }).end(body)
  } catch {
    res.writeHead(404).end()
  }
})
await new Promise((r) => server.listen(0, '127.0.0.1', r))
const port = server.address().port

const profile = await mkdtemp(path.join(tmpdir(), 'tp-uicheck-'))
const chrome = spawn(CHROME, [
  '--headless=new', '--disable-gpu', '--hide-scrollbars', '--no-first-run',
  `--user-data-dir=${profile}`, '--remote-debugging-port=0', 'about:blank',
], { stdio: ['ignore', 'ignore', 'pipe'] })

// Chrome prints the chosen debugging port on stderr.
const wsUrl = await new Promise((resolve, reject) => {
  let buf = ''
  const timer = setTimeout(() => reject(new Error('chrome did not report a debugging port')), 20000)
  chrome.stderr.on('data', (d) => {
    buf += d
    const m = buf.match(/ws:\/\/[^\s]+/)
    if (m) { clearTimeout(timer); resolve(m[0]) }
  })
  chrome.on('exit', (code) => reject(new Error(`chrome exited with ${code}`)))
})

class Cdp {
  constructor(ws) { this.ws = ws; this.id = 0; this.pending = new Map(); this.sessions = new Map()
    ws.addEventListener('message', (e) => {
      const msg = JSON.parse(e.data)
      const p = this.pending.get(msg.id)
      if (p) { this.pending.delete(msg.id); msg.error ? p.reject(new Error(JSON.stringify(msg.error))) : p.resolve(msg.result) }
    })
  }
  send(method, params = {}, sessionId) {
    const id = ++this.id
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject })
      this.ws.send(JSON.stringify({ id, method, params, ...(sessionId ? { sessionId } : {}) }))
    })
  }
}

const open = (url) => new Promise((resolve, reject) => {
  const ws = new WebSocket(url)
  ws.addEventListener('open', () => resolve(ws))
  ws.addEventListener('error', reject)
})

const browser = new Cdp(await open(wsUrl))
const { targetId } = await browser.send('Target.createTarget', { url: 'about:blank' })
const { sessionId } = await browser.send('Target.attachToTarget', { targetId, flatten: true })
const page = (method, params) => browser.send(method, params, sessionId)

// The exact viewport --window-size cannot give us.
await page('Emulation.setDeviceMetricsOverride', {
  width, height, deviceScaleFactor: scale, mobile: false,
})
await page('Page.enable')
await page('Page.navigate', { url: `http://127.0.0.1:${port}/index.html` })
await new Promise((r) => setTimeout(r, 2500))

const { result } = await page('Runtime.evaluate', {
  expression: `(() => {
    const el = document.getElementById('plugin')
    const r = el ? el.getBoundingClientRect() : { width: 0, height: 0 }
    const d = document.documentElement
    return JSON.stringify({
      viewport: [window.innerWidth, window.innerHeight],
      canvas: [Math.round(r.width), Math.round(r.height)],
      cut: [Math.max(0, Math.round(r.right) - window.innerWidth),
            Math.max(0, Math.round(r.bottom) - window.innerHeight)],
      overflow: [d.scrollWidth - window.innerWidth, d.scrollHeight - window.innerHeight],
    })
  })()`,
  returnByValue: true,
})
const m = JSON.parse(result.value)

const shot = await page('Page.captureScreenshot', { format: 'png', captureBeyondViewport: false })
await mkdir(path.dirname(out), { recursive: true })
await writeFile(out, Buffer.from(shot.data, 'base64'))

const [vw, vh] = m.viewport
const [cw, ch] = m.canvas
const [cutX, cutY] = m.cut
console.log(out)
console.log(`  viewport  ${vw}x${vh} css px @ ${scale}x`)
console.log(`  canvas    ${cw}x${ch} css px`)
console.log(cutX || cutY
  ? `  CLIPPED   ${cutX}px cut on the right, ${cutY}px cut at the bottom`
  : `  fits`)

server.close()
// Chrome keeps writing into the profile for a moment after kill(), so wait for
// it to actually exit before removing the directory.
const exited = new Promise((r) => chrome.on('exit', r))
chrome.kill()
await exited
await rm(profile, { recursive: true, force: true }).catch(() => {})
