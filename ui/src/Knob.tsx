// React port of the mockup Knob class (mockup/index.html): 270-degree sweep,
// bipolar knobs fill from the 12 o'clock detent, hardware look (variant B).
// Geometry is a 1:1 copy; interaction goes through the JUCE relay handle.

import { useEffect, useMemo, useRef, useState } from "react";
import type { ParamSpec } from "./params";
import type { ParamHandle } from "./bridge";

const SWEEP = 270; // degrees
const START = -135; // pointer angle at min

function polar(cx: number, cy: number, r: number, deg: number): [number, number] {
  const rad = ((deg - 90) * Math.PI) / 180;
  return [cx + r * Math.cos(rad), cy + r * Math.sin(rad)];
}

function arcPath(cx: number, cy: number, r: number, a0: number, a1: number): string {
  if (Math.abs(a1 - a0) < 0.01) return "";
  const [x0, y0] = polar(cx, cy, r, a0);
  const [x1, y1] = polar(cx, cy, r, a1);
  const large = Math.abs(a1 - a0) > 180 ? 1 : 0;
  const swp = a1 > a0 ? 1 : 0;
  return `M ${x0} ${y0} A ${r} ${r} 0 ${large} ${swp} ${x1} ${y1}`;
}

export default function Knob({ spec, param }: { spec: ParamSpec; param: ParamHandle }) {
  const [value, setValue] = useState(param.getScaled());
  const drag = useRef({ startY: 0, startVal: 0 });

  useEffect(() => {
    // re-read on mount: the host pushes real values only after page load
    setValue(param.getScaled());
    return param.subscribe(() => setValue(param.getScaled()));
  }, [param]);

  // Knob position is normalised 0..1. With spec.center the mapping is piecewise
  // so the centre value lands at 0.5, i.e. straight up, on an asymmetric range.
  const toNorm = (v: number) => {
    const c = spec.center;
    if (c === undefined) return (v - spec.min) / (spec.max - spec.min);
    return v <= c
      ? (0.5 * (v - spec.min)) / (c - spec.min)
      : 0.5 + (0.5 * (v - c)) / (spec.max - c);
  };
  const fromNorm = (n: number) => {
    const c = spec.center;
    if (c === undefined) return spec.min + n * (spec.max - spec.min);
    return n <= 0.5
      ? spec.min + (n / 0.5) * (c - spec.min)
      : c + ((n - 0.5) / 0.5) * (spec.max - c);
  };

  const set = (v: number, fine: boolean) => {
    v = Math.min(spec.max, Math.max(spec.min, v));
    if (spec.step) v = Math.round(v / spec.step) * spec.step;
    else if (!fine) v = Math.round(v * 100) / 100;
    param.setScaled(v);
    setValue(v);
  };

  const range = spec.max - spec.min;

  const onPointerDown = (e: React.PointerEvent) => {
    drag.current = { startY: e.clientY, startVal: value };
    (e.target as Element).setPointerCapture(e.pointerId);
    param.dragStarted();
  };
  const onPointerMove = (e: React.PointerEvent) => {
    if (!(e.target as Element).hasPointerCapture?.(e.pointerId)) return;
    const fine = e.shiftKey;
    const px = fine ? 900 : 220; // pixels for full sweep
    // Drag in knob space, not in value space, so the two halves of a centred
    // knob each get half the travel. Identical to before for linear knobs.
    const moved = toNorm(drag.current.startVal) + (drag.current.startY - e.clientY) / px;
    set(fromNorm(Math.min(1, Math.max(0, moved))), fine);
  };
  const onPointerUp = (e: React.PointerEvent) => {
    (e.target as Element).releasePointerCapture?.(e.pointerId);
    param.dragEnded();
  };
  const onDoubleClick = () => set(spec.def, false);

  const rootRef = useRef<HTMLDivElement>(null);
  useEffect(() => {
    // native listener: React's onWheel is passive, preventDefault needs this
    const el = rootRef.current;
    if (!el) return;
    const onWheel = (e: WheelEvent) => {
      e.preventDefault();
      const fine = e.shiftKey;
      const inc = spec.step || range / 48;
      set(param.getScaled() - Math.sign(e.deltaY) * (fine ? inc / 4 : inc), fine);
    };
    el.addEventListener("wheel", onWheel, { passive: false });
    return () => el.removeEventListener("wheel", onWheel);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [param, spec]);

  // ---- geometry, straight from the mockup ----
  const s = spec.size;
  const c = s / 2;
  const r = s / 2 - (spec.ringInset ?? 7);
  const small = s < 80;
  const nTicks = small ? 12 : 24;
  const showTicks = spec.ticks ?? true;
  const gid = `face-${spec.label}`;

  const ticks = useMemo(() => {
    const out: React.ReactElement[] = [];
    if (!showTicks) return out;
    for (let i = 0; i <= nTicks; i++) {
      const ang = START + (i / nTicks) * SWEEP;
      const major = i % (nTicks / 4) === 0;
      const [x1, y1] = polar(c, c, r + (major ? 1 : 2), ang);
      const [x2, y2] = polar(c, c, r + 5, ang);
      out.push(
        <line key={i} className="tick" x1={x1} y1={y1} x2={x2} y2={y2} strokeWidth={major ? 1.6 : 1} />
      );
    }
    return out;
  }, [c, r, nTicks, showTicks]);

  const norm = toNorm(value);
  const ang = START + norm * SWEEP;
  const from = spec.bipolar ? 0 : START; // bipolar fills from 12 o'clock
  const [px0, py0] = polar(c, c, r * 0.42, ang);
  const [px1, py1] = polar(c, c, r * 0.7, ang);
  const [dx, dy] = polar(c, c, r, 0);

  return (
    <div
      className="knob-unit"
      id={spec.label}
      ref={rootRef}
      onPointerDown={onPointerDown}
      onPointerMove={onPointerMove}
      onPointerUp={onPointerUp}
      onDoubleClick={onDoubleClick}
    >
      <svg width={s} height={s} viewBox={`0 0 ${s} ${s}`}>
        <defs>
          <radialGradient id={`${gid}-deep`} cx="0.5" cy="0.28" r="0.9">
            <stop offset="0" stopColor="#2E2921" />
            <stop offset="0.45" stopColor="#1C1916" />
            <stop offset="0.85" stopColor="#0E0C0B" />
            <stop offset="1" stopColor="#080707" />
          </radialGradient>
          <linearGradient id={`${gid}-bezel`} x1="0" y1="0" x2="0" y2="1">
            <stop offset="0" stopColor="#3A352C" />
            <stop offset="0.5" stopColor="#15130F" />
            <stop offset="1" stopColor="#2A261F" />
          </linearGradient>
        </defs>
        {ticks}
        <path className="track" d={arcPath(c, c, r, START, START + SWEEP)} strokeWidth={3} />
        <path
          className="fill"
          d={arcPath(c, c, r, Math.min(from, ang), Math.max(from, ang))}
          strokeWidth={3}
        />
        <circle className="bezel" cx={c} cy={c} r={r * 0.85} fill={`url(#${gid}-bezel)`} strokeWidth={1} />
        <circle className="face" cx={c} cy={c} r={r * 0.72} fill={`url(#${gid}-deep)`} />
        {spec.bipolar && <circle className="detent" cx={dx} cy={dy - 4} r={1.6} />}
        <line className="pointer" x1={px0} y1={py0} x2={px1} y2={py1} strokeWidth={small ? 3 : 4} />
      </svg>
      <div className="knob-value">{spec.fmt(value)}</div>
      <div className="knob-label">{spec.label}</div>
    </div>
  );
}
