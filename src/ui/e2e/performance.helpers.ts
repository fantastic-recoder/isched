import { performance } from 'node:perf_hooks';
import type { Page, TestInfo } from '@playwright/test';

export interface UiBenchmarkOperation {
  readonly operationClass: string;
  readonly marker: string;
}

export interface UiBenchmarkSample {
  readonly user: number;
  readonly operation: number;
  readonly operationClass: string;
  readonly durationMs: number | null;
  readonly error?: string;
}

export interface UiBenchmarkReport {
  readonly userCount: number;
  readonly warmupsPerUser: number;
  readonly measuredOperationsPerUser: number;
  readonly manifest: readonly UiBenchmarkOperation[];
  readonly warmupFailures: readonly { user: number; operation: number; error: string }[];
  readonly samples: readonly UiBenchmarkSample[];
  readonly failures: readonly UiBenchmarkSample[];
  readonly aggregateP95Ms: number | null;
  readonly classes: Readonly<
    Record<string, { count: number; failures: number; p95Ms: number | null }>
  >;
}

type PrepareOperation = (
  page: Page,
  operation: UiBenchmarkOperation,
  user: number,
  operationIndex: number,
) => Promise<() => Promise<void>>;

function barrier(participants: number): () => Promise<void> {
  let arrived = 0;
  let release!: () => void;
  const gate = new Promise<void>((resolve) => {
    release = resolve;
  });

  return async () => {
    arrived += 1;
    if (arrived === participants) release();
    await gate;
  };
}

function errorText(error: unknown): string {
  return error instanceof Error ? `${error.name}: ${error.message}` : String(error);
}

function nearestRankP95(values: readonly number[]): number | null {
  if (values.length === 0) return null;
  const ordered = [...values].sort((a, b) => a - b);
  return ordered[Math.ceil(0.95 * ordered.length) - 1];
}

/**
 * Runs each journey's frozen manifest through ten independently authenticated browser contexts.
 * The warmup and measured phases each have a synchronized barrier; failed entries are retained
 * instead of being silently omitted from the measured denominator.
 */
export async function runConcurrentUiBenchmark(
  pages: readonly Page[],
  manifest: readonly UiBenchmarkOperation[],
  prepareOperation: PrepareOperation,
  testInfo: TestInfo,
): Promise<UiBenchmarkReport> {
  const userCount = 10;
  const warmupsPerUser = 10;
  if (pages.length !== userCount) {
    throw new Error(`The benchmark requires exactly ${userCount} browser users; received ${pages.length}.`);
  }
  if (manifest.length !== 100) {
    throw new Error(`The benchmark manifest must contain exactly 100 operations; received ${manifest.length}.`);
  }

  const warmupFailures: Array<{ user: number; operation: number; error: string }> = [];
  const samples: UiBenchmarkSample[] = [];
  const warmupStart = barrier(userCount);
  const measuredStart = barrier(userCount);

  await Promise.all(
    pages.map(async (page, user) => {
      await warmupStart();
      for (let operationIndex = 0; operationIndex < warmupsPerUser; operationIndex += 1) {
        try {
          const dispatch = await prepareOperation(page, manifest[operationIndex], user, operationIndex);
          await dispatch();
        } catch (error) {
          warmupFailures.push({ user, operation: operationIndex, error: errorText(error) });
        }
      }

      await measuredStart();
      for (let operationIndex = 0; operationIndex < manifest.length; operationIndex += 1) {
        const operation = manifest[operationIndex];
        let startedAt: number | null = null;
        try {
          const dispatch = await prepareOperation(page, operation, user, operationIndex);
          startedAt = performance.now();
          await dispatch();
          samples.push({
            user,
            operation: operationIndex,
            operationClass: operation.operationClass,
            durationMs: performance.now() - startedAt,
          });
        } catch (error) {
          samples.push({
            user,
            operation: operationIndex,
            operationClass: operation.operationClass,
            durationMs: startedAt === null ? null : performance.now() - startedAt,
            error: errorText(error),
          });
        }
      }
    }),
  );

  samples.sort((left, right) => left.user - right.user || left.operation - right.operation);
  const failures = samples.filter((sample) => sample.error !== undefined);
  const allSuccessfulDurations = samples.every((sample) => sample.durationMs !== null && !sample.error)
    ? samples.map((sample) => sample.durationMs as number)
    : [];
  const classNames = [...new Set(manifest.map((operation) => operation.operationClass))].sort();
  const classes = Object.fromEntries(
    classNames.map((operationClass) => {
      const classSamples = samples.filter((sample) => sample.operationClass === operationClass);
      const classFailed = classSamples.some((sample) => sample.durationMs === null || sample.error);
      return [
        operationClass,
        {
          count: classSamples.length,
          failures: classSamples.filter((sample) => sample.durationMs === null || sample.error).length,
          p95Ms: classFailed
            ? null
            : nearestRankP95(classSamples.map((sample) => sample.durationMs as number)),
        },
      ];
    }),
  );

  const report: UiBenchmarkReport = {
    userCount,
    warmupsPerUser,
    measuredOperationsPerUser: manifest.length,
    manifest,
    warmupFailures,
    samples,
    failures,
    aggregateP95Ms: allSuccessfulDurations.length === samples.length
      ? nearestRankP95(allSuccessfulDurations)
      : null,
    classes,
  };

  await testInfo.attach('ui-performance-samples.json', {
    body: JSON.stringify(report, null, 2),
    contentType: 'application/json',
  });
  console.log(
    `[ui-performance] ${JSON.stringify({
      users: report.userCount,
      warmupsPerUser: report.warmupsPerUser,
      measuredSamples: report.samples.length,
      warmupFailures: report.warmupFailures.length,
      failures: report.failures.length,
      classCountsAndP95: report.classes,
      aggregateP95Ms: report.aggregateP95Ms,
    })}`,
  );
  return report;
}
