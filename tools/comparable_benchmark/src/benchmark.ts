import autocannon from "autocannon";
import { createClient } from "graphql-ws";
import { WebSocket } from "ws";
import { performance } from "perf_hooks";
import type { BenchmarkResult, HardwareContext } from "./types.js";

const GQL_BODY = (query: string): string =>
  JSON.stringify({ query });

const GQL_HEADERS = {
  "Content-Type": "application/json",
};

/** Run autocannon throughput benchmark: 1 connection, 5 seconds. */
export async function runThroughput(
  url: string,
  query: string,
  hardwareContext: HardwareContext,
  server: "apollo" | "isched",
  scenario: string
): Promise<BenchmarkResult> {
  const body = GQL_BODY(query);
  const result = await new Promise<autocannon.Result>((resolve, reject) => {
    const instance = autocannon(
      {
        url,
        connections: 1,
        duration: 5,
        method: "POST",
        headers: GQL_HEADERS,
        body,
      },
      (err, result) => {
        if (err) reject(err);
        else resolve(result);
      }
    );
    autocannon.track(instance, { renderProgressBar: false });
  });

  return {
    scenario,
    server,
    requests: result.requests.total,
    errors: result.errors,
    durationMs: result.duration * 1000,
    reqPerSecond: result.requests.average,
    p95Ms: null,
    fanOutMs: null,
    hardwareContext,
    timestamp: new Date().toISOString(),
  };
}

/** Run autocannon concurrent benchmark: 100 connections, 1000 total requests. */
export async function runConcurrent(
  url: string,
  query: string,
  hardwareContext: HardwareContext,
  server: "apollo" | "isched",
  scenario: string
): Promise<BenchmarkResult> {
  const body = GQL_BODY(query);
  const start = Date.now();
  const result = await new Promise<autocannon.Result>((resolve, reject) => {
    const instance = autocannon(
      {
        url,
        connections: 100,
        amount: 1000,
        method: "POST",
        headers: GQL_HEADERS,
        body,
      },
      (err, result) => {
        if (err) reject(err);
        else resolve(result);
      }
    );
    autocannon.track(instance, { renderProgressBar: false });
  });
  const durationMs = Date.now() - start;

  return {
    scenario,
    server,
    requests: result.requests.total,
    errors: result.errors,
    durationMs,
    reqPerSecond: durationMs > 0 ? (result.requests.total / durationMs) * 1000 : null,
    p95Ms: null,
    fanOutMs: null,
    hardwareContext,
    timestamp: new Date().toISOString(),
  };
}

/** Run sequential fetch loop, return p95 latency in ms. */
export async function runP95Sequential(
  url: string,
  query: string,
  hardwareContext: HardwareContext,
  server: "apollo" | "isched",
  scenario: string,
  iterations = 1000
): Promise<BenchmarkResult> {
  const body = GQL_BODY(query);
  const timings: number[] = [];

  for (let i = 0; i < iterations; i++) {
    const t0 = performance.now();
    await fetch(url, {
      method: "POST",
      headers: GQL_HEADERS,
      body,
    });
    timings.push(performance.now() - t0);
  }

  timings.sort((a, b) => a - b);
  const p95 = timings[Math.floor(0.95 * iterations)];

  return {
    scenario,
    server,
    requests: iterations,
    errors: 0,
    durationMs: timings.reduce((a, b) => a + b, 0),
    reqPerSecond: null,
    p95Ms: p95,
    fanOutMs: null,
    hardwareContext,
    timestamp: new Date().toISOString(),
  };
}

/** Connect clientCount WS subscribers, return elapsed until last first-event received. */
export async function runWsFanOut(
  wsUrl: string,
  hardwareContext: HardwareContext,
  server: "apollo" | "isched",
  scenario: string,
  clientCount = 50
): Promise<BenchmarkResult> {
  const start = performance.now();

  const promises = Array.from({ length: clientCount }, () =>
    new Promise<void>((resolve, reject) => {
      const client = createClient({
        url: wsUrl,
        webSocketImpl: WebSocket,
        retryAttempts: 0,
      });

      const unsub = client.subscribe(
        { query: "subscription { healthChanged { status timestamp } }" },
        {
          next: () => {
            unsub();
            client.dispose().then(resolve, reject);
          },
          error: (err) => {
            client.dispose().catch(() => undefined);
            reject(err instanceof Error ? err : new Error(String(err)));
          },
          complete: () => {
            client.dispose().catch(() => undefined);
            resolve();
          },
        }
      );
    })
  );

  await Promise.all(promises);
  const fanOutMs = performance.now() - start;

  return {
    scenario,
    server,
    requests: clientCount,
    errors: 0,
    durationMs: fanOutMs,
    reqPerSecond: null,
    p95Ms: null,
    fanOutMs,
    hardwareContext,
    timestamp: new Date().toISOString(),
  };
}

/** Fire count un-timed warm-up POST requests sequentially. */
export async function warmUp(url: string, query: string, count = 100): Promise<void> {
  const body = GQL_BODY(query);
  for (let i = 0; i < count; i++) {
    await fetch(url, { method: "POST", headers: GQL_HEADERS, body }).catch(() => undefined);
  }
}
