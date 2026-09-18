// Thin wrapper around the JUCE frontend library with a browser fallback,
// so the UI runs standalone in a plain browser for the screenshot loop.

import * as Juce from "juce-framework-frontend";

type Listener = () => void;

export interface ParamHandle {
  getScaled(): number;
  setScaled(v: number): void;
  dragStarted(): void;
  dragEnded(): void;
  subscribe(fn: Listener): () => void;
}

export interface ToggleHandle {
  get(): boolean;
  set(v: boolean): void;
  subscribe(fn: Listener): () => void;
}

declare global {
  interface Window {
    __JUCE__?: {
      backend: {
        addEventListener(id: string, fn: (payload: unknown) => void): unknown;
        removeEventListener(handle: unknown): void;
      };
      initialisationData?: Record<string, unknown>;
    };
  }
}

// The JUCE frontend lib installs an empty __JUCE__ shim in plain browsers,
// so probe for actually registered slider relays instead of the object.
const initData = window.__JUCE__?.initialisationData as
  | { __juce__sliders?: string[] }
  | undefined;
export const inJuce = (initData?.__juce__sliders ?? []).length > 0;

function juceParam(id: string, min: number, max: number): ParamHandle {
  const state = Juce.getSliderState(id);
  return {
    getScaled: () => state.getScaledValue(),
    // All ranges in the contract are linear, so normalising here is exact
    // (choice params are three evenly-spaced steps, same formula).
    setScaled: (v) => state.setNormalisedValue((v - min) / (max - min)),
    dragStarted: () => state.sliderDragStarted(),
    dragEnded: () => state.sliderDragEnded(),
    // Also listen to propertiesChanged: the host pushes range + initial value
    // shortly after page load, and that push must reach the UI state too.
    subscribe: (fn) => {
      const l = state.valueChangedEvent.addListener(fn);
      const p = state.propertiesChangedEvent.addListener(fn);
      return () => {
        state.valueChangedEvent.removeListener(l);
        state.propertiesChangedEvent.removeListener(p);
      };
    },
  };
}

function localParam(def: number): ParamHandle {
  let value = def;
  const listeners = new Set<Listener>();
  return {
    getScaled: () => value,
    setScaled: (v) => {
      value = v;
      listeners.forEach((fn) => fn());
    },
    dragStarted: () => {},
    dragEnded: () => {},
    subscribe: (fn) => {
      listeners.add(fn);
      return () => listeners.delete(fn);
    },
  };
}

export function makeParam(id: string, min: number, max: number, def: number): ParamHandle {
  return inJuce ? juceParam(id, min, max) : localParam(def);
}

export function makeToggle(id: string): ToggleHandle {
  if (inJuce) {
    const state = Juce.getToggleState(id);
    return {
      get: () => state.getValue(),
      set: (v) => state.setValue(v),
      subscribe: (fn) => {
        const l = state.valueChangedEvent.addListener(fn);
        return () => state.valueChangedEvent.removeListener(l);
      },
    };
  }
  let value = false;
  const listeners = new Set<Listener>();
  return {
    get: () => value,
    set: (v) => {
      value = v;
      listeners.forEach((fn) => fn());
    },
    subscribe: (fn) => {
      listeners.add(fn);
      return () => listeners.delete(fn);
    },
  };
}

// Meter levels pushed from the editor timer ("levels" events, ~30 Hz).
// gr is the peak gain reduction of the last block in dB and is never above 0.
export function onLevels(
  fn: (levels: { in: number; out: number; gr: number }) => void
): () => void {
  if (!inJuce) return () => {};
  const handle = window.__JUCE__!.backend.addEventListener("levels", (payload) => {
    const p = payload as { in?: number; out?: number; gr?: number };
    fn({ in: p.in ?? 0, out: p.out ?? 0, gr: p.gr ?? 0 });
  });
  return () => window.__JUCE__!.backend.removeEventListener(handle);
}
