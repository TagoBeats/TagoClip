// Parameter contract, mirrors tagoclip::param in PluginProcessor.h.
// IDs must never change; ranges match the APVTS layout.

export interface ParamSpec {
  id: string;
  label: string;
  min: number;
  max: number;
  def: number;
  step: number; // 0 = continuous
  bipolar: boolean;
  size: number; // knob diameter in px, from the mockup
  fmt: (v: number) => string;
}

// U+2212 minus, exactly like the mockup formatting
const sign = (v: number) => (v > 0 ? "+" : v < 0 ? "−" : "");
const fmtDb = (v: number) => sign(v) + Math.abs(v).toFixed(1) + " dB";

export const PARAMS: Record<string, ParamSpec> = {
  drive: {
    id: "drive_db",
    label: "drive",
    min: -6,
    max: 24,
    def: 0,
    step: 0,
    bipolar: true,
    size: 90,
    fmt: fmtDb,
  },
  // Fruity scale: integer 0..127, internally t = v/128 like the original DSP.
  threshold: {
    id: "threshold_steps",
    label: "threshold",
    min: 1,
    max: 127,
    def: 100,
    step: 1,
    bipolar: false,
    size: 90,
    fmt: (v) => Math.round(v).toString(),
  },
  output: {
    id: "output_db",
    label: "output",
    min: -12,
    max: 12,
    def: 0,
    step: 0,
    bipolar: true,
    size: 62,
    fmt: fmtDb,
  },
  // Logarithmic sweep: knob value is the normalized position, 20 Hz..400 Hz,
  // bottom 3% = OFF (mirrors ClipEngine.h monoLowOffBelow / monoLowFreqHz).
  monolow: {
    id: "mono_low",
    label: "mono low",
    min: 0,
    max: 1,
    def: 0,
    step: 0,
    bipolar: false,
    size: 62,
    fmt: (v) => (v < 0.03 ? "OFF" : Math.round(20 * Math.pow(20, v)) + " Hz"),
  },
};

// Curve chips: index matches the AudioParameterChoice order (Default/Hard/Tanh).
export const CURVES = ["fl", "hard", "tanh"] as const;
export type CurveKey = (typeof CURVES)[number];
export const CURVE_CHIPS: Array<{ key: CurveKey; label: string }> = [
  { key: "fl", label: "DEFAULT" },
  { key: "hard", label: "HARD" },
  { key: "tanh", label: "TANH" },
];

// 1:1 ports of tagodsp/distortion/clipper.py, same shapes as the mockup scope.
export const CURVE_FN: Record<CurveKey, (x: number, t: number) => number> = {
  fl: (x, t) => {
    const ax = Math.abs(x);
    if (ax <= t) return x;
    return Math.sign(x) * (1 - (1 - t) * Math.exp(-(ax - t) / (1 - t)));
  },
  // Display note: hard clip in the DSP ceilings at t itself; for the scope we
  // show the un-normalized shape so the knee position is honest.
  hard: (x, t) => Math.max(-t, Math.min(t, x)),
  tanh: (x, t) => {
    const ax = Math.abs(x);
    if (ax <= t) return x;
    return Math.sign(x) * (t + (1 - t) * Math.tanh((ax - t) / (1 - t)));
  },
};

// Oversampling: cycle button off -> 4x -> 8x -> off (Robin, 15.07.2026).
// Index matches osFactorTable in ClipEngine.h (0 off, 1 4x, 2 8x).
export const OS_FACTORS = [1, 4, 8] as const;

// v1 presets, ported 1:1 from mockup/index.html. INIT = the Fruity Soft
// Clipper defaults with OS off: load TagoClip, it IS Fruity 1:1.
export const PRESETS: Array<[string, Record<string, number>, CurveKey, number]> = [
  ["INIT", { drive: 0, threshold: 100, output: 0, monolow: 0 }, "fl", 1],
  ["808 GLUE", { drive: 6, threshold: 84, output: -1, monolow: 0.67 }, "fl", 8],
  ["DRUMBUS", { drive: 3, threshold: 92, output: 0, monolow: 0 }, "fl", 8],
  ["FL SOFT CLIP", { drive: 0, threshold: 100, output: 0, monolow: 0 }, "fl", 1],
  ["TAPE SOFT", { drive: 8, threshold: 51, output: -2, monolow: 0.6 }, "tanh", 8],
];
