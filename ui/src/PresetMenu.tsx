// Click-to-open preset list for the header label. Self-contained: it only needs
// the preset names, which index the label currently stands on (-1 = CUSTOM) and
// a callback. Styles live in PresetMenu.css and use the shared design tokens
// (--bg-deep, --surface, --text, --text-hi, --accent, --hairline, --dim) plus
// the Martian Mono font, so the file can be copied to another plugin UI as is.
//
// The list is portalled into the scaled canvas (the ancestor marked with
// data-pm-bounds) so it stacks above the rest of the UI, and it is capped to the
// space left below the label, so it scrolls instead of being clipped by the
// fixed-aspect canvas at any scale.

import { useEffect, useLayoutEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";
import "./PresetMenu.css";

interface Props {
  names: readonly string[];
  /** Index of the preset the label currently shows, -1 when nothing matches. */
  activeIdx: number;
  /** Text on the label: the preset name or CUSTOM. */
  label: string;
  onSelect: (index: number) => void;
}

export default function PresetMenu({ names, activeIdx, label, onSelect }: Props) {
  const [open, setOpen] = useState(false);
  const [hl, setHl] = useState(0); // keyboard highlight
  const [pos, setPos] = useState<{ left: number; top: number; maxHeight: number } | null>(null);
  const [host, setHost] = useState<HTMLElement | null>(null);
  const rootRef = useRef<HTMLDivElement>(null);
  const listRef = useRef<HTMLUListElement>(null);

  const openMenu = () => {
    setHl(activeIdx >= 0 ? activeIdx : 0);
    setOpen(true);
  };
  const close = () => {
    setOpen(false);
    setPos(null);
  };
  const choose = (i: number) => {
    onSelect(i);
    close();
  };

  // Place the list under the label in canvas pixels and cap it to the room left
  // inside the canvas. The canvas is CSS scaled, so screen pixels are converted
  // back with the scale factor.
  useLayoutEffect(() => {
    if (!open || !rootRef.current) return;
    const bounds =
      rootRef.current.closest<HTMLElement>("[data-pm-bounds]") ?? document.body;
    const b = bounds.getBoundingClientRect();
    const scale = bounds.offsetWidth > 0 ? b.width / bounds.offsetWidth || 1 : 1;
    const r = rootRef.current.getBoundingClientRect();
    const top = (r.bottom - b.top) / scale + 9;
    setHost(bounds);
    setPos({
      left: (r.left + r.width / 2 - b.left) / scale,
      top,
      maxHeight: Math.max(60, bounds.offsetHeight - top - 10),
    });
  }, [open]);

  // Outside click and keyboard while open.
  useEffect(() => {
    if (!open) return;
    const onPointerDown = (e: PointerEvent) => {
      const t = e.target as Node;
      if (!rootRef.current?.contains(t) && !listRef.current?.contains(t)) close();
    };
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        e.preventDefault();
        close();
      } else if (e.key === "ArrowDown") {
        e.preventDefault();
        setHl((h) => (h + 1) % names.length);
      } else if (e.key === "ArrowUp") {
        e.preventDefault();
        setHl((h) => (h - 1 + names.length) % names.length);
      } else if (e.key === "Home") {
        e.preventDefault();
        setHl(0);
      } else if (e.key === "End") {
        e.preventDefault();
        setHl(names.length - 1);
      } else if (e.key === "Enter") {
        e.preventDefault();
        choose(hl);
      }
    };
    document.addEventListener("pointerdown", onPointerDown, true);
    window.addEventListener("keydown", onKey);
    return () => {
      document.removeEventListener("pointerdown", onPointerDown, true);
      window.removeEventListener("keydown", onKey);
    };
    // choose closes over onSelect and hl, so re-bind when they change
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [open, hl, names.length, onSelect]);

  // Keep the highlighted row in view when the list scrolls.
  useEffect(() => {
    if (!open) return;
    listRef.current?.children[hl]?.scrollIntoView({ block: "nearest" });
  }, [open, hl]);

  return (
    <div className="pm" ref={rootRef}>
      <button
        type="button"
        className={"pm-label" + (open ? " open" : "")}
        title="Choose preset"
        aria-haspopup="listbox"
        aria-expanded={open}
        onClick={() => (open ? close() : openMenu())}
      >
        {label}
      </button>
      {open &&
        host &&
        pos &&
        createPortal(
          <ul
            className="pm-list"
            role="listbox"
            ref={listRef}
            style={{ left: pos.left, top: pos.top, maxHeight: pos.maxHeight }}
          >
            {names.map((name, i) => (
              <li
                key={name + i}
                role="option"
                aria-selected={i === activeIdx}
                className={(i === activeIdx ? "active" : "") + (i === hl ? " hl" : "")}
                onPointerEnter={() => setHl(i)}
                onClick={() => choose(i)}
              >
                {name}
              </li>
            ))}
          </ul>,
          host
        )}
    </div>
  );
}
