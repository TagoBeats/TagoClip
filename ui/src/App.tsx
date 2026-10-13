// TagoClip v1 UI, ported 1:1 from mockup/index.html (approved 15.07.2026).
// Markup and class names match the mockup so the styles stay literal.

import { useEffect, useMemo, useState } from "react";
import Knob from "./Knob";
import Scope from "./Scope";
import Meters from "./Meters";
import GrReadout from "./GrReadout";
import { CURVE_CHIPS, OS_FACTORS, PARAMS, PRESETS, type CurveKey } from "./params";
import { makeParam, makeToggle, type ParamHandle, type ToggleHandle } from "./bridge";
import "./App.css";

// The host pushes the real parameter values only after the page is up, so a
// plain useState(initial read) would keep stale defaults (FL bug, 15.07.2026).
// Re-read on mount and on every change event.
function useScaled(param: ParamHandle): number {
  const [value, setValue] = useState(param.getScaled());
  useEffect(() => {
    setValue(param.getScaled());
    return param.subscribe(() => setValue(param.getScaled()));
  }, [param]);
  return value;
}

function useToggle(toggle: ToggleHandle): boolean {
  const [value, setValue] = useState(toggle.get());
  useEffect(() => {
    setValue(toggle.get());
    return toggle.subscribe(() => setValue(toggle.get()));
  }, [toggle]);
  return value;
}

// Which preset is loaded is read back off the parameter values instead of being
// remembered in React. The WebView dies with the plugin window, so a local
// index came back as INIT on every reopen while the sound was still the
// preset's: reported 14.09., 20.09. (Ableton 12.4.5) and 13.10.2026. Deriving
// it also stops the label from claiming a preset after a knob was moved.
// Returns -1 when nothing matches, which the header shows as CUSTOM.
function matchPreset(
  values: Record<string, number>,
  curve: CurveKey,
  osFactor: number,
  held: number
): number {
  const hits: number[] = [];

  PRESETS.forEach(([, vals, presetCurve, presetOs], i) => {
    if (presetCurve !== curve || presetOs !== osFactor) return;

    const same = Object.entries(vals).every(([key, v]) => {
      const spec = PARAMS[key];
      // Half a step where the parameter is stepped, otherwise a thousandth of
      // the range: wide enough for the float32 round trip through the host,
      // tight enough that a real knob move counts as a change.
      const eps = spec.step > 0 ? spec.step / 2 : (spec.max - spec.min) * 1e-3;
      return Math.abs(values[key] - v) < eps;
    });

    if (same) hits.push(i);
  });

  if (hits.length === 0) return -1;
  // INIT and FL SOFT CLIP carry identical values by design, so a plain first
  // match would rename the user's pick. Keep whatever is already on screen.
  return hits.includes(held) ? held : hits[0];
}

export default function App() {
  const params = useMemo(
    () =>
      Object.fromEntries(
        Object.entries(PARAMS).map(([key, spec]) => [
          key,
          makeParam(spec.id, spec.min, spec.max, spec.def),
        ])
      ),
    []
  );
  // curve and oversampling are AudioParameterChoice (3 steps, index 0..2)
  const curveParam = useMemo(() => makeParam("curve", 0, 2, 0), []);
  const osParam = useMemo(() => makeParam("oversampling", 0, 2, 0), []);
  const deltaToggle = useMemo(() => makeToggle("delta"), []);
  const bypassToggle = useMemo(() => makeToggle("bypass"), []);

  const bypassed = useToggle(bypassToggle);
  const delta = useToggle(deltaToggle);
  const curveIdx = Math.round(useScaled(curveParam));
  const osIdx = Math.round(useScaled(osParam));
  const driveDb = useScaled(params.drive);
  const thresholdSteps = useScaled(params.threshold);
  const outputDb = useScaled(params.output);
  const monoLowPos = useScaled(params.monolow);
  const mixPct = useScaled(params.mix);

  // Only a tiebreaker for value-identical presets and the step base while the
  // values sit on no preset at all. The label never comes from here.
  const [heldIdx, setHeldIdx] = useState(0);
  const applyPreset = (i: number) => {
    const idx = (i + PRESETS.length) % PRESETS.length;
    setHeldIdx(idx);
    const [, vals, curve, osFactor] = PRESETS[idx];
    for (const [key, v] of Object.entries(vals)) params[key].setScaled(v);
    curveParam.setScaled(CURVE_CHIPS.findIndex((c) => c.key === curve));
    osParam.setScaled(OS_FACTORS.indexOf(osFactor as (typeof OS_FACTORS)[number]));
  };

  useEffect(() => {
    // native right-click menu (reload etc.) makes no sense in a plugin window
    const prevent = (e: Event) => e.preventDefault();
    window.addEventListener("contextmenu", prevent);
    return () => window.removeEventListener("contextmenu", prevent);
  }, []);

  const osFactor = OS_FACTORS[osIdx] ?? 1;
  const setOsFactor = (factor: number) => osParam.setScaled(OS_FACTORS.indexOf(factor as (typeof OS_FACTORS)[number]));
  const cycleOs = () => setOsFactor(osFactor === 1 ? 4 : osFactor === 4 ? 8 : 1);
  const curveKey: CurveKey = CURVE_CHIPS[curveIdx]?.key ?? "fl";

  const matchedIdx = matchPreset(
    { drive: driveDb, threshold: thresholdSteps, output: outputDb, monolow: monoLowPos, mix: mixPct },
    curveKey,
    osFactor,
    heldIdx
  );
  const presetName = matchedIdx >= 0 ? PRESETS[matchedIdx][0] : "CUSTOM";
  // Stepping starts where the label stands; on CUSTOM it continues from the
  // preset that was loaded last, so the arrows never jump back to INIT.
  const stepBase = matchedIdx >= 0 ? matchedIdx : heldIdx;

  return (
    <div id="plugin" className={bypassed ? "bypassed" : ""}>
      <header>
        <div className="wordmark">TAGOCLIP</div>
        <div className="preset">
          <button id="prev" title="Previous preset" onClick={() => applyPreset(stepBase - 1)}>
            ‹
          </button>
          <div className="name" id="preset-name">
            {presetName}
          </div>
          <button id="next" title="Next preset" onClick={() => applyPreset(stepBase + 1)}>
            ›
          </button>
        </div>
        <div className="header-right">
          <span className="header-meta">Stereo&nbsp;·&nbsp;V1.1</span>
          <div className="header-mix">
            <Knob spec={PARAMS.mix} param={params.mix} />
          </div>
          <button id="power" title="Bypass" onClick={() => bypassToggle.set(!bypassed)}>
            <svg viewBox="0 0 24 24">
              <path d="M12 3v8" />
              <path d="M6.2 6.2a8 8 0 1 0 11.6 0" />
            </svg>
          </button>
        </div>
      </header>

      <main>
        <div className="scope-unit">
          <div id="scope-wrap">
            <button
              id="delta-btn"
              className={delta ? "on" : ""}
              title="Nur das Abgeschnittene hören"
              onClick={() => deltaToggle.set(!delta)}
            >
              Δ DELTA
            </button>
            <GrReadout bypassed={bypassed} driveDb={driveDb} thresholdSteps={thresholdSteps} />
            <Scope curve={curveKey} thresholdSteps={thresholdSteps} driveDb={driveDb} delta={delta} />
          </div>
          <div className="chip-rows">
            <div className="chip-row" id="curve-row">
              <span className="chip-caption">Curve</span>
              {CURVE_CHIPS.map((chip, i) => (
                <button
                  key={chip.key}
                  className={"chip" + (i === curveIdx ? " on" : "")}
                  onClick={() => curveParam.setScaled(i)}
                >
                  {chip.label}
                </button>
              ))}
            </div>
          </div>
        </div>

        <div className="knob-col">
          <Knob spec={PARAMS.threshold} param={params.threshold} />
          <Knob spec={PARAMS.drive} param={params.drive} />
        </div>

        <div className="stack">
          <Knob spec={PARAMS.output} param={params.output} />
          <Knob spec={PARAMS.monolow} param={params.monolow} />
          <button
            id="os-toggle"
            className={osFactor > 1 ? (osFactor === 8 ? "on on8" : "on") : ""}
            title="Oversampling gegen Aliasing: Off / 4x / 8x"
            onClick={cycleOs}
          >
            {osFactor > 1 ? `Oversampling ${osFactor}x` : "Oversampling OFF"}
          </button>
        </div>

        <Meters bypassed={bypassed} driveDb={driveDb} thresholdSteps={thresholdSteps} outputDb={outputDb} />
      </main>

      <footer>
        <span>TagoBeats</span>
      </footer>
    </div>
  );
}
