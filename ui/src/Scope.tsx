// Transfer-curve display, the hero of the plugin window. Ported from the
// scope logic in mockup/index.html (drawScope + animateScope): a static SVG
// path for the curve plus a live signal dot. In the plugin the dot rides the
// curve at the real input peak (drive applied, "levels" events from the audio
// thread); in a plain browser the mockup's idle 808 animation runs instead.

import { useEffect, useRef } from "react";
import { inJuce, onLevels } from "./bridge";
import { CURVE_FN, type CurveKey } from "./params";

const SW = 288;
const SH = 178;
const XMAX = 2.2;

function sx(v: number) {
  return SW / 2 + (v / XMAX) * (SW / 2 - 10);
}
function sy(v: number) {
  return SH / 2 - (v / 1.25) * (SH / 2 - 8);
}

function curvePath(fn: (x: number, t: number) => number, t: number, steps = 160): string {
  let path = "";
  for (let i = 0; i <= steps; i++) {
    const x = -XMAX + (2 * XMAX * i) / steps;
    path += (i ? "L" : "M") + sx(x) + " " + sy(fn(x, t)) + " ";
  }
  return path;
}

function deltaPath(fn: (x: number, t: number) => number, t: number, steps = 160): string {
  let d = "";
  for (let i = 0; i <= steps; i++) {
    const x = -XMAX + (2 * XMAX * i) / steps;
    d += (i ? "L" : "M") + sx(x) + " " + sy(x) + " ";
  }
  for (let i = steps; i >= 0; i--) {
    const x = -XMAX + (2 * XMAX * i) / steps;
    d += "L" + sx(x) + " " + sy(fn(x, t)) + " ";
  }
  return d + "Z";
}

export default function Scope({
  curve,
  thresholdSteps,
  driveDb,
  delta,
}: {
  curve: CurveKey;
  thresholdSteps: number;
  driveDb: number;
  delta: boolean;
}) {
  const t = thresholdSteps / 128;
  const fn = CURVE_FN[curve];

  const dotP = useRef<SVGCircleElement>(null);
  const dotN = useRef<SVGCircleElement>(null);
  const phase = useRef(0);
  const stateRef = useRef({ t, fn, driveDb, delta });
  stateRef.current = { t, fn, driveDb, delta };

  useEffect(() => {
    let raf = 0;
    const level = { in: 0 };
    const unsub = onLevels((l) => (level.in = l.in));
    let shown = 0; // smoothed input peak so the dot glides instead of stepping
    const tick = () => {
      const { t, fn, driveDb, delta } = stateRef.current;
      const drive = Math.pow(10, driveDb / 20);
      let x: number;
      if (inJuce) {
        // dot position = driven input peak, mirrored on the negative side
        const target = level.in;
        shown = target > shown ? shown + (target - shown) * 0.4 : shown * 0.92;
        x = Math.min(XMAX, drive * shown);
      } else {
        // browser fallback: the mockup's idle 808 envelope
        const beat = (Date.now() / 600) % 1;
        const env = Math.exp(-beat * 4.5);
        phase.current += 0.11;
        x = Math.min(XMAX, Math.max(-XMAX, drive * env * Math.sin(phase.current)));
      }
      const y = fn(x, t);
      const yShown = delta ? x - y : y;
      if (dotP.current) {
        dotP.current.setAttribute("cx", String(sx(x)));
        dotP.current.setAttribute("cy", String(sy(yShown)));
      }
      if (dotN.current) {
        const yn = delta ? -x - fn(-x, t) : fn(-x, t);
        dotN.current.setAttribute("cx", String(sx(-x)));
        dotN.current.setAttribute("cy", String(sy(yn)));
      }
      raf = requestAnimationFrame(tick);
    };
    tick();
    return () => {
      cancelAnimationFrame(raf);
      unsub();
    };
  }, []);

  return (
    <svg id="scope" viewBox={`0 0 ${SW} ${SH}`}>
      {/* grid: unity diagonal + zero lines */}
      <line
        x1={sx(-XMAX)} y1={sy(-XMAX)} x2={sx(XMAX)} y2={sy(XMAX)}
        stroke="rgba(236,231,222,0.10)" strokeWidth={1} strokeDasharray="3 4"
      />
      <line x1={sx(-XMAX)} y1={sy(0)} x2={sx(XMAX)} y2={sy(0)} stroke="rgba(236,231,222,0.07)" strokeWidth={1} />
      <line x1={sx(0)} y1={sy(-1.25)} x2={sx(0)} y2={sy(1.25)} stroke="rgba(236,231,222,0.07)" strokeWidth={1} />
      {/* ceiling lines */}
      {[1, -1].map((cv) => (
        <line
          key={cv}
          x1={sx(-XMAX)} y1={sy(cv)} x2={sx(XMAX)} y2={sy(cv)}
          stroke="rgba(236,231,222,0.10)" strokeWidth={1} strokeDasharray="2 5"
        />
      ))}
      {/* threshold knee markers */}
      {[t, -t].map((tv) => (
        <line
          key={tv}
          x1={sx(tv)} y1={sy(-1.25)} x2={sx(tv)} y2={sy(1.25)}
          stroke="rgba(0,253,220,0.15)" strokeWidth={1}
        />
      ))}
      {/* delta region: the part the clipper removes */}
      {delta && <path d={deltaPath(fn, t)} fill="rgba(0,253,220,0.13)" stroke="none" />}
      {/* the transfer curve itself */}
      <path
        d={curvePath(fn, t)}
        fill="none"
        stroke="var(--accent)"
        strokeWidth={2}
        style={{ filter: "drop-shadow(0 0 6px rgba(0,253,220,0.5))" }}
      />
      <circle
        ref={dotP}
        r={3}
        fill="var(--text-hi)"
        style={{ filter: "drop-shadow(0 0 5px rgba(0,253,220,0.9))" }}
      />
      <circle
        ref={dotN}
        r={3}
        fill="var(--text-hi)"
        opacity={0.6}
        style={{ filter: "drop-shadow(0 0 5px rgba(0,253,220,0.9))" }}
      />
    </svg>
  );
}
