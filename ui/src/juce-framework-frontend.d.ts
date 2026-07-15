// The official JUCE frontend library ships without TypeScript types.
// Minimal surface we actually use; extend as needed.
declare module "juce-framework-frontend" {
  interface ListenerList {
    addListener(fn: () => void): number;
    removeListener(id: number): void;
  }

  interface SliderState {
    getNormalisedValue(): number;
    setNormalisedValue(value: number): void;
    getScaledValue(): number;
    sliderDragStarted(): void;
    sliderDragEnded(): void;
    valueChangedEvent: ListenerList;
    propertiesChangedEvent: ListenerList;
  }

  interface ToggleState {
    getValue(): boolean;
    setValue(value: boolean): void;
    valueChangedEvent: ListenerList;
  }

  export function getSliderState(name: string): SliderState;
  export function getToggleState(name: string): ToggleState;
}
