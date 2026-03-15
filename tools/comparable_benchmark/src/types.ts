export interface HardwareContext {
  os: string;
  cpuCount: number;
  nodeVersion: string;
  buildType: string;
}

export interface BenchmarkResult {
  scenario: string;
  server: "apollo" | "isched";
  requests: number;
  errors: number;
  durationMs: number;
  reqPerSecond: number | null;
  p95Ms: number | null;
  fanOutMs: number | null;
  hardwareContext: HardwareContext;
  timestamp: string;
}
