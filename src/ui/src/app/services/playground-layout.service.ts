// SPDX-License-Identifier: MPL-2.0
/**
 * @file playground-layout.service.ts
 * @brief Persists and restores playground panel sizes across browser sessions (SP-011).
 *        Uses localStorage for layout preference data only (not JWTs or session data).
 */

import { Injectable, signal } from '@angular/core';

export interface PlaygroundLayout {
  leftWidthPct: number;  // clamped to [20, 80]
  topHeightPct: number;  // clamped to [15, 85]
}

export const DEFAULT_LAYOUT: PlaygroundLayout = { leftWidthPct: 30, topHeightPct: 50 };
const STORAGE_KEY = 'isched.playground.layout';

const MIN_LEFT = 20;
const MAX_LEFT = 80;
const MIN_TOP = 15;
const MAX_TOP = 85;

function clamp(value: number, min: number, max: number): number {
  return Math.min(Math.max(value, min), max);
}

function loadFromStorage(): PlaygroundLayout {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return { ...DEFAULT_LAYOUT };

    const parsed = JSON.parse(raw) as Partial<PlaygroundLayout>;
    if (typeof parsed.leftWidthPct !== 'number' || typeof parsed.topHeightPct !== 'number') {
      return { ...DEFAULT_LAYOUT };
    }

    return {
      leftWidthPct: clamp(parsed.leftWidthPct, MIN_LEFT, MAX_LEFT),
      topHeightPct: clamp(parsed.topHeightPct, MIN_TOP, MAX_TOP),
    };
  } catch {
    return { ...DEFAULT_LAYOUT };
  }
}

@Injectable({ providedIn: 'root' })
export class PlaygroundLayoutService {
  readonly layout = signal<PlaygroundLayout>(loadFromStorage());

  setLeftWidth(pct: number): void {
    this.update({ leftWidthPct: clamp(pct, MIN_LEFT, MAX_LEFT) });
  }

  setTopHeight(pct: number): void {
    this.update({ topHeightPct: clamp(pct, MIN_TOP, MAX_TOP) });
  }

  private update(delta: Partial<PlaygroundLayout>): void {
    const next = { ...this.layout(), ...delta };
    this.layout.set(next);
    this.persist(next);
  }

  private persist(layout: PlaygroundLayout): void {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(layout));
    } catch {
      // Ignore storage errors (private browsing, quota exceeded, etc.)
    }
  }
}

