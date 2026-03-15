import { spawn, ChildProcess } from "child_process";
import { existsSync } from "fs";
import { writeFile, mkdir } from "fs/promises";
import { cpus, type } from "os";
import { resolve, dirname } from "path";
import { fileURLToPath } from "url";
import {
  runThroughput,
  runConcurrent,
  runP95Sequential,
  runWsFanOut,
  warmUp,
} from "./benchmark.js";
import { generateReport } from "./report.js";
import type { BenchmarkResult, HardwareContext } from "./types.js";

const __dirname = dirname(fileURLToPath(import.meta.url));
const REPO_ROOT = resolve(__dirname, "../../..");
const RESULTS_DIR = resolve(__dirname, "../results");

const APOLLO_HTTP = "http://127.0.0.1:18100/graphql";
const APOLLO_WS = "ws://127.0.0.1:18100/graphql";
const ISCHED_HTTP = "http://127.0.0.1:18092/graphql";
const ISCHED_WS = "ws://127.0.0.1:18093/graphql";

const REGRESSION_THRESHOLD_PCT = Number(process.env["REGRESSION_THRESHOLD_PCT"] ?? "10");

// ---------------------------------------------------------------------------
// Process management
// ---------------------------------------------------------------------------

interface SpawnOpts {
  binary: string;
  args: string[];
  env: NodeJS.ProcessEnv;
  label: string;
}

function spawnServer(opts: SpawnOpts): ChildProcess {
  const proc = spawn(opts.binary, opts.args, {
    env: { ...process.env, ...opts.env },
    stdio: ["ignore", "pipe", "pipe"],
  });
  proc.stdout?.on("data", (d: Buffer) => process.stdout.write(`[${opts.label}] ${d}`));
  proc.stderr?.on("data", (d: Buffer) => process.stderr.write(`[${opts.label}] ${d}`));
  proc.on("error", (err) => {
    console.error(`[${opts.label}] spawn error:`, err.message);
  });
  return proc;
}

async function killServer(proc: ChildProcess, label: string): Promise<void> {
  if (proc.exitCode !== null) return;
  proc.kill("SIGTERM");
  for (let i = 0; i < 20; i++) {
    await sleep(100);
    if (proc.exitCode !== null) return;
  }
  proc.kill("SIGKILL");
  await sleep(200);
}

/** Send the `shutdown` GraphQL mutation to isched and wait for the process to exit. */
async function gracefulShutdownIsched(
  proc: ChildProcess,
  shutdownToken: string
): Promise<boolean> {
  if (proc.exitCode !== null) return true;
  try {
    const body = JSON.stringify({ query: "mutation { shutdown }" });
    const res = await fetch(ISCHED_HTTP, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Authorization: `Bearer ${shutdownToken}`,
      },
      body,
      signal: AbortSignal.timeout(3_000),
    });
    if (res.ok) {
      // Wait up to 3 s for the process to exit cleanly.
      for (let i = 0; i < 30; i++) {
        await sleep(100);
        if (proc.exitCode !== null) return true;
      }
    }
  } catch {
    // fetch failed — server may already be gone
  }
  return proc.exitCode !== null;
}

async function waitReady(url: string, label: string, timeoutMs = 10_000): Promise<void> {
  const body = JSON.stringify({ query: "{ health { status } }" });
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    try {
      const res = await fetch(url, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body,
      });
      if (res.ok) {
        console.log(`[${label}] ready at ${url}`);
        return;
      }
    } catch {
      // not ready yet
    }
    await sleep(100);
  }
  throw new Error(`[${label}] did not become ready at ${url} within ${timeoutMs} ms`);
}

function sleep(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}

// ---------------------------------------------------------------------------
// Dry-run: validate environment
// ---------------------------------------------------------------------------

async function dryRun(ischedBinary: string): Promise<void> {
  let ok = true;

  if (!existsSync(ischedBinary)) {
    console.error(
      `ERROR: isched binary not found: ${ischedBinary}\n` +
        `Build it first: python3 configure.py`
    );
    ok = false;
  }

  for (const [label, port] of [
    ["Apollo HTTP+WS", 18100],
    ["isched HTTP", 18092],
    ["isched WS", 18093],
  ] as const) {
    const available = await isPortFree(port);
    if (!available) {
      console.error(`ERROR: Port ${port} (${label}) is already in use`);
      ok = false;
    }
  }

  if (!ok) process.exit(1);
  console.log("dry-run: all checks passed");
  process.exit(0);
}

async function isPortFree(port: number): Promise<boolean> {
  const { createConnection } = await import("net");
  return new Promise((resolve) => {
    const sock = createConnection(port, "127.0.0.1");
    sock.on("connect", () => {
      sock.destroy();
      resolve(false); // port is in use
    });
    sock.on("error", () => resolve(true)); // connection refused → free
  });
}

// ---------------------------------------------------------------------------
// Hardware context
// ---------------------------------------------------------------------------

function getHardwareContext(ischedBinary: string): HardwareContext {
  const lowerPath = ischedBinary.toLowerCase();
  const buildType = lowerPath.includes("release") ? "Release" : "Debug";
  return {
    os: type(),
    cpuCount: cpus().length,
    nodeVersion: process.version,
    buildType,
  };
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

async function main(): Promise<void> {
  const isDryRun = process.argv.includes("--dry-run");

  const ischedBinary = resolve(
    REPO_ROOT,
    process.env["ISCHED_BINARY"] ?? "cmake-build-release/src/main/cpp/isched/isched_srv"
  );

  if (isDryRun) {
    await dryRun(ischedBinary);
    return;
  }

  // T013 — binary existence check
  if (!existsSync(ischedBinary)) {
    console.error(
      `ERROR: isched binary not found: ${ischedBinary}\n` +
        `Build it first:\n  python3 configure.py\n` +
        `Or set ISCHED_BINARY=/path/to/binary`
    );
    process.exit(1);
  }

  // T014 — port-in-use check
  for (const [label, port] of [
    ["Apollo HTTP+WS", 18100],
    ["isched HTTP", 18092],
    ["isched WS", 18093],
  ] as const) {
    if (!(await isPortFree(port))) {
      console.error(
        `ERROR: Port ${port} (${label}) is already in use.\n` +
          `Stop the process occupying it and retry.`
      );
      process.exit(1);
    }
  }

  await mkdir(RESULTS_DIR, { recursive: true });

  const hw = getHardwareContext(ischedBinary);
  const apolloSrcPath = resolve(__dirname, "apollo-server.js");
  const apolloTsPath = resolve(__dirname, "apollo-server.ts");

  // Start Apollo server
  const apolloProc = spawnServer({
    binary: process.execPath,
    args: ["--import", "tsx/esm", apolloTsPath],
    env: {},
    label: "apollo",
  });

  // Start isched server
  const ISCHED_SHUTDOWN_TOKEN = "isched-benchmark-shutdown-" + Math.random().toString(36).slice(2);
  const ischedEnv: NodeJS.ProcessEnv = {
    ISCHED_SERVER_PORT: "18092",
    ISCHED_SERVER_HOST: "127.0.0.1",
    SPDLOG_LEVEL: process.env["SPDLOG_LEVEL"] ?? "warn",
    ISCHED_SHUTDOWN_TOKEN,
  };
  if (process.env["ISCHED_MIN_THREADS"]) ischedEnv["ISCHED_MIN_THREADS"] = process.env["ISCHED_MIN_THREADS"];
  if (process.env["ISCHED_MAX_THREADS"]) ischedEnv["ISCHED_MAX_THREADS"] = process.env["ISCHED_MAX_THREADS"];

  const ischedProc = spawnServer({
    binary: ischedBinary,
    args: [],
    env: ischedEnv,
    label: "isched",
  });

  void apolloSrcPath; // not used directly

  const results: BenchmarkResult[] = [];

  try {
    await Promise.all([
      waitReady(APOLLO_HTTP, "apollo"),
      waitReady(ISCHED_HTTP, "isched"),
    ]);

    // Define scenarios
    const helloQuery = "{ hello }";
    const versionQuery = "{ version }";

    // --- Scenario 1: hello throughput ---
    for (const [label, url, ws, server] of [
      ["apollo", APOLLO_HTTP, APOLLO_WS, "apollo"],
      ["isched", ISCHED_HTTP, ISCHED_WS, "isched"],
    ] as const) {
      await warmUp(url, helloQuery);
      console.log(`[${label}] measuring hello throughput...`);
      results.push(await runThroughput(url, helloQuery, hw, server, "hello-throughput"));
    }

    // --- Scenario 2: concurrent version ---
    for (const [label, url, server] of [
      ["apollo", APOLLO_HTTP, "apollo"],
      ["isched", ISCHED_HTTP, "isched"],
    ] as const) {
      await warmUp(url, versionQuery);
      console.log(`[${label}] measuring concurrent version...`);
      results.push(await runConcurrent(url, versionQuery, hw, server, "concurrent-version"));
    }

    // --- Scenario 3: p95 latency (version) ---
    for (const [label, url, server] of [
      ["apollo", APOLLO_HTTP, "apollo"],
      ["isched", ISCHED_HTTP, "isched"],
    ] as const) {
      await warmUp(url, versionQuery);
      console.log(`[${label}] measuring p95 latency...`);
      results.push(await runP95Sequential(url, versionQuery, hw, server, "p95-latency-version"));
    }

    // --- Scenario 4: WS healthChanged fan-out ---
    for (const [label, wsUrl, server] of [
      ["apollo", APOLLO_WS, "apollo"],
      ["isched", ISCHED_WS, "isched"],
    ] as const) {
      console.log(`[${label}] measuring WS fan-out...`);
      results.push(await runWsFanOut(wsUrl, hw, server, "ws-healthchanged-fanout"));
    }
  } finally {
    // Shut down isched gracefully via the shutdown mutation, then fall back to SIGTERM.
    const cleanExit = await gracefulShutdownIsched(ischedProc, ISCHED_SHUTDOWN_TOKEN);
    if (!cleanExit) {
      console.log("[isched] graceful shutdown timed out — sending SIGTERM");
    }
    await Promise.all([
      killServer(apolloProc, "apollo"),
      cleanExit ? Promise.resolve() : killServer(ischedProc, "isched"),
    ]);
  }

  // Write JSON result files
  for (const r of results) {
    const fname = `${r.server}-${r.scenario}.json`;
    await writeFile(
      resolve(RESULTS_DIR, fname),
      JSON.stringify(r, null, 2)
    );
    console.log(`  wrote ${fname}`);
  }

  // Generate Markdown report
  await generateReport(results);

  // Regression check (T018)
  runRegressionCheck(results);
}

function runRegressionCheck(results: BenchmarkResult[]): void {
  const httpScenarios = ["hello-throughput", "concurrent-version"];
  let failed = false;

  for (const scenario of httpScenarios) {
    const apollo = results.find((r) => r.server === "apollo" && r.scenario === scenario);
    const isched = results.find((r) => r.server === "isched" && r.scenario === scenario);

    if (!apollo || !isched || apollo.reqPerSecond == null || isched.reqPerSecond == null) {
      continue;
    }

    const ratio = (isched.reqPerSecond - apollo.reqPerSecond) / apollo.reqPerSecond;
    const thresholdFrac = -REGRESSION_THRESHOLD_PCT / 100;

    if (ratio < thresholdFrac) {
      console.error(
        `REGRESSION: isched ${scenario} is ${(-ratio * 100).toFixed(1)}% slower than Apollo ` +
          `(threshold: ${REGRESSION_THRESHOLD_PCT}%)`
      );
      failed = true;
    }
  }

  if (failed) process.exit(1);
  console.log("Regression check passed.");
}

main().catch((err) => {
  console.error("Fatal error:", err instanceof Error ? err.message : err);
  process.exit(1);
});
