// SPDX-License-Identifier: MPL-2.0
/**
 * @file operation-latency.util.ts
 * @brief Records operation start/end timestamps and computes percentiles (spec 013, SC-001).
 */

export interface OperationLatencySample {
  readonly name: string;
  readonly startedAtMs: number;
  readonly endedAtMs: number;
  readonly durationMs: number;
}

export interface OperationLatencyTimer {
  /** Stops the timer and returns the recorded sample. Idempotent. */
  stop(): OperationLatencySample;
}

export class OperationLatencyRecorder {
  private readonly samples: OperationLatencySample[] = [];

  constructor(private readonly now: () => number = () => performance.now()) {}

  start(name: string): OperationLatencyTimer {
    const startedAtMs = this.now();
    let sample: OperationLatencySample | null = null;
    return {
      stop: () => {
        if (!sample) {
          const endedAtMs = this.now();
          sample = { name, startedAtMs, endedAtMs, durationMs: endedAtMs - startedAtMs };
          this.samples.push(sample);
        }
        return sample;
      },
    };
  }

  all(): readonly OperationLatencySample[] {
    return this.samples;
  }

  /** Nearest-rank percentile (0 < p <= 100) of recorded durations; NaN if empty. */
  percentile(p: number, name?: string): number {
    const durations = this.samples
      .filter((s) => name === undefined || s.name === name)
      .map((s) => s.durationMs)
      .sort((a, b) => a - b);
    if (durations.length === 0) return Number.NaN;
    const rank = Math.max(1, Math.ceil((p / 100) * durations.length));
    return durations[Math.min(rank, durations.length) - 1];
  }
}
