import { readdir, readFile, writeFile } from "fs/promises";
import { resolve, dirname } from "path";
import { fileURLToPath } from "url";
import type { BenchmarkResult } from "./types.js";

const __dirname = dirname(fileURLToPath(import.meta.url));
const RESULTS_DIR = resolve(__dirname, "../results");
const REPORT_PATH = resolve(__dirname, "../../../docs/comparable-benchmark-results.md");

export async function generateReport(results?: BenchmarkResult[]): Promise<void> {
  let data: BenchmarkResult[];

  if (results) {
    data = results;
  } else {
    // Stand-alone mode: read from results/*.json
    const files = (await readdir(RESULTS_DIR)).filter((f) => f.endsWith(".json"));
    if (files.length === 0) {
      console.error("No result JSON files found in", RESULTS_DIR);
      process.exit(1);
    }
    data = await Promise.all(
      files.map(async (f) => JSON.parse(await readFile(resolve(RESULTS_DIR, f), "utf8")) as BenchmarkResult)
    );
  }

  const hw = data[0]?.hardwareContext;
  const timestamp = data[0]?.timestamp ?? new Date().toISOString();

  const scenarios = [
    { id: "hello-throughput", label: "hello throughput" },
    { id: "concurrent-version", label: "concurrent version" },
    { id: "p95-latency-version", label: "p95 latency (version)" },
    { id: "ws-healthchanged-fanout", label: "WS healthChanged fan-out" },
  ];

  const rows = scenarios.map(({ id, label }) => {
    const apollo = data.find((r) => r.server === "apollo" && r.scenario === id);
    const isched = data.find((r) => r.server === "isched" && r.scenario === id);

    const apolloRps = apollo?.reqPerSecond != null ? apollo.reqPerSecond.toFixed(1) : "—";
    const ischedRps = isched?.reqPerSecond != null ? isched.reqPerSecond.toFixed(1) : "—";
    const ratio =
      apollo?.reqPerSecond != null && isched?.reqPerSecond != null
        ? (isched.reqPerSecond / apollo.reqPerSecond).toFixed(2) + "×"
        : "—";

    let p95 = "—";
    if (id === "p95-latency-version") {
      p95 = isched?.p95Ms != null ? isched.p95Ms.toFixed(2) + " ms" : "—";
    } else if (id === "ws-healthchanged-fanout") {
      p95 = isched?.fanOutMs != null ? isched.fanOutMs.toFixed(0) + " ms" : "—";
    }

    return `| ${label.padEnd(28)} | ${apolloRps.padStart(14)} | ${ischedRps.padStart(14)} | ${ratio.padStart(22)} | ${p95.padStart(18)} |`;
  });

  const hwLine = hw
    ? `OS: ${hw.os}  |  CPUs: ${hw.cpuCount}  |  Node.js: ${hw.nodeVersion}  |  isched build: ${hw.buildType}`
    : "Hardware context unavailable";

  const md = [
    "# Comparable Benchmark Results",
    "",
    `> Generated: ${timestamp}`,
    `> ${hwLine}`,
    "",
    "## Results",
    "",
    "| Scenario                     | Apollo req/s   | isched req/s   | isched / Apollo ratio  | p95 isched (ms)   |",
    "|------------------------------|----------------|----------------|------------------------|-------------------|",
    ...rows,
    "",
    "### Notes",
    "",
    "- **hello throughput**: `autocannon` — 1 connection, 5 s, `POST /graphql` `{ hello }`",
    "- **concurrent version**: `autocannon` — 100 connections, 5 s sustained, `{ version }`",
    "- **p95 latency (version)**: 1000 sequential `fetch` calls, sorted, `timings[950]`",
    "- **WS healthChanged fan-out**: 50 simultaneous `graphql-transport-ws` subscribers,",
    "  wall-clock until all receive first `healthChanged` event",
    "- p95 / fan-out column shows isched only; Apollo value is informational only",
    "",
  ].join("\n");

  await writeFile(REPORT_PATH, md, "utf8");
  console.log(`Report written to ${REPORT_PATH}`);
}

// Stand-alone entry point
if (process.argv[1] === fileURLToPath(import.meta.url)) {
  generateReport().catch((err) => {
    console.error(err instanceof Error ? err.message : err);
    process.exit(1);
  });
}
