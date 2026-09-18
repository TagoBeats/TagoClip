// Numeric gain reduction, asked for twice in the r/trapproduction thread
// (daRealTENTACLES, rdmprzm). Sits in the scope corner opposite the delta
// button, not in the footer, which was cleared on purpose on 16.07.2026.
//
// The number is whatever the processor reported for the last block (peak
// reduction across the curve alone). Ballistics only smooth what was measured:
// fast attack so a single clipped transient is visible, slow release back to
// zero so the value stays readable at 30 Hz block updates.

import { useEffect, useRef } from "react";
import { inJuce, onLevels } from "./bridge";

const ATTACK = 0.6;
const RELEASE = 0.06;

export default function GrReadout({
  bypassed,
  driveDb,
  thresholdSteps,
}: {
  bypassed: boolean;
  driveDb: number;
  thresholdSteps: number;
}) {
  const valueEl = useRef<HTMLSpanElement>(null);
  const rootEl = useRef<HTMLDivElement>(null);
  const reported = useRef(0);
  const stateRef = useRef({ bypassed, driveDb, thresholdSteps });
  stateRef.current = { bypassed, driveDb, thresholdSteps };

  useEffect(() => {
    const unsub = onLevels((l) => (reported.current = l.gr));

    let raf = 0;
    let shown = 0;
    const tick = () => {
      const { bypassed: byp, driveDb: drive, thresholdSteps: steps } = stateRef.current;
      let target = 0;
      if (!byp) {
        if (inJuce) {
          target = Math.min(0, reported.current);
        } else {
          // Plain browser (mockup and screenshot loop): no audio thread, so the
          // idle model from Meters.tsx stands in, same as the meters do.
          const t = steps / 128;
          const driven = 0.45 * Math.pow(10, drive / 20);
          target = driven > t ? -Math.min(24, 20 * Math.log10(driven / t) * 0.8) : 0;
        }
      }
      shown += (target - shown) * (target < shown ? ATTACK : RELEASE);
      const active = shown < -0.05;
      if (valueEl.current)
        valueEl.current.textContent = (active ? "−" + Math.abs(shown).toFixed(1) : "0.0");
      if (rootEl.current) rootEl.current.classList.toggle("on", active);
      raf = requestAnimationFrame(tick);
    };
    tick();
    return () => {
      cancelAnimationFrame(raf);
      unsub();
    };
  }, []);

  return (
    <div id="gr-readout" ref={rootEl} title="Peak gain reduction of the clipper">
      <span className="gr-label">GR</span>
      <span className="gr-value" ref={valueEl}>
        0.0
      </span>
      <span className="gr-unit">dB</span>
    </div>
  );
}
